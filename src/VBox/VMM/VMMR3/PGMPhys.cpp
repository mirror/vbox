/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PGM_PHYS
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/pdmdev.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/system.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The number of pages to free in one batch. */
#define PGMPHYS_FREE_PAGE_BATCH_SIZE    128


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pgmR3PhysRomWriteHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);


/*
 * PGMR3PhysReadU8-64
 * PGMR3PhysWriteU8-64
 */
#define PGMPHYSFN_READNAME  PGMR3PhysReadU8
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU8
#define PGMPHYS_DATASIZE    1
#define PGMPHYS_DATATYPE    uint8_t
#include "PGMPhysRWTmpl.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU16
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU16
#define PGMPHYS_DATASIZE    2
#define PGMPHYS_DATATYPE    uint16_t
#include "PGMPhysRWTmpl.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU32
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU32
#define PGMPHYS_DATASIZE    4
#define PGMPHYS_DATATYPE    uint32_t
#include "PGMPhysRWTmpl.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU64
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU64
#define PGMPHYS_DATASIZE    8
#define PGMPHYS_DATATYPE    uint64_t
#include "PGMPhysRWTmpl.h"


/**
 * EMT worker for PGMR3PhysReadExternal.
 */
static DECLCALLBACK(int) pgmR3PhysReadExternalEMT(PVM pVM, PRTGCPHYS pGCPhys, void *pvBuf, size_t cbRead)
{
    PGMPhysRead(pVM, *pGCPhys, pvBuf, cbRead);
    return VINF_SUCCESS;
}


/**
 * Read from physical memory, external users.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 *
 * @param   pVM             The VM handle.
 * @param   GCPhys          Physical address to read from.
 * @param   pvBuf           Where to read into.
 * @param   cbRead          How many bytes to read.
 *
 * @thread  Any but EMTs.
 */
VMMR3DECL(int) PGMR3PhysReadExternal(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    VM_ASSERT_OTHER_THREAD(pVM);

    AssertMsgReturn(cbRead > 0, ("don't even think about reading zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMR3PhysReadExternal: %RGp %d\n", GCPhys, cbRead));

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, GCPhys);
    for (;;)
    {
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPHYS off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;
                PPGMPAGE pPage = &pRam->aPages[iPage];

                /*
                 * If the page has an ALL access handler, we'll have to
                 * delegate the job to EMT.
                 */
                if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage))
                {
                    pgmUnlock(pVM);

                    return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)pgmR3PhysReadExternalEMT, 4,
                                                   pVM, &GCPhys, pvBuf, cbRead);
                }
                Assert(!PGM_PAGE_IS_MMIO(pPage));

                /*
                 * Simple stuff, go ahead.
                 */
                size_t cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbRead)
                    cb = cbRead;
                PGMPAGEMAPLOCK PgMpLck;
                const void    *pvSrc;
                int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, pRam->GCPhys + off, &pvSrc, &PgMpLck);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pvBuf, pvSrc, cb);
                    pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                }
                else
                {
                    AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternalReadOnly failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                           pRam->GCPhys + off, pPage, rc));
                    memset(pvBuf, 0xff, cb);
                }

                /* next page */
                if (cb >= cbRead)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }
                cbRead -= cb;
                off    += cb;
                GCPhys += cb;
                pvBuf   = (char *)pvBuf + cb;
            } /* walk pages in ram range. */
        }
        else
        {
            LogFlow(("PGMPhysRead: Unassigned %RGp size=%u\n", GCPhys, cbRead));

            /*
             * Unassigned address space.
             */
            size_t cb = pRam ? pRam->GCPhys - GCPhys : ~(size_t)0;
            if (cb >= cbRead)
            {
                memset(pvBuf, 0xff, cbRead);
                break;
            }
            memset(pvBuf, 0xff, cb);

            cbRead -= cb;
            pvBuf   = (char *)pvBuf + cb;
            GCPhys += cb;
        }

        /* Advance range if necessary. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
    } /* Ram range walk */

    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * EMT worker for PGMR3PhysWriteExternal.
 */
static DECLCALLBACK(int) pgmR3PhysWriteExternalEMT(PVM pVM, PRTGCPHYS pGCPhys, const void *pvBuf, size_t cbWrite)
{
    /** @todo VERR_EM_NO_MEMORY */
    PGMPhysWrite(pVM, *pGCPhys, pvBuf, cbWrite);
    return VINF_SUCCESS;
}


/**
 * Write to physical memory, external users.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_EM_NO_MEMORY.
 *
 * @param   pVM             The VM handle.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 * @param   pszWho          Who is writing.  For tracking down who is writing
 *                          after we've saved the state.
 *
 * @thread  Any but EMTs.
 */
VMMDECL(int) PGMR3PhysWriteExternal(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, const char *pszWho)
{
    VM_ASSERT_OTHER_THREAD(pVM);

    AssertMsg(!pVM->pgm.s.fNoMorePhysWrites,
              ("Calling PGMR3PhysWriteExternal after pgmR3Save()! GCPhys=%RGp cbWrite=%#x pszWho=%s\n",
               GCPhys, cbWrite, pszWho));
    AssertMsgReturn(cbWrite > 0, ("don't even think about writing zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMR3PhysWriteExternal: %RGp %d\n", GCPhys, cbWrite));

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges, stop when we hit something difficult.
     */
    PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, GCPhys);
    for (;;)
    {
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPTR off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                RTGCPTR     iPage = off >> PAGE_SHIFT;
                PPGMPAGE    pPage = &pRam->aPages[iPage];

                /*
                 * Is the page problematic, we have to do the work on the EMT.
                 *
                 * Allocating writable pages and access handlers are
                 * problematic, write monitored pages are simple and can be
                 * dealt with here.
                 */
                if (    PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage)
                    ||  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED)
                {
                    if (    PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED
                        && !PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                        pgmPhysPageMakeWriteMonitoredWritable(pVM, pPage);
                    else
                    {
                        pgmUnlock(pVM);

                        return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)pgmR3PhysWriteExternalEMT, 4,
                                                       pVM, &GCPhys, pvBuf, cbWrite);
                    }
                }
                Assert(!PGM_PAGE_IS_MMIO(pPage));

                /*
                 * Simple stuff, go ahead.
                 */
                size_t cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbWrite)
                    cb = cbWrite;
                PGMPAGEMAPLOCK PgMpLck;
                void          *pvDst;
                int rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, pRam->GCPhys + off, &pvDst, &PgMpLck);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pvDst, pvBuf, cb);
                    pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                }
                else
                    AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                           pRam->GCPhys + off, pPage, rc));

                /* next page */
                if (cb >= cbWrite)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }

                cbWrite -= cb;
                off     += cb;
                GCPhys  += cb;
                pvBuf    = (const char *)pvBuf + cb;
            } /* walk pages in ram range */
        }
        else
        {
            /*
             * Unassigned address space, skip it.
             */
            if (!pRam)
                break;
            size_t cb = pRam->GCPhys - GCPhys;
            if (cb >= cbWrite)
                break;
            cbWrite -= cb;
            pvBuf   = (const char *)pvBuf + cb;
            GCPhys += cb;
        }

        /* Advance range if necessary. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
    } /* Ram range walk */

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * VMR3ReqCall worker for PGMR3PhysGCPhys2CCPtrExternal to make pages writable.
 *
 * @returns see PGMR3PhysGCPhys2CCPtrExternal
 * @param   pVM         The VM handle.
 * @param   pGCPhys     Pointer to the guest physical address.
 * @param   ppv         Where to store the mapping address.
 * @param   pLock       Where to store the lock.
 */
static DECLCALLBACK(int) pgmR3PhysGCPhys2CCPtrDelegated(PVM pVM, PRTGCPHYS pGCPhys, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    /*
     * Just hand it to PGMPhysGCPhys2CCPtr and check that it's not a page with
     * an access handler after it succeeds.
     */
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    rc = PGMPhysGCPhys2CCPtr(pVM, *pGCPhys, ppv, pLock);
    if (RT_SUCCESS(rc))
    {
        PPGMPAGEMAPTLBE pTlbe;
        int rc2 = pgmPhysPageQueryTlbe(pVM, *pGCPhys, &pTlbe);
        AssertFatalRC(rc2);
        PPGMPAGE pPage = pTlbe->pPage;
        if (PGM_PAGE_IS_MMIO(pPage))
        {
            PGMPhysReleasePageMappingLock(pVM, pLock);
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        }
        else if (    PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage)
#ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
                 ||  pgmPoolIsDirtyPage(pVM, *pGCPhys)
#endif
                )
        {
            /* We *must* flush any corresponding pgm pool page here, otherwise we'll
             * not be informed about writes and keep bogus gst->shw mappings around.
             */
            pgmPoolFlushPageByGCPhys(pVM, *pGCPhys);
            Assert(!PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage));
            /** @todo r=bird: return VERR_PGM_PHYS_PAGE_RESERVED here if it still has
             *        active handlers, see the PGMR3PhysGCPhys2CCPtrExternal docs. */
        }
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Requests the mapping of a guest page into ring-3, external threads.
 *
 * When you're done with the page, call PGMPhysReleasePageMappingLock() ASAP to
 * release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify the
 * page, use the PGMR3PhysGCPhys2CCPtrReadOnlyExternal() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical
 *          backing or if the page has any active access handlers. The caller
 *          must fall back on using PGMR3PhysWriteExternal.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than the
 *          PGM one) because of the deadlock risk when we have to delegating the
 *          task to an EMT.
 * @thread  Any.
 */
VMMR3DECL(int) PGMR3PhysGCPhys2CCPtrExternal(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    AssertPtr(ppv);
    AssertPtr(pLock);

    Assert(VM_IS_EMT(pVM) || !PGMIsLockOwner(pVM));

    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(pVM, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        PPGMPAGE pPage = pTlbe->pPage;
        if (PGM_PAGE_IS_MMIO(pPage))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        else
        {
            /*
             * If the page is shared, the zero page, or being write monitored
             * it must be converted to an page that's writable if possible.
             * We can only deal with write monitored pages here, the rest have
             * to be on an EMT.
             */
            if (    PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage)
                ||  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED
#ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
                ||  pgmPoolIsDirtyPage(pVM, GCPhys)
#endif
               )
            {
                if (    PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED
                    &&  !PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage)
#ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
                    &&  !pgmPoolIsDirtyPage(pVM, GCPhys)
#endif
                   )
                    pgmPhysPageMakeWriteMonitoredWritable(pVM, pPage);
                else
                {
                    pgmUnlock(pVM);

                    return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)pgmR3PhysGCPhys2CCPtrDelegated, 4,
                                                   pVM, &GCPhys, ppv, pLock);
                }
            }

            /*
             * Now, just perform the locking and calculate the return address.
             */
            PPGMPAGEMAP pMap = pTlbe->pMap;
            if (pMap)
                pMap->cRefs++;

            unsigned cLocks = PGM_PAGE_GET_WRITE_LOCKS(pPage);
            if (RT_LIKELY(cLocks < PGM_PAGE_MAX_LOCKS - 1))
            {
                if (cLocks == 0)
                    pVM->pgm.s.cWriteLockedPages++;
                PGM_PAGE_INC_WRITE_LOCKS(pPage);
            }
            else if (cLocks != PGM_PAGE_GET_WRITE_LOCKS(pPage))
            {
                PGM_PAGE_INC_WRITE_LOCKS(pPage);
                AssertMsgFailed(("%RGp / %R[pgmpage] is entering permanent write locked state!\n", GCPhys, pPage));
                if (pMap)
                    pMap->cRefs++; /* Extra ref to prevent it from going away. */
            }

            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
            pLock->uPageAndType = (uintptr_t)pPage | PGMPAGEMAPLOCK_TYPE_WRITE;
            pLock->pvMap = pMap;
        }
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Requests the mapping of a guest page into ring-3, external threads.
 *
 * When you're done with the page, call PGMPhysReleasePageMappingLock() ASAP to
 * release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical
 *          backing or if the page as an active ALL access handler. The caller
 *          must fall back on using PGMPhysRead.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  Any.
 */
VMMR3DECL(int) PGMR3PhysGCPhys2CCPtrReadOnlyExternal(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(pVM, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        PPGMPAGE pPage = pTlbe->pPage;
#if 1
        /* MMIO pages doesn't have any readable backing. */
        if (PGM_PAGE_IS_MMIO(pPage))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
#else
        if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
#endif
        else
        {
            /*
             * Now, just perform the locking and calculate the return address.
             */
            PPGMPAGEMAP pMap = pTlbe->pMap;
            if (pMap)
                pMap->cRefs++;

            unsigned cLocks = PGM_PAGE_GET_READ_LOCKS(pPage);
            if (RT_LIKELY(cLocks < PGM_PAGE_MAX_LOCKS - 1))
            {
                if (cLocks == 0)
                    pVM->pgm.s.cReadLockedPages++;
                PGM_PAGE_INC_READ_LOCKS(pPage);
            }
            else if (cLocks != PGM_PAGE_GET_READ_LOCKS(pPage))
            {
                PGM_PAGE_INC_READ_LOCKS(pPage);
                AssertMsgFailed(("%RGp / %R[pgmpage] is entering permanent readonly locked state!\n", GCPhys, pPage));
                if (pMap)
                    pMap->cRefs++; /* Extra ref to prevent it from going away. */
            }

            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
            pLock->uPageAndType = (uintptr_t)pPage | PGMPAGEMAPLOCK_TYPE_READ;
            pLock->pvMap = pMap;
        }
    }

    pgmUnlock(pVM);
    return rc;
}


#define MAKE_LEAF(a_pNode) \
    do { \
        (a_pNode)->pLeftR3  = NIL_RTR3PTR; \
        (a_pNode)->pRightR3 = NIL_RTR3PTR; \
        (a_pNode)->pLeftR0  = NIL_RTR0PTR; \
        (a_pNode)->pRightR0 = NIL_RTR0PTR; \
        (a_pNode)->pLeftRC  = NIL_RTRCPTR; \
        (a_pNode)->pRightRC = NIL_RTRCPTR; \
    } while (0)

#define INSERT_LEFT(a_pParent, a_pNode) \
    do { \
        (a_pParent)->pLeftR3 = (a_pNode); \
        (a_pParent)->pLeftR0 = (a_pNode)->pSelfR0; \
        (a_pParent)->pLeftRC = (a_pNode)->pSelfRC; \
    } while (0)
#define INSERT_RIGHT(a_pParent, a_pNode) \
    do { \
        (a_pParent)->pRightR3 = (a_pNode); \
        (a_pParent)->pRightR0 = (a_pNode)->pSelfR0; \
        (a_pParent)->pRightRC = (a_pNode)->pSelfRC; \
    } while (0)


/**
 * Recursive tree builder.
 *
 * @param   ppRam           Pointer to the iterator variable.
 * @param   iHeight         The hight about normal leaf nodes.  Inserts a leaf
 *                          node if 0.
 */
static PPGMRAMRANGE pgmR3PhysRebuildRamRangeSearchTreesRecursively(PPGMRAMRANGE *ppRam, int iDepth)
{
    PPGMRAMRANGE pRam;
    if (iDepth <= 0)
    {
        /*
         * Leaf node.
         */
        pRam = *ppRam;
        if (pRam)
        {
            *ppRam = pRam->pNextR3;
            MAKE_LEAF(pRam);
        }
    }
    else
    {

        /*
         * Intermediate node.
         */
        PPGMRAMRANGE pLeft = pgmR3PhysRebuildRamRangeSearchTreesRecursively(ppRam, iDepth - 1);

        pRam = *ppRam;
        if (!pRam)
            return pLeft;
        *ppRam = pRam->pNextR3;
        MAKE_LEAF(pRam);
        INSERT_LEFT(pRam, pLeft);

        PPGMRAMRANGE pRight = pgmR3PhysRebuildRamRangeSearchTreesRecursively(ppRam, iDepth - 1);
        if (pRight)
            INSERT_RIGHT(pRam, pRight);
    }
    return pRam;
}


/**
 * Rebuilds the RAM range search trees.
 *
 * @param   pVM         The VM handle.
 */
static void pgmR3PhysRebuildRamRangeSearchTrees(PVM pVM)
{

    /*
     * Create the reasonably balanced tree in a sequential fashion.
     * For simplicity (laziness) we use standard recursion here.
     */
    int             iDepth = 0;
    PPGMRAMRANGE    pRam   = pVM->pgm.s.pRamRangesXR3;
    PPGMRAMRANGE    pRoot  = pgmR3PhysRebuildRamRangeSearchTreesRecursively(&pRam, 0);
    while (pRam)
    {
        PPGMRAMRANGE pLeft = pRoot;

        pRoot = pRam;
        pRam = pRam->pNextR3;
        MAKE_LEAF(pRoot);
        INSERT_LEFT(pRoot, pLeft);

        PPGMRAMRANGE pRight = pgmR3PhysRebuildRamRangeSearchTreesRecursively(&pRam, iDepth);
        if (pRight)
            INSERT_RIGHT(pRoot, pRight);
        /** @todo else: rotate the tree. */

        iDepth++;
    }

    pVM->pgm.s.pRamRangeTreeR3 = pRoot;
    pVM->pgm.s.pRamRangeTreeR0 = pRoot ? pRoot->pSelfR0 : NIL_RTR0PTR;
    pVM->pgm.s.pRamRangeTreeRC = pRoot ? pRoot->pSelfRC : NIL_RTRCPTR;

#ifdef VBOX_STRICT
    /*
     * Verify that the above code works.
     */
    unsigned cRanges = 0;
    for (pRam = pVM->pgm.s.pRamRangesXR3; pRam; pRam = pRam->pNextR3)
        cRanges++;
    Assert(cRanges > 0);

    unsigned cMaxDepth = ASMBitLastSetU32(cRanges);
    if ((1U << cMaxDepth) < cRanges)
        cMaxDepth++;

    for (pRam = pVM->pgm.s.pRamRangesXR3; pRam; pRam = pRam->pNextR3)
    {
        unsigned     cDepth = 0;
        PPGMRAMRANGE pRam2 = pVM->pgm.s.pRamRangeTreeR3;
        for (;;)
        {
            if (pRam == pRam2)
                break;
            Assert(pRam2);
            if (pRam->GCPhys < pRam2->GCPhys)
                pRam2 = pRam2->pLeftR3;
            else
                pRam2 = pRam2->pRightR3;
        }
        AssertMsg(cDepth <= cMaxDepth, ("cDepth=%d cMaxDepth=%d\n", cDepth, cMaxDepth));
    }
#endif /* VBOX_STRICT */
}

#undef MAKE_LEAF
#undef INSERT_LEFT
#undef INSERT_RIGHT

/**
 * Relinks the RAM ranges using the pSelfRC and pSelfR0 pointers.
 *
 * Called when anything was relocated.
 *
 * @param   pVM         The VM handle.
 */
void pgmR3PhysRelinkRamRanges(PVM pVM)
{
    PPGMRAMRANGE pCur;

#ifdef VBOX_STRICT
    for (pCur = pVM->pgm.s.pRamRangesXR3; pCur; pCur = pCur->pNextR3)
    {
        Assert((pCur->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pCur->pSelfR0 == MMHyperCCToR0(pVM, pCur));
        Assert((pCur->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pCur->pSelfRC == MMHyperCCToRC(pVM, pCur));
        Assert((pCur->GCPhys     & PAGE_OFFSET_MASK) == 0);
        Assert((pCur->GCPhysLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);
        Assert((pCur->cb         & PAGE_OFFSET_MASK) == 0);
        Assert(pCur->cb == pCur->GCPhysLast - pCur->GCPhys + 1);
        for (PPGMRAMRANGE pCur2 = pVM->pgm.s.pRamRangesXR3; pCur2; pCur2 = pCur2->pNextR3)
            Assert(   pCur2 == pCur
                   || strcmp(pCur2->pszDesc, pCur->pszDesc)); /** @todo fix MMIO ranges!! */
    }
#endif

    pCur = pVM->pgm.s.pRamRangesXR3;
    if (pCur)
    {
        pVM->pgm.s.pRamRangesXR0 = pCur->pSelfR0;
        pVM->pgm.s.pRamRangesXRC = pCur->pSelfRC;

        for (; pCur->pNextR3; pCur = pCur->pNextR3)
        {
            pCur->pNextR0 = pCur->pNextR3->pSelfR0;
            pCur->pNextRC = pCur->pNextR3->pSelfRC;
        }

        Assert(pCur->pNextR0 == NIL_RTR0PTR);
        Assert(pCur->pNextRC == NIL_RTRCPTR);
    }
    else
    {
        Assert(pVM->pgm.s.pRamRangesXR0 == NIL_RTR0PTR);
        Assert(pVM->pgm.s.pRamRangesXRC == NIL_RTRCPTR);
    }
    ASMAtomicIncU32(&pVM->pgm.s.idRamRangesGen);

    pgmR3PhysRebuildRamRangeSearchTrees(pVM);
}


/**
 * Links a new RAM range into the list.
 *
 * @param   pVM         The VM handle.
 * @param   pNew        Pointer to the new list entry.
 * @param   pPrev       Pointer to the previous list entry. If NULL, insert as head.
 */
static void pgmR3PhysLinkRamRange(PVM pVM, PPGMRAMRANGE pNew, PPGMRAMRANGE pPrev)
{
    AssertMsg(pNew->pszDesc, ("%RGp-%RGp\n", pNew->GCPhys, pNew->GCPhysLast));
    Assert((pNew->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pNew->pSelfR0 == MMHyperCCToR0(pVM, pNew));
    Assert((pNew->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pNew->pSelfRC == MMHyperCCToRC(pVM, pNew));

    pgmLock(pVM);

    PPGMRAMRANGE pRam = pPrev ? pPrev->pNextR3 : pVM->pgm.s.pRamRangesXR3;
    pNew->pNextR3 = pRam;
    pNew->pNextR0 = pRam ? pRam->pSelfR0 : NIL_RTR0PTR;
    pNew->pNextRC = pRam ? pRam->pSelfRC : NIL_RTRCPTR;

    if (pPrev)
    {
        pPrev->pNextR3 = pNew;
        pPrev->pNextR0 = pNew->pSelfR0;
        pPrev->pNextRC = pNew->pSelfRC;
    }
    else
    {
        pVM->pgm.s.pRamRangesXR3 = pNew;
        pVM->pgm.s.pRamRangesXR0 = pNew->pSelfR0;
        pVM->pgm.s.pRamRangesXRC = pNew->pSelfRC;
    }
    ASMAtomicIncU32(&pVM->pgm.s.idRamRangesGen);

    pgmR3PhysRebuildRamRangeSearchTrees(pVM);
    pgmUnlock(pVM);
}


/**
 * Unlink an existing RAM range from the list.
 *
 * @param   pVM         The VM handle.
 * @param   pRam        Pointer to the new list entry.
 * @param   pPrev       Pointer to the previous list entry. If NULL, insert as head.
 */
static void pgmR3PhysUnlinkRamRange2(PVM pVM, PPGMRAMRANGE pRam, PPGMRAMRANGE pPrev)
{
    Assert(pPrev ? pPrev->pNextR3 == pRam : pVM->pgm.s.pRamRangesXR3 == pRam);
    Assert((pRam->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pRam->pSelfR0 == MMHyperCCToR0(pVM, pRam));
    Assert((pRam->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING) || pRam->pSelfRC == MMHyperCCToRC(pVM, pRam));

    pgmLock(pVM);

    PPGMRAMRANGE pNext = pRam->pNextR3;
    if (pPrev)
    {
        pPrev->pNextR3 = pNext;
        pPrev->pNextR0 = pNext ? pNext->pSelfR0 : NIL_RTR0PTR;
        pPrev->pNextRC = pNext ? pNext->pSelfRC : NIL_RTRCPTR;
    }
    else
    {
        Assert(pVM->pgm.s.pRamRangesXR3 == pRam);
        pVM->pgm.s.pRamRangesXR3 = pNext;
        pVM->pgm.s.pRamRangesXR0 = pNext ? pNext->pSelfR0 : NIL_RTR0PTR;
        pVM->pgm.s.pRamRangesXRC = pNext ? pNext->pSelfRC : NIL_RTRCPTR;
    }
    ASMAtomicIncU32(&pVM->pgm.s.idRamRangesGen);

    pgmR3PhysRebuildRamRangeSearchTrees(pVM);
    pgmUnlock(pVM);
}


/**
 * Unlink an existing RAM range from the list.
 *
 * @param   pVM         The VM handle.
 * @param   pRam        Pointer to the new list entry.
 */
static void pgmR3PhysUnlinkRamRange(PVM pVM, PPGMRAMRANGE pRam)
{
    pgmLock(pVM);

    /* find prev. */
    PPGMRAMRANGE pPrev = NULL;
    PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesXR3;
    while (pCur != pRam)
    {
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }
    AssertFatal(pCur);

    pgmR3PhysUnlinkRamRange2(pVM, pRam, pPrev);
    pgmUnlock(pVM);
}


/**
 * Frees a range of pages, replacing them with ZERO pages of the specified type.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRam        The RAM range in which the pages resides.
 * @param   GCPhys      The address of the first page.
 * @param   GCPhysLast  The address of the last page.
 * @param   uType       The page type to replace then with.
 */
static int pgmR3PhysFreePageRange(PVM pVM, PPGMRAMRANGE pRam, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast, uint8_t uType)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    uint32_t            cPendingPages = 0;
    PGMMFREEPAGESREQ    pReq;
    int rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
    AssertLogRelRCReturn(rc, rc);

    /* Iterate the pages. */
    PPGMPAGE pPageDst   = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
    uint32_t cPagesLeft = ((GCPhysLast - GCPhys) >> PAGE_SHIFT) + 1;
    while (cPagesLeft-- > 0)
    {
        rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPageDst, GCPhys);
        AssertLogRelRCReturn(rc, rc); /* We're done for if this goes wrong. */

        PGM_PAGE_SET_TYPE(pVM, pPageDst, uType);

        GCPhys += PAGE_SIZE;
        pPageDst++;
    }

    if (cPendingPages)
    {
        rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
        AssertLogRelRCReturn(rc, rc);
    }
    GMMR3FreePagesCleanup(pReq);

    return rc;
}

#if HC_ARCH_BITS == 64 && (defined(RT_OS_WINDOWS) || defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD))

/**
 * Rendezvous callback used by PGMR3ChangeMemBalloon that changes the memory balloon size
 *
 * This is only called on one of the EMTs while the other ones are waiting for
 * it to complete this function.
 *
 * @returns VINF_SUCCESS (VBox strict status code).
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU for the EMT we're being called on. Unused.
 * @param   pvUser      User parameter
 */
static DECLCALLBACK(VBOXSTRICTRC) pgmR3PhysChangeMemBalloonRendezvous(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    uintptr_t          *paUser          = (uintptr_t *)pvUser;
    bool                fInflate        = !!paUser[0];
    unsigned            cPages          = paUser[1];
    RTGCPHYS           *paPhysPage      = (RTGCPHYS *)paUser[2];
    uint32_t            cPendingPages   = 0;
    PGMMFREEPAGESREQ    pReq;
    int                 rc;

    Log(("pgmR3PhysChangeMemBalloonRendezvous: %s %x pages\n", (fInflate) ? "inflate" : "deflate", cPages));
    pgmLock(pVM);

    if (fInflate)
    {
        /* Flush the PGM pool cache as we might have stale references to pages that we just freed. */
        pgmR3PoolClearAllRendezvous(pVM, pVCpu, NULL);

        /* Replace pages with ZERO pages. */
        rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
        if (RT_FAILURE(rc))
        {
            pgmUnlock(pVM);
            AssertLogRelRC(rc);
            return rc;
        }

        /* Iterate the pages. */
        for (unsigned i = 0; i < cPages; i++)
        {
            PPGMPAGE pPage = pgmPhysGetPage(pVM, paPhysPage[i]);
            if (    pPage == NULL
                ||  PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_RAM)
            {
                Log(("pgmR3PhysChangeMemBalloonRendezvous: invalid physical page %RGp pPage->u3Type=%d\n", paPhysPage[i], pPage ? PGM_PAGE_GET_TYPE(pPage) : 0));
                break;
            }

            LogFlow(("balloon page: %RGp\n", paPhysPage[i]));

            /* Flush the shadow PT if this page was previously used as a guest page table. */
            pgmPoolFlushPageByGCPhys(pVM, paPhysPage[i]);

            rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPage, paPhysPage[i]);
            if (RT_FAILURE(rc))
            {
                pgmUnlock(pVM);
                AssertLogRelRC(rc);
                return rc;
            }
            Assert(PGM_PAGE_IS_ZERO(pPage));
            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_BALLOONED);
        }

        if (cPendingPages)
        {
            rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
            if (RT_FAILURE(rc))
            {
                pgmUnlock(pVM);
                AssertLogRelRC(rc);
                return rc;
            }
        }
        GMMR3FreePagesCleanup(pReq);
    }
    else
    {
        /* Iterate the pages. */
        for (unsigned i = 0; i < cPages; i++)
        {
            PPGMPAGE pPage = pgmPhysGetPage(pVM, paPhysPage[i]);
            AssertBreak(pPage && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM);

            LogFlow(("Free ballooned page: %RGp\n", paPhysPage[i]));

            Assert(PGM_PAGE_IS_BALLOONED(pPage));

            /* Change back to zero page. */
            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
        }

        /* Note that we currently do not map any ballooned pages in our shadow page tables, so no need to flush the pgm pool. */
    }

    /* Notify GMM about the balloon change. */
    rc = GMMR3BalloonedPages(pVM, (fInflate) ? GMMBALLOONACTION_INFLATE : GMMBALLOONACTION_DEFLATE, cPages);
    if (RT_SUCCESS(rc))
    {
        if (!fInflate)
        {
            Assert(pVM->pgm.s.cBalloonedPages >= cPages);
            pVM->pgm.s.cBalloonedPages -= cPages;
        }
        else
            pVM->pgm.s.cBalloonedPages += cPages;
    }

    pgmUnlock(pVM);

    /* Flush the recompiler's TLB as well. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
        CPUMSetChangedFlags(&pVM->aCpus[i], CPUM_CHANGED_GLOBAL_TLB_FLUSH);

    AssertLogRelRC(rc);
    return rc;
}


/**
 * Frees a range of ram pages, replacing them with ZERO pages; helper for PGMR3PhysFreeRamPages
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   fInflate    Inflate or deflate memory balloon
 * @param   cPages      Number of pages to free
 * @param   paPhysPage  Array of guest physical addresses
 */
static DECLCALLBACK(void) pgmR3PhysChangeMemBalloonHelper(PVM pVM, bool fInflate, unsigned cPages, RTGCPHYS *paPhysPage)
{
    uintptr_t paUser[3];

    paUser[0] = fInflate;
    paUser[1] = cPages;
    paUser[2] = (uintptr_t)paPhysPage;
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3PhysChangeMemBalloonRendezvous, (void *)paUser);
    AssertRC(rc);

    /* Made a copy in PGMR3PhysFreeRamPages; free it here. */
    RTMemFree(paPhysPage);
}

#endif /* 64-bit host && (Windows || Solaris || Linux || FreeBSD) */

/**
 * Inflate or deflate a memory balloon
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   fInflate    Inflate or deflate memory balloon
 * @param   cPages      Number of pages to free
 * @param   paPhysPage  Array of guest physical addresses
 */
VMMR3DECL(int) PGMR3PhysChangeMemBalloon(PVM pVM, bool fInflate, unsigned cPages, RTGCPHYS *paPhysPage)
{
    /* This must match GMMR0Init; currently we only support memory ballooning on all 64-bit hosts except Mac OS X */
#if HC_ARCH_BITS == 64 && (defined(RT_OS_WINDOWS) || defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD))
    int rc;

    /* Older additions (ancient non-functioning balloon code) pass wrong physical addresses. */
    AssertReturn(!(paPhysPage[0] & 0xfff), VERR_INVALID_PARAMETER);

    /* We own the IOM lock here and could cause a deadlock by waiting for another VCPU that is blocking on the IOM lock.
     * In the SMP case we post a request packet to postpone the job.
     */
    if (pVM->cCpus > 1)
    {
        unsigned cbPhysPage = cPages * sizeof(paPhysPage[0]);
        RTGCPHYS *paPhysPageCopy = (RTGCPHYS *)RTMemAlloc(cbPhysPage);
        AssertReturn(paPhysPageCopy, VERR_NO_MEMORY);

        memcpy(paPhysPageCopy, paPhysPage, cbPhysPage);

        rc = VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE, (PFNRT)pgmR3PhysChangeMemBalloonHelper, 4, pVM, fInflate, cPages, paPhysPageCopy);
        AssertRC(rc);
    }
    else
    {
        uintptr_t paUser[3];

        paUser[0] = fInflate;
        paUser[1] = cPages;
        paUser[2] = (uintptr_t)paPhysPage;
        rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3PhysChangeMemBalloonRendezvous, (void *)paUser);
        AssertRC(rc);
    }
    return rc;

#else
    NOREF(pVM); NOREF(fInflate); NOREF(cPages); NOREF(paPhysPage);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Rendezvous callback used by PGMR3WriteProtectRAM that write protects all
 * physical RAM.
 *
 * This is only called on one of the EMTs while the other ones are waiting for
 * it to complete this function.
 *
 * @returns VINF_SUCCESS (VBox strict status code).
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU for the EMT we're being called on. Unused.
 * @param   pvUser      User parameter, unused.
 */
static DECLCALLBACK(VBOXSTRICTRC) pgmR3PhysWriteProtectRAMRendezvous(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser); NOREF(pVCpu);

    pgmLock(pVM);
#ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
    pgmPoolResetDirtyPages(pVM);
#endif

    /** @todo pointless to write protect the physical page pointed to by RSP. */

    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangesX);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        uint32_t cPages = pRam->cb >> PAGE_SHIFT;
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            PPGMPAGE    pPage = &pRam->aPages[iPage];
            PGMPAGETYPE enmPageType = (PGMPAGETYPE)PGM_PAGE_GET_TYPE(pPage);

            if (    RT_LIKELY(enmPageType == PGMPAGETYPE_RAM)
                ||  enmPageType == PGMPAGETYPE_MMIO2)
            {
                /*
                 * A RAM page.
                 */
                switch (PGM_PAGE_GET_STATE(pPage))
                {
                    case PGM_PAGE_STATE_ALLOCATED:
                        /** @todo Optimize this: Don't always re-enable write
                         * monitoring if the page is known to be very busy. */
                        if (PGM_PAGE_IS_WRITTEN_TO(pPage))
                        {
                            PGM_PAGE_CLEAR_WRITTEN_TO(pVM, pPage);
                            /* Remember this dirty page for the next (memory) sync. */
                            PGM_PAGE_SET_FT_DIRTY(pPage);
                        }

                        pgmPhysPageWriteMonitor(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT));
                        break;

                    case PGM_PAGE_STATE_SHARED:
                        AssertFailed();
                        break;

                    case PGM_PAGE_STATE_WRITE_MONITORED:    /* nothing to change. */
                    default:
                        break;
                }
            }
        }
    }
    pgmR3PoolWriteProtectPages(pVM);
    PGM_INVL_ALL_VCPU_TLBS(pVM);
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        CPUMSetChangedFlags(&pVM->aCpus[idCpu], CPUM_CHANGED_GLOBAL_TLB_FLUSH);

    pgmUnlock(pVM);
    return rc;
}

/**
 * Protect all physical RAM to monitor writes
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) PGMR3PhysWriteProtectRAM(PVM pVM)
{
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);

    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3PhysWriteProtectRAMRendezvous, NULL);
    AssertRC(rc);
    return rc;
}

/**
 * Enumerate all dirty FT pages.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pfnEnum     Enumerate callback handler.
 * @param   pvUser      Enumerate callback handler parameter.
 */
VMMR3DECL(int) PGMR3PhysEnumDirtyFTPages(PVM pVM, PFNPGMENUMDIRTYFTPAGES pfnEnum, void *pvUser)
{
    int rc = VINF_SUCCESS;

    pgmLock(pVM);
    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangesX);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        uint32_t cPages = pRam->cb >> PAGE_SHIFT;
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            PPGMPAGE    pPage       = &pRam->aPages[iPage];
            PGMPAGETYPE enmPageType = (PGMPAGETYPE)PGM_PAGE_GET_TYPE(pPage);

            if (    RT_LIKELY(enmPageType == PGMPAGETYPE_RAM)
                ||  enmPageType == PGMPAGETYPE_MMIO2)
            {
                /*
                 * A RAM page.
                 */
                switch (PGM_PAGE_GET_STATE(pPage))
                {
                    case PGM_PAGE_STATE_ALLOCATED:
                    case PGM_PAGE_STATE_WRITE_MONITORED:
                        if (   !PGM_PAGE_IS_WRITTEN_TO(pPage)  /* not very recently updated? */
                            && PGM_PAGE_IS_FT_DIRTY(pPage))
                        {
                            unsigned       cbPageRange = PAGE_SIZE;
                            unsigned       iPageClean  = iPage + 1;
                            RTGCPHYS       GCPhysPage  = pRam->GCPhys + iPage * PAGE_SIZE;
                            uint8_t       *pu8Page     = NULL;
                            PGMPAGEMAPLOCK Lock;

                            /* Find the next clean page, so we can merge adjacent dirty pages. */
                            for (; iPageClean < cPages; iPageClean++)
                            {
                                PPGMPAGE pPageNext = &pRam->aPages[iPageClean];
                                if (    RT_UNLIKELY(PGM_PAGE_GET_TYPE(pPageNext) != PGMPAGETYPE_RAM)
                                    ||  PGM_PAGE_GET_STATE(pPageNext) != PGM_PAGE_STATE_ALLOCATED
                                    ||  PGM_PAGE_IS_WRITTEN_TO(pPageNext)
                                    ||  !PGM_PAGE_IS_FT_DIRTY(pPageNext)
                                    /* Crossing a chunk boundary? */
                                    ||  (GCPhysPage & GMM_PAGEID_IDX_MASK) != ((GCPhysPage + cbPageRange) & GMM_PAGEID_IDX_MASK)
                                    )
                                    break;

                                cbPageRange += PAGE_SIZE;
                            }

                            rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhysPage, (const void **)&pu8Page, &Lock);
                            if (RT_SUCCESS(rc))
                            {
                                /** @todo this is risky; the range might be changed, but little choice as the sync
                                 *  costs a lot of time. */
                                pgmUnlock(pVM);
                                pfnEnum(pVM, GCPhysPage, pu8Page, cbPageRange, pvUser);
                                pgmLock(pVM);
                                PGMPhysReleasePageMappingLock(pVM, &Lock);
                            }

                            for (iPage; iPage < iPageClean; iPage++)
                                PGM_PAGE_CLEAR_FT_DIRTY(&pRam->aPages[iPage]);

                            iPage = iPageClean - 1;
                        }
                        break;
                }
            }
        }
    }
    pgmUnlock(pVM);
    return rc;
}


/**
 * Gets the number of ram ranges.
 *
 * @returns Number of ram ranges.  Returns UINT32_MAX if @a pVM is invalid.
 * @param   pVM             The VM handle.
 */
VMMR3DECL(uint32_t) PGMR3PhysGetRamRangeCount(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, UINT32_MAX);

    pgmLock(pVM);
    uint32_t cRamRanges = 0;
    for (PPGMRAMRANGE pCur = pVM->pgm.s.CTX_SUFF(pRamRangesX); pCur; pCur = pCur->CTX_SUFF(pNext))
        cRamRanges++;
    pgmUnlock(pVM);
    return cRamRanges;
}


/**
 * Get information about a range.
 *
 * @returns VINF_SUCCESS or VERR_OUT_OF_RANGE.
 * @param   pVM             The VM handle
 * @param   iRange          The ordinal of the range.
 * @param   pGCPhysStart    Where to return the start of the range. Optional.
 * @param   pGCPhysLast     Where to return the address of the last byte in the
 *                          range. Optional.
 * @param   pfIsMmio        Where to indicate that this is a pure MMIO range.
 *                          Optional.
 */
VMMR3DECL(int) PGMR3PhysGetRange(PVM pVM, uint32_t iRange, PRTGCPHYS pGCPhysStart, PRTGCPHYS pGCPhysLast,
                                 const char **ppszDesc, bool *pfIsMmio)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    pgmLock(pVM);
    uint32_t iCurRange = 0;
    for (PPGMRAMRANGE pCur = pVM->pgm.s.CTX_SUFF(pRamRangesX); pCur; pCur = pCur->CTX_SUFF(pNext), iCurRange++)
        if (iCurRange == iRange)
        {
            if (pGCPhysStart)
                *pGCPhysStart = pCur->GCPhys;
            if (pGCPhysLast)
                *pGCPhysLast  = pCur->GCPhysLast;
            if (ppszDesc)
                *ppszDesc     = pCur->pszDesc;
            if (pfIsMmio)
                *pfIsMmio     = !!(pCur->fFlags & PGM_RAM_RANGE_FLAGS_AD_HOC_MMIO);

            pgmUnlock(pVM);
            return VINF_SUCCESS;
        }
    pgmUnlock(pVM);
    return VERR_OUT_OF_RANGE;
}


/**
 * Query the amount of free memory inside VMMR0
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pcbAllocMem         Where to return the amount of memory allocated
 *                              by VMs.
 * @param   pcbFreeMem          Where to return the amount of memory that is
 *                              allocated from the host but not currently used
 *                              by any VMs.
 * @param   pcbBallonedMem      Where to return the sum of memory that is
 *                              currently ballooned by the VMs.
 * @param   pcbSharedMem        Where to return the amount of memory that is
 *                              currently shared.
 */
VMMR3DECL(int) PGMR3QueryGlobalMemoryStats(PVM pVM, uint64_t *pcbAllocMem, uint64_t *pcbFreeMem,
                                           uint64_t *pcbBallonedMem, uint64_t *pcbSharedMem)
{
    uint64_t cAllocPages   = 0;
    uint64_t cFreePages    = 0;
    uint64_t cBalloonPages = 0;
    uint64_t cSharedPages  = 0;
    int rc = GMMR3QueryHypervisorMemoryStats(pVM, &cAllocPages, &cFreePages, &cBalloonPages, &cSharedPages);
    AssertRCReturn(rc, rc);

    if (pcbAllocMem)
        *pcbAllocMem    = cAllocPages * _4K;

    if (pcbFreeMem)
        *pcbFreeMem     = cFreePages * _4K;

    if (pcbBallonedMem)
        *pcbBallonedMem = cBalloonPages * _4K;

    if (pcbSharedMem)
        *pcbSharedMem   = cSharedPages * _4K;

    Log(("PGMR3QueryVMMMemoryStats: all=%llx free=%llx ballooned=%llx shared=%llx\n",
         cAllocPages, cFreePages, cBalloonPages, cSharedPages));
    return VINF_SUCCESS;
}


/**
 * Query memory stats for the VM.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pcbTotalMem         Where to return total amount memory the VM may
 *                              possibly use.
 * @param   pcbPrivateMem       Where to return the amount of private memory
 *                              currently allocated.
 * @param   pcbSharedMem        Where to return the amount of actually shared
 *                              memory currently used by the VM.
 * @param   pcbZeroMem          Where to return the amount of memory backed by
 *                              zero pages.
 *
 * @remarks The total mem is normally larger than the sum of the three
 *          components.  There are two reasons for this, first the amount of
 *          shared memory is what we're sure is shared instead of what could
 *          possibly be shared with someone.  Secondly, because the total may
 *          include some pure MMIO pages that doesn't go into any of the three
 *          sub-counts.
 *
 * @todo Why do we return reused shared pages instead of anything that could
 *       potentially be shared?  Doesn't this mean the first VM gets a much
 *       lower number of shared pages?
 */
VMMR3DECL(int) PGMR3QueryMemoryStats(PVM pVM, uint64_t *pcbTotalMem, uint64_t *pcbPrivateMem,
                                     uint64_t *pcbSharedMem, uint64_t *pcbZeroMem)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    if (pcbTotalMem)
        *pcbTotalMem    = (uint64_t)pVM->pgm.s.cAllPages            * PAGE_SIZE;

    if (pcbPrivateMem)
        *pcbPrivateMem  = (uint64_t)pVM->pgm.s.cPrivatePages        * PAGE_SIZE;

    if (pcbSharedMem)
        *pcbSharedMem   = (uint64_t)pVM->pgm.s.cReusedSharedPages   * PAGE_SIZE;

    if (pcbZeroMem)
        *pcbZeroMem     = (uint64_t)pVM->pgm.s.cZeroPages           * PAGE_SIZE;

    Log(("PGMR3QueryMemoryStats: all=%x private=%x reused=%x zero=%x\n", pVM->pgm.s.cAllPages, pVM->pgm.s.cPrivatePages, pVM->pgm.s.cReusedSharedPages, pVM->pgm.s.cZeroPages));
    return VINF_SUCCESS;
}


/**
 * PGMR3PhysRegisterRam worker that initializes and links a RAM range.
 *
 * @param   pVM             The VM handle.
 * @param   pNew            The new RAM range.
 * @param   GCPhys          The address of the RAM range.
 * @param   GCPhysLast      The last address of the RAM range.
 * @param   RCPtrNew        The RC address if the range is floating. NIL_RTRCPTR
 *                          if in HMA.
 * @param   R0PtrNew        Ditto for R0.
 * @param   pszDesc         The description.
 * @param   pPrev           The previous RAM range (for linking).
 */
static void pgmR3PhysInitAndLinkRamRange(PVM pVM, PPGMRAMRANGE pNew, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                         RTRCPTR RCPtrNew, RTR0PTR R0PtrNew, const char *pszDesc, PPGMRAMRANGE pPrev)
{
    /*
     * Initialize the range.
     */
    pNew->pSelfR0       = R0PtrNew != NIL_RTR0PTR ? R0PtrNew : MMHyperCCToR0(pVM, pNew);
    pNew->pSelfRC       = RCPtrNew != NIL_RTRCPTR ? RCPtrNew : MMHyperCCToRC(pVM, pNew);
    pNew->GCPhys        = GCPhys;
    pNew->GCPhysLast    = GCPhysLast;
    pNew->cb            = GCPhysLast - GCPhys + 1;
    pNew->pszDesc       = pszDesc;
    pNew->fFlags        = RCPtrNew != NIL_RTRCPTR ? PGM_RAM_RANGE_FLAGS_FLOATING : 0;
    pNew->pvR3          = NULL;
    pNew->paLSPages     = NULL;

    uint32_t const cPages = pNew->cb >> PAGE_SHIFT;
    RTGCPHYS iPage = cPages;
    while (iPage-- > 0)
        PGM_PAGE_INIT_ZERO(&pNew->aPages[iPage], pVM, PGMPAGETYPE_RAM);

    /* Update the page count stats. */
    pVM->pgm.s.cZeroPages += cPages;
    pVM->pgm.s.cAllPages  += cPages;

    /*
     * Link it.
     */
    pgmR3PhysLinkRamRange(pVM, pNew, pPrev);
}


/**
 * Relocate a floating RAM range.
 *
 * @copydoc FNPGMRELOCATE.
 */
static DECLCALLBACK(bool) pgmR3PhysRamRangeRelocate(PVM pVM, RTGCPTR GCPtrOld, RTGCPTR GCPtrNew, PGMRELOCATECALL enmMode, void *pvUser)
{
    PPGMRAMRANGE pRam = (PPGMRAMRANGE)pvUser;
    Assert(pRam->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING);
    Assert(pRam->pSelfRC == GCPtrOld + PAGE_SIZE);

    switch (enmMode)
    {
        case PGMRELOCATECALL_SUGGEST:
            return true;

        case PGMRELOCATECALL_RELOCATE:
        {
            /*
             * Update myself, then relink all the ranges and flush the RC TLB.
             */
            pgmLock(pVM);

            pRam->pSelfRC = (RTRCPTR)(GCPtrNew + PAGE_SIZE);

            pgmR3PhysRelinkRamRanges(pVM);
            for (unsigned i = 0; i < PGM_RAMRANGE_TLB_ENTRIES; i++)
                pVM->pgm.s.apRamRangesTlbRC[i] = NIL_RTRCPTR;

            pgmUnlock(pVM);
            return true;
        }

        default:
            AssertFailedReturn(false);
    }
}


/**
 * PGMR3PhysRegisterRam worker that registers a high chunk.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   GCPhys          The address of the RAM.
 * @param   cRamPages       The number of RAM pages to register.
 * @param   cbChunk         The size of the PGMRAMRANGE guest mapping.
 * @param   iChunk          The chunk number.
 * @param   pszDesc         The RAM range description.
 * @param   ppPrev          Previous RAM range pointer. In/Out.
 */
static int pgmR3PhysRegisterHighRamChunk(PVM pVM, RTGCPHYS GCPhys, uint32_t cRamPages,
                                         uint32_t cbChunk, uint32_t iChunk, const char *pszDesc,
                                         PPGMRAMRANGE *ppPrev)
{
    const char *pszDescChunk = iChunk == 0
                             ? pszDesc
                             : MMR3HeapAPrintf(pVM, MM_TAG_PGM_PHYS, "%s (#%u)", pszDesc, iChunk + 1);
    AssertReturn(pszDescChunk, VERR_NO_MEMORY);

    /*
     * Allocate memory for the new chunk.
     */
    size_t const cChunkPages  = RT_ALIGN_Z(RT_UOFFSETOF(PGMRAMRANGE, aPages[cRamPages]), PAGE_SIZE) >> PAGE_SHIFT;
    PSUPPAGE     paChunkPages = (PSUPPAGE)RTMemTmpAllocZ(sizeof(SUPPAGE) * cChunkPages);
    AssertReturn(paChunkPages, VERR_NO_TMP_MEMORY);
    RTR0PTR      R0PtrChunk   = NIL_RTR0PTR;
    void        *pvChunk      = NULL;
    int rc = SUPR3PageAllocEx(cChunkPages, 0 /*fFlags*/, &pvChunk,
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
                              VMMIsHwVirtExtForced(pVM) ? &R0PtrChunk : NULL,
#else
                              NULL,
#endif
                              paChunkPages);
    if (RT_SUCCESS(rc))
    {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        if (!VMMIsHwVirtExtForced(pVM))
            R0PtrChunk = NIL_RTR0PTR;
#else
        R0PtrChunk = (uintptr_t)pvChunk;
#endif
        memset(pvChunk, 0, cChunkPages << PAGE_SHIFT);

        PPGMRAMRANGE pNew = (PPGMRAMRANGE)pvChunk;

        /*
         * Create a mapping and map the pages into it.
         * We push these in below the HMA.
         */
        RTGCPTR GCPtrChunkMap = pVM->pgm.s.GCPtrPrevRamRangeMapping - cbChunk;
        rc = PGMR3MapPT(pVM, GCPtrChunkMap, cbChunk, 0 /*fFlags*/, pgmR3PhysRamRangeRelocate, pNew, pszDescChunk);
        if (RT_SUCCESS(rc))
        {
            pVM->pgm.s.GCPtrPrevRamRangeMapping = GCPtrChunkMap;

            RTGCPTR const   GCPtrChunk = GCPtrChunkMap + PAGE_SIZE;
            RTGCPTR         GCPtrPage  = GCPtrChunk;
            for (uint32_t iPage = 0; iPage < cChunkPages && RT_SUCCESS(rc); iPage++, GCPtrPage += PAGE_SIZE)
                rc = PGMMap(pVM, GCPtrPage, paChunkPages[iPage].Phys, PAGE_SIZE, 0);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Ok, init and link the range.
                 */
                pgmR3PhysInitAndLinkRamRange(pVM, pNew, GCPhys, GCPhys + ((RTGCPHYS)cRamPages << PAGE_SHIFT) - 1,
                                             (RTRCPTR)GCPtrChunk, R0PtrChunk, pszDescChunk, *ppPrev);
                *ppPrev = pNew;
            }
        }

        if (RT_FAILURE(rc))
            SUPR3PageFreeEx(pvChunk, cChunkPages);
    }

    RTMemTmpFree(paChunkPages);
    return rc;
}


/**
 * Sets up a range RAM.
 *
 * This will check for conflicting registrations, make a resource
 * reservation for the memory (with GMM), and setup the per-page
 * tracking structures (PGMPAGE).
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   GCPhys          The physical address of the RAM.
 * @param   cb              The size of the RAM.
 * @param   pszDesc         The description - not copied, so, don't free or change it.
 */
VMMR3DECL(int) PGMR3PhysRegisterRam(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, const char *pszDesc)
{
   /*
     * Validate input.
     */
    Log(("PGMR3PhysRegisterRam: GCPhys=%RGp cb=%RGp pszDesc=%s\n", GCPhys, cb, pszDesc));
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertMsgReturn(GCPhysLast > GCPhys, ("The range wraps! GCPhys=%RGp cb=%RGp\n", GCPhys, cb), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);

    pgmLock(pVM);

    /*
     * Find range location and check for conflicts.
     * (We don't lock here because the locking by EMT is only required on update.)
     */
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pRam = pVM->pgm.s.pRamRangesXR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhysLast >= pRam->GCPhys
            &&  GCPhys     <= pRam->GCPhysLast)
            AssertLogRelMsgFailedReturn(("%RGp-%RGp (%s) conflicts with existing %RGp-%RGp (%s)\n",
                                         GCPhys, GCPhysLast, pszDesc,
                                         pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                        VERR_PGM_RAM_CONFLICT);

        /* next */
        pPrev = pRam;
        pRam = pRam->pNextR3;
    }

    /*
     * Register it with GMM (the API bitches).
     */
    const RTGCPHYS cPages = cb >> PAGE_SHIFT;
    int rc = MMR3IncreaseBaseReservation(pVM, cPages);
    if (RT_FAILURE(rc))
    {
        pgmUnlock(pVM);
        return rc;
    }

    if (    GCPhys >= _4G
        &&  cPages > 256)
    {
        /*
         * The PGMRAMRANGE structures for the high memory can get very big.
         * In order to avoid SUPR3PageAllocEx allocation failures due to the
         * allocation size limit there and also to avoid being unable to find
         * guest mapping space for them, we split this memory up into 4MB in
         * (potential) raw-mode configs and 16MB chunks in forced AMD-V/VT-x
         * mode.
         *
         * The first and last page of each mapping are guard pages and marked
         * not-present. So, we've got 4186112 and 16769024 bytes available for
         * the PGMRAMRANGE structure.
         *
         * Note! The sizes used here will influence the saved state.
         */
        uint32_t cbChunk;
        uint32_t cPagesPerChunk;
        if (VMMIsHwVirtExtForced(pVM))
        {
            cbChunk = 16U*_1M;
            cPagesPerChunk = 1048048; /* max ~1048059 */
            AssertCompile(sizeof(PGMRAMRANGE) + sizeof(PGMPAGE) * 1048048 < 16U*_1M - PAGE_SIZE * 2);
        }
        else
        {
            cbChunk = 4U*_1M;
            cPagesPerChunk = 261616; /* max ~261627 */
            AssertCompile(sizeof(PGMRAMRANGE) + sizeof(PGMPAGE) * 261616  <  4U*_1M - PAGE_SIZE * 2);
        }
        AssertRelease(RT_UOFFSETOF(PGMRAMRANGE, aPages[cPagesPerChunk]) + PAGE_SIZE * 2 <= cbChunk);

        RTGCPHYS cPagesLeft  = cPages;
        RTGCPHYS GCPhysChunk = GCPhys;
        uint32_t iChunk      = 0;
        while (cPagesLeft > 0)
        {
            uint32_t cPagesInChunk = cPagesLeft;
            if (cPagesInChunk > cPagesPerChunk)
                cPagesInChunk = cPagesPerChunk;

            rc = pgmR3PhysRegisterHighRamChunk(pVM, GCPhysChunk, cPagesInChunk, cbChunk, iChunk, pszDesc, &pPrev);
            AssertRCReturn(rc, rc);

            /* advance */
            GCPhysChunk += (RTGCPHYS)cPagesInChunk << PAGE_SHIFT;
            cPagesLeft  -= cPagesInChunk;
            iChunk++;
        }
    }
    else
    {
        /*
         * Allocate, initialize and link the new RAM range.
         */
        const size_t cbRamRange = RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]);
        PPGMRAMRANGE pNew;
        rc = MMR3HyperAllocOnceNoRel(pVM, cbRamRange, 0, MM_TAG_PGM_PHYS, (void **)&pNew);
        AssertLogRelMsgRCReturn(rc, ("cbRamRange=%zu\n", cbRamRange), rc);

        pgmR3PhysInitAndLinkRamRange(pVM, pNew, GCPhys, GCPhysLast, NIL_RTRCPTR, NIL_RTR0PTR, pszDesc, pPrev);
    }
    pgmPhysInvalidatePageMapTLB(pVM);
    pgmUnlock(pVM);

#ifdef VBOX_WITH_REM
    /*
     * Notify REM.
     */
    REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, REM_NOTIFY_PHYS_RAM_FLAGS_RAM);
#endif

    return VINF_SUCCESS;
}


/**
 * Worker called by PGMR3InitFinalize if we're configured to pre-allocate RAM.
 *
 * We do this late in the init process so that all the ROM and MMIO ranges have
 * been registered already and we don't go wasting memory on them.
 *
 * @returns VBox status code.
 *
 * @param   pVM     The VM handle.
 */
int pgmR3PhysRamPreAllocate(PVM pVM)
{
    Assert(pVM->pgm.s.fRamPreAlloc);
    Log(("pgmR3PhysRamPreAllocate: enter\n"));

    /*
     * Walk the RAM ranges and allocate all RAM pages, halt at
     * the first allocation error.
     */
    uint64_t cPages = 0;
    uint64_t NanoTS = RTTimeNanoTS();
    pgmLock(pVM);
    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3; pRam; pRam = pRam->pNextR3)
    {
        PPGMPAGE    pPage  = &pRam->aPages[0];
        RTGCPHYS    GCPhys = pRam->GCPhys;
        uint32_t    cLeft  = pRam->cb >> PAGE_SHIFT;
        while (cLeft-- > 0)
        {
            if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM)
            {
                switch (PGM_PAGE_GET_STATE(pPage))
                {
                    case PGM_PAGE_STATE_ZERO:
                    {
                        int rc = pgmPhysAllocPage(pVM, pPage, GCPhys);
                        if (RT_FAILURE(rc))
                        {
                            LogRel(("PGM: RAM Pre-allocation failed at %RGp (in %s) with rc=%Rrc\n", GCPhys, pRam->pszDesc, rc));
                            pgmUnlock(pVM);
                            return rc;
                        }
                        cPages++;
                        break;
                    }

                    case PGM_PAGE_STATE_BALLOONED:
                    case PGM_PAGE_STATE_ALLOCATED:
                    case PGM_PAGE_STATE_WRITE_MONITORED:
                    case PGM_PAGE_STATE_SHARED:
                        /* nothing to do here. */
                        break;
                }
            }

            /* next */
            pPage++;
            GCPhys += PAGE_SIZE;
        }
    }
    pgmUnlock(pVM);
    NanoTS = RTTimeNanoTS() - NanoTS;

    LogRel(("PGM: Pre-allocated %llu pages in %llu ms\n", cPages, NanoTS / 1000000));
    Log(("pgmR3PhysRamPreAllocate: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Resets (zeros) the RAM.
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
int pgmR3PhysRamReset(PVM pVM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    /* Reset the memory balloon. */
    int rc = GMMR3BalloonedPages(pVM, GMMBALLOONACTION_RESET, 0);
    AssertRC(rc);

#ifdef VBOX_WITH_PAGE_SHARING
    /* Clear all registered shared modules. */
    rc = GMMR3ResetSharedModules(pVM);
    AssertRC(rc);
#endif
    /* Reset counters. */
    pVM->pgm.s.cReusedSharedPages = 0;
    pVM->pgm.s.cBalloonedPages    = 0;

    /*
     * We batch up pages that should be freed instead of calling GMM for
     * each and every one of them.
     */
    uint32_t            cPendingPages = 0;
    PGMMFREEPAGESREQ    pReq;
    rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Walk the ram ranges.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3; pRam; pRam = pRam->pNextR3)
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT;
        AssertMsg(((RTGCPHYS)iPage << PAGE_SHIFT) == pRam->cb, ("%RGp %RGp\n", (RTGCPHYS)iPage << PAGE_SHIFT, pRam->cb));

#ifndef NO_RAM_RESET
        if (!pVM->pgm.s.fRamPreAlloc)
#else
        if (0)
#endif
        {
            /* Replace all RAM pages by ZERO pages. */
            while (iPage-- > 0)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                switch (PGM_PAGE_GET_TYPE(pPage))
                {
                    case PGMPAGETYPE_RAM:
                        /* Do not replace pages part of a 2 MB continuous range
                           with zero pages, but zero them instead. */
                        if (   PGM_PAGE_GET_PDE_TYPE(pPage) == PGM_PAGE_PDE_TYPE_PDE
                            || PGM_PAGE_GET_PDE_TYPE(pPage) == PGM_PAGE_PDE_TYPE_PDE_DISABLED)
                        {
                            void *pvPage;
                            rc = pgmPhysPageMap(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pvPage);
                            AssertLogRelRCReturn(rc, rc);
                            ASMMemZeroPage(pvPage);
                        }
                        else if (PGM_PAGE_IS_BALLOONED(pPage))
                        {
                            /* Turn into a zero page; the balloon status is lost when the VM reboots. */
                            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
                        }
                        else if (!PGM_PAGE_IS_ZERO(pPage))
                        {
                            rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT));
                            AssertLogRelRCReturn(rc, rc);
                        }
                        break;

                    case PGMPAGETYPE_MMIO2_ALIAS_MMIO:
                        pgmHandlerPhysicalResetAliasedPage(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT),
                                                           true /*fDoAccounting*/);
                        break;

                    case PGMPAGETYPE_MMIO2:
                    case PGMPAGETYPE_ROM_SHADOW: /* handled by pgmR3PhysRomReset. */
                    case PGMPAGETYPE_ROM:
                    case PGMPAGETYPE_MMIO:
                        break;
                    default:
                        AssertFailed();
                }
            } /* for each page */
        }
        else
        {
            /* Zero the memory. */
            while (iPage-- > 0)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                switch (PGM_PAGE_GET_TYPE(pPage))
                {
                    case PGMPAGETYPE_RAM:
                        switch (PGM_PAGE_GET_STATE(pPage))
                        {
                            case PGM_PAGE_STATE_ZERO:
                                break;

                            case PGM_PAGE_STATE_BALLOONED:
                                /* Turn into a zero page; the balloon status is lost when the VM reboots. */
                                PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
                                break;

                            case PGM_PAGE_STATE_SHARED:
                            case PGM_PAGE_STATE_WRITE_MONITORED:
                                rc = pgmPhysPageMakeWritable(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT));
                                AssertLogRelRCReturn(rc, rc);
                                /* no break */

                            case PGM_PAGE_STATE_ALLOCATED:
                            {
                                void *pvPage;
                                rc = pgmPhysPageMap(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pvPage);
                                AssertLogRelRCReturn(rc, rc);
#ifndef NO_RAM_RESET
                                ASMMemZeroPage(pvPage);
#endif
                                break;
                            }
                        }
                        break;

                    case PGMPAGETYPE_MMIO2_ALIAS_MMIO:
                        pgmHandlerPhysicalResetAliasedPage(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT),
                                                           true /*fDoAccounting*/);
                        break;

                    case PGMPAGETYPE_MMIO2:
                    case PGMPAGETYPE_ROM_SHADOW:
                    case PGMPAGETYPE_ROM:
                    case PGMPAGETYPE_MMIO:
                        break;
                    default:
                        AssertFailed();

                }
            } /* for each page */
        }

    }

    /*
     * Finish off any pages pending freeing.
     */
    if (cPendingPages)
    {
        rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
        AssertLogRelRCReturn(rc, rc);
    }
    GMMR3FreePagesCleanup(pReq);

    return VINF_SUCCESS;
}

/**
 * Frees all RAM during VM termination
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
int pgmR3PhysRamTerm(PVM pVM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    /* Reset the memory balloon. */
    int rc = GMMR3BalloonedPages(pVM, GMMBALLOONACTION_RESET, 0);
    AssertRC(rc);

#ifdef VBOX_WITH_PAGE_SHARING
    /* Clear all registered shared modules. */
    rc = GMMR3ResetSharedModules(pVM);
    AssertRC(rc);
#endif

    /*
     * We batch up pages that should be freed instead of calling GMM for
     * each and every one of them.
     */
    uint32_t            cPendingPages = 0;
    PGMMFREEPAGESREQ    pReq;
    rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Walk the ram ranges.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3; pRam; pRam = pRam->pNextR3)
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT;
        AssertMsg(((RTGCPHYS)iPage << PAGE_SHIFT) == pRam->cb, ("%RGp %RGp\n", (RTGCPHYS)iPage << PAGE_SHIFT, pRam->cb));

        /* Replace all RAM pages by ZERO pages. */
        while (iPage-- > 0)
        {
            PPGMPAGE pPage = &pRam->aPages[iPage];
            switch (PGM_PAGE_GET_TYPE(pPage))
            {
                case PGMPAGETYPE_RAM:
                    /* Free all shared pages. Private pages are automatically freed during GMM VM cleanup. */
                    if (PGM_PAGE_IS_SHARED(pPage))
                    {
                        rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPage, pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT));
                        AssertLogRelRCReturn(rc, rc);
                    }
                    break;

                case PGMPAGETYPE_MMIO2_ALIAS_MMIO:
                case PGMPAGETYPE_MMIO2:
                case PGMPAGETYPE_ROM_SHADOW: /* handled by pgmR3PhysRomReset. */
                case PGMPAGETYPE_ROM:
                case PGMPAGETYPE_MMIO:
                    break;
                default:
                    AssertFailed();
            }
        } /* for each page */
    }

    /*
     * Finish off any pages pending freeing.
     */
    if (cPendingPages)
    {
        rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
        AssertLogRelRCReturn(rc, rc);
    }
    GMMR3FreePagesCleanup(pReq);
    return VINF_SUCCESS;
}

/**
 * This is the interface IOM is using to register an MMIO region.
 *
 * It will check for conflicts and ensure that a RAM range structure
 * is present before calling the PGMR3HandlerPhysicalRegister API to
 * register the callbacks.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   GCPhys          The start of the MMIO region.
 * @param   cb              The size of the MMIO region.
 * @param   pfnHandlerR3    The address of the ring-3 handler. (IOMR3MMIOHandler)
 * @param   pvUserR3        The user argument for R3.
 * @param   pfnHandlerR0    The address of the ring-0 handler. (IOMMMIOHandler)
 * @param   pvUserR0        The user argument for R0.
 * @param   pfnHandlerRC    The address of the RC handler. (IOMMMIOHandler)
 * @param   pvUserRC        The user argument for RC.
 * @param   pszDesc         The description of the MMIO region.
 */
VMMR3DECL(int) PGMR3PhysMMIORegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb,
                                     R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                     R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                     RCPTRTYPE(PFNPGMRCPHYSHANDLER) pfnHandlerRC, RTRCPTR pvUserRC,
                                     R3PTRTYPE(const char *) pszDesc)
{
    /*
     * Assert on some assumption.
     */
    VM_ASSERT_EMT(pVM);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc, VERR_INVALID_PARAMETER);

    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Make sure there's a RAM range structure for the region.
     */
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    bool fRamExists = false;
    PPGMRAMRANGE pRamPrev = NULL;
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhysLast >= pRam->GCPhys
            &&  GCPhys     <= pRam->GCPhysLast)
        {
            /* Simplification: all within the same range. */
            AssertLogRelMsgReturnStmt(   GCPhys     >= pRam->GCPhys
                                      && GCPhysLast <= pRam->GCPhysLast,
                                      ("%RGp-%RGp (MMIO/%s) falls partly outside %RGp-%RGp (%s)\n",
                                       GCPhys, GCPhysLast, pszDesc,
                                       pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                      pgmUnlock(pVM),
                                      VERR_PGM_RAM_CONFLICT);

            /* Check that it's all RAM or MMIO pages. */
            PCPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
            uint32_t cLeft = cb >> PAGE_SHIFT;
            while (cLeft-- > 0)
            {
                AssertLogRelMsgReturnStmt(   PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM
                                          || PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO,
                                          ("%RGp-%RGp (MMIO/%s): %RGp is not a RAM or MMIO page - type=%d desc=%s\n",
                                           GCPhys, GCPhysLast, pszDesc, PGM_PAGE_GET_TYPE(pPage), pRam->pszDesc),
                                          pgmUnlock(pVM),
                                          VERR_PGM_RAM_CONFLICT);
                pPage++;
            }

            /* Looks good. */
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    PPGMRAMRANGE pNew;
    if (fRamExists)
    {
        pNew = NULL;

        /*
         * Make all the pages in the range MMIO/ZERO pages, freeing any
         * RAM pages currently mapped here. This might not be 100% correct
         * for PCI memory, but we're doing the same thing for MMIO2 pages.
         */
        rc = pgmR3PhysFreePageRange(pVM, pRam, GCPhys, GCPhysLast, PGMPAGETYPE_MMIO);
        AssertRCReturnStmt(rc, pgmUnlock(pVM), rc);

        /* Force a PGM pool flush as guest ram references have been changed. */
        /** @todo not entirely SMP safe; assuming for now the guest takes
         *   care of this internally (not touch mapped mmio while changing the
         *   mapping). */
        PVMCPU pVCpu = VMMGetCpu(pVM);
        pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
    }
    else
    {

        /*
         * No RAM range, insert an ad hoc one.
         *
         * Note that we don't have to tell REM about this range because
         * PGMHandlerPhysicalRegisterEx will do that for us.
         */
        Log(("PGMR3PhysMMIORegister: Adding ad hoc MMIO range for %RGp-%RGp %s\n", GCPhys, GCPhysLast, pszDesc));

        const uint32_t cPages = cb >> PAGE_SHIFT;
        const size_t cbRamRange = RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]);
        rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]), 16, MM_TAG_PGM_PHYS, (void **)&pNew);
        AssertLogRelMsgRCReturnStmt(rc, ("cbRamRange=%zu\n", cbRamRange), pgmUnlock(pVM), rc);

        /* Initialize the range. */
        pNew->pSelfR0       = MMHyperCCToR0(pVM, pNew);
        pNew->pSelfRC       = MMHyperCCToRC(pVM, pNew);
        pNew->GCPhys        = GCPhys;
        pNew->GCPhysLast    = GCPhysLast;
        pNew->cb            = cb;
        pNew->pszDesc       = pszDesc;
        pNew->fFlags        = PGM_RAM_RANGE_FLAGS_AD_HOC_MMIO;
        pNew->pvR3          = NULL;
        pNew->paLSPages     = NULL;

        uint32_t iPage = cPages;
        while (iPage-- > 0)
            PGM_PAGE_INIT_ZERO(&pNew->aPages[iPage], pVM, PGMPAGETYPE_MMIO);
        Assert(PGM_PAGE_GET_TYPE(&pNew->aPages[0]) == PGMPAGETYPE_MMIO);

        /* update the page count stats. */
        pVM->pgm.s.cPureMmioPages += cPages;
        pVM->pgm.s.cAllPages      += cPages;

        /* link it */
        pgmR3PhysLinkRamRange(pVM, pNew, pRamPrev);
    }

    /*
     * Register the access handler.
     */
    rc = PGMHandlerPhysicalRegisterEx(pVM, PGMPHYSHANDLERTYPE_MMIO, GCPhys, GCPhysLast,
                                      pfnHandlerR3, pvUserR3,
                                      pfnHandlerR0, pvUserR0,
                                      pfnHandlerRC, pvUserRC, pszDesc);
    if (    RT_FAILURE(rc)
        &&  !fRamExists)
    {
        pVM->pgm.s.cPureMmioPages -= cb >> PAGE_SHIFT;
        pVM->pgm.s.cAllPages      -= cb >> PAGE_SHIFT;

        /* remove the ad hoc range. */
        pgmR3PhysUnlinkRamRange2(pVM, pNew, pRamPrev);
        pNew->cb = pNew->GCPhys = pNew->GCPhysLast = NIL_RTGCPHYS;
        MMHyperFree(pVM, pRam);
    }
    pgmPhysInvalidatePageMapTLB(pVM);

    pgmUnlock(pVM);
    return rc;
}


/**
 * This is the interface IOM is using to register an MMIO region.
 *
 * It will take care of calling PGMHandlerPhysicalDeregister and clean up
 * any ad hoc PGMRAMRANGE left behind.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   GCPhys          The start of the MMIO region.
 * @param   cb              The size of the MMIO region.
 */
VMMR3DECL(int) PGMR3PhysMMIODeregister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    VM_ASSERT_EMT(pVM);

    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * First deregister the handler, then check if we should remove the ram range.
     */
    rc = PGMHandlerPhysicalDeregister(pVM, GCPhys);
    if (RT_SUCCESS(rc))
    {
        RTGCPHYS        GCPhysLast  = GCPhys + (cb - 1);
        PPGMRAMRANGE    pRamPrev    = NULL;
        PPGMRAMRANGE    pRam        = pVM->pgm.s.pRamRangesXR3;
        while (pRam && GCPhysLast >= pRam->GCPhys)
        {
            /** @todo We're being a bit too careful here. rewrite. */
            if (    GCPhysLast == pRam->GCPhysLast
                &&  GCPhys     == pRam->GCPhys)
            {
                Assert(pRam->cb == cb);

                /*
                 * See if all the pages are dead MMIO pages.
                 */
                uint32_t const  cPages   = cb >> PAGE_SHIFT;
                bool            fAllMMIO = true;
                uint32_t        iPage    = 0;
                uint32_t        cLeft    = cPages;
                while (cLeft-- > 0)
                {
                    PPGMPAGE    pPage    = &pRam->aPages[iPage];
                    if (    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO
                        /*|| not-out-of-action later */)
                    {
                        fAllMMIO = false;
                        Assert(PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO2_ALIAS_MMIO);
                        AssertMsgFailed(("%RGp %R[pgmpage]\n", pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), pPage));
                        break;
                    }
                    Assert(PGM_PAGE_IS_ZERO(pPage));
                    pPage++;
                }
                if (fAllMMIO)
                {
                    /*
                     * Ad-hoc range, unlink and free it.
                     */
                    Log(("PGMR3PhysMMIODeregister: Freeing ad hoc MMIO range for %RGp-%RGp %s\n",
                         GCPhys, GCPhysLast, pRam->pszDesc));

                    pVM->pgm.s.cAllPages      -= cPages;
                    pVM->pgm.s.cPureMmioPages -= cPages;

                    pgmR3PhysUnlinkRamRange2(pVM, pRam, pRamPrev);
                    pRam->cb = pRam->GCPhys = pRam->GCPhysLast = NIL_RTGCPHYS;
                    MMHyperFree(pVM, pRam);
                    break;
                }
            }

            /*
             * Range match? It will all be within one range (see PGMAllHandler.cpp).
             */
            if (    GCPhysLast >= pRam->GCPhys
                &&  GCPhys     <= pRam->GCPhysLast)
            {
                Assert(GCPhys     >= pRam->GCPhys);
                Assert(GCPhysLast <= pRam->GCPhysLast);

                /*
                 * Turn the pages back into RAM pages.
                 */
                uint32_t iPage = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
                uint32_t cLeft = cb >> PAGE_SHIFT;
                while (cLeft--)
                {
                    PPGMPAGE pPage = &pRam->aPages[iPage];
                    AssertMsg(PGM_PAGE_IS_MMIO(pPage), ("%RGp %R[pgmpage]\n", pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), pPage));
                    AssertMsg(PGM_PAGE_IS_ZERO(pPage), ("%RGp %R[pgmpage]\n", pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), pPage));
                    if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO)
                        PGM_PAGE_SET_TYPE(pVM, pPage, PGMPAGETYPE_RAM);
                }
                break;
            }

            /* next */
            pRamPrev = pRam;
            pRam = pRam->pNextR3;
        }
    }

    /* Force a PGM pool flush as guest ram references have been changed. */
    /** todo; not entirely SMP safe; assuming for now the guest takes care of this internally (not touch mapped mmio while changing the mapping). */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

    pgmPhysInvalidatePageMapTLB(pVM);
    pgmPhysInvalidRamRangeTlbs(pVM);
    pgmUnlock(pVM);
    return rc;
}


/**
 * Locate a MMIO2 range.
 *
 * @returns Pointer to the MMIO2 range.
 * @param   pVM             The VM handle.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region.
 */
DECLINLINE(PPGMMMIO2RANGE) pgmR3PhysMMIO2Find(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion)
{
    /*
     * Search the list.
     */
    for (PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3; pCur; pCur = pCur->pNextR3)
        if (   pCur->pDevInsR3 == pDevIns
            && pCur->iRegion == iRegion)
            return pCur;
    return NULL;
}


/**
 * Allocate and register an MMIO2 region.
 *
 * As mentioned elsewhere, MMIO2 is just RAM spelled differently.  It's RAM
 * associated with a device. It is also non-shared memory with a permanent
 * ring-3 mapping and page backing (presently).
 *
 * A MMIO2 range may overlap with base memory if a lot of RAM is configured for
 * the VM, in which case we'll drop the base memory pages.  Presently we will
 * make no attempt to preserve anything that happens to be present in the base
 * memory that is replaced, this is of course incorrectly but it's too much
 * effort.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *ppv pointing to the R3 mapping of the
 *          memory.
 * @retval  VERR_ALREADY_EXISTS if the region already exists.
 *
 * @param   pVM             The VM handle.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region number.  If the MMIO2 memory is a PCI
 *                          I/O region this number has to be the number of that
 *                          region. Otherwise it can be any number safe
 *                          UINT8_MAX.
 * @param   cb              The size of the region.  Must be page aligned.
 * @param   fFlags          Reserved for future use, must be zero.
 * @param   ppv             Where to store the pointer to the ring-3 mapping of
 *                          the memory.
 * @param   pszDesc         The description.
 */
VMMR3DECL(int) PGMR3PhysMMIO2Register(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc, VERR_INVALID_PARAMETER);
    AssertReturn(pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion) == NULL, VERR_ALREADY_EXISTS);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cb, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    const uint32_t cPages = cb >> PAGE_SHIFT;
    AssertLogRelReturn(((RTGCPHYS)cPages << PAGE_SHIFT) == cb, VERR_INVALID_PARAMETER);
    AssertLogRelReturn(cPages <= INT32_MAX / 2, VERR_NO_MEMORY);

    /*
     * For the 2nd+ instance, mangle the description string so it's unique.
     */
    if (pDevIns->iInstance > 0) /** @todo Move to PDMDevHlp.cpp and use a real string cache. */
    {
        pszDesc = MMR3HeapAPrintf(pVM, MM_TAG_PGM_PHYS, "%s [%u]", pszDesc, pDevIns->iInstance);
        if (!pszDesc)
            return VERR_NO_MEMORY;
    }

    /*
     * Try reserve and allocate the backing memory first as this is what is
     * most likely to fail.
     */
    int rc = MMR3AdjustFixedReservation(pVM, cPages, pszDesc);
    if (RT_SUCCESS(rc))
    {
        void *pvPages;
        PSUPPAGE paPages = (PSUPPAGE)RTMemTmpAlloc(cPages * sizeof(SUPPAGE));
        if (RT_SUCCESS(rc))
            rc = SUPR3PageAllocEx(cPages, 0 /*fFlags*/, &pvPages, NULL /*pR0Ptr*/, paPages);
        if (RT_SUCCESS(rc))
        {
            memset(pvPages, 0, cPages * PAGE_SIZE);

            /*
             * Create the MMIO2 range record for it.
             */
            const size_t cbRange = RT_OFFSETOF(PGMMMIO2RANGE, RamRange.aPages[cPages]);
            PPGMMMIO2RANGE pNew;
            rc = MMR3HyperAllocOnceNoRel(pVM, cbRange, 0, MM_TAG_PGM_PHYS, (void **)&pNew);
            AssertLogRelMsgRC(rc, ("cbRamRange=%zu\n", cbRange));
            if (RT_SUCCESS(rc))
            {
                pNew->pDevInsR3             = pDevIns;
                pNew->pvR3                  = pvPages;
                //pNew->pNext               = NULL;
                //pNew->fMapped             = false;
                //pNew->fOverlapping        = false;
                pNew->iRegion               = iRegion;
                pNew->idSavedState          = UINT8_MAX;
                pNew->RamRange.pSelfR0      = MMHyperCCToR0(pVM, &pNew->RamRange);
                pNew->RamRange.pSelfRC      = MMHyperCCToRC(pVM, &pNew->RamRange);
                pNew->RamRange.GCPhys       = NIL_RTGCPHYS;
                pNew->RamRange.GCPhysLast   = NIL_RTGCPHYS;
                pNew->RamRange.pszDesc      = pszDesc;
                pNew->RamRange.cb           = cb;
                pNew->RamRange.fFlags       = PGM_RAM_RANGE_FLAGS_AD_HOC_MMIO2;
                pNew->RamRange.pvR3         = pvPages;
                //pNew->RamRange.paLSPages    = NULL;

                uint32_t iPage = cPages;
                while (iPage-- > 0)
                {
                    PGM_PAGE_INIT(&pNew->RamRange.aPages[iPage],
                                  paPages[iPage].Phys, NIL_GMM_PAGEID,
                                  PGMPAGETYPE_MMIO2, PGM_PAGE_STATE_ALLOCATED);
                }

                /* update page count stats */
                pVM->pgm.s.cAllPages     += cPages;
                pVM->pgm.s.cPrivatePages += cPages;

                /*
                 * Link it into the list.
                 * Since there is no particular order, just push it.
                 */
                pgmLock(pVM);
                pNew->pNextR3 = pVM->pgm.s.pMmio2RangesR3;
                pVM->pgm.s.pMmio2RangesR3 = pNew;
                pgmUnlock(pVM);

                *ppv = pvPages;
                RTMemTmpFree(paPages);
                pgmPhysInvalidatePageMapTLB(pVM);
                return VINF_SUCCESS;
            }

            SUPR3PageFreeEx(pvPages, cPages);
        }
        RTMemTmpFree(paPages);
        MMR3AdjustFixedReservation(pVM, -(int32_t)cPages, pszDesc);
    }
    if (pDevIns->iInstance > 0)
        MMR3HeapFree((void *)pszDesc);
    return rc;
}


/**
 * Deregisters and frees an MMIO2 region.
 *
 * Any physical (and virtual) access handlers registered for the region must
 * be deregistered before calling this function.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region. If it's UINT32_MAX it'll be a wildcard match.
 */
VMMR3DECL(int) PGMR3PhysMMIO2Deregister(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX || iRegion == UINT32_MAX, VERR_INVALID_PARAMETER);

    pgmLock(pVM);
    int rc = VINF_SUCCESS;
    unsigned cFound = 0;
    PPGMMMIO2RANGE pPrev = NULL;
    PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3;
    while (pCur)
    {
        if (    pCur->pDevInsR3 == pDevIns
            &&  (   iRegion == UINT32_MAX
                 || pCur->iRegion == iRegion))
        {
            cFound++;

            /*
             * Unmap it if it's mapped.
             */
            if (pCur->fMapped)
            {
                int rc2 = PGMR3PhysMMIO2Unmap(pVM, pCur->pDevInsR3, pCur->iRegion, pCur->RamRange.GCPhys);
                AssertRC(rc2);
                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                    rc = rc2;
            }

            /*
             * Unlink it
             */
            PPGMMMIO2RANGE pNext = pCur->pNextR3;
            if (pPrev)
                pPrev->pNextR3 = pNext;
            else
                pVM->pgm.s.pMmio2RangesR3 = pNext;
            pCur->pNextR3 = NULL;

            /*
             * Free the memory.
             */
            int rc2 = SUPR3PageFreeEx(pCur->pvR3, pCur->RamRange.cb >> PAGE_SHIFT);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;

            uint32_t const cPages = pCur->RamRange.cb >> PAGE_SHIFT;
            rc2 = MMR3AdjustFixedReservation(pVM, -(int32_t)cPages, pCur->RamRange.pszDesc);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;

            /* we're leaking hyper memory here if done at runtime. */
#ifdef VBOX_STRICT
            VMSTATE const enmState = VMR3GetState(pVM);
            AssertMsg(   enmState == VMSTATE_POWERING_OFF
                      || enmState == VMSTATE_POWERING_OFF_LS
                      || enmState == VMSTATE_OFF
                      || enmState == VMSTATE_OFF_LS
                      || enmState == VMSTATE_DESTROYING
                      || enmState == VMSTATE_TERMINATED
                      || enmState == VMSTATE_CREATING
                      , ("%s\n", VMR3GetStateName(enmState)));
#endif
            /*rc = MMHyperFree(pVM, pCur);
            AssertRCReturn(rc, rc); - not safe, see the alloc call. */


            /* update page count stats */
            pVM->pgm.s.cAllPages     -= cPages;
            pVM->pgm.s.cPrivatePages -= cPages;

            /* next */
            pCur = pNext;
        }
        else
        {
            pPrev = pCur;
            pCur = pCur->pNextR3;
        }
    }
    pgmPhysInvalidatePageMapTLB(pVM);
    pgmUnlock(pVM);
    return !cFound && iRegion != UINT32_MAX ? VERR_NOT_FOUND : rc;
}


/**
 * Maps a MMIO2 region.
 *
 * This is done when a guest / the bios / state loading changes the
 * PCI config. The replacing of base memory has the same restrictions
 * as during registration, of course.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pDevIns         The device instance owning the region.
 */
VMMR3DECL(int) PGMR3PhysMMIO2Map(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != NIL_RTGCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != 0, VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(!pCur->fMapped, VERR_WRONG_ORDER);
    Assert(pCur->RamRange.GCPhys == NIL_RTGCPHYS);
    Assert(pCur->RamRange.GCPhysLast == NIL_RTGCPHYS);

    const RTGCPHYS GCPhysLast = GCPhys + pCur->RamRange.cb - 1;
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);

    /*
     * Find our location in the ram range list, checking for
     * restriction we don't bother implementing yet (partially overlapping).
     */
    bool fRamExists = false;
    PPGMRAMRANGE pRamPrev = NULL;
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhys     <= pRam->GCPhysLast
            &&  GCPhysLast >= pRam->GCPhys)
        {
            /* completely within? */
            AssertLogRelMsgReturn(   GCPhys     >= pRam->GCPhys
                                  && GCPhysLast <= pRam->GCPhysLast,
                                  ("%RGp-%RGp (MMIO2/%s) falls partly outside %RGp-%RGp (%s)\n",
                                   GCPhys, GCPhysLast, pCur->RamRange.pszDesc,
                                   pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    if (fRamExists)
    {
        PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM,
                                  ("%RGp isn't a RAM page (%d) - mapping %RGp-%RGp (MMIO2/%s).\n",
                                   GCPhys, PGM_PAGE_GET_TYPE(pPage), GCPhys, GCPhysLast, pCur->RamRange.pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            pPage++;
        }
    }
    Log(("PGMR3PhysMMIO2Map: %RGp-%RGp fRamExists=%RTbool %s\n",
         GCPhys, GCPhysLast, fRamExists, pCur->RamRange.pszDesc));

    /*
     * Make the changes.
     */
    pgmLock(pVM);

    pCur->RamRange.GCPhys = GCPhys;
    pCur->RamRange.GCPhysLast = GCPhysLast;
    pCur->fMapped = true;
    pCur->fOverlapping = fRamExists;

    if (fRamExists)
    {
/** @todo use pgmR3PhysFreePageRange here. */
        uint32_t            cPendingPages = 0;
        PGMMFREEPAGESREQ    pReq;
        int rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
        AssertLogRelRCReturn(rc, rc);

        /* replace the pages, freeing all present RAM pages. */
        PPGMPAGE pPageSrc = &pCur->RamRange.aPages[0];
        PPGMPAGE pPageDst = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPageDst, GCPhys);
            AssertLogRelRCReturn(rc, rc); /* We're done for if this goes wrong. */

            RTHCPHYS const HCPhys = PGM_PAGE_GET_HCPHYS(pPageSrc);
            PGM_PAGE_SET_HCPHYS(pVM, pPageDst, HCPhys);
            PGM_PAGE_SET_TYPE(pVM, pPageDst, PGMPAGETYPE_MMIO2);
            PGM_PAGE_SET_STATE(pVM, pPageDst, PGM_PAGE_STATE_ALLOCATED);
            PGM_PAGE_SET_PDE_TYPE(pVM, pPageDst, PGM_PAGE_PDE_TYPE_DONTCARE);
            PGM_PAGE_SET_PTE_INDEX(pVM, pPageDst, 0);
            PGM_PAGE_SET_TRACKING(pVM, pPageDst, 0);

            pVM->pgm.s.cZeroPages--;
            GCPhys += PAGE_SIZE;
            pPageSrc++;
            pPageDst++;
        }

        /* Flush physical page map TLB. */
        pgmPhysInvalidatePageMapTLB(pVM);

        if (cPendingPages)
        {
            rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
            AssertLogRelRCReturn(rc, rc);
        }
        GMMR3FreePagesCleanup(pReq);

        /* Force a PGM pool flush as guest ram references have been changed. */
        /** todo; not entirely SMP safe; assuming for now the guest takes care of this internally (not touch mapped mmio while changing the mapping). */
        PVMCPU pVCpu = VMMGetCpu(pVM);
        pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

        pgmUnlock(pVM);
    }
    else
    {
        RTGCPHYS cb = pCur->RamRange.cb;

        /* Clear the tracking data of pages we're going to reactivate. */
        PPGMPAGE pPageSrc = &pCur->RamRange.aPages[0];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            PGM_PAGE_SET_TRACKING(pVM, pPageSrc, 0);
            PGM_PAGE_SET_PTE_INDEX(pVM, pPageSrc, 0);
            pPageSrc++;
        }

        /* link in the ram range */
        pgmR3PhysLinkRamRange(pVM, &pCur->RamRange, pRamPrev);
        pgmUnlock(pVM);

#ifdef VBOX_WITH_REM
        REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, REM_NOTIFY_PHYS_RAM_FLAGS_MMIO2);
#endif
    }

    pgmPhysInvalidatePageMapTLB(pVM);
    return VINF_SUCCESS;
}


/**
 * Unmaps a MMIO2 region.
 *
 * This is done when a guest / the bios / state loading changes the
 * PCI config. The replacing of base memory has the same restrictions
 * as during registration, of course.
 */
VMMR3DECL(int) PGMR3PhysMMIO2Unmap(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != NIL_RTGCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != 0, VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(pCur->fMapped, VERR_WRONG_ORDER);
    AssertReturn(pCur->RamRange.GCPhys == GCPhys, VERR_INVALID_PARAMETER);
    Assert(pCur->RamRange.GCPhysLast != NIL_RTGCPHYS);

    Log(("PGMR3PhysMMIO2Unmap: %RGp-%RGp %s\n",
         pCur->RamRange.GCPhys, pCur->RamRange.GCPhysLast, pCur->RamRange.pszDesc));

    /*
     * Unmap it.
     */
    pgmLock(pVM);

#ifdef VBOX_WITH_REM
    RTGCPHYS    GCPhysRangeREM;
    RTGCPHYS    cbRangeREM;
    bool        fInformREM;
#endif
    if (pCur->fOverlapping)
    {
        /* Restore the RAM pages we've replaced. */
        PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3;
        while (pRam->GCPhys > pCur->RamRange.GCPhysLast)
            pRam = pRam->pNextR3;

        PPGMPAGE pPageDst = &pRam->aPages[(pCur->RamRange.GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            PGM_PAGE_INIT_ZERO(pPageDst, pVM, PGMPAGETYPE_RAM);
            pVM->pgm.s.cZeroPages++;
            pPageDst++;
        }

        /* Flush physical page map TLB. */
        pgmPhysInvalidatePageMapTLB(pVM);
#ifdef VBOX_WITH_REM
        GCPhysRangeREM = NIL_RTGCPHYS;  /* shuts up gcc */
        cbRangeREM     = RTGCPHYS_MAX;  /* ditto */
        fInformREM     = false;
#endif
    }
    else
    {
#ifdef VBOX_WITH_REM
        GCPhysRangeREM = pCur->RamRange.GCPhys;
        cbRangeREM     = pCur->RamRange.cb;
        fInformREM     = true;
#endif
        pgmR3PhysUnlinkRamRange(pVM, &pCur->RamRange);
    }

    pCur->RamRange.GCPhys = NIL_RTGCPHYS;
    pCur->RamRange.GCPhysLast = NIL_RTGCPHYS;
    pCur->fOverlapping = false;
    pCur->fMapped = false;

    /* Force a PGM pool flush as guest ram references have been changed. */
    /** @todo not entirely SMP safe; assuming for now the guest takes care
     *  of this internally (not touch mapped mmio while changing the
     *  mapping). */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

    pgmPhysInvalidatePageMapTLB(pVM);
    pgmPhysInvalidRamRangeTlbs(pVM);
    pgmUnlock(pVM);

#ifdef VBOX_WITH_REM
    if (fInformREM)
        REMR3NotifyPhysRamDeregister(pVM, GCPhysRangeREM, cbRangeREM);
#endif

    return VINF_SUCCESS;
}


/**
 * Checks if the given address is an MMIO2 base address or not.
 *
 * @returns true/false accordingly.
 * @param   pVM             The VM handle.
 * @param   pDevIns         The owner of the memory, optional.
 * @param   GCPhys          The address to check.
 */
VMMR3DECL(bool) PGMR3PhysMMIO2IsBase(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, false);
    AssertPtrReturn(pDevIns, false);
    AssertReturn(GCPhys != NIL_RTGCPHYS, false);
    AssertReturn(GCPhys != 0, false);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), false);

    /*
     * Search the list.
     */
    pgmLock(pVM);
    for (PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3; pCur; pCur = pCur->pNextR3)
        if (pCur->RamRange.GCPhys == GCPhys)
        {
            Assert(pCur->fMapped);
            pgmUnlock(pVM);
            return true;
        }
    pgmUnlock(pVM);
    return false;
}


/**
 * Gets the HC physical address of a page in the MMIO2 region.
 *
 * This is API is intended for MMHyper and shouldn't be called
 * by anyone else...
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         The owner of the memory, optional.
 * @param   iRegion         The region.
 * @param   off             The page expressed an offset into the MMIO2 region.
 * @param   pHCPhys         Where to store the result.
 */
VMMR3DECL(int) PGMR3PhysMMIO2GetHCPhys(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, PRTHCPHYS pHCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);

    pgmLock(pVM);
    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(off < pCur->RamRange.cb, VERR_INVALID_PARAMETER);

    PCPGMPAGE pPage = &pCur->RamRange.aPages[off >> PAGE_SHIFT];
    *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage);
    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Maps a portion of an MMIO2 region into kernel space (host).
 *
 * The kernel mapping will become invalid when the MMIO2 memory is deregistered
 * or the VM is terminated.
 *
 * @return VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   pDevIns     The device owning the MMIO2 memory.
 * @param   iRegion     The region.
 * @param   off         The offset into the region. Must be page aligned.
 * @param   cb          The number of bytes to map. Must be page aligned.
 * @param   pszDesc     Mapping description.
 * @param   pR0Ptr      Where to store the R0 address.
 */
VMMR3DECL(int) PGMR3PhysMMIO2MapKernel(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                       const char *pszDesc, PRTR0PTR pR0Ptr)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(off < pCur->RamRange.cb, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= pCur->RamRange.cb, VERR_INVALID_PARAMETER);
    AssertReturn(off + cb <= pCur->RamRange.cb, VERR_INVALID_PARAMETER);
    NOREF(pszDesc);

    /*
     * Pass the request on to the support library/driver.
     */
    int rc = SUPR3PageMapKernel(pCur->pvR3, off, cb, 0, pR0Ptr);

    return rc;
}


/**
 * Worker for PGMR3PhysRomRegister.
 *
 * This is here to simplify lock management, i.e. the caller does all the
 * locking and we can simply return without needing to remember to unlock
 * anything first.
 *
 * @returns VBox status.
 * @param   pVM                 The VM handle.
 * @param   pDevIns             The device instance owning the ROM.
 * @param   GCPhys              First physical address in the range.
 *                              Must be page aligned!
 * @param   cb                  The size of the range (in bytes).
 *                              Must be page aligned!
 * @param   pvBinary            Pointer to the binary data backing the ROM image.
 * @param   cbBinary            The size of the binary data pvBinary points to.
 *                              This must be less or equal to @a cb.
 * @param   fFlags              Mask of flags. PGMPHYS_ROM_FLAGS_SHADOWED
 *                              and/or PGMPHYS_ROM_FLAGS_PERMANENT_BINARY.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
static int pgmR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS cb,
                                const void *pvBinary, uint32_t cbBinary, uint32_t fFlags, const char *pszDesc)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBinary, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~(PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY)), VERR_INVALID_PARAMETER);
    VM_ASSERT_STATE_RETURN(pVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    const uint32_t cPages = cb >> PAGE_SHIFT;

    /*
     * Find the ROM location in the ROM list first.
     */
    PPGMROMRANGE    pRomPrev = NULL;
    PPGMROMRANGE    pRom = pVM->pgm.s.pRomRangesR3;
    while (pRom && GCPhysLast >= pRom->GCPhys)
    {
        if (    GCPhys     <= pRom->GCPhysLast
            &&  GCPhysLast >= pRom->GCPhys)
            AssertLogRelMsgFailedReturn(("%RGp-%RGp (%s) conflicts with existing %RGp-%RGp (%s)\n",
                                         GCPhys, GCPhysLast, pszDesc,
                                         pRom->GCPhys, pRom->GCPhysLast, pRom->pszDesc),
                                        VERR_PGM_RAM_CONFLICT);
        /* next */
        pRomPrev = pRom;
        pRom = pRom->pNextR3;
    }

    /*
     * Find the RAM location and check for conflicts.
     *
     * Conflict detection is a bit different than for RAM
     * registration since a ROM can be located within a RAM
     * range. So, what we have to check for is other memory
     * types (other than RAM that is) and that we don't span
     * more than one RAM range (layz).
     */
    bool            fRamExists = false;
    PPGMRAMRANGE    pRamPrev = NULL;
    PPGMRAMRANGE    pRam = pVM->pgm.s.pRamRangesXR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhys     <= pRam->GCPhysLast
            &&  GCPhysLast >= pRam->GCPhys)
        {
            /* completely within? */
            AssertLogRelMsgReturn(   GCPhys     >= pRam->GCPhys
                                  && GCPhysLast <= pRam->GCPhysLast,
                                  ("%RGp-%RGp (%s) falls partly outside %RGp-%RGp (%s)\n",
                                   GCPhys, GCPhysLast, pszDesc,
                                   pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    if (fRamExists)
    {
        PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = cPages;
        while (cPagesLeft-- > 0)
        {
            AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM,
                                  ("%RGp (%R[pgmpage]) isn't a RAM page - registering %RGp-%RGp (%s).\n",
                                   pRam->GCPhys + ((RTGCPHYS)(uintptr_t)(pPage - &pRam->aPages[0]) << PAGE_SHIFT),
                                   pPage, GCPhys, GCPhysLast, pszDesc), VERR_PGM_RAM_CONFLICT);
            Assert(PGM_PAGE_IS_ZERO(pPage));
            pPage++;
        }
    }

    /*
     * Update the base memory reservation if necessary.
     */
    uint32_t cExtraBaseCost = fRamExists ? 0 : cPages;
    if (fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
        cExtraBaseCost += cPages;
    if (cExtraBaseCost)
    {
        int rc = MMR3IncreaseBaseReservation(pVM, cExtraBaseCost);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Allocate memory for the virgin copy of the RAM.
     */
    PGMMALLOCATEPAGESREQ pReq;
    int rc = GMMR3AllocatePagesPrepare(pVM, &pReq, cPages, GMMACCOUNT_BASE);
    AssertRCReturn(rc, rc);

    for (uint32_t iPage = 0; iPage < cPages; iPage++)
    {
        pReq->aPages[iPage].HCPhysGCPhys = GCPhys + (iPage << PAGE_SHIFT);
        pReq->aPages[iPage].idPage = NIL_GMM_PAGEID;
        pReq->aPages[iPage].idSharedPage = NIL_GMM_PAGEID;
    }

    rc = GMMR3AllocatePagesPerform(pVM, pReq);
    if (RT_FAILURE(rc))
    {
        GMMR3AllocatePagesCleanup(pReq);
        return rc;
    }

    /*
     * Allocate the new ROM range and RAM range (if necessary).
     */
    PPGMROMRANGE pRomNew;
    rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMROMRANGE, aPages[cPages]), 0, MM_TAG_PGM_PHYS, (void **)&pRomNew);
    if (RT_SUCCESS(rc))
    {
        PPGMRAMRANGE pRamNew = NULL;
        if (!fRamExists)
            rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]), sizeof(PGMPAGE), MM_TAG_PGM_PHYS, (void **)&pRamNew);
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize and insert the RAM range (if required).
             */
            PPGMROMPAGE pRomPage = &pRomNew->aPages[0];
            if (!fRamExists)
            {
                pRamNew->pSelfR0       = MMHyperCCToR0(pVM, pRamNew);
                pRamNew->pSelfRC       = MMHyperCCToRC(pVM, pRamNew);
                pRamNew->GCPhys        = GCPhys;
                pRamNew->GCPhysLast    = GCPhysLast;
                pRamNew->cb            = cb;
                pRamNew->pszDesc       = pszDesc;
                pRamNew->fFlags        = PGM_RAM_RANGE_FLAGS_AD_HOC_ROM;
                pRamNew->pvR3          = NULL;
                pRamNew->paLSPages     = NULL;

                PPGMPAGE pPage = &pRamNew->aPages[0];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pPage++, pRomPage++)
                {
                    PGM_PAGE_INIT(pPage,
                                  pReq->aPages[iPage].HCPhysGCPhys,
                                  pReq->aPages[iPage].idPage,
                                  PGMPAGETYPE_ROM,
                                  PGM_PAGE_STATE_ALLOCATED);

                    pRomPage->Virgin = *pPage;
                }

                pVM->pgm.s.cAllPages += cPages;
                pgmR3PhysLinkRamRange(pVM, pRamNew, pRamPrev);
            }
            else
            {
                PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pPage++, pRomPage++)
                {
                    PGM_PAGE_SET_TYPE(pVM, pPage,   PGMPAGETYPE_ROM);
                    PGM_PAGE_SET_HCPHYS(pVM, pPage, pReq->aPages[iPage].HCPhysGCPhys);
                    PGM_PAGE_SET_STATE(pVM, pPage,  PGM_PAGE_STATE_ALLOCATED);
                    PGM_PAGE_SET_PAGEID(pVM, pPage, pReq->aPages[iPage].idPage);
                    PGM_PAGE_SET_PDE_TYPE(pVM, pPage, PGM_PAGE_PDE_TYPE_DONTCARE);
                    PGM_PAGE_SET_PTE_INDEX(pVM, pPage, 0);
                    PGM_PAGE_SET_TRACKING(pVM, pPage, 0);

                    pRomPage->Virgin = *pPage;
                }

                pRamNew = pRam;

                pVM->pgm.s.cZeroPages -= cPages;
            }
            pVM->pgm.s.cPrivatePages += cPages;

            /* Flush physical page map TLB. */
            pgmPhysInvalidatePageMapTLB(pVM);


            /*
             * !HACK ALERT!  REM + (Shadowed) ROM ==> mess.
             *
             * If it's shadowed we'll register the handler after the ROM notification
             * so we get the access handler callbacks that we should. If it isn't
             * shadowed we'll do it the other way around to make REM use the built-in
             * ROM behavior and not the handler behavior (which is to route all access
             * to PGM atm).
             */
            if (fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
            {
#ifdef VBOX_WITH_REM
                REMR3NotifyPhysRomRegister(pVM, GCPhys, cb, NULL, true /* fShadowed */);
#endif
                rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhysLast,
                                                  pgmR3PhysRomWriteHandler, pRomNew,
                                                  NULL, "pgmPhysRomWriteHandler", MMHyperCCToR0(pVM, pRomNew),
                                                  NULL, "pgmPhysRomWriteHandler", MMHyperCCToRC(pVM, pRomNew), pszDesc);
            }
            else
            {
                rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhysLast,
                                                  pgmR3PhysRomWriteHandler, pRomNew,
                                                  NULL, "pgmPhysRomWriteHandler", MMHyperCCToR0(pVM, pRomNew),
                                                  NULL, "pgmPhysRomWriteHandler", MMHyperCCToRC(pVM, pRomNew), pszDesc);
#ifdef VBOX_WITH_REM
                REMR3NotifyPhysRomRegister(pVM, GCPhys, cb, NULL, false /* fShadowed */);
#endif
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Copy the image over to the virgin pages.
                 * This must be done after linking in the RAM range.
                 */
                size_t          cbBinaryLeft = cbBinary;
                PPGMPAGE        pRamPage     = &pRamNew->aPages[(GCPhys - pRamNew->GCPhys) >> PAGE_SHIFT];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pRamPage++)
                {
                    void *pvDstPage;
                    rc = pgmPhysPageMap(pVM, pRamPage, GCPhys + (iPage << PAGE_SHIFT), &pvDstPage);
                    if (RT_FAILURE(rc))
                    {
                        VMSetError(pVM, rc, RT_SRC_POS, "Failed to map virgin ROM page at %RGp", GCPhys);
                        break;
                    }
                    if (cbBinaryLeft >= PAGE_SIZE)
                    {
                        memcpy(pvDstPage, (uint8_t const *)pvBinary + ((size_t)iPage << PAGE_SHIFT), PAGE_SIZE);
                        cbBinaryLeft -= PAGE_SIZE;
                    }
                    else
                    {
                        ASMMemZeroPage(pvDstPage); /* (shouldn't be necessary, but can't hurt either) */
                        if (cbBinaryLeft > 0)
                        {
                            memcpy(pvDstPage, (uint8_t const *)pvBinary + ((size_t)iPage << PAGE_SHIFT), cbBinaryLeft);
                            cbBinaryLeft = 0;
                        }
                    }
                }
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Initialize the ROM range.
                     * Note that the Virgin member of the pages has already been initialized above.
                     */
                    pRomNew->GCPhys     = GCPhys;
                    pRomNew->GCPhysLast = GCPhysLast;
                    pRomNew->cb         = cb;
                    pRomNew->fFlags     = fFlags;
                    pRomNew->idSavedState = UINT8_MAX;
                    pRomNew->cbOriginal = cbBinary;
#ifdef VBOX_STRICT
                    pRomNew->pvOriginal = fFlags & PGMPHYS_ROM_FLAGS_PERMANENT_BINARY
                                        ? pvBinary : RTMemDup(pvBinary, cbBinary);
#else
                    pRomNew->pvOriginal = fFlags & PGMPHYS_ROM_FLAGS_PERMANENT_BINARY ? pvBinary : NULL;
#endif
                    pRomNew->pszDesc    = pszDesc;

                    for (unsigned iPage = 0; iPage < cPages; iPage++)
                    {
                        PPGMROMPAGE pPage = &pRomNew->aPages[iPage];
                        pPage->enmProt = PGMROMPROT_READ_ROM_WRITE_IGNORE;
                        PGM_PAGE_INIT_ZERO(&pPage->Shadow, pVM, PGMPAGETYPE_ROM_SHADOW);
                    }

                    /* update the page count stats for the shadow pages. */
                    if (fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
                    {
                        pVM->pgm.s.cZeroPages += cPages;
                        pVM->pgm.s.cAllPages  += cPages;
                    }

                    /*
                     * Insert the ROM range, tell REM and return successfully.
                     */
                    pRomNew->pNextR3 = pRom;
                    pRomNew->pNextR0 = pRom ? MMHyperCCToR0(pVM, pRom) : NIL_RTR0PTR;
                    pRomNew->pNextRC = pRom ? MMHyperCCToRC(pVM, pRom) : NIL_RTRCPTR;

                    if (pRomPrev)
                    {
                        pRomPrev->pNextR3 = pRomNew;
                        pRomPrev->pNextR0 = MMHyperCCToR0(pVM, pRomNew);
                        pRomPrev->pNextRC = MMHyperCCToRC(pVM, pRomNew);
                    }
                    else
                    {
                        pVM->pgm.s.pRomRangesR3 = pRomNew;
                        pVM->pgm.s.pRomRangesR0 = MMHyperCCToR0(pVM, pRomNew);
                        pVM->pgm.s.pRomRangesRC = MMHyperCCToRC(pVM, pRomNew);
                    }

                    pgmPhysInvalidatePageMapTLB(pVM);
                    GMMR3AllocatePagesCleanup(pReq);
                    return VINF_SUCCESS;
                }

                /* bail out */

                int rc2 = PGMHandlerPhysicalDeregister(pVM, GCPhys);
                AssertRC(rc2);
            }

            if (!fRamExists)
            {
                pgmR3PhysUnlinkRamRange2(pVM, pRamNew, pRamPrev);
                MMHyperFree(pVM, pRamNew);
            }
        }
        MMHyperFree(pVM, pRomNew);
    }

    /** @todo Purge the mapping cache or something... */
    GMMR3FreeAllocatedPages(pVM, pReq);
    GMMR3AllocatePagesCleanup(pReq);
    return rc;
}


/**
 * Registers a ROM image.
 *
 * Shadowed ROM images requires double the amount of backing memory, so,
 * don't use that unless you have to. Shadowing of ROM images is process
 * where we can select where the reads go and where the writes go. On real
 * hardware the chipset provides means to configure this. We provide
 * PGMR3PhysProtectROM() for this purpose.
 *
 * A read-only copy of the ROM image will always be kept around while we
 * will allocate RAM pages for the changes on demand (unless all memory
 * is configured to be preallocated).
 *
 * @returns VBox status.
 * @param   pVM                 The VM handle.
 * @param   pDevIns             The device instance owning the ROM.
 * @param   GCPhys              First physical address in the range.
 *                              Must be page aligned!
 * @param   cb                  The size of the range (in bytes).
 *                              Must be page aligned!
 * @param   pvBinary            Pointer to the binary data backing the ROM image.
 * @param   cbBinary            The size of the binary data pvBinary points to.
 *                              This must be less or equal to @a cb.
 * @param   fFlags              Mask of flags. PGMPHYS_ROM_FLAGS_SHADOWED
 *                              and/or PGMPHYS_ROM_FLAGS_PERMANENT_BINARY.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 *
 * @remark  There is no way to remove the rom, automatically on device cleanup or
 *          manually from the device yet. This isn't difficult in any way, it's
 *          just not something we expect to be necessary for a while.
 */
VMMR3DECL(int) PGMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS cb,
                                    const void *pvBinary, uint32_t cbBinary, uint32_t fFlags, const char *pszDesc)
{
    Log(("PGMR3PhysRomRegister: pDevIns=%p GCPhys=%RGp(-%RGp) cb=%RGp pvBinary=%p cbBinary=%#x fFlags=%#x pszDesc=%s\n",
         pDevIns, GCPhys, GCPhys + cb, cb, pvBinary, cbBinary, fFlags, pszDesc));
    pgmLock(pVM);
    int rc = pgmR3PhysRomRegister(pVM, pDevIns, GCPhys, cb, pvBinary, cbBinary, fFlags, pszDesc);
    pgmUnlock(pVM);
    return rc;
}


/**
 * \#PF Handler callback for ROM write accesses.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             The VM handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pgmR3PhysRomWriteHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf,
                                                  PGMACCESSTYPE enmAccessType, void *pvUser)
{
    PPGMROMRANGE    pRom     = (PPGMROMRANGE)pvUser;
    const uint32_t  iPage    = (GCPhys - pRom->GCPhys) >> PAGE_SHIFT;
    Assert(iPage < (pRom->cb >> PAGE_SHIFT));
    PPGMROMPAGE     pRomPage = &pRom->aPages[iPage];
    Log5(("pgmR3PhysRomWriteHandler: %d %c %#08RGp %#04zx\n", pRomPage->enmProt, enmAccessType == PGMACCESSTYPE_READ ? 'R' : 'W', GCPhys, cbBuf));
    NOREF(pvPhys);

    if (enmAccessType == PGMACCESSTYPE_READ)
    {
        switch (pRomPage->enmProt)
        {
            /*
             * Take the default action.
             */
            case PGMROMPROT_READ_ROM_WRITE_IGNORE:
            case PGMROMPROT_READ_RAM_WRITE_IGNORE:
            case PGMROMPROT_READ_ROM_WRITE_RAM:
            case PGMROMPROT_READ_RAM_WRITE_RAM:
                return VINF_PGM_HANDLER_DO_DEFAULT;

            default:
                AssertMsgFailedReturn(("enmProt=%d iPage=%d GCPhys=%RGp\n",
                                       pRom->aPages[iPage].enmProt, iPage, GCPhys),
                                      VERR_IPE_NOT_REACHED_DEFAULT_CASE);
        }
    }
    else
    {
        Assert(enmAccessType == PGMACCESSTYPE_WRITE);
        switch (pRomPage->enmProt)
        {
            /*
             * Ignore writes.
             */
            case PGMROMPROT_READ_ROM_WRITE_IGNORE:
            case PGMROMPROT_READ_RAM_WRITE_IGNORE:
                return VINF_SUCCESS;

            /*
             * Write to the RAM page.
             */
            case PGMROMPROT_READ_ROM_WRITE_RAM:
            case PGMROMPROT_READ_RAM_WRITE_RAM: /* yes this will get here too, it's *way* simpler that way. */
            {
                /* This should be impossible now, pvPhys doesn't work cross page anylonger. */
                Assert(((GCPhys - pRom->GCPhys + cbBuf - 1) >> PAGE_SHIFT) == iPage);

                /*
                 * Take the lock, do lazy allocation, map the page and copy the data.
                 *
                 * Note that we have to bypass the mapping TLB since it works on
                 * guest physical addresses and entering the shadow page would
                 * kind of screw things up...
                 */
                int rc = pgmLock(pVM);
                AssertRC(rc);

                PPGMPAGE pShadowPage = &pRomPage->Shadow;
                if (!PGMROMPROT_IS_ROM(pRomPage->enmProt))
                {
                    pShadowPage = pgmPhysGetPage(pVM, GCPhys);
                    AssertLogRelReturn(pShadowPage, VERR_PGM_PHYS_PAGE_GET_IPE);
                }

                void *pvDstPage;
                rc = pgmPhysPageMakeWritableAndMap(pVM, pShadowPage, GCPhys & X86_PTE_PG_MASK, &pvDstPage);
                if (RT_SUCCESS(rc))
                {
                    memcpy((uint8_t *)pvDstPage + (GCPhys & PAGE_OFFSET_MASK), pvBuf, cbBuf);
                    pRomPage->LiveSave.fWrittenTo = true;
                }

                pgmUnlock(pVM);
                return rc;
            }

            default:
                AssertMsgFailedReturn(("enmProt=%d iPage=%d GCPhys=%RGp\n",
                                       pRom->aPages[iPage].enmProt, iPage, GCPhys),
                                      VERR_IPE_NOT_REACHED_DEFAULT_CASE);
        }
    }
}


/**
 * Called by PGMR3Reset to reset the shadow, switch to the virgin,
 * and verify that the virgin part is untouched.
 *
 * This is done after the normal memory has been cleared.
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @param   pVM         The VM handle.
 */
int pgmR3PhysRomReset(PVM pVM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        const uint32_t cPages = pRom->cb >> PAGE_SHIFT;

        if (pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
        {
            /*
             * Reset the physical handler.
             */
            int rc = PGMR3PhysRomProtect(pVM, pRom->GCPhys, pRom->cb, PGMROMPROT_READ_ROM_WRITE_IGNORE);
            AssertRCReturn(rc, rc);

            /*
             * What we do with the shadow pages depends on the memory
             * preallocation option. If not enabled, we'll just throw
             * out all the dirty pages and replace them by the zero page.
             */
            if (!pVM->pgm.s.fRamPreAlloc)
            {
                /* Free the dirty pages. */
                uint32_t            cPendingPages = 0;
                PGMMFREEPAGESREQ    pReq;
                rc = GMMR3FreePagesPrepare(pVM, &pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
                AssertRCReturn(rc, rc);

                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                    if (    !PGM_PAGE_IS_ZERO(&pRom->aPages[iPage].Shadow)
                        &&  !PGM_PAGE_IS_BALLOONED(&pRom->aPages[iPage].Shadow))
                    {
                        Assert(PGM_PAGE_GET_STATE(&pRom->aPages[iPage].Shadow) == PGM_PAGE_STATE_ALLOCATED);
                        rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, &pRom->aPages[iPage].Shadow,
                                             pRom->GCPhys + (iPage << PAGE_SHIFT));
                        AssertLogRelRCReturn(rc, rc);
                    }

                if (cPendingPages)
                {
                    rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
                    AssertLogRelRCReturn(rc, rc);
                }
                GMMR3FreePagesCleanup(pReq);
            }
            else
            {
                /* clear all the shadow pages. */
                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                {
                    if (PGM_PAGE_IS_ZERO(&pRom->aPages[iPage].Shadow))
                        continue;
                    Assert(!PGM_PAGE_IS_BALLOONED(&pRom->aPages[iPage].Shadow));
                    void *pvDstPage;
                    const RTGCPHYS GCPhys = pRom->GCPhys + (iPage << PAGE_SHIFT);
                    rc = pgmPhysPageMakeWritableAndMap(pVM, &pRom->aPages[iPage].Shadow, GCPhys, &pvDstPage);
                    if (RT_FAILURE(rc))
                        break;
                    ASMMemZeroPage(pvDstPage);
                }
                AssertRCReturn(rc, rc);
            }
        }

#ifdef VBOX_STRICT
        /*
         * Verify that the virgin page is unchanged if possible.
         */
        if (pRom->pvOriginal)
        {
            size_t         cbSrcLeft = pRom->cbOriginal;
            uint8_t const *pbSrcPage = (uint8_t const *)pRom->pvOriginal;
            for (uint32_t iPage = 0; iPage < cPages && cbSrcLeft > 0; iPage++, pbSrcPage += PAGE_SIZE)
            {
                const RTGCPHYS GCPhys = pRom->GCPhys + (iPage << PAGE_SHIFT);
                void const *pvDstPage;
                int rc = pgmPhysPageMapReadOnly(pVM, &pRom->aPages[iPage].Virgin, GCPhys, &pvDstPage);
                if (RT_FAILURE(rc))
                    break;

                if (memcmp(pvDstPage, pbSrcPage, RT_MIN(cbSrcLeft, PAGE_SIZE)))
                    LogRel(("pgmR3PhysRomReset: %RGp rom page changed (%s) - loaded saved state?\n",
                            GCPhys, pRom->pszDesc));
                cbSrcLeft -= RT_MIN(cbSrcLeft, PAGE_SIZE);
            }
        }
#endif
    }

    return VINF_SUCCESS;
}


/**
 * Called by PGMR3Term to free resources.
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @param   pVM         The VM handle.
 */
void pgmR3PhysRomTerm(PVM pVM)
{
#ifdef RT_STRICT
    /*
     * Free the heap copy of the original bits.
     */
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        if (   pRom->pvOriginal
            && !(pRom->fFlags & PGMPHYS_ROM_FLAGS_PERMANENT_BINARY))
        {
            RTMemFree((void *)pRom->pvOriginal);
            pRom->pvOriginal = NULL;
        }
    }
#endif
}


/**
 * Change the shadowing of a range of ROM pages.
 *
 * This is intended for implementing chipset specific memory registers
 * and will not be very strict about the input. It will silently ignore
 * any pages that are not the part of a shadowed ROM.
 *
 * @returns VBox status code.
 * @retval  VINF_PGM_SYNC_CR3
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      Where to start. Page aligned.
 * @param   cb          How much to change. Page aligned.
 * @param   enmProt     The new ROM protection.
 */
VMMR3DECL(int) PGMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, PGMROMPROT enmProt)
{
    /*
     * Check input
     */
    if (!cb)
        return VINF_SUCCESS;
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(enmProt >= PGMROMPROT_INVALID && enmProt <= PGMROMPROT_END, VERR_INVALID_PARAMETER);

    /*
     * Process the request.
     */
    pgmLock(pVM);
    int  rc = VINF_SUCCESS;
    bool fFlushTLB = false;
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        if (    GCPhys     <= pRom->GCPhysLast
            &&  GCPhysLast >= pRom->GCPhys
            &&  (pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED))
        {
            /*
             * Iterate the relevant pages and make necessary the changes.
             */
            bool fChanges = false;
            uint32_t const cPages = pRom->GCPhysLast <= GCPhysLast
                                  ? pRom->cb >> PAGE_SHIFT
                                  : (GCPhysLast - pRom->GCPhys + 1) >> PAGE_SHIFT;
            for (uint32_t iPage = (GCPhys - pRom->GCPhys) >> PAGE_SHIFT;
                 iPage < cPages;
                 iPage++)
            {
                PPGMROMPAGE pRomPage = &pRom->aPages[iPage];
                if (PGMROMPROT_IS_ROM(pRomPage->enmProt) != PGMROMPROT_IS_ROM(enmProt))
                {
                    fChanges = true;

                    /* flush references to the page. */
                    PPGMPAGE pRamPage = pgmPhysGetPage(pVM, pRom->GCPhys + (iPage << PAGE_SHIFT));
                    int rc2 = pgmPoolTrackUpdateGCPhys(pVM, pRom->GCPhys + (iPage << PAGE_SHIFT), pRamPage,
                                                       true /*fFlushPTEs*/, &fFlushTLB);
                    if (rc2 != VINF_SUCCESS && (rc == VINF_SUCCESS || RT_FAILURE(rc2)))
                        rc = rc2;

                    PPGMPAGE pOld = PGMROMPROT_IS_ROM(pRomPage->enmProt) ? &pRomPage->Virgin : &pRomPage->Shadow;
                    PPGMPAGE pNew = PGMROMPROT_IS_ROM(pRomPage->enmProt) ? &pRomPage->Shadow : &pRomPage->Virgin;

                    *pOld = *pRamPage;
                    *pRamPage = *pNew;
                    /** @todo preserve the volatile flags (handlers) when these have been moved out of HCPhys! */
                }
                pRomPage->enmProt = enmProt;
            }

            /*
             * Reset the access handler if we made changes, no need
             * to optimize this.
             */
            if (fChanges)
            {
                int rc2 = PGMHandlerPhysicalReset(pVM, pRom->GCPhys);
                if (RT_FAILURE(rc2))
                {
                    pgmUnlock(pVM);
                    AssertRC(rc);
                    return rc2;
                }
            }

            /* Advance - cb isn't updated. */
            GCPhys = pRom->GCPhys + (cPages << PAGE_SHIFT);
        }
    }
    pgmUnlock(pVM);
    if (fFlushTLB)
        PGM_INVL_ALL_VCPU_TLBS(pVM);

    return rc;
}


/**
 * Sets the Address Gate 20 state.
 *
 * @param   pVCpu       The VCPU to operate on.
 * @param   fEnable     True if the gate should be enabled.
 *                      False if the gate should be disabled.
 */
VMMDECL(void) PGMR3PhysSetA20(PVMCPU pVCpu, bool fEnable)
{
    LogFlow(("PGMR3PhysSetA20 %d (was %d)\n", fEnable, pVCpu->pgm.s.fA20Enabled));
    if (pVCpu->pgm.s.fA20Enabled != fEnable)
    {
        pVCpu->pgm.s.fA20Enabled = fEnable;
        pVCpu->pgm.s.GCPhysA20Mask = ~(RTGCPHYS)(!fEnable << 20);
#ifdef VBOX_WITH_REM
        REMR3A20Set(pVCpu->pVMR3, pVCpu, fEnable);
#endif
        /** @todo we're not handling this correctly for VT-x / AMD-V. See #2911 */
    }
}


/**
 * Tree enumeration callback for dealing with age rollover.
 * It will perform a simple compression of the current age.
 */
static DECLCALLBACK(int) pgmR3PhysChunkAgeingRolloverCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    /* Age compression - ASSUMES iNow == 4. */
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)pNode;
    if (pChunk->iLastUsed >= UINT32_C(0xffffff00))
        pChunk->iLastUsed = 3;
    else if (pChunk->iLastUsed >= UINT32_C(0xfffff000))
        pChunk->iLastUsed = 2;
    else if (pChunk->iLastUsed)
        pChunk->iLastUsed = 1;
    else /* iLastUsed = 0 */
        pChunk->iLastUsed = 4;

    NOREF(pvUser);
    return 0;
}


/**
 * The structure passed in the pvUser argument of pgmR3PhysChunkUnmapCandidateCallback().
 */
typedef struct PGMR3PHYSCHUNKUNMAPCB
{
    PVM                 pVM;            /**< The VM handle. */
    PPGMCHUNKR3MAP      pChunk;         /**< The chunk to unmap. */
} PGMR3PHYSCHUNKUNMAPCB, *PPGMR3PHYSCHUNKUNMAPCB;


/**
 * Callback used to find the mapping that's been unused for
 * the longest time.
 */
static DECLCALLBACK(int) pgmR3PhysChunkUnmapCandidateCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    PPGMCHUNKR3MAP          pChunk = (PPGMCHUNKR3MAP)pNode;
    PPGMR3PHYSCHUNKUNMAPCB  pArg   = (PPGMR3PHYSCHUNKUNMAPCB)pvUser;

    /*
     * Check for locks and compare when last used.
     */
    if (pChunk->cRefs)
        return 0;
    if (pChunk->cPermRefs)
        return 0;
    if (   pArg->pChunk
        && pChunk->iLastUsed >= pArg->pChunk->iLastUsed)
        return 0;

    /*
     * Check that it's not in any of the TLBs.
     */
    PVM pVM = pArg->pVM;
    if (   pVM->pgm.s.ChunkR3Map.Tlb.aEntries[PGM_CHUNKR3MAPTLB_IDX(pChunk->Core.Key)].idChunk
        == pChunk->Core.Key)
    {
        pChunk = NULL;
        return 0;
    }
#ifdef VBOX_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
    {
        Assert(pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk != pChunk);
        Assert(pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk != pChunk->Core.Key);
    }
#endif

    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.PhysTlbHC.aEntries); i++)
        if (pVM->pgm.s.PhysTlbHC.aEntries[i].pMap == pChunk)
            return 0;

    pArg->pChunk = pChunk;
    return 0;
}


/**
 * Finds a good candidate for unmapping when the ring-3 mapping cache is full.
 *
 * The candidate will not be part of any TLBs, so no need to flush
 * anything afterwards.
 *
 * @returns Chunk id.
 * @param   pVM         The VM handle.
 */
static int32_t pgmR3PhysChunkFindUnmapCandidate(PVM pVM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Enumerate the age tree starting with the left most node.
     */
    STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkFindCandidate, a);
    PGMR3PHYSCHUNKUNMAPCB Args;
    Args.pVM    = pVM;
    Args.pChunk = NULL;
    RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkUnmapCandidateCallback, &Args);
    Assert(Args.pChunk);
    if (Args.pChunk)
    {
        Assert(Args.pChunk->cRefs == 0);
        Assert(Args.pChunk->cPermRefs == 0);
        STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkFindCandidate, a);
        return Args.pChunk->Core.Key;
    }

    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkFindCandidate, a);
    return INT32_MAX;
}


/**
 * Rendezvous callback used by pgmR3PhysUnmapChunk that unmaps a chunk
 *
 * This is only called on one of the EMTs while the other ones are waiting for
 * it to complete this function.
 *
 * @returns VINF_SUCCESS (VBox strict status code).
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU for the EMT we're being called on. Unused.
 * @param   pvUser      User pointer. Unused
 *
 */
static DECLCALLBACK(VBOXSTRICTRC) pgmR3PhysUnmapChunkRendezvous(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    int rc = VINF_SUCCESS;
    pgmLock(pVM);
    NOREF(pVCpu); NOREF(pvUser);

    if (pVM->pgm.s.ChunkR3Map.c >= pVM->pgm.s.ChunkR3Map.cMax)
    {
        /* Flush the pgm pool cache; call the internal rendezvous handler as we're already in a rendezvous handler here. */
        /** @todo also not really efficient to unmap a chunk that contains PD
         *  or PT pages. */
        pgmR3PoolClearAllRendezvous(pVM, &pVM->aCpus[0], NULL /* no need to flush the REM TLB as we already did that above */);

        /*
         * Request the ring-0 part to unmap a chunk to make space in the mapping cache.
         */
        GMMMAPUNMAPCHUNKREQ Req;
        Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        Req.Hdr.cbReq    = sizeof(Req);
        Req.pvR3         = NULL;
        Req.idChunkMap   = NIL_GMM_CHUNKID;
        Req.idChunkUnmap = pgmR3PhysChunkFindUnmapCandidate(pVM);
        if (Req.idChunkUnmap != INT32_MAX)
        {
            STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkUnmap, a);
            rc = VMMR3CallR0(pVM, VMMR0_DO_GMM_MAP_UNMAP_CHUNK, 0, &Req.Hdr);
            STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkUnmap, a);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Remove the unmapped one.
                 */
                PPGMCHUNKR3MAP pUnmappedChunk = (PPGMCHUNKR3MAP)RTAvlU32Remove(&pVM->pgm.s.ChunkR3Map.pTree, Req.idChunkUnmap);
                AssertRelease(pUnmappedChunk);
                AssertRelease(!pUnmappedChunk->cRefs);
                AssertRelease(!pUnmappedChunk->cPermRefs);
                pUnmappedChunk->pv       = NULL;
                pUnmappedChunk->Core.Key = UINT32_MAX;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
                MMR3HeapFree(pUnmappedChunk);
#else
                MMR3UkHeapFree(pVM, pUnmappedChunk, MM_TAG_PGM_CHUNK_MAPPING);
#endif
                pVM->pgm.s.ChunkR3Map.c--;
                pVM->pgm.s.cUnmappedChunks++;

                /*
                 * Flush dangling PGM pointers (R3 & R0 ptrs to GC physical addresses).
                 */
                /** todo: we should not flush chunks which include cr3 mappings. */
                for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
                {
                    PPGMCPU pPGM = &pVM->aCpus[idCpu].pgm.s;

                    pPGM->pGst32BitPdR3    = NULL;
                    pPGM->pGstPaePdptR3    = NULL;
                    pPGM->pGstAmd64Pml4R3  = NULL;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
                    pPGM->pGst32BitPdR0    = NIL_RTR0PTR;
                    pPGM->pGstPaePdptR0    = NIL_RTR0PTR;
                    pPGM->pGstAmd64Pml4R0  = NIL_RTR0PTR;
#endif
                    for (unsigned i = 0; i < RT_ELEMENTS(pPGM->apGstPaePDsR3); i++)
                    {
                        pPGM->apGstPaePDsR3[i]             = NULL;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
                        pPGM->apGstPaePDsR0[i]             = NIL_RTR0PTR;
#endif
                    }

                    /* Flush REM TLBs. */
                    CPUMSetChangedFlags(&pVM->aCpus[idCpu], CPUM_CHANGED_GLOBAL_TLB_FLUSH);
                }
#ifdef VBOX_WITH_REM
                /* Flush REM translation blocks. */
                REMFlushTBs(pVM);
#endif
            }
        }
    }
    pgmUnlock(pVM);
    return rc;
}

/**
 * Unmap a chunk to free up virtual address space (request packet handler for pgmR3PhysChunkMap)
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
void pgmR3PhysUnmapChunk(PVM pVM)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3PhysUnmapChunkRendezvous, NULL);
    AssertRC(rc);
}


/**
 * Maps the given chunk into the ring-3 mapping cache.
 *
 * This will call ring-0.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk in question.
 * @param   ppChunk     Where to store the chunk tracking structure.
 *
 * @remarks Called from within the PGM critical section.
 * @remarks Can be called from any thread!
 */
int pgmR3PhysChunkMap(PVM pVM, uint32_t idChunk, PPPGMCHUNKR3MAP ppChunk)
{
    int rc;

    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Move the chunk time forward.
     */
    pVM->pgm.s.ChunkR3Map.iNow++;
    if (pVM->pgm.s.ChunkR3Map.iNow == 0)
    {
        pVM->pgm.s.ChunkR3Map.iNow = 4;
        RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkAgeingRolloverCallback, NULL);
    }

    /*
     * Allocate a new tracking structure first.
     */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)MMR3HeapAllocZ(pVM, MM_TAG_PGM_CHUNK_MAPPING, sizeof(*pChunk));
#else
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)MMR3UkHeapAllocZ(pVM, MM_TAG_PGM_CHUNK_MAPPING, sizeof(*pChunk), NULL);
#endif
    AssertReturn(pChunk, VERR_NO_MEMORY);
    pChunk->Core.Key  = idChunk;
    pChunk->iLastUsed = pVM->pgm.s.ChunkR3Map.iNow;

    /*
     * Request the ring-0 part to map the chunk in question.
     */
    GMMMAPUNMAPCHUNKREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq    = sizeof(Req);
    Req.pvR3         = NULL;
    Req.idChunkMap   = idChunk;
    Req.idChunkUnmap = NIL_GMM_CHUNKID;

    /* Must be callable from any thread, so can't use VMMR3CallR0. */
    STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkMap, a);
    rc = SUPR3CallVMMR0Ex(pVM->pVMR0, NIL_VMCPUID, VMMR0_DO_GMM_MAP_UNMAP_CHUNK, 0, &Req.Hdr);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatChunkMap, a);
    if (RT_SUCCESS(rc))
    {
        pChunk->pv = Req.pvR3;

        /*
         * If we're running out of virtual address space, then we should
         * unmap another chunk.
         *
         * Currently, an unmap operation requires that all other virtual CPUs
         * are idling and not by chance making use of the memory we're
         * unmapping.  So, we create an async unmap operation here.
         *
         * Now, when creating or restoring a saved state this wont work very
         * well since we may want to restore all guest RAM + a little something.
         * So, we have to do the unmap synchronously.  Fortunately for us
         * though, during these operations the other virtual CPUs are inactive
         * and it should be safe to do this.
         */
        /** @todo Eventually we should lock all memory when used and do
         *        map+unmap as one kernel call without any rendezvous or
         *        other precautions. */
        if (pVM->pgm.s.ChunkR3Map.c + 1 >= pVM->pgm.s.ChunkR3Map.cMax)
        {
            switch (VMR3GetState(pVM))
            {
                case VMSTATE_LOADING:
                case VMSTATE_SAVING:
                {
                    PVMCPU pVCpu = VMMGetCpu(pVM);
                    if (   pVCpu
                        && pVM->pgm.s.cDeprecatedPageLocks == 0)
                    {
                        pgmR3PhysUnmapChunkRendezvous(pVM, pVCpu, NULL);
                        break;
                    }
                    /* fall thru */
                }
                default:
                    rc = VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE, (PFNRT)pgmR3PhysUnmapChunk, 1, pVM);
                    AssertRC(rc);
                    break;
            }
        }

        /*
         * Update the tree.  We must do this after any unmapping to make sure
         * the chunk we're going to return isn't unmapped by accident.
         */
        AssertPtr(Req.pvR3);
        bool fRc = RTAvlU32Insert(&pVM->pgm.s.ChunkR3Map.pTree, &pChunk->Core);
        AssertRelease(fRc);
        pVM->pgm.s.ChunkR3Map.c++;
        pVM->pgm.s.cMappedChunks++;
    }
    else
    {
        /** @todo this may fail because of /proc/sys/vm/max_map_count, so we
         *        should probably restrict ourselves on linux. */
        AssertRC(rc);
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        MMR3HeapFree(pChunk);
#else
        MMR3UkHeapFree(pVM, pChunk, MM_TAG_PGM_CHUNK_MAPPING);
#endif
        pChunk = NULL;
    }

    *ppChunk = pChunk;
    return rc;
}


/**
 * For VMMCALLRING3_PGM_MAP_CHUNK, considered internal.
 *
 * @returns see pgmR3PhysChunkMap.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk to map.
 */
VMMR3DECL(int) PGMR3PhysChunkMap(PVM pVM, uint32_t idChunk)
{
    PPGMCHUNKR3MAP pChunk;
    int rc;

    pgmLock(pVM);
    rc = pgmR3PhysChunkMap(pVM, idChunk, &pChunk);
    pgmUnlock(pVM);
    return rc;
}


/**
 * Invalidates the TLB for the ring-3 mapping cache.
 *
 * @param   pVM         The VM handle.
 */
VMMR3DECL(void) PGMR3PhysChunkInvalidateTLB(PVM pVM)
{
    pgmLock(pVM);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
    {
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk = NIL_GMM_CHUNKID;
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk = NULL;
    }
    /* The page map TLB references chunks, so invalidate that one too. */
    pgmPhysInvalidatePageMapTLB(pVM);
    pgmUnlock(pVM);
}


/**
 * Response to VMMCALLRING3_PGM_ALLOCATE_LARGE_PAGE to allocate a large (2MB) page
 * for use with a nested paging PDE.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      GC physical start address of the 2 MB range
 */
VMMR3DECL(int) PGMR3PhysAllocateLargeHandyPage(PVM pVM, RTGCPHYS GCPhys)
{
#ifdef PGM_WITH_LARGE_PAGES
    uint64_t u64TimeStamp1, u64TimeStamp2;

    pgmLock(pVM);

    STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->StatAllocLargePage, a);
    u64TimeStamp1 = RTTimeMilliTS();
    int rc = VMMR3CallR0(pVM, VMMR0_DO_PGM_ALLOCATE_LARGE_HANDY_PAGE, 0, NULL);
    u64TimeStamp2 = RTTimeMilliTS();
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatAllocLargePage, a);
    if (RT_SUCCESS(rc))
    {
        Assert(pVM->pgm.s.cLargeHandyPages == 1);

        uint32_t idPage = pVM->pgm.s.aLargeHandyPage[0].idPage;
        RTHCPHYS HCPhys = pVM->pgm.s.aLargeHandyPage[0].HCPhysGCPhys;

        void *pv;

        /* Map the large page into our address space.
         *
         * Note: assuming that within the 2 MB range:
         * - GCPhys + PAGE_SIZE = HCPhys + PAGE_SIZE (whole point of this exercise)
         * - user space mapping is continuous as well
         * - page id (GCPhys) + 1 = page id (GCPhys + PAGE_SIZE)
         */
        rc = pgmPhysPageMapByPageID(pVM, idPage, HCPhys, &pv);
        AssertLogRelMsg(RT_SUCCESS(rc), ("idPage=%#x HCPhysGCPhys=%RHp rc=%Rrc\n", idPage, HCPhys, rc));

        if (RT_SUCCESS(rc))
        {
            /*
             * Clear the pages.
             */
            STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->StatClearLargePage, b);
            for (unsigned i = 0; i < _2M/PAGE_SIZE; i++)
            {
                ASMMemZeroPage(pv);

                PPGMPAGE pPage;
                rc = pgmPhysGetPageEx(pVM, GCPhys, &pPage);
                AssertRC(rc);

                Assert(PGM_PAGE_IS_ZERO(pPage));
                STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->StatRZPageReplaceZero);
                pVM->pgm.s.cZeroPages--;

                /*
                 * Do the PGMPAGE modifications.
                 */
                pVM->pgm.s.cPrivatePages++;
                PGM_PAGE_SET_HCPHYS(pVM, pPage, HCPhys);
                PGM_PAGE_SET_PAGEID(pVM, pPage, idPage);
                PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
                PGM_PAGE_SET_PDE_TYPE(pVM, pPage, PGM_PAGE_PDE_TYPE_PDE);
                PGM_PAGE_SET_PTE_INDEX(pVM, pPage, 0);
                PGM_PAGE_SET_TRACKING(pVM, pPage, 0);

                /* Somewhat dirty assumption that page ids are increasing. */
                idPage++;

                HCPhys += PAGE_SIZE;
                GCPhys += PAGE_SIZE;

                pv = (void *)((uintptr_t)pv + PAGE_SIZE);

                Log3(("PGMR3PhysAllocateLargePage: idPage=%#x HCPhys=%RGp\n", idPage, HCPhys));
            }
            STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->StatClearLargePage, b);

            /* Flush all TLBs. */
            PGM_INVL_ALL_VCPU_TLBS(pVM);
            pgmPhysInvalidatePageMapTLB(pVM);
        }
        pVM->pgm.s.cLargeHandyPages = 0;
    }

    if (RT_SUCCESS(rc))
    {
        static uint32_t cTimeOut = 0;
        uint64_t u64TimeStampDelta = u64TimeStamp2 - u64TimeStamp1;

        if (u64TimeStampDelta > 100)
        {
            STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->StatLargePageOverflow);
            if (    ++cTimeOut > 10
                ||  u64TimeStampDelta > 1000 /* more than one second forces an early retirement from allocating large pages. */)
            {
                /* If repeated attempts to allocate a large page takes more than 100 ms, then we fall back to normal 4k pages.
                 * E.g. Vista 64 tries to move memory around, which takes a huge amount of time.
                 */
                LogRel(("PGMR3PhysAllocateLargePage: allocating large pages takes too long (last attempt %d ms; nr of timeouts %d); DISABLE\n", u64TimeStampDelta, cTimeOut));
                PGMSetLargePageUsage(pVM, false);
            }
        }
        else
        if (cTimeOut > 0)
            cTimeOut--;
    }

    pgmUnlock(pVM);
    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif /* PGM_WITH_LARGE_PAGES */
}


/**
 * Response to VM_FF_PGM_NEED_HANDY_PAGES and VMMCALLRING3_PGM_ALLOCATE_HANDY_PAGES.
 *
 * This function will also work the VM_FF_PGM_NO_MEMORY force action flag, to
 * signal and clear the out of memory condition. When contracted, this API is
 * used to try clear the condition when the user wants to resume.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FFs cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is not cleared in
 *          this case and it gets accompanied by VM_FF_PGM_NO_MEMORY.
 *
 * @param   pVM         The VM handle.
 *
 * @remarks The VINF_EM_NO_MEMORY status is for the benefit of the FF processing
 *          in EM.cpp and shouldn't be propagated outside TRPM, HWACCM, EM and
 *          pgmPhysEnsureHandyPage. There is one exception to this in the \#PF
 *          handler.
 */
VMMR3DECL(int) PGMR3PhysAllocateHandyPages(PVM pVM)
{
    pgmLock(pVM);

    /*
     * Allocate more pages, noting down the index of the first new page.
     */
    uint32_t iClear = pVM->pgm.s.cHandyPages;
    AssertMsgReturn(iClear <= RT_ELEMENTS(pVM->pgm.s.aHandyPages), ("%d", iClear), VERR_PGM_HANDY_PAGE_IPE);
    Log(("PGMR3PhysAllocateHandyPages: %d -> %d\n", iClear, RT_ELEMENTS(pVM->pgm.s.aHandyPages)));
    int rcAlloc = VINF_SUCCESS;
    int rcSeed  = VINF_SUCCESS;
    int rc = VMMR3CallR0(pVM, VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES, 0, NULL);
    while (rc == VERR_GMM_SEED_ME)
    {
        void *pvChunk;
        rcAlloc = rc = SUPR3PageAlloc(GMM_CHUNK_SIZE >> PAGE_SHIFT, &pvChunk);
        if (RT_SUCCESS(rc))
        {
            rcSeed = rc = VMMR3CallR0(pVM, VMMR0_DO_GMM_SEED_CHUNK, (uintptr_t)pvChunk, NULL);
            if (RT_FAILURE(rc))
                SUPR3PageFree(pvChunk, GMM_CHUNK_SIZE >> PAGE_SHIFT);
        }
        if (RT_SUCCESS(rc))
            rc = VMMR3CallR0(pVM, VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES, 0, NULL);
    }

    /* todo: we should split this up into an allocate and flush operation. sometimes you want to flush and not allocate more (which will trigger the vm account limit error) */
    if (    rc == VERR_GMM_HIT_VM_ACCOUNT_LIMIT
        &&  pVM->pgm.s.cHandyPages > 0)
    {
        /* Still handy pages left, so don't panic. */
        rc = VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
        AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc));
        Assert(pVM->pgm.s.cHandyPages > 0);
        VM_FF_CLEAR(pVM, VM_FF_PGM_NEED_HANDY_PAGES);
        VM_FF_CLEAR(pVM, VM_FF_PGM_NO_MEMORY);

#ifdef VBOX_STRICT
        uint32_t i;
        for (i = iClear; i < pVM->pgm.s.cHandyPages; i++)
            if (   pVM->pgm.s.aHandyPages[i].idPage == NIL_GMM_PAGEID
                || pVM->pgm.s.aHandyPages[i].idSharedPage != NIL_GMM_PAGEID
                || (pVM->pgm.s.aHandyPages[i].HCPhysGCPhys & PAGE_OFFSET_MASK))
                break;
        if (i != pVM->pgm.s.cHandyPages)
        {
            RTAssertMsg1Weak(NULL, __LINE__, __FILE__, __FUNCTION__);
            RTAssertMsg2Weak("i=%d iClear=%d cHandyPages=%d\n", i, iClear, pVM->pgm.s.cHandyPages);
            for (uint32_t j = iClear; j < pVM->pgm.s.cHandyPages; j++)
                RTAssertMsg2Add(("%03d: idPage=%d HCPhysGCPhys=%RHp idSharedPage=%d%\n", j,
                                 pVM->pgm.s.aHandyPages[j].idPage,
                                 pVM->pgm.s.aHandyPages[j].HCPhysGCPhys,
                                 pVM->pgm.s.aHandyPages[j].idSharedPage,
                                 j == i ? " <---" : ""));
            RTAssertPanic();
        }
#endif
        /*
         * Clear the pages.
         */
        while (iClear < pVM->pgm.s.cHandyPages)
        {
            PGMMPAGEDESC pPage = &pVM->pgm.s.aHandyPages[iClear];
            void *pv;
            rc = pgmPhysPageMapByPageID(pVM, pPage->idPage, pPage->HCPhysGCPhys, &pv);
            AssertLogRelMsgBreak(RT_SUCCESS(rc),
                                 ("%u/%u: idPage=%#x HCPhysGCPhys=%RHp rc=%Rrc\n",
                                  iClear, pVM->pgm.s.cHandyPages, pPage->idPage, pPage->HCPhysGCPhys, rc));
            ASMMemZeroPage(pv);
            iClear++;
            Log3(("PGMR3PhysAllocateHandyPages: idPage=%#x HCPhys=%RGp\n", pPage->idPage, pPage->HCPhysGCPhys));
        }
    }
    else
    {
        uint64_t cAllocPages, cMaxPages, cBalloonPages;

        /*
         * We should never get here unless there is a genuine shortage of
         * memory (or some internal error). Flag the error so the VM can be
         * suspended ASAP and the user informed. If we're totally out of
         * handy pages we will return failure.
         */
        /* Report the failure. */
        LogRel(("PGM: Failed to procure handy pages; rc=%Rrc rcAlloc=%Rrc rcSeed=%Rrc cHandyPages=%#x\n"
                "     cAllPages=%#x cPrivatePages=%#x cSharedPages=%#x cZeroPages=%#x\n",
                rc, rcAlloc, rcSeed,
                pVM->pgm.s.cHandyPages,
                pVM->pgm.s.cAllPages,
                pVM->pgm.s.cPrivatePages,
                pVM->pgm.s.cSharedPages,
                pVM->pgm.s.cZeroPages));

        if (GMMR3QueryMemoryStats(pVM, &cAllocPages, &cMaxPages, &cBalloonPages) == VINF_SUCCESS)
        {
            LogRel(("GMM: Statistics:\n"
                    "     Allocated pages: %RX64\n"
                    "     Maximum   pages: %RX64\n"
                    "     Ballooned pages: %RX64\n", cAllocPages, cMaxPages, cBalloonPages));
        }

        if (   rc != VERR_NO_MEMORY
            && rc != VERR_NO_PHYS_MEMORY
            && rc != VERR_LOCK_FAILED)
        {
            for (uint32_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
            {
                LogRel(("PGM: aHandyPages[#%#04x] = {.HCPhysGCPhys=%RHp, .idPage=%#08x, .idSharedPage=%#08x}\n",
                        i, pVM->pgm.s.aHandyPages[i].HCPhysGCPhys, pVM->pgm.s.aHandyPages[i].idPage,
                        pVM->pgm.s.aHandyPages[i].idSharedPage));
                uint32_t const idPage = pVM->pgm.s.aHandyPages[i].idPage;
                if (idPage != NIL_GMM_PAGEID)
                {
                    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesXR3;
                         pRam;
                         pRam = pRam->pNextR3)
                    {
                        uint32_t const cPages = pRam->cb >> PAGE_SHIFT;
                        for (uint32_t iPage = 0; iPage < cPages; iPage++)
                            if (PGM_PAGE_GET_PAGEID(&pRam->aPages[iPage]) == idPage)
                                LogRel(("PGM: Used by %RGp %R[pgmpage] (%s)\n",
                                        pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pRam->aPages[iPage], pRam->pszDesc));
                    }
                }
            }
        }

        /* Set the FFs and adjust rc. */
        VM_FF_SET(pVM, VM_FF_PGM_NEED_HANDY_PAGES);
        VM_FF_SET(pVM, VM_FF_PGM_NO_MEMORY);
        if (    rc == VERR_NO_MEMORY
            ||  rc == VERR_NO_PHYS_MEMORY
            ||  rc == VERR_LOCK_FAILED)
            rc = VINF_EM_NO_MEMORY;
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Frees the specified RAM page and replaces it with the ZERO page.
 *
 * This is used by ballooning, remapping MMIO2, RAM reset and state loading.
 *
 * @param   pVM             The VM handle.
 * @param   pReq            Pointer to the request.
 * @param   pcPendingPages  Where the number of pages waiting to be freed are
 *                          kept.  This will normally be incremented.
 * @param   pPage           Pointer to the page structure.
 * @param   GCPhys          The guest physical address of the page, if applicable.
 *
 * @remarks The caller must own the PGM lock.
 */
int pgmPhysFreePage(PVM pVM, PGMMFREEPAGESREQ pReq, uint32_t *pcPendingPages, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    /*
     * Assert sanity.
     */
    PGM_LOCK_ASSERT_OWNER(pVM);
    if (RT_UNLIKELY(    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_RAM
                    &&  PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM_SHADOW))
    {
        AssertMsgFailed(("GCPhys=%RGp pPage=%R[pgmpage]\n", GCPhys, pPage));
        return VMSetError(pVM, VERR_PGM_PHYS_NOT_RAM, RT_SRC_POS, "GCPhys=%RGp type=%d", GCPhys, PGM_PAGE_GET_TYPE(pPage));
    }

    /** @todo What about ballooning of large pages??! */
    Assert(   PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE
           && PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE_DISABLED);

    if (    PGM_PAGE_IS_ZERO(pPage)
        ||  PGM_PAGE_IS_BALLOONED(pPage))
        return VINF_SUCCESS;

    const uint32_t idPage = PGM_PAGE_GET_PAGEID(pPage);
    Log3(("pgmPhysFreePage: idPage=%#x GCPhys=%RGp pPage=%R[pgmpage]\n", idPage, GCPhys, pPage));
    if (RT_UNLIKELY(    idPage == NIL_GMM_PAGEID
                    ||  idPage > GMM_PAGEID_LAST
                    ||  PGM_PAGE_GET_CHUNKID(pPage) == NIL_GMM_CHUNKID))
    {
        AssertMsgFailed(("GCPhys=%RGp pPage=%R[pgmpage]\n", GCPhys, pPage));
        return VMSetError(pVM, VERR_PGM_PHYS_INVALID_PAGE_ID, RT_SRC_POS, "GCPhys=%RGp idPage=%#x", GCPhys, pPage);
    }

    /* update page count stats. */
    if (PGM_PAGE_IS_SHARED(pPage))
        pVM->pgm.s.cSharedPages--;
    else
        pVM->pgm.s.cPrivatePages--;
    pVM->pgm.s.cZeroPages++;

    /* Deal with write monitored pages. */
    if (PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED)
    {
        PGM_PAGE_SET_WRITTEN_TO(pVM, pPage);
        pVM->pgm.s.cWrittenToPages++;
    }

    /*
     * pPage = ZERO page.
     */
    PGM_PAGE_SET_HCPHYS(pVM, pPage, pVM->pgm.s.HCPhysZeroPg);
    PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
    PGM_PAGE_SET_PAGEID(pVM, pPage, NIL_GMM_PAGEID);
    PGM_PAGE_SET_PDE_TYPE(pVM, pPage, PGM_PAGE_PDE_TYPE_DONTCARE);
    PGM_PAGE_SET_PTE_INDEX(pVM, pPage, 0);
    PGM_PAGE_SET_TRACKING(pVM, pPage, 0);

    /* Flush physical page map TLB entry. */
    pgmPhysInvalidatePageMapTLBEntry(pVM, GCPhys);

    /*
     * Make sure it's not in the handy page array.
     */
    for (uint32_t i = pVM->pgm.s.cHandyPages; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
    {
        if (pVM->pgm.s.aHandyPages[i].idPage == idPage)
        {
            pVM->pgm.s.aHandyPages[i].idPage = NIL_GMM_PAGEID;
            break;
        }
        if (pVM->pgm.s.aHandyPages[i].idSharedPage == idPage)
        {
            pVM->pgm.s.aHandyPages[i].idSharedPage = NIL_GMM_PAGEID;
            break;
        }
    }

    /*
     * Push it onto the page array.
     */
    uint32_t iPage = *pcPendingPages;
    Assert(iPage < PGMPHYS_FREE_PAGE_BATCH_SIZE);
    *pcPendingPages += 1;

    pReq->aPages[iPage].idPage = idPage;

    if (iPage + 1 < PGMPHYS_FREE_PAGE_BATCH_SIZE)
        return VINF_SUCCESS;

    /*
     * Flush the pages.
     */
    int rc = GMMR3FreePagesPerform(pVM, pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE);
    if (RT_SUCCESS(rc))
    {
        GMMR3FreePagesRePrep(pVM, pReq, PGMPHYS_FREE_PAGE_BATCH_SIZE, GMMACCOUNT_BASE);
        *pcPendingPages = 0;
    }
    return rc;
}


/**
 * Converts a GC physical address to a HC ring-3 pointer, with some
 * additional checks.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_PHYS_TLB_CATCH_WRITE and *ppv set if the page has a write
 *          access handler of some kind.
 * @retval  VERR_PGM_PHYS_TLB_CATCH_ALL if the page has a handler catching all
 *          accesses or is odd in any way.
 * @retval  VERR_PGM_PHYS_TLB_UNASSIGNED if the page doesn't exist.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The GC physical address to convert.
 * @param   fWritable   Whether write access is required.
 * @param   ppv         Where to store the pointer corresponding to GCPhys on
 *                      success.
 */
VMMR3DECL(int) PGMR3PhysTlbGCPhys2Ptr(PVM pVM, RTGCPHYS GCPhys, bool fWritable, void **ppv)
{
    pgmLock(pVM);

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(pVM, GCPhys, &pPage, &pRam);
    if (RT_SUCCESS(rc))
    {
        if (PGM_PAGE_IS_BALLOONED(pPage))
            rc = VINF_PGM_PHYS_TLB_CATCH_WRITE;
        else if (!PGM_PAGE_HAS_ANY_HANDLERS(pPage))
            rc = VINF_SUCCESS;
        else
        {
            if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)) /* catches MMIO */
                rc = VERR_PGM_PHYS_TLB_CATCH_ALL;
            else if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
            {
                /** @todo Handle TLB loads of virtual handlers so ./test.sh can be made to work
                 *        in -norawr0 mode. */
                if (fWritable)
                    rc = VINF_PGM_PHYS_TLB_CATCH_WRITE;
            }
            else
            {
                /* Temporarily disabled physical handler(s), since the recompiler
                   doesn't get notified when it's reset we'll have to pretend it's
                   operating normally. */
                if (pgmHandlerPhysicalIsAll(pVM, GCPhys))
                    rc = VERR_PGM_PHYS_TLB_CATCH_ALL;
                else
                    rc = VINF_PGM_PHYS_TLB_CATCH_WRITE;
            }
        }
        if (RT_SUCCESS(rc))
        {
            int rc2;

            /* Make sure what we return is writable. */
            if (fWritable)
                switch (PGM_PAGE_GET_STATE(pPage))
                {
                    case PGM_PAGE_STATE_ALLOCATED:
                        break;
                    case PGM_PAGE_STATE_BALLOONED:
                        AssertFailed();
                        break;
                    case PGM_PAGE_STATE_ZERO:
                    case PGM_PAGE_STATE_SHARED:
                        if (rc == VINF_PGM_PHYS_TLB_CATCH_WRITE)
                            break;
                    case PGM_PAGE_STATE_WRITE_MONITORED:
                        rc2 = pgmPhysPageMakeWritable(pVM, pPage, GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK);
                        AssertLogRelRCReturn(rc2, rc2);
                        break;
                }

            /* Get a ring-3 mapping of the address. */
            PPGMPAGER3MAPTLBE pTlbe;
            rc2 = pgmPhysPageQueryTlbe(pVM, GCPhys, &pTlbe);
            AssertLogRelRCReturn(rc2, rc2);
            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
            /** @todo mapping/locking hell; this isn't horribly efficient since
             *        pgmPhysPageLoadIntoTlb will repeat the lookup we've done here. */

            Log6(("PGMR3PhysTlbGCPhys2Ptr: GCPhys=%RGp rc=%Rrc pPage=%R[pgmpage] *ppv=%p\n", GCPhys, rc, pPage, *ppv));
        }
        else
            Log6(("PGMR3PhysTlbGCPhys2Ptr: GCPhys=%RGp rc=%Rrc pPage=%R[pgmpage]\n", GCPhys, rc, pPage));

        /* else: handler catching all access, no pointer returned. */
    }
    else
        rc = VERR_PGM_PHYS_TLB_UNASSIGNED;

    pgmUnlock(pVM);
    return rc;
}

