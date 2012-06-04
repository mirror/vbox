/* $Id$ */
/** @file
 * Motorola MC146818 RTC/CMOS Device with PIIX4 extensions.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
 * QEMU MC146818 RTC emulation
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
#define LOG_GROUP LOG_GROUP_DEV_RTC
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/uuid.h>
#endif /* IN_RING3 */

#include "VBoxDD.h"

struct RTCState;
typedef struct RTCState RTCState;

#define RTC_CRC_START   0x10
#define RTC_CRC_LAST    0x2d
#define RTC_CRC_HIGH    0x2e
#define RTC_CRC_LOW     0x2f


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) rtcIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) rtcIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(void) rtcTimerPeriodic(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);
PDMBOTHCBDECL(void) rtcTimerSecond(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);
PDMBOTHCBDECL(void) rtcTimerSecond2(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);
RT_C_DECLS_END
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/*#define DEBUG_CMOS*/

#define RTC_SECONDS             0
#define RTC_SECONDS_ALARM       1
#define RTC_MINUTES             2
#define RTC_MINUTES_ALARM       3
#define RTC_HOURS               4
#define RTC_HOURS_ALARM         5
#define RTC_ALARM_DONT_CARE    0xC0

#define RTC_DAY_OF_WEEK         6
#define RTC_DAY_OF_MONTH        7
#define RTC_MONTH               8
#define RTC_YEAR                9

#define RTC_REG_A               10
#define RTC_REG_B               11
#define RTC_REG_C               12
#define RTC_REG_D               13

#define REG_A_UIP 0x80

#define REG_B_SET 0x80
#define REG_B_PIE 0x40
#define REG_B_AIE 0x20
#define REG_B_UIE 0x10


/** The saved state version. */
#define RTC_SAVED_STATE_VERSION             4
/** The saved state version used by VirtualBox pre-3.2.
 * This does not include the second 128-byte bank. */
#define RTC_SAVED_STATE_VERSION_VBOX_32PRE  3
/** The saved state version used by VirtualBox 3.1 and earlier.
 * This does not include disabled by HPET state. */
#define RTC_SAVED_STATE_VERSION_VBOX_31     2
/** The saved state version used by VirtualBox 3.0 and earlier.
 * This does not include the configuration.  */
#define RTC_SAVED_STATE_VERSION_VBOX_30     1


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @todo Replace struct my_tm with RTTIME. */
struct my_tm
{
    int32_t tm_sec;
    int32_t tm_min;
    int32_t tm_hour;
    int32_t tm_mday;
    int32_t tm_mon;
    int32_t tm_year;
    int32_t tm_wday;
    int32_t tm_yday;
};


struct RTCState {
    uint8_t cmos_data[256];
    uint8_t cmos_index[2];
    uint8_t Alignment0[6];
    struct my_tm current_tm;
    /** The configured IRQ. */
    int32_t irq;
    /** The configured I/O port base. */
    RTIOPORT IOPortBase;
    /** Use UTC or local time initially. */
    bool fUTC;
    /** Disabled by HPET legacy mode. */
    bool fDisabledByHpet;
    /* periodic timer */
    int64_t next_periodic_time;
    /* second update */
    int64_t next_second_time;

    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3 pDevInsR3;
    /** The periodic timer (rtcTimerPeriodic) - R3 Ptr. */
    PTMTIMERR3 pPeriodicTimerR3;
    /** The second timer (rtcTimerSecond) - R3 Ptr. */
    PTMTIMERR3 pSecondTimerR3;
    /** The second second timer (rtcTimerSecond2) - R3 Ptr. */
    PTMTIMERR3 pSecondTimer2R3;

    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0 pDevInsR0;
    /** The periodic timer (rtcTimerPeriodic) - R0 Ptr. */
    PTMTIMERR0 pPeriodicTimerR0;
    /** The second timer (rtcTimerSecond) - R0 Ptr. */
    PTMTIMERR0 pSecondTimerR0;
    /** The second second timer (rtcTimerSecond2) - R0 Ptr. */
    PTMTIMERR0 pSecondTimer2R0;

    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC pDevInsRC;
    /** The periodic timer (rtcTimerPeriodic) - RC Ptr. */
    PTMTIMERRC pPeriodicTimerRC;
    /** The second timer (rtcTimerSecond) - RC Ptr. */
    PTMTIMERRC pSecondTimerRC;
    /** The second second timer (rtcTimerSecond2) - RC Ptr. */
    PTMTIMERRC pSecondTimer2RC;

    /** The RTC registration structure. */
    PDMRTCREG RtcReg;
    /** The RTC device helpers. */
    R3PTRTYPE(PCPDMRTCHLP) pRtcHlpR3;
    /** Number of release log entries. Used to prevent flooding. */
    uint32_t cRelLogEntries;
    /** The current/previous logged timer period. */
    int32_t CurLogPeriod;
    /** The current/previous hinted timer period. */
    int32_t CurHintPeriod;
    uint32_t u32AlignmentPadding;

    /** HPET legacy mode notification interface. */
    PDMIHPETLEGACYNOTIFY  IHpetLegacyNotify;
};

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

static void rtc_timer_update(RTCState *pThis, int64_t current_time)
{
    int period_code, period;
    uint64_t cur_clock, next_irq_clock;
    uint32_t freq;

    Assert(TMTimerIsLockOwner(pThis->CTX_SUFF(pPeriodicTimer)));
    Assert(PDMCritSectIsOwner(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo)));

    period_code = pThis->cmos_data[RTC_REG_A] & 0x0f;
    if (   period_code != 0
        && (pThis->cmos_data[RTC_REG_B] & REG_B_PIE))
    {
        if (period_code <= 2)
            period_code += 7;
        /* period in 32 kHz cycles */
        period = 1 << (period_code - 1);
        /* compute 32 kHz clock */
        freq = TMTimerGetFreq(pThis->CTX_SUFF(pPeriodicTimer));

        cur_clock = ASMMultU64ByU32DivByU32(current_time, 32768, freq);
        next_irq_clock = (cur_clock & ~(uint64_t)(period - 1)) + period;
        pThis->next_periodic_time = ASMMultU64ByU32DivByU32(next_irq_clock, freq, 32768) + 1;
        TMTimerSet(pThis->CTX_SUFF(pPeriodicTimer), pThis->next_periodic_time);

#ifdef IN_RING3
        if (RT_UNLIKELY(period != pThis->CurLogPeriod))
#else
        if (RT_UNLIKELY(period != pThis->CurHintPeriod))
#endif
        {
#ifdef IN_RING3
            if (pThis->cRelLogEntries++ < 64)
                LogRel(("RTC: period=%#x (%d) %u Hz\n", period, period, _32K / period));
            pThis->CurLogPeriod  = period;
#endif
            pThis->CurHintPeriod = period;
            TMTimerSetFrequencyHint(pThis->CTX_SUFF(pPeriodicTimer), _32K / period);
        }
    }
    else
    {
        if (TMTimerIsActive(pThis->CTX_SUFF(pPeriodicTimer)) && pThis->cRelLogEntries++ < 64)
            LogRel(("RTC: stopped the periodic timer\n"));
        TMTimerStop(pThis->CTX_SUFF(pPeriodicTimer));
    }
}


static void rtc_raise_irq(RTCState* pThis, uint32_t iLevel)
{
    if (!pThis->fDisabledByHpet)
        PDMDevHlpISASetIrq(pThis->CTX_SUFF(pDevIns), pThis->irq, iLevel);
}


DECLINLINE(int) to_bcd(RTCState *pThis, int a)
{
    if (pThis->cmos_data[RTC_REG_B] & 0x04)
        return a;
    return ((a / 10) << 4) | (a % 10);
}


DECLINLINE(int) from_bcd(RTCState *pThis, int a)
{
    if (pThis->cmos_data[RTC_REG_B] & 0x04)
        return a;
    return ((a >> 4) * 10) + (a & 0x0f);
}


static void rtc_set_time(RTCState *pThis)
{
    struct my_tm *tm = &pThis->current_tm;

    tm->tm_sec  = from_bcd(pThis, pThis->cmos_data[RTC_SECONDS]);
    tm->tm_min  = from_bcd(pThis, pThis->cmos_data[RTC_MINUTES]);
    tm->tm_hour = from_bcd(pThis, pThis->cmos_data[RTC_HOURS] & 0x7f);
    if (   !(pThis->cmos_data[RTC_REG_B] & 0x02)
        && (pThis->cmos_data[RTC_HOURS] & 0x80))
        tm->tm_hour += 12;
    tm->tm_wday = from_bcd(pThis, pThis->cmos_data[RTC_DAY_OF_WEEK]);
    tm->tm_mday = from_bcd(pThis, pThis->cmos_data[RTC_DAY_OF_MONTH]);
    tm->tm_mon  = from_bcd(pThis, pThis->cmos_data[RTC_MONTH]) - 1;
    tm->tm_year = from_bcd(pThis, pThis->cmos_data[RTC_YEAR]) + 100;
}


/* -=-=-=-=-=- I/O Port Handlers -=-=-=-=-=- */


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) rtcIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    if ((Port & 1) == 0)
        *pu32 = 0xff;
    else
    {
        unsigned bank = (Port >> 1) & 1;
        switch (pThis->cmos_index[bank])
        {
            case RTC_SECONDS:
            case RTC_MINUTES:
            case RTC_HOURS:
            case RTC_DAY_OF_WEEK:
            case RTC_DAY_OF_MONTH:
            case RTC_MONTH:
            case RTC_YEAR:
                *pu32 = pThis->cmos_data[pThis->cmos_index[0]];
                break;

            case RTC_REG_A:
                *pu32 = pThis->cmos_data[pThis->cmos_index[0]];
                break;

            case RTC_REG_C:
                *pu32 = pThis->cmos_data[pThis->cmos_index[0]];
                rtc_raise_irq(pThis, 0);
                pThis->cmos_data[RTC_REG_C] = 0x00;
                break;

            default:
                *pu32 = pThis->cmos_data[pThis->cmos_index[bank]];
                break;
        }

        Log(("CMOS: Read bank %d idx %#04x: %#04x\n", bank, pThis->cmos_index[bank], *pu32));
    }

    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) rtcIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb != 1)
        return VINF_SUCCESS;

    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    uint32_t bank = (Port >> 1) & 1;
    if ((Port & 1) == 0)
    {
        pThis->cmos_index[bank] = (u32 & 0x7f) + (bank * 128);
    }
    else
    {
        Log(("CMOS: Write bank %d idx %#04x: %#04x (old %#04x)\n", bank,
             pThis->cmos_index[bank], u32, pThis->cmos_data[pThis->cmos_index[bank]]));

        int const idx = pThis->cmos_index[bank];
        switch (idx)
        {
            case RTC_SECONDS_ALARM:
            case RTC_MINUTES_ALARM:
            case RTC_HOURS_ALARM:
                pThis->cmos_data[pThis->cmos_index[0]] = u32;
                break;

            case RTC_SECONDS:
            case RTC_MINUTES:
            case RTC_HOURS:
            case RTC_DAY_OF_WEEK:
            case RTC_DAY_OF_MONTH:
            case RTC_MONTH:
            case RTC_YEAR:
                pThis->cmos_data[pThis->cmos_index[0]] = u32;
                /* if in set mode, do not update the time */
                if (!(pThis->cmos_data[RTC_REG_B] & REG_B_SET))
                    rtc_set_time(pThis);
                break;

            case RTC_REG_A:
            case RTC_REG_B:
            {
                /* We need to acquire the clock lock, because of lock ordering
                   issues this means having to release the device lock.  Since
                   we're letting IOM do the locking, we must not return without
                   holding the device lock.*/
                PDMCritSectLeave(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo));
                int rc1 = TMTimerLock(pThis->CTX_SUFF(pPeriodicTimer), VINF_SUCCESS /* must get it */);
                int rc2 = PDMCritSectEnter(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo), VINF_SUCCESS /* must get it */);
                AssertRCReturn(rc1, rc1);
                AssertRCReturnStmt(rc2, TMTimerUnlock(pThis->CTX_SUFF(pPeriodicTimer)), rc2);

                if (idx == RTC_REG_A)
                {
                    /* UIP bit is read only */
                    pThis->cmos_data[RTC_REG_A] = (u32                        & ~REG_A_UIP)
                                                | (pThis->cmos_data[RTC_REG_A] & REG_A_UIP);
                }
                else
                {
                    if (u32 & REG_B_SET)
                    {
                        /* set mode: reset UIP mode */
                        pThis->cmos_data[RTC_REG_A] &= ~REG_A_UIP;
#if 0 /* This is probably wrong as it breaks changing the time/date in OS/2. */
                        u32 &= ~REG_B_UIE;
#endif
                    }
                    else
                    {
                        /* if disabling set mode, update the time */
                        if (pThis->cmos_data[RTC_REG_B] & REG_B_SET)
                            rtc_set_time(pThis);
                    }
                    pThis->cmos_data[RTC_REG_B] = u32;
                }

                rtc_timer_update(pThis, TMTimerGet(pThis->CTX_SUFF(pPeriodicTimer)));

                TMTimerUnlock(pThis->CTX_SUFF(pPeriodicTimer));
                /* the caller leaves the other lock. */
                break;
            }

            case RTC_REG_C:
            case RTC_REG_D:
                /* cannot write to them */
                break;

            default:
                pThis->cmos_data[pThis->cmos_index[bank]] = u32;
                break;
        }
    }

    return VINF_SUCCESS;
}

#ifdef IN_RING3

/* -=-=-=-=-=- Timers and their support code  -=-=-=-=-=- */


/**
 * Device timer callback function, periodic.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the RTC state.
 */
static DECLCALLBACK(void) rtcTimerPeriodic(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    Assert(TMTimerIsLockOwner(pThis->CTX_SUFF(pPeriodicTimer)));
    Assert(PDMCritSectIsOwner(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo)));

    rtc_timer_update(pThis, pThis->next_periodic_time);
    pThis->cmos_data[RTC_REG_C] |= 0xc0;

    rtc_raise_irq(pThis, 1);
}


/* month is between 0 and 11. */
static int get_days_in_month(int month, int year)
{
    static const int days_tab[12] =
    {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int d;

    if ((unsigned )month >= 12)
        return 31;

    d = days_tab[month];
    if (month == 1)
    {
        if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0))
            d++;
    }
    return d;
}


/* update 'tm' to the next second */
static void rtc_next_second(struct my_tm *tm)
{
    int days_in_month;

    tm->tm_sec++;
    if ((unsigned)tm->tm_sec >= 60)
    {
        tm->tm_sec = 0;
        tm->tm_min++;
        if ((unsigned)tm->tm_min >= 60)
        {
            tm->tm_min = 0;
            tm->tm_hour++;
            if ((unsigned)tm->tm_hour >= 24)
            {
                tm->tm_hour = 0;
                /* next day */
                tm->tm_wday++;
                if ((unsigned)tm->tm_wday >= 7)
                    tm->tm_wday = 0;
                days_in_month = get_days_in_month(tm->tm_mon,
                                                  tm->tm_year + 1900);
                tm->tm_mday++;
                if (tm->tm_mday < 1)
                    tm->tm_mday = 1;
                else if (tm->tm_mday > days_in_month)
                {
                    tm->tm_mday = 1;
                    tm->tm_mon++;
                    if (tm->tm_mon >= 12)
                    {
                        tm->tm_mon = 0;
                        tm->tm_year++;
                    }
                }
            }
        }
    }
}


/**
 * Device timer callback function, second.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the RTC state.
 */
static DECLCALLBACK(void) rtcTimerSecond(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    Assert(TMTimerIsLockOwner(pThis->CTX_SUFF(pPeriodicTimer)));
    Assert(PDMCritSectIsOwner(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo)));

    /* if the oscillator is not in normal operation, we do not update */
    if ((pThis->cmos_data[RTC_REG_A] & 0x70) != 0x20)
    {
        pThis->next_second_time += TMTimerGetFreq(pThis->CTX_SUFF(pSecondTimer));
        TMTimerSet(pThis->CTX_SUFF(pSecondTimer), pThis->next_second_time);
    }
    else
    {
        rtc_next_second(&pThis->current_tm);

        if (!(pThis->cmos_data[RTC_REG_B] & REG_B_SET))
        {
            /* update in progress bit */
            Log2(("RTC: UIP %x -> 1\n", !!(pThis->cmos_data[RTC_REG_A] & REG_A_UIP)));
            pThis->cmos_data[RTC_REG_A] |= REG_A_UIP;
        }

        /* 244140 ns = 8 / 32768 seconds */
        uint64_t delay = TMTimerFromNano(pThis->CTX_SUFF(pSecondTimer2), 244140);
        TMTimerSet(pThis->CTX_SUFF(pSecondTimer2), pThis->next_second_time + delay);
    }
}


/* Used by rtc_set_date and rtcTimerSecond2. */
static void rtc_copy_date(RTCState *pThis)
{
    const struct my_tm *tm = &pThis->current_tm;

    pThis->cmos_data[RTC_SECONDS] = to_bcd(pThis, tm->tm_sec);
    pThis->cmos_data[RTC_MINUTES] = to_bcd(pThis, tm->tm_min);
    if (pThis->cmos_data[RTC_REG_B] & 0x02)
    {
        /* 24 hour format */
        pThis->cmos_data[RTC_HOURS] = to_bcd(pThis, tm->tm_hour);
    }
    else
    {
        /* 12 hour format */
        pThis->cmos_data[RTC_HOURS] = to_bcd(pThis, tm->tm_hour % 12);
        if (tm->tm_hour >= 12)
            pThis->cmos_data[RTC_HOURS] |= 0x80;
    }
    pThis->cmos_data[RTC_DAY_OF_WEEK] = to_bcd(pThis, tm->tm_wday);
    pThis->cmos_data[RTC_DAY_OF_MONTH] = to_bcd(pThis, tm->tm_mday);
    pThis->cmos_data[RTC_MONTH] = to_bcd(pThis, tm->tm_mon + 1);
    pThis->cmos_data[RTC_YEAR] = to_bcd(pThis, tm->tm_year % 100);
}


/**
 * Device timer callback function, second2.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 * @param   pvUser          Pointer to the RTC state.
 */
static DECLCALLBACK(void) rtcTimerSecond2(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    Assert(TMTimerIsLockOwner(pThis->CTX_SUFF(pPeriodicTimer)));
    Assert(PDMCritSectIsOwner(pThis->CTX_SUFF(pDevIns)->CTX_SUFF(pCritSectRo)));

    if (!(pThis->cmos_data[RTC_REG_B] & REG_B_SET))
        rtc_copy_date(pThis);

    /* check alarm */
    if (pThis->cmos_data[RTC_REG_B] & REG_B_AIE)
    {
        if (   (   (pThis->cmos_data[RTC_SECONDS_ALARM] & 0xc0) == 0xc0
                || from_bcd(pThis, pThis->cmos_data[RTC_SECONDS_ALARM]) == pThis->current_tm.tm_sec)
            && (   (pThis->cmos_data[RTC_MINUTES_ALARM] & 0xc0) == 0xc0
                || from_bcd(pThis, pThis->cmos_data[RTC_MINUTES_ALARM]) == pThis->current_tm.tm_min)
            && (   (pThis->cmos_data[RTC_HOURS_ALARM  ] & 0xc0) == 0xc0
                || from_bcd(pThis, pThis->cmos_data[RTC_HOURS_ALARM  ]) == pThis->current_tm.tm_hour)
            )
        {
            pThis->cmos_data[RTC_REG_C] |= 0xa0;
            rtc_raise_irq(pThis, 1);
        }
    }

    /* update ended interrupt */
    if (pThis->cmos_data[RTC_REG_B] & REG_B_UIE)
    {
        pThis->cmos_data[RTC_REG_C] |= 0x90;
        rtc_raise_irq(pThis, 1);
    }

    /* clear update in progress bit */
    Log2(("RTC: UIP %x -> 0\n", !!(pThis->cmos_data[RTC_REG_A] & REG_A_UIP)));
    pThis->cmos_data[RTC_REG_A] &= ~REG_A_UIP;

    pThis->next_second_time += TMTimerGetFreq(pThis->CTX_SUFF(pSecondTimer));
    TMTimerSet(pThis->CTX_SUFF(pSecondTimer), pThis->next_second_time);
}


/* -=-=-=-=-=- Saved State -=-=-=-=-=- */


/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) rtcLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);

    SSMR3PutU8(    pSSM, pThis->irq);
    SSMR3PutIOPort(pSSM, pThis->IOPortBase);
    SSMR3PutBool(  pSSM, pThis->fUTC);

    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) rtcSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);

    /* The config. */
    rtcLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    /* The state. */
    SSMR3PutMem(pSSM, pThis->cmos_data, 128);
    SSMR3PutU8(pSSM, pThis->cmos_index[0]);

    SSMR3PutS32(pSSM, pThis->current_tm.tm_sec);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_min);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_hour);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_wday);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_mday);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_mon);
    SSMR3PutS32(pSSM, pThis->current_tm.tm_year);

    TMR3TimerSave(pThis->CTX_SUFF(pPeriodicTimer), pSSM);

    SSMR3PutS64(pSSM, pThis->next_periodic_time);

    SSMR3PutS64(pSSM, pThis->next_second_time);
    TMR3TimerSave(pThis->CTX_SUFF(pSecondTimer), pSSM);
    TMR3TimerSave(pThis->CTX_SUFF(pSecondTimer2), pSSM);

    SSMR3PutBool(pSSM, pThis->fDisabledByHpet);

    SSMR3PutMem(pSSM, &pThis->cmos_data[128], 128);
    return SSMR3PutU8(pSSM, pThis->cmos_index[1]);
}


/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) rtcLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    RTCState   *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    int         rc;

    if (    uVersion != RTC_SAVED_STATE_VERSION
        &&  uVersion != RTC_SAVED_STATE_VERSION_VBOX_32PRE
        &&  uVersion != RTC_SAVED_STATE_VERSION_VBOX_31
        &&  uVersion != RTC_SAVED_STATE_VERSION_VBOX_30)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* The config. */
    if (uVersion > RTC_SAVED_STATE_VERSION_VBOX_30)
    {
        uint8_t u8Irq;
        rc = SSMR3GetU8(pSSM, &u8Irq);          AssertRCReturn(rc, rc);
        if (u8Irq != pThis->irq)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - u8Irq: saved=%#x config=%#x"), u8Irq, pThis->irq);

        RTIOPORT IOPortBase;
        rc = SSMR3GetIOPort(pSSM, &IOPortBase); AssertRCReturn(rc, rc);
        if (IOPortBase != pThis->IOPortBase)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - IOPortBase: saved=%RTiop config=%RTiop"), IOPortBase, pThis->IOPortBase);

        bool fUTC;
        rc = SSMR3GetBool(pSSM, &fUTC);         AssertRCReturn(rc, rc);
        if (fUTC != pThis->fUTC)
            LogRel(("RTC: Config mismatch - fUTC: saved=%RTbool config=%RTbool\n", fUTC, pThis->fUTC));
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* The state. */
    SSMR3GetMem(pSSM, pThis->cmos_data, 128);
    SSMR3GetU8(pSSM, &pThis->cmos_index[0]);

    SSMR3GetS32(pSSM, &pThis->current_tm.tm_sec);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_min);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_hour);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_wday);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_mday);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_mon);
    SSMR3GetS32(pSSM, &pThis->current_tm.tm_year);

    TMR3TimerLoad(pThis->CTX_SUFF(pPeriodicTimer), pSSM);

    SSMR3GetS64(pSSM, &pThis->next_periodic_time);

    SSMR3GetS64(pSSM, &pThis->next_second_time);
    TMR3TimerLoad(pThis->CTX_SUFF(pSecondTimer), pSSM);
    TMR3TimerLoad(pThis->CTX_SUFF(pSecondTimer2), pSSM);

    if (uVersion > RTC_SAVED_STATE_VERSION_VBOX_31)
         SSMR3GetBool(pSSM, &pThis->fDisabledByHpet);

    if (uVersion > RTC_SAVED_STATE_VERSION_VBOX_32PRE)
    {
        /* Second CMOS bank. */
        SSMR3GetMem(pSSM, &pThis->cmos_data[128], 128);
        SSMR3GetU8(pSSM, &pThis->cmos_index[1]);
    }

    int period_code = pThis->cmos_data[RTC_REG_A] & 0x0f;
    if (    period_code != 0
        &&  (pThis->cmos_data[RTC_REG_B] & REG_B_PIE))
    {
        if (period_code <= 2)
            period_code += 7;
        int period = 1 << (period_code - 1);
        LogRel(("RTC: period=%#x (%d) %u Hz (restore)\n", period, period, _32K / period));
        PDMCritSectEnter(pThis->pDevInsR3->pCritSectRoR3, VINF_SUCCESS);
        TMTimerSetFrequencyHint(pThis->CTX_SUFF(pPeriodicTimer), _32K / period);
        PDMCritSectLeave(pThis->pDevInsR3->pCritSectRoR3);
        pThis->CurLogPeriod  = period;
        pThis->CurHintPeriod = period;
    }
    else
    {
        LogRel(("RTC: stopped the periodic timer (restore)\n"));
        pThis->CurLogPeriod  = 0;
        pThis->CurHintPeriod = 0;
    }
    pThis->cRelLogEntries = 0;

    return VINF_SUCCESS;
}


/* -=-=-=-=-=- PDM Interface provided by the RTC device  -=-=-=-=-=- */

/**
 * Calculate and update the standard CMOS checksum.
 *
 * @param   pThis       Pointer to the RTC state data.
 */
static void rtcCalcCRC(RTCState *pThis)
{
    uint16_t u16;
    unsigned i;

    for (i = RTC_CRC_START, u16 = 0; i <= RTC_CRC_LAST; i++)
        u16 += pThis->cmos_data[i];
    pThis->cmos_data[RTC_CRC_LOW]  = u16 & 0xff;
    pThis->cmos_data[RTC_CRC_HIGH] = (u16 >> 8) & 0xff;
}


/**
 * Write to a CMOS register and update the checksum if necessary.
 *
 * @returns VBox status code.
 * @param   pDevIns     Device instance of the RTC.
 * @param   iReg        The CMOS register index; bit 8 determines bank.
 * @param   u8Value     The CMOS register value.
 */
static DECLCALLBACK(int) rtcCMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    if (iReg < RT_ELEMENTS(pThis->cmos_data))
    {
        PDMCritSectEnter(pDevIns->pCritSectRoR3, VERR_IGNORED);

        pThis->cmos_data[iReg] = u8Value;

        /* does it require checksum update? */
        if (    iReg >= RTC_CRC_START
            &&  iReg <= RTC_CRC_LAST)
            rtcCalcCRC(pThis);

        PDMCritSectLeave(pDevIns->pCritSectRoR3);
        return VINF_SUCCESS;
    }

    AssertMsgFailed(("iReg=%d\n", iReg));
    return VERR_INVALID_PARAMETER;
}


/**
 * Read a CMOS register.
 *
 * @returns VBox status code.
 * @param   pDevIns     Device instance of the RTC.
 * @param   iReg        The CMOS register index; bit 8 determines bank.
 * @param   pu8Value    Where to store the CMOS register value.
 */
static DECLCALLBACK(int) rtcCMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    RTCState   *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    if (iReg < RT_ELEMENTS(pThis->cmos_data))
    {
        PDMCritSectEnter(pDevIns->pCritSectRoR3, VERR_IGNORED);

        *pu8Value = pThis->cmos_data[iReg];

        PDMCritSectLeave(pDevIns->pCritSectRoR3);
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("iReg=%d\n", iReg));
    return VERR_INVALID_PARAMETER;
}


/**
 * @interface_method_impl{PDMIHPETLEGACYNOTIFY,pfnModeChanged}
 */
static DECLCALLBACK(void) rtcHpetLegacyNotify_ModeChanged(PPDMIHPETLEGACYNOTIFY pInterface, bool fActivated)
{
    RTCState *pThis = RT_FROM_MEMBER(pInterface, RTCState, IHpetLegacyNotify);
    PDMCritSectEnter(pThis->pDevInsR3->pCritSectRoR3, VERR_IGNORED);

    pThis->fDisabledByHpet = fActivated;

    PDMCritSectLeave(pThis->pDevInsR3->pCritSectRoR3);
}


/* -=-=-=-=-=- based on bits from pc.c -=-=-=-=-=- */


static void rtc_set_memory(RTCState *pThis, int addr, int val)
{
    if (addr >= 0 && addr <= 127)
        pThis->cmos_data[addr] = val;
}


static void rtc_set_date(RTCState *pThis, const struct my_tm *tm)
{
    pThis->current_tm = *tm;
    rtc_copy_date(pThis);
}


/** @copydoc FNPDMDEVINITCOMPLETE */
static DECLCALLBACK(int)  rtcInitComplete(PPDMDEVINS pDevIns)
{
    /** @todo this should be (re)done at power on if we didn't load a state... */
    RTCState   *pThis = PDMINS_2_DATA(pDevIns, RTCState *);

    /*
     * Set the CMOS date/time.
     */
    RTTIMESPEC  Now;
    PDMDevHlpTMUtcNow(pDevIns, &Now);
    RTTIME Time;
    if (pThis->fUTC)
        RTTimeExplode(&Time, &Now);
    else
        RTTimeLocalExplode(&Time, &Now);

    struct my_tm Tm;
    memset(&Tm, 0, sizeof(Tm));
    Tm.tm_year = Time.i32Year - 1900;
    Tm.tm_mon  = Time.u8Month - 1;
    Tm.tm_mday = Time.u8MonthDay;
    Tm.tm_wday = (Time.u8WeekDay + 1 + 7) % 7; /* 0 = Monday -> Sunday */
    Tm.tm_yday = Time.u16YearDay - 1;
    Tm.tm_hour = Time.u8Hour;
    Tm.tm_min  = Time.u8Minute;
    Tm.tm_sec  = Time.u8Second;

    rtc_set_date(pThis, &Tm);

    int iYear = to_bcd(pThis, (Tm.tm_year / 100) + 19); /* tm_year is 1900 based */
    rtc_set_memory(pThis, 0x32, iYear);                 /* 32h - Century Byte (BCD value for the century */
    rtc_set_memory(pThis, 0x37, iYear);                 /* 37h - (IBM PS/2) Date Century Byte */

    /*
     * Recalculate the checksum just in case.
     */
    rtcCalcCRC(pThis);

    Log(("CMOS bank 0: \n%16.128Rhxd\n", &pThis->cmos_data[0]));
    Log(("CMOS bank 1: \n%16.128Rhxd\n", &pThis->cmos_data[128]));
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- real code -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) rtcQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDEVINS  pDevIns = RT_FROM_MEMBER(pInterface, PDMDEVINS, IBase);
    RTCState   *pThis   = PDMINS_2_DATA(pDevIns, RTCState *);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,             &pDevIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHPETLEGACYNOTIFY, &pThis->IHpetLegacyNotify);
    return NULL;
}

/**
 * @copydoc
 */
static DECLCALLBACK(void) rtcRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);

    pThis->pDevInsRC        = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pPeriodicTimerRC = TMTimerRCPtr(pThis->pPeriodicTimerR3);
    pThis->pSecondTimerRC   = TMTimerRCPtr(pThis->pSecondTimerR3);
    pThis->pSecondTimer2RC  = TMTimerRCPtr(pThis->pSecondTimer2R3);
}


/**
 * @copydoc
 */
static DECLCALLBACK(void) rtcReset(PPDMDEVINS pDevIns)
{
    RTCState *pThis = PDMINS_2_DATA(pDevIns, RTCState *);

    /* If shutdown status is non-zero, log its value. */
    if (pThis->cmos_data[0xF])
    {
        LogRel(("CMOS shutdown status byte is %02X\n", pThis->cmos_data[0xF]));

#if 0   /* It would be nice to log the warm reboot vector but alas, we already trashed it. */
        uint32_t u32WarmVector;
        int rc;
        rc = PDMDevHlpPhysRead(pDevIns, 0x467, &u32WarmVector, sizeof(u32WarmVector));
        AssertRC(rc);
        LogRel((", 40:67 contains %04X:%04X\n", u32WarmVector >> 16, u32WarmVector & 0xFFFF));
#endif
        /* If we're going to trash the VM's memory, we also have to clear this. */
        pThis->cmos_data[0xF] = 0;
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)  rtcConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RTCState   *pThis = PDMINS_2_DATA(pDevIns, RTCState *);
    int         rc;
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "Irq\0"
                              "Base\0"
                              "UseUTC\0"
                              "GCEnabled\0"
                              "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init the data.
     */
    uint8_t u8Irq;
    rc = CFGMR3QueryU8Def(pCfg, "Irq", &u8Irq, 8);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Irq\" as a uint8_t failed"));
    pThis->irq = u8Irq;

    rc = CFGMR3QueryPortDef(pCfg, "Base", &pThis->IOPortBase, 0x70);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Base\" as a RTIOPORT failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "UseUTC", &pThis->fUTC, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"UseUTC\" as a bool failed"));

    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read GCEnabled as boolean"));

    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));

    Log(("RTC: Irq=%#x Base=%#x fGCEnabled=%RTbool fR0Enabled=%RTbool\n",
         u8Irq, pThis->IOPortBase, fGCEnabled, fR0Enabled));


    pThis->pDevInsR3            = pDevIns;
    pThis->pDevInsR0            = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC            = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->cmos_data[RTC_REG_A] = 0x26;
    pThis->cmos_data[RTC_REG_B] = 0x02;
    pThis->cmos_data[RTC_REG_C] = 0x00;
    pThis->cmos_data[RTC_REG_D] = 0x80;
    pThis->RtcReg.u32Version    = PDM_RTCREG_VERSION;
    pThis->RtcReg.pfnRead       = rtcCMOSRead;
    pThis->RtcReg.pfnWrite      = rtcCMOSWrite;
    pThis->fDisabledByHpet      = false;

    /* IBase */
    pDevIns->IBase.pfnQueryInterface        = rtcQueryInterface;
    /* IHpetLegacyNotify */
    pThis->IHpetLegacyNotify.pfnModeChanged = rtcHpetLegacyNotify_ModeChanged;

    /*
     * Create timers.
     */
    PTMTIMER pTimer;
    /* Periodic timer. */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, rtcTimerPeriodic, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "MC146818 RTC/CMOS - Periodic",
                                &pTimer);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pPeriodicTimerR3 = pTimer;
    pThis->pPeriodicTimerR0 = TMTimerR0Ptr(pTimer);
    pThis->pPeriodicTimerRC = TMTimerRCPtr(pTimer);

    /* Seconds timer. */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, rtcTimerSecond, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "MC146818 RTC/CMOS - Second",
                                &pTimer);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pSecondTimerR3 = pTimer;
    pThis->pSecondTimerR0 = TMTimerR0Ptr(pTimer);
    pThis->pSecondTimerRC = TMTimerRCPtr(pTimer);

    /* The second2 timer, this is always active. */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, rtcTimerSecond2, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "MC146818 RTC/CMOS - Second2",
                                &pTimer);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pSecondTimer2R3  = pTimer;
    pThis->pSecondTimer2R0  = TMTimerR0Ptr(pTimer);
    pThis->pSecondTimer2RC  = TMTimerRCPtr(pTimer);
    pThis->next_second_time = TMTimerGet(pTimer)
                            + (TMTimerGetFreq(pTimer) * 99) / 100;
    rc = TMTimerLock(pTimer, VERR_IGNORED);
    AssertRCReturn(rc, rc);
    rc = TMTimerSet(pTimer, pThis->next_second_time);
    TMTimerUnlock(pTimer);
    AssertRCReturn(rc, rc);

    /*
     * Register I/O ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, pThis->IOPortBase, 4, NULL,
                                 rtcIOPortWrite, rtcIOPortRead, NULL, NULL, "MC146818 RTC/CMOS");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, pThis->IOPortBase, 4, NIL_RTRCPTR,
                                       "rtcIOPortWrite", "rtcIOPortRead", NULL, NULL, "MC146818 RTC/CMOS");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, pThis->IOPortBase, 4, NIL_RTR0PTR,
                                       "rtcIOPortWrite", "rtcIOPortRead", NULL, NULL, "MC146818 RTC/CMOS");
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Register the saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, RTC_SAVED_STATE_VERSION, sizeof(*pThis), rtcLiveExec, rtcSaveExec, rtcLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register ourselves as the RTC/CMOS with PDM.
     */
    rc = PDMDevHlpRTCRegister(pDevIns, &pThis->RtcReg, &pThis->pRtcHlpR3);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceMC146818 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "mc146818",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Motorola MC146818 RTC/CMOS Device.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_RTC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(RTCState),
    /* pfnConstruct */
    rtcConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    rtcRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    rtcReset,
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
    rtcInitComplete,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

