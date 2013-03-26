/* $Id$ */
/** @file
 * IPRT - Read/Write Critical Section, Generic.
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM//_CRITSECT
#include "PDMInternal.h"
#include <VBox/vmm/pdmcritsectrw.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/vmm/hm.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/lockvalidator.h>
# include <iprt/semaphore.h>
#endif
#if defined(IN_RING3) || defined(IN_RING0)
# include <iprt/thread.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The number loops to spin for shared access in ring-3. */
#define PDMCRITSECTRW_SHRD_SPIN_COUNT_R3       20
/** The number loops to spin for shared access in ring-0. */
#define PDMCRITSECTRW_SHRD_SPIN_COUNT_R0       128
/** The number loops to spin for shared access in the raw-mode context. */
#define PDMCRITSECTRW_SHRD_SPIN_COUNT_RC       128

/** The number loops to spin for exclusive access in ring-3. */
#define PDMCRITSECTRW_EXCL_SPIN_COUNT_R3       20
/** The number loops to spin for exclusive access in ring-0. */
#define PDMCRITSECTRW_EXCL_SPIN_COUNT_R0       256
/** The number loops to spin for exclusive access in the raw-mode context. */
#define PDMCRITSECTRW_EXCL_SPIN_COUNT_RC       256


/* Undefine the automatic VBOX_STRICT API mappings. */
#undef PDMCritSectRwEnterExcl
#undef PDMCritSectRwTryEnterExcl
#undef PDMCritSectRwEnterShared
#undef PDMCritSectRwTryEnterShared


/**
 * Gets the ring-3 native thread handle of the calling thread.
 *
 * @returns native thread handle (ring-3).
 * @param   pThis       The read/write critical section.  This is only used in
 *                      R0 and RC.
 */
DECL_FORCE_INLINE(RTNATIVETHREAD) pdmCritSectRwGetNativeSelf(PCPDMCRITSECTRW pThis)
{
#ifdef IN_RING3
    NOREF(pThis);
    RTNATIVETHREAD  hNativeSelf = RTThreadNativeSelf();
#else
    AssertMsgReturn(pThis->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%RX32\n", pThis->s.Core.u32Magic),
                    NIL_RTNATIVETHREAD);
    PVM             pVM         = pThis->s.CTX_SUFF(pVM);   AssertPtr(pVM);
    PVMCPU          pVCpu       = VMMGetCpu(pVM);           AssertPtr(pVCpu);
    RTNATIVETHREAD  hNativeSelf = pVCpu->hNativeThread;     Assert(hNativeSelf != NIL_RTNATIVETHREAD);
#endif
    return hNativeSelf;
}





#ifdef IN_RING3
/**
 * Changes the lock validator sub-class of the read/write critical section.
 *
 * It is recommended to try make sure that nobody is using this critical section
 * while changing the value.
 *
 * @returns The old sub-class.  RTLOCKVAL_SUB_CLASS_INVALID is returns if the
 *          lock validator isn't compiled in or either of the parameters are
 *          invalid.
 * @param   pThis           Pointer to the read/write critical section.
 * @param   uSubClass       The new sub-class value.
 */
VMMDECL(uint32_t) PDMR3CritSectRwSetSubClass(PPDMCRITSECTRW pThis, uint32_t uSubClass)
{
    AssertPtrReturn(pThis, RTLOCKVAL_SUB_CLASS_INVALID);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, RTLOCKVAL_SUB_CLASS_INVALID);
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    AssertReturn(!(pThis->s.Core.fFlags & RTCRITSECT_FLAGS_NOP), RTLOCKVAL_SUB_CLASS_INVALID);

    RTLockValidatorRecSharedSetSubClass(pThis->s.Core.pValidatorRead, uSubClass);
    return RTLockValidatorRecExclSetSubClass(pThis->s.Core.pValidatorWrite, uSubClass);
# else
    NOREF(uSubClass);
    return RTLOCKVAL_SUB_CLASS_INVALID;
# endif
}
#endif /* IN_RING3 */


static int pdmCritSectRwEnterShared(PPDMCRITSECTRW pThis, int rcBusy, PCRTLOCKVALSRCPOS pSrcPos, bool fTryOnly)
{
    /*
     * Validate input.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, VERR_SEM_DESTROYED);

#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    RTTHREAD hThreadSelf = RTThreadSelfAutoAdopt();
    if (!fTryOnly)
    {
        int            rc9;
        RTNATIVETHREAD hNativeWriter;
        ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
        if (hNativeWriter != NIL_RTTHREAD && hNativeWriter == pdmCritSectRwGetNativeSelf(pThis))
            rc9 = RTLockValidatorRecExclCheckOrder(pThis->s.Core.pValidatorWrite, hThreadSelf, pSrcPos, RT_INDEFINITE_WAIT);
        else
            rc9 = RTLockValidatorRecSharedCheckOrder(pThis->s.Core.pValidatorRead, hThreadSelf, pSrcPos, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc9))
            return rc9;
    }
#endif

    /*
     * Get cracking...
     */
    uint64_t u64State    = ASMAtomicReadU64(&pThis->s.Core.u64State);
    uint64_t u64OldState = u64State;

    for (;;)
    {
        if ((u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT))
        {
            /* It flows in the right direction, try follow it before it changes. */
            uint64_t c = (u64State & RTCSRW_CNT_RD_MASK) >> RTCSRW_CNT_RD_SHIFT;
            c++;
            Assert(c < RTCSRW_CNT_MASK / 2);
            u64State &= ~RTCSRW_CNT_RD_MASK;
            u64State |= c << RTCSRW_CNT_RD_SHIFT;
            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
            {
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
                RTLockValidatorRecSharedAddOwner(pThis->s.Core.pValidatorRead, hThreadSelf, pSrcPos);
#endif
                break;
            }
        }
        else if ((u64State & (RTCSRW_CNT_RD_MASK | RTCSRW_CNT_WR_MASK)) == 0)
        {
            /* Wrong direction, but we're alone here and can simply try switch the direction. */
            u64State &= ~(RTCSRW_CNT_RD_MASK | RTCSRW_CNT_WR_MASK | RTCSRW_DIR_MASK);
            u64State |= (UINT64_C(1) << RTCSRW_CNT_RD_SHIFT) | (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT);
            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
            {
                Assert(!pThis->s.Core.fNeedReset);
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
                RTLockValidatorRecSharedAddOwner(pThis->s.Core.pValidatorRead, hThreadSelf, pSrcPos);
#endif
                break;
            }
        }
        else
        {
            /* Is the writer perhaps doing a read recursion? */
            RTNATIVETHREAD hNativeSelf = pdmCritSectRwGetNativeSelf(pThis);
            RTNATIVETHREAD hNativeWriter;
            ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
            if (hNativeSelf == hNativeWriter)
            {
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
                int rc9 = RTLockValidatorRecExclRecursionMixed(pThis->s.Core.pValidatorWrite, &pThis->s.Core.pValidatorRead->Core, pSrcPos);
                if (RT_FAILURE(rc9))
                    return rc9;
#endif
                Assert(pThis->s.Core.cWriterReads < UINT32_MAX / 2);
                ASMAtomicIncU32(&pThis->s.Core.cWriterReads);
                return VINF_SUCCESS; /* don't break! */
            }

            /* If we're only trying, return already. */
            if (fTryOnly)
                return VERR_SEM_BUSY;

            /* Add ourselves to the queue and wait for the direction to change. */
            uint64_t c = (u64State & RTCSRW_CNT_RD_MASK) >> RTCSRW_CNT_RD_SHIFT;
            c++;
            Assert(c < RTCSRW_CNT_MASK / 2);

            uint64_t cWait = (u64State & RTCSRW_WAIT_CNT_RD_MASK) >> RTCSRW_WAIT_CNT_RD_SHIFT;
            cWait++;
            Assert(cWait <= c);
            Assert(cWait < RTCSRW_CNT_MASK / 2);

            u64State &= ~(RTCSRW_CNT_RD_MASK | RTCSRW_WAIT_CNT_RD_MASK);
            u64State |= (c << RTCSRW_CNT_RD_SHIFT) | (cWait << RTCSRW_WAIT_CNT_RD_SHIFT);

            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
            {
                for (uint32_t iLoop = 0; ; iLoop++)
                {
                    int rc;
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
                    rc = RTLockValidatorRecSharedCheckBlocking(pThis->s.Core.pValidatorRead, hThreadSelf, pSrcPos, true,
                                                               RT_INDEFINITE_WAIT, RTTHREADSTATE_RW_READ, false);
                    if (RT_SUCCESS(rc))
#else
                    RTTHREAD hThreadSelf = RTThreadSelf();
                    RTThreadBlocking(hThreadSelf, RTTHREADSTATE_RW_READ, false);
#endif
                    {
                        do
                            rc = SUPSemEventMultiWaitNoResume(pThis->s.CTX_SUFF(pVM)->pSession,
                                                              (SUPSEMEVENTMULTI)pThis->s.Core.hEvtRead,
                                                              RT_INDEFINITE_WAIT);
                        while (rc == VERR_INTERRUPTED && pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC);
                        RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_RW_READ);
                        if (pThis->s.Core.u32Magic != RTCRITSECTRW_MAGIC)
                            return VERR_SEM_DESTROYED;
                    }
                    if (RT_FAILURE(rc))
                    {
                        /* Decrement the counts and return the error. */
                        for (;;)
                        {
                            u64OldState = u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
                            c = (u64State & RTCSRW_CNT_RD_MASK) >> RTCSRW_CNT_RD_SHIFT; Assert(c > 0);
                            c--;
                            cWait = (u64State & RTCSRW_WAIT_CNT_RD_MASK) >> RTCSRW_WAIT_CNT_RD_SHIFT; Assert(cWait > 0);
                            cWait--;
                            u64State &= ~(RTCSRW_CNT_RD_MASK | RTCSRW_WAIT_CNT_RD_MASK);
                            u64State |= (c << RTCSRW_CNT_RD_SHIFT) | (cWait << RTCSRW_WAIT_CNT_RD_SHIFT);
                            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                                break;
                        }
                        return rc;
                    }

                    Assert(pThis->s.Core.fNeedReset);
                    u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
                    if ((u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT))
                        break;
                    AssertMsg(iLoop < 1, ("%u\n", iLoop));
                }

                /* Decrement the wait count and maybe reset the semaphore (if we're last). */
                for (;;)
                {
                    u64OldState = u64State;

                    cWait = (u64State & RTCSRW_WAIT_CNT_RD_MASK) >> RTCSRW_WAIT_CNT_RD_SHIFT;
                    Assert(cWait > 0);
                    cWait--;
                    u64State &= ~RTCSRW_WAIT_CNT_RD_MASK;
                    u64State |= cWait << RTCSRW_WAIT_CNT_RD_SHIFT;

                    if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                    {
                        if (cWait == 0)
                        {
                            if (ASMAtomicXchgBool(&pThis->s.Core.fNeedReset, false))
                            {
                                int rc = SUPSemEventMultiReset(pThis->s.CTX_SUFF(pVM)->pSession,
                                                               (SUPSEMEVENTMULTI)pThis->s.Core.hEvtRead);
                                AssertRCReturn(rc, rc);
                            }
                        }
                        break;
                    }
                    u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
                }

#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
                RTLockValidatorRecSharedAddOwner(pThis->s.Core.pValidatorRead, hThreadSelf, pSrcPos);
#endif
                break;
            }
        }

        if (pThis->s.Core.u32Magic != RTCRITSECTRW_MAGIC)
            return VERR_SEM_DESTROYED;

        ASMNopPause();
        u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
        u64OldState = u64State;
    }

    /* got it! */
    STAM_REL_COUNTER_INC(&pThis->s.CTX_MID_Z(Stat,EnterShared));
    Assert((ASMAtomicReadU64(&pThis->s.Core.u64State) & RTCSRW_DIR_MASK) == (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT));
    return VINF_SUCCESS;

}


/**
 * Enter a critical section with shared (read) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  @a rcBusy if in ring-0 or raw-mode context and it is busy.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   rcBusy      The status code to return when we're in RC or R0 and the
 *                      section is busy.   Pass VINF_SUCCESS to acquired the
 *                      critical section thru a ring-3 call if necessary.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwEnterSharedDebug, PDMCritSectRwTryEnterShared,
 *          PDMCritSectRwTryEnterSharedDebug, PDMCritSectRwLeaveShared,
 *          RTCritSectRwEnterShared.
 */
VMMDECL(int) PDMCritSectRwEnterShared(PPDMCRITSECTRW pThis, int rcBusy)
{
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    return pdmCritSectRwEnterShared(pThis, rcBusy, NULL, false /*fTryOnly*/);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectRwEnterShared(pThis, rcBusy, &SrcPos, false /*fTryOnly*/);
#endif
}


/**
 * Enter a critical section with shared (read) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  @a rcBusy if in ring-0 or raw-mode context and it is busy.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   rcBusy      The status code to return when we're in RC or R0 and the
 *                      section is busy.   Pass VINF_SUCCESS to acquired the
 *                      critical section thru a ring-3 call if necessary.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwEnterShared, PDMCritSectRwTryEnterShared,
 *          PDMCritSectRwTryEnterSharedDebug, PDMCritSectRwLeaveShared,
 *          RTCritSectRwEnterSharedDebug.
 */
VMMDECL(int) PDMCritSectRwEnterSharedDebug(PPDMCRITSECTRW pThis, int rcBusy, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectRwEnterShared(pThis, rcBusy, &SrcPos, false /*fTryOnly*/);
}


/**
 * Try enter a critical section with shared (read) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwTryEnterSharedDebug, PDMCritSectRwEnterShared,
 *          PDMCritSectRwEnterSharedDebug, PDMCritSectRwLeaveShared,
 *          RTCritSectRwTryEnterShared.
 */
VMMDECL(int) PDMCritSectRwTryEnterShared(PPDMCRITSECTRW pThis)
{
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    return pdmCritSectRwEnterShared(pThis, VERR_SEM_BUSY, NULL, true /*fTryOnly*/);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectRwEnterShared(pThis, VERR_SEM_BUSY, &SrcPos, true /*fTryOnly*/);
#endif
}


/**
 * Try enter a critical section with shared (read) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwTryEnterShared, PDMCritSectRwEnterShared,
 *          PDMCritSectRwEnterSharedDebug, PDMCritSectRwLeaveShared,
 *          RTCritSectRwTryEnterSharedDebug.
 */
VMMDECL(int) PDMCritSectRwTryEnterSharedDebug(PPDMCRITSECTRW pThis, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectRwEnterShared(pThis, VERR_SEM_BUSY, &SrcPos, true /*fTryOnly*/);
}


/**
 * Leave a critical section held with shared access.
 *
 * @returns VBox status code.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwEnterShared, PDMCritSectRwTryEnterShared,
 *          PDMCritSectRwEnterSharedDebug, PDMCritSectRwTryEnterSharedDebug,
 *          PDMCritSectRwLeaveExcl, RTCritSectRwLeaveShared.
 */
VMMDECL(int) PDMCritSectRwLeaveShared(PPDMCRITSECTRW pThis)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, VERR_SEM_DESTROYED);

    /*
     * Check the direction and take action accordingly.
     */
    uint64_t u64State    = ASMAtomicReadU64(&pThis->s.Core.u64State);
    uint64_t u64OldState = u64State;
    if ((u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT))
    {
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
        int rc9 = RTLockValidatorRecSharedCheckAndRelease(pThis->s.Core.pValidatorRead, NIL_RTTHREAD);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        for (;;)
        {
            uint64_t c = (u64State & RTCSRW_CNT_RD_MASK) >> RTCSRW_CNT_RD_SHIFT;
            AssertReturn(c > 0, VERR_NOT_OWNER);
            c--;

            if (   c > 0
                || (u64State & RTCSRW_CNT_RD_MASK) == 0)
            {
                /* Don't change the direction. */
                u64State &= ~RTCSRW_CNT_RD_MASK;
                u64State |= c << RTCSRW_CNT_RD_SHIFT;
                if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                    break;
            }
            else
            {
                /* Reverse the direction and signal the reader threads. */
                u64State &= ~(RTCSRW_CNT_RD_MASK | RTCSRW_DIR_MASK);
                u64State |= RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT;
                if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                {
                    int rc = SUPSemEventSignal(pThis->s.CTX_SUFF(pVM)->pSession, (SUPSEMEVENT)pThis->s.Core.hEvtWrite);
                    AssertRC(rc);
                    break;
                }
            }

            ASMNopPause();
            u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
            u64OldState = u64State;
        }
    }
    else
    {
        RTNATIVETHREAD hNativeSelf = pdmCritSectRwGetNativeSelf(pThis);
        RTNATIVETHREAD hNativeWriter;
        ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
        AssertReturn(hNativeSelf == hNativeWriter, VERR_NOT_OWNER);
        AssertReturn(pThis->s.Core.cWriterReads > 0, VERR_NOT_OWNER);
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
        int rc = RTLockValidatorRecExclUnwindMixed(pThis->s.Core.pValidatorWrite, &pThis->s.Core.pValidatorRead->Core);
        if (RT_FAILURE(rc))
            return rc;
#endif
        ASMAtomicDecU32(&pThis->s.Core.cWriterReads);
    }

    return VINF_SUCCESS;
}


static int pdmCritSectRwEnterExcl(PPDMCRITSECTRW pThis, int rcBusy, PCRTLOCKVALSRCPOS pSrcPos, bool fTryOnly)
{
    /*
     * Validate input.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, VERR_SEM_DESTROYED);

#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    RTTHREAD hThreadSelf = NIL_RTTHREAD;
    if (!fTryOnly)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        int rc9 = RTLockValidatorRecExclCheckOrder(pThis->s.Core.pValidatorWrite, hThreadSelf, pSrcPos, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc9))
            return rc9;
    }
#endif

    /*
     * Check if we're already the owner and just recursing.
     */
    RTNATIVETHREAD hNativeSelf = pdmCritSectRwGetNativeSelf(pThis);
    RTNATIVETHREAD hNativeWriter;
    ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
    if (hNativeSelf == hNativeWriter)
    {
        Assert((ASMAtomicReadU64(&pThis->s.Core.u64State) & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT));
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
        int rc9 = RTLockValidatorRecExclRecursion(pThis->s.Core.pValidatorWrite, pSrcPos);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        Assert(pThis->s.Core.cWriteRecursions < UINT32_MAX / 2);
        STAM_REL_COUNTER_INC(&pThis->s.CTX_MID_Z(Stat,EnterExcl));
        ASMAtomicIncU32(&pThis->s.Core.cWriteRecursions);
        return VINF_SUCCESS;
    }

    /*
     * Get cracking.
     */
    uint64_t u64State    = ASMAtomicReadU64(&pThis->s.Core.u64State);
    uint64_t u64OldState = u64State;

    for (;;)
    {
        if (   (u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT)
            || (u64State & (RTCSRW_CNT_RD_MASK | RTCSRW_CNT_WR_MASK)) != 0)
        {
            /* It flows in the right direction, try follow it before it changes. */
            uint64_t c = (u64State & RTCSRW_CNT_WR_MASK) >> RTCSRW_CNT_WR_SHIFT;
            c++;
            Assert(c < RTCSRW_CNT_MASK / 2);
            u64State &= ~RTCSRW_CNT_WR_MASK;
            u64State |= c << RTCSRW_CNT_WR_SHIFT;
            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                break;
        }
        else if ((u64State & (RTCSRW_CNT_RD_MASK | RTCSRW_CNT_WR_MASK)) == 0)
        {
            /* Wrong direction, but we're alone here and can simply try switch the direction. */
            u64State &= ~(RTCSRW_CNT_RD_MASK | RTCSRW_CNT_WR_MASK | RTCSRW_DIR_MASK);
            u64State |= (UINT64_C(1) << RTCSRW_CNT_WR_SHIFT) | (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT);
            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                break;
        }
        else if (fTryOnly)
            /* Wrong direction and we're not supposed to wait, just return. */
            return VERR_SEM_BUSY;
        else
        {
            /* Add ourselves to the write count and break out to do the wait. */
            uint64_t c = (u64State & RTCSRW_CNT_WR_MASK) >> RTCSRW_CNT_WR_SHIFT;
            c++;
            Assert(c < RTCSRW_CNT_MASK / 2);
            u64State &= ~RTCSRW_CNT_WR_MASK;
            u64State |= c << RTCSRW_CNT_WR_SHIFT;
            if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                break;
        }

        if (pThis->s.Core.u32Magic != RTCRITSECTRW_MAGIC)
            return VERR_SEM_DESTROYED;

        ASMNopPause();
        u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
        u64OldState = u64State;
    }

    /*
     * If we're in write mode now try grab the ownership. Play fair if there
     * are threads already waiting.
     */
    bool fDone = (u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT)
              && (  ((u64State & RTCSRW_CNT_WR_MASK) >> RTCSRW_CNT_WR_SHIFT) == 1
                  || fTryOnly);
    if (fDone)
        ASMAtomicCmpXchgHandle(&pThis->s.Core.hNativeWriter, hNativeSelf, NIL_RTNATIVETHREAD, fDone);
    if (!fDone)
    {
        /*
         * Wait for our turn.
         */
        for (uint32_t iLoop = 0; ; iLoop++)
        {
            int rc;
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
            if (!fTryOnly)
            {
                if (hThreadSelf == NIL_RTTHREAD)
                    hThreadSelf = RTThreadSelfAutoAdopt();
                rc = RTLockValidatorRecExclCheckBlocking(pThis->s.Core.pValidatorWrite, hThreadSelf, pSrcPos, true,
                                                         RT_INDEFINITE_WAIT, RTTHREADSTATE_RW_WRITE, false);
            }
            else
                rc = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
#else
            RTTHREAD hThreadSelf = RTThreadSelf();
            RTThreadBlocking(hThreadSelf, RTTHREADSTATE_RW_WRITE, false);
#endif
            {
                do
                    rc = SUPSemEventWaitNoResume(pThis->s.CTX_SUFF(pVM)->pSession,
                                                 (SUPSEMEVENT)pThis->s.Core.hEvtWrite,
                                                 RT_INDEFINITE_WAIT);
                while (rc == VERR_INTERRUPTED && pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC);
                RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_RW_WRITE);
                if (pThis->s.Core.u32Magic != RTCRITSECTRW_MAGIC)
                    return VERR_SEM_DESTROYED;
            }
            if (RT_FAILURE(rc))
            {
                /* Decrement the counts and return the error. */
                for (;;)
                {
                    u64OldState = u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
                    uint64_t c = (u64State & RTCSRW_CNT_WR_MASK) >> RTCSRW_CNT_WR_SHIFT; Assert(c > 0);
                    c--;
                    u64State &= ~RTCSRW_CNT_WR_MASK;
                    u64State |= c << RTCSRW_CNT_WR_SHIFT;
                    if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                        break;
                }
                return rc;
            }

            u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
            if ((u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT))
            {
                ASMAtomicCmpXchgHandle(&pThis->s.Core.hNativeWriter, hNativeSelf, NIL_RTNATIVETHREAD, fDone);
                if (fDone)
                    break;
            }
            AssertMsg(iLoop < 1000, ("%u\n", iLoop)); /* may loop a few times here... */
        }
    }

    /*
     * Got it!
     */
    Assert((ASMAtomicReadU64(&pThis->s.Core.u64State) & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT));
    ASMAtomicWriteU32(&pThis->s.Core.cWriteRecursions, 1);
    Assert(pThis->s.Core.cWriterReads == 0);
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    RTLockValidatorRecExclSetOwner(pThis->s.Core.pValidatorWrite, hThreadSelf, pSrcPos, true);
#endif
    STAM_REL_COUNTER_INC(&pThis->s.CTX_MID_Z(Stat,EnterExcl));
    STAM_PROFILE_ADV_START(&pThis->s.StatWriteLocked, swl);

    return VINF_SUCCESS;
}


/**
 * Try enter a critical section with exclusive (write) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  @a rcBusy if in ring-0 or raw-mode context and it is busy.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   rcBusy      The status code to return when we're in RC or R0 and the
 *                      section is busy.   Pass VINF_SUCCESS to acquired the
 *                      critical section thru a ring-3 call if necessary.
 * @sa      PDMCritSectRwEnterExclDebug, PDMCritSectRwTryEnterExcl,
 *          PDMCritSectRwTryEnterExclDebug,
 *          PDMCritSectEnterDebug, PDMCritSectEnter,
 * RTCritSectRwEnterExcl.
 */
VMMDECL(int) PDMCritSectRwEnterExcl(PPDMCRITSECTRW pThis, int rcBusy)
{
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    return pdmCritSectRwEnterExcl(pThis, rcBusy, NULL, false /*fTryAgain*/);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectRwEnterExcl(pThis, rcBusy, &SrcPos, false /*fTryAgain*/);
#endif
}


/**
 * Try enter a critical section with exclusive (write) access.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  @a rcBusy if in ring-0 or raw-mode context and it is busy.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   rcBusy      The status code to return when we're in RC or R0 and the
 *                      section is busy.   Pass VINF_SUCCESS to acquired the
 *                      critical section thru a ring-3 call if necessary.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwEnterExcl, PDMCritSectRwTryEnterExcl,
 *          PDMCritSectRwTryEnterExclDebug,
 *          PDMCritSectEnterDebug, PDMCritSectEnter,
 *          RTCritSectRwEnterExclDebug.
 */
VMMDECL(int) PDMCritSectRwEnterExclDebug(PPDMCRITSECTRW pThis, int rcBusy, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectRwEnterExcl(pThis, rcBusy, &SrcPos, false /*fTryAgain*/);
}


/**
 * Try enter a critical section with exclusive (write) access.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwEnterExcl, PDMCritSectRwTryEnterExclDebug,
 *          PDMCritSectRwEnterExclDebug,
 *          PDMCritSectTryEnter, PDMCritSectTryEnterDebug,
 *          RTCritSectRwTryEnterExcl.
 */
VMMDECL(int) PDMCritSectRwTryEnterExcl(PPDMCRITSECTRW pThis)
{
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    return pdmCritSectRwEnterExcl(pThis, VERR_SEM_BUSY, NULL, true /*fTryAgain*/);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectRwEnterExcl(pThis, VERR_SEM_BUSY, &SrcPos, true /*fTryAgain*/);
#endif
}


/**
 * Try enter a critical section with exclusive (write) access.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pThis       Pointer to the read/write critical section.
 * @param   uId         Where we're entering the section.
 * @param   pszFile     The source position - file.
 * @param   iLine       The source position - line.
 * @param   pszFunction The source position - function.
 * @sa      PDMCritSectRwTryEnterExcl, PDMCritSectRwEnterExcl,
 *          PDMCritSectRwEnterExclDebug,
 *          PDMCritSectTryEnterDebug, PDMCritSectTryEnter,
 *          RTCritSectRwTryEnterExclDebug.
 */
VMMDECL(int) PDMCritSectRwTryEnterExclDebug(PPDMCRITSECTRW pThis, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectRwEnterExcl(pThis, VERR_SEM_BUSY, &SrcPos, true /*fTryAgain*/);
}


/**
 * Leave a critical section held exclusively.
 *
 * @returns VBox status code.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwLeaveShared, RTCritSectRwLeaveExcl.
 */
VMMDECL(int) PDMCritSectRwLeaveExcl(PPDMCRITSECTRW pThis)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, VERR_SEM_DESTROYED);

    RTNATIVETHREAD hNativeSelf = pdmCritSectRwGetNativeSelf(pThis);
    RTNATIVETHREAD hNativeWriter;
    ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
    AssertReturn(hNativeSelf == hNativeWriter, VERR_NOT_OWNER);

    /*
     * Unwind one recursion. Is it the final one?
     */
    if (pThis->s.Core.cWriteRecursions == 1)
    {
        AssertReturn(pThis->s.Core.cWriterReads == 0, VERR_WRONG_ORDER); /* (must release all read recursions before the final write.) */
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
        int rc9 = RTLockValidatorRecExclReleaseOwner(pThis->s.Core.pValidatorWrite, true);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        /*
         * Update the state.
         */
        ASMAtomicWriteU32(&pThis->s.Core.cWriteRecursions, 0);
        ASMAtomicWriteHandle(&pThis->s.Core.hNativeWriter, NIL_RTNATIVETHREAD);
        STAM_PROFILE_ADV_STOP(&pThis->s.StatWriteLocked, swl);

        for (;;)
        {
            uint64_t u64State    = ASMAtomicReadU64(&pThis->s.Core.u64State);
            uint64_t u64OldState = u64State;

            uint64_t c = (u64State & RTCSRW_CNT_WR_MASK) >> RTCSRW_CNT_WR_SHIFT;
            Assert(c > 0);
            c--;

            if (   c > 0
                || (u64State & RTCSRW_CNT_RD_MASK) == 0)
            {
                /* Don't change the direction, wake up the next writer if any. */
                u64State &= ~RTCSRW_CNT_WR_MASK;
                u64State |= c << RTCSRW_CNT_WR_SHIFT;
                if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                {
                    if (c > 0)
                    {
                        int rc = SUPSemEventSignal(pThis->s.CTX_SUFF(pVM)->pSession, (SUPSEMEVENT)pThis->s.Core.hEvtWrite);
                        AssertRC(rc);
                    }
                    break;
                }
            }
            else
            {
                /* Reverse the direction and signal the reader threads. */
                u64State &= ~(RTCSRW_CNT_WR_MASK | RTCSRW_DIR_MASK);
                u64State |= RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT;
                if (ASMAtomicCmpXchgU64(&pThis->s.Core.u64State, u64State, u64OldState))
                {
                    Assert(!pThis->s.Core.fNeedReset);
                    ASMAtomicWriteBool(&pThis->s.Core.fNeedReset, true);
                    int rc = SUPSemEventMultiSignal(pThis->s.CTX_SUFF(pVM)->pSession, (SUPSEMEVENTMULTI)pThis->s.Core.hEvtRead);
                    AssertRC(rc);
                    break;
                }
            }

            ASMNopPause();
            if (pThis->s.Core.u32Magic != RTCRITSECTRW_MAGIC)
                return VERR_SEM_DESTROYED;
        }
    }
    else
    {
        /*
         * Not the final recursion.
         */
        Assert(pThis->s.Core.cWriteRecursions != 0);
#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
        int rc9 = RTLockValidatorRecExclUnwind(pThis->s.Core.pValidatorWrite);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        ASMAtomicDecU32(&pThis->s.Core.cWriteRecursions);
    }

    return VINF_SUCCESS;
}


/**
 * Checks the caller is the exclusive (write) owner of the critical section.
 *
 * @retval  @c true if owner.
 * @retval  @c false if not owner.
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwIsReadOwner, PDMCritSectIsOwner,
 *          RTCritSectRwIsWriteOwner.
 */
VMMDECL(bool) PDMCritSectRwIsWriteOwner(PPDMCRITSECTRW pThis)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, false);

    /*
     * Check ownership.
     */
    RTNATIVETHREAD hNativeWriter;
    ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hNativeWriter);
    if (hNativeWriter == NIL_RTNATIVETHREAD)
        return false;
    return hNativeWriter == pdmCritSectRwGetNativeSelf(pThis);
}


/**
 * Checks if the caller is one of the read owners of the critical section.
 *
 * @note    !CAUTION!  This API doesn't work reliably if lock validation isn't
 *          enabled. Meaning, the answer is not trustworhty unless
 *          RT_LOCK_STRICT or PDMCRITSECTRW_STRICT was defined at build time.
 *          Also, make sure you do not use RTCRITSECTRW_FLAGS_NO_LOCK_VAL when
 *          creating the semaphore.  And finally, if you used a locking class,
 *          don't disable deadlock detection by setting cMsMinDeadlock to
 *          RT_INDEFINITE_WAIT.
 *
 *          In short, only use this for assertions.
 *
 * @returns @c true if reader, @c false if not.
 * @param   pThis       Pointer to the read/write critical section.
 * @param   fWannaHear  What you'd like to hear when lock validation is not
 *                      available.  (For avoiding asserting all over the place.)
 * @sa      PDMCritSectRwIsWriteOwner, RTCritSectRwIsReadOwner.
 */
VMMDECL(bool) PDMCritSectRwIsReadOwner(PPDMCRITSECTRW pThis, bool fWannaHear)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, false);

    /*
     * Inspect the state.
     */
    uint64_t u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
    if ((u64State & RTCSRW_DIR_MASK) == (RTCSRW_DIR_WRITE << RTCSRW_DIR_SHIFT))
    {
        /*
         * It's in write mode, so we can only be a reader if we're also the
         * current writer.
         */
        RTNATIVETHREAD hWriter;
        ASMAtomicUoReadHandle(&pThis->s.Core.hNativeWriter, &hWriter);
        if (hWriter == NIL_RTNATIVETHREAD)
            return false;
        return hWriter == pdmCritSectRwGetNativeSelf(pThis);
    }

    /*
     * Read mode.  If there are no current readers, then we cannot be a reader.
     */
    if (!(u64State & RTCSRW_CNT_RD_MASK))
        return false;

#if defined(PDMCRITSECTRW_STRICT) && defined(IN_RING3)
    /*
     * Ask the lock validator.
     * Note! It doesn't know everything, let's deal with that if it becomes an issue...
     */
    return RTLockValidatorRecSharedIsOwner(pThis->s.Core.pValidatorRead, NIL_RTTHREAD);
#else
    /*
     * Ok, we don't know, just tell the caller what he want to hear.
     */
    return fWannaHear;
#endif
}


/**
 * Gets the write recursion count.
 *
 * @returns The write recursion count (0 if bad critsect).
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwGetWriterReadRecursion, PDMCritSectRwGetReadCount,
 *          RTCritSectRwGetWriteRecursion.
 */
VMMDECL(uint32_t) PDMCritSectRwGetWriteRecursion(PPDMCRITSECTRW pThis)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, 0);

    /*
     * Return the requested data.
     */
    return pThis->s.Core.cWriteRecursions;
}


/**
 * Gets the read recursion count of the current writer.
 *
 * @returns The read recursion count (0 if bad critsect).
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwGetWriteRecursion, PDMCritSectRwGetReadCount,
 *          RTCritSectRwGetWriterReadRecursion.
 */
VMMDECL(uint32_t) PDMCritSectRwGetWriterReadRecursion(PPDMCRITSECTRW pThis)
{
    /*
     * Validate handle.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, 0);

    /*
     * Return the requested data.
     */
    return pThis->s.Core.cWriterReads;
}


/**
 * Gets the current number of reads.
 *
 * This includes all read recursions, so it might be higher than the number of
 * read owners.  It does not include reads done by the current writer.
 *
 * @returns The read count (0 if bad critsect).
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectRwGetWriteRecursion, PDMCritSectRwGetWriterReadRecursion,
 *          RTCritSectRwGetReadCount.
 */
VMMDECL(uint32_t) PDMCritSectRwGetReadCount(PPDMCRITSECTRW pThis)
{
    /*
     * Validate input.
     */
    AssertPtr(pThis);
    AssertReturn(pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC, 0);

    /*
     * Return the requested data.
     */
    uint64_t u64State = ASMAtomicReadU64(&pThis->s.Core.u64State);
    if ((u64State & RTCSRW_DIR_MASK) != (RTCSRW_DIR_READ << RTCSRW_DIR_SHIFT))
        return 0;
    return (u64State & RTCSRW_CNT_RD_MASK) >> RTCSRW_CNT_RD_SHIFT;
}


/**
 * Checks if the read/write critical section is initialized or not.
 *
 * @retval  @c true if initialized.
 * @retval  @c false if not initialized.
 * @param   pThis       Pointer to the read/write critical section.
 * @sa      PDMCritSectIsInitialized, RTCritSectRwIsInitialized.
 */
VMMDECL(bool) PDMCritSectRwIsInitialized(PCPDMCRITSECTRW pThis)
{
    AssertPtr(pThis);
    return pThis->s.Core.u32Magic == RTCRITSECTRW_MAGIC;
}

