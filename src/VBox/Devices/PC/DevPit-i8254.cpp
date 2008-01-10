/** $Id$ */
/** @file
 * Intel 8254 Programmable Interval Timer (PIT) And Dummy Speaker Device.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include "Builtins.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The PIT frequency. */
#define PIT_FREQ 1193182

#define RW_STATE_LSB 1
#define RW_STATE_MSB 2
#define RW_STATE_WORD0 3
#define RW_STATE_WORD1 4

/** The version of the saved state. */
#define PIT_SAVED_STATE_VERSION 2

/** @def FAKE_REFRESH_CLOCK
 * Define this to flip the 15usec refresh bit on every read.
 * If not defined, it will be flipped correctly. */
//#define FAKE_REFRESH_CLOCK

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct PITChannelState
{
    /** Pointer to the instance data - HCPtr. */
    R3R0PTRTYPE(struct PITState *)  pPitHC;
    /** The timer - HCPtr. */
    R3R0PTRTYPE(PTMTIMER)           pTimerHC;
    /** Pointer to the instance data - GCPtr. */
    GCPTRTYPE(struct PITState *)    pPitGC;
    /** The timer - HCPtr. */
    PTMTIMERGC                      pTimerGC;
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
    /** Number of release log entries. Used to prevent floading. */
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
    PITChannelState         channels[3];
    /** Speaker data. */
    int32_t                 speaker_data_on;
#ifdef FAKE_REFRESH_CLOCK
    /** Speaker dummy. */
    int32_t                 dummy_refresh_clock;
#else
    uint32_t                Alignment1;
#endif
    /** Pointer to the device instance. */
    R3PTRTYPE(PPDMDEVINS)   pDevIns;
#if HC_ARCH_BITS == 32
    uint32_t                Alignment0;
#endif
    /** Number of IRQs that's been raised. */
    STAMCOUNTER             StatPITIrq;
    /** Profiling the timer callback handler. */
    STAMPROFILEADV          StatPITHandler;
} PITState;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PDMBOTHCBDECL(int) pitIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) pitIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) pitIOPortSpeakerRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
#ifdef IN_RING3
PDMBOTHCBDECL(int) pitIOPortSpeakerWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
static void pit_irq_timer_update(PITChannelState *s, uint64_t current_time);
#endif
__END_DECLS




static int pit_get_count(PITChannelState *s)
{
    uint64_t d;
    int counter;
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);

    if (s->mode == 2) /** @todo Implement proper virtual time and get rid of this hack.. */
    {
#if 0
        d = TMTimerGet(pTimer);
        d -= s->u64ReloadTS;
        d = ASMMultU64ByU32DivByU32(d, PIT_FREQ, TMTimerGetFreq(pTimer));
#else /* variable time because of catch up */
        if (s->u64NextTS == UINT64_MAX)
            return 1; /** @todo check this value. */
        d = TMTimerGet(pTimer);
        d = ASMMultU64ByU32DivByU32(d - s->u64ReloadTS, s->count, s->u64NextTS - s->u64ReloadTS);
#endif
        if (d >= s->count)
            return 1;
        return s->count - d;
    }
    d = ASMMultU64ByU32DivByU32(TMTimerGet(pTimer) - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(s->mode) {
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
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);
    int out;

    d = ASMMultU64ByU32DivByU32(current_time - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(s->mode) {
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
        out = (d == s->count);
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
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);
    Assert((val & 1) == val);

    switch(s->mode) {
    default:
    case 0:
    case 4:
        /* XXX: just disable/enable counting */
        break;
    case 1:
    case 5:
        if (s->gate < val) {
            /* restart counting on rising edge */
            s->count_load_time = TMTimerGet(pTimer);
            pit_irq_timer_update(s, s->count_load_time);
        }
        break;
    case 2:
    case 3:
        if (s->gate < val) {
            /* restart counting on rising edge */
            s->count_load_time = s->u64ReloadTS = TMTimerGet(pTimer);
            pit_irq_timer_update(s, s->count_load_time);
        }
        /* XXX: disable/enable counting */
        break;
    }
    s->gate = val;
}

static inline void pit_load_count(PITChannelState *s, int val)
{
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);
    if (val == 0)
        val = 0x10000;
    s->count_load_time = s->u64ReloadTS = TMTimerGet(pTimer);
    s->count = val;
    pit_irq_timer_update(s, s->count_load_time);

    /* log the new rate (ch 0 only). */
    if (    s->pTimerHC /* ch 0 */
        &&  s->cRelLogEntries++ < 32)
        LogRel(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=0)\n",
                s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100));
}

/* return -1 if no transition will occur.  */
static int64_t pit_get_next_transition_time(PITChannelState *s,
                                            uint64_t current_time)
{
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);
    uint64_t d, next_time, base;
    uint32_t period2;

    d = ASMMultU64ByU32DivByU32(current_time - s->count_load_time, PIT_FREQ, TMTimerGetFreq(pTimer));
    switch(s->mode) {
    default:
    case 0:
    case 1:
        if (d < s->count)
            next_time = s->count;
        else
            return -1;
        break;
    /*
     * Mode 2: The period is count + 1 PIT ticks.
     * When the counter reaches 1 we sent the output low (for channel 0 that
     * means raise an irq). On the next tick, where we should be decrementing
     * from 1 to 0, the count is loaded and the output goes high (channel 0
     * means clearing the irq).
     *
     * In VBox we simplify the tick cycle between 1 and 0 and immediately clears
     * the irq. We also don't set it until we reach 0, which is a tick late - will
     * try fix that later some day.
     */
    case 2:
        base = (d / s->count) * s->count;
#ifndef VBOX /* see above */
        if ((d - base) == 0 && d != 0)
            next_time = base + s->count;
        else
#endif
            next_time = base + s->count + 1;
        break;
    case 3:
        base = (d / s->count) * s->count;
        period2 = ((s->count + 1) >> 1);
        if ((d - base) < period2)
            next_time = base + period2;
        else
            next_time = base + s->count;
        break;
    case 4:
    case 5:
        if (d < s->count)
            next_time = s->count;
        else if (d == s->count)
            next_time = s->count + 1;
        else
            return -1;
        break;
    }
    /* convert to timer units */
    LogFlow(("PIT: next_time=%14RI64 %20RI64 mode=%#x count=%#06x\n", next_time,
             ASMMultU64ByU32DivByU32(next_time, TMTimerGetFreq(pTimer), PIT_FREQ), s->mode, s->count));
    next_time = s->count_load_time + ASMMultU64ByU32DivByU32(next_time, TMTimerGetFreq(pTimer), PIT_FREQ);
    /* fix potential rounding problems */
    /* XXX: better solution: use a clock at PIT_FREQ Hz */
    if (next_time <= current_time)
        next_time = current_time + 1;
    return next_time;
}

static void pit_irq_timer_update(PITChannelState *s, uint64_t current_time)
{
    uint64_t now;
    int64_t expire_time;
    int irq_level;
    PPDMDEVINS pDevIns;
    PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);

    if (!s->CTXSUFF(pTimer))
        return;
    expire_time = pit_get_next_transition_time(s, current_time);
    irq_level = pit_get_out1(s, current_time);

    /* We just flip-flop the irq level to save that extra timer call, which isn't generally required (we haven't served it for months). */
    pDevIns = s->CTXSUFF(pPit)->pDevIns;
    PDMDevHlpISASetIrq(pDevIns, s->irq, irq_level);
    if (irq_level)
        PDMDevHlpISASetIrq(pDevIns, s->irq, 0);
    now = TMTimerGet(pTimer);
    Log3(("pit_irq_timer_update: %lldns late\n", now - s->u64NextTS));
    if (irq_level)
    {
        s->u64ReloadTS = now;
        STAM_COUNTER_INC(&s->CTXSUFF(pPit)->StatPITIrq);
    }

    if (expire_time != -1)
    {
        s->u64NextTS = expire_time;
        TMTimerSet(s->CTXSUFF(pTimer), s->u64NextTS);
    }
    else
    {
        LogFlow(("PIT: m=%d count=%#4x irq_level=%#x stopped\n", s->mode, s->count, irq_level));
        TMTimerStop(s->CTXSUFF(pTimer));
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

    PITState *pit = PDMINS2DATA(pDevIns, PITState *);
    int ret;
    PITChannelState *s = &pit->channels[Port];
    if (s->status_latched)
    {
        s->status_latched = 0;
        ret = s->status;
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
    }
    else
    {
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

    PITState *pit = PDMINS2DATA(pDevIns, PITState *);
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
            for (channel = 0; channel < ELEMENTS(pit->channels); channel++)
            {
                PITChannelState *s = &pit->channels[channel];
                if (u32 & (2 << channel)) {
                    if (!(u32 & 0x20))
                        pit_latch_count(s);
                    if (!(u32 & 0x10) && !s->status_latched)
                    {
                        /* status latch */
                        /* XXX: add BCD and null count */
                        PTMTIMER pTimer = s->CTXSUFF(pPit)->channels[0].CTXSUFF(pTimer);
                        s->status = (pit_get_out1(s, TMTimerGet(pTimer)) << 7)
                            | (s->rw_mode << 4)
                            | (s->mode << 1)
                            | s->bcd;
                        s->status_latched = 1;
                    }
                }
            }
        }
        else
        {
            PITChannelState *s = &pit->channels[channel];
            unsigned access = (u32 >> 4) & 3;
            if (access == 0)
                pit_latch_count(s);
            else
            {
                s->rw_mode = access;
                s->read_state = access;
                s->write_state = access;

                s->mode = (u32 >> 1) & 7;
                s->bcd = u32 & 1;
                /* XXX: update irq timer ? */
            }
        }
    }
    else
    {
#ifndef IN_RING3
        return VINF_IOM_HC_IOPORT_WRITE;
#else /* IN_RING3 */
        /*
         * Port 40-42h - Channel Data Ports.
         */
        PITChannelState *s = &pit->channels[Port];
        switch(s->write_state)
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
        PITState *pData = PDMINS2DATA(pDevIns, PITState *);
        const uint64_t u64Now = TMTimerGet(pData->channels[0].CTXSUFF(pTimer));
        Assert(TMTimerGetFreq(pData->channels[0].CTXSUFF(pTimer)) == 1000000000); /* lazy bird. */

        /* bit 6,7 Parity error stuff. */
        /* bit 5 - mirrors timer 2 output condition. */
        const int fOut = pit_get_out(pData, 2, u64Now);
        /* bit 4 - toggled every with each (DRAM?) refresh request, every 15.085 µs. */
#ifdef FAKE_REFRESH_CLOCK
        pData->dummy_refresh_clock ^= 1;
        const int fRefresh = pData->dummy_refresh_clock;
#else
        const int fRefresh = (u64Now / 15085) & 1;
#endif
        /* bit 2,3 NMI / parity status stuff. */
        /* bit 1 - speaker data status */
        const int fSpeakerStatus = pData->speaker_data_on;
        /* bit 0 - timer 2 clock gate to speaker status. */
        const int fTimer2GateStatus = pit_get_gate(pData, 2);

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
        PITState *pData = PDMINS2DATA(pDevIns, PITState *);
        pData->speaker_data_on = (u32 >> 1) & 1;
        pit_set_gate(pData, 2, u32 & 1);
    }
    Log(("pitIOPortSpeakerWrite: Port=%#x cb=%x u32=%#x\n", Port, cb, u32));
    return VINF_SUCCESS;
}


/**
 * Saves a state of the programmable interval timer device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) pitSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    PITState *pData = PDMINS2DATA(pDevIns, PITState *);
    unsigned i;

    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        PITChannelState *s = &pData->channels[i];
        SSMR3PutU32(pSSMHandle, s->count);
        SSMR3PutU16(pSSMHandle, s->latched_count);
        SSMR3PutU8(pSSMHandle, s->count_latched);
        SSMR3PutU8(pSSMHandle, s->status_latched);
        SSMR3PutU8(pSSMHandle, s->status);
        SSMR3PutU8(pSSMHandle, s->read_state);
        SSMR3PutU8(pSSMHandle, s->write_state);
        SSMR3PutU8(pSSMHandle, s->write_latch);
        SSMR3PutU8(pSSMHandle, s->rw_mode);
        SSMR3PutU8(pSSMHandle, s->mode);
        SSMR3PutU8(pSSMHandle, s->bcd);
        SSMR3PutU8(pSSMHandle, s->gate);
        SSMR3PutU64(pSSMHandle, s->count_load_time);
        SSMR3PutU64(pSSMHandle, s->u64NextTS);
        SSMR3PutU64(pSSMHandle, s->u64ReloadTS);
        SSMR3PutS64(pSSMHandle, s->next_transition_time);
        if (s->CTXSUFF(pTimer))
            TMR3TimerSave(s->CTXSUFF(pTimer), pSSMHandle);
    }

    SSMR3PutS32(pSSMHandle, pData->speaker_data_on);
#ifdef FAKE_REFRESH_CLOCK
    return SSMR3PutS32(pSSMHandle, pData->dummy_refresh_clock);
#else
    return SSMR3PutS32(pSSMHandle, 0);
#endif
}


/**
 * Loads a saved programmable interval timer device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) pitLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    PITState *pData = PDMINS2DATA(pDevIns, PITState *);
    unsigned i;

    if (u32Version != PIT_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        PITChannelState *s = &pData->channels[i];
        SSMR3GetU32(pSSMHandle, &s->count);
        SSMR3GetU16(pSSMHandle, &s->latched_count);
        SSMR3GetU8(pSSMHandle, &s->count_latched);
        SSMR3GetU8(pSSMHandle, &s->status_latched);
        SSMR3GetU8(pSSMHandle, &s->status);
        SSMR3GetU8(pSSMHandle, &s->read_state);
        SSMR3GetU8(pSSMHandle, &s->write_state);
        SSMR3GetU8(pSSMHandle, &s->write_latch);
        SSMR3GetU8(pSSMHandle, &s->rw_mode);
        SSMR3GetU8(pSSMHandle, &s->mode);
        SSMR3GetU8(pSSMHandle, &s->bcd);
        SSMR3GetU8(pSSMHandle, &s->gate);
        SSMR3GetU64(pSSMHandle, &s->count_load_time);
        SSMR3GetU64(pSSMHandle, &s->u64NextTS);
        SSMR3GetU64(pSSMHandle, &s->u64ReloadTS);
        SSMR3GetS64(pSSMHandle, &s->next_transition_time);
        if (s->CTXSUFF(pTimer))
        {
            TMR3TimerLoad(s->CTXSUFF(pTimer), pSSMHandle);
            LogRel(("PIT: mode=%d count=%#x (%u) - %d.%02d Hz (ch=%d) (restore)\n",
                    s->mode, s->count, s->count, PIT_FREQ / s->count, (PIT_FREQ * 100 / s->count) % 100, i));
        }
        pData->channels[0].cRelLogEntries = 0;
    }

    SSMR3GetS32(pSSMHandle, &pData->speaker_data_on);
#ifdef FAKE_REFRESH_CLOCK
    return SSMR3GetS32(pSSMHandle, &pData->dummy_refresh_clock);
#else
    int32_t u32Dummy;
    return SSMR3GetS32(pSSMHandle, &u32Dummy);
#endif
}


/**
 * Device timer callback function.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 */
static DECLCALLBACK(void) pitTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    PITState *pData = PDMINS2DATA(pDevIns, PITState *);
    PITChannelState *s = &pData->channels[0];
    STAM_PROFILE_ADV_START(&s->CTXSUFF(pPit)->StatPITHandler, a);
    pit_irq_timer_update(s, s->next_transition_time);
    STAM_PROFILE_ADV_STOP(&s->CTXSUFF(pPit)->StatPITHandler, a);
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
    PITState *pData = PDMINS2DATA(pDevIns, PITState *);
    unsigned i;
    LogFlow(("pitRelocate: \n"));

    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        PITChannelState *pCh = &pData->channels[i];
        if (pCh->pTimerHC)
            pCh->pTimerGC = TMTimerGCPtr(pCh->pTimerHC);
        pData->channels[i].pPitGC = PDMINS2DATA_GCPTR(pDevIns);
    }
}

/** @todo remove this! */
static DECLCALLBACK(void) pitInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs);

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) pitReset(PPDMDEVINS pDevIns)
{
    PITState *pData = PDMINS2DATA(pDevIns, PITState *);
    unsigned i;
    LogFlow(("pitReset: \n"));

    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        PITChannelState *s = &pData->channels[i];

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
        s->cRelLogEntries = 0;
        s->mode = 3;
        s->gate = (i != 2);
        pit_load_count(s, 0);
    }
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
    PITState   *pData = PDMINS2DATA(pDevIns, PITState *);
    unsigned    i;
    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        const PITChannelState *pCh = &pData->channels[i];

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
                    pData->speaker_data_on, pData->dummy_refresh_clock);
#else
    pHlp->pfnPrintf(pHlp, "speaker_data_on=%#x\n", pData->speaker_data_on);
#endif
}


/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int)  pitConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PITState   *pData = PDMINS2DATA(pDevIns, PITState *);
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
    if (!CFGMR3AreValuesValid(pCfgHandle, "Irq\0Base\0Speaker\0GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init the data.
     */
    rc = CFGMR3QueryU8(pCfgHandle, "Irq", &u8Irq);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u8Irq = 0;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Irq\" as a uint8_t failed"));

    rc = CFGMR3QueryU16(pCfgHandle, "Base", &u16Base);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u16Base = 0x40;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Base\" as a uint16_t failed"));

    rc = CFGMR3QueryBool(pCfgHandle, "SpeakerEnabled", &fSpeaker);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fSpeaker = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"SpeakerEnabled\" as a bool failed"));

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"GCEnabled\" as a bool failed"));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));

    pData->pDevIns = pDevIns;
    pData->channels[0].irq = u8Irq;
    for (i = 0; i < ELEMENTS(pData->channels); i++)
    {
        pData->channels[i].pPitHC = pData;
        pData->channels[i].pPitGC = PDMINS2DATA_GCPTR(pDevIns);
    }

    /*
     * Create timer, register I/O Ports and save state.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, pitTimer, "i8254 Programmable Interval Timer",
                                &pData->channels[0].CTXSUFF(pTimer));
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("pfnTMTimerCreate -> %Vrc\n", rc));
        return rc;
    }

    rc = PDMDevHlpIOPortRegister(pDevIns, u16Base, 4, NULL, pitIOPortWrite, pitIOPortRead, NULL, NULL, "i8254 Programmable Interval Timer");
    if (VBOX_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, u16Base, 4, 0, "pitIOPortWrite", "pitIOPortRead", NULL, NULL, "i8254 Programmable Interval Timer");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, u16Base, 4, 0, "pitIOPortWrite", "pitIOPortRead", NULL, NULL, "i8254 Programmable Interval Timer");
        if (VBOX_FAILURE(rc))
            return rc;
    }

    if (fSpeaker)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, 0x61, 1, NULL, pitIOPortSpeakerWrite, pitIOPortSpeakerRead, NULL, NULL, "PC Speaker");
        if (VBOX_FAILURE(rc))
            return rc;
        if (fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x61, 1, 0, NULL, "pitIOPortSpeakerRead", NULL, NULL, "PC Speaker");
            if (VBOX_FAILURE(rc))
                return rc;
        }
    }

    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, PIT_SAVED_STATE_VERSION, sizeof(*pData),
                                          NULL, pitSaveExec, NULL,
                                          NULL, pitLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Initialize the device state.
     */
    pitReset(pDevIns);

    /*
     * Register statistics and debug info.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatPITIrq,      STAMTYPE_COUNTER, "/TM/PIT/Irq",      STAMUNIT_OCCURENCES,     "The number of times a timer interrupt was triggered.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatPITHandler,  STAMTYPE_PROFILE, "/TM/PIT/Handler",  STAMUNIT_TICKS_PER_CALL, "Profiling timer callback handler.");

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
    /* szDeviceName */
    "i8254",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel 8254 Programmable Interval Timer (PIT) And Dummy Speaker Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
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
    /* pfnQueryInterface. */
    NULL
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

