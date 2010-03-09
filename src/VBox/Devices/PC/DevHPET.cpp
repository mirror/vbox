/* $Id$ */
/** @file
 * HPET virtual device - high precision event timer emulation
 */
/*
 * Copyright (C) 2009-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HPET
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm.h>

#include "../Builtins.h"


/*
 * Current limitations:
 *   - not entirely correct time of interrupt, i.e. never
 *     schedule interrupt earlier than in 1ms
 *   - statistics not implemented
 *   - level-triggered mode not implemented
 */
/*
 * Base address for MMIO
 */
#define HPET_BASE                   0xfed00000

/*
 * Number of available timers, cannot be changed without
 * breaking saved states.
 */
#define HPET_NUM_TIMERS             3

/*
 * 10000000 femtoseconds == 10ns
 */
#define HPET_CLK_PERIOD             10000000UL

/*
 * Femptosecods in nanosecond
 */
#define FS_PER_NS                   1000000

/*
 * Interrupt type
 */
#define HPET_TIMER_TYPE_LEVEL       1
#define HPET_TIMER_TYPE_EDGE        0

/* Delivery mode */
/* Via APIC */
#define HPET_TIMER_DELIVERY_APIC    0
/* Via FSB */
#define HPET_TIMER_DELIVERY_FSB     1

#define HPET_TIMER_CAP_FSB_INT_DEL (1 << 15)
#define HPET_TIMER_CAP_PER_INT     (1 << 4)

#define HPET_CFG_ENABLE          0x001 /* ENABLE_CNF */
#define HPET_CFG_LEGACY          0x002 /* LEG_RT_CNF */

#define HPET_ID                  0x000
#define HPET_PERIOD              0x004
#define HPET_CFG                 0x010
#define HPET_STATUS              0x020
#define HPET_COUNTER             0x0f0
#define HPET_TN_CFG              0x000
#define HPET_TN_CMP              0x008
#define HPET_TN_ROUTE            0x010
#define HPET_CFG_WRITE_MASK      0x3

#define HPET_TN_INT_TYPE                      (1 << 1)
#define HPET_TN_ENABLE                        (1 << 2)
#define HPET_TN_PERIODIC                      (1 << 3)
#define HPET_TN_PERIODIC_CAP                  (1 << 4)
#define HPET_TN_SIZE_CAP                      (1 << 5)
#define HPET_TN_SETVAL                        (1 << 6)
#define HPET_TN_32BIT                         (1 << 8)
#define HPET_TN_INT_ROUTE_MASK                0x3e00
#define HPET_TN_CFG_WRITE_MASK                0x3f4e
#define HPET_TN_INT_ROUTE_SHIFT               9
#define HPET_TN_INT_ROUTE_CAP_SHIFT           32
#define HPET_TN_CFG_BITS_READONLY_OR_RESERVED 0xffff80b1U

/** The version of the saved state. */
#define HPET_SAVED_STATE_VERSION       2

/* Empty saved state */
#define HPET_SAVED_STATE_VERSION_EMPTY 1

struct HpetState;
typedef struct HpetTimer
{
    /** The HPET timer - R3 Ptr. */
    PTMTIMERR3                       pTimerR3;
    /** Pointer to the instance data - R3 Ptr. */
    R3PTRTYPE(struct HpetState *)    pHpetR3;

    /** The HPET timer - R0 Ptr. */
    PTMTIMERR0                       pTimerR0;
    /** Pointer to the instance data - R0 Ptr. */
    R0PTRTYPE(struct HpetState *)    pHpetR0;

    /** The HPET timer - RC Ptr. */
    PTMTIMERRC                       pTimerRC;
    /** Pointer to the instance data - RC Ptr. */
    RCPTRTYPE(struct HpetState *)    pHpetRC;

    /* timer number*/
    uint8_t                          u8TimerNumber;
     /* Wrap */
    uint8_t                          u8Wrap;
    /* Alignment */
    uint32_t                         alignment0;
    /* Memory-mapped, software visible timer registers */
    /* Configuration/capabilities */
    uint64_t                         u64Config;
    /* comparator */
    uint64_t                         u64Cmp;
    /* FSB route, not supported now */
    uint64_t                         u64Fsb;

    /* Hidden register state */
    /* Last value written to comparator */
    uint64_t                         u64Period;
} HpetTimer;

typedef struct HpetState
{
    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3         pDevInsR3;
    /** The HPET helpers - R3 Ptr. */
    PCPDMHPETHLPR3       pHpetHlpR3;

    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0         pDevInsR0;
    /** The HPET helpers - R0 Ptr. */
    PCPDMHPETHLPR0       pHpetHlpR0;

    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC         pDevInsRC;
    /** The HPET helpers - RC Ptr. */
    PCPDMHPETHLPRC       pHpetHlpRC;

    /* Timer structures */
    HpetTimer            aTimers[HPET_NUM_TIMERS];

    /* Offset realtive to the system clock */
    uint64_t             u64HpetOffset;

    /* Memory-mapped, software visible registers */
    /* capabilities */
    uint64_t             u64Capabilities;
    /* configuration */
    uint64_t             u64Config;
    /* interrupt status register */
    uint64_t             u64Isr;
    /* main counter */
    uint64_t             u64HpetCounter;

    /* Global device lock */
    PDMCRITSECT          csLock;
} HpetState;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/*
 * We shall declare MMIO accessors as extern "C" to avoid name mangling
 * and let them be found during R0/RC module init.
 * Maybe PDMBOTHCBDECL macro shall have extern "C" part in it.
 */

RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) hpetMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) hpetMMIORead (PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
RT_C_DECLS_END


/*
 * Temporary control to disble locking if problems found
 */
static const bool fHpetLocking = true;

DECLINLINE(int) hpetLock(HpetState* pThis, int rcBusy)
{
    if (!fHpetLocking)
        return VINF_SUCCESS;

    return PDMCritSectEnter(&pThis->csLock, rcBusy);
}

DECLINLINE(void) hpetUnlock(HpetState* pThis)
{
    if (!fHpetLocking)
        return;

    PDMCritSectLeave(&pThis->csLock);
}

static uint32_t hpetTimeAfter32(uint64_t a, uint64_t b)
{
    return ((int32_t)(b) - (int32_t)(a) <= 0);
}

static uint32_t hpetTimeAfter64(uint64_t a, uint64_t b)
{
    return ((int64_t)(b) - (int64_t)(a) <= 0);
}

static uint64_t hpetTicksToNs(uint64_t value)
{
    return (ASMMultU64ByU32DivByU32(value, HPET_CLK_PERIOD, FS_PER_NS));
}

static uint64_t nsToHpetTicks(uint64_t u64Value)
{
    return (ASMMultU64ByU32DivByU32(u64Value, FS_PER_NS, HPET_CLK_PERIOD));
}

static uint64_t hpetGetTicks(HpetState* pThis)
{
    /*
     * We can use any timer to get current time, they all go
     * with the same speed.
     */
    return nsToHpetTicks(TMTimerGet(pThis->aTimers[0].CTX_SUFF(pTimer)) +
                         pThis->u64HpetOffset);
}

static uint64_t updateMasked(uint64_t u64NewValue,
                             uint64_t u64OldValue,
                             uint64_t u64Mask)
{
    u64NewValue &= u64Mask;
    u64NewValue |= u64OldValue & ~u64Mask;
    return u64NewValue;
}

static bool isBitJustSet(uint64_t u64OldValue,
                         uint64_t u64NewValue,
                         uint64_t u64Mask)
{
    return (!(u64OldValue & u64Mask) && (u64NewValue & u64Mask));
}

static bool isBitJustCleared(uint64_t u64OldValue,
                             uint64_t u64NewValue,
                             uint64_t u64Mask)
{
    return ((u64OldValue & u64Mask) && !(u64NewValue & u64Mask));
}

DECLINLINE(uint64_t) hpetComputeDiff(HpetTimer* pTimer,
                                     uint64_t   u64Now)
{

    if (pTimer->u64Config & HPET_TN_32BIT)
    {
        uint32_t u32Diff;

        u32Diff = (uint32_t)pTimer->u64Cmp - (uint32_t)u64Now;
        u32Diff = ((int32_t)u32Diff > 0) ? u32Diff : (uint32_t)0;
        return (uint64_t)u32Diff;
    } else {
        uint64_t u64Diff;

        u64Diff = pTimer->u64Cmp - u64Now;
        u64Diff = ((int64_t)u64Diff > 0) ?  u64Diff : (uint64_t)0;
        return u64Diff;
    }
}


static void hpetAdjustComparator(HpetTimer* pTimer,
                                 uint64_t   u64Now)
{
  uint64_t u64Period = pTimer->u64Period;
  if ((pTimer->u64Config & HPET_TN_PERIODIC) && (u64Period != 0))
  {
      /* While loop is suboptimal */
      if (pTimer->u64Config & HPET_TN_32BIT)
      {
          while (hpetTimeAfter32(u64Now, pTimer->u64Cmp))
              pTimer->u64Cmp = (uint32_t)(pTimer->u64Cmp + u64Period);
      }
      else
      {
          while (hpetTimeAfter64(u64Now, pTimer->u64Cmp))
              pTimer->u64Cmp += u64Period;
      }
  }
}

static void hpetProgramTimer(HpetTimer *pTimer)
{
    uint64_t u64Diff;
    uint32_t u32TillWrap;
    uint64_t u64Ticks = hpetGetTicks(pTimer->CTX_SUFF(pHpet));

    /* no wrapping on new timers */
    pTimer->u8Wrap = 0;

    hpetAdjustComparator(pTimer, u64Ticks);

    u64Diff = hpetComputeDiff(pTimer, u64Ticks);

    /* Spec says in one-shot 32-bit mode, generate an interrupt when
     * counter wraps in addition to an interrupt with comparator match.
     */
    if ((pTimer->u64Config & HPET_TN_32BIT) && !(pTimer->u64Config & HPET_TN_PERIODIC))
    {
        u32TillWrap = 0xffffffff - (uint32_t)u64Ticks;
        if (u32TillWrap < (uint32_t)u64Diff)
        {
            u64Diff = u32TillWrap;
            pTimer->u8Wrap = 1;
        }
    }

    /* Avoid killing VM with interrupts */
#if 1
    /* @todo: HACK, rethink, may have negative impact on the guest */
    if (u64Diff == 0)
        u64Diff = 100000; /* 1 millisecond */
#endif

    Log4(("HPET: next IRQ in %lld ticks (%lld ns)\n", u64Diff, hpetTicksToNs(u64Diff)));

    TMTimerSetNano(pTimer->CTX_SUFF(pTimer), hpetTicksToNs(u64Diff));
}

static uint32_t getTimerIrq(struct HpetTimer *pTimer)
{
    /*
     * Per spec, in legacy mode HPET timers wired as:
     *   timer 0: IRQ0 for PIC and IRQ2 for APIC
     *   timer 1: IRQ8 for both PIC and APIC
     * As primary usecase for HPET is APIC config, we pretend
     * being always APIC, although for safety we shall check currect IC.
     * @todo: implement private interface between HPET and PDM
     *        to allow figuring that out and enabling/disabling
     *        PIT and RTC
     */
    if ((pTimer->u8TimerNumber <= 1) &&
        (pTimer->CTX_SUFF(pHpet)->u64Config & HPET_CFG_LEGACY))
        return (pTimer->u8TimerNumber == 0) ? 2 : 8;
    else
        return (pTimer->u64Config & HPET_TN_INT_ROUTE_MASK) >> HPET_TN_INT_ROUTE_SHIFT;
}

static int timerRegRead32(HpetState* pThis,
                          uint32_t   iTimerNo,
                          uint32_t   iTimerReg,
                          uint32_t * pValue)
{
    HpetTimer *pTimer;

    if (iTimerNo >= HPET_NUM_TIMERS)
    {
        LogRel(("HPET: using timer above configured range: %d\n", iTimerNo));
        return VINF_SUCCESS;
    }

    pTimer = &pThis->aTimers[iTimerNo];

    switch (iTimerReg)
    {
        case HPET_TN_CFG:
            Log(("read HPET_TN_CFG on %d\n", pTimer->u8TimerNumber));
            *pValue = (uint32_t)(pTimer->u64Config);
            break;
        case HPET_TN_CFG + 4:
            Log(("read HPET_TN_CFG+4 on %d\n", pTimer->u8TimerNumber));
            *pValue = (uint32_t)(pTimer->u64Config >> 32);
            break;
        case HPET_TN_CMP:
            Log(("read HPET_TN_CMP on %d, cmp=%llx\n", pTimer->u8TimerNumber, pTimer->u64Cmp));
            *pValue = (uint32_t)(pTimer->u64Cmp);
            break;
        case HPET_TN_CMP + 4:
            Log(("read HPET_TN_CMP+4 on %d, cmp=%llx\n", pTimer->u8TimerNumber, pTimer->u64Cmp));
            *pValue = (uint32_t)(pTimer->u64Cmp >> 32);
            break;
        case HPET_TN_ROUTE:
            Log(("read HPET_TN_ROUTE on %d\n", pTimer->u8TimerNumber));
            *pValue = (uint32_t)(pTimer->u64Fsb >> 32);
            break;
        default:
            LogRel(("invalid HPET register read %d on %d\n", iTimerReg, pTimer->u8TimerNumber));
            break;
        }

    return VINF_SUCCESS;
}

static int configRegRead32(HpetState* pThis,
                           uint32_t   iIndex,
                           uint32_t  *pValue)
{
    switch (iIndex)
    {
        case HPET_ID:
            Log(("read HPET_ID\n"));
            *pValue = (uint32_t)(pThis->u64Capabilities);
            break;
        case HPET_PERIOD:
            Log(("read HPET_PERIOD\n"));
            *pValue = (uint32_t)(pThis->u64Capabilities >> 32);
            break;
        case HPET_CFG:
            Log(("read HPET_CFG\n"));
            *pValue = (uint32_t)(pThis->u64Config);
            break;
        case HPET_CFG + 4:
            Log(("read of HPET_CFG + 4\n"));
            *pValue = (uint32_t)(pThis->u64Config >> 32);
            break;
        case HPET_COUNTER:
        case HPET_COUNTER + 4:
        {
            uint64_t u64Ticks;
            Log(("read HPET_COUNTER\n"));
            if (pThis->u64Config & HPET_CFG_ENABLE)
                u64Ticks = hpetGetTicks(pThis);
            else
                u64Ticks = pThis->u64HpetCounter;
            /** @todo: is it correct? */
            *pValue = (iIndex == HPET_COUNTER) ? (uint32_t)u64Ticks : (uint32_t)(u64Ticks >> 32);
            break;
        }
        case HPET_STATUS:
            Log(("read HPET_STATUS\n"));
            *pValue = (uint32_t)(pThis->u64Isr);
            break;
        default:
            Log(("invalid HPET register read: %x\n", iIndex));
            break;
    }
    return VINF_SUCCESS;
}

static int timerRegWrite32(HpetState* pThis,
                           uint32_t   iTimerNo,
                           uint32_t   iTimerReg,
                           uint32_t   iNewValue)
{
    HpetTimer * pTimer;
    uint64_t    iOldValue = 0;
    uint32_t    u32Temp;
    int         rc;

    if (iTimerNo >= HPET_NUM_TIMERS)
    {
        LogRel(("HPET: using timer above configured range: %d\n", iTimerNo));
        return VINF_SUCCESS;
    }
    pTimer = &pThis->aTimers[iTimerNo];

    rc = timerRegRead32(pThis, iTimerNo, iTimerReg, &u32Temp);
    if (RT_FAILURE(rc))
        return rc;
    iOldValue = u32Temp;

    switch (iTimerReg)
    {
        case HPET_TN_CFG:
        {
            Log(("write HPET_TN_CFG: %d\n", iTimerNo));
            if (iNewValue & HPET_TN_32BIT)
            {
                pTimer->u64Cmp = (uint32_t)pTimer->u64Cmp;
                pTimer->u64Period = (uint32_t)pTimer->u64Period;
            }
            if ((iNewValue & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_LEVEL) 
            {
                LogRel(("level-triggered config not yet supported\n"));
                Assert(false);
            }
            /** We only care about lower 32-bits so far */
            pTimer->u64Config =
                    updateMasked(iNewValue, iOldValue, HPET_TN_CFG_WRITE_MASK);
            break;
        }
        case HPET_TN_CFG + 4: /* Interrupt capabilities */
        {
            Log(("write HPET_TN_CFG + 4, useless\n"));
            break;
        }
        case HPET_TN_CMP: /* lower bits of comparator register */
        {
            Log(("write HPET_TN_CMP on %d: %x\n", iTimerNo, iNewValue));
            if (pTimer->u64Config & HPET_TN_32BIT)
                iNewValue = (uint32_t)iNewValue;

            if (pTimer->u64Config & HPET_TN_SETVAL)
            {
                /* HPET_TN_SETVAL allows to adjust comparator w/o updating period, and it's cleared on access */
                if (pTimer->u64Config & HPET_TN_32BIT)
                    pTimer->u64Config &= ~HPET_TN_SETVAL;
            } else if (pTimer->u64Config & HPET_TN_PERIODIC)
            {
                iNewValue &= (pTimer->u64Config & HPET_TN_32BIT ? ~0U : ~0ULL) >> 1;
                pTimer->u64Period = (pTimer->u64Period & 0xffffffff00000000ULL)
                        | iNewValue;
            }

            pTimer->u64Cmp = (pTimer->u64Cmp & 0xffffffff00000000ULL)
                    | iNewValue;

            Log2(("after HPET_TN_CMP cmp=%llx per=%llx\n", pTimer->u64Cmp, pTimer->u64Period));

            if (pThis->u64Config & HPET_CFG_ENABLE)
                hpetProgramTimer(pTimer);
            break;
        }
        case HPET_TN_CMP + 4: /* upper bits of comparator register */
        {
            Log(("write HPET_TN_CMP + 4 on %d: %x\n", iTimerNo, iNewValue));
            if (pTimer->u64Config & HPET_TN_32BIT)
                break;

            if (pTimer->u64Config & HPET_TN_SETVAL)
            {
                /* HPET_TN_SETVAL allows to adjust comparator w/o updating period, and it's cleared on access */
                pTimer->u64Config &= ~HPET_TN_SETVAL;
            } else if (pTimer->u64Config & HPET_TN_PERIODIC)
            {
                 pTimer->u64Period = (pTimer->u64Period & 0xffffffffULL)
                         | ((uint64_t)iNewValue << 32);
            }

            pTimer->u64Cmp = (pTimer->u64Cmp & 0xffffffffULL)
                    | ((uint64_t)iNewValue << 32);

            Log2(("after HPET_TN_CMP+4 cmp=%llx per=%llx\n", pTimer->u64Cmp, pTimer->u64Period));

            if (pThis->u64Config & HPET_CFG_ENABLE)
                hpetProgramTimer(pTimer);
            break;
        }
        case HPET_TN_ROUTE:
        {
            Log(("write HPET_TN_ROUTE\n"));
            break;
        }
        case HPET_TN_ROUTE + 4:
        {
            Log(("write HPET_TN_ROUTE + 4\n"));
            break;
        }
        default:
        {
            LogRel(("invalid timer register write: %d\n", iTimerReg));
            Assert(false);
            break;
        }
    }
    return VINF_SUCCESS;
}

static int hpetLegacyMode(HpetState* pThis,
                          bool       fActivate)
{
    int rc = VINF_SUCCESS;
#ifndef IN_RING3
    /* Don't do anything complicated outside of R3 */
    rc =  VINF_IOM_HC_MMIO_WRITE;
#else /* IN_RING3 */
    if (pThis->pHpetHlpR3)
        rc = pThis->pHpetHlpR3->pfnSetLegacyMode(pThis->pDevInsR3, fActivate);
#endif
    return rc;
}

static int configRegWrite32(HpetState* pThis,
                            uint32_t   iIndex,
                            uint32_t   iNewValue)
{
    int rc = VINF_SUCCESS;

    switch (iIndex)
    {
        case HPET_ID:
        case HPET_ID + 4:
        {
            Log(("write HPET_ID, useless\n"));
            break;
        }
        case HPET_CFG:
        {
            uint32_t i, iOldValue;

            Log(("write HPET_CFG: %x\n", iNewValue));

            iOldValue = (uint32_t)(pThis->u64Config);

            /*
             * This check must be here, before actual update, as hpetLegacyMode
             * may request retry in R3 - so we must keep state intact.
             */
            if (isBitJustSet(iOldValue, iNewValue, HPET_CFG_LEGACY))
            {
                rc = hpetLegacyMode(pThis, true);
            }
            else if (isBitJustCleared(iOldValue, iNewValue, HPET_CFG_LEGACY))
            {
                rc = hpetLegacyMode(pThis, false);
            }
            if (rc != VINF_SUCCESS)
                return rc;

            pThis->u64Config = updateMasked(iNewValue, iOldValue, HPET_CFG_WRITE_MASK);
            if (isBitJustSet(iOldValue, iNewValue, HPET_CFG_ENABLE))
            {
                /* Enable main counter and interrupt generation. */
                pThis->u64HpetOffset = hpetTicksToNs(pThis->u64HpetCounter)
                        - TMTimerGet(pThis->aTimers[0].CTX_SUFF(pTimer));
                for (i = 0; i < HPET_NUM_TIMERS; i++)
                    if (pThis->aTimers[i].u64Cmp != ~0ULL)
                        hpetProgramTimer(&pThis->aTimers[i]);
            }
            else if (isBitJustCleared(iOldValue, iNewValue, HPET_CFG_ENABLE))
            {
                /* Halt main counter and disable interrupt generation. */
                pThis->u64HpetCounter = hpetGetTicks(pThis);
                for (i = 0; i < HPET_NUM_TIMERS; i++)
                    TMTimerStop(pThis->aTimers[i].CTX_SUFF(pTimer));
            }
            break;
        }
        case HPET_CFG + 4:
        {
            Log(("write HPET_CFG + 4: %x\n", iNewValue));
            pThis->u64Config = updateMasked((uint64_t)iNewValue << 32,
                                            pThis->u64Config,
                                            0xffffffff00000000ULL);
            break;
        }
        case HPET_STATUS:
        {
            Log(("write HPET_STATUS: %x\n", iNewValue));
            // clear ISR for all set bits in iNewValue, see p. 14 of HPET spec
            pThis->u64Isr &= ~((uint64_t)iNewValue);
            break;
        }
        case HPET_STATUS + 4:
        {
            Log(("write HPET_STATUS + 4: %x\n", iNewValue));
            if (iNewValue != 0)
                LogRel(("Writing HPET_STATUS + 4 with non-zero, ignored\n"));
            break;
        }
        case HPET_COUNTER:
        {
            pThis->u64HpetCounter = (pThis->u64HpetCounter & 0xffffffff00000000ULL) | iNewValue;
            Log(("write HPET_COUNTER: %#x -> %llx\n",
                 iNewValue, pThis->u64HpetCounter));
            break;
        }
        case HPET_COUNTER + 4:
        {
            pThis->u64HpetCounter = (pThis->u64HpetCounter & 0xffffffffULL)
                    | (((uint64_t)iNewValue) << 32);
            Log(("write HPET_COUNTER + 4: %#x -> %llx\n",
                 iNewValue, pThis->u64HpetCounter));
            break;
        }
        default:
            LogRel(("invalid HPET config write: %x\n", iIndex));
            break;
    }

    return rc;
}

PDMBOTHCBDECL(int)  hpetMMIORead(PPDMDEVINS pDevIns,
                                 void *     pvUser,
                                 RTGCPHYS   GCPhysAddr,
                                 void *     pv,
                                 unsigned   cb)
{
    HpetState * pThis  = PDMINS_2_DATA(pDevIns, HpetState*);
    int         rc     = VINF_SUCCESS;
    uint32_t    iIndex = (uint32_t)(GCPhysAddr - HPET_BASE);

    LogFlow(("hpetMMIORead: %llx (%x)\n", (uint64_t)GCPhysAddr, iIndex));

    rc = hpetLock(pThis, VINF_IOM_HC_MMIO_READ);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
            Log(("Narrow read: %d\n", cb));
            rc = VERR_INTERNAL_ERROR;
            break;
        case 4:
        {
            if ((iIndex >= 0x100) && (iIndex < 0x400))
                rc = timerRegRead32(pThis, (iIndex - 0x100) / 0x20, (iIndex - 0x100) % 0x20, (uint32_t*)pv);
            else
                rc = configRegRead32(pThis, iIndex, (uint32_t*)pv);
            break;
        }
        case 8:
        {
            union {
                uint32_t u32[2];
                uint64_t u64;
            } value;

            /* Unaligned accesses not allowed */
            if (iIndex % 8 != 0)
            {
                AssertMsgFailed(("Unaligned HPET read access\n"));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
            // for 8-byte accesses we just split them, happens under lock anyway
            if ((iIndex >= 0x100) && (iIndex < 0x400))
            {
                uint32_t iTimer = (iIndex - 0x100) / 0x20;
                uint32_t iTimerReg = (iIndex - 0x100) % 0x20;

                rc = timerRegRead32(pThis, iTimer, iTimerReg, &value.u32[0]);
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                    break;
                rc = timerRegRead32(pThis, iTimer, iTimerReg + 4, &value.u32[1]);
            }
            else
            {
                rc = configRegRead32(pThis, iIndex, &value.u32[0]);
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                    break;
                rc = configRegRead32(pThis, iIndex+4, &value.u32[1]);
            }
            if (rc == VINF_SUCCESS)
                *(uint64_t*)pv = value.u64;
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VERR_INTERNAL_ERROR;
    }

    hpetUnlock(pThis);

    return rc;
}

PDMBOTHCBDECL(int) hpetMMIOWrite(PPDMDEVINS pDevIns,
                                 void  *    pvUser,
                                 RTGCPHYS   GCPhysAddr,
                                 void *     pv,
                                 unsigned   cb)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState*);
    int rc = VINF_SUCCESS;
    uint32_t iIndex = (uint32_t)(GCPhysAddr - HPET_BASE);

    LogFlow(("hpetMMIOWrite: %llx (%x) <- %x\n",
             (uint64_t)GCPhysAddr, iIndex, *(uint32_t*)pv));

    rc = hpetLock(pThis, VINF_IOM_HC_MMIO_WRITE);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
            Log(("Narrow write: %d\n", cb));
            rc = VERR_INTERNAL_ERROR;
            break;
        case 4:
        {
            if ((iIndex >= 0x100) && (iIndex < 0x400))
                rc = timerRegWrite32(pThis,
                                     (iIndex - 0x100) / 0x20,
                                     (iIndex - 0x100) % 0x20,
                                     *(uint32_t*)pv);
            else
                rc = configRegWrite32(pThis, iIndex, *(uint32_t*)pv);
            break;
        }
        case 8:
        {
            union {
                uint32_t u32[2];
                uint64_t u64;
            } value;

            /* Unaligned accesses not allowed */
            if (iIndex % 8 != 0)
            {
                AssertMsgFailed(("Unaligned HPET write access\n"));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
            value.u64 = *(uint64_t*)pv;
            // for 8-byte accesses we just split them, happens under lock anyway
            if ((iIndex >= 0x100) && (iIndex < 0x400))
            {
                uint32_t iTimer = (iIndex - 0x100) / 0x20;
                uint32_t iTimerReg = (iIndex - 0x100) % 0x20;

                rc = timerRegWrite32(pThis, iTimer, iTimerReg, value.u32[0]);
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                    break;
                rc = timerRegWrite32(pThis, iTimer, iTimerReg + 4, value.u32[1]);
            }
            else
            {
                rc = configRegWrite32(pThis, iIndex, value.u32[0]);
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                    break;
                rc = configRegWrite32(pThis, iIndex+4, value.u32[1]);
            }
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VERR_INTERNAL_ERROR;
    }

    hpetUnlock(pThis);

    return rc;
}

#ifdef IN_RING3

static int hpetSaveTimer(HpetTimer *pTimer,
                         PSSMHANDLE pSSM)
{
    TMR3TimerSave(pTimer->pTimerR3, pSSM);
    SSMR3PutU8  (pSSM,              pTimer->u8Wrap);
    SSMR3PutU64 (pSSM,              pTimer->u64Config);
    SSMR3PutU64 (pSSM,              pTimer->u64Cmp);
    SSMR3PutU64 (pSSM,              pTimer->u64Fsb);
    SSMR3PutU64 (pSSM,              pTimer->u64Period);

    return VINF_SUCCESS;
}

static int hpetLoadTimer(HpetTimer *pTimer,
                         PSSMHANDLE pSSM)
{
    TMR3TimerLoad(pTimer->pTimerR3, pSSM);
    SSMR3GetU8(pSSM,                &pTimer->u8Wrap);
    SSMR3GetU64(pSSM,               &pTimer->u64Config);
    SSMR3GetU64(pSSM,               &pTimer->u64Cmp);
    SSMR3GetU64(pSSM,               &pTimer->u64Fsb);
    SSMR3GetU64(pSSM,               &pTimer->u64Period);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) hpetLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);

    SSMR3PutU8(pSSM, HPET_NUM_TIMERS);

    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Saves a state of the HPET device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) hpetSaveExec(PPDMDEVINS pDevIns,
                                      PSSMHANDLE pSSM)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    uint32_t   iTimer;
    int        rc;

    /* The config. */
    hpetLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    for (iTimer = 0; iTimer < HPET_NUM_TIMERS; iTimer++)
    {
        rc = hpetSaveTimer(&pThis->aTimers[iTimer], pSSM);
        AssertRCReturn(rc, rc);
    }

    SSMR3PutU64(pSSM, pThis->u64HpetOffset);
    SSMR3PutU64(pSSM, pThis->u64Capabilities);
    SSMR3PutU64(pSSM, pThis->u64Config);
    SSMR3PutU64(pSSM, pThis->u64Isr);
    SSMR3PutU64(pSSM, pThis->u64HpetCounter);

    return VINF_SUCCESS;
}

/**
 * Loads a HPET device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) hpetLoadExec(PPDMDEVINS pDevIns,
                                      PSSMHANDLE pSSM,
                                      uint32_t   uVersion,
                                      uint32_t   uPass)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    uint32_t   iTimer;
    int        rc;

    if (uVersion == HPET_SAVED_STATE_VERSION_EMPTY)
        return VINF_SUCCESS;

    if (uVersion != HPET_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    uint8_t u8NumTimers;

    rc = SSMR3GetU8(pSSM, &u8NumTimers);          AssertRCReturn(rc, rc);
    if (u8NumTimers != HPET_NUM_TIMERS)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - wrong number of timers: saved=%#x config=%#x"), u8NumTimers, HPET_NUM_TIMERS);

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    for (iTimer = 0; iTimer < HPET_NUM_TIMERS; iTimer++)
    {
        rc = hpetLoadTimer(&pThis->aTimers[iTimer], pSSM);
        AssertRCReturn(rc, rc);
    }

    SSMR3GetU64(pSSM, &pThis->u64HpetOffset);
    SSMR3GetU64(pSSM, &pThis->u64Capabilities);
    SSMR3GetU64(pSSM, &pThis->u64Config);
    SSMR3GetU64(pSSM, &pThis->u64Isr);
    SSMR3GetU64(pSSM, &pThis->u64HpetCounter);

    return VINF_SUCCESS;
}

static void irqUpdate(struct HpetTimer *pTimer)
{
    uint32_t irq     = getTimerIrq(pTimer);
    HpetState* pThis = pTimer->CTX_SUFF(pHpet);

    /** @todo: is it correct? */
    if ((pTimer->u64Config & HPET_TN_ENABLE) &&
        (pThis->u64Config & HPET_CFG_ENABLE))
    {
        Log4(("HPET: raising IRQ %d\n", irq));

        /* ISR bits are only set in level-triggered mode */
        if ((pTimer->u64Config & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_LEVEL)
            pThis->u64Isr |= (uint64_t)(1 << pTimer->u8TimerNumber);

        /* We trigger flip/flop in edge-triggered mode and do nothing in level-triggered mode yet */
        if ((pTimer->u64Config & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_EDGE)
            pThis->pHpetHlpR3->pfnSetIrq(pThis->CTX_SUFF(pDevIns), irq, PDM_IRQ_LEVEL_FLIP_FLOP);
        /* @todo: implement IRQs in level-triggered mode */
    }
}

/**
 * Device timer callback function.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the HPET timer state.
 */
static DECLCALLBACK(void) hpetTimer(PPDMDEVINS pDevIns,
                                    PTMTIMER   pTmTimer,
                                    void *     pvUser)
{
    HpetState *pThis      = PDMINS_2_DATA(pDevIns, HpetState *);
    HpetTimer *pTimer     = (HpetTimer *)pvUser;
    uint64_t   u64Period  = pTimer->u64Period;
    uint64_t   u64CurTick = hpetGetTicks(pThis);
    uint64_t   u64Diff;
    int        rc;

    if (pTimer == NULL)
        return;

    /* Lock in R3 must either block or succeed */
    rc = hpetLock(pThis, VERR_IGNORED);

    AssertLogRelRCReturnVoid(rc);

    if ((pTimer->u64Config & HPET_TN_PERIODIC) && (u64Period != 0))
    {
        hpetAdjustComparator(pTimer, u64CurTick);

        u64Diff = hpetComputeDiff(pTimer, u64CurTick);

        Log4(("HPET: periodical: next in %lld\n",  hpetTicksToNs(u64Diff)));
        TMTimerSetNano(pTmTimer, hpetTicksToNs(u64Diff));
    }
    else if ((pTimer->u64Config & HPET_TN_32BIT) &&
             !(pTimer->u64Config & HPET_TN_PERIODIC))
    {
        if (pTimer->u8Wrap)
        {
            u64Diff = hpetComputeDiff(pTimer, u64CurTick);
            TMTimerSetNano(pTmTimer, hpetTicksToNs(u64Diff));
            pTimer->u8Wrap = 0;
        }
    }

    /* Should it really be under lock, does it really matter? */
    irqUpdate(pTimer);

    hpetUnlock(pThis);
}

/**
 * Relocation notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @param   offDelta    The delta relative to the old address.
 */
static DECLCALLBACK(void) hpetRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    unsigned i;
    LogFlow(("hpetRelocate:\n"));

    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pHpetHlpRC   = pThis->pHpetHlpR3->pfnGetRCHelpers(pDevIns);

    for (i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        HpetTimer *pTm = &pThis->aTimers[i];
        if (pTm->pTimerR3)
            pTm->pTimerRC = TMTimerRCPtr(pTm->pTimerR3);
        pTm->pHpetRC = PDMINS_2_DATA_RCPTR(pDevIns);
    }
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) hpetReset(PPDMDEVINS pDevIns)
{
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    unsigned i;

    LogFlow(("hpetReset:\n"));

    pThis->u64Config = 0;
    for (i = 0; i < HPET_NUM_TIMERS; i++)
    {
        HpetTimer *pTimer = &pThis->aTimers[i];
        pTimer->u8TimerNumber = i;
        pTimer->u64Cmp = ~0ULL;
        /* capable of periodic operations and 64-bits */
        pTimer->u64Config =  HPET_TN_PERIODIC_CAP | HPET_TN_SIZE_CAP;
        /* We can do all IRQs */
        uint32_t u32RoutingCap = 0xffffffff;
        pTimer->u64Config |=   ((uint64_t)u32RoutingCap) << 32;
        pTimer->u64Period = 0ULL;
        pTimer->u8Wrap = 0;
    }
    pThis->u64HpetCounter = 0ULL;
    pThis->u64HpetOffset = 0ULL;
    /* 64-bit main counter; 3 timers supported; LegacyReplacementRoute. */
    uint32_t u32Vendor = 0x8086;
    uint32_t u32Caps =
      (1 << 15) /* LEG_RT_CAP, LegacyReplacementRoute capable */ |
      (1 << 13) /* COUNTER_SIZE_CAP, main counter is 64-bit capable */ |
      ((HPET_NUM_TIMERS-1) << 8) /* NUM_TIM_CAP, number of timers -1 */ |
      1 /* REV_ID, revision, must not be 0 */;
    pThis->u64Capabilities = (u32Vendor << 16) | u32Caps;
    pThis->u64Capabilities |= ((uint64_t)(HPET_CLK_PERIOD) << 32);
}

/**
 * Initialization routine.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static int hpetInit(PPDMDEVINS pDevIns)
{
    unsigned   i;
    int        rc;
    HpetState *pThis = PDMINS_2_DATA(pDevIns, HpetState *);

    memset(pThis, 0, sizeof(*pThis));

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0  = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC  = PDMDEVINS_2_RCPTR(pDevIns);

    for (i = 0; i < HPET_NUM_TIMERS; i++)
    {
        HpetTimer *timer = &pThis->aTimers[i];

        timer->pHpetR3 = pThis;
        timer->pHpetR0 = PDMINS_2_DATA_R0PTR(pDevIns);
        timer->pHpetRC = PDMINS_2_DATA_RCPTR(pDevIns);

        rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, hpetTimer, timer,
                                    TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "HPET Timer",
                                    &pThis->aTimers[i].pTimerR3);
        if (RT_FAILURE(rc))
            return rc;
        pThis->aTimers[i].pTimerRC = TMTimerRCPtr(pThis->aTimers[i].pTimerR3);
        pThis->aTimers[i].pTimerR0 = TMTimerR0Ptr(pThis->aTimers[i].pTimerR3);
    }

    hpetReset(pDevIns);

    return VINF_SUCCESS;
}

/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) hpetInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    HpetState   *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    int i;

    pHlp->pfnPrintf(pHlp,
                    "HPET status:\n"
                    " config = %016RX64\n"
                    " offset = %016RX64 counter = %016RX64 isr = %016RX64\n"
                    " legacy mode is %s\n",
                    pThis->u64Config,
                    pThis->u64HpetOffset, pThis->u64HpetCounter, pThis->u64Isr,
                    (pThis->u64Config & HPET_CFG_LEGACY) ? "on" : "off");
    pHlp->pfnPrintf(pHlp,
                    "Timers:\n");
    for (i = 0; i < HPET_NUM_TIMERS; i++)
    {
        pHlp->pfnPrintf(pHlp, " %d: comparator=%016RX64 period(hidden)=%016RX64 cfg=%016RX64\n",
                        pThis->aTimers[i].u8TimerNumber,
                        pThis->aTimers[i].u64Cmp,
                        pThis->aTimers[i].u64Period,
                        pThis->aTimers[i].u64Config);
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hpetConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    HpetState   *pThis = PDMINS_2_DATA(pDevIns, HpetState *);
    int          rc;
    bool         fRCEnabled = false;
    bool         fR0Enabled = false;
    PDMHPETREG   HpetReg;

    /* Only one HPET device now */
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* Query configuration. */
#if 0
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fRCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"GCEnabled\" as a bool failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));
#endif
    /* Initialize the device state */
    rc = hpetInit(pDevIns);
    if (RT_FAILURE(rc))
        return rc;

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /*
     * Register the HPET and get helpers.
     */
    HpetReg.u32Version  = PDM_HPETREG_VERSION;
    rc = PDMDevHlpHPETRegister(pDevIns, &HpetReg, &pThis->pHpetHlpR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgRC(rc, ("Cannot HPETRegister: %Rrc\n", rc));
        return rc;
    }

    /*
     * Initialize critical section.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->csLock, RT_SRC_POS, "HPET");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("HPET cannot initialize critical section"));

    /*
     * Register the MMIO range, PDM API requests page aligned
     * addresses and sizes.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, HPET_BASE, 0x1000, pThis,
                               hpetMMIOWrite, hpetMMIORead, NULL, "HPET Memory");
    if (RT_FAILURE(rc))
    {
        AssertMsgRC(rc, ("Cannot register MMIO: %Rrc\n", rc));
        return rc;
    }

    if (fRCEnabled)
    {
        rc = PDMDevHlpMMIORegisterRC(pDevIns, HPET_BASE, 0x1000, 0,
                                     "hpetMMIOWrite", "hpetMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;

        pThis->pHpetHlpRC = pThis->pHpetHlpR3->pfnGetRCHelpers(pDevIns);
        if (!pThis->pHpetHlpRC)
        {
            AssertReleaseMsgFailed(("cannot get RC helper\n"));
            return VERR_INTERNAL_ERROR;
        }
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, HPET_BASE, 0x1000, 0,
                                     "hpetMMIOWrite", "hpetMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;

        pThis->pHpetHlpR0 = pThis->pHpetHlpR3->pfnGetR0Helpers(pDevIns);
        if (!pThis->pHpetHlpR0)
        {
            AssertReleaseMsgFailed(("cannot get R0 helper\n"));
            return VERR_INTERNAL_ERROR;
        }
    }

    /* Register SSM callbacks */
    rc = PDMDevHlpSSMRegister3(pDevIns, HPET_SAVED_STATE_VERSION, sizeof(*pThis), hpetLiveExec, hpetSaveExec, hpetLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /**
     * @todo Register statistics.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "hpet", "Display HPET status. (no arguments)", hpetInfo);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceHPET =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "hpet",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    " High Precision Event Timer (HPET) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PIT,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(HpetState),
    /* pfnConstruct */
    hpetConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    hpetRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    hpetReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
