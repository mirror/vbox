/* $Id$ */
/** @file
 * Intel 8259 Programmable Interrupt Controller (PIC) Device.
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PIC
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <iprt/assert.h>

#include "vl_vbox.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PIC_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def PIC_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
#ifdef VBOX_WITH_PDM_LOCK
# define PIC_LOCK(pThis, rc) \
    do { \
        int rc2 = (pThis)->CTXALLSUFF(pPicHlp)->pfnLock((pThis)->CTXSUFF(pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
# define PIC_UNLOCK(pThis) \
    (pThis)->CTXALLSUFF(pPicHlp)->pfnUnlock((pThis)->CTXSUFF(pDevIns))
#else /* !VBOX_WITH_PDM_LOCK */
# define PIC_LOCK(pThis, rc)   do { } while (0)
# define PIC_UNLOCK(pThis)     do { } while (0)
#endif /* !VBOX_WITH_PDM_LOCK */


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

PDMBOTHCBDECL(void) picSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
PDMBOTHCBDECL(int) picGetInterrupt(PPDMDEVINS pDevIns);
PDMBOTHCBDECL(int) picIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) picIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) picIOPortElcrRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) picIOPortElcrWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);

__END_DECLS
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */


/*
 * QEMU 8259 interrupt controller emulation
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

/* debug PIC */
#define DEBUG_PIC

/*#define DEBUG_IRQ_COUNT*/

typedef struct PicState {
    uint8_t last_irr; /* edge detection */
    uint8_t irr; /* interrupt request register */
    uint8_t imr; /* interrupt mask register */
    uint8_t isr; /* interrupt service register */
    uint8_t priority_add; /* highest irq priority */
    uint8_t irq_base;
    uint8_t read_reg_select;
    uint8_t poll;
    uint8_t special_mask;
    uint8_t init_state;
    uint8_t auto_eoi;
    uint8_t rotate_on_auto_eoi;
    uint8_t special_fully_nested_mode;
    uint8_t init4; /* true if 4 byte init */
    uint8_t elcr; /* PIIX edge/trigger selection*/
    uint8_t elcr_mask;
    /** Pointer to the device instance, HCPtr. */
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC;
    /** Pointer to the device instance, GCPtr. */
    RCPTRTYPE(PPDMDEVINS)   pDevInsGC;
#if HC_ARCH_BITS == 64
    RTRCPTR                 Alignment0;
#endif
} PicState;

/**
 * A PIC device instance data.
 */
typedef struct DEVPIC
{
    /** The two interrupt controllers. */
    PicState                aPics[2];
    /** Pointer to the PIC R3 helpers. */
    PCPDMPICHLPR3           pPicHlpR3;
    /** Pointer to the PIC R0 helpers. */
    PCPDMPICHLPR0           pPicHlpR0;
    /** Pointer to the PIC GC helpers. */
    PCPDMPICHLPGC           pPicHlpGC;
#if HC_ARCH_BITS == 32
    uint32_t                Alignmnet1;
#endif
    /** Pointer to the device instance - GC Ptr. */
    RCPTRTYPE(PPDMDEVINS)   pDevInsGC;
    /** Pointer to the device instance - GC Ptr. */
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC;
#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             StatSetIrqGC;
    STAMCOUNTER             StatSetIrqHC;
    STAMCOUNTER             StatClearedActiveIRQ2;
    STAMCOUNTER             StatClearedActiveMasterIRQ;
    STAMCOUNTER             StatClearedActiveSlaveIRQ;
#endif
} DEVPIC, *PDEVPIC;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
#ifdef LOG_ENABLED
static inline void DumpPICState(PicState *s, const char *szFn)
{
    PDEVPIC pData = PDMINS2DATA(CTXSUFF(s->pDevIns), PDEVPIC);

    Log2(("%s: pic%d: elcr=%x last_irr=%x irr=%x imr=%x isr=%x irq_base=%x\n",
        szFn, (&pData->aPics[0] == s) ? 0 : 1,
          s->elcr, s->last_irr, s->irr, s->imr, s->isr, s->irq_base));
}
#else
# define DumpPICState(pData, szFn) do { } while (0)
#endif

/* set irq level. If an edge is detected, then the IRR is set to 1 */
static inline void pic_set_irq1(PicState *s, int irq, int level)
{
    int mask;
    Log(("pic_set_irq1: irq=%d level=%d\n", irq, level));
    mask = 1 << irq;
    if (s->elcr & mask) {
        /* level triggered */
        if (level) {
            Log2(("pic_set_irq1(ls) irr=%d irrnew=%d\n", s->irr, s->irr | mask));
            s->irr |= mask;
            s->last_irr |= mask;
        } else {
            Log2(("pic_set_irq1(lc) irr=%d irrnew=%d\n", s->irr, s->irr & ~mask));
            s->irr &= ~mask;
            s->last_irr &= ~mask;
        }
    } else {
        /* edge triggered */
        if (level) {
            if ((s->last_irr & mask) == 0)
            {
                Log2(("pic_set_irq1 irr=%x last_irr=%x\n", s->irr | mask, s->last_irr));
                s->irr |= mask;
            }
            s->last_irr |= mask;
        } else {
            s->last_irr &= ~mask;
        }
    }
    DumpPICState(s, "pic_set_irq1");
}

/* return the highest priority found in mask (highest = smallest
   number). Return 8 if no irq */
static inline int get_priority(PicState *s, int mask)
{
    int priority;
    if (mask == 0)
        return 8;
    priority = 0;
    while ((mask & (1 << ((priority + s->priority_add) & 7))) == 0)
        priority++;
    return priority;
}

/* return the pic wanted interrupt. return -1 if none */
static int pic_get_irq(PicState *s)
{
    PicState *pics = &(PDMINS2DATA(CTXSUFF(s->pDevIns), PDEVPIC))->aPics[0];
    int mask, cur_priority, priority;
    Log(("pic_get_irq%d: mask=%x\n", (s == pics) ? 0 : 1, s->irr & ~s->imr));
    DumpPICState(s, "pic_get_irq");

    mask = s->irr & ~s->imr;
    priority = get_priority(s, mask);
    Log(("pic_get_irq: priority=%x\n", priority));
    if (priority == 8)
        return -1;
    /* compute current priority. If special fully nested mode on the
       master, the IRQ coming from the slave is not taken into account
       for the priority computation. */
    mask = s->isr;
    if (s->special_fully_nested_mode && s == &pics[0])
        mask &= ~(1 << 2);
    cur_priority = get_priority(s, mask);
    Log(("pic_get_irq%d: cur_priority=%x pending=%d\n", (s == pics) ? 0 : 1, cur_priority, (priority == 8) ? -1 : (priority + s->priority_add) & 7));
    if (priority < cur_priority) {
        /* higher priority found: an irq should be generated */
        return (priority + s->priority_add) & 7;
    } else {
        return -1;
    }
}

/* raise irq to CPU if necessary. must be called every time the active
   irq may change */
static int pic_update_irq(PDEVPIC pData)
{
    PicState *pics = &pData->aPics[0];
    int irq2, irq;

    /* first look at slave pic */
    irq2 = pic_get_irq(&pics[1]);
    Log(("pic_update_irq irq2=%d\n", irq2));
    if (irq2 >= 0) {
        /* if irq request by slave pic, signal master PIC */
        pic_set_irq1(&pics[0], 2, 1);
        pic_set_irq1(&pics[0], 2, 0);
    }
    /* look at requested irq */
    irq = pic_get_irq(&pics[0]);
    if (irq >= 0)
    {
        /* If irq 2 is pending on the master pic, then there must be one pending on the slave pic too! Otherwise we'll get
         * spurious slave interrupts in picGetInterrupt.
         */
        if (irq != 2 || irq2 != -1)
        {
#if defined(DEBUG_PIC)
            int i;
            for(i = 0; i < 2; i++) {
                Log(("pic%d: imr=%x irr=%x padd=%d\n",
                    i, pics[i].imr, pics[i].irr,
                    pics[i].priority_add));
            }
            Log(("pic: cpu_interrupt\n"));
#endif
            pData->CTXALLSUFF(pPicHlp)->pfnSetInterruptFF(pData->CTXSUFF(pDevIns));
        }
        else
        {
            STAM_COUNTER_INC(&pData->StatClearedActiveIRQ2);
            Log(("pic_update_irq: irq 2 is active, but no interrupt is pending on the slave pic!!\n"));
            /* Clear it here, so lower priority interrupts can still be dispatched. */

            /* if this was the only pending irq, then we must clear the interrupt ff flag */
            pData->CTXALLSUFF(pPicHlp)->pfnClearInterruptFF(pData->CTXSUFF(pDevIns));

            /** @note Is this correct? */
            pics[0].irr &= ~(1 << 2);

            /* Call ourselves again just in case other interrupts are pending */
            return pic_update_irq(pData);
        }
    }
    else
    {
        Log(("pic_update_irq: no interrupt is pending!!\n"));

        /* we must clear the interrupt ff flag */
        pData->CTXALLSUFF(pPicHlp)->pfnClearInterruptFF(pData->CTXSUFF(pDevIns));
    }
    return VINF_SUCCESS;
}

/** @note if an interrupt line state changes from unmasked to masked, then it must be deactivated when currently pending! */
static void pic_update_imr(PDEVPIC pData, PicState *s, uint8_t val)
{
    int       irq, intno;
    PicState *pActivePIC;

    /* Query the current pending irq, if any. */
    pActivePIC = &pData->aPics[0];
    intno = irq = pic_get_irq(pActivePIC);
    if (irq == 2)
    {
        pActivePIC = &pData->aPics[1];
        irq = pic_get_irq(pActivePIC);
        intno = irq + 8;
    }

    /* Update IMR */
    s->imr = val;

    /* If an interrupt is pending and now masked, then clear the FF flag. */
    if (    irq >= 0
        &&  ((1 << irq) & ~pActivePIC->imr) == 0)
    {
        Log(("pic_update_imr: pic0: elcr=%x last_irr=%x irr=%x imr=%x isr=%x irq_base=%x\n",
            pData->aPics[0].elcr, pData->aPics[0].last_irr, pData->aPics[0].irr, pData->aPics[0].imr, pData->aPics[0].isr, pData->aPics[0].irq_base));
        Log(("pic_update_imr: pic1: elcr=%x last_irr=%x irr=%x imr=%x isr=%x irq_base=%x\n",
            pData->aPics[1].elcr, pData->aPics[1].last_irr, pData->aPics[1].irr, pData->aPics[1].imr, pData->aPics[1].isr, pData->aPics[1].irq_base));

        /* Clear pending IRQ 2 on master controller in case of slave interrupt. */
        /** @todo Is this correct? */
        if (intno > 7)
        {
            pData->aPics[0].irr &= ~(1 << 2);
            STAM_COUNTER_INC(&pData->StatClearedActiveSlaveIRQ);
        }
        else
            STAM_COUNTER_INC(&pData->StatClearedActiveMasterIRQ);

        Log(("pic_update_imr: clear pending interrupt %d\n", intno));
        pData->CTXALLSUFF(pPicHlp)->pfnClearInterruptFF(pData->CTXSUFF(pDevIns));
    }
}


/**
 * Set the an IRQ.
 *
 * @param   pDevIns         Device instance of the PICs.
 * @param   iIrq            IRQ number to set.
 * @param   iLevel          IRQ level.
 */
PDMBOTHCBDECL(void) picSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    Assert(pData->CTXSUFF(pDevIns) == pDevIns);
    Assert(pData->aPics[0].CTXSUFF(pDevIns) == pDevIns);
    Assert(pData->aPics[1].CTXSUFF(pDevIns) == pDevIns);
    AssertMsg(iIrq < 16, ("iIrq=%d\n", iIrq));

    Log(("picSetIrq %d %d\n", iIrq, iLevel));
    DumpPICState(&pData->aPics[0], "picSetIrq");
    DumpPICState(&pData->aPics[1], "picSetIrq");
    STAM_COUNTER_INC(&pData->CTXSUFF(StatSetIrq));
    pic_set_irq1(&pData->aPics[iIrq >> 3], iIrq & 7, iLevel & PDM_IRQ_LEVEL_HIGH);
    pic_update_irq(pData);
    if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
    {
        pic_set_irq1(&pData->aPics[iIrq >> 3], iIrq & 7, 0);
        pic_update_irq(pData);
    }
}


/* acknowledge interrupt 'irq' */
static inline void pic_intack(PicState *s, int irq)
{
    if (s->auto_eoi) {
        if (s->rotate_on_auto_eoi)
            s->priority_add = (irq + 1) & 7;
    } else {
        s->isr |= (1 << irq);
    }
    /* We don't clear a level sensitive interrupt here */
    if (!(s->elcr & (1 << irq)))
    {
        Log2(("pic_intack: irr=%x irrnew=%x\n", s->irr, s->irr & ~(1 << irq)));
        s->irr &= ~(1 << irq);
    }
}


/**
 * Get a pending interrupt.
 *
 * @returns Pending interrupt number.
 * @param   pDevIns         Device instance of the PICs.
 */
PDMBOTHCBDECL(int) picGetInterrupt(PPDMDEVINS pDevIns)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    int         irq;
    int         irq2;
    int         intno;

    /* read the irq from the PIC */
    DumpPICState(&pData->aPics[0], "picGetInterrupt");
    DumpPICState(&pData->aPics[1], "picGetInterrupt");

    irq = pic_get_irq(&pData->aPics[0]);
    if (irq >= 0)
    {
        pic_intack(&pData->aPics[0], irq);
        if (irq == 2)
        {
            irq2 = pic_get_irq(&pData->aPics[1]);
            if (irq2 >= 0) {
                pic_intack(&pData->aPics[1], irq2);
            }
            else
            {
                /* spurious IRQ on slave controller (impossible) */
                AssertMsgFailed(("picGetInterrupt: spurious IRQ on slave controller\n"));
                irq2 = 7;
            }
            intno = pData->aPics[1].irq_base + irq2;
            Log2(("picGetInterrupt1: %x base=%x irq=%x\n", intno, pData->aPics[1].irq_base, irq2));
            irq = irq2 + 8;
        }
        else {
            intno = pData->aPics[0].irq_base + irq;
            Log2(("picGetInterrupt0: %x base=%x irq=%x\n", intno, pData->aPics[0].irq_base, irq));
        }
    }
    else
    {
        /* spurious IRQ on host controller (impossible) */
        AssertMsgFailed(("picGetInterrupt: spurious IRQ on master controller\n"));
        irq = 7;
        intno = pData->aPics[0].irq_base + irq;
    }
    pic_update_irq(pData);

    Log(("picGetInterrupt: 0x%02x pending 0:%d 1:%d\n", intno, pic_get_irq(&pData->aPics[0]), pic_get_irq(&pData->aPics[1])));

    return intno;
}

static void pic_reset(PicState *s)
{
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC = s->pDevInsHC;
    RCPTRTYPE(PPDMDEVINS) pDevInsGC = s->pDevInsGC;
    int tmp, tmp2;

    tmp = s->elcr_mask;
    tmp2 = s->elcr;
    memset(s, 0, sizeof(PicState));
    s->elcr_mask = tmp;
    s->elcr = tmp2;
    s->pDevInsHC = pDevInsHC;
    s->pDevInsGC = pDevInsGC;
}


static int pic_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    PicState *s = (PicState*)opaque;
    PDEVPIC     pData = PDMINS2DATA(CTXSUFF(s->pDevIns), PDEVPIC);
    int         rc = VINF_SUCCESS;
    int priority, cmd, irq;

    Log(("pic_write: addr=0x%02x val=0x%02x\n", addr, val));
    addr &= 1;
    if (addr == 0) {
        if (val & 0x10) {
            /* init */
            pic_reset(s);
            /* deassert a pending interrupt */
            pData->CTXALLSUFF(pPicHlp)->pfnClearInterruptFF(pData->CTXSUFF(pDevIns));

            s->init_state = 1;
            s->init4 = val & 1;
            if (val & 0x02)
                AssertReleaseMsgFailed(("single mode not supported"));
            if (val & 0x08)
                AssertReleaseMsgFailed(("level sensitive irq not supported"));
        } else if (val & 0x08) {
            if (val & 0x04)
                s->poll = 1;
            if (val & 0x02)
                s->read_reg_select = val & 1;
            if (val & 0x40)
                s->special_mask = (val >> 5) & 1;
        } else {
            cmd = val >> 5;
            switch(cmd) {
            case 0:
            case 4:
                s->rotate_on_auto_eoi = cmd >> 2;
                break;
            case 1: /* end of interrupt */
            case 5:
            {
                priority = get_priority(s, s->isr);
                if (priority != 8) {
                    irq = (priority + s->priority_add) & 7;
                    Log(("pic_write: EOI prio=%d irq=%d\n", priority, irq));
                    s->isr &= ~(1 << irq);
                    if (cmd == 5)
                        s->priority_add = (irq + 1) & 7;
                    rc = pic_update_irq(pData);
                    Assert(rc == VINF_SUCCESS);
                    DumpPICState(s, "eoi");
                }
                break;
            }
            case 3:
            {
                irq = val & 7;
                Log(("pic_write: EOI2 for irq %d\n", irq));
                s->isr &= ~(1 << irq);
                rc = pic_update_irq(pData);
                Assert(rc == VINF_SUCCESS);
                DumpPICState(s, "eoi2");
                break;
            }
            case 6:
            {
                s->priority_add = (val + 1) & 7;
                Log(("pic_write: lowest priority %d (highest %d)\n", val & 7, s->priority_add));
                rc = pic_update_irq(pData);
                Assert(rc == VINF_SUCCESS);
                break;
            }
            case 7:
            {
                irq = val & 7;
                Log(("pic_write: EOI3 for irq %d\n", irq));
                s->isr &= ~(1 << irq);
                s->priority_add = (irq + 1) & 7;
                rc = pic_update_irq(pData);
                Assert(rc == VINF_SUCCESS);
                DumpPICState(s, "eoi3");
                break;
            }
            default:
                /* no operation */
                break;
            }
        }
    } else {
        switch(s->init_state) {
        case 0:
        {
            /* normal mode */
            pic_update_imr(pData, s, val);

            rc = pic_update_irq(pData);
            Assert(rc == VINF_SUCCESS);
            break;
        }
        case 1:
            s->irq_base = val & 0xf8;
            s->init_state = 2;
            Log(("pic_write: set irq base to %x\n", s->irq_base));
            break;
        case 2:
            if (s->init4) {
                s->init_state = 3;
            } else {
                s->init_state = 0;
            }
            break;
        case 3:
            s->special_fully_nested_mode = (val >> 4) & 1;
            s->auto_eoi = (val >> 1) & 1;
            s->init_state = 0;
            Log(("pic_write: special_fully_nested_mode=%d auto_eoi=%d\n", s->special_fully_nested_mode, s->auto_eoi));
            break;
        }
    }
    return rc;
}


static uint32_t pic_poll_read (PicState *s, uint32_t addr1)
{
    PDEVPIC     pData = PDMINS2DATA(CTXSUFF(s->pDevIns), PDEVPIC);
    PicState   *pics = &pData->aPics[0];
    int ret;

    ret = pic_get_irq(s);
    if (ret >= 0) {
        if (addr1 >> 7) {
            Log2(("pic_poll_read: clear slave irq (isr)\n"));
            pics[0].isr &= ~(1 << 2);
            pics[0].irr &= ~(1 << 2);
        }
        Log2(("pic_poll_read: clear irq %d (isr)\n", ret));
        s->irr &= ~(1 << ret);
        s->isr &= ~(1 << ret);
        if (addr1 >> 7 || ret != 2)
            pic_update_irq(pData);
    } else {
        ret = 0x07;
        pic_update_irq(pData);
    }

    return ret;
}


static uint32_t pic_ioport_read(void *opaque, uint32_t addr1, int *pRC)
{
    PicState *s = (PicState*)opaque;
    unsigned int addr;
    int ret;

    *pRC = VINF_SUCCESS;

    addr = addr1;
    addr &= 1;
    if (s->poll) {
        ret = pic_poll_read(s, addr1);
        s->poll = 0;
    } else {
        if (addr == 0) {
            if (s->read_reg_select)
                ret = s->isr;
            else
                ret = s->irr;
        } else {
            ret = s->imr;
        }
    }
    Log(("pic_read: addr=0x%02x val=0x%02x\n", addr1, ret));
    return ret;
}



#ifdef IN_RING3

static void pic_save(QEMUFile *f, void *opaque)
{
    PicState *s = (PicState*)opaque;

    qemu_put_8s(f, &s->last_irr);
    qemu_put_8s(f, &s->irr);
    qemu_put_8s(f, &s->imr);
    qemu_put_8s(f, &s->isr);
    qemu_put_8s(f, &s->priority_add);
    qemu_put_8s(f, &s->irq_base);
    qemu_put_8s(f, &s->read_reg_select);
    qemu_put_8s(f, &s->poll);
    qemu_put_8s(f, &s->special_mask);
    qemu_put_8s(f, &s->init_state);
    qemu_put_8s(f, &s->auto_eoi);
    qemu_put_8s(f, &s->rotate_on_auto_eoi);
    qemu_put_8s(f, &s->special_fully_nested_mode);
    qemu_put_8s(f, &s->init4);
    qemu_put_8s(f, &s->elcr);
}

static int pic_load(QEMUFile *f, void *opaque, int version_id)
{
    PicState *s = (PicState*)opaque;

    if (version_id != 1)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    qemu_get_8s(f, &s->last_irr);
    qemu_get_8s(f, &s->irr);
    qemu_get_8s(f, &s->imr);
    qemu_get_8s(f, &s->isr);
    qemu_get_8s(f, &s->priority_add);
    qemu_get_8s(f, &s->irq_base);
    qemu_get_8s(f, &s->read_reg_select);
    qemu_get_8s(f, &s->poll);
    qemu_get_8s(f, &s->special_mask);
    qemu_get_8s(f, &s->init_state);
    qemu_get_8s(f, &s->auto_eoi);
    qemu_get_8s(f, &s->rotate_on_auto_eoi);
    qemu_get_8s(f, &s->special_fully_nested_mode);
    qemu_get_8s(f, &s->init4);
    qemu_get_8s(f, &s->elcr);
    return 0;
}
#endif /* IN_RING3 */


/* -=-=-=-=-=- wrappers -=-=-=-=-=- */

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - pointer to the PIC in question.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) picIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    uint32_t    iPic  = (uint32_t)(uintptr_t)pvUser;

    Assert(iPic == 0 || iPic == 1);
    if (cb == 1)
    {
        int rc;
        PIC_LOCK(pData, VINF_IOM_HC_IOPORT_READ);
        *pu32 = pic_ioport_read(&pData->aPics[iPic], Port, &rc);
        PIC_UNLOCK(pData);
        return rc;
    }
    return VERR_IOM_IOPORT_UNUSED;
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - pointer to the PIC in question.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) picIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    uint32_t    iPic  = (uint32_t)(uintptr_t)pvUser;

    Assert(iPic == 0 || iPic == 1);

    if (cb == 1)
    {
        int rc;
        PIC_LOCK(pData, VINF_IOM_HC_IOPORT_WRITE);
        rc = pic_ioport_write(&pData->aPics[iPic], Port, u32);
        PIC_UNLOCK(pData);
        return rc;
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - pointer to the PIC in question.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) picIOPortElcrRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    if (cb == 1)
    {
        PicState *s = (PicState*)pvUser;
        PIC_LOCK(PDMINS2DATA(pDevIns, PDEVPIC), VINF_IOM_HC_IOPORT_READ);
        *pu32 = s->elcr;
        PIC_UNLOCK(PDMINS2DATA(pDevIns, PDEVPIC));
        return VINF_SUCCESS;
    }
    return VERR_IOM_IOPORT_UNUSED;
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - pointer to the PIC in question.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) picIOPortElcrWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    if (cb == 1)
    {
        PicState *s = (PicState*)pvUser;
        PIC_LOCK(PDMINS2DATA(pDevIns, PDEVPIC), VINF_IOM_HC_IOPORT_WRITE);
        s->elcr = u32 & s->elcr_mask;
        PIC_UNLOCK(PDMINS2DATA(pDevIns, PDEVPIC));
    }
    return VINF_SUCCESS;
}


#ifdef IN_RING3

#ifdef DEBUG
/**
 * PIC status info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) picInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PDEVPIC pData = PDMINS2DATA(pDevIns, PDEVPIC);

    /*
     * Show info.
     */
    for (int i=0;i<2;i++)
    {
        pHlp->pfnPrintf(pHlp, "PIC%d:\n", i);
        pHlp->pfnPrintf(pHlp, "  last_irr                  = %02x\n", pData->aPics[i].last_irr);
        pHlp->pfnPrintf(pHlp, "  irr                       = %02x\n", pData->aPics[i].irr);
        pHlp->pfnPrintf(pHlp, "  imr                       = %02x\n", pData->aPics[i].imr);
        pHlp->pfnPrintf(pHlp, "  isr                       = %02x\n", pData->aPics[i].isr);
        pHlp->pfnPrintf(pHlp, "  priority_add              = %02x\n", pData->aPics[i].priority_add);
        pHlp->pfnPrintf(pHlp, "  irq_base                  = %02x\n", pData->aPics[i].irq_base);
        pHlp->pfnPrintf(pHlp, "  read_reg_select           = %02x\n", pData->aPics[i].read_reg_select);
        pHlp->pfnPrintf(pHlp, "  poll                      = %02x\n", pData->aPics[i].poll);
        pHlp->pfnPrintf(pHlp, "  special_mask              = %02x\n", pData->aPics[i].special_mask);
        pHlp->pfnPrintf(pHlp, "  init_state                = %02x\n", pData->aPics[i].init_state);
        pHlp->pfnPrintf(pHlp, "  auto_eoi                  = %02x\n", pData->aPics[i].auto_eoi);
        pHlp->pfnPrintf(pHlp, "  rotate_on_auto_eoi        = %02x\n", pData->aPics[i].rotate_on_auto_eoi);
        pHlp->pfnPrintf(pHlp, "  special_fully_nested_mode = %02x\n", pData->aPics[i].special_fully_nested_mode);
        pHlp->pfnPrintf(pHlp, "  init4                     = %02x\n", pData->aPics[i].init4);
        pHlp->pfnPrintf(pHlp, "  elcr                      = %02x\n", pData->aPics[i].elcr);
        pHlp->pfnPrintf(pHlp, "  elcr_mask                 = %02x\n", pData->aPics[i].elcr_mask);
    }
}
#endif /* DEBUG */

/**
 * Saves a state of the programmable interrupt controller device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) picSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    pic_save(pSSMHandle, &pData->aPics[0]);
    pic_save(pSSMHandle, &pData->aPics[1]);
    return VINF_SUCCESS;
}


/**
 * Loads a saved programmable interrupt controller device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) picLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    int rc = pic_load(pSSMHandle, &pData->aPics[0], u32Version);
    if (VBOX_SUCCESS(rc))
        rc = pic_load(pSSMHandle, &pData->aPics[1], u32Version);
    return rc;
}


/* -=-=-=-=-=- real code -=-=-=-=-=- */

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  picReset(PPDMDEVINS pDevIns)
{
    PDEVPIC     pData = PDMINS2DATA(pDevIns, PDEVPIC);
    unsigned    i;
    LogFlow(("picReset:\n"));
#ifdef VBOX_WITH_PDM_LOCK
    pData->pPicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);
#endif

    for (i = 0; i < ELEMENTS(pData->aPics); i++)
        pic_reset(&pData->aPics[i]);

    PIC_UNLOCK(pData);
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) picRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PDEVPIC         pData = PDMINS2DATA(pDevIns, PDEVPIC);
    unsigned        i;

    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pData->pPicHlpGC = pData->pPicHlpR3->pfnGetGCHelpers(pDevIns);
    for (i = 0; i < ELEMENTS(pData->aPics); i++)
        pData->aPics[i].pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
}


/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int)  picConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PDEVPIC         pData = PDMINS2DATA(pDevIns, PDEVPIC);
    PDMPICREG       PicReg;
    int             rc;
    bool            fGCEnabled;
    bool            fR0Enabled;
    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read GCEnabled as boolean"));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));

    Log(("i8259: fGCEnabled=%d fR0Enabled=%d\n", fGCEnabled, fR0Enabled));

    /*
     * Init the data.
     */
    Assert(ELEMENTS(pData->aPics) == 2);
    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pData->aPics[0].elcr_mask = 0xf8;
    pData->aPics[1].elcr_mask = 0xde;
    pData->aPics[0].pDevInsHC = pDevIns;
    pData->aPics[1].pDevInsHC = pDevIns;
    pData->aPics[0].pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pData->aPics[1].pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);

    /*
     * Register PIC, I/O ports and save state.
     */
    PicReg.u32Version           = PDM_PICREG_VERSION;
    PicReg.pfnSetIrqHC          = picSetIrq;
    PicReg.pfnGetInterruptHC    = picGetInterrupt;
    if (fGCEnabled)
    {
        PicReg.pszSetIrqGC          = "picSetIrq";
        PicReg.pszGetInterruptGC    = "picGetInterrupt";
    }
    else
    {
        PicReg.pszSetIrqGC          = NULL;
        PicReg.pszGetInterruptGC    = NULL;
    }

    if (fR0Enabled)
    {
        PicReg.pszSetIrqR0          = "picSetIrq";
        PicReg.pszGetInterruptR0    = "picGetInterrupt";
    }
    else
    {
        PicReg.pszSetIrqR0          = NULL;
        PicReg.pszGetInterruptR0    = NULL;
    }

    Assert(pDevIns->pDevHlp->pfnPICRegister);
    rc = pDevIns->pDevHlp->pfnPICRegister(pDevIns, &PicReg, &pData->pPicHlpR3);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("PICRegister -> %Vrc\n", rc));
        return rc;
    }
    if (fGCEnabled)
        pData->pPicHlpGC = pData->pPicHlpR3->pfnGetGCHelpers(pDevIns);
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x20, 2, (void *)0, picIOPortWrite, picIOPortRead, NULL, NULL, "i8259 PIC #0");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0xa0, 2, (void *)1, picIOPortWrite, picIOPortRead, NULL, NULL, "i8259 PIC #1");
    if (VBOX_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x20, 2, 0, "picIOPortWrite", "picIOPortRead", NULL, NULL, "i8259 PIC #0");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0xa0, 2, 1, "picIOPortWrite", "picIOPortRead", NULL, NULL, "i8259 PIC #1");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        pData->pPicHlpR0 = pData->pPicHlpR3->pfnGetR0Helpers(pDevIns);

        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x20, 2, 0, "picIOPortWrite", "picIOPortRead", NULL, NULL, "i8259 PIC #0");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0xa0, 2, 1, "picIOPortWrite", "picIOPortRead", NULL, NULL, "i8259 PIC #1");
        if (VBOX_FAILURE(rc))
            return rc;
    }

    rc = PDMDevHlpIOPortRegister(pDevIns, 0x4d0, 1, &pData->aPics[0],
                                 picIOPortElcrWrite, picIOPortElcrRead, NULL, NULL, "i8259 PIC #0 - elcr");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x4d1, 1, &pData->aPics[1],
                                 picIOPortElcrWrite, picIOPortElcrRead, NULL, NULL, "i8259 PIC #1 - elcr");
    if (VBOX_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        RTGCPTR pDataGC = PDMINS2DATA_GCPTR(pDevIns);
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x4d0, 1, pDataGC + RT_OFFSETOF(DEVPIC, aPics[0]),
                                       "picIOPortElcrWrite", "picIOPortElcrRead", NULL, NULL, "i8259 PIC #0 - elcr");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x4d1, 1, pDataGC + RT_OFFSETOF(DEVPIC, aPics[1]),
                                       "picIOPortElcrWrite", "picIOPortElcrRead", NULL, NULL, "i8259 PIC #1 - elcr");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        RTR0PTR pDataR0 = PDMINS2DATA_R0PTR(pDevIns);
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x4d0, 1, pDataR0 + RT_OFFSETOF(DEVPIC, aPics[0]),
                                       "picIOPortElcrWrite", "picIOPortElcrRead", NULL, NULL, "i8259 PIC #0 - elcr");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x4d1, 1, pDataR0 + RT_OFFSETOF(DEVPIC, aPics[1]),
                                       "picIOPortElcrWrite", "picIOPortElcrRead", NULL, NULL, "i8259 PIC #1 - elcr");
        if (VBOX_FAILURE(rc))
            return rc;
    }

    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1 /* version */, sizeof(*pData),
                              NULL, picSaveExec, NULL,
                              NULL, picLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;


#ifdef DEBUG
    /*
     * Register the info item.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "pic", "PIC info.", picInfo);
#endif

    /*
     * Initialize the device state.
     */
    picReset(pDevIns);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatSetIrqGC, STAMTYPE_COUNTER, "/PDM/PIC/SetIrqGC", STAMUNIT_OCCURENCES, "Number of PIC SetIrq calls in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatSetIrqHC, STAMTYPE_COUNTER, "/PDM/PIC/SetIrqHC", STAMUNIT_OCCURENCES, "Number of PIC SetIrq calls in HC.");

    PDMDevHlpSTAMRegister(pDevIns, &pData->StatClearedActiveIRQ2,       STAMTYPE_COUNTER, "/PDM/PIC/Masked/ActiveIRQ2",   STAMUNIT_OCCURENCES, "Number of cleared irq 2.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatClearedActiveMasterIRQ,  STAMTYPE_COUNTER, "/PDM/PIC/Masked/ActiveMaster", STAMUNIT_OCCURENCES, "Number of cleared master irqs.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatClearedActiveSlaveIRQ,   STAMTYPE_COUNTER, "/PDM/PIC/Masked/ActiveSlave",  STAMUNIT_OCCURENCES, "Number of cleared slave irqs.");
#endif

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceI8259 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "i8259",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel 8259 Programmable Interrupt Controller (PIC) Device.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PIC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVPIC),
    /* pfnConstruct */
    picConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    picRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    picReset,
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

