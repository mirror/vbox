#ifdef VBOX
/* $Id$ */
/** @file
 * Advanced Programmable Interrupt Controller (APIC) Device and
 * I/O Advanced Programmable Interrupt Controller (IO-APIC) Device.
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
 * apic.c revision 1.5  @@OSETODO
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include <VBox/pdmdev.h>

#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include "Builtins2.h"
#include "vl_vbox.h"

#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_BASE          (0xfffff<<12)

#ifndef EINVAL
# define EINVAL 1
#endif

#ifdef _MSC_VER
# pragma warning(disable:4244)
#endif

/** @def APIC_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def APIC_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
/** @def IOAPIC_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def IOAPIC_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
#ifdef VBOX_WITH_PDM_LOCK
# define APIC_LOCK(pThis, rc) \
    do { \
        int rc2 = (pThis)->CTXALLSUFF(pApicHlp)->pfnLock((pThis)->CTXSUFF(pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
# define APIC_UNLOCK(pThis) \
    (pThis)->CTXALLSUFF(pApicHlp)->pfnUnlock((pThis)->CTXSUFF(pDevIns))
# define IOAPIC_LOCK(pThis, rc) \
    do { \
        int rc2 = (pThis)->CTXALLSUFF(pIoApicHlp)->pfnLock((pThis)->CTXSUFF(pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
# define IOAPIC_UNLOCK(pThis) (pThis)->CTXALLSUFF(pIoApicHlp)->pfnUnlock((pThis)->CTXSUFF(pDevIns))
#else /* !VBOX_WITH_PDM_LOCK */
# define APIC_LOCK(pThis, rc)   do { } while (0)
# define APIC_UNLOCK(pThis)     do { } while (0)
# define IOAPIC_LOCK(pThis, rc) do { } while (0)
# define IOAPIC_UNLOCK(pThis)   do { } while (0)
#endif /* !VBOX_WITH_PDM_LOCK */


#endif /* VBOX */

/*
 *  APIC support
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef VBOX
#include "vl.h"
#endif

#define DEBUG_APIC
#define DEBUG_IOAPIC

/* APIC Local Vector Table */
#define APIC_LVT_TIMER   0
#define APIC_LVT_THERMAL 1
#define APIC_LVT_PERFORM 2
#define APIC_LVT_LINT0   3
#define APIC_LVT_LINT1   4
#define APIC_LVT_ERROR   5
#define APIC_LVT_NB      6

/* APIC delivery modes */
#define APIC_DM_FIXED	0
#define APIC_DM_LOWPRI	1
#define APIC_DM_SMI	2
#define APIC_DM_NMI	4
#define APIC_DM_INIT	5
#define APIC_DM_SIPI	6
#define APIC_DM_EXTINT	7

/* APIC destination mode */
#define APIC_DESTMODE_FLAT	0xf
#define APIC_DESTMODE_CLUSTER	1

#define APIC_TRIGGER_EDGE  0
#define APIC_TRIGGER_LEVEL 1

#define	APIC_LVT_TIMER_PERIODIC		(1<<17)
#define	APIC_LVT_MASKED			(1<<16)
#define	APIC_LVT_LEVEL_TRIGGER		(1<<15)
#define	APIC_LVT_REMOTE_IRR		(1<<14)
#define	APIC_INPUT_POLARITY		(1<<13)
#define	APIC_SEND_PENDING		(1<<12)

#define IOAPIC_NUM_PINS			0x18

#define ESR_ILLEGAL_ADDRESS (1 << 7)

#define APIC_SV_ENABLE (1 << 8)

#ifdef VBOX
#define APIC_MAX_PATCH_ATTEMPTS         100
#endif

typedef struct APICState {
#ifndef VBOX
    CPUState *cpu_env;
#endif /* !VBOX */
    uint32_t apicbase;
    uint8_t id;
    uint8_t arb_id;
#ifdef VBOX
    uint32_t tpr;
#else
    uint8_t tpr;
#endif
    uint32_t spurious_vec;
    uint8_t log_dest;
    uint8_t dest_mode;
    uint32_t isr[8];  /* in service register */
    uint32_t tmr[8];  /* trigger mode register */
    uint32_t irr[8]; /* interrupt request register */
    uint32_t lvt[APIC_LVT_NB];
    uint32_t esr; /* error register */
    uint32_t icr[2];

    uint32_t divide_conf;
    int count_shift;
    uint32_t initial_count;
#ifdef VBOX
    uint32_t Alignment0;
#endif
    int64_t initial_count_load_time, next_time;
#ifndef VBOX
    QEMUTimer *timer;

    struct APICState *next_apic;
#else /* VBOX */
    /** HC pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC;
    /** Pointer to the APIC HC helpers. */
    PCPDMAPICHLPR3          pApicHlpR3;
    /** The APIC timer - HC Ptr. */
    R3R0PTRTYPE(PTMTIMER)   pTimerHC;
    /** Pointer to the APIC R0 helpers. */
    PCPDMAPICHLPR0          pApicHlpR0;

    /** GC pointer to the device instance. */
    PPDMDEVINSGC    pDevInsGC;
    /** Pointer to the APIC GC helpers. */
    PCPDMAPICHLPGC  pApicHlpGC;
    /** The APIC timer - GC Ptr. */
    PTMTIMERGC      pTimerGC;

    /** Number of attempts made to optimize TPR accesses. */
    uint32_t        ulTPRPatchAttempts;

# ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER     StatMMIOReadGC;
    STAMCOUNTER     StatMMIOReadHC;
    STAMCOUNTER     StatMMIOWriteGC;
    STAMCOUNTER     StatMMIOWriteHC;
    STAMCOUNTER     StatClearedActiveIrq;
# endif
#endif /* VBOX */
} APICState;

struct IOAPICState {
    uint8_t id;
    uint8_t ioregsel;

    uint32_t irr;
    uint64_t ioredtbl[IOAPIC_NUM_PINS];

#ifdef VBOX
    /** HC pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC;
    /** Pointer to the IOAPIC R3 helpers. */
    PCPDMIOAPICHLPR3        pIoApicHlpR3;

    /** GC pointer to the device instance. */
    PPDMDEVINSGC            pDevInsGC;
    /** Pointer to the IOAPIC GC helpers. */
    PCPDMIOAPICHLPGC        pIoApicHlpGC;

    /** Pointer to the IOAPIC R0 helpers. */
    PCPDMIOAPICHLPR0        pIoApicHlpR0;
# if HC_ARCH_BITS == 32
    uint32_t                Alignment0;
# endif
# ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER StatMMIOReadGC;
    STAMCOUNTER StatMMIOReadHC;
    STAMCOUNTER StatMMIOWriteGC;
    STAMCOUNTER StatMMIOWriteHC;
    STAMCOUNTER StatSetIrqGC;
    STAMCOUNTER StatSetIrqHC;
# endif
#endif /* VBOX */
};

#ifdef VBOX
typedef struct IOAPICState IOAPICState;
#endif /* VBOX */

#ifndef VBOX_DEVICE_STRUCT_TESTCASE
#ifndef VBOX
static int apic_io_memory;
static APICState *first_local_apic = NULL;
static int last_apic_id = 0;
#endif /* !VBOX */

static void apic_init_ipi(APICState *s);
static void apic_set_irq(APICState *s, int vector_num, int trigger_mode);
static bool apic_update_irq(APICState *s);

#ifdef VBOX
static uint32_t apic_get_delivery_bitmask(APICState *s, uint8_t dest, uint8_t dest_mode);
__BEGIN_DECLS
PDMBOTHCBDECL(int)  apicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int)  apicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int)  apicGetInterrupt(PPDMDEVINS pDevIns);
PDMBOTHCBDECL(void) apicSetBase(PPDMDEVINS pDevIns, uint64_t val);
PDMBOTHCBDECL(uint64_t) apicGetBase(PPDMDEVINS pDevIns);
PDMBOTHCBDECL(void) apicSetTPR(PPDMDEVINS pDevIns, uint8_t val);
PDMBOTHCBDECL(uint8_t) apicGetTPR(PPDMDEVINS pDevIns);
PDMBOTHCBDECL(void) apicBusDeliverCallback(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode,
                                           uint8_t u8DeliveryMode, uint8_t iVector, uint8_t u8Polarity,
                                           uint8_t u8TriggerMode);
PDMBOTHCBDECL(int)  ioapicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int)  ioapicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(void) ioapicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);

static void apic_update_tpr(APICState *s, uint32_t val);
__END_DECLS
#endif /* VBOX */

#ifndef VBOX
static void apic_bus_deliver(uint32_t deliver_bitmask, uint8_t delivery_mode,
                             uint8_t vector_num, uint8_t polarity,
                             uint8_t trigger_mode)
{
    APICState *apic_iter;
#else /* VBOX */
static void apic_bus_deliver(APICState *s, uint32_t deliver_bitmask, uint8_t delivery_mode,
                             uint8_t vector_num, uint8_t polarity,
                             uint8_t trigger_mode)
{
#endif /* VBOX */

    switch (delivery_mode) {
        case APIC_DM_LOWPRI:
        case APIC_DM_FIXED:
            /* XXX: arbitration */
            break;

        case APIC_DM_SMI:
        case APIC_DM_NMI:
            break;

        case APIC_DM_INIT:
            /* normal INIT IPI sent to processors */
#ifdef VBOX
            apic_init_ipi (s);
#else
            for (apic_iter = first_local_apic; apic_iter != NULL;
                 apic_iter = apic_iter->next_apic) {
                apic_init_ipi(apic_iter);
            }
#endif
            return;

        case APIC_DM_EXTINT:
            /* handled in I/O APIC code */
            break;

        default:
            return;
    }

#ifdef VBOX
    if (deliver_bitmask & (1 << s->id))
        apic_set_irq (s, vector_num, trigger_mode);
#else  /* VBOX */
    for (apic_iter = first_local_apic; apic_iter != NULL;
         apic_iter = apic_iter->next_apic) {
        if (deliver_bitmask & (1 << apic_iter->id))
            apic_set_irq(apic_iter, vector_num, trigger_mode);
    }
#endif /* VBOX */
}

#ifndef VBOX
void cpu_set_apic_base(CPUState *env, uint64_t val)
{
    APICState *s = env->apic_state;
#ifdef DEBUG_APIC
    Log(("cpu_set_apic_base: %016llx\n", val));
#endif

    s->apicbase = (val & 0xfffff000) |
        (s->apicbase & (MSR_IA32_APICBASE_BSP | MSR_IA32_APICBASE_ENABLE));
    /* if disabled, cannot be enabled again */
    if (!(val & MSR_IA32_APICBASE_ENABLE)) {
        s->apicbase &= ~MSR_IA32_APICBASE_ENABLE;
        env->cpuid_features &= ~CPUID_APIC;
        s->spurious_vec &= ~APIC_SV_ENABLE;
    }
}
#else /* VBOX */
PDMBOTHCBDECL(void) apicSetBase(PPDMDEVINS pDevIns, uint64_t val)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    Log(("cpu_set_apic_base: %016RX64\n", val));

    /** @todo If this change is valid immediately, then we should change the MMIO registration! */
    s->apicbase = (val & 0xfffff000) |
        (s->apicbase & (MSR_IA32_APICBASE_BSP | MSR_IA32_APICBASE_ENABLE));
    /* if disabled, cannot be enabled again (until reset) */
    if (!(val & MSR_IA32_APICBASE_ENABLE)) {
        s->apicbase &= ~MSR_IA32_APICBASE_ENABLE;
        s->spurious_vec &= ~APIC_SV_ENABLE;

        /* Clear any pending APIC interrupt action flag. */
        s->CTXALLSUFF(pApicHlp)->pfnClearInterruptFF(s->CTXSUFF(pDevIns));
        s->CTXALLSUFF(pApicHlp)->pfnChangeFeature(pDevIns, false);
    }
}
#endif  /* VBOX */
#ifndef VBOX

uint64_t cpu_get_apic_base(CPUState *env)
{
    APICState *s = env->apic_state;
#ifdef DEBUG_APIC
    Log(("cpu_get_apic_base: %016llx\n", (uint64_t)s->apicbase));
#endif
    return s->apicbase;
}

void cpu_set_apic_tpr(CPUX86State *env, uint8_t val)
{
    APICState *s = env->apic_state;
    s->tpr = (val & 0x0f) << 4;
    apic_update_irq(s);
}

uint8_t cpu_get_apic_tpr(CPUX86State *env)
{
    APICState *s = env->apic_state;
    return s->tpr >> 4;
}

static int fls_bit(int value)
{
    unsigned int ret = 0;

#ifdef HOST_I386
    __asm__ __volatile__ ("bsr %1, %0\n" : "+r" (ret) : "rm" (value));
    return ret;
#else
    if (value > 0xffff)
        value >>= 16, ret = 16;
    if (value > 0xff)
        value >>= 8, ret += 8;
    if (value > 0xf)
        value >>= 4, ret += 4;
    if (value > 0x3)
        value >>= 2, ret += 2;
    return ret + (value >> 1);
#endif
}

static inline void set_bit(uint32_t *tab, int index)
{
    int i, mask;
    i = index >> 5;
    mask = 1 << (index & 0x1f);
    tab[i] |= mask;
}

static inline void reset_bit(uint32_t *tab, int index)
{
    int i, mask;
    i = index >> 5;
    mask = 1 << (index & 0x1f);
    tab[i] &= ~mask;
}


#else /* VBOX */

PDMBOTHCBDECL(uint64_t) apicGetBase(PPDMDEVINS pDevIns)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    Log(("apicGetBase: %016llx\n", (uint64_t)s->apicbase));
    return s->apicbase;
}

PDMBOTHCBDECL(void) apicSetTPR(PPDMDEVINS pDevIns, uint8_t val)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    LogFlow(("apicSetTPR: val=%#x (trp %#x -> %#x)\n", val, s->tpr, (val & 0x0f) << 4));
    apic_update_tpr(s, (val & 0x0f) << 4);
}

PDMBOTHCBDECL(uint8_t) apicGetTPR(PPDMDEVINS pDevIns)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    LogFlow(("apicGetTPR: returns %#x\n", s->tpr >> 4));
    return s->tpr >> 4;
}

/**
 * More or less private interface between IOAPIC, only PDM is responsible
 * for connecting the two devices.
 */
PDMBOTHCBDECL(void) apicBusDeliverCallback(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode,
                                           uint8_t u8DeliveryMode, uint8_t iVector, uint8_t u8Polarity,
                                           uint8_t u8TriggerMode)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    LogFlow(("apicBusDeliverCallback: s=%p pDevIns=%p u8Dest=%#x u8DestMode=%#x u8DeliveryMode=%#x iVector=%#x u8Polarity=%#x u8TriggerMode=%#x\n",
             s, pDevIns, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    apic_bus_deliver(s, apic_get_delivery_bitmask(s, u8Dest, u8DestMode),
                     u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}

# define set_bit(pvBitmap, iBit)    ASMBitSet(pvBitmap, iBit)
# define reset_bit(pvBitmap, iBit)  ASMBitClear(pvBitmap, iBit)
# define fls_bit(value)             (ASMBitLastSetU32(value) - 1)

#endif /* VBOX */

/* return -1 if no bit is set */
static int get_highest_priority_int(uint32_t *tab)
{
    int i;
    for(i = 7; i >= 0; i--) {
        if (tab[i] != 0) {
            return i * 32 + fls_bit(tab[i]);
        }
    }
    return -1;
}

static int apic_get_ppr(APICState *s)
{
    int tpr, isrv, ppr;

    tpr = (s->tpr >> 4);
    isrv = get_highest_priority_int(s->isr);
    if (isrv < 0)
        isrv = 0;
    isrv >>= 4;
    if (tpr >= isrv)
        ppr = s->tpr;
    else
        ppr = isrv << 4;
    return ppr;
}

static int apic_get_arb_pri(APICState *s)
{
    /* XXX: arbitration */
    return 0;
}

/* signal the CPU if an irq is pending */
static bool apic_update_irq(APICState *s)
{
    int irrv, ppr;
    if (!(s->spurious_vec & APIC_SV_ENABLE))
#ifdef VBOX
    {
        /* Clear any pending APIC interrupt action flag. */
        s->CTXALLSUFF(pApicHlp)->pfnClearInterruptFF(s->CTXSUFF(pDevIns));
        return false;
    }
#else
        return false;
#endif /* VBOX */
    irrv = get_highest_priority_int(s->irr);
    if (irrv < 0)
        return false;
    ppr = apic_get_ppr(s);
    if (ppr && (irrv & 0xf0) <= (ppr & 0xf0))
        return false;
#ifndef VBOX
    cpu_interrupt(s->cpu_env, CPU_INTERRUPT_HARD);
#else
    s->CTXALLSUFF(pApicHlp)->pfnSetInterruptFF(s->CTXSUFF(pDevIns));
    return true;
#endif
}

#ifdef VBOX
static void apic_update_tpr(APICState *s, uint32_t val)
{
    bool fIrqIsActive = false;
    bool fIrqWasActive = false;

    fIrqWasActive = apic_update_irq(s);
    s->tpr        = val;
    fIrqIsActive  = apic_update_irq(s);

    /* If an interrupt is pending and now masked, then clear the FF flag. */
    if (fIrqWasActive && !fIrqIsActive)
    {
        Log(("apic_update_tpr: deactivate interrupt that was masked by the TPR update (%x)\n", val));
        STAM_COUNTER_INC(&s->StatClearedActiveIrq);
        s->CTXALLSUFF(pApicHlp)->pfnClearInterruptFF(s->CTXSUFF(pDevIns));
    }
}
#endif

static void apic_set_irq(APICState *s, int vector_num, int trigger_mode)
{
    set_bit(s->irr, vector_num);
    if (trigger_mode)
        set_bit(s->tmr, vector_num);
    else
        reset_bit(s->tmr, vector_num);
    apic_update_irq(s);
}

static void apic_eoi(APICState *s)
{
    int isrv;
    isrv = get_highest_priority_int(s->isr);
    if (isrv < 0)
        return;
    reset_bit(s->isr, isrv);
    /* XXX: send the EOI packet to the APIC bus to allow the I/O APIC to
            set the remote IRR bit for level triggered interrupts. */
    apic_update_irq(s);
}

#ifndef VBOX
static uint32_t apic_get_delivery_bitmask(uint8_t dest, uint8_t dest_mode)
#else /* VBOX */
static uint32_t apic_get_delivery_bitmask(APICState *s, uint8_t dest, uint8_t dest_mode)
#endif /* VBOX */
{
    uint32_t mask = 0;
#ifndef VBOX
    APICState *apic_iter;
#endif /* !VBOX */

    if (dest_mode == 0) {
        if (dest == 0xff)
            mask = 0xff;
        else
            mask = 1 << dest;
    } else {
        /* XXX: cluster mode */
#ifdef VBOX
        if (dest & s->log_dest)
            mask |= 1 << s->id;
#else /* !VBOX */
        for (apic_iter = first_local_apic; apic_iter != NULL;
             apic_iter = apic_iter->next_apic) {
            if (dest & apic_iter->log_dest)
                mask |= (1 << apic_iter->id);
        }
#endif /* !VBOX */
    }

    return mask;
}


static void apic_init_ipi(APICState *s)
{
    int i;

    for(i = 0; i < APIC_LVT_NB; i++)
        s->lvt[i] = 1 << 16; /* mask LVT */
    s->tpr = 0;
    s->spurious_vec = 0xff;
    s->log_dest = 0;
    s->dest_mode = 0xff;
    memset(s->isr, 0, sizeof(s->isr));
    memset(s->tmr, 0, sizeof(s->tmr));
    memset(s->irr, 0, sizeof(s->irr));
    for(i = 0; i < APIC_LVT_NB; i++)
        s->lvt[i] = 1 << 16; /* mask LVT */
    s->esr = 0;
    memset(s->icr, 0, sizeof(s->icr));
    s->divide_conf = 0;
    s->count_shift = 0;
    s->initial_count = 0;
    s->initial_count_load_time = 0;
    s->next_time = 0;
}

static void apic_deliver(APICState *s, uint8_t dest, uint8_t dest_mode,
                         uint8_t delivery_mode, uint8_t vector_num,
                         uint8_t polarity, uint8_t trigger_mode)
{
    uint32_t deliver_bitmask = 0;
    int dest_shorthand = (s->icr[0] >> 18) & 3;
#ifndef VBOX
    APICState *apic_iter;
#endif /* !VBOX */

    switch (delivery_mode) {
        case APIC_DM_LOWPRI:
            /* XXX: serch for focus processor, arbitration */
            dest = s->id;

        case APIC_DM_INIT:
            {
                int trig_mode = (s->icr[0] >> 15) & 1;
                int level = (s->icr[0] >> 14) & 1;
                if (level == 0 && trig_mode == 1) {
#ifdef VBOX
                    if (deliver_bitmask & (1 << s->id)) {
                        s->arb_id = s->id;
                    }
#else /* !VBOX */
                    for (apic_iter = first_local_apic; apic_iter != NULL;
                         apic_iter = apic_iter->next_apic) {
                        if (deliver_bitmask & (1 << apic_iter->id)) {
                            apic_iter->arb_id = apic_iter->id;
                        }
                    }
#endif /* !VBOX */
                    return;
                }
            }
            break;

        case APIC_DM_SIPI:
#ifndef VBOX
            for (apic_iter = first_local_apic; apic_iter != NULL;
                 apic_iter = apic_iter->next_apic) {
                if (deliver_bitmask & (1 << apic_iter->id)) {
                    /* XXX: SMP support */
                    /* apic_startup(apic_iter); */
                }
            }
#endif /* !VBOX */
            return;
    }

    switch (dest_shorthand) {
        case 0:
#ifndef VBOX
            deliver_bitmask = apic_get_delivery_bitmask(dest, dest_mode);
#else /* VBOX */
            deliver_bitmask = apic_get_delivery_bitmask(s, dest, dest_mode);
#endif /* !VBOX */
            break;
        case 1:
            deliver_bitmask = (1 << s->id);
            break;
        case 2:
            deliver_bitmask = 0xffffffff;
            break;
        case 3:
            deliver_bitmask = 0xffffffff & ~(1 << s->id);
            break;
    }

#ifndef VBOX
    apic_bus_deliver(deliver_bitmask, delivery_mode, vector_num, polarity,
                     trigger_mode);
#else /* VBOX */
    apic_bus_deliver(s, deliver_bitmask, delivery_mode, vector_num, polarity,
                     trigger_mode);
#endif /* VBOX */
}

#ifndef VBOX
int apic_get_interrupt(CPUState *env)
{
    APICState *s = env->apic_state;
#else /* VBOX */
PDMBOTHCBDECL(int) apicGetInterrupt(PPDMDEVINS pDevIns)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
#endif /* VBOX */
    int intno;

    /* if the APIC is installed or enabled, we let the 8259 handle the
       IRQs */
    if (!s) {
        Log(("apic_get_interrupt: returns -1 (!s)\n"));
        return -1;
    }
    if (!(s->spurious_vec & APIC_SV_ENABLE)) {
        Log(("apic_get_interrupt: returns -1 (APIC_SV_ENABLE)\n"));
        return -1;
    }

    /* XXX: spurious IRQ handling */
    intno = get_highest_priority_int(s->irr);
    if (intno < 0) {
        Log(("apic_get_interrupt: returns -1 (irr)\n"));
        return -1;
    }
    if (s->tpr && (uint32_t)intno <= s->tpr) {
        Log(("apic_get_interrupt: returns %d (sp)\n", s->spurious_vec & 0xff));
        return s->spurious_vec & 0xff;
    }
    reset_bit(s->irr, intno);
    set_bit(s->isr, intno);
    apic_update_irq(s);
    LogFlow(("apic_get_interrupt: returns %d\n", intno));
    return intno;
}

static uint32_t apic_get_current_count(APICState *s)
{
    int64_t d;
    uint32_t val;
#ifndef VBOX
    d = (qemu_get_clock(vm_clock) - s->initial_count_load_time) >>
        s->count_shift;
#else /* VBOX */
    d = (TMTimerGet(s->CTXSUFF(pTimer)) - s->initial_count_load_time) >>
        s->count_shift;
#endif /* VBOX */
    if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
        /* periodic */
        val = s->initial_count - (d % ((uint64_t)s->initial_count + 1));
    } else {
        if (d >= s->initial_count)
            val = 0;
        else
            val = s->initial_count - d;
    }
    return val;
}

static void apic_timer_update(APICState *s, int64_t current_time)
{
    int64_t next_time, d;

    if (!(s->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED)) {
        d = (current_time - s->initial_count_load_time) >>
            s->count_shift;
        if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
            d = ((d / ((uint64_t)s->initial_count + 1)) + 1) * ((uint64_t)s->initial_count + 1);
        } else {
            if (d >= s->initial_count)
                goto no_timer;
            d = (uint64_t)s->initial_count + 1;
        }
        next_time = s->initial_count_load_time + (d << s->count_shift);
#ifndef VBOX
        qemu_mod_timer(s->timer, next_time);
#else
        TMTimerSet(s->CTXSUFF(pTimer), next_time);
#endif
        s->next_time = next_time;
    } else {
    no_timer:
#ifndef VBOX
        qemu_del_timer(s->timer);
#else
        TMTimerStop(s->CTXSUFF(pTimer));
#endif
    }
}

#ifdef IN_RING3
#ifndef VBOX
static void apic_timer(void *opaque)
{
    APICState *s = opaque;
#else /* VBOX */
static DECLCALLBACK(void) apicTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
# ifdef VBOX_WITH_PDM_LOCK
    s->pApicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);
# endif
#endif /* VBOX */

    if (!(s->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED)) {
        apic_set_irq(s, s->lvt[APIC_LVT_TIMER] & 0xff, APIC_TRIGGER_EDGE);
    }
    apic_timer_update(s, s->next_time);

#ifdef VBOX
    APIC_UNLOCK(s);
#endif
}
#endif /* IN_RING3 */

#ifndef VBOX
static uint32_t apic_mem_readb(void *opaque, target_phys_addr_t addr)
{
    return 0;
}

static uint32_t apic_mem_readw(void *opaque, target_phys_addr_t addr)
{
    return 0;
}

static void apic_mem_writeb(void *opaque, target_phys_addr_t addr, uint32_t val)
{
}

static void apic_mem_writew(void *opaque, target_phys_addr_t addr, uint32_t val)
{
}
#endif /* !VBOX */


#ifndef VBOX
static uint32_t apic_mem_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState *env;
    APICState *s;
#else /* VBOX */
static uint32_t apic_mem_readl(APICState *s, target_phys_addr_t addr)
{
#endif /* VBOX */
    uint32_t val;
    int index;

#ifndef VBOX
    env = cpu_single_env;
    if (!env)
        return 0;
    s = env->apic_state;
#endif /* !VBOX */

    index = (addr >> 4) & 0xff;
    switch(index) {
    case 0x02: /* id */
        val = s->id << 24;
        break;
    case 0x03: /* version */
        val = 0x11 | ((APIC_LVT_NB - 1) << 16); /* version 0x11 */
        break;
    case 0x08:
        val = s->tpr;
        break;
    case 0x09:
        val = apic_get_arb_pri(s);
        break;
    case 0x0a:
        /* ppr */
        val = apic_get_ppr(s);
        break;
#ifdef VBOX
    case 0x0b:
        Log(("apic_mem_readl %x %x -> write only returning 0\n", addr, index));
        val = 0;
        break;
#endif

    case 0x0d:
        val = s->log_dest << 24;
        break;
    case 0x0e:
#ifdef VBOX
        /* Bottom 28 bits are always 1 */
        val = (s->dest_mode << 28) | 0xfffffff;
#else
        val = s->dest_mode << 28;
#endif
        break;
    case 0x0f:
        val = s->spurious_vec;
        break;
#ifndef VBOX
    case 0x10 ... 0x17:
#else /* VBOX */
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
#endif /* VBOX */
        val = s->isr[index & 7];
        break;
#ifndef VBOX
    case 0x18 ... 0x1f:
#else /* VBOX */
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
#endif /* VBOX */
        val = s->tmr[index & 7];
        break;
#ifndef VBOX
    case 0x20 ... 0x27:
#else /* VBOX */
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
#endif /* VBOX */
        val = s->irr[index & 7];
        break;
    case 0x28:
        val = s->esr;
        break;
    case 0x30:
    case 0x31:
        val = s->icr[index & 1];
        break;
#ifndef VBOX
    case 0x32 ... 0x37:
#else /* VBOX */
    case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
#endif /* VBOX */
        val = s->lvt[index - 0x32];
        break;
    case 0x38:
        val = s->initial_count;
        break;
    case 0x39:
        val = apic_get_current_count(s);
        break;
    case 0x3e:
        val = s->divide_conf;
        break;
    default:
        AssertMsgFailed(("apic_mem_readl: unknown index %x\n", index));
        s->esr |= ESR_ILLEGAL_ADDRESS;
        val = 0;
        break;
    }
#ifdef DEBUG_APIC
    Log(("APIC read: %08x = %08x\n", (uint32_t)addr, val));
#endif
    return val;
}

#ifndef VBOX
static void apic_mem_writel(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    CPUState *env;
    APICState *s;
#else /* VBOX */
static int apic_mem_writel(APICState *s, target_phys_addr_t addr, uint32_t val)
{
#endif /* VBOX */
    int index;

#ifndef VBOX
    env = cpu_single_env;
    if (!env)
        return;
    s = env->apic_state;
#endif /* !VBOX */

#ifdef DEBUG_APIC
    Log(("APIC write: %08x = %08x\n", (uint32_t)addr, val));
#endif

    index = (addr >> 4) & 0xff;
    switch(index) {
    case 0x02:
        s->id = (val >> 24);
        break;
    case 0x03:
        Log(("apic_mem_writel: write to version register; ignored\n"));
        break;
    case 0x08:
#ifdef VBOX
        apic_update_tpr(s, val);
#else
        s->tpr = val;
        apic_update_irq(s);
#endif
        break;
    case 0x09:
    case 0x0a:
        Log(("apic_mem_writel: write to read-only register %d ignored\n", index));
        break;
    case 0x0b: /* EOI */
        apic_eoi(s);
        break;
    case 0x0d:
        s->log_dest = val >> 24;
        break;
    case 0x0e:
        s->dest_mode = val >> 28;
        break;
    case 0x0f:
        s->spurious_vec = val & 0x1ff;
        apic_update_irq(s);
        break;
#ifndef VBOX
    case 0x10 ... 0x17:
    case 0x18 ... 0x1f:
    case 0x20 ... 0x27:
    case 0x28:
#else
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28:
        Log(("apic_mem_writel: write to read-only register %d ignored\n", index));
#endif
        break;

    case 0x30:
        s->icr[0] = val;
        apic_deliver(s, (s->icr[1] >> 24) & 0xff, (s->icr[0] >> 11) & 1,
                     (s->icr[0] >> 8) & 7, (s->icr[0] & 0xff),
                     (s->icr[0] >> 14) & 1, (s->icr[0] >> 15) & 1);
        break;
    case 0x31:
        s->icr[1] = val;
        break;
#ifndef VBOX
    case 0x32 ... 0x37:
#else /* VBOX */
    case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
#endif /* VBOX */
        {
            int n = index - 0x32;
            s->lvt[n] = val;
            if (n == APIC_LVT_TIMER)
#ifndef VBOX
                apic_timer_update(s, qemu_get_clock(vm_clock));
#else /* VBOX */
                apic_timer_update(s, TMTimerGet(s->CTXSUFF(pTimer)));
#endif /* VBOX*/
        }
        break;
    case 0x38:
        s->initial_count = val;
#ifndef VBOX
        s->initial_count_load_time = qemu_get_clock(vm_clock);
#else /* VBOX */
        s->initial_count_load_time = TMTimerGet(s->CTXSUFF(pTimer));
#endif /* VBOX*/
        apic_timer_update(s, s->initial_count_load_time);
        break;
    case 0x39:
        Log(("apic_mem_writel: write to read-only register %d ignored\n", index));
        break;
    case 0x3e:
        {
            int v;
            s->divide_conf = val & 0xb;
            v = (s->divide_conf & 3) | ((s->divide_conf >> 1) & 4);
            s->count_shift = (v + 1) & 7;
        }
        break;
    default:
        AssertMsgFailed(("apic_mem_writel: unknown index %x\n", index));
        s->esr |= ESR_ILLEGAL_ADDRESS;
        break;
    }
#ifdef VBOX
    return VINF_SUCCESS;
#endif
}

#ifdef IN_RING3

static void apic_save(QEMUFile *f, void *opaque)
{
    APICState *s = (APICState*)opaque;
    int i;

    qemu_put_be32s(f, &s->apicbase);
    qemu_put_8s(f, &s->id);
    qemu_put_8s(f, &s->arb_id);
#ifdef VBOX
    qemu_put_be32s(f, &s->tpr);
#else
    qemu_put_8s(f, &s->tpr);
#endif
    qemu_put_be32s(f, &s->spurious_vec);
    qemu_put_8s(f, &s->log_dest);
    qemu_put_8s(f, &s->dest_mode);
    for (i = 0; i < 8; i++) {
        qemu_put_be32s(f, &s->isr[i]);
        qemu_put_be32s(f, &s->tmr[i]);
        qemu_put_be32s(f, &s->irr[i]);
    }
    for (i = 0; i < APIC_LVT_NB; i++) {
        qemu_put_be32s(f, &s->lvt[i]);
    }
    qemu_put_be32s(f, &s->esr);
    qemu_put_be32s(f, &s->icr[0]);
    qemu_put_be32s(f, &s->icr[1]);
    qemu_put_be32s(f, &s->divide_conf);
    qemu_put_be32s(f, &s->count_shift);
    qemu_put_be32s(f, &s->initial_count);
    qemu_put_be64s(f, &s->initial_count_load_time);
    qemu_put_be64s(f, &s->next_time);
}

static int apic_load(QEMUFile *f, void *opaque, int version_id)
{
    APICState *s = (APICState*)opaque;
    int i;

    if (version_id != 1)
        return -EINVAL;

    /* XXX: what if the base changes? (registered memory regions) */
    qemu_get_be32s(f, &s->apicbase);
    qemu_get_8s(f, &s->id);
    qemu_get_8s(f, &s->arb_id);
#ifdef VBOX
    qemu_get_be32s(f, &s->tpr);
#else
    qemu_get_8s(f, &s->tpr);
#endif
    qemu_get_be32s(f, &s->spurious_vec);
    qemu_get_8s(f, &s->log_dest);
    qemu_get_8s(f, &s->dest_mode);
    for (i = 0; i < 8; i++) {
        qemu_get_be32s(f, &s->isr[i]);
        qemu_get_be32s(f, &s->tmr[i]);
        qemu_get_be32s(f, &s->irr[i]);
    }
    for (i = 0; i < APIC_LVT_NB; i++) {
        qemu_get_be32s(f, &s->lvt[i]);
    }
    qemu_get_be32s(f, &s->esr);
    qemu_get_be32s(f, &s->icr[0]);
    qemu_get_be32s(f, &s->icr[1]);
    qemu_get_be32s(f, &s->divide_conf);
    qemu_get_be32s(f, (uint32_t *)&s->count_shift);
    qemu_get_be32s(f, (uint32_t *)&s->initial_count);
    qemu_get_be64s(f, (uint64_t *)&s->initial_count_load_time);
    qemu_get_be64s(f, (uint64_t *)&s->next_time);
    return 0;
}

static void apic_reset(void *opaque)
{
    APICState *s = (APICState*)opaque;
#ifdef VBOX
    TMTimerStop(s->CTXSUFF(pTimer));

    /* malc, I've removed the initing duplicated in apic_init_ipi(). This
     * arb_id was left over.. */
    s->arb_id = 0;

    /* Reset should re-enable the APIC. */
    s->apicbase = 0xfee00000 | MSR_IA32_APICBASE_BSP | MSR_IA32_APICBASE_ENABLE;
    s->pApicHlpR3->pfnChangeFeature(s->pDevInsHC, true);

#endif /* VBOX */
    apic_init_ipi(s);
}

#endif /* IN_RING3 */

#ifndef VBOX
static CPUReadMemoryFunc *apic_mem_read[3] = {
    apic_mem_readb,
    apic_mem_readw,
    apic_mem_readl,
};

static CPUWriteMemoryFunc *apic_mem_write[3] = {
    apic_mem_writeb,
    apic_mem_writew,
    apic_mem_writel,
};

int apic_init(CPUState *env)
{
    APICState *s;

    s = qemu_mallocz(sizeof(APICState));
    if (!s)
        return -1;
    env->apic_state = s;
    apic_init_ipi(s);
    s->id = last_apic_id++;
    s->cpu_env = env;
    s->apicbase = 0xfee00000 |
        (s->id ? 0 : MSR_IA32_APICBASE_BSP) | MSR_IA32_APICBASE_ENABLE;

    /* XXX: mapping more APICs at the same memory location */
    if (apic_io_memory == 0) {
        /* NOTE: the APIC is directly connected to the CPU - it is not
           on the global memory bus. */
        apic_io_memory = cpu_register_io_memory(0, apic_mem_read,
                                                apic_mem_write, NULL);
        cpu_register_physical_memory(s->apicbase & ~0xfff, 0x1000,
                                     apic_io_memory);
    }
    s->timer = qemu_new_timer(vm_clock, apic_timer, s);

    register_savevm("apic", 0, 1, apic_save, apic_load, s);
    qemu_register_reset(apic_reset, s);

    s->next_apic = first_local_apic;
    first_local_apic = s;

    return 0;
}
#endif /* !VBOX */

static void ioapic_service(IOAPICState *s)
{
    uint8_t i;
    uint8_t trig_mode;
    uint8_t vector;
    uint8_t delivery_mode;
    uint32_t mask;
    uint64_t entry;
    uint8_t dest;
    uint8_t dest_mode;
    uint8_t polarity;

    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        mask = 1 << i;
        if (s->irr & mask) {
            entry = s->ioredtbl[i];
            if (!(entry & APIC_LVT_MASKED)) {
                trig_mode = ((entry >> 15) & 1);
                dest = entry >> 56;
                dest_mode = (entry >> 11) & 1;
                delivery_mode = (entry >> 8) & 7;
                polarity = (entry >> 13) & 1;
                if (trig_mode == APIC_TRIGGER_EDGE)
                    s->irr &= ~mask;
                if (delivery_mode == APIC_DM_EXTINT)
#ifndef VBOX /* malc: i'm still not so sure about ExtINT delivery */
                    vector = pic_read_irq(isa_pic);
#else /* VBOX */
                {
                    AssertMsgFailed(("Delivery mode ExtINT"));
                    vector = 0xff; /* incorrect but shuts up gcc. */
                }
#endif /* VBOX */
                else
                    vector = entry & 0xff;

#ifndef VBOX
                apic_bus_deliver(apic_get_delivery_bitmask(dest, dest_mode),
                                 delivery_mode, vector, polarity, trig_mode);
#else /* VBOX */
                s->CTXALLSUFF(pIoApicHlp)->pfnApicBusDeliver(s->CTXSUFF(pDevIns),
                                                             dest,
                                                             dest_mode,
                                                             delivery_mode,
                                                             vector,
                                                             polarity,
                                                             trig_mode);
#endif /* VBOX */
            }
        }
    }
}

#ifdef VBOX
static
#endif
void ioapic_set_irq(void *opaque, int vector, int level)
{
    IOAPICState *s = (IOAPICState*)opaque;

    if (vector >= 0 && vector < IOAPIC_NUM_PINS) {
        uint32_t mask = 1 << vector;
        uint64_t entry = s->ioredtbl[vector];

        if ((entry >> 15) & 1) {
            /* level triggered */
            if (level) {
                s->irr |= mask;
                ioapic_service(s);
#ifdef VBOX
                if ((level & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
                    s->irr &= ~mask;
                }
#endif
            } else {
                s->irr &= ~mask;
            }
        } else {
            /* edge triggered */
            if (level) {
                s->irr |= mask;
                ioapic_service(s);
            }
        }
    }
}

static uint32_t ioapic_mem_readl(void *opaque, target_phys_addr_t addr)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int index;
    uint32_t val = 0;

    addr &= 0xff;
    if (addr == 0x00) {
        val = s->ioregsel;
    } else if (addr == 0x10) {
        switch (s->ioregsel) {
            case 0x00:
                val = s->id << 24;
                break;
            case 0x01:
                val = 0x11 | ((IOAPIC_NUM_PINS - 1) << 16); /* version 0x11 */
                break;
            case 0x02:
                val = 0;
                break;
            default:
                index = (s->ioregsel - 0x10) >> 1;
                if (index >= 0 && index < IOAPIC_NUM_PINS) {
                    if (s->ioregsel & 1)
                        val = s->ioredtbl[index] >> 32;
                    else
                        val = s->ioredtbl[index] & 0xffffffff;
                }
        }
#ifdef DEBUG_IOAPIC
        Log(("I/O APIC read: %08x = %08x\n", s->ioregsel, val));
#endif
    }
    return val;
}

static void ioapic_mem_writel(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int index;

    addr &= 0xff;
    if (addr == 0x00)  {
        s->ioregsel = val;
        return;
    } else if (addr == 0x10) {
#ifdef DEBUG_IOAPIC
        Log(("I/O APIC write: %08x = %08x\n", s->ioregsel, val));
#endif
        switch (s->ioregsel) {
            case 0x00:
                s->id = (val >> 24) & 0xff;
                return;
            case 0x01:
            case 0x02:
                return;
            default:
                index = (s->ioregsel - 0x10) >> 1;
                if (index >= 0 && index < IOAPIC_NUM_PINS) {
                    if (s->ioregsel & 1) {
                        s->ioredtbl[index] &= 0xffffffff;
                        s->ioredtbl[index] |= (uint64_t)val << 32;
                    } else {
                        s->ioredtbl[index] &= ~0xffffffffULL;
                        s->ioredtbl[index] |= val;
                    }
                    ioapic_service(s);
                }
        }
    }
}

#ifdef IN_RING3

static void ioapic_save(QEMUFile *f, void *opaque)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int i;

    qemu_put_8s(f, &s->id);
    qemu_put_8s(f, &s->ioregsel);
    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        qemu_put_be64s(f, &s->ioredtbl[i]);
    }
}

static int ioapic_load(QEMUFile *f, void *opaque, int version_id)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int i;

    if (version_id != 1)
        return -EINVAL;

    qemu_get_8s(f, &s->id);
    qemu_get_8s(f, &s->ioregsel);
    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        qemu_get_be64s(f, &s->ioredtbl[i]);
    }
    return 0;
}

static void ioapic_reset(void *opaque)
{
    IOAPICState *s = (IOAPICState*)opaque;
#ifdef VBOX
    PPDMDEVINSR3        pDevIns    = s->pDevInsHC;
    PCPDMIOAPICHLPR3    pIoApicHlp = s->pIoApicHlpR3;
#endif
    int i;

    memset(s, 0, sizeof(*s));
    for(i = 0; i < IOAPIC_NUM_PINS; i++)
        s->ioredtbl[i] = 1 << 16; /* mask LVT */

#ifdef VBOX
    if (pDevIns)
    {
        s->pDevInsHC = pDevIns;
        s->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    }
    if (pIoApicHlp)
    {
        s->pIoApicHlpR3 = pIoApicHlp;
        s->pIoApicHlpGC = s->pIoApicHlpR3->pfnGetGCHelpers(pDevIns);
        s->pIoApicHlpR0 = s->pIoApicHlpR3->pfnGetR0Helpers(pDevIns);
    }
#endif
}

#endif /* IN_RING3 */

#ifndef VBOX
static CPUReadMemoryFunc *ioapic_mem_read[3] = {
    ioapic_mem_readl,
    ioapic_mem_readl,
    ioapic_mem_readl,
};

static CPUWriteMemoryFunc *ioapic_mem_write[3] = {
    ioapic_mem_writel,
    ioapic_mem_writel,
    ioapic_mem_writel,
};

IOAPICState *ioapic_init(void)
{
    IOAPICState *s;
    int io_memory;

    s = qemu_mallocz(sizeof(IOAPICState));
    if (!s)
        return NULL;
    ioapic_reset(s);
    s->id = last_apic_id++;

    io_memory = cpu_register_io_memory(0, ioapic_mem_read,
                                       ioapic_mem_write, s);
    cpu_register_physical_memory(0xfec00000, 0x1000, io_memory);

    register_savevm("ioapic", 0, 1, ioapic_save, ioapic_load, s);
    qemu_register_reset(ioapic_reset, s);

    return s;
}
#endif /* !VBOX */

/* LAPIC */

/* LAPICs MMIO is wrong, in SMP there is no relation between memory range and
   lapic, it's only the CPU that executes this memory access is what matters */

PDMBOTHCBDECL(int) apicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIORead));
    switch (cb)
    {
        case 1:
            *(uint8_t *)pv = 0;
            break;

        case 2:
            *(uint16_t *)pv = 0;
            break;

        case 4:
        {
#if 0 /** @note experimental */
#ifndef IN_RING3
            uint32_t index = (GCPhysAddr >> 4) & 0xff;

            if (    index == 0x08 /* TPR */
                &&  ++s->ulTPRPatchAttempts < APIC_MAX_PATCH_ATTEMPTS)
            {
#ifdef IN_GC
                pDevIns->pDevHlpGC->pfnPATMSetMMIOPatchInfo(pDevIns, GCPhysAddr, &s->tpr);
#else
                RTGCPTR pDevInsGC = PDMINS2DATA_GCPTR(pDevIns);
                pDevIns->pDevHlpR0->pfnPATMSetMMIOPatchInfo(pDevIns, GCPhysAddr, pDevIns + RT_OFFSETOF(APICState, tpr));
#endif
                return VINF_PATM_HC_MMIO_PATCH_READ;
            }
#endif
#endif /* experimental */
            APIC_LOCK(s, VINF_IOM_HC_MMIO_READ);
            *(uint32_t *)pv = apic_mem_readl(s, GCPhysAddr);
            APIC_UNLOCK(s);
            break;
        }
        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) apicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIOWrite));
    switch (cb)
    {
        case 1:
        case 2:
            /* ignore */
            break;

        case 4:
        {
            int rc;
            APIC_LOCK(s, VINF_IOM_HC_MMIO_WRITE);
            rc = apic_mem_writel(s, GCPhysAddr, *(uint32_t *)pv);
            APIC_UNLOCK(s);
            return rc;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) apicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    apic_save(pSSMHandle, s);
    return TMR3TimerSave(s->CTXSUFF(pTimer), pSSMHandle);
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) apicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
    if (apic_load(pSSMHandle, s, u32Version)) {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    return TMR3TimerLoad(s->CTXSUFF(pTimer), pSSMHandle);
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) apicReset(PPDMDEVINS pDevIns)
{
    APICState *s = PDMINS2DATA(pDevIns, APICState *);
#ifdef VBOX_WITH_PDM_LOCK
    s->pApicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);
#endif
    apic_reset(s);
    /* Clear any pending APIC interrupt action flag. */
    s->pApicHlpR3->pfnClearInterruptFF(pDevIns);
    APIC_UNLOCK(s);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) apicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    APICState *pData = PDMINS2DATA(pDevIns, APICState *);
    pData->pDevInsGC  = PDMDEVINS_2_GCPTR(pDevIns);
    pData->pApicHlpGC = pData->pApicHlpR3->pfnGetGCHelpers(pDevIns);
    pData->pTimerGC   = TMTimerGCPtr(pData->CTXSUFF(pTimer));
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) apicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    APICState      *pData = PDMINS2DATA(pDevIns, APICState *);
    PDMAPICREG      ApicReg;
    int             rc;
    int             i;
    bool            fIOAPIC;
    bool            fGCEnabled;
    bool            fR0Enabled;
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IOAPIC\0GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBool (pCfgHandle, "IOAPIC", &fIOAPIC);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fIOAPIC = true;
    else if (VBOX_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));
    Log(("APIC: fGCEnabled=%d\n", fGCEnabled));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("APIC: fR0Enabled=%d\n", fR0Enabled));

    /*
     * Init the data.
     */
    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pData->apicbase  = 0xfee00000 | MSR_IA32_APICBASE_BSP | MSR_IA32_APICBASE_ENABLE;
    for (i = 0; i < APIC_LVT_NB; i++)
        pData->lvt[i] = 1 << 16; /* mask LVT */
    pData->spurious_vec = 0xff;

    /*
     * Register the APIC.
     */
    ApicReg.u32Version          = PDM_APICREG_VERSION;
    ApicReg.pfnGetInterruptHC   = apicGetInterrupt;
    ApicReg.pfnSetBaseHC        = apicSetBase;
    ApicReg.pfnGetBaseHC        = apicGetBase;
    ApicReg.pfnSetTPRHC         = apicSetTPR;
    ApicReg.pfnGetTPRHC         = apicGetTPR;
    ApicReg.pfnBusDeliverHC     = apicBusDeliverCallback;
    if (fGCEnabled) {
        ApicReg.pszGetInterruptGC   = "apicGetInterrupt";
        ApicReg.pszSetBaseGC        = "apicSetBase";
        ApicReg.pszGetBaseGC        = "apicGetBase";
        ApicReg.pszSetTPRGC         = "apicSetTPR";
        ApicReg.pszGetTPRGC         = "apicGetTPR";
        ApicReg.pszBusDeliverGC     = "apicBusDeliverCallback";
    } else {
        ApicReg.pszGetInterruptGC   = NULL;
        ApicReg.pszSetBaseGC        = NULL;
        ApicReg.pszGetBaseGC        = NULL;
        ApicReg.pszSetTPRGC         = NULL;
        ApicReg.pszGetTPRGC         = NULL;
        ApicReg.pszBusDeliverGC     = NULL;
    }
    if (fR0Enabled) {
        ApicReg.pszGetInterruptR0   = "apicGetInterrupt";
        ApicReg.pszSetBaseR0        = "apicSetBase";
        ApicReg.pszGetBaseR0        = "apicGetBase";
        ApicReg.pszSetTPRR0         = "apicSetTPR";
        ApicReg.pszGetTPRR0         = "apicGetTPR";
        ApicReg.pszBusDeliverR0     = "apicBusDeliverCallback";
    } else {
        ApicReg.pszGetInterruptR0   = NULL;
        ApicReg.pszSetBaseR0        = NULL;
        ApicReg.pszGetBaseR0        = NULL;
        ApicReg.pszSetTPRR0         = NULL;
        ApicReg.pszGetTPRR0         = NULL;
        ApicReg.pszBusDeliverR0     = NULL;
    }

    Assert(pDevIns->pDevHlp->pfnAPICRegister);
    rc = pDevIns->pDevHlp->pfnAPICRegister(pDevIns, &ApicReg, &pData->pApicHlpR3);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("APICRegister -> %Vrc\n", rc));
        return rc;
    }
    pData->pApicHlpGC = pData->pApicHlpR3->pfnGetGCHelpers(pDevIns);

    /*
     * The the CPUID feature bit.
     */
    uint32_t u32Eax, u32Ebx, u32Ecx, u32Edx;
    PDMDevHlpGetCpuId(pDevIns, 0, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
    if (u32Eax >= 1)
    {
        if (   fIOAPIC                       /* If IOAPIC is enabled, enable Local APIC in any case */
            || (   u32Ebx == X86_CPUID_VENDOR_INTEL_EBX
                && u32Ecx == X86_CPUID_VENDOR_INTEL_ECX
                && u32Edx == X86_CPUID_VENDOR_INTEL_EDX /* GenuineIntel */)
            || (   u32Ebx == X86_CPUID_VENDOR_AMD_EBX
                && u32Ecx == X86_CPUID_VENDOR_AMD_ECX
                && u32Edx == X86_CPUID_VENDOR_AMD_EDX   /* AuthenticAMD */))
        {
            LogRel(("Activating Local APIC\n"));
            pData->pApicHlpR3->pfnChangeFeature(pDevIns, true);
        }
    }

    /*
     * Register the MMIO range.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, pData->apicbase & ~0xfff, 0x1000, pData,
                               apicMMIOWrite, apicMMIORead, NULL, "APIC Memory");
    if (VBOX_FAILURE(rc))
        return rc;

    if (fGCEnabled) {
        rc = PDMDevHlpMMIORegisterGC(pDevIns, pData->apicbase & ~0xfff, 0x1000, 0,
                                     "apicMMIOWrite", "apicMMIORead", NULL);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    if (fR0Enabled) {
        pData->pApicHlpR0 = pData->pApicHlpR3->pfnGetR0Helpers(pDevIns);

        rc = PDMDevHlpMMIORegisterR0(pDevIns, pData->apicbase & ~0xfff, 0x1000, 0,
                                     "apicMMIOWrite", "apicMMIORead", NULL);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /*
     * Create the APIC timer.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, apicTimer,
                                "APIC Timer", &pData->CTXSUFF(pTimer));
    if (VBOX_FAILURE(rc))
        return rc;
    pData->pTimerGC = TMTimerGCPtr(pData->CTXSUFF(pTimer));

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1 /* version */,
                              sizeof(*pData), NULL, apicSaveExec, NULL, NULL, apicLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatMMIOReadGC,     STAMTYPE_COUNTER,  "/PDM/APIC/MMIOReadGC",   STAMUNIT_OCCURENCES, "Number of APIC MMIO reads in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatMMIOReadHC,     STAMTYPE_COUNTER,  "/PDM/APIC/MMIOReadHC",   STAMUNIT_OCCURENCES, "Number of APIC MMIO reads in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatMMIOWriteGC,    STAMTYPE_COUNTER,  "/PDM/APIC/MMIOWriteGC",  STAMUNIT_OCCURENCES, "Number of APIC MMIO writes in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatMMIOWriteHC,    STAMTYPE_COUNTER,  "/PDM/APIC/MMIOWriteHC",  STAMUNIT_OCCURENCES, "Number of APIC MMIO writes in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pData->StatClearedActiveIrq, STAMTYPE_COUNTER,  "/PDM/APIC/Masked/ActiveIRQ",  STAMUNIT_OCCURENCES, "Number of cleared irqs.");
#endif

    return VINF_SUCCESS;
}


/**
 * APIC device registration structure.
 */
const PDMDEVREG g_DeviceAPIC =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "apic",
    /* szGCMod */
    "VBoxDD2GC.gc",
    /* szR0Mod */
    "VBoxDD2R0.r0",
    /* pszDescription */
    "Advanced Programmable Interrupt Controller (APIC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PIC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(APICState),
    /* pfnConstruct */
    apicConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    apicRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    apicReset,
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




/* IOAPIC */

PDMBOTHCBDECL(int) ioapicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);
    IOAPIC_LOCK(s, VINF_IOM_HC_MMIO_READ);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIORead));
    switch (cb)
    {
        case 1:
            *(uint8_t *)pv = ioapic_mem_readl(s, GCPhysAddr);
            break;

        case 2:
            *(uint16_t *)pv = ioapic_mem_readl(s, GCPhysAddr);
            break;

        case 4:
            *(uint32_t *)pv = ioapic_mem_readl(s, GCPhysAddr);
            break;

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            IOAPIC_UNLOCK(s);
            return VERR_INTERNAL_ERROR;
    }
    IOAPIC_UNLOCK(s);
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) ioapicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIOWrite));
    switch (cb)
    {
        case 1:
        case 2:
        case 4:
            IOAPIC_LOCK(s, VINF_IOM_HC_MMIO_WRITE);
            ioapic_mem_writel(s, GCPhysAddr, *(uint32_t *)pv);
            IOAPIC_UNLOCK(s);
            break;

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(void) ioapicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    IOAPICState *pThis = PDMINS2DATA(pDevIns, IOAPICState *);
    STAM_COUNTER_INC(&pThis->CTXSUFF(StatSetIrq));
    LogFlow(("ioapicSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    ioapic_set_irq(pThis, iIrq, iLevel);
}


#ifdef IN_RING3

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) ioapicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);
    ioapic_save(pSSMHandle, s);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) ioapicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);

    if (ioapic_load(pSSMHandle, s, u32Version)) {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ioapicReset(PPDMDEVINS pDevIns)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);
#ifdef VBOX_WITH_PDM_LOCK
    s->pIoApicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);
#endif
    ioapic_reset(s);
    IOAPIC_UNLOCK(s);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ioapicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);
    s->pDevInsGC    = PDMDEVINS_2_GCPTR(pDevIns);
    s->pIoApicHlpGC = s->pIoApicHlpR3->pfnGetGCHelpers(pDevIns);
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) ioapicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    IOAPICState *s = PDMINS2DATA(pDevIns, IOAPICState *);
    PDMIOAPICREG IoApicReg;
    bool         fGCEnabled;
    bool         fR0Enabled;
    int          rc;

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
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));
    Log(("IOAPIC: fGCEnabled=%d\n", fGCEnabled));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("IOAPIC: fR0Enabled=%d\n", fR0Enabled));

    /*
     * Initialize the state data.
     */
    s->pDevInsHC = pDevIns;
    s->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    ioapic_reset(s);
    s->id = 0;

    /*
     * Register the IOAPIC and get helpers.
     */
    IoApicReg.u32Version  = PDM_IOAPICREG_VERSION;
    IoApicReg.pfnSetIrqHC = ioapicSetIrq;
    IoApicReg.pszSetIrqGC = fGCEnabled ? "ioapicSetIrq" : NULL;
    IoApicReg.pszSetIrqR0 = fR0Enabled ? "ioapicSetIrq" : NULL;
    rc = pDevIns->pDevHlp->pfnIOAPICRegister(pDevIns, &IoApicReg, &s->pIoApicHlpR3);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("IOAPICRegister -> %Vrc\n", rc));
        return rc;
    }
    s->pIoApicHlpGC = s->pIoApicHlpR3->pfnGetGCHelpers(pDevIns);

    /*
     * Register MMIO callbacks and saved state.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, 0xfec00000, 0x1000, s,
                               ioapicMMIOWrite, ioapicMMIORead, NULL, "I/O APIC Memory");
    if (VBOX_FAILURE(rc))
        return rc;

    if (fGCEnabled) {
        rc = PDMDevHlpMMIORegisterGC(pDevIns, 0xfec00000, 0x1000, 0,
                                     "ioapicMMIOWrite", "ioapicMMIORead", NULL);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    if (fR0Enabled) {
        s->pIoApicHlpR0 = s->pIoApicHlpR3->pfnGetR0Helpers(pDevIns);

        rc = PDMDevHlpMMIORegisterR0(pDevIns, 0xfec00000, 0x1000, 0,
                                     "ioapicMMIOWrite", "ioapicMMIORead", NULL);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1 /* version */,
                              sizeof(*s), NULL, ioapicSaveExec, NULL, NULL, ioapicLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOReadGC,     STAMTYPE_COUNTER,  "/PDM/IOAPIC/MMIOReadGC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOReadHC,     STAMTYPE_COUNTER,  "/PDM/IOAPIC/MMIOReadHC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOWriteGC,    STAMTYPE_COUNTER,  "/PDM/IOAPIC/MMIOWriteGC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOWriteHC,    STAMTYPE_COUNTER,  "/PDM/IOAPIC/MMIOWriteHC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatSetIrqGC,       STAMTYPE_COUNTER,  "/PDM/IOAPIC/SetIrqGC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatSetIrqHC,       STAMTYPE_COUNTER,  "/PDM/IOAPIC/SetIrqHC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in HC.");
#endif

    return VINF_SUCCESS;
}

/**
 * IO APIC device registration structure.
 */
const PDMDEVREG g_DeviceIOAPIC =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "ioapic",
    /* szGCMod */
    "VBoxDD2GC.gc",
    /* szR0Mod */
    "VBoxDD2R0.r0",
    /* pszDescription */
    "I/O Advanced Programmable Interrupt Controller (IO-APIC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PIC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(IOAPICState),
    /* pfnConstruct */
    ioapicConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    ioapicRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ioapicReset,
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
    NULL
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

