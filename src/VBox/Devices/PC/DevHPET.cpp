/* $Id$ */
/** @file
 * HPET virtual device - High Precision Event Timer emulation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* This implementation is based on the (generic) Intel IA-PC HPET specification
 * and the Intel ICH9 datasheet.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HPET
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/stam.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm-math.h>
#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/*
 * Current limitations:
 *   - not entirely correct time of interrupt, i.e. never
 *     schedule interrupt earlier than in 1ms
 *   - statistics not implemented
 *   - level-triggered mode not implemented
 */

/** Base address for MMIO.
 * On ICH9, it is 0xFED0x000 where 'x' is 0-3, default 0. We do not support
 * relocation as the platform firmware is responsible for configuring the
 * HPET base address and the OS isn't expected to move it.
 * WARNING: This has to match the ACPI tables! */
#define HPET_BASE                   0xfed00000

/** HPET reserves a 1K range. */
#define HPET_BAR_SIZE               0x1000

/** The number of timers for PIIX4 / PIIX3. */
#define HPET_NUM_TIMERS_PIIX        3   /* Minimal implementation. */
/** The number of timers for ICH9. */
#define HPET_NUM_TIMERS_ICH9        4

/** HPET clock period for PIIX4 / PIIX3.
 * 10000000 femtoseconds == 10ns.
 */
#define HPET_CLK_PERIOD_PIIX        UINT32_C(10000000)

/** HPET clock period for ICH9.
 * 69841279 femtoseconds == 69.84 ns (1 / 14.31818MHz).
 */
#define HPET_CLK_PERIOD_ICH9        UINT32_C(69841279)

/**
 * Femtosecods in a nanosecond
 */
#define FS_PER_NS                   1000000

/**
 * Femtoseconds in a day. Still fits within int64_t.
 */
#define FS_PER_DAY                  (1000000LL * 60 * 60 * 24 * FS_PER_NS)

/**
 * Number of HPET ticks in 100 years, ICH9 frequency.
 */
#define HPET_TICKS_IN_100YR_ICH9    (FS_PER_DAY / HPET_CLK_PERIOD_ICH9 * 365 * 100)

/**
 * Number of HPET ticks in 100 years, made-up PIIX frequency.
 */
#define HPET_TICKS_IN_100YR_PIIX    (FS_PER_DAY / HPET_CLK_PERIOD_PIIX * 365 * 100)

/** @name Interrupt type
 * @{ */
#define HPET_TIMER_TYPE_LEVEL       (1 << 1)
#define HPET_TIMER_TYPE_EDGE        (0 << 1)
/** @} */

/** @name Delivery mode
 * @{ */
#define HPET_TIMER_DELIVERY_APIC    0   /**< Delivery through APIC. */
#define HPET_TIMER_DELIVERY_FSB     1   /**< Delivery through FSB. */
/** @} */

#define HPET_TIMER_CAP_FSB_INT_DEL (1 << 15)
#define HPET_TIMER_CAP_PER_INT     (1 << 4)

#define HPET_CFG_ENABLE          0x001  /**< ENABLE_CNF */
#define HPET_CFG_LEGACY          0x002  /**< LEG_RT_CNF */

/** @name Register offsets in HPET space.
 * @{ */
#define HPET_ID                  0x000  /**< Device ID. */
#define HPET_PERIOD              0x004  /**< Clock period in femtoseconds. */
#define HPET_CFG                 0x010  /**< Configuration register. */
#define HPET_STATUS              0x020  /**< Status register. */
#define HPET_COUNTER             0x0f0  /**< Main HPET counter. */
/** @} */

/** @name Timer N offsets (within each timer's space).
 * @{ */
#define HPET_TN_CFG              0x000  /**< Timer N configuration. */
#define HPET_TN_CMP              0x008  /**< Timer N comparator. */
#define HPET_TN_ROUTE            0x010  /**< Timer N interrupt route. */
/** @} */

#define HPET_CFG_WRITE_MASK      0x3

#define HPET_TN_INT_TYPE                RT_BIT_64(1)
#define HPET_TN_ENABLE                  RT_BIT_64(2)
#define HPET_TN_PERIODIC                RT_BIT_64(3)
#define HPET_TN_PERIODIC_CAP            RT_BIT_64(4)
#define HPET_TN_SIZE_CAP                RT_BIT_64(5)
#define HPET_TN_SETVAL                  RT_BIT_64(6)
#define HPET_TN_32BIT                   RT_BIT_64(8)
#define HPET_TN_INT_ROUTE_MASK          UINT64_C(0x3e00)
#define HPET_TN_CFG_WRITE_MASK          UINT64_C(0x3e46)
#define HPET_TN_INT_ROUTE_SHIFT         9
#define HPET_TN_INT_ROUTE_CAP_SHIFT     32

#define HPET_TN_CFG_BITS_READONLY_OR_RESERVED 0xffff80b1U

/** Extract the timer count from the capabilities. */
#define HPET_CAP_GET_TIMERS(a_u32)      ( ((a_u32) >> 8) & 0x1f )

/** The version of the saved state. */
#define HPET_SAVED_STATE_VERSION       2
/** Empty saved state */
#define HPET_SAVED_STATE_VERSION_EMPTY 1


/**
 * Acquires the HPET lock or returns.
 */
#define DEVHPET_LOCK_RETURN(a_pDevIns, a_pThis, a_rcBusy)  \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, (a_rcBusy)); \
        if (RT_LIKELY(rcLock == VINF_SUCCESS)) \
        { /* likely */ } \
        else \
            return rcLock; \
    } while (0)

/**
 * Releases the HPET lock.
 */
#define DEVHPET_UNLOCK(a_pDevIns, a_pThis) \
    do { PDMDevHlpCritSectLeave((a_pDevIns), &(a_pThis)->CritSect); } while (0)


/**
 * Acquires the TM lock and HPET lock, returns on failure.
 * @todo r=bird: Aren't the timers using the same critsect?!?
 */
#define DEVHPET_LOCK_BOTH_RETURN(a_pDevIns, a_pThis, a_rcBusy)  \
    do { \
        VBOXSTRICTRC rcLock = PDMDevHlpTimerLockClock2((a_pDevIns), (a_pThis)->aTimers[0].hTimer, &(a_pThis)->CritSect, (a_rcBusy)); \
        if (RT_LIKELY(rcLock == VINF_SUCCESS)) \
        { /* likely */ } \
        else \
            return rcLock; \
    } while (0)


/**
 * Releases the HPET lock and TM lock.
 */
#define DEVHPET_UNLOCK_BOTH(a_pDevIns, a_pThis) \
        PDMDevHlpTimerUnlockClock2((a_pDevIns), (a_pThis)->aTimers[0].hTimer, &(a_pThis)->CritSect)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * A HPET timer.
 */
typedef struct HPETTIMER
{
    /** The HPET timer. */
    TMTIMERHANDLE               hTimer;

    /** Timer index. */
    uint8_t                     idxTimer;
    /** Wrap. */
    uint8_t                     u8Wrap;
    /** Explicit padding. */
    uint8_t                     abPadding[6];

    /** @name Memory-mapped, software visible timer registers.
     * @{ */
    /** Configuration/capabilities. */
    uint64_t                    u64Config;
    /** Comparator. */
    uint64_t                    u64Cmp;
    /** FSB route, not supported now. */
    uint64_t                    u64Fsb;
    /** @} */

    /** @name Hidden register state.
     * @{ */
    /** Last value written to comparator. */
    uint64_t                    u64Period;
    /** @} */

    STAMCOUNTER                 StatSetIrq;
    STAMCOUNTER                 StatSetTimer;
} HPETTIMER;
AssertCompileMemberAlignment(HPETTIMER, u64Config, sizeof(uint64_t));
/** Pointer to the shared state of an HPET timer. */
typedef HPETTIMER *PHPETTIMER;
/** Const pointer to the shared state of an HPET timer. */
typedef HPETTIMER const *PCHPETTIMER;


/**
 * The shared HPET device state.
 */
typedef struct HPET
{
    /** Timer structures. */
    HPETTIMER                   aTimers[RT_MAX(HPET_NUM_TIMERS_PIIX, HPET_NUM_TIMERS_ICH9)];

    /** Offset realtive to the virtual sync clock. */
    uint64_t                    u64HpetOffset;

    /** @name Memory-mapped, software visible registers
     * @{ */
    /** Capabilities. */
    uint32_t                    u32Capabilities;
    /** HPET_PERIOD - . */
    uint32_t                    u32Period;
    /** Configuration. */
    uint64_t                    u64HpetConfig;
    /** Interrupt status register. */
    uint64_t                    u64Isr;
    /** Main counter. */
    uint64_t                    u64HpetCounter;
    /** @}  */

    /** Whether we emulate ICH9 HPET (different frequency & timer count). */
    bool                        fIch9;
    /** Size alignment padding. */
    uint8_t                     abPadding0[7];

    /** Global device lock. */
    PDMCRITSECT                 CritSect;

    /** The handle of the MMIO region. */
    IOMMMIOHANDLE               hMmio;

    STAMCOUNTER                 StatCounterRead4Byte;
    STAMCOUNTER                 StatCounterRead8Byte;
    STAMCOUNTER                 StatCounterWriteLow;
    STAMCOUNTER                 StatCounterWriteHigh;
} HPET;
/** Pointer to the shared HPET device state. */
typedef HPET *PHPET;
/** Const pointer to the shared HPET device state. */
typedef const HPET *PCHPET;


/**
 * The ring-3 specific HPET device state.
 */
typedef struct HPETR3
{
    /** The HPET helpers. */
    PCPDMHPETHLPR3              pHpetHlp;
} HPETR3;
/** Pointer to the ring-3 specific HPET device state. */
typedef HPETR3 *PHPETR3;


/**
 * The ring-0 specific HPET device state.
 */
typedef struct HPETR0
{
    /** The HPET helpers. */
    PCPDMHPETHLPR0              pHpetHlp;
} HPETR0;
/** Pointer to the ring-0 specific HPET device state. */
typedef HPETR0 *PHPETR0;


/**
 * The raw-mode specific HPET device state.
 */
typedef struct HPETRC
{
    /** The HPET helpers. */
    PCPDMHPETHLPRC              pHpetHlp;
} HPETRC;
/** Pointer to the raw-mode specific HPET device state. */
typedef HPETRC *PHPETRC;


/** The HPET device state specific to the current context. */
typedef CTX_SUFF(HPET) HPETCC;
/** Pointer to the HPET device state specific to the current context. */
typedef CTX_SUFF(PHPET) PHPETCC;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


DECLINLINE(bool) hpet32bitTimer(PHPETTIMER pHpetTimer)
{
    uint64_t u64Cfg = pHpetTimer->u64Config;
    return ((u64Cfg & HPET_TN_SIZE_CAP) == 0) || ((u64Cfg & HPET_TN_32BIT) != 0);
}

DECLINLINE(uint64_t) hpetInvalidValue(PHPETTIMER pHpetTimer)
{
    return hpet32bitTimer(pHpetTimer) ? UINT32_MAX : UINT64_MAX;
}

DECLINLINE(uint64_t) hpetTicksToNs(PHPET pThis, uint64_t value)
{
    return ASMMultU64ByU32DivByU32(value,  pThis->u32Period, FS_PER_NS);
}

DECLINLINE(uint64_t) nsToHpetTicks(PCHPET pThis, uint64_t u64Value)
{
    return ASMMultU64ByU32DivByU32(u64Value, FS_PER_NS, RT_MAX(pThis->u32Period, 1 /* no div/zero */));
}

DECLINLINE(uint64_t) hpetGetTicks(PPDMDEVINS pDevIns, PCHPET pThis)
{
    /*
     * We can use any timer to get current time, they all go with the same speed.
     */
    return nsToHpetTicks(pThis, PDMDevHlpTimerGet(pDevIns, pThis->aTimers[0].hTimer) + pThis->u64HpetOffset);
}

DECLINLINE(uint64_t) hpetUpdateMasked(uint64_t u64NewValue, uint64_t u64OldValue, uint64_t u64Mask)
{
    u64NewValue &= u64Mask;
    u64NewValue |= (u64OldValue & ~u64Mask);
    return u64NewValue;
}

DECLINLINE(bool) hpetBitJustSet(uint64_t u64OldValue, uint64_t u64NewValue, uint64_t u64Mask)
{
    return !(u64OldValue & u64Mask)
        && !!(u64NewValue & u64Mask);
}

DECLINLINE(bool) hpetBitJustCleared(uint64_t u64OldValue, uint64_t u64NewValue, uint64_t u64Mask)
{
    return !!(u64OldValue & u64Mask)
        && !(u64NewValue & u64Mask);
}

DECLINLINE(uint64_t) hpetComputeDiff(PHPETTIMER pHpetTimer, uint64_t u64Now)
{

    if (hpet32bitTimer(pHpetTimer))
    {
        uint32_t u32Diff;

        u32Diff = (uint32_t)pHpetTimer->u64Cmp - (uint32_t)u64Now;
        u32Diff = (int32_t)u32Diff > 0 ? u32Diff : (uint32_t)0;
        return (uint64_t)u32Diff;
    }
    uint64_t u64Diff;

    u64Diff = pHpetTimer->u64Cmp - u64Now;
    u64Diff = (int64_t)u64Diff > 0 ?  u64Diff : (uint64_t)0;
    return u64Diff;
}


static void hpetAdjustComparator(PHPETTIMER pHpetTimer, uint64_t u64Now)
{
    if ((pHpetTimer->u64Config & HPET_TN_PERIODIC))
    {
        uint64_t u64Period = pHpetTimer->u64Period;
        if (u64Period)
        {
            uint64_t  cPeriods = (u64Now - pHpetTimer->u64Cmp) / u64Period;
            pHpetTimer->u64Cmp += (cPeriods + 1) * u64Period;
        }
    }
}


/**
 * Sets the frequency hint if it's a periodic timer.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared HPET state.
 * @param   pHpetTimer  The timer.
 */
DECLINLINE(void) hpetTimerSetFrequencyHint(PPDMDEVINS pDevIns, PHPET pThis, PHPETTIMER pHpetTimer)
{
    if (pHpetTimer->u64Config & HPET_TN_PERIODIC)
    {
        uint64_t const u64Period = pHpetTimer->u64Period;
        uint32_t const u32Freq   = pThis->u32Period;
        if (u64Period > 0 && u64Period < u32Freq)
            PDMDevHlpTimerSetFrequencyHint(pDevIns, pHpetTimer->hTimer, u32Freq / (uint32_t)u64Period);
    }
}


static void hpetProgramTimer(PPDMDEVINS pDevIns, PHPET pThis, PHPETTIMER pHpetTimer)
{
    /* no wrapping on new timers */
    pHpetTimer->u8Wrap = 0;

    uint64_t u64Ticks = hpetGetTicks(pDevIns, pThis);
    hpetAdjustComparator(pHpetTimer, u64Ticks);

    uint64_t u64Diff = hpetComputeDiff(pHpetTimer, u64Ticks);

    /*
     * HPET spec says in one-shot 32-bit mode, generate an interrupt when
     * counter wraps in addition to an interrupt with comparator match.
     */
    if (    hpet32bitTimer(pHpetTimer)
        && !(pHpetTimer->u64Config & HPET_TN_PERIODIC))
    {
        uint32_t u32TillWrap = 0xffffffff - (uint32_t)u64Ticks + 1;
        if (u32TillWrap < (uint32_t)u64Diff)
        {
            Log(("wrap on timer %d: till=%u ticks=%lld diff64=%lld\n",
                 pHpetTimer->idxTimer, u32TillWrap, u64Ticks, u64Diff));
            u64Diff = u32TillWrap;
            pHpetTimer->u8Wrap = 1;
        }
    }

    /*
     * HACK ALERT! Avoid killing VM with interrupts.
     */
#if 1 /** @todo HACK, rethink, may have negative impact on the guest */
    if (u64Diff == 0)
        u64Diff = 100000; /* 1 millisecond */
#endif

    uint64_t u64TickLimit = pThis->fIch9 ? HPET_TICKS_IN_100YR_ICH9 : HPET_TICKS_IN_100YR_PIIX;
    if (u64Diff <= u64TickLimit)
    {
        Log4(("HPET: next IRQ in %lld ticks (%lld ns)\n", u64Diff, hpetTicksToNs(pThis, u64Diff)));
        STAM_REL_COUNTER_INC(&pHpetTimer->StatSetTimer);
        PDMDevHlpTimerSetNano(pDevIns, pHpetTimer->hTimer, hpetTicksToNs(pThis, u64Diff));
    }
    else
    {
        LogRelMax(10, ("HPET: Not scheduling an interrupt more than 100 years in the future.\n"));
    }
    hpetTimerSetFrequencyHint(pDevIns, pThis, pHpetTimer);
}


/* -=-=-=-=-=- Timer register accesses -=-=-=-=-=- */


/**
 * Reads a HPET timer register.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The HPET instance.
 * @param   iTimerNo            The timer index.
 * @param   iTimerReg           The index of the timer register to read.
 * @param   pu32Value           Where to return the register value.
 *
 * @remarks ASSUMES the caller holds the HPET lock.
 */
static void hpetTimerRegRead32(PPDMDEVINS pDevIns, PCHPET pThis, uint32_t iTimerNo, uint32_t iTimerReg, uint32_t *pu32Value)
{
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    RT_NOREF(pDevIns);

    uint32_t u32Value;
    if (   iTimerNo < HPET_CAP_GET_TIMERS(pThis->u32Capabilities)
        && iTimerNo < RT_ELEMENTS(pThis->aTimers) )
    {
        PCHPETTIMER pHpetTimer = &pThis->aTimers[iTimerNo];
        switch (iTimerReg)
        {
            case HPET_TN_CFG:
                u32Value = (uint32_t)pHpetTimer->u64Config;
                Log(("read HPET_TN_CFG on %d: %#x\n", iTimerNo, u32Value));
                break;

            case HPET_TN_CFG + 4:
                u32Value = (uint32_t)(pHpetTimer->u64Config >> 32);
                Log(("read HPET_TN_CFG+4 on %d: %#x\n", iTimerNo, u32Value));
                break;

            case HPET_TN_CMP:
                u32Value = (uint32_t)pHpetTimer->u64Cmp;
                Log(("read HPET_TN_CMP on %d: %#x (%#llx)\n", pHpetTimer->idxTimer, u32Value, pHpetTimer->u64Cmp));
                break;

            case HPET_TN_CMP + 4:
                u32Value = (uint32_t)(pHpetTimer->u64Cmp >> 32);
                Log(("read HPET_TN_CMP+4 on %d: %#x (%#llx)\n", pHpetTimer->idxTimer, u32Value, pHpetTimer->u64Cmp));
                break;

            case HPET_TN_ROUTE:
                u32Value = (uint32_t)(pHpetTimer->u64Fsb >> 32); /** @todo Looks wrong, but since it's not supported, who cares. */
                Log(("read HPET_TN_ROUTE on %d: %#x\n", iTimerNo, u32Value));
                break;

            default:
            {
                LogRelMax(10, ("HPET: Invalid HPET register read %d on %d\n", iTimerReg, pHpetTimer->idxTimer));
                u32Value = 0;
                break;
            }
        }
    }
    else
    {
        LogRelMax(10, ("HPET: Using timer above configured range: %d\n", iTimerNo));
        u32Value = 0;
    }
    *pu32Value = u32Value;
}


/**
 * 32-bit write to a HPET timer register.
 *
 * @returns Strict VBox status code.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared HPET state.
 * @param   iTimerNo        The timer being written to.
 * @param   iTimerReg       The register being written to.
 * @param   u32NewValue     The value being written.
 *
 * @remarks The caller should not hold the device lock, unless it also holds
 *          the TM lock.
 */
static VBOXSTRICTRC hpetTimerRegWrite32(PPDMDEVINS pDevIns, PHPET pThis, uint32_t iTimerNo,
                                        uint32_t iTimerReg, uint32_t u32NewValue)
{
    Assert(!PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect) || PDMDevHlpTimerIsLockOwner(pDevIns, pThis->aTimers[0].hTimer));

    if (   iTimerNo >= HPET_CAP_GET_TIMERS(pThis->u32Capabilities)
        || iTimerNo >= RT_ELEMENTS(pThis->aTimers) )    /* Parfait - see above. */
    {
        LogRelMax(10, ("HPET: Using timer above configured range: %d\n", iTimerNo));
        return VINF_SUCCESS;
    }
    PHPETTIMER pHpetTimer = &pThis->aTimers[iTimerNo];

    switch (iTimerReg)
    {
        case HPET_TN_CFG:
        {
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            uint64_t    u64Mask = HPET_TN_CFG_WRITE_MASK;

            Log(("write HPET_TN_CFG: %d: %x\n", iTimerNo, u32NewValue));
            if (pHpetTimer->u64Config & HPET_TN_PERIODIC_CAP)
                u64Mask |= HPET_TN_PERIODIC;

            if (pHpetTimer->u64Config & HPET_TN_SIZE_CAP)
                u64Mask |= HPET_TN_32BIT;
            else
                u32NewValue &= ~HPET_TN_32BIT;

            if (u32NewValue & HPET_TN_32BIT)
            {
                Log(("setting timer %d to 32-bit mode\n", iTimerNo));
                pHpetTimer->u64Cmp    = (uint32_t)pHpetTimer->u64Cmp;
                pHpetTimer->u64Period = (uint32_t)pHpetTimer->u64Period;
            }
            if ((u32NewValue & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_LEVEL)
            {
                LogRelMax(10, ("HPET: Level-triggered config not yet supported\n"));
                AssertFailed();
            }

            /* We only care about lower 32-bits so far */
            pHpetTimer->u64Config = hpetUpdateMasked(u32NewValue, pHpetTimer->u64Config, u64Mask);
            DEVHPET_UNLOCK(pDevIns, pThis);
            break;
        }

        case HPET_TN_CFG + 4: /* Interrupt capabilities - read only. */
            Log(("write HPET_TN_CFG + 4, useless\n"));
            break;

        case HPET_TN_CMP: /* lower bits of comparator register */
        {
            DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            Log(("write HPET_TN_CMP on %d: %#x\n", iTimerNo, u32NewValue));

            if (pHpetTimer->u64Config & HPET_TN_PERIODIC)
                pHpetTimer->u64Period = RT_MAKE_U64(u32NewValue, RT_HI_U32(pHpetTimer->u64Period));
            pHpetTimer->u64Cmp     = RT_MAKE_U64(u32NewValue, RT_HI_U32(pHpetTimer->u64Cmp));
            pHpetTimer->u64Config &= ~HPET_TN_SETVAL;
            Log2(("after HPET_TN_CMP cmp=%#llx per=%#llx\n", pHpetTimer->u64Cmp, pHpetTimer->u64Period));

            if (pThis->u64HpetConfig & HPET_CFG_ENABLE)
                hpetProgramTimer(pDevIns, pThis, pHpetTimer);
            DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
            break;
        }

        case HPET_TN_CMP + 4: /* upper bits of comparator register */
        {
            DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            Log(("write HPET_TN_CMP + 4 on %d: %#x\n", iTimerNo, u32NewValue));
            if (!hpet32bitTimer(pHpetTimer))
            {
                if (pHpetTimer->u64Config & HPET_TN_PERIODIC)
                    pHpetTimer->u64Period = RT_MAKE_U64(RT_LO_U32(pHpetTimer->u64Period), u32NewValue);
                pHpetTimer->u64Cmp = RT_MAKE_U64(RT_LO_U32(pHpetTimer->u64Cmp), u32NewValue);

                Log2(("after HPET_TN_CMP+4 cmp=%llx per=%llx tmr=%d\n", pHpetTimer->u64Cmp, pHpetTimer->u64Period, iTimerNo));

                pHpetTimer->u64Config &= ~HPET_TN_SETVAL;

                if (pThis->u64HpetConfig & HPET_CFG_ENABLE)
                    hpetProgramTimer(pDevIns, pThis, pHpetTimer);
            }
            DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
            break;
        }

        case HPET_TN_ROUTE:
            Log(("write HPET_TN_ROUTE\n"));
            break;

        case HPET_TN_ROUTE + 4:
            Log(("write HPET_TN_ROUTE + 4\n"));
            break;

        default:
            LogRelMax(10, ("HPET: Invalid timer register write: %d\n", iTimerReg));
            break;
    }

    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Non-timer register accesses -=-=-=-=-=- */


/**
 * Read a 32-bit HPET register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HPET state.
 * @param   idxReg              The register to read.
 * @param   pu32Value           Where to return the register value.
 *
 * @remarks The caller must not own the device lock if HPET_COUNTER is read.
 */
static VBOXSTRICTRC hpetConfigRegRead32(PPDMDEVINS pDevIns, PHPET pThis, uint32_t idxReg, uint32_t *pu32Value)
{
    Assert(!PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect) || (idxReg != HPET_COUNTER && idxReg != HPET_COUNTER + 4));

    uint32_t u32Value;
    switch (idxReg)
    {
        case HPET_ID:
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            u32Value = pThis->u32Capabilities;
            DEVHPET_UNLOCK(pDevIns, pThis);
            Log(("read HPET_ID: %#x\n", u32Value));
            break;

        case HPET_PERIOD:
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            u32Value = pThis->u32Period;
            DEVHPET_UNLOCK(pDevIns, pThis);
            Log(("read HPET_PERIOD: %#x\n", u32Value));
            break;

        case HPET_CFG:
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            u32Value = (uint32_t)pThis->u64HpetConfig;
            DEVHPET_UNLOCK(pDevIns, pThis);
            Log(("read HPET_CFG: %#x\n", u32Value));
            break;

        case HPET_CFG + 4:
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            u32Value = (uint32_t)(pThis->u64HpetConfig >> 32);
            DEVHPET_UNLOCK(pDevIns, pThis);
            Log(("read of HPET_CFG + 4: %#x\n", u32Value));
            break;

        case HPET_COUNTER:
        case HPET_COUNTER + 4:
        {
            STAM_REL_COUNTER_INC(&pThis->StatCounterRead4Byte);
            DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);

            uint64_t u64Ticks;
            if (pThis->u64HpetConfig & HPET_CFG_ENABLE)
                u64Ticks = hpetGetTicks(pDevIns, pThis);
            else
                u64Ticks = pThis->u64HpetCounter;

            DEVHPET_UNLOCK_BOTH(pDevIns, pThis);

            /** @todo is it correct? */
            u32Value = (idxReg == HPET_COUNTER) ? (uint32_t)u64Ticks : (uint32_t)(u64Ticks >> 32);
            Log(("read HPET_COUNTER: %s part value %x (%#llx)\n", (idxReg == HPET_COUNTER) ? "low" : "high", u32Value, u64Ticks));
            break;
        }

        case HPET_STATUS:
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            u32Value = (uint32_t)pThis->u64Isr;
            DEVHPET_UNLOCK(pDevIns, pThis);
            Log(("read HPET_STATUS: %#x\n", u32Value));
            break;

        default:
            Log(("invalid HPET register read: %x\n", idxReg));
            u32Value = 0;
            break;
    }

    *pu32Value = u32Value;
    return VINF_SUCCESS;
}


/**
 * 32-bit write to a config register.
 *
 * @returns Strict VBox status code.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared HPET state.
 * @param   idxReg          The register being written to.
 * @param   u32NewValue     The value being written.
 *
 * @remarks The caller should not hold the device lock, unless it also holds
 *          the TM lock.
 */
static VBOXSTRICTRC hpetConfigRegWrite32(PPDMDEVINS pDevIns, PHPET pThis, uint32_t idxReg, uint32_t u32NewValue)
{
    Assert(!PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect) || PDMDevHlpTimerIsLockOwner(pDevIns, pThis->aTimers[0].hTimer));

    VBOXSTRICTRC rc = VINF_SUCCESS;
    switch (idxReg)
    {
        case HPET_ID:
        case HPET_ID + 4:
        {
            Log(("write HPET_ID, useless\n"));
            break;
        }

        case HPET_CFG:
        {
            DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            uint32_t const iOldValue = (uint32_t)(pThis->u64HpetConfig);
            Log(("write HPET_CFG: %x (old %x)\n", u32NewValue, iOldValue));

            /*
             * This check must be here, before actual update, as hpetLegacyMode
             * may request retry in R3 - so we must keep state intact.
             */
            if ((iOldValue ^ u32NewValue) & HPET_CFG_LEGACY)
            {
#ifdef IN_RING3
                PHPETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHPETCC);
                if (pThisCC->pHpetHlp != NULL)
                {
                    rc = pThisCC->pHpetHlp->pfnSetLegacyMode(pDevIns, RT_BOOL(u32NewValue & HPET_CFG_LEGACY));
                    if (rc != VINF_SUCCESS)
                    {
                        DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
                        break;
                    }
                }
#else
                rc = VINF_IOM_R3_MMIO_WRITE;
                DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
                break;
#endif
            }

            pThis->u64HpetConfig = hpetUpdateMasked(u32NewValue, iOldValue, HPET_CFG_WRITE_MASK);

            uint32_t const cTimers = HPET_CAP_GET_TIMERS(pThis->u32Capabilities);
            if (hpetBitJustSet(iOldValue, u32NewValue, HPET_CFG_ENABLE))
            {
/** @todo Only get the time stamp once when reprogramming? */
                /* Enable main counter and interrupt generation. */
                uint64_t u64TickLimit = pThis->fIch9 ? HPET_TICKS_IN_100YR_ICH9 : HPET_TICKS_IN_100YR_PIIX;
                if (pThis->u64HpetCounter <= u64TickLimit)
                {
                    pThis->u64HpetOffset = hpetTicksToNs(pThis, pThis->u64HpetCounter)
                                         - PDMDevHlpTimerGet(pDevIns, pThis->aTimers[0].hTimer);
                }
                else
                {
                    LogRelMax(10, ("HPET: Counter set more than 100 years in the future, reducing.\n"));
                    pThis->u64HpetOffset = 1000000LL * 60 * 60 * 24 * 365 * 100
                                         - PDMDevHlpTimerGet(pDevIns, pThis->aTimers[0].hTimer);
                }
                for (uint32_t i = 0; i < cTimers; i++)
                    if (pThis->aTimers[i].u64Cmp != hpetInvalidValue(&pThis->aTimers[i]))
                        hpetProgramTimer(pDevIns, pThis, &pThis->aTimers[i]);
            }
            else if (hpetBitJustCleared(iOldValue, u32NewValue, HPET_CFG_ENABLE))
            {
                /* Halt main counter and disable interrupt generation. */
                pThis->u64HpetCounter = hpetGetTicks(pDevIns, pThis);
                for (uint32_t i = 0; i < cTimers; i++)
                    PDMDevHlpTimerStop(pDevIns, pThis->aTimers[i].hTimer);
            }

            DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
            break;
        }

        case HPET_CFG + 4:
        {
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            pThis->u64HpetConfig = hpetUpdateMasked((uint64_t)u32NewValue << 32,
                                                    pThis->u64HpetConfig,
                                                    UINT64_C(0xffffffff00000000));
            Log(("write HPET_CFG + 4: %x -> %#llx\n", u32NewValue, pThis->u64HpetConfig));
            DEVHPET_UNLOCK(pDevIns, pThis);
            break;
        }

        case HPET_STATUS:
        {
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            /* Clear ISR for all set bits in u32NewValue, see p. 14 of the HPET spec. */
            pThis->u64Isr &= ~((uint64_t)u32NewValue);
            Log(("write HPET_STATUS: %x -> ISR=%#llx\n", u32NewValue, pThis->u64Isr));
            DEVHPET_UNLOCK(pDevIns, pThis);
            break;
        }

        case HPET_STATUS + 4:
        {
            Log(("write HPET_STATUS + 4: %x\n", u32NewValue));
            if (u32NewValue != 0)
                LogRelMax(10, ("HPET: Writing HPET_STATUS + 4 with non-zero, ignored\n"));
            break;
        }

        case HPET_COUNTER:
        {
            STAM_REL_COUNTER_INC(&pThis->StatCounterWriteLow);
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            pThis->u64HpetCounter = RT_MAKE_U64(u32NewValue, RT_HI_U32(pThis->u64HpetCounter));
            Log(("write HPET_COUNTER: %#x -> %llx\n", u32NewValue, pThis->u64HpetCounter));
            DEVHPET_UNLOCK(pDevIns, pThis);
            break;
        }

        case HPET_COUNTER + 4:
        {
            STAM_REL_COUNTER_INC(&pThis->StatCounterWriteHigh);
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
            pThis->u64HpetCounter = RT_MAKE_U64(RT_LO_U32(pThis->u64HpetCounter), u32NewValue);
            Log(("write HPET_COUNTER + 4: %#x -> %llx\n", u32NewValue, pThis->u64HpetCounter));
            DEVHPET_UNLOCK(pDevIns, pThis);
            break;
        }

        default:
            LogRelMax(10, ("HPET: Invalid HPET config write: %x\n", idxReg));
            break;
    }

    return rc;
}


/* -=-=-=-=-=- MMIO callbacks -=-=-=-=-=- */


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) hpetMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    HPET      *pThis  = PDMDEVINS_2_DATA(pDevIns, HPET*);
    NOREF(pvUser);
    Assert(cb == 4 || cb == 8);

    LogFlow(("hpetMMIORead (%d): %RGp\n", cb, off));

    VBOXSTRICTRC rc;
    if (cb == 4)
    {
        /*
         * 4-byte access.
         */
        if (off >= 0x100 && off < 0x400)
        {
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            hpetTimerRegRead32(pDevIns, pThis,
                               (uint32_t)(off - 0x100) / 0x20,
                               (uint32_t)(off - 0x100) % 0x20,
                               (uint32_t *)pv);
            DEVHPET_UNLOCK(pDevIns, pThis);
            rc = VINF_SUCCESS;
        }
        else
            rc = hpetConfigRegRead32(pDevIns, pThis, off, (uint32_t *)pv);
    }
    else
    {
        /*
         * 8-byte access - Split the access except for timing sensitive registers.
         * The others assume the protection of the lock.
         */
        PRTUINT64U pValue = (PRTUINT64U)pv;
        if (off == HPET_COUNTER)
        {
            /* When reading HPET counter we must read it in a single read,
               to avoid unexpected time jumps on 32-bit overflow. */
            STAM_REL_COUNTER_INC(&pThis->StatCounterRead8Byte);
            DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            if (pThis->u64HpetConfig & HPET_CFG_ENABLE)
                pValue->u = hpetGetTicks(pDevIns, pThis);
            else
                pValue->u = pThis->u64HpetCounter;
            DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
            rc = VINF_SUCCESS;
        }
        else
        {
            DEVHPET_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_READ);
            if (off >= 0x100 && off < 0x400)
            {
                uint32_t iTimer    = (uint32_t)(off - 0x100) / 0x20;
                uint32_t iTimerReg = (uint32_t)(off - 0x100) % 0x20;
                hpetTimerRegRead32(pDevIns, pThis, iTimer, iTimerReg, &pValue->s.Lo);
                hpetTimerRegRead32(pDevIns, pThis, iTimer, iTimerReg + 4, &pValue->s.Hi);
                rc = VINF_SUCCESS;
            }
            else
            {
                /* for most 8-byte accesses we just split them, happens under lock anyway. */
                rc = hpetConfigRegRead32(pDevIns, pThis, off, &pValue->s.Lo);
                if (rc == VINF_SUCCESS)
                    rc = hpetConfigRegRead32(pDevIns, pThis, off + 4, &pValue->s.Hi);
            }
            DEVHPET_UNLOCK(pDevIns, pThis);
        }
    }
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) hpetMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    HPET  *pThis  = PDMDEVINS_2_DATA(pDevIns, HPET*);
    LogFlow(("hpetMMIOWrite: cb=%u reg=%RGp val=%llx\n",
             cb, off, cb == 4 ? *(uint32_t *)pv : cb == 8 ? *(uint64_t *)pv : 0xdeadbeef));
    NOREF(pvUser);
    Assert(cb == 4 || cb == 8);

    VBOXSTRICTRC rc;
    if (cb == 4)
    {
        if (off >= 0x100 && off < 0x400)
            rc = hpetTimerRegWrite32(pDevIns, pThis,
                                     (uint32_t)(off - 0x100) / 0x20,
                                     (uint32_t)(off - 0x100) % 0x20,
                                     *(uint32_t const *)pv);
        else
            rc = hpetConfigRegWrite32(pDevIns, pThis, off, *(uint32_t const *)pv);
    }
    else
    {
        /*
         * 8-byte access.
         */
        /* Split the access and rely on the locking to prevent trouble. */
        DEVHPET_LOCK_BOTH_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);
        RTUINT64U uValue;
        uValue.u = *(uint64_t const *)pv;
        if (off >= 0x100 && off < 0x400)
        {
            uint32_t iTimer    = (uint32_t)(off - 0x100) / 0x20;
            uint32_t iTimerReg = (uint32_t)(off - 0x100) % 0x20;
    /** @todo Consider handling iTimerReg == HPET_TN_CMP specially here */
            rc = hpetTimerRegWrite32(pDevIns, pThis, iTimer, iTimerReg, uValue.s.Lo);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = hpetTimerRegWrite32(pDevIns, pThis, iTimer, iTimerReg + 4, uValue.s.Hi);
        }
        else
        {
            rc = hpetConfigRegWrite32(pDevIns, pThis, off, uValue.s.Lo);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = hpetConfigRegWrite32(pDevIns, pThis, off + 4, uValue.s.Hi);
        }
        DEVHPET_UNLOCK_BOTH(pDevIns, pThis);
    }

    return rc;
}

#ifdef IN_RING3

/* -=-=-=-=-=- Timer Callback Processing -=-=-=-=-=- */

/**
 * Gets the IRQ of an HPET timer.
 *
 * @returns IRQ number.
 * @param   pThis               The shared HPET state.
 * @param   pHpetTimer          The HPET timer.
 */
static uint32_t hpetR3TimerGetIrq(PHPET pThis, PCHPETTIMER pHpetTimer)
{
    /*
     * Per spec, in legacy mode the HPET timers are wired as follows:
     *   timer 0: IRQ0 for PIC and IRQ2 for APIC
     *   timer 1: IRQ8 for both PIC and APIC
     *
     * ISA IRQ delivery logic will take care of correct delivery
     * to the different ICs.
     */
    if (   (pHpetTimer->idxTimer <= 1)
        && (pThis->u64HpetConfig & HPET_CFG_LEGACY))
        return (pHpetTimer->idxTimer == 0) ? 0 : 8;

    return (pHpetTimer->u64Config & HPET_TN_INT_ROUTE_MASK) >> HPET_TN_INT_ROUTE_SHIFT;
}


/**
 * Used by hpetR3Timer to update the IRQ status.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HPET state.
 * @param   pHpetTimer          The HPET timer.
 */
static void hpetR3TimerUpdateIrq(PPDMDEVINS pDevIns, PHPET pThis, PHPETTIMER pHpetTimer)
{
    /** @todo is it correct? */
    if (   !!(pHpetTimer->u64Config & HPET_TN_ENABLE)
        && !!(pThis->u64HpetConfig & HPET_CFG_ENABLE))
    {
        uint32_t irq = hpetR3TimerGetIrq(pThis, pHpetTimer);
        Log4(("HPET: raising IRQ %d\n", irq));

        /* ISR bits are only set in level-triggered mode. */
        if ((pHpetTimer->u64Config & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_LEVEL)
            pThis->u64Isr |= UINT64_C(1) << pHpetTimer->idxTimer;

        /* We trigger flip/flop in edge-triggered mode and do nothing in
           level-triggered mode yet. */
        if ((pHpetTimer->u64Config & HPET_TN_INT_TYPE) == HPET_TIMER_TYPE_EDGE)
        {
            PHPETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHPETCC);
            AssertReturnVoid(pThisCC);
            pThisCC->pHpetHlp->pfnSetIrq(pDevIns, irq, PDM_IRQ_LEVEL_FLIP_FLOP);
            STAM_REL_COUNTER_INC(&pHpetTimer->StatSetIrq);
        }
        else
            AssertFailed();
        /** @todo implement IRQs in level-triggered mode */
    }
}

/**
 * Device timer callback function.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the HPET timer state.
 */
static DECLCALLBACK(void) hpetR3Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PHPET       pThis      = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PHPETTIMER  pHpetTimer = (HPETTIMER *)pvUser;
    uint64_t    u64Period  = pHpetTimer->u64Period;
    uint64_t    u64CurTick = hpetGetTicks(pDevIns, pThis);
    uint64_t    u64Diff;
    RT_NOREF(pTimer);

    if (pHpetTimer->u64Config & HPET_TN_PERIODIC)
    {
        if (u64Period)
        {
            hpetAdjustComparator(pHpetTimer, u64CurTick);

            u64Diff = hpetComputeDiff(pHpetTimer, u64CurTick);

            uint64_t u64TickLimit = pThis->fIch9 ? HPET_TICKS_IN_100YR_ICH9 : HPET_TICKS_IN_100YR_PIIX;
            if (u64Diff <= u64TickLimit)
            {
                Log4(("HPET: periodic: next in %llu\n", hpetTicksToNs(pThis, u64Diff)));
                STAM_REL_COUNTER_INC(&pHpetTimer->StatSetTimer);
                PDMDevHlpTimerSetNano(pDevIns, pHpetTimer->hTimer, hpetTicksToNs(pThis, u64Diff));
            }
            else
            {
                LogRelMax(10, ("HPET: Not scheduling periodic interrupt more than 100 years in the future.\n"));
            }
        }
    }
    else if (hpet32bitTimer(pHpetTimer))
    {
        /* For 32-bit non-periodic timers, generate wrap-around interrupts. */
        if (pHpetTimer->u8Wrap)
        {
            u64Diff = hpetComputeDiff(pHpetTimer, u64CurTick);
            PDMDevHlpTimerSetNano(pDevIns, pHpetTimer->hTimer, hpetTicksToNs(pThis, u64Diff));
            pHpetTimer->u8Wrap = 0;
        }
    }

    /* Should it really be under lock, does it really matter? */
    hpetR3TimerUpdateIrq(pDevIns, pThis, pHpetTimer);
}


/* -=-=-=-=-=- DBGF Info Handlers -=-=-=-=-=- */


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hpetR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHPET pThis = PDMDEVINS_2_DATA(pDevIns, PHPET);
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp,
                    "HPET status:\n"
                    " config=%016RX64     isr=%016RX64\n"
                    " offset=%016RX64 counter=%016RX64 frequency=%08x\n"
                    " legacy-mode=%s  timer-count=%u\n",
                    pThis->u64HpetConfig, pThis->u64Isr,
                    pThis->u64HpetOffset, pThis->u64HpetCounter, pThis->u32Period,
                    !!(pThis->u64HpetConfig & HPET_CFG_LEGACY) ? "on " : "off",
                    HPET_CAP_GET_TIMERS(pThis->u32Capabilities));
    pHlp->pfnPrintf(pHlp,
                    "Timers:\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        pHlp->pfnPrintf(pHlp, " %d: comparator=%016RX64 period(hidden)=%016RX64 cfg=%016RX64\n",
                        pThis->aTimers[i].idxTimer,
                        pThis->aTimers[i].u64Cmp,
                        pThis->aTimers[i].u64Period,
                        pThis->aTimers[i].u64Config);
    }
}


/* -=-=-=-=-=- Saved State -=-=-=-=-=- */


/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) hpetR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PHPET           pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    NOREF(uPass);

    pHlp->pfnSSMPutU8(pSSM, HPET_CAP_GET_TIMERS(pThis->u32Capabilities));

    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) hpetR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PHPET           pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    /*
     * The config.
     */
    hpetR3LiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    /*
     * The state.
     */
    uint32_t const cTimers = HPET_CAP_GET_TIMERS(pThis->u32Capabilities);
    for (uint32_t iTimer = 0; iTimer < cTimers; iTimer++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[iTimer];
        PDMDevHlpTimerSave(pDevIns, pHpetTimer->hTimer, pSSM);
        pHlp->pfnSSMPutU8(pSSM,  pHpetTimer->u8Wrap);
        pHlp->pfnSSMPutU64(pSSM, pHpetTimer->u64Config);
        pHlp->pfnSSMPutU64(pSSM, pHpetTimer->u64Cmp);
        pHlp->pfnSSMPutU64(pSSM, pHpetTimer->u64Fsb);
        pHlp->pfnSSMPutU64(pSSM, pHpetTimer->u64Period);
    }

    pHlp->pfnSSMPutU64(pSSM, pThis->u64HpetOffset);
    uint64_t u64CapPer = RT_MAKE_U64(pThis->u32Capabilities, pThis->u32Period);
    pHlp->pfnSSMPutU64(pSSM, u64CapPer);
    pHlp->pfnSSMPutU64(pSSM, pThis->u64HpetConfig);
    pHlp->pfnSSMPutU64(pSSM, pThis->u64Isr);
    return pHlp->pfnSSMPutU64(pSSM, pThis->u64HpetCounter);
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) hpetR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PHPET           pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    /*
     * Version checks.
     */
    if (uVersion == HPET_SAVED_STATE_VERSION_EMPTY)
        return VINF_SUCCESS;
    if (uVersion != HPET_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /*
     * The config.
     */
    uint8_t cTimers;
    int rc = pHlp->pfnSSMGetU8(pSSM, &cTimers);
    AssertRCReturn(rc, rc);
    if (cTimers > RT_ELEMENTS(pThis->aTimers))
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - too many timers: saved=%#x config=%#x"),
                                       cTimers, RT_ELEMENTS(pThis->aTimers));

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /*
     * The state.
     */
    for (uint32_t iTimer = 0; iTimer < cTimers; iTimer++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[iTimer];
        PDMDevHlpTimerLoad(pDevIns, pHpetTimer->hTimer, pSSM);
        pHlp->pfnSSMGetU8(pSSM,  &pHpetTimer->u8Wrap);
        pHlp->pfnSSMGetU64(pSSM, &pHpetTimer->u64Config);
        pHlp->pfnSSMGetU64(pSSM, &pHpetTimer->u64Cmp);
        pHlp->pfnSSMGetU64(pSSM, &pHpetTimer->u64Fsb);
        pHlp->pfnSSMGetU64(pSSM, &pHpetTimer->u64Period);
    }

    pHlp->pfnSSMGetU64(pSSM, &pThis->u64HpetOffset);
    uint64_t u64CapPer;
    pHlp->pfnSSMGetU64(pSSM, &u64CapPer);
    pHlp->pfnSSMGetU64(pSSM, &pThis->u64HpetConfig);
    pHlp->pfnSSMGetU64(pSSM, &pThis->u64Isr);
    rc = pHlp->pfnSSMGetU64(pSSM, &pThis->u64HpetCounter);
    if (RT_FAILURE(rc))
        return rc;
    if (HPET_CAP_GET_TIMERS(RT_LO_U32(u64CapPer)) != cTimers)
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Capabilities does not match timer count: cTimers=%#x caps=%#x"),
                                       cTimers, (unsigned)HPET_CAP_GET_TIMERS(u64CapPer));
    pThis->u32Capabilities  = RT_LO_U32(u64CapPer);
    pThis->u32Period        = RT_HI_U32(u64CapPer);

    /*
     * Set the timer frequency hints.
     */
    PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_IGNORED);
    for (uint32_t iTimer = 0; iTimer < cTimers; iTimer++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[iTimer];
        if (PDMDevHlpTimerIsActive(pDevIns, pHpetTimer->hTimer))
            hpetTimerSetFrequencyHint(pDevIns, pThis, pHpetTimer);
    }
    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- PDMDEVREG -=-=-=-=-=- */


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) hpetR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PHPETRC pThisRC = PDMINS_2_DATA_RC(pDevIns, PHPETRC);
    LogFlow(("hpetR3Relocate:\n"));

    pThisRC->pHpetHlp += offDelta;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) hpetR3Reset(PPDMDEVINS pDevIns)
{
    PHPET   pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PHPETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHPETCC);
    LogFlow(("hpetR3Reset:\n"));

    /*
     * The timers first.
     */
    PDMDevHlpTimerLockClock(pDevIns, pThis->aTimers[0].hTimer, VERR_IGNORED);
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[i];
        Assert(pHpetTimer->idxTimer == i);
        PDMDevHlpTimerStop(pDevIns, pHpetTimer->hTimer);

        /* capable of periodic operations and 64-bits */
        if (pThis->fIch9)
            pHpetTimer->u64Config = (i == 0)
                                  ? (HPET_TN_PERIODIC_CAP | HPET_TN_SIZE_CAP)
                                  : 0;
        else
            pHpetTimer->u64Config = HPET_TN_PERIODIC_CAP | HPET_TN_SIZE_CAP;

        /* We can do all IRQs */
        uint32_t u32RoutingCap = 0xffffffff;
        pHpetTimer->u64Config |= ((uint64_t)u32RoutingCap) << HPET_TN_INT_ROUTE_CAP_SHIFT;
        pHpetTimer->u64Period  = 0;
        pHpetTimer->u8Wrap     = 0;
        pHpetTimer->u64Cmp     = hpetInvalidValue(pHpetTimer);
    }
    PDMDevHlpTimerUnlockClock(pDevIns, pThis->aTimers[0].hTimer);

    /*
     * The shared HPET state.
     */
    pThis->u64HpetConfig  = 0;
    pThis->u64HpetCounter = 0;
    pThis->u64HpetOffset  = 0;

    /* 64-bit main counter; 3 timers supported; LegacyReplacementRoute. */
    pThis->u32Capabilities = (1 << 15)        /* LEG_RT_CAP       - LegacyReplacementRoute capable. */
                           | (1 << 13)        /* COUNTER_SIZE_CAP - Main counter is 64-bit capable. */
                           | 1;               /* REV_ID           - Revision, must not be 0 */
    if (pThis->fIch9)                         /* NUM_TIM_CAP      - Number of timers -1. */
        pThis->u32Capabilities |= (HPET_NUM_TIMERS_ICH9 - 1) << 8;
    else
        pThis->u32Capabilities |= (HPET_NUM_TIMERS_PIIX - 1) << 8;
    pThis->u32Capabilities |= UINT32_C(0x80860000); /* VENDOR */
    AssertCompile(HPET_NUM_TIMERS_ICH9 <= RT_ELEMENTS(pThis->aTimers));
    AssertCompile(HPET_NUM_TIMERS_PIIX <= RT_ELEMENTS(pThis->aTimers));

    pThis->u32Period = pThis->fIch9 ? HPET_CLK_PERIOD_ICH9 : HPET_CLK_PERIOD_PIIX;

    /*
     * Notify the PIT/RTC devices.
     */
    if (pThisCC->pHpetHlp)
        pThisCC->pHpetHlp->pfnSetLegacyMode(pDevIns, false /*fActive*/);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hpetR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PHPET           pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PHPETCC         pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHPETCC);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    /* Only one HPET device now, as we use fixed MMIO region. */
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Initialize the device state.
     */

    /* Init the HPET timers (init all regardless of how many we expose). */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[i];
        pHpetTimer->idxTimer = i;
        pHpetTimer->hTimer   = NIL_TMTIMERHANDLE;
    }

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "ICH9", "");

    int rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "ICH9", &pThis->fIch9, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: failed to read ICH9 as boolean"));


    /*
     * Create critsect and timers.
     * Note! We don't use the default critical section of the device, but our own.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "HPET");
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Init the HPET timers (init all regardless of how many we expose). */
    static const char * const s_apszTimerNames[] =
    { "HPET Timer 0", "HPET Timer 1", "HPET Timer 2", "HPET Timer 3" };
    AssertCompile(RT_ELEMENTS(pThis->aTimers) == RT_ELEMENTS(s_apszTimerNames));
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        PHPETTIMER pHpetTimer = &pThis->aTimers[i];

        rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, hpetR3Timer, pHpetTimer,
                                  TMTIMER_FLAGS_NO_CRIT_SECT, s_apszTimerNames[i], &pThis->aTimers[i].hTimer);
        AssertRCReturn(rc, rc);
        /** @todo r=bird: This is TOTALLY MESSED UP!  Why do we need
         *        DEVHPET_LOCK_BOTH_RETURN() when the timers use the same critsect as
         *        we do?!? */
        rc = PDMDevHlpTimerSetCritSect(pDevIns, pThis->aTimers[i].hTimer, &pThis->CritSect);
        AssertRCReturn(rc, rc);
    }

    /*
     * This must be done prior to registering the HPET, right?
     */
    hpetR3Reset(pDevIns);

    /*
     * Register the HPET and get helpers.
     */
    PDMHPETREG HpetReg;
    HpetReg.u32Version = PDM_HPETREG_VERSION;
    rc = PDMDevHlpHpetRegister(pDevIns, &HpetReg, &pThisCC->pHpetHlp);
    AssertRCReturn(rc, rc);

    /*
     * Register the MMIO range, PDM API requests page aligned
     * addresses and sizes.
     */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, HPET_BASE, HPET_BAR_SIZE, hpetMMIOWrite, hpetMMIORead,
                                   IOMMMIO_FLAGS_READ_DWORD_QWORD | IOMMMIO_FLAGS_WRITE_ONLY_DWORD_QWORD,
                                   "HPET Memory", &pThis->hMmio);
    AssertRCReturn(rc, rc);

    /*
     * Register SSM state, info item and statistics.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, HPET_SAVED_STATE_VERSION, sizeof(*pThis), hpetR3LiveExec, hpetR3SaveExec, hpetR3LoadExec);
    AssertRCReturn(rc, rc);

    PDMDevHlpDBGFInfoRegister(pDevIns, "hpet", "Display HPET status. (no arguments)", hpetR3Info);

    /* Statistics: */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCounterRead4Byte, STAMTYPE_COUNTER,
                          "CounterRead4Byte", STAMUNIT_OCCURENCES, "HPET_COUNTER 32-bit reads");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCounterRead8Byte, STAMTYPE_COUNTER,
                          "CounterRead8Byte", STAMUNIT_OCCURENCES, "HPET_COUNTER 64-bit reads");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCounterWriteLow,  STAMTYPE_COUNTER,
                          "CounterWriteLow",  STAMUNIT_OCCURENCES, "Low HPET_COUNTER writes");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCounterWriteHigh, STAMTYPE_COUNTER,
                          "CounterWriteHigh", STAMUNIT_OCCURENCES, "High HPET_COUNTER writes");
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aTimers); i++)
    {
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aTimers[i].StatSetIrq, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                               STAMUNIT_OCCURENCES, "Number of times the IRQ has been set.",  "timer%u/SetIrq", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aTimers[i].StatSetTimer, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                               STAMUNIT_OCCURENCES, "Number of times the timer has be programmed.",  "timer%u/SetTimer", i);
    }

    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) hpetRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PHPET   pThis   = PDMDEVINS_2_DATA(pDevIns, PHPET);
    PHPETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHPETCC);

    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    PDMHPETREG HpetReg;
    HpetReg.u32Version = PDM_HPETREG_VERSION;
    rc = PDMDevHlpHpetSetUpContext(pDevIns, &HpetReg, &pThisCC->pHpetHlp);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, hpetMMIOWrite, hpetMMIORead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceHPET =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "hpet",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIT,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(HPET),
    /* .cbInstanceCC = */           sizeof(HPETCC),
    /* .cbInstanceRC = */           sizeof(HPETRC),
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "High Precision Event Timer (HPET) Device",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           hpetR3Construct,
    /* .pfnDestruct = */            NULL,
    /* .pfnRelocate = */            hpetR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               hpetR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           hpetRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           hpetRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

