#ifdef VBOX
/** @file
 *
 * VBox input devices:
 * PS/2 keyboard & mouse controller device
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <VBox/pdmdev.h>
#include <iprt/assert.h>

#include "Builtins.h"

#define PCKBD_SAVED_STATE_VERSION 2


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PDMBOTHCBDECL(int) kbdIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortStatusRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) kbdIOPortCommandWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
__END_DECLS
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
#endif /* VBOX */


#ifndef VBOX
#include "vl.h"
#endif

#ifdef VBOX
/* debug PC keyboard */
#define DEBUG_KBD

/* debug PC keyboard : only mouse */
#define DEBUG_MOUSE
#endif /* VBOX */

/*	Keyboard Controller Commands */
#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xAB	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xAD	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xAE	/* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT    0xC0    /* read input port */
#define KBD_CCMD_READ_OUTPORT	0xD0    /* read output port */
#define KBD_CCMD_WRITE_OUTPORT	0xD1    /* write output port */
#define KBD_CCMD_WRITE_OBUF	0xD2
#define KBD_CCMD_WRITE_AUX_OBUF	0xD3    /* Write to output buffer as if
					   initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */
#define KBD_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define KBD_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define KBD_CCMD_READ_TSTINP    0xE0    /* Read test inputs T0, T1 */
#define KBD_CCMD_RESET	        0xFE

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO     	0xEE
#define KBD_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Status Register Bits */
#define KBD_STAT_OBF 		0x01	/* Keyboard output buffer full */
#define KBD_STAT_IBF 		0x02	/* Keyboard input buffer full */
#define KBD_STAT_SELFTEST	0x04	/* Self test successful */
#define KBD_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define KBD_STAT_GTO 		0x40	/* General receive/xmit timeout */
#define KBD_STAT_PERR 		0x80	/* Parity error */

/* Controller Mode Register Bits */
#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generate IRQ1 */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define KBD_MODE_SYS 		0x04	/* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define KBD_MODE_KCC 		0x40	/* Scan code conversion to PC format */
#define KBD_MODE_RFU		0x80

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */
#ifdef VBOX
#define AUX_NACK			0xFE	/* Command byte NACK. */
#endif 

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define KBD_QUEUE_SIZE 256

typedef struct {
#ifndef VBOX
    uint8_t aux[KBD_QUEUE_SIZE];
#endif /* !VBOX */
    uint8_t data[KBD_QUEUE_SIZE];
    int rptr, wptr, count;
} KBDQueue;

#ifdef VBOX

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

#endif /* VBOX */

typedef struct KBDState {
    KBDQueue queue;
#ifdef VBOX
    MouseCmdQueue mouse_command_queue;
    MouseEventQueue mouse_event_queue;
#endif /* VBOX */
    uint8_t write_cmd; /* if non zero, write data to port 60 is expected */
    uint8_t status;
    uint8_t mode;
    /* keyboard state */
    int32_t kbd_write_cmd;
    int32_t scan_enabled;
    /* mouse state */
    int32_t mouse_write_cmd;
    uint8_t mouse_status;
    uint8_t mouse_resolution;
    uint8_t mouse_sample_rate;
    uint8_t mouse_wrap;
    uint8_t mouse_type; /* 0 = PS2, 3 = IMPS/2, 4 = IMEX */
    uint8_t mouse_detect_state;
    int32_t mouse_dx; /* current values, needed for 'poll' mode */
    int32_t mouse_dy;
    int32_t mouse_dz;
    uint8_t mouse_buttons;

#ifdef VBOX
    /** Pointer to the device instance. */
    PPDMDEVINSGC                pDevInsGC;
    /** Pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS)     pDevInsHC;
    /**
     * Keyboard port - LUN#0.
     */
    struct
    {
        /** The base interface for the keyboard port. */
        PDMIBASE                            Base;
        /** The keyboard port base interface. */
        PDMIKEYBOARDPORT                    Port;

        /** The base interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The keyboard interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIKEYBOARDCONNECTOR)   pDrv;
    } Keyboard;

    /**
     * Mouse port - LUN#1.
     */
    struct
    {
        /** The base interface for the mouse port. */
        PDMIBASE                            Base;
        /** The mouse port base interface. */
        PDMIMOUSEPORT                       Port;

        /** The base interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The mouse interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIMOUSECONNECTOR)      pDrv;
    } Mouse;
#endif
} KBDState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE
#ifndef VBOX
KBDState kbd_state;
#endif

/* update irq and KBD_STAT_[MOUSE_]OBF */
/* XXX: not generating the irqs if KBD_MODE_DISABLE_KBD is set may be
   incorrect, but it avoids having to simulate exact delays */
static void kbd_update_irq(KBDState *s)
{
    KBDQueue *q = &s->queue;
#ifdef VBOX
    MouseCmdQueue *mcq = &s->mouse_command_queue;
    MouseEventQueue *meq = &s->mouse_event_queue;
#endif /* VBOX */
    int irq12_level, irq1_level;

    irq1_level = 0;
    irq12_level = 0;
    s->status &= ~(KBD_STAT_OBF | KBD_STAT_MOUSE_OBF);
#ifdef VBOX
    if (q->count != 0)
    {
        s->status |= KBD_STAT_OBF;
        if ((s->mode & KBD_MODE_KBD_INT) && !(s->mode & KBD_MODE_DISABLE_KBD))
            irq1_level = 1;
    }
    else if (mcq->count != 0 || meq->count != 0)
    {
        s->status |= KBD_STAT_OBF | KBD_STAT_MOUSE_OBF;
        if (s->mode & KBD_MODE_MOUSE_INT)
            irq12_level = 1;
    }
#else /* !VBOX */
    if (q->count != 0) {
        s->status |= KBD_STAT_OBF;
        if (q->aux[q->rptr]) {
            s->status |= KBD_STAT_MOUSE_OBF;
            if (s->mode & KBD_MODE_MOUSE_INT)
                irq12_level = 1;
        } else {
            if ((s->mode & KBD_MODE_KBD_INT) &&
                !(s->mode & KBD_MODE_DISABLE_KBD))
                irq1_level = 1;
        }
    }
#endif /* !VBOX */
#ifndef VBOX
    pic_set_irq(1, irq1_level);
    pic_set_irq(12, irq12_level);
#else /* VBOX */
    PDMDevHlpISASetIrq(CTXSUFF(s->pDevIns), 1, irq1_level);
    PDMDevHlpISASetIrq(CTXSUFF(s->pDevIns), 12, irq12_level);
#endif /* VBOX */
}

static void kbd_queue(KBDState *s, int b, int aux)
{
    KBDQueue *q = &s->queue;
#ifdef VBOX
    MouseCmdQueue *mcq = &s->mouse_command_queue;
    MouseEventQueue *meq = &s->mouse_event_queue;
#endif /* VBOX */

#if defined(DEBUG_MOUSE) || defined(DEBUG_KBD)
    if (aux)
        Log(("mouse event: 0x%02x\n", b));
#ifdef DEBUG_KBD
    else
        Log(("kbd event: 0x%02x\n", b));
#endif
#endif
#ifdef VBOX
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
#else /* !VBOX */
    if (q->count >= KBD_QUEUE_SIZE)
        return;
    q->aux[q->wptr] = aux;
    q->data[q->wptr] = b;
    if (++q->wptr == KBD_QUEUE_SIZE)
        q->wptr = 0;
    q->count++;
#endif /* !VBOX */
    kbd_update_irq(s);
}

#ifdef IN_RING3
static void pc_kbd_put_keycode(void *opaque, int keycode)
{
    KBDState *s = (KBDState*)opaque;
    kbd_queue(s, keycode, 0);
}
#endif /* IN_RING3 */

static uint32_t kbd_read_status(void *opaque, uint32_t addr)
{
    KBDState *s = (KBDState*)opaque;
    int val;
    val = s->status;
#if defined(DEBUG_KBD)
    Log(("kbd: read status=0x%02x\n", val));
#endif
    return val;
}

#ifndef VBOX
static void kbd_write_command(void *opaque, uint32_t addr, uint32_t val)
{
#else /* VBOX (we need VMReset return code passed back) */
static int kbd_write_command(void *opaque, uint32_t addr, uint32_t val)
{
    int rc = VINF_SUCCESS;
#endif /* VBOX */
    KBDState *s = (KBDState*)opaque;

#ifdef DEBUG_KBD
    Log(("kbd: write cmd=0x%02x\n", val));
#endif
    switch(val) {
    case KBD_CCMD_READ_MODE:
        kbd_queue(s, s->mode, 0);
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
        break;
    case KBD_CCMD_TEST_MOUSE:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_SELF_TEST:
        s->status |= KBD_STAT_SELFTEST;
        kbd_queue(s, 0x55, 0);
        break;
    case KBD_CCMD_KBD_TEST:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_KBD_DISABLE:
        s->mode |= KBD_MODE_DISABLE_KBD;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_KBD_ENABLE:
        s->mode &= ~KBD_MODE_DISABLE_KBD;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_READ_INPORT:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_READ_OUTPORT:
        /* XXX: check that */
#ifdef TARGET_I386
# ifndef VBOX
        val = 0x01 | (ioport_get_a20() << 1);
# else /* VBOX */
        val = 0x01 | (PDMDevHlpA20IsEnabled(CTXSUFF(s->pDevIns)) << 1);
# endif /* VBOX */
#else
        val = 0x01;
#endif
        if (s->status & KBD_STAT_OBF)
            val |= 0x10;
        if (s->status & KBD_STAT_MOUSE_OBF)
            val |= 0x20;
        kbd_queue(s, val, 0);
        break;
#ifdef TARGET_I386
    case KBD_CCMD_ENABLE_A20:
#ifndef VBOX
        ioport_set_a20(1);
#else /* VBOX */
# ifndef IN_RING3
        if (!PDMDevHlpA20IsEnabled(CTXSUFF(s->pDevIns)))
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        PDMDevHlpA20Set(CTXSUFF(s->pDevIns), true);
# endif /* IN_RING3 */
#endif /* VBOX */
        break;
    case KBD_CCMD_DISABLE_A20:
#ifndef VBOX
        ioport_set_a20(0);
#else /* VBOX */
# ifndef IN_RING3
        if (PDMDevHlpA20IsEnabled(CTXSUFF(s->pDevIns)))
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        PDMDevHlpA20Set(CTXSUFF(s->pDevIns), false);
# endif /* !IN_RING3 */
#endif /* VBOX */
        break;
#endif
    case KBD_CCMD_READ_TSTINP:
        /* Keyboard clock line is zero IFF keyboard is disabled */
        val = (s->mode & KBD_MODE_DISABLE_KBD) ? 0 : 1;
        kbd_queue(s, val, 0);
        break;
    case KBD_CCMD_RESET:
#ifndef VBOX
        qemu_system_reset_request();
#else /* VBOX */
# ifndef IN_RING3
        rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        rc = PDMDevHlpVMReset(CTXSUFF(s->pDevIns));
# endif /* !IN_RING3 */
#endif /* VBOX */
        break;
    case 0xff:
        /* ignore that - I don't know what is its use */
        break;
#ifdef VBOX /* Make OS/2 happy. */
    /* The 8042 RAM is readble using commands 0x20 thru 0x3f, and writable 
       by 0x60 thru 0x7f. Now days only the firs byte, the mode, is used.
       We'll ignore the writes (0x61..7f) and return 0 for all the reads
       just to make some OS/2 debug stuff a bit happier. */
    case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        kbd_queue(s, 0, 0);
        Log(("kbd: reading non-standard RAM addr %#x\n", val & 0x1f));
        break;
#endif 
    default:
        Log(("kbd: unsupported keyboard cmd=0x%02x\n", val));
        break;
    }
#ifdef VBOX
    return rc;
#endif
}

static uint32_t kbd_read_data(void *opaque, uint32_t addr)
{
    KBDState *s = (KBDState*)opaque;
    KBDQueue *q;
#ifdef VBOX
    MouseCmdQueue *mcq;
    MouseEventQueue *meq;
#endif /* VBOX */
    int val, index, aux;

    q = &s->queue;
#ifdef VBOX
    mcq = &s->mouse_command_queue;
    meq = &s->mouse_event_queue;
    if (q->count == 0 && mcq->count == 0 && meq->count == 0) {
#else /* !VBOX */
    if (q->count == 0) {
#endif /* !VBOX */
        /* NOTE: if no data left, we return the last keyboard one
           (needed for EMM386) */
        /* XXX: need a timer to do things correctly */
        index = q->rptr - 1;
        if (index < 0)
            index = KBD_QUEUE_SIZE - 1;
        val = q->data[index];
    } else {
#ifdef VBOX
        aux = (s->status & KBD_STAT_MOUSE_OBF);
        if (!aux)
        {
            val = q->data[q->rptr];
            if (++q->rptr == KBD_QUEUE_SIZE)
                q->rptr = 0;
            q->count--;
        }
        else
        {
            if (mcq->count)
            {
                val = mcq->data[mcq->rptr];
                if (++mcq->rptr == MOUSE_CMD_QUEUE_SIZE)
                    mcq->rptr = 0;
                mcq->count--;
            }
            else
            {
                val = meq->data[meq->rptr];
                if (++meq->rptr == MOUSE_EVENT_QUEUE_SIZE)
                    meq->rptr = 0;
                meq->count--;
            }
        }
#else /* !VBOX */
        aux = q->aux[q->rptr];
        val = q->data[q->rptr];
        if (++q->rptr == KBD_QUEUE_SIZE)
            q->rptr = 0;
        q->count--;
#endif /* !VBOX */
        /* reading deasserts IRQ */
#ifndef VBOX
        if (aux)
            pic_set_irq(12, 0);
        else
            pic_set_irq(1, 0);
#else /* VBOX */
        if (aux)
            PDMDevHlpISASetIrq(CTXSUFF(s->pDevIns), 12, 0);
        else
            PDMDevHlpISASetIrq(CTXSUFF(s->pDevIns), 1, 0);
#endif /* VBOX */
    }
    /* reassert IRQs if data left */
    kbd_update_irq(s);
#ifdef DEBUG_KBD
    Log(("kbd: read data=0x%02x\n", val));
#endif
    return val;
}

static void kbd_reset_keyboard(KBDState *s)
{
    s->scan_enabled = 1;
}

#ifndef VBOX
static void kbd_write_keyboard(KBDState *s, int val)
#else
static int  kbd_write_keyboard(KBDState *s, int val)
#endif
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

#ifdef VBOX
static void kbd_mouse_send_packet(KBDState *s, bool fToCmdQueue)
#else /* !VBOX */
static void kbd_mouse_send_packet(KBDState *s)
#endif /* !VBOX */
{
#ifdef VBOX
    int aux = fToCmdQueue ? 1 : 2;
#endif /* VBOX */
    unsigned int b;
    int dx1, dy1, dz1;

    dx1 = s->mouse_dx;
    dy1 = s->mouse_dy;
    dz1 = s->mouse_dz;
    /* XXX: increase range to 8 bits ? */
    if (dx1 > 127)
        dx1 = 127;
    else if (dx1 < -127)
        dx1 = -127;
    if (dy1 > 127)
        dy1 = 127;
    else if (dy1 < -127)
        dy1 = -127;
    b = 0x08 | ((dx1 < 0) << 4) | ((dy1 < 0) << 5) | (s->mouse_buttons & 0x07);
#ifdef VBOX
    kbd_queue(s, b, aux);
    kbd_queue(s, dx1 & 0xff, aux);
    kbd_queue(s, dy1 & 0xff, aux);
#else /* !VBOX */
    kbd_queue(s, b, 1);
    kbd_queue(s, dx1 & 0xff, 1);
    kbd_queue(s, dy1 & 0xff, 1);
#endif /* !VBOX */
    /* extra byte for IMPS/2 or IMEX */
    switch(s->mouse_type) {
    default:
        break;
    case 3:
        if (dz1 > 127)
            dz1 = 127;
        else if (dz1 < -127)
                dz1 = -127;
#ifdef VBOX
        kbd_queue(s, dz1 & 0xff, aux);
#else /* !VBOX */
        kbd_queue(s, dz1 & 0xff, 1);
#endif /* !VBOX */
        break;
    case 4:
        if (dz1 > 7)
            dz1 = 7;
        else if (dz1 < -7)
            dz1 = -7;
        b = (dz1 & 0x0f) | ((s->mouse_buttons & 0x18) << 1);
#ifdef VBOX
        kbd_queue(s, b, aux);
#else /* !VBOX */
        kbd_queue(s, b, 1);
#endif /* !VBOX */
        break;
    }

    /* update deltas */
    s->mouse_dx -= dx1;
    s->mouse_dy -= dy1;
    s->mouse_dz -= dz1;
}

#ifdef IN_RING3
static void pc_kbd_mouse_event(void *opaque,
                               int dx, int dy, int dz, int buttons_state)
{
    KBDState *s = (KBDState*)opaque;

    /* check if deltas are recorded when disabled */
    if (!(s->mouse_status & MOUSE_STATUS_ENABLED))
        return;

    s->mouse_dx += dx;
    s->mouse_dy -= dy;
    s->mouse_dz += dz;
    /* XXX: SDL sometimes generates nul events: we delete them */
    if (s->mouse_dx == 0 && s->mouse_dy == 0 && s->mouse_dz == 0 &&
        s->mouse_buttons == buttons_state)
	return;
    s->mouse_buttons = buttons_state;

#ifdef VBOX
    if (!(s->mouse_status & MOUSE_STATUS_REMOTE) &&
        (s->mouse_event_queue.count < (MOUSE_EVENT_QUEUE_SIZE - 4))) {
        for(;;) {
            /* if not remote, send event. Multiple events are sent if
               too big deltas */
            kbd_mouse_send_packet(s, false);
            if (s->mouse_dx == 0 && s->mouse_dy == 0 && s->mouse_dz == 0)
                break;
        }
    }
#else /* !VBOX */
    if (!(s->mouse_status & MOUSE_STATUS_REMOTE) &&
        (s->queue.count < (KBD_QUEUE_SIZE - 16))) {
        for(;;) {
            /* if not remote, send event. Multiple events are sent if
               too big deltas */
            kbd_mouse_send_packet(s);
            if (s->mouse_dx == 0 && s->mouse_dy == 0 && s->mouse_dz == 0)
                break;
        }
    }
#endif /* !VBOX */
}
#endif /* IN_RING3 */

static void kbd_write_mouse(KBDState *s, int val)
{
#ifdef DEBUG_MOUSE
    Log(("kbd: write mouse 0x%02x\n", val));
#endif
#ifdef VBOX
    /* Flush the mouse command response queue. */
    s->mouse_command_queue.count = 0;
    s->mouse_command_queue.rptr = 0;
    s->mouse_command_queue.wptr = 0;
#endif /* VBOX */
    switch(s->mouse_write_cmd) {
    default:
    case -1:
        /* mouse command */
        if (s->mouse_wrap) {
            if (val == AUX_RESET_WRAP) {
                s->mouse_wrap = 0;
                kbd_queue(s, AUX_ACK, 1);
                return;
            } else if (val != AUX_RESET) {
                kbd_queue(s, val, 1);
                return;
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
#ifdef VBOX
            kbd_mouse_send_packet(s, true);
#else /* !VBOX */
            kbd_mouse_send_packet(s);
#endif /* !VBOX */
            break;
        case AUX_ENABLE_DEV:
            s->mouse_status |= MOUSE_STATUS_ENABLED;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_DISABLE_DEV:
            s->mouse_status &= ~MOUSE_STATUS_ENABLED;
            kbd_queue(s, AUX_ACK, 1);
#ifdef VBOX
            /* Flush the mouse events queue. */
            s->mouse_event_queue.count = 0;
            s->mouse_event_queue.rptr = 0;
            s->mouse_event_queue.wptr = 0;
#endif /* VBOX */
            break;
        case AUX_SET_DEFAULT:
            s->mouse_sample_rate = 100;
            s->mouse_resolution = 2;
            s->mouse_status = 0;
            kbd_queue(s, AUX_ACK, 1);
            break;
        case AUX_RESET:
            s->mouse_sample_rate = 100;
            s->mouse_resolution = 2;
            s->mouse_status = 0;
            s->mouse_type = 0;
            kbd_queue(s, AUX_ACK, 1);
            kbd_queue(s, 0xaa, 1);
            kbd_queue(s, s->mouse_type, 1);
#ifdef VBOX
            /* Flush the mouse events queue. */
            s->mouse_event_queue.count = 0;
            s->mouse_event_queue.rptr = 0;
            s->mouse_event_queue.wptr = 0;
#endif /* VBOX */
            break;
        default:
#ifdef VBOX
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
#endif
            break;
        }
        break;
    case AUX_SET_SAMPLE:
        s->mouse_sample_rate = val;
        /* detect IMPS/2 or IMEX */
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
            else
                s->mouse_detect_state = 0;
            break;
        case 2:
            if (val == 80)
                s->mouse_type = 3; /* IMPS/2 */
            s->mouse_detect_state = 0;
            break;
        case 3:
            if (val == 80)
                s->mouse_type = 4; /* IMEX */
            s->mouse_detect_state = 0;
            break;
        }
        kbd_queue(s, AUX_ACK, 1);
        s->mouse_write_cmd = -1;
        break;
    case AUX_SET_RES:
        s->mouse_resolution = val;
        kbd_queue(s, AUX_ACK, 1);
        s->mouse_write_cmd = -1;
        break;
    }
}

#ifndef VBOX
static void kbd_write_data(void *opaque, uint32_t addr, uint32_t val)
{
#else /* VBOX */
static int kbd_write_data(void *opaque, uint32_t addr, uint32_t val)
{
    int rc = VINF_SUCCESS;
#endif /* VBOX */
    KBDState *s = (KBDState*)opaque;

#ifdef DEBUG_KBD
    Log(("kbd: write data=0x%02x\n", val));
#endif

    switch(s->write_cmd) {
    case 0:
        rc = kbd_write_keyboard(s, val);
        break;
    case KBD_CCMD_WRITE_MODE:
        s->mode = val;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_WRITE_OBUF:
        kbd_queue(s, val, 0);
        break;
    case KBD_CCMD_WRITE_AUX_OBUF:
        kbd_queue(s, val, 1);
        break;
    case KBD_CCMD_WRITE_OUTPORT:
#ifdef TARGET_I386
# ifndef VBOX
        ioport_set_a20((val >> 1) & 1);
# else /* VBOX */
#  ifndef IN_RING3
        if (PDMDevHlpA20IsEnabled(CTXSUFF(s->pDevIns)) != !!(val & 2))
            rc = VINF_IOM_HC_IOPORT_WRITE;
#  else /* IN_RING3 */
        PDMDevHlpA20Set(CTXSUFF(s->pDevIns), !!(val & 2));
#  endif /* !IN_RING3 */
# endif /* VBOX */
#endif
        if (!(val & 1)) {
#ifndef VBOX
            qemu_system_reset_request();
#else /* VBOX */
# ifndef IN_RING3
            rc = VINF_IOM_HC_IOPORT_WRITE;
# else
            rc = PDMDevHlpVMReset(CTXSUFF(s->pDevIns));
# endif
#endif /* VBOX */
        }
        break;
    case KBD_CCMD_WRITE_MOUSE:
        kbd_write_mouse(s, val);
        break;
    default:
        break;
    }
    s->write_cmd = 0;

#ifdef VBOX
    return rc;
#endif
}

#ifdef IN_RING3

static void kbd_reset(void *opaque)
{
    KBDState *s = (KBDState*)opaque;
    KBDQueue *q;
#ifdef VBOX
    MouseCmdQueue *mcq;
    MouseEventQueue *meq;
#endif /* VBOX */

    s->kbd_write_cmd = -1;
    s->mouse_write_cmd = -1;
    s->mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
    s->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
#ifdef VBOX /* Resetting everything, keyword was not working right on NT4 reboot. */
    s->write_cmd = 0;
    s->scan_enabled = 0;
    s->mouse_status = 0;
    s->mouse_resolution = 0;
    s->mouse_sample_rate = 0;
    s->mouse_wrap = 0;
    s->mouse_type = 0;
    s->mouse_detect_state = 0;
    s->mouse_dx = 0;
    s->mouse_dy = 0;
    s->mouse_dz = 0;
    s->mouse_buttons = 0;
#endif
    q = &s->queue;
    q->rptr = 0;
    q->wptr = 0;
    q->count = 0;
#ifdef VBOX
    mcq = &s->mouse_command_queue;
    mcq->rptr = 0;
    mcq->wptr = 0;
    mcq->count = 0;
    meq = &s->mouse_event_queue;
    meq->rptr = 0;
    meq->wptr = 0;
    meq->count = 0;
#endif /* VBOX */
}

static void kbd_save(QEMUFile* f, void* opaque)
{
#ifdef VBOX
    uint32_t    cItems;
    int i;
#endif /* VBOX */
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
    qemu_put_8s(f, &s->mouse_buttons);

#ifdef VBOX
    /*
     * We have to save the queues too.
     */
    cItems = s->queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->queue.rptr; cItems-- > 0; i = (i + 1) % ELEMENTS(s->queue.data))
        SSMR3PutU8(f, s->queue.data[i]);
    Log(("kbd_save: %d keyboard queue items stored\n", s->queue.count));

    cItems = s->mouse_command_queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->mouse_command_queue.rptr; cItems-- > 0; i = (i + 1) % ELEMENTS(s->mouse_command_queue.data))
        SSMR3PutU8(f, s->mouse_command_queue.data[i]);
    Log(("kbd_save: %d mouse command queue items stored\n", s->mouse_command_queue.count));

    cItems = s->mouse_event_queue.count;
    SSMR3PutU32(f, cItems);
    for (i = s->mouse_event_queue.rptr; cItems-- > 0; i = (i + 1) % ELEMENTS(s->mouse_event_queue.data))
        SSMR3PutU8(f, s->mouse_event_queue.data[i]);
    Log(("kbd_save: %d mouse event queue items stored\n", s->mouse_event_queue.count));

    /* terminator */
    SSMR3PutU32(f, ~0);
#endif /* VBOX */
}

static int kbd_load(QEMUFile* f, void* opaque, int version_id)
{
#ifdef VBOX
    uint32_t    u32, i;
    int         rc;
#endif
    KBDState *s = (KBDState*)opaque;

    if (version_id != PCKBD_SAVED_STATE_VERSION)
#ifndef VBOX
        return -EINVAL;
#else
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
#endif
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
    qemu_get_8s(f, &s->mouse_buttons);
#ifdef VBOX
    s->queue.count = 0;
    s->queue.rptr = 0;
    s->queue.wptr = 0;
    s->mouse_command_queue.count = 0;
    s->mouse_command_queue.rptr = 0;
    s->mouse_command_queue.wptr = 0;
    s->mouse_event_queue.count = 0;
    s->mouse_event_queue.rptr = 0;
    s->mouse_event_queue.wptr = 0;

    /*
     * Load the queues
     */
    rc = SSMR3GetU32(f, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 > ELEMENTS(s->queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->queue.data[i]);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    s->queue.wptr = u32 % ELEMENTS(s->queue.data);
    s->queue.count = u32;
    Log(("kbd_load: %d keyboard queue items loaded\n", u32));

    rc = SSMR3GetU32(f, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 > ELEMENTS(s->mouse_command_queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->mouse_command_queue.data[i]);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    s->mouse_command_queue.wptr = u32 % ELEMENTS(s->mouse_command_queue.data);
    s->mouse_command_queue.count = u32;
    Log(("kbd_load: %d mouse command queue items loaded\n", u32));

    rc = SSMR3GetU32(f, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 > ELEMENTS(s->mouse_event_queue.data))
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    for (i = 0; i < u32; i++)
    {
        rc = SSMR3GetU8(f, &s->mouse_event_queue.data[i]);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    s->mouse_event_queue.wptr = u32 % ELEMENTS(s->mouse_event_queue.data);
    s->mouse_event_queue.count = u32;
    Log(("kbd_load: %d mouse event queue items loaded\n", u32));

    /* terminator */
    rc = SSMR3GetU32(f, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
#endif /* VBOX */
    return 0;
}

#ifndef VBOX
void kbd_init(void)
{
    KBDState *s = &kbd_state;

    kbd_reset(s);
    register_savevm("pckbd", 0, 1, kbd_save, kbd_load, s);
    register_ioport_read(0x60, 1, 1, kbd_read_data, s);
    register_ioport_write(0x60, 1, 1, kbd_write_data, s);
    register_ioport_read(0x64, 1, 1, kbd_read_status, s);
    register_ioport_write(0x64, 1, 1, kbd_write_command, s);

    qemu_add_kbd_event_handler(pc_kbd_put_keycode, s);
    qemu_add_mouse_event_handler(pc_kbd_mouse_event, s);
    qemu_register_reset(kbd_reset, s);
}
#endif /* !VBOX */

#endif /* IN_RING3 */


#ifdef VBOX /* innotek code start */

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
        *pu32 = kbd_read_data(PDMINS2DATA(pDevIns, KBDState *), Port);
        Log2(("kbdIOPortDataRead: Port=%#x cb=%d *pu32=%#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
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
        rc = kbd_write_data(PDMINS2DATA(pDevIns, KBDState *), Port, u32);
        Log2(("kbdIOPortDataWrite: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
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
        *pu32 = kbd_read_status(PDMINS2DATA(pDevIns, KBDState *), Port);
        Log2(("kbdIOPortStatusRead: Port=%#x cb=%d -> *pu32=%#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
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
    NOREF(pvUser);
    if (cb == 1)
    {
        int rc = kbd_write_command(PDMINS2DATA(pDevIns, KBDState *), Port, u32);
        Log2(("kbdIOPortCommandWrite: Port=%#x cb=%d u32=%#x rc=%Vrc\n", Port, cb, u32, rc));
        return rc;
    }
    AssertMsgFailed(("Port=%#x cb=%d\n", Port, cb));
    return VINF_SUCCESS;
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
    kbd_save(pSSMHandle, PDMINS2DATA(pDevIns, KBDState *));
    return VINF_SUCCESS;
}


/**
 * Loads a saved keyboard device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) kbdLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    return kbd_load(pSSMHandle, PDMINS2DATA(pDevIns, KBDState *), u32Version);
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  kbdReset(PPDMDEVINS pDevIns)
{
    kbd_reset(PDMINS2DATA(pDevIns, KBDState *));
}


/* -=-=-=-=-=- Keyboard: IBase  -=-=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to the keyboard port base interface (KBDState::Keyboard.Base).
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *)  kbdKeyboardQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    KBDState *pData = (KBDState *)((uintptr_t)pInterface -  RT_OFFSETOF(KBDState, Keyboard.Base));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->Keyboard.Base;
        case PDMINTERFACE_KEYBOARD_PORT:
            return &pData->Keyboard.Port;
        default:
            return NULL;
    }
}


/* -=-=-=-=-=- Keyboard: IKeyboardPort  -=-=-=-=-=- */

/** Converts a keyboard port interface pointer to a keyboard state pointer. */
#define IKEYBOARDPORT_2_KBDSTATE(pInterface) ( (KBDState *)((uintptr_t)pInterface -  RT_OFFSETOF(KBDState, Keyboard.Port)) )

/**
 * Keyboard event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the keyboard port interface (KBDState::Keyboard.Port).
 * @param   u8KeyCode       The keycode.
 */
static DECLCALLBACK(int) kbdKeyboardPutEvent(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode)
{
    KBDState *pData = IKEYBOARDPORT_2_KBDSTATE(pInterface);
    pc_kbd_put_keycode(pData, u8KeyCode);
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Mouse: IBase  -=-=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to the mouse port base interface (KBDState::Mouse.Base).
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *)  kbdMouseQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    KBDState *pData = (KBDState *)((uintptr_t)pInterface -  RT_OFFSETOF(KBDState, Mouse.Base));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->Mouse.Base;
        case PDMINTERFACE_MOUSE_PORT:
            return &pData->Mouse.Port;
        default:
            return NULL;
    }
}


/* -=-=-=-=-=- Mouse: IMousePort  -=-=-=-=-=- */

/** Converts a mouse port interface pointer to a keyboard state pointer. */
#define IMOUSEPORT_2_KBDSTATE(pInterface) ( (KBDState *)((uintptr_t)pInterface -  RT_OFFSETOF(KBDState, Mouse.Port)) )

/**
 * Mouse event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the mouse port interface (KBDState::Mouse.Port).
 * @param   i32DeltaX       The X delta.
 * @param   i32DeltaY       The Y delta.
 * @param   i32DeltaZ       The Z delta.
 * @param   fButtonStates   The button states.
 */
static DECLCALLBACK(int) kbdMousePutEvent(PPDMIMOUSEPORT pInterface, int32_t i32DeltaX, int32_t i32DeltaY, int32_t i32DeltaZ, uint32_t fButtonStates)
{
    KBDState *pData = IMOUSEPORT_2_KBDSTATE(pInterface);
    pc_kbd_mouse_event(pData, i32DeltaX, i32DeltaY, i32DeltaZ, fButtonStates);
    return VINF_SUCCESS;
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
 * @remark  The keyboard controller doesn't support this action, this is just
 *          implemented to try out the driver<->device structure.
 */
static DECLCALLBACK(int)  kbdAttach(PPDMDEVINS pDevIns, unsigned iLUN)
{
    int         rc;
    KBDState   *pData = PDMINS2DATA(pDevIns, KBDState *);
    switch (iLUN)
    {
        /* LUN #0: keyboard */
        case 0:
            rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pData->Keyboard.Base, &pData->Keyboard.pDrvBase, "Keyboard Port");
            if (VBOX_SUCCESS(rc))
            {
                pData->Keyboard.pDrv = (PDMIKEYBOARDCONNECTOR*)(pData->Keyboard.pDrvBase->pfnQueryInterface(pData->Keyboard.pDrvBase, PDMINTERFACE_KEYBOARD_CONNECTOR));
                if (!pData->Keyboard.pDrv)
                {
                    AssertMsgFailed(("LUN #0 doesn't have a keyboard interface! rc=%Vrc\n", rc));
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertMsgFailed(("Failed to attach LUN #0! rc=%Vrc\n", rc));
            break;

        /* LUN #1: aux/mouse */
        case 1:
            rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pData->Mouse.Base, &pData->Mouse.pDrvBase, "Aux (Mouse) Port");
            if (VBOX_SUCCESS(rc))
            {
                pData->Mouse.pDrv = (PDMIMOUSECONNECTOR*)(pData->Mouse.pDrvBase->pfnQueryInterface(pData->Mouse.pDrvBase, PDMINTERFACE_MOUSE_CONNECTOR));
                if (!pData->Mouse.pDrv)
                {
                    AssertMsgFailed(("LUN #1 doesn't have a mouse interface! rc=%Vrc\n", rc));
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #1!\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertMsgFailed(("Failed to attach LUN #1! rc=%Vrc\n", rc));
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
 * @remark  The keyboard controller doesn't support this action, this is just
 *          implemented to try out the driver<->device structure.
 */
static DECLCALLBACK(void)  kbdDetach(PPDMDEVINS pDevIns, unsigned iLUN)
{
#if 0
    /*
     * Reset the interfaces and update the controller state.
     */
    KBDState   *pData = PDMINS2DATA(pDevIns, KBDState *);
    switch (iLUN)
    {
        /* LUN #0: keyboard */
        case 0:
            pData->Keyboard.pDrv = NULL;
            pData->Keyboard.pDrvBase = NULL;
            break;

        /* LUN #1: aux/mouse */
        case 1:
            pData->Mouse.pDrv = NULL;
            pData->Mouse.pDrvBase = NULL;
            break;

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            break;
    }
#endif
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) kdbRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    KBDState   *pData = PDMINS2DATA(pDevIns, KBDState *);
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
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
static DECLCALLBACK(int) kbdConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    KBDState   *pData = PDMINS2DATA(pDevIns, KBDState *);
    int         rc;
    bool        fGCEnabled;
    bool        fR0Enabled;
    Assert(iInstance == 0);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("configuration error: failed to read GCEnabled as boolean. rc=%Vrc\n", rc));
        return rc;
    }
    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("configuration error: failed to read R0Enabled as boolean. rc=%Vrc\n", rc));
        return rc;
    }
    Log(("pckbd: fGCEnabled=%d fR0Enabled=%d\n", fGCEnabled, fR0Enabled));


    /*
     * Initialize the interfaces.
     */
    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pData->Keyboard.Base.pfnQueryInterface  = kbdKeyboardQueryInterface;
    pData->Keyboard.Port.pfnPutEvent        = kbdKeyboardPutEvent;

    pData->Mouse.Base.pfnQueryInterface     = kbdMouseQueryInterface;
    pData->Mouse.Port.pfnPutEvent           = kbdMousePutEvent;

    /*
     * Register I/O ports, save state, keyboard event handler and mouse event handlers.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x60, 1, NULL, kbdIOPortDataWrite,    kbdIOPortDataRead, NULL, NULL,   "PC Keyboard - Data");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x64, 1, NULL, kbdIOPortCommandWrite, kbdIOPortStatusRead, NULL, NULL, "PC Keyboard - Command / Status");
    if (VBOX_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x60, 1, 0, "kbdIOPortDataWrite",    "kbdIOPortDataRead", NULL, NULL,   "PC Keyboard - Data");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x64, 1, 0, "kbdIOPortCommandWrite", "kbdIOPortStatusRead", NULL, NULL, "PC Keyboard - Command / Status");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = pDevIns->pDevHlp->pfnIOPortRegisterR0(pDevIns, 0x60, 1, 0, "kbdIOPortDataWrite",    "kbdIOPortDataRead", NULL, NULL,   "PC Keyboard - Data");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = pDevIns->pDevHlp->pfnIOPortRegisterR0(pDevIns, 0x64, 1, 0, "kbdIOPortCommandWrite", "kbdIOPortStatusRead", NULL, NULL, "PC Keyboard - Command / Status");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    rc = PDMDevHlpSSMRegister(pDevIns, g_DevicePS2KeyboardMouse.szDeviceName, iInstance, PCKBD_SAVED_STATE_VERSION, sizeof(*pData),
                              NULL, kbdSaveExec, NULL,
                              NULL, kbdLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Attach to the keyboard and mouse drivers.
     */
    rc = kbdAttach(pDevIns, 0 /* keyboard LUN # */);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = kbdAttach(pDevIns, 1 /* aux/mouse LUN # */);
    if (VBOX_FAILURE(rc))
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
    /* szDeviceName */
    "pckbd",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "PS/2 Keyboard and Mouse device. Emulates both the keyboard, mouse and the keyboard controller. "
    "LUN #0 is the keyboard connector. "
    "LUN #1 is the aux/mouse connector.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_INPUT,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(KBDState),
    /* pfnConstruct */
    kbdConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    kdbRelocate,
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
    NULL
};

#endif /* IN_RING3 */
#endif /* VBOX */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

