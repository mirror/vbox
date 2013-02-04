/* $Id$ */
/** @file
 * DevPIT-i8254 - Intel 8254 Programmable Interval Timer (PIT) And Dummy Speaker Device.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU 8253/8254 interval timer emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PIT
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/vmm/stam.h>
#include <iprt/assert.h>
#include <iprt/asm-math.h>

#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif /* IN_RING3 */

#include "VBoxDD.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The PIT frequency. */
#define PIT_FREQ 1193182

#define RW_STATE_LSB 1
#define RW_STATE_MSB 2
#define RW_STATE_WORD0 3
#define RW_STATE_WORD1 4

/** The current saved state version. */
#define PIT_SAVED_STATE_VERSION             4
/** The saved state version used by VirtualBox 3.1 and earlier.
 * This did not include disable by HPET flag. */
#define PIT_SAVED_STATE_VERSION_VBOX_31     3
/** The saved state version used by VirtualBox 3.0 and earlier.
 * This did not include the config part. */
#define PIT_SAVED_STATE_VERSION_VBOX_30     2

/** @def FAKE_REFRESH_CLOCK
 * Define this to flip the 15usec refresh bit on every read.
 * If not defined, it will be flipped correctly. */
/* #define FAKE_REFRESH_CLOCK */
#ifdef DOXYGEN_RUNNING
# define FAKE_REFRESH_CLOCK
#endif

/** The effective counter mode - if bit 1 is set, bit 2 is ignored. */
#define EFFECTIVE_MODE(x)   ((x) & ~(((x) & 2) << 1))


/**
 * Acquires the PIT lock or returns.
 */
#define DEVPIT_LOCK_RETURN(a_pThis, a_rcBusy)  \
    do { \
        int rcLock = PDMCritSectEnter(&(a_pThis)->CritSect, (a_rcBusy)); \
        if (rcLock != VINF_SUCCESS) \
            return rcLock; \
    } while (0)

/**
 * Releases the PIT lock.
 */
#define DEVPIT_UNLOCK(a_pThis) \
    do { PDMCritSectLeave(&(a_pThis)->CritSect); } while (0)


/**
 * Acquires the TM lock and PIT lock, returns on failure.
 */
#define DEVPIT_LOCK_BOTH_RETURN(a_pThis, a_rcBusy)  \
    do { \
        int rcLock = TMTimerLock((a_pThis)->channels[0].CTX_SUFF(pTimer), (a_rcBusy)); \
        if (rcLock != VINF_SUCCESS) \
            return rcLock; \
        rcLock = PDMCritSectEnter(&(a_pThis)->CritSect, (a_rcBusy)); \
        if (rcLock != VINF_SUCCESS) \
        { \
            TMTimerUnlock((a_pThis)->channels[0].CTX_SUFF(pTimer)); \
            return rcLock; \
        } \
    } while (0)

#if IN_RING3
/**
 * Acquires the TM lock and PIT lock, ignores failures.
 */
# define DEVPIT_R3_LOCK_BOTH(a_pThis)  \
    do { \
        TMTimerLock((a_pThis)->channels[0].CTX_SUFF(pTimer), VERR_IGNORED); \
        PDMCritSectEnter(&(a_pThis)->CritSect, VERR_IGNORED); \
    } while (0)
#endif /* IN_RING3 */

/**
 * Releases the PIT lock and TM lock.
 */
#define DEVPIT_UNLOCK_BOTH(a_pThis) \
    do { \
        PDMCritSectLeave(&(a_pThis)->CritSect); \
        TMTimerUnlock((a_pThis)->channels[0].CTX_SUFF(pTimer)); \
    } while (0)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct PITChannelState
{
    /** Pointer to the instance data - R3 Ptr. */
    R3PTRTYPE(struct PITState *)    pPitR3;
    /** The timer - R3 Ptr. */
    PTMTIMERR3                      pTimerR3;
    /** Pointer to the instance data - R0 Ptr. */
    R0PTRTYPE(struct PITState *)    pPitR0;
    /** The timer - R0 Ptr. */
    PTMTIMERR0                      pTimerR0;
    /** Pointer to the instance data - RC Ptr. */
    RCPTRTYPE(struct PITState *)    pPitRC;
    /** The timer - RC Ptr. */
    PTMTIMERRC                      pTimerRC;
    /** The virtual time stamp at the last reload. (only used in mode 2 for now) */
    uint64_t                        u64ReloadTS;
    /** The actual time of the next tick.
     * As apposed to the next_transition_time which contains the correct time of the next tick. */
    uint64_t                        u64NextTS;

    /** (count_load_time is only set by TMTimerGet() which returns uint64_t) */
    uint64_t count_load_time;
    /* irq handling */
    int64_t next_transition_time;
    int32_t irq;
    /** Number of release log entries. Used to prevent flooding. */
    uint32_t cRelLogEntries;

    uint32_t count; /* can be 65536 */
    uint16_t latched_count;
    uint8_t count_latched;
    uint8_t status_latched;

    uint8_t status;
    uint8_t read_state;
    uint8_t write_state;
    uint8_t write_latch;

    uint8_t rw_mode;
    uint8_t mode;
    uint8_t bcd; /* not supported */
    uint8_t gate; /* timer start */

} PITChannelState;

typedef struct PITState
{
    /** Channel state. Must come first? */
    PITChannelState         channels[3];
    /** Speaker data. */
    int32_t                 speaker_data_on;
#ifdef FAKE_REFRESH_CLOCK
    /** Speaker dummy. */
    int32_t                 dummy_refresh_clock;
#else
    uint32_t                Alignment1;
#endif
    /** Config: I/O port base. */
    RTIOPORT                IOPortBaseCfg;
    /** Config: Speaker enabled. */
    bool                    fSpeakerCfg;
    bool                    fDisabledByHpet;
    bool                    afAlignment0[HC_ARCH_BITS == 32 ? 4 : 4];
    /** PIT port interface. */
    PDMIHPETLEGACYNOTIFY    IHpetLegacyNotify;
    /** Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** Number of IRQs that's been raised. */
    STAMCOUNTER             StatPITIrq;
    /** Profiling the timer callback handler. */
    STAMPROFILEADV          StatPITHandler;
    /** Critical section protecting the state. */
    PDMCRITSECT             CritSect;
} PITState;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef IN_RING3
static void pit_irq_timer_update(PITChannelState *s, uint64_t current_time, uint64_t now, bool in_timer);
#endif



static int pit_get_count(PITChannelState *s)
{
    uint64_t d;
    int counter;
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    Assert(TMTimerIsLockOwner(pTimer));

    if (EFFECTIVE_MODE(s->mode) == 2)
    {
        if (s->u64NextTS == UINT64_MAX)
        {
            d = ASMMultU64ByU32DivByU32(TMTimerGet(pTimer) - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
            return s->count - (d % s->count); /** @todo check this value. */
        }
        uint64_t Interval = s->u64NextTS - s->u64ReloadTS;
        if (!Interval)
            return s->count - 1; /** @todo This is WRONG! But I'm too tired to fix it properly and just want to shut up a DIV/0 trap now. */
        d = TMTimerGet(pTimer);
        d = ASMMultU64ByU32DivByU32(d - s->u64ReloadTS, s->count, Interval);
        if (d >= s->count)
            return 1;
        return s->count - d;
    }
    d = ASMMultU64ByU32DivByU32(TMTimerGet(pTimer) - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(EFFECTIVE_MODE(s->mode)) {
    case 0:
    case 1:
    case 4:
    case 5:
        counter = (s->count - d) & 0xffff;
        break;
    case 3:
        /* XXX: may be incorrect for odd counts */
        counter = s->count - ((2 * d) % s->count);
        break;
    default:
        counter = s->count - (d % s->count);
        break;
    }
    /** @todo check that we don't return 0, in most modes (all?) the counter shouldn't be zero. */
    return counter;
}

/* get pit output bit */
static int pit_get_out1(PITChannelState *s, int64_t current_time)
{
    uint64_t d;
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    int out;

    d = ASMMultU64ByU32DivByU32(current_time - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(EFFECTIVE_MODE(s->mode)) {
    default:
    case 0:
        out = (d >= s->count);
        break;
    case 1:
        out = (d < s->count);
        break;
    case 2:
        Log2(("pit_get_out1: d=%llx c=%x %x \n", d, s->count, (unsigned)(d % s->count)));
        if ((d % s->count) == 0 && d != 0)
            out = 1;
        else
            out = 0;
        break;
    case 3:
        out = (d % s->count) < ((s->count + 1) >> 1);
        break;
    case 4:
    case 5:
        out = (d != s->count);
        break;
    }
    return out;
}


static int pit_get_out(PITState *pit, int channel, int64_t current_time)
{
    PITChannelState *s = &pit->channels[channel];
    return pit_get_out1(s, current_time);
}


static int pit_get_gate(PITState *pit, int channel)
{
    PITChannelState *s = &pit->channels[channel];
    return s->gate;
}


/* if already latched, do not latch again */
static void pit_latch_count(PITChannelState *s)
{
    if (!s->count_latched) {
        s->latched_count = pit_get_count(s);
        s->count_latched = s->rw_mode;
        LogFlow(("pit_latch_count: latched_count=%#06x / %10RU64 ns (c=%#06x m=%d)\n",
                 s->latched_count, ASMMultU64ByU32DivByU32(s->count - s->latched_count, 1000000000, PIT_FREQ), s->count, s->mode));
    }
}

#ifdef IN_RING3

/* val must be 0 or 1 */
static void pit_set_gate(PITState *pit, int channel, int val)
{
    PITChannelState *s = &pit->channels[channel];
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    Assert((val & 1) == val);
    Assert(TMTimerIsLockOwner(pTimer));

    switch(EFFECTIVE_MODE(s->mode)) {
    default:
    case 0:
    case 4:
        /* XXX: just disable/enable counting */
        break;
    case 1:
    case 5:
        if (s->gate < val) {
            /* restart counting on rising edge */
            Log(("pit_set_gate: restarting mode %d\n", s->mode));
            s->count_load_time = TMTimerGet(pTimer);
            pit_irq_timer_update(s, s->count_load_time, s->count_load_time, false);
        }
        break;
    case 2:
    case 3:
        if (s->gate < val) {
            /* restart counting on rising edge */
            Log(("pit_set_gate: restarting mode %d\n", s->mode));
            s->count_load_time = s->u64ReloadTS = TMTimerGet(pTimer);
            pit_irq_timer_update(s, s->count_load_time, s->count_load_time, false);
        }
        /* XXX: disable/enable counting */
        break;
    }
    s->gate = val;
}

static void pit_load_count(PITChannelState *s, int val)
{
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    Assert(TMTimerIsLockOwner(pTimer));

    if (val == 0)
        val = 0x10000;
    s->count_load_time = s->u64ReloadTS = TMTimerGet(pTimer);
    s->count = val;
    pit_irq_timer_update(s, s->count_load_time, s->count_load_time, false);

    /* log the new rate (ch 0 only). */
    if (s->pTimerR3 /* ch 0 */)
    {
        if (s->cRelLogEntries++ < 32)
            LogRel(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=0)\n",
                    s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100));
        else
            Log(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=0)\n",
                 s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100));
        TMTimerSetFrequencyHint(s->CTX_SUFF(pTimer), PIT_FREQ / s->count);
    }
    else
        Log(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=%d)\n",
             s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100,
             s - &s->CTX_SUFF(pPit)->channels[0]));
}

/* return -1 if no transition will occur.  */
static int64_t pit_get_next_transition_time(PITChannelState *s,
                                            uint64_t current_time)
{
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    uint64_t d, next_time, base;
    uint32_t period2;

    d = ASMMultU64ByU32DivByU32(current_time - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(EFFECTIVE_MODE(s->mode)) {
    default:
    case 0:
    case 1:
        if (d < s->count)
            next_time = s->count;
        else
            return -1;
        break;
    /*
     * Mode 2: The period is 'count' PIT ticks.
     * When the counter reaches 1 we set the output low (for channel 0 that
     * means lowering IRQ0). On the next tick, where we should be decrementing
     * from 1 to 0, the count is loaded and the output goes high (channel 0
     * means raising IRQ0 again and triggering timer interrupt).
     *
     * In VirtualBox we compress the pulse and flip-flop the IRQ line at the
     * end of the period, which signals an interrupt at the exact same time.
     */
    case 2:
        base = (d / s->count) * s->count;
#ifndef VBOX /* see above */
        if ((d - base) == 0 && d != 0)
            next_time = base + s->count - 1;
        else
#endif
            next_time = base + s->count;
        break;
    case 3:
        base = (d / s->count) * s->count;
        period2 = ((s->count + 1) >> 1);
        if ((d - base) < period2)
            next_time = base + period2;
        else
            next_time = base + s->count;
        break;
    /* Modes 4 and 5 generate a short pulse at the end of the time delay. This
     * is similar to mode 2, except modes 4/5 aren't periodic. We use the same
     * optimization - only use one timer callback and pulse the IRQ.
     * Note: Tickless Linux kernels use PIT mode 4 with 'nolapic'.
     */
    case 4:
    case 5:
#ifdef VBOX
        if (d <= s->count)
            next_time = s->count;
#else
        if (d < s->count)
            next_time = s->count;
        else if (d == s->count)
            next_time = s->count + 1;
#endif
        else
            return -1;
        break;
    }
    /* convert to timer units */
    LogFlow(("PIT: next_time=%'14RU64 %'20RU64 mode=%#x count=%#06x\n", next_time,
             ASMMultU64ByU32DivByU32(next_time, TMTimerGetFreq(pTimer), PIT_FREQ), s->mode, s->count));
    next_time = s->count_load_time + ASMMultU64ByU32DivByU32(next_time, TMTimerGetFreq(pTimer), PIT_FREQ);
    /* fix potential rounding problems */
    if (next_time <= current_time)
        next_time = current_time;
    /* Add one to next_time; if we don't, integer truncation will cause
     * the algorithm to think that at the end of each period, it's still
     * within the first one instead of at the beginning of the next one.
     */
    return next_time + 1;
}

static void pit_irq_timer_update(PITChannelState *s, uint64_t current_time, uint64_t now, bool in_timer)
{
    int64_t expire_time;
    int irq_level;
    PPDMDEVINS pDevIns;
    PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
    Assert(TMTimerIsLockOwner(pTimer));

    if (!s->CTX_SUFF(pTimer))
        return;
    expire_time = pit_get_next_transition_time(s, current_time);
    irq_level = pit_get_out1(s, current_time) ? PDM_IRQ_LEVEL_HIGH : PDM_IRQ_LEVEL_LOW;

    /* If PIT is disabled by HPET - simply disconnect ticks from interrupt controllers,
     * but do not modify other aspects of device operation.
     */
    if (!s->pPitR3->fDisabledByHpet)
    {
        pDevIns = s->CTX_SUFF(pPit)->pDevIns;

        switch (EFFECTIVE_MODE(s->mode))
        {
            case 2:
            case 4:
            case 5:
                /* We just flip-flop the IRQ line to save an extra timer call,
                 * which isn't generally required. However, the pulse is only
                 * generated when running on the timer callback (and thus on
                 * the trailing edge of the output signal pulse).
                 */
                if (in_timer)
                {
                    PDMDevHlpISASetIrq(pDevIns, s->irq, PDM_IRQ_LEVEL_FLIP_FLOP);
                    break;
                }
                /* Else fall through! */
            default:
                PDMDevHlpISASetIrq(pDevIns, s->irq, irq_level);
                break;
        }
    }

    if (irq_level)
    {
        s->u64ReloadTS = now;
        STAM_COUNTER_INC(&s->CTX_SUFF(pPit)->StatPITIrq);
    }

    if (expire_time != -1)
    {
        Log3(("pit_irq_timer_update: next=%'RU64 now=%'RU64\n", expire_time, now));
        s->u64NextTS = expire_time;
        TMTimerSet(s->CTX_SUFF(pTimer), s->u64NextTS);
    }
    else
    {
        LogFlow(("PIT: m=%d count=%#4x irq_level=%#x stopped\n", s->mode, s->count, irq_level));
        TMTimerStop(s->CTX_SUFF(pTimer));
        s->u64NextTS = UINT64_MAX;
    }
    s->next_transition_time = expire_time;
}

#endif /* IN_RING3 */


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) pitIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    Log2(("pitIOPortRead: Port=%#x cb=%x\n", Port, cb));
    NOREF(pvUser);
    Port &= 3;
    if (cb != 1 || Port == 3)
    {
        Log(("pitIOPortRead: Port=%#x cb=%x *pu32=unused!\n", Port, cb));
        return VERR_IOM_IOPORT_UNUSED;
    }

    PITState *pit = PDMINS_2_DATA(pDevIns, PITState *);
    int ret;
    PITChannelState *s = &pit->channels[Port];

    DEVPIT_LOCK_RETURN(pit, VINF_IOM_R3_IOPORT_READ);
    if (s->status_latched)
    {
        s->status_latched = 0;
        ret = s->status;
        DEVPIT_UNLOCK(pit);
    }
    else if (s->count_latched)
    {
        switch (s->count_latched)
        {
            default:
            case RW_STATE_LSB:
                ret = s->latched_count & 0xff;
                s->count_latched = 0;
                break;
            case RW_STATE_MSB:
                ret = s->latched_count >> 8;
                s->count_latched = 0;
                break;
            case RW_STATE_WORD0:
                ret = s->latched_count & 0xff;
                s->count_latched = RW_STATE_MSB;
                break;
        }
        DEVPIT_UNLOCK(pit);
    }
    else
    {
        DEVPIT_UNLOCK(pit);
        DEVPIT_LOCK_BOTH_RETURN(pit, VINF_IOM_R3_IOPORT_READ);
        int count;
        switch (s->read_state)
        {
            default:
            case RW_STATE_LSB:
                count = pit_get_count(s);
                ret = count & 0xff;
                break;
            case RW_STATE_MSB:
                count = pit_get_count(s);
                ret = (count >> 8) & 0xff;
                break;
            case RW_STATE_WORD0:
                count = pit_get_count(s);
                ret = count & 0xff;
                s->read_state = RW_STATE_WORD1;
                break;
            case RW_STATE_WORD1:
                count = pit_get_count(s);
                ret = (count >> 8) & 0xff;
                s->read_state = RW_STATE_WORD0;
                break;
        }
        DEVPIT_UNLOCK_BOTH(pit);
    }

    *pu32 = ret;
    Log2(("pitIOPortRead: Port=%#x cb=%x *pu32=%#04x\n", Port, cb, *pu32));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) pitIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log2(("pitIOPortWrite: Port=%#x cb=%x u32=%#04x\n", Port, cb, u32));
    NOREF(pvUser);
    if (cb != 1)
        return VINF_SUCCESS;

    PITState *pit = PDMINS_2_DATA(pDevIns, PITState *);
    Port &= 3;
    if (Port == 3)
    {
        /*
         * Port 43h - Mode/Command Register.
         *  7 6 5 4 3 2 1 0
         *  * * . . . . . .  Select channel: 0 0 = Channel 0
         *                                   0 1 = Channel 1
         *                                   1 0 = Channel 2
         *                                   1 1 = Read-back command (8254 only)
         *                                                  (Illegal on 8253)
         *                                                  (Illegal on PS/2 {JAM})
         *  . . * * . . . .  Command/Access mode: 0 0 = Latch count value command
         *                                        0 1 = Access mode: lobyte only
         *                                        1 0 = Access mode: hibyte only
         *                                        1 1 = Access mode: lobyte/hibyte
         *  . . . . * * * .  Operating mode: 0 0 0 = Mode 0, 0 0 1 = Mode 1,
         *                                   0 1 0 = Mode 2, 0 1 1 = Mode 3,
         *                                   1 0 0 = Mode 4, 1 0 1 = Mode 5,
         *                                   1 1 0 = Mode 2, 1 1 1 = Mode 3
         *  . . . . . . . *  BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD
         */
        unsigned channel = u32 >> 6;
        if (channel == 3)
        {
            /* read-back command */
            DEVPIT_LOCK_BOTH_RETURN(pit, VINF_IOM_R3_IOPORT_WRITE);
            for (channel = 0; channel < RT_ELEMENTS(pit->channels); channel++)
            {
                PITChannelState *s = &pit->channels[channel];
                if (u32 & (2 << channel)) {
                    if (!(u32 & 0x20))
                        pit_latch_count(s);
                    if (!(u32 & 0x10) && !s->status_latched)
                    {
                        /* status latch */
                        /* XXX: add BCD and null count */
                        PTMTIMER pTimer = s->CTX_SUFF(pPit)->channels[0].CTX_SUFF(pTimer);
                        s->status = (pit_get_out1(s, TMTimerGet(pTimer)) << 7)
                            | (s->rw_mode << 4)
                            | (s->mode << 1)
                            | s->bcd;
                        s->status_latched = 1;
                    }
                }
            }
            DEVPIT_UNLOCK_BOTH(pit);
        }
        else
        {
            PITChannelState *s = &pit->channels[channel];
            unsigned access = (u32 >> 4) & 3;
            if (access == 0)
            {
                DEVPIT_LOCK_BOTH_RETURN(pit, VINF_IOM_R3_IOPORT_WRITE);
                pit_latch_count(s);
                DEVPIT_UNLOCK_BOTH(pit);
            }
            else
            {
                DEVPIT_LOCK_RETURN(pit, VINF_IOM_R3_IOPORT_WRITE);
                s->rw_mode = access;
                s->read_state = access;
                s->write_state = access;

                s->mode = (u32 >> 1) & 7;
                s->bcd = u32 & 1;
                /* XXX: update irq timer ? */
                DEVPIT_UNLOCK(pit);
            }
        }
    }
    else
    {
#ifndef IN_RING3
        /** @todo There is no reason not to do this in all contexts these
         *        days... */
        return VINF_IOM_R3_IOPORT_WRITE;
#else /* IN_RING3 */
        /*
         * Port 40-42h - Channel Data Ports.
         */
        PITChannelState *s = &pit->channels[Port];
        uint8_t const write_state = s->write_state;
        DEVPIT_LOCK_BOTH_RETURN(pit, VINF_IOM_R3_IOPORT_WRITE);
        switch (s->write_state)
        {
            default:
            case RW_STATE_LSB:
                pit_load_count(s, u32);
                break;
            case RW_STATE_MSB:
                pit_load_count(s, u32 << 8);
                break;
            case RW_STATE_WORD0:
                s->write_latch = u32;
                s->write_state = RW_STATE_WORD1;
                break;
            case RW_STATE_WORD1:
                pit_load_count(s, s->write_latch | (u32 << 8));
                s->write_state = RW_STATE_WORD0;
                break;
        }
        DEVPIT_UNLOCK_BOTH(pit);
#endif /* !IN_RING3 */
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for speaker IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) pitIOPortSpeakerRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
        DEVPIT_LOCK_BOTH_RETURN(pThis, VINF_IOM_R3_IOPORT_READ);

        const uint64_t u64Now = TMTimerGet(pThis->channels[0].CTX_SUFF(pTimer));
        Assert(TMTimerGetFreq(pThis->channels[0].CTX_SUFF(pTimer)) == 1000000000); /* lazy bird. */

        /* bit 6,7 Parity error stuff. */
        /* bit 5 - mirrors timer 2 output condition. */
        const int fOut = pit_get_out(pThis, 2, u64Now);
        /* bit 4 - toggled with each (DRAM?) refresh request, every 15.085 µs.
                   ASSUMES ns timer freq, see assertion above. */
#ifndef FAKE_REFRESH_CLOCK
        const int fRefresh = (u64Now / 15085) & 1;
#else
        pThis->dummy_refresh_clock ^= 1;
        const int fRefresh = pThis->dummy_refresh_clock;
#endif
        /* bit 2,3 NMI / parity status stuff. */
        /* bit 1 - speaker data status */
        const int fSpeakerStatus = pThis->speaker_data_on;
        /* bit 0 - timer 2 clock gate to speaker status. */
        const int fTimer2GateStatus = pit_get_gate(pThis, 2);

        DEVPIT_UNLOCK_BOTH(pThis);

        *pu32 = fTimer2GateStatus
              | (fSpeakerStatus << 1)
              | (fRefresh << 4)
              | (fOut << 5);
        Log(("pitIOPortSpeakerRead: Port=%#x cb=%x *pu32=%#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
    }
    Log(("pitIOPortSpeakerRead: Port=%#x cb=%x *pu32=unused!\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

#ifdef IN_RING3

/**
 * Port I/O Handler for speaker OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) pitIOPortSpeakerWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
        DEVPIT_LOCK_BOTH_RETURN(pThis, VERR_IGNORED);

        pThis->speaker_data_on = (u32 >> 1) & 1;
        pit_set_gate(pThis, 2, u32 & 1);

        DEVPIT_UNLOCK_BOTH(pThis);
    }
    Log(("pitIOPortSpeakerWrite: Port=%#x cb=%x u32=%#x\n", Port, cb, u32));
    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) pitLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    SSMR3PutIOPort(pSSM, pThis->IOPortBaseCfg);
    SSMR3PutU8(    pSSM, pThis->channels[0].irq);
    SSMR3PutBool(  pSSM, pThis->fSpeakerCfg);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) pitSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);

    /* The config. */
    pitLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    /* The state. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        PITChannelState *s = &pThis->channels[i];
        SSMR3PutU32(pSSM, s->count);
        SSMR3PutU16(pSSM, s->latched_count);
        SSMR3PutU8(pSSM, s->count_latched);
        SSMR3PutU8(pSSM, s->status_latched);
        SSMR3PutU8(pSSM, s->status);
        SSMR3PutU8(pSSM, s->read_state);
        SSMR3PutU8(pSSM, s->write_state);
        SSMR3PutU8(pSSM, s->write_latch);
        SSMR3PutU8(pSSM, s->rw_mode);
        SSMR3PutU8(pSSM, s->mode);
        SSMR3PutU8(pSSM, s->bcd);
        SSMR3PutU8(pSSM, s->gate);
        SSMR3PutU64(pSSM, s->count_load_time);
        SSMR3PutU64(pSSM, s->u64NextTS);
        SSMR3PutU64(pSSM, s->u64ReloadTS);
        SSMR3PutS64(pSSM, s->next_transition_time);
        if (s->CTX_SUFF(pTimer))
            TMR3TimerSave(s->CTX_SUFF(pTimer), pSSM);
    }

    SSMR3PutS32(pSSM, pThis->speaker_data_on);
#ifdef FAKE_REFRESH_CLOCK
    SSMR3PutS32(pSSM, pThis->dummy_refresh_clock);
#else
    SSMR3PutS32(pSSM, 0);
#endif

    SSMR3PutBool(pSSM, pThis->fDisabledByHpet);

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) pitLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PITState   *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    int         rc;

    if (    uVersion != PIT_SAVED_STATE_VERSION
        &&  uVersion != PIT_SAVED_STATE_VERSION_VBOX_30
        &&  uVersion != PIT_SAVED_STATE_VERSION_VBOX_31)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* The config. */
    if (uVersion > PIT_SAVED_STATE_VERSION_VBOX_30)
    {
        RTIOPORT IOPortBaseCfg;
        rc = SSMR3GetIOPort(pSSM, &IOPortBaseCfg); AssertRCReturn(rc, rc);
        if (IOPortBaseCfg != pThis->IOPortBaseCfg)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - IOPortBaseCfg: saved=%RTiop config=%RTiop"),
                                    IOPortBaseCfg, pThis->IOPortBaseCfg);

        uint8_t u8Irq;
        rc = SSMR3GetU8(pSSM, &u8Irq); AssertRCReturn(rc, rc);
        if (u8Irq != pThis->channels[0].irq)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - u8Irq: saved=%#x config=%#x"),
                                    u8Irq, pThis->channels[0].irq);

        bool fSpeakerCfg;
        rc = SSMR3GetBool(pSSM, &fSpeakerCfg); AssertRCReturn(rc, rc);
        if (fSpeakerCfg != pThis->fSpeakerCfg)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - fSpeakerCfg: saved=%RTbool config=%RTbool"),
                                    fSpeakerCfg, pThis->fSpeakerCfg);
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* The state. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        PITChannelState *s = &pThis->channels[i];
        SSMR3GetU32(pSSM, &s->count);
        SSMR3GetU16(pSSM, &s->latched_count);
        SSMR3GetU8(pSSM, &s->count_latched);
        SSMR3GetU8(pSSM, &s->status_latched);
        SSMR3GetU8(pSSM, &s->status);
        SSMR3GetU8(pSSM, &s->read_state);
        SSMR3GetU8(pSSM, &s->write_state);
        SSMR3GetU8(pSSM, &s->write_latch);
        SSMR3GetU8(pSSM, &s->rw_mode);
        SSMR3GetU8(pSSM, &s->mode);
        SSMR3GetU8(pSSM, &s->bcd);
        SSMR3GetU8(pSSM, &s->gate);
        SSMR3GetU64(pSSM, &s->count_load_time);
        SSMR3GetU64(pSSM, &s->u64NextTS);
        SSMR3GetU64(pSSM, &s->u64ReloadTS);
        SSMR3GetS64(pSSM, &s->next_transition_time);
        if (s->CTX_SUFF(pTimer))
        {
            TMR3TimerLoad(s->CTX_SUFF(pTimer), pSSM);
            LogRel(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=%d) (restore)\n",
                    s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100, i));
            PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
            TMTimerSetFrequencyHint(s->CTX_SUFF(pTimer), PIT_FREQ / s->count);
            PDMCritSectLeave(&pThis->CritSect);
        }
        pThis->channels[i].cRelLogEntries = 0;
    }

    SSMR3GetS32(pSSM, &pThis->speaker_data_on);
#ifdef FAKE_REFRESH_CLOCK
    SSMR3GetS32(pSSM, &pThis->dummy_refresh_clock);
#else
    int32_t u32Dummy;
    SSMR3GetS32(pSSM, &u32Dummy);
#endif
    if (uVersion > PIT_SAVED_STATE_VERSION_VBOX_31)
        SSMR3GetBool(pSSM, &pThis->fDisabledByHpet);

    return VINF_SUCCESS;
}


/**
 * Device timer callback function.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the PIT channel state.
 */
static DECLCALLBACK(void) pitTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PITChannelState *s = (PITChannelState *)pvUser;
    STAM_PROFILE_ADV_START(&s->CTX_SUFF(pPit)->StatPITHandler, a);

    Log(("pitTimer\n"));
    Assert(PDMCritSectIsOwner(&PDMINS_2_DATA(pDevIns, PITState *)->CritSect));
    Assert(TMTimerIsLockOwner(pTimer));

    pit_irq_timer_update(s, s->next_transition_time, TMTimerGet(pTimer), true);

    STAM_PROFILE_ADV_STOP(&s->CTX_SUFF(pPit)->StatPITHandler, a);
}


/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) pitInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PITState   *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    unsigned    i;
    for (i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        const PITChannelState *pCh = &pThis->channels[i];

        pHlp->pfnPrintf(pHlp,
                        "PIT (i8254) channel %d status: irq=%#x\n"
                        "      count=%08x"      "  latched_count=%04x  count_latched=%02x\n"
                        "           status=%02x   status_latched=%02x     read_state=%02x\n"
                        "      write_state=%02x      write_latch=%02x        rw_mode=%02x\n"
                        "             mode=%02x              bcd=%02x           gate=%02x\n"
                        "  count_load_time=%016RX64 next_transition_time=%016RX64\n"
                        "      u64ReloadTS=%016RX64            u64NextTS=%016RX64\n"
                        ,
                        i, pCh->irq,
                        pCh->count,         pCh->latched_count,     pCh->count_latched,
                        pCh->status,        pCh->status_latched,    pCh->read_state,
                        pCh->write_state,   pCh->write_latch,       pCh->rw_mode,
                        pCh->mode,          pCh->bcd,               pCh->gate,
                        pCh->count_load_time,   pCh->next_transition_time,
                        pCh->u64ReloadTS,       pCh->u64NextTS);
    }
#ifdef FAKE_REFRESH_CLOCK
    pHlp->pfnPrintf(pHlp, "speaker_data_on=%#x dummy_refresh_clock=%#x\n",
                    pThis->speaker_data_on, pThis->dummy_refresh_clock);
#else
    pHlp->pfnPrintf(pHlp, "speaker_data_on=%#x\n", pThis->speaker_data_on);
#endif
    if (pThis->fDisabledByHpet)
        pHlp->pfnPrintf(pHlp, "Disabled by HPET\n");
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) pitQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDEVINS  pDevIns = RT_FROM_MEMBER(pInterface, PDMDEVINS, IBase);
    PITState   *pThis   = PDMINS_2_DATA(pDevIns, PITState *);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,    &pDevIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHPETLEGACYNOTIFY, &pThis->IHpetLegacyNotify);
    return NULL;
}


/**
 * @interface_method_impl{PDMIHPETLEGACYNOTIFY,pfnModeChanged}
 */
static DECLCALLBACK(void) pitNotifyHpetLegacyNotify_ModeChanged(PPDMIHPETLEGACYNOTIFY pInterface, bool fActivated)
{
    PITState *pThis = RT_FROM_MEMBER(pInterface, PITState, IHpetLegacyNotify);
    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);

    pThis->fDisabledByHpet = fActivated;

    PDMCritSectLeave(&pThis->CritSect);
}


/**
 * Relocation notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @param   offDelta    The delta relative to the old address.
 */
static DECLCALLBACK(void) pitRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    LogFlow(("pitRelocate: \n"));

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        PITChannelState *pCh = &pThis->channels[i];
        if (pCh->pTimerR3)
            pCh->pTimerRC = TMTimerRCPtr(pCh->pTimerR3);
        pThis->channels[i].pPitRC = PDMINS_2_DATA_RCPTR(pDevIns);
    }
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) pitReset(PPDMDEVINS pDevIns)
{
    PITState *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    LogFlow(("pitReset: \n"));

    DEVPIT_R3_LOCK_BOTH(pThis);

    pThis->fDisabledByHpet = false;

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        PITChannelState *s = &pThis->channels[i];

#if 1 /* Set everything back to virgin state. (might not be strictly correct) */
        s->latched_count = 0;
        s->count_latched = 0;
        s->status_latched = 0;
        s->status = 0;
        s->read_state = 0;
        s->write_state = 0;
        s->write_latch = 0;
        s->rw_mode = 0;
        s->bcd = 0;
#endif
        s->u64NextTS = UINT64_MAX;
        s->cRelLogEntries = 0;
        s->mode = 3;
        s->gate = (i != 2);
        pit_load_count(s, 0);
    }

    DEVPIT_UNLOCK_BOTH(pThis);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)  pitConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PITState   *pThis = PDMINS_2_DATA(pDevIns, PITState *);
    int         rc;
    uint8_t     u8Irq;
    uint16_t    u16Base;
    bool        fSpeaker;
    bool        fGCEnabled;
    bool        fR0Enabled;
    unsigned    i;
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Irq\0" "Base\0" "SpeakerEnabled\0" "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init the data.
     */
    rc = CFGMR3QueryU8Def(pCfg, "Irq", &u8Irq, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Irq\" as a uint8_t failed"));

    rc = CFGMR3QueryU16Def(pCfg, "Base", &u16Base, 0x40);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Base\" as a uint16_t failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "SpeakerEnabled", &fSpeaker, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"SpeakerEnabled\" as a bool failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"GCEnabled\" as a bool failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));

    pThis->pDevIns         = pDevIns;
    pThis->IOPortBaseCfg   = u16Base;
    pThis->fSpeakerCfg     = fSpeaker;
    pThis->channels[0].irq = u8Irq;
    for (i = 0; i < RT_ELEMENTS(pThis->channels); i++)
    {
        pThis->channels[i].pPitR3 = pThis;
        pThis->channels[i].pPitR0 = PDMINS_2_DATA_R0PTR(pDevIns);
        pThis->channels[i].pPitRC = PDMINS_2_DATA_RCPTR(pDevIns);
    }

    /*
     * Interfaces
     */
    /* IBase */
    pDevIns->IBase.pfnQueryInterface        = pitQueryInterface;
    /* IHpetLegacyNotify */
    pThis->IHpetLegacyNotify.pfnModeChanged = pitNotifyHpetLegacyNotify_ModeChanged;

    /*
     * We do our own locking.  This must be done before creating timers.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "pit#%u", iInstance);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Create the timer, make it take our critsect.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, pitTimer, &pThis->channels[0],
                                TMTIMER_FLAGS_NO_CRIT_SECT, "i8254 Programmable Interval Timer",
                                &pThis->channels[0].pTimerR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->channels[0].pTimerRC = TMTimerRCPtr(pThis->channels[0].pTimerR3);
    pThis->channels[0].pTimerR0 = TMTimerR0Ptr(pThis->channels[0].pTimerR3);
    rc = TMR3TimerSetCritSect(pThis->channels[0].pTimerR3, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * Register I/O ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, u16Base, 4, NULL, pitIOPortWrite, pitIOPortRead, NULL, NULL, "i8254 Programmable Interval Timer");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, u16Base, 4, 0, "pitIOPortWrite", "pitIOPortRead", NULL, NULL, "i8254 Programmable Interval Timer");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, u16Base, 4, 0, "pitIOPortWrite", "pitIOPortRead", NULL, NULL, "i8254 Programmable Interval Timer");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (fSpeaker)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, 0x61, 1, NULL, pitIOPortSpeakerWrite, pitIOPortSpeakerRead, NULL, NULL, "PC Speaker");
        if (RT_FAILURE(rc))
            return rc;
        if (fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x61, 1, 0, NULL, "pitIOPortSpeakerRead", NULL, NULL, "PC Speaker");
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, PIT_SAVED_STATE_VERSION, sizeof(*pThis), pitLiveExec, pitSaveExec, pitLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the device state.
     */
    pitReset(pDevIns);

    /*
     * Register statistics and debug info.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPITIrq,      STAMTYPE_COUNTER, "/TM/PIT/Irq",      STAMUNIT_OCCURENCES,     "The number of times a timer interrupt was triggered.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPITHandler,  STAMTYPE_PROFILE, "/TM/PIT/Handler",  STAMUNIT_TICKS_PER_CALL, "Profiling timer callback handler.");

    PDMDevHlpDBGFInfoRegister(pDevIns, "pit", "Display PIT (i8254) status. (no arguments)", pitInfo);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceI8254 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "i8254",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel 8254 Programmable Interval Timer (PIT) And Dummy Speaker Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PIT,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PITState),
    /* pfnConstruct */
    pitConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pitRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pitReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface */
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
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
