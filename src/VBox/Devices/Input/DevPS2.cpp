/* $Id$ */
/** @file
 * DevPS2 - PS/2 keyboard & mouse controller device.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
 * QEMU PC keyboard emulation (revision 1.12)
 *
 * Copyright (c) 2003 Fabrice Bellard
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_KBD
#include "vl_vbox.h"
#include <VBox/vmm/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#define PCKBD_SAVED_STATE_VERSION 5


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) kbdIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortStatusRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortCommandWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
RT_C_DECLS_END
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

/* debug PC keyboard */
#define DEBUG_KBD

/* debug PC keyboard : only mouse */
#define DEBUG_MOUSE

/*      Keyboard Controller Commands */
#define KBD_CCMD_READ_MODE      0x20    /* Read mode bits */
#define KBD_CCMD_WRITE_MODE     0x60    /* Write mode bits */
#define KBD_CCMD_GET_VERSION    0xA1    /* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE  0xA7    /* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE   0xA8    /* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE     0xA9    /* Mouse interface test */
#define KBD_CCMD_SELF_TEST      0xAA    /* Controller self test */
#define KBD_CCMD_KBD_TEST       0xAB    /* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE    0xAD    /* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE     0xAE    /* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT    0xC0    /* read input port */
#define KBD_CCMD_READ_OUTPORT   0xD0    /* read output port */
#define KBD_CCMD_WRITE_OUTPORT  0xD1    /* write output port */
#define KBD_CCMD_WRITE_OBUF     0xD2
#define KBD_CCMD_WRITE_AUX_OBUF 0xD3    /* Write to output buffer as if
                                           initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE    0xD4    /* Write the following byte to the mouse */
#define KBD_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define KBD_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define KBD_CCMD_READ_TSTINP    0xE0    /* Read test inputs T0, T1 */
#define KBD_CCMD_RESET          0xFE

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS        0xED    /* Set keyboard leds */
#define KBD_CMD_ECHO            0xEE
#define KBD_CMD_SCANCODE        0xF0    /* Get/set scancode set */
#define KBD_CMD_GET_ID          0xF2    /* get keyboard ID */
#define KBD_CMD_SET_RATE        0xF3    /* Set typematic rate */
#define KBD_CMD_ENABLE          0xF4    /* Enable scanning */
#define KBD_CMD_RESET_DISABLE   0xF5    /* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE    0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET           0xFF    /* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR           0xAA    /* Power on reset */
#define KBD_REPLY_ACK           0xFA    /* Command ACK */
#define KBD_REPLY_RESEND        0xFE    /* Command NACK, send the cmd again */

/* Status Register Bits */
#define KBD_STAT_OBF            0x01    /* Keyboard output buffer full */
#define KBD_STAT_IBF            0x02    /* Keyboard input buffer full */
#define KBD_STAT_SELFTEST       0x04    /* Self test successful */
#define KBD_STAT_CMD            0x08    /* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED       0x10    /* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF      0x20    /* Mouse output buffer full */
#define KBD_STAT_GTO            0x40    /* General receive/xmit timeout */
#define KBD_STAT_PERR           0x80    /* Parity error */

/* Controller Mode Register Bits */
#define KBD_MODE_KBD_INT        0x01    /* Keyboard data generate IRQ1 */
#define KBD_MODE_MOUSE_INT      0x02    /* Mouse data generate IRQ12 */
#define KBD_MODE_SYS            0x04    /* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK     0x08    /* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD    0x10    /* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE  0x20    /* Disable mouse interface */
#define KBD_MODE_KCC            0x40    /* Scan code conversion to PC format */
#define KBD_MODE_RFU            0x80

/* Mouse Commands */
#define AUX_SET_SCALE11         0xE6    /* Set 1:1 scaling */
#define AUX_SET_SCALE21         0xE7    /* Set 2:1 scaling */
#define AUX_SET_RES             0xE8    /* Set resolution */
#define AUX_GET_SCALE           0xE9    /* Get scaling factor */
#define AUX_SET_STREAM          0xEA    /* Set stream mode */
#define AUX_POLL                0xEB    /* Poll */
#define AUX_RESET_WRAP          0xEC    /* Reset wrap mode */
#define AUX_SET_WRAP            0xEE    /* Set wrap mode */
#define AUX_SET_REMOTE          0xF0    /* Set remote mode */
#define AUX_GET_TYPE            0xF2    /* Get type */
#define AUX_SET_SAMPLE          0xF3    /* Set sample rate */
#define AUX_ENABLE_DEV          0xF4    /* Enable aux device */
#define AUX_DISABLE_DEV         0xF5    /* Disable aux device */
#define AUX_SET_DEFAULT         0xF6
#define AUX_RESET               0xFF    /* Reset aux device */
#define AUX_ACK                 0xFA    /* Command byte ACK. */
#define AUX_NACK                0xFE    /* Command byte NACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define KBD_QUEUE_SIZE 256

/** Supported mouse protocols */
enum
{
    MOUSE_PROT_PS2 = 0,
    MOUSE_PROT_IMPS2 = 3,
    MOUSE_PROT_IMEX = 4
};

/** @name Mouse flags */
/** @{ */
/** IMEX horizontal scroll-wheel mode is active */
# define MOUSE_REPORT_HORIZONTAL  0x01
/** @} */

typedef struct {
    uint8_t data[KBD_QUEUE_SIZE];
    int rptr, wptr, count;
} KBDQueue;

#define MOUSE_CMD_QUEUE_SIZE 8

typedef struct {
    uint8_t data[MOUSE_CMD_QUEUE_SIZE];
    int rptr, wptr, count;
} MouseCmdQueue;


#define MOUSE_EVENT_QUEUE_SIZE 256

typedef struct {
    uint8_t data[MOUSE_EVENT_QUEUE_SIZE];
    int rptr, wptr, count;
} MouseEventQueue;

typedef struct KBDState {
    KBDQueue queue;
    MouseCmdQueue mouse_command_queue;
    MouseEventQueue mouse_event_queue;
    uint8_t write_cmd; /* if non zero, write data to port 60 is expected */
    uint8_t status;
    uint8_t mode;
    uint8_t dbbout;    /* data buffer byte */
    /* keyboard state */
    int32_t kbd_write_cmd;
    int32_t scan_enabled;
    int32_t translate;
    int32_t scancode_set;   /* 1=XT, 2=AT, 3=PS/2 */
    /* mouse state */
    int32_t mouse_write_cmd;
    uint8_t mouse_status;
    uint8_t mouse_resolution;
    uint8_t mouse_sample_rate;
    uint8_t mouse_wrap;
    uint8_t mouse_type; /* MOUSE_PROT_PS2, *_IMPS/2, *_IMEX */
    uint8_t mouse_detect_state;
    int32_t mouse_dx; /* current values, needed for 'poll' mode */
    int32_t mouse_dy;
    int32_t mouse_dz;
    int32_t mouse_dw;
    int32_t mouse_flags;
    uint8_t mouse_buttons;
    uint8_t mouse_buttons_reported;

    /** Pointer to the device instance - RC. */
    PPDMDEVINSRC                pDevInsRC;
    /** Pointer to the device instance - R3 . */
    PPDMDEVINSR3                pDevInsR3;
    /** Pointer to the device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** Critical section protecting the state. */
    PDMCRITSECT                 CritSect;
    /**
     * Keyboard port - LUN#0.
     *
     * @implements  PDMIBASE
     * @implements  PDMIKEYBOARDPORT
     */
    struct
    {
        /** The base interface for the keyboard port. */
        PDMIBASE                            IBase;
        /** The keyboard port base interface. */
        PDMIKEYBOARDPORT                    IPort;

        /** The base interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The keyboard interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIKEYBOARDCONNECTOR)   pDrv;
    } Keyboard;

    /**
     * Mouse port - LUN#1.
     *
     * @implements  PDMIBASE
     * @implements  PDMIMOUSEPORT
     */
    struct
    {
        /** The base interface for the mouse port. */
        PDMIBASE                            IBase;
        /** The mouse port base interface. */
        PDMIMOUSEPORT                       IPort;

        /** The base interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The mouse interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIMOUSECONNECTOR)      pDrv;
    } Mouse;
} KBDState;

/* Table to convert from PC scancodes to scan code set 2. */
static const unsigned char ps2_raw_keycode_set2[128] = {
      0,118, 22, 30, 38, 37, 46, 54, 61, 62, 70, 69, 78, 85,102, 13,
     21, 29, 36, 45, 44, 53, 60, 67, 68, 77, 84, 91, 90, 20, 28, 27,
     35, 43, 52, 51, 59, 66, 75, 76, 82, 14, 18, 93, 26, 34, 33, 42,
     50, 49, 58, 65, 73, 74, 89,124, 17, 41, 88,  5,  6,  4, 12,  3,
     11,  2, 10,  1,  9,119,126,108,117,125,123,107,115,116,121,105,
    114,122,112,113,127, 96, 97,120,  7, 15, 23, 31, 39, 47, 55, 63,
     71, 79, 86, 94,  8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 87,111,
     19, 25, 57, 81, 83, 92, 95, 98, 99,100,101,103,104,106,109,110
};

/* Table to convert from PC scancodes to scan code set 3. */
static const unsigned char ps2_raw_keycode_set3[128] = {
      0,118, 22, 30, 38, 37, 46, 54, 61, 62, 70, 69, 78, 85,102, 13,
     21, 29, 36, 45, 44, 53, 60, 67, 68, 77, 84, 91, 90, 17, 28, 27,
     35, 43, 52, 51, 59, 66, 75, 76, 82, 14, 18, 92, 26, 34, 33, 42,
     50, 49, 58, 65, 73, 74, 89,124, 25, 41, 20,  5,  6,  4, 12,  3,
     11,  2, 10,  1,  9,118,126,108,117,125,123,107,115,116,121,105,
    114,122,112,113,127, 96, 19,120,  7, 15, 23, 31, 39, 47, 55, 63,
     71, 79, 86, 94,  8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 87,111,
     19, 25, 57, 81, 83, 92, 95, 98, 99,100,101,103,104,106,109,110
};

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/* update irq and KBD_STAT_[MOUSE_]OBF */
static void kbd_update_irq(KBDState *s)
{
    KBDQueue *q = &s->queue;
    MouseCmdQueue *mcq = &s->mouse_command_queue;
    MouseEventQueue *meq = &s->mouse_event_queue;
    int irq12_level, irq1_level;

    irq1_level = 0;
    irq12_level = 0;

    /* Determine new OBF state, but only if OBF is clear. If OBF was already
     * set, we cannot risk changing the event type after an ISR potentially
     * started executing! Only kbd_read_data() clears the OBF bits.
     */
    if (!(s->status & KBD_STAT_OBF)) {
        s->status &= ~KBD_STAT_MOUSE_OBF;
        /* Keyboard data has priority if both kbd and aux data is available. */
        if (q->count && !(s->mode & KBD_MODE_DISABLE_KBD))
        {
            s->status |= KBD_STAT_OBF;
            s->dbbout = q->data[q->rptr];
            if (++q->rptr == KBD_QUEUE_SIZE)
                q->rptr = 0;
            q->count--;
        }
        else if ((mcq->count || meq->count) /*&& !(s->mode & KBD_MODE_DISABLE_MOUSE)*/)
        {
            s->status |= KBD_STAT_OBF | KBD_STAT_MOUSE_OBF;
            if (mcq->count)
            {
                s->dbbout = mcq->data[mcq->rptr];
                if (++mcq->rptr == MOUSE_CMD_QUEUE_SIZE)
                    mcq->rptr = 0;
                mcq->count--;
            }
            else
            {
                s->dbbout = meq->data[meq->rptr];
                if (++meq->rptr == MOUSE_EVENT_QUEUE_SIZE)
                    meq->rptr = 0;
                meq->count--;
            }
        }
    }
    /* Determine new IRQ state. */
    if (s->status & KBD_STAT_OBF) {
        if (s->status & KBD_STAT_MOUSE_OBF)
        {
            if (s->mode & KBD_MODE_MOUSE_INT)
                irq12_level = 1;
        }
        else
        {   /* KBD_STAT_OBF set but KBD_STAT_MOUSE_OBF isn't. */
            if (s->mode & KBD_MODE_KBD_INT)
                irq1_level = 1;
        }
    }
    PDMDevHlpISASetIrq(s->CTX_SUFF(pDevIns), 1, irq1_level);
    PDMDevHlpISASetIrq(s->CTX_SUFF(pDevIns), 12, irq12_level);
}

static void kbd_queue(KBDState *s, int b, int aux)
{
    KBDQueue *q = &s->queue;
    MouseCmdQueue *mcq = &s->mouse_command_queue;
    MouseEventQueue *meq = &s->mouse_event_queue;

#if defined(DEBUG_MOUSE) || defined(DEBUG_KBD)
    if (aux == 1)
        LogRel3(("%s: mouse command response: 0x%02x\n", __PRETTY_FUNCTION__, b));
    else if (aux == 2)
        LogRel3(("%s: mouse event data: 0x%02x\n", __PRETTY_FUNCTION__, b));
#ifdef DEBUG_KBD
    else
        LogRel3(("%s: kbd event: 0x%02x\n", __PRETTY_FUNCTION__, b));
#endif
#endif
    switch (aux)
    {
        case 0: /* keyboard */
            if (q->count >= KBD_QUEUE_SIZE)
                return;
            q->data[q->wptr] = b;
            if (++q->wptr == KBD_QUEUE_SIZE)
                q->wptr = 0;
            q->count++;
            break;
        case 1: /* mouse command response */
            if (mcq->count >= MOUSE_CMD_QUEUE_SIZE)
                return;
            mcq->data[mcq->wptr] = b;
            if (++mcq->wptr == MOUSE_CMD_QUEUE_SIZE)
                mcq->wptr = 0;
            mcq->count++;
            break;
        case 2: /* mouse event data */
            if (meq->count >= MOUSE_EVENT_QUEUE_SIZE)
                return;
            meq->data[meq->wptr] = b;
            if (++meq->wptr == MOUSE_EVENT_QUEUE_SIZE)
                meq->wptr = 0;
            meq->count++;
            break;
        default:
            AssertMsgFailed(("aux=%d\n", aux));
    }
    kbd_update_irq(s);
}

#ifdef IN_RING3
static void pc_kbd_put_keycode(void *opaque, int keycode)
{
    KBDState *s = (KBDState*)opaque;

    /* XXX: add support for scancode sets 1 and 3 */
    if (!s->translate && keycode < 0xe0 && s->scancode_set >= 2)
    {
        if (keycode & 0x80)
           kbd_queue(s, 0xf0, 0);
        if (s->scancode_set == 2)
            keycode = ps2_raw_keycode_set2[keycode & 0x7f];
        else if (s->scancode_set == 3)
            keycode = ps2_raw_keycode_set3[keycode & 0x7f];
    }
    kbd_queue(s, keycode, 0);
}
#endif /* IN_RING3 */

static void kbc_dbb_out(void *opaque, uint8_t val)
{
    KBDState *s = (KBDState*)opaque;

    s->dbbout = val;
    /* Set the OBF and raise IRQ. */
    s->status |= KBD_STAT_OBF;
    if (s->mode & KBD_MODE_KBD_INT)
        PDMDevHlpISASetIrq(s->CTX_SUFF(pDevIns), 1, 1);
}

static uint32_t kbd_read_status(void *opaque, uint32_t addr)
{
    KBDState *s = (KBDState*)opaque;
    int val = s->status;
    NOREF(addr);

#if defined(DEBUG_KBD)
    Log(("kbd: read status=0x%02x\n", val));
#endif
    return val;
}

static int kbd_write_command(void *opaque, uint32_t addr, uint32_t val)
{
    int rc = VINF_SUCCESS;
    KBDState *s = (KBDState*)opaque;
    NOREF(addr);

#ifdef DEBUG_KBD
    Log(("kbd: write cmd=0x%02x\n", val));
#endif
    switch(val) {
    case KBD_CCMD_READ_MODE:
        kbc_dbb_out(s, s->mode);
        break;
    case KBD_CCMD_WRITE_MODE:
    case KBD_CCMD_WRITE_OBUF:
    case KBD_CCMD_WRITE_AUX_OBUF:
    case KBD_CCMD_WRITE_MOUSE:
    case KBD_CCMD_WRITE_OUTPORT:
        s->write_cmd = val;
        break;
    case KBD_CCMD_MOUSE_DISABLE:
        s->mode |= KBD_MODE_DISABLE_MOUSE;
        break;
    case KBD_CCMD_MOUSE_ENABLE:
        s->mode &= ~KBD_MODE_DISABLE_MOUSE;
        /* Check for queued input. */
        kbd_update_irq(s);
        break;
    case KBD_CCMD_TEST_MOUSE:
        kbc_dbb_out(s, 0x00);
        break;
    case KBD_CCMD_SELF_TEST:
        /* Enable the A20 line - that is the power-on state(!). */
# ifndef IN_RING3
        if (!PDMDevHlpA20IsEnabled(s->CTX_SUFF(pDevIns)))
        {
            rc = VINF_IOM_HC_IOPORT_WRITE;
            break;
        }
# else /* IN_RING3 */
        PDMDevHlpA20Set(s->CTX_SUFF(pDevIns), true);
# endif /* IN_RING3 */
        s->status |= KBD_STAT_SELFTEST;
        s->mode |= KBD_MODE_DISABLE_KBD;
        kbc_dbb_out(s, 0x55);
        break;
    case KBD_CCMD_KBD_TEST:
        kbc_dbb_out(s, 0x00);
        break;
    case KBD_CCMD_KBD_DISABLE:
        s->mode |= KBD_MODE_DISABLE_KBD;
        break;
    case KBD_CCMD_KBD_ENABLE:
        s->mode &= ~KBD_MODE_DISABLE_KBD;
        /* Check for queued input. */
        kbd_update_irq(s);
        break;
    case KBD_CCMD_READ_INPORT:
        kbc_dbb_out(s, 0x00);
        break;
    case KBD_CCMD_READ_OUTPORT:
        /* XXX: check that */
#ifdef TARGET_I386
        val = 0x01 | (PDMDevHlpA20IsEnabled(s->CTX_SUFF(pDevIns)) << 1);
#else
        val = 0x01;
#endif
        if (s->status & KBD_STAT_OBF)
            val |= 0x10;
        if (s->status & KBD_STAT_MOUSE_OBF)
            val |= 0x20;
        kbc_dbb_out(s, val);
        break;
#ifdef TARGET_I386
    case KBD_CCMD_ENABLE_A20:
# ifndef IN_RING3
        if (!PDMDevHlpA20IsEnabled(s->CTX_SUFF(pDevIns)))
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        PDMDevHlpA20Set(s->CTX_SUFF(pDevIns), true);
# endif /* IN_RING3 */
        break;
    case KBD_CCMD_DISABLE_A20:
# ifndef IN_RING3
        if (PDMDevHlpA20IsEnabled(s->CTX_SUFF(pDevIns)))
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        PDMDevHlpA20Set(s->CTX_SUFF(pDevIns), false);
# endif /* !IN_RING3 */
        break;
#endif
    case KBD_CCMD_READ_TSTINP:
        /* Keyboard clock line is zero IFF keyboard is disabled */
        val = (s->mode & KBD_MODE_DISABLE_KBD) ? 0 : 1;
        kbc_dbb_out(s, val);
        break;
    case KBD_CCMD_RESET:
#ifndef IN_RING3
        rc = VINF_IOM_HC_IOPORT_WRITE;
#else /* IN_RING3 */
        rc = PDMDevHlpVMReset(s->CTX_SUFF(pDevIns));
#endif /* !IN_RING3 */
        break;
    case 0xff:
        /* ignore that - I don't know what is its use */
        break;
    /* Make OS/2 happy. */
    /* The 8042 RAM is readable using commands 0x20 thru 0x3f, and writable
       by 0x60 thru 0x7f. Now days only the firs byte, the mode, is used.
       We'll ignore the writes (0x61..7f) and return 0 for all the reads
       just to make some OS/2 debug stuff a bit happier. */
    case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        kbc_dbb_out(s, 0);
        Log(("kbd: reading non-standard RAM addr %#x\n", val & 0x1f));
        break;
    default:
        Log(("kbd: unsupported keyboard cmd=0x%02x\n", val));
        break;
    }
    return rc;
}

static uint32_t kbd_read_data(void *opaque, uint32_t addr)
{
    KBDState *s = (KBDState*)opaque;
    uint32_t val;
    NOREF(addr);

    /* Return the current DBB contents. */
    val = s->dbbout;

    /* Reading the DBB deasserts IRQs... */
    if (s->status & KBD_STAT_MOUSE_OBF)
        PDMDevHlpISASetIrq(s->CTX_SUFF(pDevIns), 12, 0);
    else
        PDMDevHlpISASetIrq(s->CTX_SUFF(pDevIns), 1, 0);
    /* ...and clears the OBF bits. */
    s->status &= ~(KBD_STAT_OBF | KBD_STAT_MOUSE_OBF);

    /* Check if more data is available. */
    kbd_update_irq(s);
#ifdef DEBUG_KBD
    Log(("kbd: read data=0x%02x\n", val));
#endif
    return val;
}

static void kbd_reset_keyboard(KBDState *s)
{
    s->scan_enabled = 1;
    s->scancode_set = 2;
    /* Flush the keyboard queue. */
    s->queue.count = 0;
    s->queue.rptr = 0;
    s->queue.wptr = 0;
}

static int  kbd_write_keyboard(KBDState *s, int val)
{
    switch(s->kbd_write_cmd) {
    default:
    case -1:
        switch(val) {
        case 0x00:
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        case 0x05:
            kbd_queue(s, KBD_REPLY_RESEND, 0);
            break;
        case KBD_CMD_GET_ID:
            kbd_queue(s, KBD_REPLY_ACK, 0);
            kbd_queue(s, 0xab, 0);
            kbd_queue(s, 0x83, 0);
            break;
        case KBD_CMD_ECHO:
            kbd_queue(s, KBD_CMD_ECHO, 0);
            break;
        case KBD_CMD_ENABLE:
            s->scan_enabled = 1;
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        case KBD_CMD_SCANCODE:
        case KBD_CMD_SET_LEDS:
        case KBD_CMD_SET_RATE:
            s->kbd_write_cmd = val;
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        case KBD_CMD_RESET_DISABLE:
            kbd_reset_keyboard(s);
            s->scan_enabled = 0;
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        case KBD_CMD_RESET_ENABLE:
            kbd_reset_keyboard(s);
            s->scan_enabled = 1;
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        case KBD_CMD_RESET:
            kbd_reset_keyboard(s);
            kbd_queue(s, KBD_REPLY_ACK, 0);
            kbd_queue(s, KBD_REPLY_POR, 0);
            break;
        default:
            kbd_queue(s, KBD_REPLY_ACK, 0);
            break;
        }
        break;
    case KBD_CMD_SCANCODE:
#ifdef IN_RING3
        if (val == 0) {
            if (s->scancode_set == 1)
                pc_kbd_put_keycode(s, 0x43);
            else if (s->scancode_set == 2)
                pc_kbd_put_keycode(s, 0x41);
            else if (s->scancode_set == 3)
                pc_kbd_put_keycode(s, 0x3f);
        } else {
            if (val >= 1 && val <= 3) {
                LogRel(("kbd: scan code set %d selected\n", val));
                s->scancode_set = val;
            }
            kbd_queue(s, KBD_REPLY_ACK, 0);
        }
#else
        return VINF_IOM_HC_IOPORT_WRITE;
#endif
    case KBD_CMD_SET_LEDS:
        {
#ifdef IN_RING3
            PDMKEYBLEDS enmLeds = PDMKEYBLEDS_NONE;
            if (val & 0x01)
                enmLeds = (PDMKEYBLEDS)(enmLeds | PDMKEYBLEDS_SCROLLLOCK);
            if (val & 0x02)
                enmLeds = (PDMKEYBLEDS)(enmLeds | PDMKEYBLEDS_NUMLOCK);
            if (val & 0x04)
                enmLeds = (PDMKEYBLEDS)(enmLeds | PDMKEYBLEDS_CAPSLOCK);
            s->Keyboard.pDrv->pfnLedStatusChange(s->Keyboard.pDrv, enmLeds);
#else
            return VINF_IOM_HC_IOPORT_WRITE;
#endif
            kbd_queue(s, KBD_REPLY_ACK, 0);
            s->kbd_write_cmd = -1;
        }
        break;
    case KBD_CMD_SET_RATE:
        kbd_queue(s, KBD_REPLY_ACK, 0);
        s->kbd_write_cmd = -1;
        break;
    }

    return VINF_SUCCESS;
}

static void kbd_mouse_set_reported_buttons(KBDState *s, unsigned fButtons, unsigned fButtonMask)
{
    s->mouse_buttons_reported |= (fButtons & fButtonMask);
    s->mouse_buttons_reported &= (fButtons | ~fButtonMask);
}

/**
 * Send a single relative packet in 3-byte PS/2 format to the PS/2 controller.
 * @param  s               keyboard state object
 * @param  dx              relative X value, must be between -256 and +255
 * @param  dy              relative y value, must be between -256 and +255
 * @param  fButtonsLow     the state of the two first mouse buttons
 * @param  fButtonsPacked  the state of the upper three mouse buttons and
 *                         scroll wheel movement, packed as per the
 *                         MOUSE_EXT_* defines.  For standard PS/2 packets
 *                         only pass the value of button 3 here.
 */
static void kbd_mouse_send_rel3_packet(KBDState *s, bool fToCmdQueue)
{
    int aux = fToCmdQueue ? 1 : 2;
    int dx1 = s->mouse_dx < 0 ? RT_MAX(s->mouse_dx, -256)
                              : RT_MIN(s->mouse_dx, 255);
    int dy1 = s->mouse_dy < 0 ? RT_MAX(s->mouse_dy, -256)
                              : RT_MIN(s->mouse_dy, 255);
    unsigned int b;
    unsigned fButtonsLow = s->mouse_buttons & 0x07;
    s->mouse_dx -= dx1;
    s->mouse_dy -= dy1;
    kbd_mouse_set_reported_buttons(s, fButtonsLow, 0x07);
    LogRel3(("%s: dx1=%d, dy1=%d, fButtonsLow=0x%x\n",
             __PRETTY_FUNCTION__, dx1, dy1, fButtonsLow));
    b = 0x08 | ((dx1 < 0 ? 1 : 0) << 4) | ((dy1 < 0 ? 1 : 0) << 5)
             | fButtonsLow;
    kbd_queue(s, b, aux);
    kbd_queue(s, dx1 & 0xff, aux);
    kbd_queue(s, dy1 & 0xff, aux);
}

static void kbd_mouse_send_imps2_byte4(KBDState *s, bool fToCmdQueue)
{
    int aux = fToCmdQueue ? 1 : 2;

    int dz1 = s->mouse_dz < 0 ? RT_MAX(s->mouse_dz, -127)
                              : RT_MIN(s->mouse_dz, 127);
    LogRel3(("%s: dz1=%d\n", __PRETTY_FUNCTION__, dz1));
    s->mouse_dz -= dz1;
    kbd_queue(s, dz1 & 0xff, aux);
}

static void kbd_mouse_send_imex_byte4(KBDState *s, bool fToCmdQueue)
{
    int aux = fToCmdQueue ? 1 : 2;
    int dz1 = 0, dw1 = 0;
    unsigned fButtonsHigh = s->mouse_buttons & 0x18;

    if (s->mouse_dw > 0)
        dw1 = 1;
    else if (s->mouse_dw < 0)
        dw1 = -1;
    else if (s->mouse_dz > 0)
        dz1 = 1;
    else if (s->mouse_dz < 0)
        dz1 = -1;
    if (s->mouse_dw && s->mouse_flags & MOUSE_REPORT_HORIZONTAL)
    {
        LogRel3(("%s: dw1=%d\n", __PRETTY_FUNCTION__, dw1));
        kbd_queue(s, 0x40 | (dw1 & 0x3f), aux);
    }
    else
    {
        LogRel3(("%s: dz1=%d, dw1=%d, fButtonsHigh=0x%x\n",
                 __PRETTY_FUNCTION__, dz1, dw1, fButtonsHigh));
        unsigned u4Low =   dw1 > 0 ? 9 /* -7 & 0xf */
                         : dw1 < 0 ? 7
                         : dz1 > 0 ? 1
                         : dz1 < 0 ? 0xf /* -1 & 0xf */
                         : 0;
        kbd_mouse_set_reported_buttons(s, fButtonsHigh, 0x18);
        kbd_queue(s, (fButtonsHigh << 1) | u4Low, aux);
    }
    s->mouse_dz -= dz1;
    s->mouse_dw -= dw1;
}

/**
 * Send a single relative packet in (IM)PS/2 or IMEX format to the PS/2
 * controller.
 * @param  s            keyboard state object
 * @param  fToCmdQueue  should this packet go to the command queue (or the
 *                      event queue)?
 */
static void kbd_mouse_send_packet(KBDState *s, bool fToCmdQueue)
{
    kbd_mouse_send_rel3_packet(s, fToCmdQueue);
    if (s->mouse_type == MOUSE_PROT_IMPS2)
        kbd_mouse_send_imps2_byte4(s, fToCmdQueue);
    if (s->mouse_type == MOUSE_PROT_IMEX)
        kbd_mouse_send_imex_byte4(s, fToCmdQueue);
}

#ifdef IN_RING3

static bool kbd_mouse_unreported(KBDState *s)
{
    return s->mouse_dx
        || s->mouse_dy
        || s->mouse_dz
        || s->mouse_dw
        || s->mouse_buttons != s->mouse_buttons_reported;
}

static size_t kbd_mouse_event_queue_free(KBDState *s)
{
    AssertReturn(s->mouse_event_queue.count <= MOUSE_EVENT_QUEUE_SIZE, 0);
    return MOUSE_EVENT_QUEUE_SIZE - s->mouse_event_queue.count;
}

static void pc_kbd_mouse_event(void *opaque, int dx, int dy, int dz, int dw,
                               int buttons_state)
{
    LogRel3(("%s: dx=%d, dy=%d, dz=%d, dw=%d, buttons_state=0x%x\n",
             __PRETTY_FUNCTION__, dx, dy, dz, dw, buttons_state));
    KBDState *s = (KBDState*)opaque;

    /* check if deltas are recorded when disabled */
    if (!(s->mouse_status & MOUSE_STATUS_ENABLED))
        return;
    AssertReturnVoid((buttons_state & ~0x1f) == 0);

    s->mouse_dx += dx;
    s->mouse_dy -= dy;
    if (   (s->mouse_type == MOUSE_PROT_IMPS2)
        || (s->mouse_type == MOUSE_PROT_IMEX))
        s->mouse_dz += dz;
    if (s->mouse_type == MOUSE_PROT_IMEX)
        s->mouse_dw += dw;
    s->mouse_buttons = buttons_state;
    if (!(s->mouse_status & MOUSE_STATUS_REMOTE))
        /* if not remote, send event. Multiple events are sent if
           too big deltas */
        while (   kbd_mouse_unreported(s)
               && kbd_mouse_event_queue_free(s) > 4)
            kbd_mouse_send_packet(s, false);
}

/* Report a change in status down the driver chain */
static void kbd_mouse_update_downstream_status(KBDState *pThis)
{
    PPDMIMOUSECONNECTOR pDrv = pThis->Mouse.pDrv;
    bool fEnabled = !!(pThis->mouse_status & MOUSE_STATUS_ENABLED);
    pDrv->pfnReportModes(pDrv, fEnabled, false);
}

#endif /* IN_RING3 */

static int kbd_write_mouse(KBDState *s, int val)
{
#ifdef DEBUG_MOUSE
    LogRelFlowFunc(("kbd: write mouse 0x%02x\n", val));
#endif
    int rc = VINF_SUCCESS;
    /* Flush the mouse command response queue. */
    s->mouse_command_queue.count = 0;
    s->mouse_command_queue.rptr = 0;
    s->mouse_command_queue.wptr = 0;
    switch(s->mouse_write_cmd) {
    default:
    case -1:
        /* mouse command */
        if (s->mouse_wrap) {
            if (val == AUX_RESET_WRAP) {
                s->mouse_wrap = 0;
                kbd_queue(s, AUX_ACK, 1);
                return VINF_SUCCESS;
            } else if (val != AUX_RESET) {
                kbd_queue(s, val, 1);
                return VINF_SUCCESS;
            }
        }
        switch(val) {
        case AUX_SET_SCALE11:
            s->mouse_status &= ~MOUSE_STATUS_SCALE21;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_SET_SCALE21:
            s->mouse_status |= MOUSE_STATUS_SCALE21;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_SET_STREAM:
            s->mouse_status &= ~MOUSE_STATUS_REMOTE;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_SET_WRAP:
            s->mouse_wrap = 1;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_SET_REMOTE:
            s->mouse_status |= MOUSE_STATUS_REMOTE;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_GET_TYPE:
            kbd_queue(s, AUX_ACK, 1);
            kbd_queue(s, s->mouse_type, 1);
            break;
        case AUX_SET_RES:
        case AUX_SET_SAMPLE:
            s->mouse_write_cmd = val;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_GET_SCALE:
            kbd_queue(s, AUX_ACK, 1);
            kbd_queue(s, s->mouse_status, 1);
            kbd_queue(s, s->mouse_resolution, 1);
            kbd_queue(s, s->mouse_sample_rate, 1);
            break;
        case AUX_POLL:
            kbd_queue(s, AUX_ACK, 1);
            kbd_mouse_send_packet(s, true);
            break;
        case AUX_ENABLE_DEV:
#ifdef IN_RING3
            LogRelFlowFunc(("Enabling mouse device\n"));
            s->mouse_status |= MOUSE_STATUS_ENABLED;
            kbd_queue(s, AUX_ACK, 1);
            kbd_mouse_update_downstream_status(s);
#else
            LogRelFlowFunc(("Enabling mouse device, R0 stub\n"));
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;
        case AUX_DISABLE_DEV:
#ifdef IN_RING3
            s->mouse_status &= ~MOUSE_STATUS_ENABLED;
            kbd_queue(s, AUX_ACK, 1);
            /* Flush the mouse events queue. */
            s->mouse_event_queue.count = 0;
            s->mouse_event_queue.rptr = 0;
            s->mouse_event_queue.wptr = 0;
            kbd_mouse_update_downstream_status(s);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;
        case AUX_SET_DEFAULT:
#ifdef IN_RING3
            s->mouse_sample_rate = 100;
            s->mouse_resolution = 2;
            s->mouse_status = 0;
            kbd_queue(s, AUX_ACK, 1);
            kbd_mouse_update_downstream_status(s);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;
        case AUX_RESET:
#ifdef IN_RING3
            s->mouse_sample_rate = 100;
            s->mouse_resolution = 2;
            s->mouse_status = 0;
            s->mouse_type = MOUSE_PROT_PS2;
            kbd_queue(s, AUX_ACK, 1);
            kbd_queue(s, 0xaa, 1);
            kbd_queue(s, s->mouse_type, 1);
            /* Flush the mouse events queue. */
            s->mouse_event_queue.count = 0;
            s->mouse_event_queue.rptr = 0;
            s->mouse_event_queue.wptr = 0;
            kbd_mouse_update_downstream_status(s);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;
        default:
            /* NACK all commands we don't know.

               The usecase for this is the OS/2 mouse driver which will try
               read 0xE2 in order to figure out if it's a trackpoint device
               or not. If it doesn't get a NACK (or ACK) on the command it'll
               do several hundred thousand status reads before giving up. This
               is slows down the OS/2 boot up considerably. (It also seems that
               the code is somehow vulnerable while polling like this and that
               mouse or keyboard input at this point might screw things up badly.)

               From http://www.win.tue.nl/~aeb/linux/kbd/scancodes-13.html:

               Every command or data byte sent to the mouse (except for the
               resend command fe) is ACKed with fa. If the command or data
               is invalid, it is NACKed with fe. If the next byte is again
               invalid, the reply is ERROR: fc. */
            /** @todo send error if we NACKed the previous command? */
            kbd_queue(s, AUX_NACK, 1);
            break;
        }
        break;
    case AUX_SET_SAMPLE:
        s->mouse_sample_rate = val;
        /* detect IMPS/2 or IMEX */
        /* And enable horizontal scrolling reporting when requested */
        switch(s->mouse_detect_state) {
        default:
        case 0:
            if (val == 200)
                s->mouse_detect_state = 1;
            break;
        case 1:
            if (val == 100)
                s->mouse_detect_state = 2;
            else if (val == 200)
                s->mouse_detect_state = 3;
            else if ((val == 80) && s->mouse_type == MOUSE_PROT_IMEX)
                /* enable horizontal scrolling, byte two */
                s->mouse_detect_state = 4;
            else
                s->mouse_detect_state = 0;
            break;
        case 2:
            if (val == 80 && s->mouse_type < MOUSE_PROT_IMEX)
            {
                LogRelFlowFunc(("switching mouse device to IMPS/2 mode\n"));
                s->mouse_type = MOUSE_PROT_IMPS2;
            }
            s->mouse_detect_state = 0;
            break;
        case 3:
            if (val == 80)
            {
                LogRelFlowFunc(("switching mouse device to IMEX mode\n"));
                s->mouse_type = MOUSE_PROT_IMEX;
            }
            s->mouse_detect_state = 0;
            break;
        case 4:
            if (val == 40)
            {
                LogRelFlowFunc(("enabling IMEX horizontal scrolling reporting\n"));
                s->mouse_flags |= MOUSE_REPORT_HORIZONTAL;
            }
            s->mouse_detect_state = 0;
            break;
        }
        kbd_queue(s, AUX_ACK, 1);
        s->mouse_write_cmd = -1;
        break;
    case AUX_SET_RES:
        if (0 <= val && val < 4)
        {
            s->mouse_resolution = val;
            kbd_queue(s, AUX_ACK, 1);
        }
        else
            kbd_queue(s, AUX_NACK, 1);
        s->mouse_write_cmd = -1;
        break;
    }
    return rc;
}

static int kbd_write_data(void *opaque, uint32_t addr, uint32_t val)
{
    int rc = VINF_SUCCESS;
    KBDState *s = (KBDState*)opaque;
    NOREF(addr);

#ifdef DEBUG_KBD
    Log(("kbd: write data=0x%02x\n", val));
#endif

    switch(s->write_cmd) {
    case 0:
        /* Automatically enables keyboard interface. */
        s->mode &= ~KBD_MODE_DISABLE_KBD;
        rc = kbd_write_keyboard(s, val);
        break;
    case KBD_CCMD_WRITE_MODE:
        s->mode = val;
        s->translate = (s->mode & KBD_MODE_KCC) == KBD_MODE_KCC;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_WRITE_OBUF:
        kbc_dbb_out(s, val);
        break;
    case KBD_CCMD_WRITE_AUX_OBUF:
        kbd_queue(s, val, 1);
        break;
    case KBD_CCMD_WRITE_OUTPORT:
#ifdef TARGET_I386
#  ifndef IN_RING3
        if (PDMDevHlpA20IsEnabled(s->CTX_SUFF(pDevIns)) != !!(val & 2))
            rc = VINF_IOM_HC_IOPORT_WRITE;
#  else /* IN_RING3 */
        PDMDevHlpA20Set(s->CTX_SUFF(pDevIns), !!(val & 2));
#  endif /* !IN_RING3 */
#endif
        if (!(val & 1)) {
# ifndef IN_RING3
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else
            rc = PDMDevHlpVMReset(s->CTX_SUFF(pDevIns));
# endif
        }
        break;
    case KBD_CCMD_WRITE_MOUSE:
        rc = kbd_write_mouse(s, val);
        break;
    default:
        break;
    }
    if (rc != VINF_IOM_HC_IOPORT_WRITE)
        s->write_cmd = 0;
    return rc;
}

#ifdef IN_RING3

static void kbd_reset(void *opaque)
{
    KBDState *s = (KBDState*)opaque;
    KBDQueue *q;
    MouseCmdQueue *mcq;
    MouseEventQueue *meq;

    s->kbd_write_cmd = -1;
    s->mouse_write_cmd = -1;
    s->mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
    s->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
    /* Resetting everything, keyword was not working right on NT4 reboot. */
    s->write_cmd = 0;
    s->scan_enabled = 0;
    s->translate = 0;
    s->scancode_set = 2;
    if (s->mouse_status)
    {
        s->mouse_status = 0;
        kbd_mouse_update_downstream_status(s);
    }
    s->mouse_resolution = 0;
    s->mouse_sample_rate = 0;
    s->mouse_wrap = 0;
    s->mouse_type = MOUSE_PROT_PS2;
    s->mouse_detect_state = 0;
    s->mouse_dx = 0;
    s->mouse_dy = 0;
    s->mouse_dz = 0;
    s->mouse_dw = 0;
    s->mouse_flags = 0;
    s->mouse_buttons = 0;
    s->mouse_buttons_reported = 0;
    q = &s->queue;
    q->rptr = 0;
    q->wptr = 0;
    q->count = 0;
    mcq = &s->mouse_command_queue;
    mcq->rptr = 0;
    mcq->wptr = 0;
    mcq->count = 0;
    meq = &s->mouse_event_queue;
    meq->rptr = 0;
    meq->wptr = 0;
    meq->count = 0;
}

static void kbd_save(QEMUFile* f, void* opaque)
{
    uint32_t    cItems;
    int i;
    KBDState *s = (KBDState*)opaque;

    qemu_put_8s(f, &s->write_cmd);
    qemu_put_8s(f, &s->status);
    qemu_put_8s(f, &s->mode);
    qemu_put_be32s(f, &s->kbd_write_cmd);
    qemu_put_be32s(f, &s->scan_enabled);
    qemu_put_be32s(f, &s->mouse_write_cmd);
    qemu_put_8s(f, &s->mouse_status);
    qemu_put_8s(f, &s->mouse_resolution);
    qemu_put_8s(f, &s->mouse_sample_rate);
    qemu_put_8s(f, &s->mouse_wrap);
    qemu_put_8s(f, &s->mouse_type);
    qemu_put_8s(f, &s->mouse_detect_state);
    qemu_put_be32s(f, &s->mouse_dx);
    qemu_put_be32s(f, &s->mouse_dy);
    qemu_put_be32s(f, &s->mouse_dz);
    qemu_put_be32s(f, &s->mouse_dw);
    qemu_put_be32s(f, &s->mouse_flags);
    qemu_put_8s(f, &s->mouse_buttons);
    qemu_put_8s(f, &s->mouse_buttons_reported);

    /* XXX: s->scancode_set isn't being saved, but we only really support set 2,
     * so no real harm done.
     */

    /*
     * We have to save the queues too.
     */
    cItems = s->queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->queue.rptr; cItems-- > 0; i = (i + 1) % RT_ELEMENTS(s->queue.data))
        SSMR3PutU8(f, s->queue.data[i]);
    Log(("kbd_save: %d keyboard queue items stored\n", s->queue.count));

    cItems = s->mouse_command_queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->mouse_command_queue.rptr; cItems-- > 0; i = (i + 1) % RT_ELEMENTS(s->mouse_command_queue.data))
        SSMR3PutU8(f, s->mouse_command_queue.data[i]);
    Log(("kbd_save: %d mouse command queue items stored\n", s->mouse_command_queue.count));

    cItems = s->mouse_event_queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->mouse_event_queue.rptr; cItems-- > 0; i = (i + 1) % RT_ELEMENTS(s->mouse_event_queue.data))
        SSMR3PutU8(f, s->mouse_event_queue.data[i]);
    Log(("kbd_save: %d mouse event queue items stored\n", s->mouse_event_queue.count));

    /* terminator */
    SSMR3PutU32(f, ~0);
}

static int kbd_load(QEMUFile* f, void* opaque, int version_id)
{
    uint32_t    u32, i;
    uint8_t u8Dummy;
    uint32_t u32Dummy;
    int         rc;
    KBDState *s = (KBDState*)opaque;

#if 0
    /** @todo enable this and remove the "if (version_id == 4)" code at some
     * later time */
    /* Version 4 was never created by any publicly released version of VBox */
    AssertReturn(version_id != 4, VERR_NOT_SUPPORTED);
#endif
    if (version_id < 2 || version_id > PCKBD_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    qemu_get_8s(f, &s->write_cmd);
    qemu_get_8s(f, &s->status);
    qemu_get_8s(f, &s->mode);
    qemu_get_be32s(f, (uint32_t *)&s->kbd_write_cmd);
    qemu_get_be32s(f, (uint32_t *)&s->scan_enabled);
    qemu_get_be32s(f, (uint32_t *)&s->mouse_write_cmd);
    qemu_get_8s(f, &s->mouse_status);
    qemu_get_8s(f, &s->mouse_resolution);
    qemu_get_8s(f, &s->mouse_sample_rate);
    qemu_get_8s(f, &s->mouse_wrap);
    qemu_get_8s(f, &s->mouse_type);
    qemu_get_8s(f, &s->mouse_detect_state);
    qemu_get_be32s(f, (uint32_t *)&s->mouse_dx);
    qemu_get_be32s(f, (uint32_t *)&s->mouse_dy);
    qemu_get_be32s(f, (uint32_t *)&s->mouse_dz);
    if (version_id > 2)
    {
        SSMR3GetS32(f, &s->mouse_dw);
        SSMR3GetS32(f, &s->mouse_flags);
    }
    qemu_get_8s(f, &s->mouse_buttons);
    if (version_id == 4)
    {
        SSMR3GetU32(f, &u32Dummy);
        SSMR3GetU32(f, &u32Dummy);
    }
    if (version_id > 3)
        SSMR3GetU8(f, &s->mouse_buttons_reported);
    if (version_id == 4)
        SSMR3GetU8(f, &u8Dummy);
    s->queue.count = 0;
    s->queue.rptr = 0;
    s->queue.wptr = 0;
    s->mouse_command_queue.count = 0;
    s->mouse_command_queue.rptr = 0;
    s->mouse_command_queue.wptr = 0;
    s->mouse_event_queue.count = 0;
    s->mouse_event_queue.rptr = 0;
    s->mouse_event_queue.wptr = 0;

    /* Determine the translation state. */
    s->translate = (s->mode & KBD_MODE_KCC) == KBD_MODE_KCC;
    s->scancode_set = 2;    /* XXX: See comment in kbd_save(). */

    /*
     * Load the queues
     */
    rc = SSMR3GetU32(f, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 > RT_ELEMENTS(s->queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->queue.data[i]);
        if (RT_FAILURE(rc))
            return rc;
    }
    s->queue.wptr = u32 % RT_ELEMENTS(s->queue.data);
    s->queue.count = u32;
    Log(("kbd_load: %d keyboard queue items loaded\n", u32));

    rc = SSMR3GetU32(f, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 > RT_ELEMENTS(s->mouse_command_queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->mouse_command_queue.data[i]);
        if (RT_FAILURE(rc))
            return rc;
    }
    s->mouse_command_queue.wptr = u32 % RT_ELEMENTS(s->mouse_command_queue.data);
    s->mouse_command_queue.count = u32;
    Log(("kbd_load: %d mouse command queue items loaded\n", u32));

    rc = SSMR3GetU32(f, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 > RT_ELEMENTS(s->mouse_event_queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->mouse_event_queue.data[i]);
        if (RT_FAILURE(rc))
            return rc;
    }
    s->mouse_event_queue.wptr = u32 % RT_ELEMENTS(s->mouse_event_queue.data);
    s->mouse_event_queue.count = u32;
    Log(("kbd_load: %d mouse event queue items loaded\n", u32));

    /* terminator */
    rc = SSMR3GetU32(f, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    /* Resend a notification to Main if the device is active */
    kbd_mouse_update_downstream_status(s);
    return 0;
}
#endif /* IN_RING3 */


/* VirtualBox code start */

/* -=-=-=-=-=- wrappers -=-=-=-=-=- */

/**
 * Port I/O Handler for keyboard data IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) kbdIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        KBDState *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
        int rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            *pu32 = kbd_read_data(pThis, Port);
            PDMCritSectLeave(&pThis->CritSect);
            Log2(("kbdIOPortDataRead: Port=%#x cb=%d *pu32=%#x\n", Port, cb, *pu32));
        }
        return rc;
    }
    AssertMsgFailed(("Port=%#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

/**
 * Port I/O Handler for keyboard data OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) kbdIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser);
    if (cb == 1)
    {
        KBDState *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
        rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            rc = kbd_write_data(pThis, Port, u32);
            PDMCritSectLeave(&pThis->CritSect);
            Log2(("kbdIOPortDataWrite: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d\n", Port, cb));
    return rc;
}

/**
 * Port I/O Handler for keyboard status IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) kbdIOPortStatusRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        KBDState *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
        int rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            *pu32 = kbd_read_status(pThis, Port);
            PDMCritSectLeave(&pThis->CritSect);
            Log2(("kbdIOPortStatusRead: Port=%#x cb=%d -> *pu32=%#x\n", Port, cb, *pu32));
        }
        return rc;
    }
    AssertMsgFailed(("Port=%#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

/**
 * Port I/O Handler for keyboard command OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) kbdIOPortCommandWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser);
    if (cb == 1)
    {
        KBDState *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
        rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            rc = kbd_write_command(pThis, Port, u32);
            PDMCritSectLeave(&pThis->CritSect);
            Log2(("kbdIOPortCommandWrite: Port=%#x cb=%d u32=%#x rc=%Rrc\n", Port, cb, u32, rc));
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d\n", Port, cb));
    return rc;
}

#ifdef IN_RING3

/**
 * Saves a state of the keyboard device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) kbdSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    kbd_save(pSSMHandle, PDMINS_2_DATA(pDevIns, KBDState *));
    return VINF_SUCCESS;
}


/**
 * Loads a saved keyboard device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) kbdLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t uVersion, uint32_t uPass)
{
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    return kbd_load(pSSMHandle, PDMINS_2_DATA(pDevIns, KBDState *), uVersion);
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  kbdReset(PPDMDEVINS pDevIns)
{
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);

    kbd_reset(pThis);
    /* Activate the PS/2 keyboard by default. */
    if (pThis->Keyboard.pDrv)
        pThis->Keyboard.pDrv->pfnSetActive(pThis->Keyboard.pDrv, true);
}


/* -=-=-=-=-=- Keyboard: IBase  -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  kbdKeyboardQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    KBDState *pThis = RT_FROM_MEMBER(pInterface, KBDState, Keyboard.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->Keyboard.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDPORT, &pThis->Keyboard.IPort);
    return NULL;
}


/* -=-=-=-=-=- Keyboard: IKeyboardPort  -=-=-=-=-=- */

/**
 * Keyboard event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the keyboard port interface (KBDState::Keyboard.IPort).
 * @param   u8KeyCode       The keycode.
 */
static DECLCALLBACK(int) kbdKeyboardPutEvent(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode)
{
    KBDState *pThis = RT_FROM_MEMBER(pInterface, KBDState, Keyboard.IPort);
    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertReleaseRC(rc);

    pc_kbd_put_keycode(pThis, u8KeyCode);

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Mouse: IBase  -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  kbdMouseQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    KBDState *pThis = RT_FROM_MEMBER(pInterface, KBDState, Mouse.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->Mouse.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSEPORT, &pThis->Mouse.IPort);
    return NULL;
}


/* -=-=-=-=-=- Mouse: IMousePort  -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIMOUSEPORT, pfnPutEvent}
 */
static DECLCALLBACK(int) kbdMousePutEvent(PPDMIMOUSEPORT pInterface, int32_t iDeltaX, int32_t iDeltaY,
                                          int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtonStates)
{
    KBDState *pThis = RT_FROM_MEMBER(pInterface, KBDState, Mouse.IPort);
    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertReleaseRC(rc);

    pc_kbd_mouse_event(pThis, iDeltaX, iDeltaY, iDeltaZ, iDeltaW, fButtonStates);

    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMOUSEPORT, pfnPutEventAbs}
 */
static DECLCALLBACK(int) kbdMousePutEventAbs(PPDMIMOUSEPORT pInterface, uint32_t uX, uint32_t uY, int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtons)
{
    AssertFailedReturn(VERR_NOT_SUPPORTED);
    NOREF(pInterface); NOREF(uX); NOREF(uY); NOREF(iDeltaZ); NOREF(iDeltaW); NOREF(fButtons);
}


/* -=-=-=-=-=- real code -=-=-=-=-=- */


/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor have to attach to all the available drivers.
 *
 * This is like plugging in the keyboard or mouse after turning on the PC.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 * @remark  The keyboard controller doesn't support this action, this is just
 *          implemented to try out the driver<->device structure.
 */
static DECLCALLBACK(int)  kbdAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    int         rc;
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("PS/2 device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    switch (iLUN)
    {
        /* LUN #0: keyboard */
        case 0:
            rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pThis->Keyboard.IBase, &pThis->Keyboard.pDrvBase, "Keyboard Port");
            if (RT_SUCCESS(rc))
            {
                pThis->Keyboard.pDrv = PDMIBASE_QUERY_INTERFACE(pThis->Keyboard.pDrvBase, PDMIKEYBOARDCONNECTOR);
                if (!pThis->Keyboard.pDrv)
                {
                    AssertLogRelMsgFailed(("LUN #0 doesn't have a keyboard interface! rc=%Rrc\n", rc));
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pReg->szName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertLogRelMsgFailed(("Failed to attach LUN #0! rc=%Rrc\n", rc));
            break;

        /* LUN #1: aux/mouse */
        case 1:
            rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pThis->Mouse.IBase, &pThis->Mouse.pDrvBase, "Aux (Mouse) Port");
            if (RT_SUCCESS(rc))
            {
                pThis->Mouse.pDrv = PDMIBASE_QUERY_INTERFACE(pThis->Mouse.pDrvBase, PDMIMOUSECONNECTOR);
                if (!pThis->Mouse.pDrv)
                {
                    AssertLogRelMsgFailed(("LUN #1 doesn't have a mouse interface! rc=%Rrc\n", rc));
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #1!\n", pDevIns->pReg->szName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertLogRelMsgFailed(("Failed to attach LUN #1! rc=%Rrc\n", rc));
            break;

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            return VERR_PDM_NO_SUCH_LUN;
    }

    return rc;
}


/**
 * Detach notification.
 *
 * This is called when a driver is detaching itself from a LUN of the device.
 * The device should adjust it's state to reflect this.
 *
 * This is like unplugging the network cable to use it for the laptop or
 * something while the PC is still running.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 * @remark  The keyboard controller doesn't support this action, this is just
 *          implemented to try out the driver<->device structure.
 */
static DECLCALLBACK(void)  kbdDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
#if 0
    /*
     * Reset the interfaces and update the controller state.
     */
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
    switch (iLUN)
    {
        /* LUN #0: keyboard */
        case 0:
            pThis->Keyboard.pDrv = NULL;
            pThis->Keyboard.pDrvBase = NULL;
            break;

        /* LUN #1: aux/mouse */
        case 1:
            pThis->Mouse.pDrv = NULL;
            pThis->Mouse.pDrvBase = NULL;
            break;

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            break;
    }
#else
    NOREF(pDevIns); NOREF(iLUN); NOREF(fFlags);
#endif
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) kbdRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    NOREF(offDelta);
}


/**
 * Destruct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) kbdDestruct(PPDMDEVINS pDevIns)
{
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    PDMR3CritSectDelete(&pThis->CritSect);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) kbdConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    KBDState   *pThis = PDMINS_2_DATA(pDevIns, KBDState *);
    int         rc;
    bool        fGCEnabled;
    bool        fR0Enabled;
    Assert(iInstance == 0);

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to query \"GCEnabled\" from the config"));
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to query \"R0Enabled\" from the config"));
    Log(("pckbd: fGCEnabled=%RTbool fR0Enabled=%RTbool\n", fGCEnabled, fR0Enabled));


    /*
     * Initialize the interfaces.
     */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->Keyboard.IBase.pfnQueryInterface = kbdKeyboardQueryInterface;
    pThis->Keyboard.IPort.pfnPutEvent       = kbdKeyboardPutEvent;

    pThis->Mouse.IBase.pfnQueryInterface    = kbdMouseQueryInterface;
    pThis->Mouse.IPort.pfnPutEvent          = kbdMousePutEvent;
    pThis->Mouse.IPort.pfnPutEventAbs       = kbdMousePutEventAbs;

    /*
     * Initialize the critical section.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "PS2KM#%u", iInstance);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register I/O ports, save state, keyboard event handler and mouse event handlers.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x60, 1, NULL, kbdIOPortDataWrite,    kbdIOPortDataRead, NULL, NULL,   "PC Keyboard - Data");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x64, 1, NULL, kbdIOPortCommandWrite, kbdIOPortStatusRead, NULL, NULL, "PC Keyboard - Command / Status");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x60, 1, 0, "kbdIOPortDataWrite",    "kbdIOPortDataRead", NULL, NULL,   "PC Keyboard - Data");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x64, 1, 0, "kbdIOPortCommandWrite", "kbdIOPortStatusRead", NULL, NULL, "PC Keyboard - Command / Status");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x60, 1, 0, "kbdIOPortDataWrite",    "kbdIOPortDataRead", NULL, NULL,   "PC Keyboard - Data");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x64, 1, 0, "kbdIOPortCommandWrite", "kbdIOPortStatusRead", NULL, NULL, "PC Keyboard - Command / Status");
        if (RT_FAILURE(rc))
            return rc;
    }
    rc = PDMDevHlpSSMRegister(pDevIns, PCKBD_SAVED_STATE_VERSION, sizeof(*pThis), kbdSaveExec, kbdLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Attach to the keyboard and mouse drivers.
     */
    rc = kbdAttach(pDevIns, 0 /* keyboard LUN # */, PDM_TACH_FLAGS_NOT_HOT_PLUG);
    if (RT_FAILURE(rc))
        return rc;
    rc = kbdAttach(pDevIns, 1 /* aux/mouse LUN # */, PDM_TACH_FLAGS_NOT_HOT_PLUG);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the device state.
     */
    kbdReset(pDevIns);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePS2KeyboardMouse =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "pckbd",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "PS/2 Keyboard and Mouse device. Emulates both the keyboard, mouse and the keyboard controller. "
    "LUN #0 is the keyboard connector. "
    "LUN #1 is the aux/mouse connector.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_INPUT,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(KBDState),
    /* pfnConstruct */
    kbdConstruct,
    /* pfnDestruct */
    kbdDestruct,
    /* pfnRelocate */
    kbdRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    kbdReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    kbdAttach,
    /* pfnDetach */
    kbdDetach,
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
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

