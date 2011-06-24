/* $Id$ */
/** @file
 * Advanced Programmable Interrupt Controller (APIC) Device and
 * I/O Advanced Programmable Interrupt Controller (IO-APIC) Device.
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
 * apic.c revision 1.5  @@OSETODO
 *
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include <VBox/vmm/pdmdev.h>

#include <VBox/log.h>
#include <VBox/vmm/stam.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include <VBox/msi.h>

#include "VBoxDD2.h"
#include "DevApic.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_X2ENABLE      (1<<10)
#define MSR_IA32_APICBASE_BASE          (0xfffff<<12)

#ifdef _MSC_VER
# pragma warning(disable:4244)
#endif

/** The current saved state version.*/
#define APIC_SAVED_STATE_VERSION            3
/** The saved state version used by VirtualBox v3 and earlier.
 * This does not include the config.  */
#define APIC_SAVED_STATE_VERSION_VBOX_30    2
/** Some ancient version... */
#define APIC_SAVED_STATE_VERSION_ANCIENT    1

/* version 0x14: Pentium 4, Xeon; LVT count depends on that */
#define APIC_HW_VERSION                    0x14

/** @def APIC_LOCK
 * Acquires the PDM lock. */
#define APIC_LOCK(a_pDev, rcBusy) \
    do { \
        int rc2 = PDMCritSectEnter((a_pDev)->CTX_SUFF(pCritSect), (rcBusy)); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)

/** @def APIC_LOCK_VOID
 * Acquires the PDM lock and does not expect failure (i.e. ring-3 only!). */
#define APIC_LOCK_VOID(a_pDev, rcBusy) \
    do { \
        int rc2 = PDMCritSectEnter((a_pDev)->CTX_SUFF(pCritSect), (rcBusy)); \
        AssertLogRelRCReturnVoid(rc2); \
    } while (0)

/** @def APIC_UNLOCK
 * Releases the PDM lock. */
#define APIC_UNLOCK(a_pDev) \
    PDMCritSectLeave((a_pDev)->CTX_SUFF(pCritSect))

/** @def APIC_AND_TM_LOCK
 * Acquires the virtual sync clock lock as well as the PDM lock. */
#define APIC_AND_TM_LOCK(a_pDev, a_pAcpi, rcBusy) \
    do { \
        int rc2 = TMTimerLock((a_pAcpi)->CTX_SUFF(pTimer), (rcBusy)); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
        rc2 = PDMCritSectEnter((a_pDev)->CTX_SUFF(pCritSect), (rcBusy)); \
        if (rc2 != VINF_SUCCESS) \
        { \
            TMTimerUnlock((a_pAcpi)->CTX_SUFF(pTimer)); \
            return rc2; \
        } \
    } while (0)

/** @def APIC_AND_TM_UNLOCK
 * Releases the PDM lock as well as the TM virtual sync clock lock.  */
#define APIC_AND_TM_UNLOCK(a_pDev, a_pAcpi) \
    do { \
        TMTimerUnlock((a_pAcpi)->CTX_SUFF(pTimer)); \
        PDMCritSectLeave((a_pDev)->CTX_SUFF(pCritSect)); \
    } while (0)

#define foreach_apic(pDev, mask, code)                    \
    do {                                                  \
        APICState *apic = (pDev)->CTX_SUFF(paLapics);     \
        for (uint32_t i = 0; i < (pDev)->cCpus; i++)      \
        {                                                 \
            if (mask & (1 << (apic->id)))                 \
            {                                             \
                code;                                     \
            }                                             \
            apic++;                                       \
        }                                                 \
    } while (0)

# define set_bit(pvBitmap, iBit)    ASMBitSet(pvBitmap, iBit)
# define reset_bit(pvBitmap, iBit)  ASMBitClear(pvBitmap, iBit)
# define fls_bit(value)             (ASMBitLastSetU32(value) - 1)
# define ffs_bit(value)             (ASMBitFirstSetU32(value) - 1)

#define DEBUG_APIC

/* APIC Local Vector Table */
#define APIC_LVT_TIMER   0
#define APIC_LVT_THERMAL 1
#define APIC_LVT_PERFORM 2
#define APIC_LVT_LINT0   3
#define APIC_LVT_LINT1   4
#define APIC_LVT_ERROR   5
#define APIC_LVT_NB      6

/* APIC delivery modes */
#define APIC_DM_FIXED   0
#define APIC_DM_LOWPRI  1
#define APIC_DM_SMI     2
#define APIC_DM_NMI     4
#define APIC_DM_INIT    5
#define APIC_DM_SIPI    6
#define APIC_DM_EXTINT  7

/* APIC destination mode */
#define APIC_DESTMODE_FLAT      0xf
#define APIC_DESTMODE_CLUSTER   0x0

#define APIC_TRIGGER_EDGE  0
#define APIC_TRIGGER_LEVEL 1

#define APIC_LVT_TIMER_PERIODIC         (1<<17)
#define APIC_LVT_MASKED                 (1<<16)
#define APIC_LVT_LEVEL_TRIGGER          (1<<15)
#define APIC_LVT_REMOTE_IRR             (1<<14)
#define APIC_INPUT_POLARITY             (1<<13)
#define APIC_SEND_PENDING               (1<<12)

#define ESR_ILLEGAL_ADDRESS (1 << 7)

#define APIC_SV_ENABLE (1 << 8)

#define APIC_MAX_PATCH_ATTEMPTS         100

typedef uint32_t PhysApicId;
typedef uint32_t LogApicId;


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct APICState {
    uint32_t apicbase;
    /* Task priority register (interrupt level) */
    uint32_t   tpr;
    /* Logical APIC id - user programmable */
    LogApicId  id;
    /* Physical APIC id - not visible to user, constant */
    PhysApicId phys_id;
    /** @todo: is it logical or physical? Not really used anyway now. */
    PhysApicId arb_id;
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
    uint32_t Alignment0;

    /** The time stamp of the initial_count load, i.e. when it was started. */
    uint64_t                initial_count_load_time;
    /** The time stamp of the next timer callback. */
    uint64_t                next_time;
     /** The APIC timer - R3 Ptr. */
    PTMTIMERR3              pTimerR3;
    /** The APIC timer - R0 Ptr. */
    PTMTIMERR0              pTimerR0;
    /** The APIC timer - RC Ptr. */
    PTMTIMERRC              pTimerRC;
    /** Whether the timer is armed or not */
    bool                    fTimerArmed;
    /** Alignment */
    bool                    afAlignment[3];
    /** The initial_count value used for the current frequency hint. */
    uint32_t                uHintedInitialCount;
    /** The count_shift value used for the current frequency hint. */
    uint32_t                uHintedCountShift;
    /** Timer description timer. */
    R3PTRTYPE(char *)       pszDesc;
# ifdef VBOX_WITH_STATISTICS
#  if HC_ARCH_BITS == 32
    uint32_t                u32Alignment0;
#  endif
    STAMCOUNTER             StatTimerSetInitialCount;
    STAMCOUNTER             StatTimerSetInitialCountArm;
    STAMCOUNTER             StatTimerSetInitialCountDisarm;
    STAMCOUNTER             StatTimerSetLvt;
    STAMCOUNTER             StatTimerSetLvtClearPeriodic;
    STAMCOUNTER             StatTimerSetLvtPostponed;
    STAMCOUNTER             StatTimerSetLvtArmed;
    STAMCOUNTER             StatTimerSetLvtArm;
    STAMCOUNTER             StatTimerSetLvtArmRetries;
    STAMCOUNTER             StatTimerSetLvtNoRelevantChange;
# endif

} APICState;

AssertCompileMemberAlignment(APICState, initial_count_load_time, 8);
# ifdef VBOX_WITH_STATISTICS
AssertCompileMemberAlignment(APICState, StatTimerSetInitialCount, 8);
# endif

typedef struct
{
    /** The device instance - R3 Ptr. */
    PPDMDEVINSR3            pDevInsR3;
    /** The APIC helpers - R3 Ptr. */
    PCPDMAPICHLPR3          pApicHlpR3;
    /** LAPICs states - R3 Ptr */
    R3PTRTYPE(APICState *)  paLapicsR3;
    /** The critical section - R3 Ptr. */
    R3PTRTYPE(PPDMCRITSECT) pCritSectR3;

    /** The device instance - R0 Ptr. */
    PPDMDEVINSR0            pDevInsR0;
    /** The APIC helpers - R0 Ptr. */
    PCPDMAPICHLPR0          pApicHlpR0;
    /** LAPICs states - R0 Ptr */
    R0PTRTYPE(APICState *)  paLapicsR0;
    /** The critical section - R3 Ptr. */
    R0PTRTYPE(PPDMCRITSECT) pCritSectR0;

    /** The device instance - RC Ptr. */
    PPDMDEVINSRC            pDevInsRC;
    /** The APIC helpers - RC Ptr. */
    PCPDMAPICHLPRC          pApicHlpRC;
    /** LAPICs states - RC Ptr */
    RCPTRTYPE(APICState *)  paLapicsRC;
    /** The critical section - R3 Ptr. */
    RCPTRTYPE(PPDMCRITSECT) pCritSectRC;

    /** APIC specification version in this virtual hardware configuration. */
    PDMAPICVERSION          enmVersion;

    /** Number of attempts made to optimize TPR accesses. */
    uint32_t                cTPRPatchAttempts;

    /** Number of CPUs on the system (same as LAPIC count). */
    uint32_t                cCpus;
    /** Whether we've got an IO APIC or not. */
    bool                    fIoApic;
    /** Alignment padding. */
    bool                    afPadding[3];

# ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             StatMMIOReadGC;
    STAMCOUNTER             StatMMIOReadHC;
    STAMCOUNTER             StatMMIOWriteGC;
    STAMCOUNTER             StatMMIOWriteHC;
    STAMCOUNTER             StatClearedActiveIrq;
# endif
} APICDeviceInfo;
# ifdef VBOX_WITH_STATISTICS
AssertCompileMemberAlignment(APICDeviceInfo, StatMMIOReadGC, 8);
# endif

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void apic_update_tpr(APICDeviceInfo *pDev, APICState* s, uint32_t val);

static void apic_eoi(APICDeviceInfo *pDev, APICState* s); /*  */
static uint32_t apic_get_delivery_bitmask(APICDeviceInfo* pDev, uint8_t dest, uint8_t dest_mode);
static int apic_deliver(APICDeviceInfo* pDev, APICState *s,
                        uint8_t dest, uint8_t dest_mode,
                        uint8_t delivery_mode, uint8_t vector_num,
                        uint8_t polarity, uint8_t trigger_mode);
static int apic_get_arb_pri(APICState *s);
static int apic_get_ppr(APICState *s);
static uint32_t apic_get_current_count(APICDeviceInfo* pDev, APICState *s);
static void apicTimerSetInitialCount(APICDeviceInfo *pDev, APICState *s, uint32_t initial_count);
static void apicTimerSetLvt(APICDeviceInfo *pDev, APICState *pApic, uint32_t fNew);
static void apicSendInitIpi(APICDeviceInfo* pDev, APICState *s);

static void apic_init_ipi(APICDeviceInfo* pDev, APICState *s);
static void apic_set_irq(APICDeviceInfo* pDev, APICState *s, int vector_num, int trigger_mode);
static bool apic_update_irq(APICDeviceInfo* pDev, APICState *s);


DECLINLINE(APICState*) getLapicById(APICDeviceInfo *pDev, VMCPUID id)
{
    AssertFatalMsg(id < pDev->cCpus, ("CPU id %d out of range\n", id));
    return &pDev->CTX_SUFF(paLapics)[id];
}

DECLINLINE(APICState*) getLapic(APICDeviceInfo* pDev)
{
    /* LAPIC's array is indexed by CPU id */
    VMCPUID id = pDev->CTX_SUFF(pApicHlp)->pfnGetCpuId(pDev->CTX_SUFF(pDevIns));
    return getLapicById(pDev, id);
}

DECLINLINE(VMCPUID) getCpuFromLapic(APICDeviceInfo* pDev, APICState *s)
{
    /* for now we assume LAPIC physical id == CPU id */
    return VMCPUID(s->phys_id);
}

DECLINLINE(void) cpuSetInterrupt(APICDeviceInfo* pDev, APICState *s, PDMAPICIRQ enmType = PDMAPICIRQ_HARDWARE)
{
    LogFlow(("apic: setting interrupt flag for cpu %d\n", getCpuFromLapic(pDev, s)));
    pDev->CTX_SUFF(pApicHlp)->pfnSetInterruptFF(pDev->CTX_SUFF(pDevIns), enmType,
                                               getCpuFromLapic(pDev, s));
}

DECLINLINE(void) cpuClearInterrupt(APICDeviceInfo* pDev, APICState *s, PDMAPICIRQ enmType = PDMAPICIRQ_HARDWARE)
{
    LogFlow(("apic: clear interrupt flag\n"));
    pDev->CTX_SUFF(pApicHlp)->pfnClearInterruptFF(pDev->CTX_SUFF(pDevIns), enmType,
                                                 getCpuFromLapic(pDev, s));
}

# ifdef IN_RING3

DECLINLINE(void) cpuSendSipi(APICDeviceInfo* pDev, APICState *s, int vector)
{
    Log2(("apic: send SIPI vector=%d\n", vector));

    pDev->pApicHlpR3->pfnSendSipi(pDev->pDevInsR3,
                                 getCpuFromLapic(pDev, s),
                                 vector);
}

DECLINLINE(void) cpuSendInitIpi(APICDeviceInfo* pDev, APICState *s)
{
    Log2(("apic: send init IPI\n"));

    pDev->pApicHlpR3->pfnSendInitIpi(pDev->pDevInsR3,
                                    getCpuFromLapic(pDev, s));
}

# endif /* IN_RING3 */

DECLINLINE(uint32_t) getApicEnableBits(APICDeviceInfo* pDev)
{
    switch (pDev->enmVersion)
    {
        case PDMAPICVERSION_NONE:
            return 0;
        case PDMAPICVERSION_APIC:
            return MSR_IA32_APICBASE_ENABLE;
        case PDMAPICVERSION_X2APIC:
            return MSR_IA32_APICBASE_ENABLE | MSR_IA32_APICBASE_X2ENABLE ;
        default:
            AssertMsgFailed(("Unsupported APIC version %d\n", pDev->enmVersion));
            return 0;
    }
}

DECLINLINE(PDMAPICVERSION) getApicMode(APICState *apic)
{
    switch (((apic->apicbase) >> 10) & 0x3)
    {
        case 0:
            return PDMAPICVERSION_NONE;
        case 1:
        default:
            /* Invalid */
            return PDMAPICVERSION_NONE;
        case 2:
            return PDMAPICVERSION_APIC;
        case 3:
            return PDMAPICVERSION_X2APIC;
    }
}

static int apic_bus_deliver(APICDeviceInfo* pDev,
                            uint32_t deliver_bitmask, uint8_t delivery_mode,
                            uint8_t vector_num, uint8_t polarity,
                            uint8_t trigger_mode)
{
    LogFlow(("apic_bus_deliver mask=%x mode=%x vector=%x polarity=%x trigger_mode=%x\n", deliver_bitmask, delivery_mode, vector_num, polarity, trigger_mode));
    switch (delivery_mode) {
        case APIC_DM_LOWPRI:
        {
            int d = -1;
            if (deliver_bitmask)
                d = ffs_bit(deliver_bitmask);
            if (d >= 0)
            {
                APICState* apic = getLapicById(pDev, d);
                apic_set_irq(pDev, apic, vector_num, trigger_mode);
            }
            return VINF_SUCCESS;
        }
        case APIC_DM_FIXED:
            /* XXX: arbitration */
            break;

        case APIC_DM_SMI:
            foreach_apic(pDev, deliver_bitmask,
                         cpuSetInterrupt(pDev, apic, PDMAPICIRQ_SMI));
            return VINF_SUCCESS;

        case APIC_DM_NMI:
            foreach_apic(pDev, deliver_bitmask,
                         cpuSetInterrupt(pDev, apic, PDMAPICIRQ_NMI));
            return VINF_SUCCESS;

        case APIC_DM_INIT:
            /* normal INIT IPI sent to processors */
#ifdef IN_RING3
            foreach_apic(pDev, deliver_bitmask,
                         apicSendInitIpi(pDev, apic));
            return VINF_SUCCESS;
#else
            /* We shall send init IPI only in R3, R0 calls should be
               rescheduled to R3 */
            return VINF_IOM_HC_MMIO_READ_WRITE;
#endif /* IN_RING3 */
        case APIC_DM_EXTINT:
            /* handled in I/O APIC code */
            break;

        default:
            return VINF_SUCCESS;
    }

    foreach_apic(pDev, deliver_bitmask,
                       apic_set_irq (pDev, apic, vector_num, trigger_mode));
    return VINF_SUCCESS;
}


PDMBOTHCBDECL(void) apicSetBase(PPDMDEVINS pDevIns, uint64_t val)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));
    APICState *s = getLapic(pDev); /** @todo fix interface */
    Log(("apicSetBase: %016RX64\n", val));

    /** @todo: do we need to lock here ? */
    /* APIC_LOCK_VOID(pDev, VERR_INTERNAL_ERROR); */
    /** @todo If this change is valid immediately, then we should change the MMIO registration! */
    /* We cannot change if this CPU is BSP or not by writing to MSR - it's hardwired */
    PDMAPICVERSION oldMode = getApicMode(s);
    s->apicbase =
            (val & 0xfffff000) | /* base */
            (val & getApicEnableBits(pDev)) | /* mode */
            (s->apicbase & MSR_IA32_APICBASE_BSP) /* keep BSP bit */;
    PDMAPICVERSION newMode = getApicMode(s);

    if (oldMode != newMode)
    {
        switch (newMode)
        {
            case PDMAPICVERSION_NONE:
            {
                s->spurious_vec &= ~APIC_SV_ENABLE;
                /* Clear any pending APIC interrupt action flag. */
                cpuClearInterrupt(pDev, s);
                /** @todo: why do we do that? */
                pDev->CTX_SUFF(pApicHlp)->pfnChangeFeature(pDevIns, PDMAPICVERSION_NONE);
                break;
            }
            case PDMAPICVERSION_APIC:
                /** @todo: map MMIO ranges, if needed */
                break;
            case PDMAPICVERSION_X2APIC:
                /** @todo: unmap MMIO ranges of this APIC, according to the spec */
                break;
            default:
                break;
        }
    }
    /* APIC_UNLOCK(pDev); */
}

PDMBOTHCBDECL(uint64_t) apicGetBase(PPDMDEVINS pDevIns)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));
    APICState *s = getLapic(pDev); /** @todo fix interface */
    LogFlow(("apicGetBase: %016llx\n", (uint64_t)s->apicbase));
    return s->apicbase;
}

PDMBOTHCBDECL(void) apicSetTPR(PPDMDEVINS pDevIns, VMCPUID idCpu, uint8_t val)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));
    APICState *s = getLapicById(pDev, idCpu);
    LogFlow(("apicSetTPR: val=%#x (trp %#x -> %#x)\n", val, s->tpr, val));
    apic_update_tpr(pDev, s, val);
}

PDMBOTHCBDECL(uint8_t) apicGetTPR(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    /* We don't perform any locking here as that would cause a lot of contention for VT-x/AMD-V. */
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState *s = getLapicById(pDev, idCpu);
    Log2(("apicGetTPR: returns %#x\n", s->tpr));
    return s->tpr;
}

/**
 * Writes to an APIC register via MMIO or MSR.
 *
 * @returns Strict VBox status code.
 * @param   pDev                The PDM device instance.
 * @param   pApic               The APIC being written to.
 * @param   iReg                The APIC register index.
 * @param   u64Value            The value being written.
 * @param   rcBusy              The busy return code to employ.  See
 *                              PDMCritSectEnter for a description.
 * @param   fMsr                Set if called via MSR, clear if MMIO.
 */
static int apicWriteRegister(APICDeviceInfo *pDev, APICState *pApic, uint32_t iReg, uint64_t u64Value,
                             int rcBusy, bool fMsr)
{
    Assert(!PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));

    int rc = VINF_SUCCESS;
    switch (iReg)
    {
        case 0x02:
            APIC_LOCK(pDev, rcBusy);
            pApic->id = (u64Value >> 24); /** @todo r=bird: Is the range supposed to be 40 bits??? */
            APIC_UNLOCK(pDev);
            break;

        case 0x03:
            /* read only, ignore write. */
            break;

        case 0x08:
            APIC_LOCK(pDev, rcBusy);
            apic_update_tpr(pDev, pApic, u64Value);
            APIC_UNLOCK(pDev);
            break;

        case 0x09: case 0x0a:
            Log(("apicWriteRegister: write to read-only register %d ignored\n", iReg));
            break;

        case 0x0b: /* EOI */
            APIC_LOCK(pDev, rcBusy);
            apic_eoi(pDev, pApic);
            APIC_UNLOCK(pDev);
            break;

        case 0x0d:
            APIC_LOCK(pDev, rcBusy);
            pApic->log_dest = (u64Value >> 24) & 0xff;
            APIC_UNLOCK(pDev);
            break;

        case 0x0e:
            APIC_LOCK(pDev, rcBusy);
            pApic->dest_mode = u64Value >> 28; /** @todo r=bird: range?  This used to be 32-bit before morphed into an MSR handler. */
            APIC_UNLOCK(pDev);
            break;

        case 0x0f:
            APIC_LOCK(pDev, rcBusy);
            pApic->spurious_vec = u64Value & 0x1ff;
            apic_update_irq(pDev, pApic);
            APIC_UNLOCK(pDev);
            break;

        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
        case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
        case 0x28:
            Log(("apicWriteRegister: write to read-only register %d ignored\n", iReg));
            break;

        case 0x30:
            APIC_LOCK(pDev, rcBusy);
            pApic->icr[0] = (uint32_t)u64Value;
            if (fMsr) /* Here one of the differences with regular APIC: ICR is single 64-bit register */
                pApic->icr[1] = (uint32_t)(u64Value >> 32);
            rc = apic_deliver(pDev, pApic, (pApic->icr[1] >> 24) & 0xff, (pApic->icr[0] >> 11) & 1,
                              (pApic->icr[0] >>  8) & 7, (pApic->icr[0] & 0xff),
                              (pApic->icr[0] >> 14) & 1, (pApic->icr[0] >> 15) & 1);
            APIC_UNLOCK(pDev);
            break;

        case 0x31:
            APIC_LOCK(pDev, rcBusy);
            if (!fMsr)
                pApic->icr[1] = (uint64_t)u64Value;
            else
                pApic->esr |= ESR_ILLEGAL_ADDRESS;
            APIC_UNLOCK(pDev);
            break;

        case 0x32 + APIC_LVT_TIMER:
            AssertCompile(APIC_LVT_TIMER == 0);
            APIC_AND_TM_LOCK(pDev, pApic, rcBusy);
            apicTimerSetLvt(pDev, pApic, u64Value);
            APIC_AND_TM_UNLOCK(pDev, pApic);
            break;

        case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
            APIC_LOCK(pDev, rcBusy);
            pApic->lvt[iReg - 0x32] = u64Value;
            APIC_UNLOCK(pDev);
            break;

        case 0x38:
            APIC_AND_TM_LOCK(pDev, pApic, rcBusy);
            apicTimerSetInitialCount(pDev, pApic, u64Value);
            APIC_AND_TM_UNLOCK(pDev, pApic);
            break;

        case 0x39:
            Log(("apicWriteRegister: write to read-only register %d ignored\n", iReg));
            break;

        case 0x3e:
        {
            APIC_LOCK(pDev, rcBusy);
            pApic->divide_conf = u64Value & 0xb;
            int v = (pApic->divide_conf & 3) | ((pApic->divide_conf >> 1) & 4);
            pApic->count_shift = (v + 1) & 7;
            APIC_UNLOCK(pDev);
            break;
        }

        case 0x3f:
            if (fMsr)
            {
                /* Self IPI, see x2APIC book 2.4.5 */
                APIC_LOCK(pDev, rcBusy);
                int vector = u64Value & 0xff;
                rc = apic_bus_deliver(pDev,
                                      1 << pApic->id /* Self */,
                                      0 /* Delivery mode - fixed */,
                                      vector,
                                      0 /* Polarity - conform to the bus */,
                                      0 /* Trigger mode - edge */);
                APIC_UNLOCK(pDev);
                break;
            }
            /* else: fall thru */
        default:
            AssertMsgFailed(("unknown iReg %x\n", iReg));
            pApic->esr |= ESR_ILLEGAL_ADDRESS;
            break;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMAPICREG,pfnWriteMSRR3}
 */
PDMBOTHCBDECL(int) apicWriteMSR(PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t u64Value)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    if (pDev->enmVersion < PDMAPICVERSION_X2APIC)
        return VERR_EM_INTERPRETER; /** @todo tell the caller to raise hell (\#GP(0)).  */

    APICState      *pApic = getLapicById(pDev, idCpu);
    uint32_t        iReg = (u32Reg - MSR_IA32_APIC_START) & 0xff;
    return apicWriteRegister(pDev, pApic, iReg, u64Value, VINF_SUCCESS /*rcBusy*/, true /*fMsr*/);
}


/**
 * @interface_method_impl{PDMAPICREG,pfnReadMSRR3}
 */
PDMBOTHCBDECL(int) apicReadMSR(PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));

    if (pDev->enmVersion < PDMAPICVERSION_X2APIC)
        return VERR_EM_INTERPRETER;

    uint32_t    index = (u32Reg - MSR_IA32_APIC_START) & 0xff;
    APICState  *apic = getLapicById(pDev, idCpu);
    uint64_t    val = 0;
    int         rc = VINF_SUCCESS;

    switch (index)
    {
        case 0x02: /* id */
            val = apic->id << 24;
            break;
        case 0x03: /* version */
            val = APIC_HW_VERSION
                | ((APIC_LVT_NB - 1) << 16) /* Max LVT index */
                | (0 << 24) /* Support for EOI broadcast suppression */;
            break;
        case 0x08:
            val = apic->tpr;
            break;
        case 0x09:
            val = apic_get_arb_pri(apic);
            break;
        case 0x0a:
            /* ppr */
            val = apic_get_ppr(apic);
            break;
        case 0x0b:
            val = 0;
            break;
        case 0x0d:
            val = (uint64_t)apic->log_dest << 24;
            break;
        case 0x0e:
            /* Bottom 28 bits are always 1 */
            val = ((uint64_t)apic->dest_mode << 28) | 0xfffffff;
            break;
        case 0x0f:
            val = apic->spurious_vec;
            break;
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
            val = apic->isr[index & 7];
            break;
        case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
            val = apic->tmr[index & 7];
            break;
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
            val = apic->irr[index & 7];
            break;
        case 0x28:
            val = apic->esr;
            break;
        case 0x30:
            /* Here one of the differences with regular APIC: ICR is single 64-bit register */
            val = ((uint64_t)apic->icr[1] << 32) | apic->icr[0];
            break;
        case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
            val = apic->lvt[index - 0x32];
            break;
        case 0x38:
            val = apic->initial_count;
            break;
        case 0x39:
            val = apic_get_current_count(pDev, apic);
            break;
        case 0x3e:
            val = apic->divide_conf;
            break;
        case 0x3f:
            /* Self IPI register is write only */
            Log(("apicReadMSR: read from write-only register %d ignored\n", index));
            break;
        case 0x2f:
            /**
             * Correctable machine check exception vector, @todo: implement me!
             */
        default:
            AssertMsgFailed(("apicReadMSR: unknown index %x\n", index));
            /**
             * @todo: according to spec when APIC writes to ESR it msut raise error interrupt,
             *        i.e. LVT[5]
             */
            apic->esr |= ESR_ILLEGAL_ADDRESS;
            val = 0;
            break;
    }
    *pu64Value = val;
    return rc;
}

/**
 * More or less private interface between IOAPIC, only PDM is responsible
 * for connecting the two devices.
 */
PDMBOTHCBDECL(int) apicBusDeliverCallback(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode,
                                           uint8_t u8DeliveryMode, uint8_t iVector, uint8_t u8Polarity,
                                           uint8_t u8TriggerMode)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));
    LogFlow(("apicBusDeliverCallback: pDevIns=%p u8Dest=%#x u8DestMode=%#x u8DeliveryMode=%#x iVector=%#x u8Polarity=%#x u8TriggerMode=%#x\n",
             pDevIns, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    return apic_bus_deliver(pDev, apic_get_delivery_bitmask(pDev, u8Dest, u8DestMode),
                            u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}

/**
 * Local interrupt delivery, for devices attached to the CPU's LINT0/LINT1 pin.
 * Normally used for 8259A PIC and NMI.
 */
PDMBOTHCBDECL(int) apicLocalInterrupt(PPDMDEVINS pDevIns, uint8_t u8Pin, uint8_t u8Level)
{
    APICDeviceInfo  *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState       *s = getLapicById(pDev, 0);

    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));
    LogFlow(("apicLocalInterrupt: pDevIns=%p u8Pin=%x u8Level=%x\n", pDevIns, u8Pin, u8Level));

    /* If LAPIC is disabled, go straight to the CPU. */
    if (!(s->spurious_vec & APIC_SV_ENABLE))
    {
        LogFlow(("apicLocalInterrupt: LAPIC disabled, delivering directly to CPU core.\n"));
        if (u8Level)
            cpuSetInterrupt(pDev, s, PDMAPICIRQ_EXTINT);
        else
            cpuClearInterrupt(pDev, s, PDMAPICIRQ_EXTINT);

        return VINF_SUCCESS;
    }

    /* If LAPIC is enabled, interrupts are subject to LVT programming. */

    /* There are only two local interrupt pins. */
    AssertMsgReturn(u8Pin <= 1, ("Invalid LAPIC pin %d\n", u8Pin), VERR_INVALID_PARAMETER);

    /* NB: We currently only deliver local interrupts to the first CPU. In theory they
     * should be delivered to all CPUs and it is the guest's responsibility to ensure
     * no more than one CPU has the interrupt unmasked.
     */
    uint32_t    u32Lvec;

    u32Lvec = s->lvt[APIC_LVT_LINT0 + u8Pin];   /* Fetch corresponding LVT entry. */
    /* Drop int if entry is masked. May not be correct for level-triggered interrupts. */
    if (!(u32Lvec & APIC_LVT_MASKED))
    {   uint8_t     u8Delivery;
        PDMAPICIRQ  enmType;

        u8Delivery = (u32Lvec >> 8) & 7;
        switch (u8Delivery)
        {
            case APIC_DM_EXTINT:
                Assert(u8Pin == 0); /* PIC should be wired to LINT0. */
                enmType = PDMAPICIRQ_EXTINT;
                /* ExtINT can be both set and cleared, NMI/SMI/INIT can only be set. */
                LogFlow(("apicLocalInterrupt: %s ExtINT interrupt\n", u8Level ? "setting" : "clearing"));
                if (u8Level)
                    cpuSetInterrupt(pDev, s, enmType);
                else
                    cpuClearInterrupt(pDev, s, enmType);
                return VINF_SUCCESS;
            case APIC_DM_NMI:
                /* External NMI should be wired to LINT1, but Linux sometimes programs
                 * LVT0 to NMI delivery mode as well.
                 */
                enmType = PDMAPICIRQ_NMI;
                /* Currently delivering NMIs through here causes problems with NMI watchdogs
                 * on certain Linux kernels, e.g. 64-bit CentOS 5.3. Disable NMIs for now.
                 */
                return VINF_SUCCESS;
            case APIC_DM_SMI:
                enmType = PDMAPICIRQ_SMI;
                break;
            case APIC_DM_FIXED:
            {
                /** @todo implement APIC_DM_FIXED! */
                static unsigned s_c = 0;
                if (s_c++ < 5)
                    LogRel(("delivery type APIC_DM_FIXED not implemented. u8Pin=%d u8Level=%d\n", u8Pin, u8Level));
                return  VINF_SUCCESS;
            }
            case APIC_DM_INIT:
                /** @todo implement APIC_DM_INIT? */
            default:
            {
                static unsigned s_c = 0;
                if (s_c++ < 100)
                    AssertLogRelMsgFailed(("delivery type %d not implemented. u8Pin=%d u8Level=%d\n", u8Delivery, u8Pin, u8Level));
                return VERR_INTERNAL_ERROR_4;
            }
        }
        LogFlow(("apicLocalInterrupt: setting local interrupt type %d\n", enmType));
        cpuSetInterrupt(pDev, s, enmType);
    }
    return VINF_SUCCESS;
}

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

static int apic_get_ppr_zero_tpr(APICState *s)
{
    int isrv;

    isrv = get_highest_priority_int(s->isr);
    if (isrv < 0)
        isrv = 0;
    return isrv;
}

static int apic_get_arb_pri(APICState *s)
{
    /* XXX: arbitration */
    return 0;
}

/* signal the CPU if an irq is pending */
static bool apic_update_irq(APICDeviceInfo *pDev, APICState* s)
{
    int irrv, ppr;
    if (!(s->spurious_vec & APIC_SV_ENABLE))
    {
        /* Clear any pending APIC interrupt action flag. */
        cpuClearInterrupt(pDev, s);
        return false;
    }

    irrv = get_highest_priority_int(s->irr);
    if (irrv < 0)
        return false;
    ppr = apic_get_ppr(s);
    if (ppr && (irrv & 0xf0) <= (ppr & 0xf0))
        return false;
    cpuSetInterrupt(pDev, s);
    return true;
}

/* Check if the APIC has a pending interrupt/if a TPR change would active one. */
PDMBOTHCBDECL(bool) apicHasPendingIrq(PPDMDEVINS pDevIns)
{
    int irrv, ppr;
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    if (!pDev)
        return false;

    /* We don't perform any locking here as that would cause a lot of contention for VT-x/AMD-V. */

    APICState *s = getLapic(pDev); /** @todo fix interface */

    /*
     * All our callbacks now come from single IOAPIC, thus locking
     * seems to be excessive now (@todo: check)
     */
    irrv = get_highest_priority_int(s->irr);
    if (irrv < 0)
        return false;

    ppr = apic_get_ppr_zero_tpr(s);

    if (ppr && (irrv & 0xf0) <= (ppr & 0xf0))
        return false;

    return true;
}

static void apic_update_tpr(APICDeviceInfo *pDev, APICState* s, uint32_t val)
{
    bool fIrqIsActive = false;
    bool fIrqWasActive = false;

    fIrqWasActive = apic_update_irq(pDev, s);
    s->tpr        = val;
    fIrqIsActive  = apic_update_irq(pDev, s);

    /* If an interrupt is pending and now masked, then clear the FF flag. */
    if (fIrqWasActive && !fIrqIsActive)
    {
        Log(("apic_update_tpr: deactivate interrupt that was masked by the TPR update (%x)\n", val));
        STAM_COUNTER_INC(&pDev->StatClearedActiveIrq);
        cpuClearInterrupt(pDev, s);
    }
}

static void apic_set_irq(APICDeviceInfo *pDev,  APICState* s, int vector_num, int trigger_mode)
{
    LogFlow(("CPU%d: apic_set_irq vector=%x, trigger_mode=%x\n", s->phys_id, vector_num, trigger_mode));
    set_bit(s->irr, vector_num);
    if (trigger_mode)
        set_bit(s->tmr, vector_num);
    else
        reset_bit(s->tmr, vector_num);
    apic_update_irq(pDev, s);
}

static void apic_eoi(APICDeviceInfo *pDev, APICState* s)
{
    int isrv;
    isrv = get_highest_priority_int(s->isr);
    if (isrv < 0)
        return;
    reset_bit(s->isr, isrv);
    LogFlow(("CPU%d: apic_eoi isrv=%x\n", s->phys_id, isrv));
    /* XXX: send the EOI packet to the APIC bus to allow the I/O APIC to
            set the remote IRR bit for level triggered interrupts. */
    apic_update_irq(pDev, s);
}

static uint32_t apic_get_delivery_bitmask(APICDeviceInfo *pDev, uint8_t dest, uint8_t dest_mode)
{
    uint32_t mask = 0;

    if (dest_mode == 0)
    {
        if (dest == 0xff)
            mask = 0xff;
        else
            mask = 1 << dest;
    }
    else
    {
        APICState *apic = pDev->CTX_SUFF(paLapics);
        uint32_t i;

        /* XXX: cluster mode */
        for(i = 0; i < pDev->cCpus; i++)
        {
            if (apic->dest_mode == APIC_DESTMODE_FLAT)
            {
                if (dest & apic->log_dest)
                    mask |= (1 << i);
            }
            else if (apic->dest_mode == APIC_DESTMODE_CLUSTER)
            {
                if ((dest & 0xf0) == (apic->log_dest & 0xf0)
                    &&
                    (dest & apic->log_dest & 0x0f))
                {
                    mask |= (1 << i);
                }
            }
            apic++;
        }
    }

    return mask;
}

#ifdef IN_RING3
static void apic_init_ipi(APICDeviceInfo* pDev, APICState *s)
{
    int i;

    for(i = 0; i < APIC_LVT_NB; i++)
        s->lvt[i] = 1 << 16; /* mask LVT */
    s->tpr = 0;
    s->spurious_vec = 0xff;
    s->log_dest = 0;
    s->dest_mode = 0xff; /** @todo 0xff???? */
    memset(s->isr, 0, sizeof(s->isr));
    memset(s->tmr, 0, sizeof(s->tmr));
    memset(s->irr, 0, sizeof(s->irr));
    s->esr = 0;
    memset(s->icr, 0, sizeof(s->icr));
    s->divide_conf = 0;
    s->count_shift = 1;
    s->initial_count = 0;
    s->initial_count_load_time = 0;
    s->next_time = 0;
}


static void apicSendInitIpi(APICDeviceInfo* pDev, APICState *s)
{
    apic_init_ipi(pDev, s);
    cpuSendInitIpi(pDev, s);
}

/* send a SIPI message to the CPU to start it */
static void apic_startup(APICDeviceInfo* pDev, APICState *s, int vector_num)
{
    Log(("[SMP] apic_startup: %d on CPUs %d\n", vector_num, s->phys_id));
    cpuSendSipi(pDev, s, vector_num);
}
#endif /* IN_RING3 */

static int  apic_deliver(APICDeviceInfo* pDev, APICState *s,
                         uint8_t dest, uint8_t dest_mode,
                         uint8_t delivery_mode, uint8_t vector_num,
                         uint8_t polarity, uint8_t trigger_mode)
{
    uint32_t deliver_bitmask = 0;
    int dest_shorthand = (s->icr[0] >> 18) & 3;

    LogFlow(("apic_deliver dest=%x dest_mode=%x dest_shorthand=%x delivery_mode=%x vector_num=%x polarity=%x trigger_mode=%x\n", dest, dest_mode, dest_shorthand, delivery_mode, vector_num, polarity, trigger_mode));

    switch (dest_shorthand) {
        case 0:
            deliver_bitmask = apic_get_delivery_bitmask(pDev, dest, dest_mode);
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

    switch (delivery_mode) {
        case APIC_DM_INIT:
            {
                int trig_mode = (s->icr[0] >> 15) & 1;
                int level = (s->icr[0] >> 14) & 1;
                if (level == 0 && trig_mode == 1) {
                    foreach_apic(pDev, deliver_bitmask,
                                       apic->arb_id = apic->id);
                    Log(("CPU%d: APIC_DM_INIT arbitration id(s) set\n", s->phys_id));
                    return VINF_SUCCESS;
                }
            }
            break;

        case APIC_DM_SIPI:
# ifdef IN_RING3
            foreach_apic(pDev, deliver_bitmask,
                         apic_startup(pDev, apic, vector_num));
            return VINF_SUCCESS;
# else
            /* We shall send SIPI only in R3, R0 calls should be
               rescheduled to R3 */
            return  VINF_IOM_HC_MMIO_WRITE;
# endif
    }

    return apic_bus_deliver(pDev, deliver_bitmask, delivery_mode, vector_num,
                            polarity, trigger_mode);
}


PDMBOTHCBDECL(int) apicGetInterrupt(PPDMDEVINS pDevIns)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    /* if the APIC is not installed or enabled, we let the 8259 handle the
       IRQs */
    if (!pDev)
    {
        Log(("apic_get_interrupt: returns -1 (!s)\n"));
        return -1;
    }

    Assert(PDMCritSectIsOwner(pDev->CTX_SUFF(pCritSect)));

    APICState *s = getLapic(pDev);  /** @todo fix interface */
    int intno;

    if (!(s->spurious_vec & APIC_SV_ENABLE)) {
        Log(("CPU%d: apic_get_interrupt: returns -1 (APIC_SV_ENABLE)\n", s->phys_id));
        return -1;
    }

    /* XXX: spurious IRQ handling */
    intno = get_highest_priority_int(s->irr);
    if (intno < 0) {
        Log(("CPU%d: apic_get_interrupt: returns -1 (irr)\n", s->phys_id));
        return -1;
    }
    if (s->tpr && (uint32_t)intno <= s->tpr) {
        Log(("apic_get_interrupt: returns %d (sp)\n", s->spurious_vec & 0xff));
        return s->spurious_vec & 0xff;
    }
    reset_bit(s->irr, intno);
    set_bit(s->isr, intno);
    apic_update_irq(pDev, s);
    LogFlow(("CPU%d: apic_get_interrupt: returns %d\n", s->phys_id, intno));
    return intno;
}

/**
 * May return to ring-3 to acquire the TM and PDM lock.
 */
static uint32_t apic_get_current_count(APICDeviceInfo *pDev, APICState *s)
{
    int64_t d;
    uint32_t val;

    /* Acquire the timer lock w/ lock order kludge. */
    PDMCritSectLeave(pDev->CTX_SUFF(pCritSect));
    TMTimerLock(s->CTX_SUFF(pTimer), VINF_SUCCESS);
    PDMCritSectEnter(pDev->CTX_SUFF(pCritSect), VINF_SUCCESS);

    d = (TMTimerGet(s->CTX_SUFF(pTimer)) - s->initial_count_load_time) >>
        s->count_shift;

    if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
        /* periodic */
        val = s->initial_count - (d % ((uint64_t)s->initial_count + 1));
    } else {
        if (d >= s->initial_count)
            val = 0;
        else
            val = s->initial_count - d;
    }

    TMTimerUnlock(s->CTX_SUFF(pTimer));

    return val;
}

/**
 * Does the frequency hinting and logging.
 *
 * @param   pApic               The device state.
 */
DECLINLINE(void) apicDoFrequencyHinting(APICState *pApic)
{
    if (   pApic->uHintedInitialCount != pApic->initial_count
        || pApic->uHintedCountShift   != (uint32_t)pApic->count_shift)
    {
        pApic->uHintedInitialCount  = pApic->initial_count;
        pApic->uHintedCountShift    = pApic->count_shift;

        uint32_t uHz;
        if (pApic->initial_count > 0)
        {
            Assert((unsigned)pApic->count_shift < 30);
            uint64_t cTickPerPeriod = ((uint64_t)pApic->initial_count + 1) << pApic->count_shift;
            uHz = TMTimerGetFreq(pApic->CTX_SUFF(pTimer)) / cTickPerPeriod;
        }
        else
            uHz = 0;
        TMTimerSetFrequencyHint(pApic->CTX_SUFF(pTimer), uHz);
        Log(("apic: %u Hz\n", uHz));
    }
}

/**
 * Implementation of the 0380h access: Timer reset + new initial count.
 *
 * @param   pDev                 The device state.
 * @param   pApic               The APIC sub-device state.
 * @param   u32NewInitialCount  The new initial count for the timer.
 */
static void apicTimerSetInitialCount(APICDeviceInfo *pDev, APICState *pApic, uint32_t u32NewInitialCount)
{
    STAM_COUNTER_INC(&pApic->StatTimerSetInitialCount);
    pApic->initial_count = u32NewInitialCount;

    /*
     * Don't (re-)arm the timer if the it's masked or if it's
     * a zero length one-shot timer.
     */
    if (    !(pApic->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED)
        &&  u32NewInitialCount > 0)
    {
        /*
         * Calculate the relative next time and perform a combined timer get/set
         * operation. This avoids racing the clock between get and set.
         */
        uint64_t cTicksNext = u32NewInitialCount;
        cTicksNext         += 1;
        cTicksNext        <<= pApic->count_shift;
        TMTimerSetRelative(pApic->CTX_SUFF(pTimer), cTicksNext, &pApic->initial_count_load_time);
        pApic->next_time = pApic->initial_count_load_time + cTicksNext;
        pApic->fTimerArmed = true;
        apicDoFrequencyHinting(pApic);
        STAM_COUNTER_INC(&pApic->StatTimerSetInitialCountArm);
        Log(("apicTimerSetInitialCount: cTicksNext=%'llu (%#llx) ic=%#x sh=%#x nxt=%#llx\n",
             cTicksNext, cTicksNext, u32NewInitialCount, pApic->count_shift, pApic->next_time));
    }
    else
    {
        /* Stop it if necessary and record the load time for unmasking. */
        if (pApic->fTimerArmed)
        {
            STAM_COUNTER_INC(&pApic->StatTimerSetInitialCountDisarm);
            TMTimerStop(pApic->CTX_SUFF(pTimer));
            pApic->fTimerArmed = false;
            pApic->uHintedCountShift = pApic->uHintedInitialCount = 0;
        }
        pApic->initial_count_load_time = TMTimerGet(pApic->CTX_SUFF(pTimer));
        Log(("apicTimerSetInitialCount: ic=%#x sh=%#x iclt=%#llx\n", u32NewInitialCount, pApic->count_shift, pApic->initial_count_load_time));
    }
}

/**
 * Implementation of the 0320h access: change the LVT flags.
 *
 * @param   pDev             The device state.
 * @param   pApic           The APIC sub-device state to operate on.
 * @param   fNew            The new flags.
 */
static void apicTimerSetLvt(APICDeviceInfo *pDev, APICState *pApic, uint32_t fNew)
{
    STAM_COUNTER_INC(&pApic->StatTimerSetLvt);

    /*
     * Make the flag change, saving the old ones so we can avoid
     * unnecessary work.
     */
    uint32_t const fOld = pApic->lvt[APIC_LVT_TIMER];
    pApic->lvt[APIC_LVT_TIMER] = fNew;

    /* Only the masked and peridic bits are relevant (see apic_timer_update). */
    if (    (fOld & (APIC_LVT_MASKED | APIC_LVT_TIMER_PERIODIC))
        !=  (fNew & (APIC_LVT_MASKED | APIC_LVT_TIMER_PERIODIC)))
    {
        /*
         * If changed to one-shot from periodic, stop the timer if we're not
         * in the first period.
         */
        /** @todo check how clearing the periodic flag really should behave when not
         *        in period 1. The current code just mirrors the behavior of the
         *        original implementation. */
        if (    (fOld & APIC_LVT_TIMER_PERIODIC)
            && !(fNew & APIC_LVT_TIMER_PERIODIC))
        {
            STAM_COUNTER_INC(&pApic->StatTimerSetLvtClearPeriodic);
            uint64_t cTicks = (pApic->next_time - pApic->initial_count_load_time) >> pApic->count_shift;
            if (cTicks >= pApic->initial_count)
            {
                /* not first period, stop it. */
                TMTimerStop(pApic->CTX_SUFF(pTimer));
                pApic->fTimerArmed = false;
                pApic->uHintedCountShift = pApic->uHintedInitialCount = 0;
            }
            /* else: first period, let it fire normally. */
        }

        /*
         * We postpone stopping the timer when it's masked, this way we can
         * avoid some timer work when the guest temporarily masks the timer.
         * (apicTimerCallback will stop it if still masked.)
         */
        if (fNew & APIC_LVT_MASKED)
            STAM_COUNTER_INC(&pApic->StatTimerSetLvtPostponed);
        else if (pApic->fTimerArmed)
            STAM_COUNTER_INC(&pApic->StatTimerSetLvtArmed);
        /*
         * If unmasked, not armed and with a valid initial count value (according
         * to our interpretation of the spec), we will have to rearm the timer so
         * it will fire at the end of the current period.
         *
         * N.B. This is code is currently RACING the virtual sync clock!
         */
        else if (   (fOld & APIC_LVT_MASKED)
                 && pApic->initial_count > 0)
        {
            STAM_COUNTER_INC(&pApic->StatTimerSetLvtArm);
            for (unsigned cTries = 0; ; cTries++)
            {
                uint64_t NextTS;
                uint64_t cTicks = (TMTimerGet(pApic->CTX_SUFF(pTimer)) - pApic->initial_count_load_time) >> pApic->count_shift;
                if (fNew & APIC_LVT_TIMER_PERIODIC)
                    NextTS = ((cTicks / ((uint64_t)pApic->initial_count + 1)) + 1) * ((uint64_t)pApic->initial_count + 1);
                else
                {
                    if (cTicks >= pApic->initial_count)
                        break;
                    NextTS = (uint64_t)pApic->initial_count + 1;
                }
                NextTS <<= pApic->count_shift;
                NextTS += pApic->initial_count_load_time;

                /* Try avoid the assertion in TM.cpp... this isn't perfect! */
                if (    NextTS > TMTimerGet(pApic->CTX_SUFF(pTimer))
                    ||  cTries > 10)
                {
                    TMTimerSet(pApic->CTX_SUFF(pTimer), NextTS);
                    pApic->next_time = NextTS;
                    pApic->fTimerArmed = true;
                    apicDoFrequencyHinting(pApic);
                    Log(("apicTimerSetLvt: ic=%#x sh=%#x nxt=%#llx\n", pApic->initial_count, pApic->count_shift, pApic->next_time));
                    break;
                }
                STAM_COUNTER_INC(&pApic->StatTimerSetLvtArmRetries);
            }
        }
    }
    else
        STAM_COUNTER_INC(&pApic->StatTimerSetLvtNoRelevantChange);
}

# ifdef IN_RING3
/**
 * Timer callback function.
 *
 * @param  pDevIns      The device state.
 * @param  pTimer       The timer handle.
 * @param  pvUser       User argument pointing to the APIC instance.
 */
static DECLCALLBACK(void) apicTimerCallback(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    APICDeviceInfo *pDev   = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState      *pApic = (APICState *)pvUser;
    Assert(pApic->pTimerR3 == pTimer);
    Assert(pApic->fTimerArmed);
    Assert(PDMCritSectIsOwner(pDev->pCritSectR3));
    Assert(TMTimerIsLockOwner(pTimer));

    if (!(pApic->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED)) {
        LogFlow(("apic_timer: trigger irq\n"));
        apic_set_irq(pDev, pApic, pApic->lvt[APIC_LVT_TIMER] & 0xff, APIC_TRIGGER_EDGE);

        if (   (pApic->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC)
            && pApic->initial_count > 0) {
            /* new interval. */
            pApic->next_time += (((uint64_t)pApic->initial_count + 1) << pApic->count_shift);
            TMTimerSet(pApic->CTX_SUFF(pTimer), pApic->next_time);
            pApic->fTimerArmed = true;
            apicDoFrequencyHinting(pApic);
            Log2(("apicTimerCallback: ic=%#x sh=%#x nxt=%#llx\n", pApic->initial_count, pApic->count_shift, pApic->next_time));
        } else {
            /* single shot or disabled. */
            pApic->fTimerArmed = false;
            pApic->uHintedCountShift = pApic->uHintedInitialCount = 0;
        }
    } else {
        /* masked, do not rearm. */
        pApic->fTimerArmed = false;
        pApic->uHintedCountShift = pApic->uHintedInitialCount = 0;
    }
}
# endif /* IN_RING3 */

static uint32_t apic_mem_readl(APICDeviceInfo* pDev, APICState *s, RTGCPHYS addr)
{
    uint32_t val;
    int index;

    index = (addr >> 4) & 0xff;

    switch(index) {
    case 0x02: /* id */
        val = s->id << 24;
        break;
    case 0x03: /* version */
        val = APIC_HW_VERSION | ((APIC_LVT_NB - 1) << 16);
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
    case 0x0b:
        Log(("apic_mem_readl %x %x -> write only returning 0\n", addr, index));
        val = 0;
        break;
    case 0x0d:
        val = s->log_dest << 24;
        break;
    case 0x0e:
        /* Bottom 28 bits are always 1 */
        val = (s->dest_mode << 28) | 0xfffffff;
        break;
    case 0x0f:
        val = s->spurious_vec;
        break;
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
        val = s->isr[index & 7];
        break;
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
        val = s->tmr[index & 7];
        break;
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
        val = s->irr[index & 7];
        break;
    case 0x28:
        val = s->esr;
        break;
    case 0x30:
    case 0x31:
        val = s->icr[index & 1];
        break;
    case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
        val = s->lvt[index - 0x32];
        break;
    case 0x38:
        val = s->initial_count;
        break;
    case 0x39:
        val = apic_get_current_count(pDev, s);
        break;
    case 0x3e:
        val = s->divide_conf;
        break;
    case 0x2f:
        /**
         * Correctable machine check exception vector, @todo: implement me!
         */
    default:
        AssertMsgFailed(("apic_mem_readl: unknown index %x\n", index));
        s->esr |= ESR_ILLEGAL_ADDRESS;
        val = 0;
        break;
    }
#ifdef DEBUG_APIC
    Log(("CPU%d: APIC read: %08x = %08x\n", s->phys_id, (uint32_t)addr, val));
#endif
    return val;
}

#ifdef IN_RING3

static void apic_save(SSMHANDLE* f, void *opaque)
{
    APICState *s = (APICState*)opaque;
    int i;

    SSMR3PutU32(f, s->apicbase);
    SSMR3PutU32(f, s->id);
    SSMR3PutU32(f, s->phys_id);
    SSMR3PutU32(f, s->arb_id);
    SSMR3PutU32(f, s->tpr);
    SSMR3PutU32(f, s->spurious_vec);
    SSMR3PutU8(f,  s->log_dest);
    SSMR3PutU8(f,  s->dest_mode);
    for (i = 0; i < 8; i++) {
        SSMR3PutU32(f, s->isr[i]);
        SSMR3PutU32(f, s->tmr[i]);
        SSMR3PutU32(f, s->irr[i]);
    }
    for (i = 0; i < APIC_LVT_NB; i++) {
        SSMR3PutU32(f, s->lvt[i]);
    }
    SSMR3PutU32(f, s->esr);
    SSMR3PutU32(f, s->icr[0]);
    SSMR3PutU32(f, s->icr[1]);
    SSMR3PutU32(f, s->divide_conf);
    SSMR3PutU32(f, s->count_shift);
    SSMR3PutU32(f, s->initial_count);
    SSMR3PutU64(f, s->initial_count_load_time);
    SSMR3PutU64(f, s->next_time);

    TMR3TimerSave(s->CTX_SUFF(pTimer), f);
}

static int apic_load(SSMHANDLE *f, void *opaque, int version_id)
{
    APICState *s = (APICState*)opaque;
    int i;

     /* XXX: what if the base changes? (registered memory regions) */
    SSMR3GetU32(f, &s->apicbase);

    switch (version_id)
    {
        case APIC_SAVED_STATE_VERSION_ANCIENT:
        {
            uint8_t val = 0;
            SSMR3GetU8(f, &val);
            s->id = val;
            /* UP only in old saved states */
            s->phys_id = 0;
            SSMR3GetU8(f, &val);
            s->arb_id = val;
            break;
        }
        case APIC_SAVED_STATE_VERSION:
        case APIC_SAVED_STATE_VERSION_VBOX_30:
            SSMR3GetU32(f, &s->id);
            SSMR3GetU32(f, &s->phys_id);
            SSMR3GetU32(f, &s->arb_id);
            break;
        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    SSMR3GetU32(f, &s->tpr);
    SSMR3GetU32(f, &s->spurious_vec);
    SSMR3GetU8(f, &s->log_dest);
    SSMR3GetU8(f, &s->dest_mode);
    for (i = 0; i < 8; i++) {
        SSMR3GetU32(f, &s->isr[i]);
        SSMR3GetU32(f, &s->tmr[i]);
        SSMR3GetU32(f, &s->irr[i]);
    }
    for (i = 0; i < APIC_LVT_NB; i++) {
        SSMR3GetU32(f, &s->lvt[i]);
    }
    SSMR3GetU32(f, &s->esr);
    SSMR3GetU32(f, &s->icr[0]);
    SSMR3GetU32(f, &s->icr[1]);
    SSMR3GetU32(f, &s->divide_conf);
    SSMR3GetU32(f, (uint32_t *)&s->count_shift);
    SSMR3GetU32(f, (uint32_t *)&s->initial_count);
    SSMR3GetU64(f, (uint64_t *)&s->initial_count_load_time);
    SSMR3GetU64(f, (uint64_t *)&s->next_time);

    int rc = TMR3TimerLoad(s->CTX_SUFF(pTimer), f);
    s->uHintedCountShift = s->uHintedInitialCount = 0;
    s->fTimerArmed = TMTimerIsActive(s->CTX_SUFF(pTimer));
    if (s->fTimerArmed)
        apicDoFrequencyHinting(s);

    return VINF_SUCCESS; /** @todo darn mess! */
}

#endif /* IN_RING3 */

/* LAPIC */
PDMBOTHCBDECL(int) apicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState *s = getLapic(pDev);

    Log(("CPU%d: apicMMIORead at %llx\n", s->phys_id,  (uint64_t)GCPhysAddr));

    /** @todo: add LAPIC range validity checks (different LAPICs can theoretically have
               different physical addresses, see #3092) */

    STAM_COUNTER_INC(&CTXSUFF(pDev->StatMMIORead));
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
                &&  ++s->cTPRPatchAttempts < APIC_MAX_PATCH_ATTEMPTS)
            {
#ifdef IN_RC
                pDevIns->pDevHlpGC->pfnPATMSetMMIOPatchInfo(pDevIns, GCPhysAddr, &s->tpr);
#else
                RTGCPTR pDevInsGC = PDMINS2DATA_GCPTR(pDevIns);
                pDevIns->pHlpR0->pfnPATMSetMMIOPatchInfo(pDevIns, GCPhysAddr, pDevIns + RT_OFFSETOF(APICState, tpr));
#endif
                return VINF_PATM_HC_MMIO_PATCH_READ;
            }
#endif
#endif /* experimental */
            APIC_LOCK(pDev, VINF_IOM_HC_MMIO_READ);
            *(uint32_t *)pv = apic_mem_readl(pDev, s, GCPhysAddr);
            APIC_UNLOCK(pDev);
            break;
        }
        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) apicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState *s = getLapic(pDev);

    Log(("CPU%d: apicMMIOWrite at %llx\n", s->phys_id, (uint64_t)GCPhysAddr));

    /** @todo: add LAPIC range validity checks (multiple LAPICs can theoretically have
               different physical addresses, see #3092) */

    STAM_COUNTER_INC(&CTXSUFF(pDev->StatMMIOWrite));
    switch (cb)
    {
        case 1:
        case 2:
            /* ignore */
            break;

        case 4:
            /* It does its own locking. */
            return apicWriteRegister(pDev, s, (GCPhysAddr >> 4) & 0xff, *(uint32_t const *)pv,
                                     VINF_IOM_HC_MMIO_WRITE, false /*fMsr*/);

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/* Print a 8-dword LAPIC bit map (256 bits). */
static void lapicDumpVec(APICDeviceInfo  *pDev, APICState *lapic, PCDBGFINFOHLP pHlp, unsigned start)
{
    unsigned    i;
    uint32_t    val;

    for (i = 0; i < 8; ++i)
    {
        val = apic_mem_readl(pDev, lapic, start + (i << 4));
        pHlp->pfnPrintf(pHlp, "%08X", val);
    }
    pHlp->pfnPrintf(pHlp, "\n");
}

/* Print basic LAPIC state. */
static DECLCALLBACK(void) lapicInfoBasic(APICDeviceInfo  *pDev, APICState *lapic, PCDBGFINFOHLP pHlp)
{
    uint32_t        val;
    unsigned        max_lvt;

    pHlp->pfnPrintf(pHlp, "Local APIC at %08X:\n", lapic->apicbase);
    val = apic_mem_readl(pDev, lapic, 0x20);
    pHlp->pfnPrintf(pHlp, "  LAPIC ID  : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    APIC ID = %02X\n", (val >> 24) & 0xff);
    val = apic_mem_readl(pDev, lapic, 0x30);
    max_lvt = (val >> 16) & 0xff;
    pHlp->pfnPrintf(pHlp, "  APIC VER   : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    version  = %02X\n", val & 0xff);
    pHlp->pfnPrintf(pHlp, "    lvts     = %d\n", ((val >> 16) & 0xff) + 1);
    val = apic_mem_readl(pDev, lapic, 0x80);
    pHlp->pfnPrintf(pHlp, "  TPR        : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    task pri = %d/%d\n", (val >> 4) & 0xf, val & 0xf);
    val = apic_mem_readl(pDev, lapic, 0xA0);
    pHlp->pfnPrintf(pHlp, "  PPR        : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    cpu pri  = %d/%d\n", (val >> 4) & 0xf, val & 0xf);
    val = apic_mem_readl(pDev, lapic, 0xD0);
    pHlp->pfnPrintf(pHlp, "  LDR       : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    log id  = %02X\n", (val >> 24) & 0xff);
    val = apic_mem_readl(pDev, lapic, 0xE0);
    pHlp->pfnPrintf(pHlp, "  DFR       : %08X\n", val);
    val = apic_mem_readl(pDev, lapic, 0xF0);
    pHlp->pfnPrintf(pHlp, "  SVR       : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    focus   = %s\n", val & (1 << 9) ? "check off" : "check on");
    pHlp->pfnPrintf(pHlp, "    lapic   = %s\n", val & (1 << 8) ? "ENABLED" : "DISABLED");
    pHlp->pfnPrintf(pHlp, "    vector  = %02X\n", val & 0xff);
    pHlp->pfnPrintf(pHlp, "  ISR       : ");
    lapicDumpVec(pDev, lapic, pHlp, 0x100);
    val = get_highest_priority_int(lapic->isr);
    pHlp->pfnPrintf(pHlp, "    highest = %02X\n", val == ~0U ? 0 : val);
    pHlp->pfnPrintf(pHlp, "  IRR       : ");
    lapicDumpVec(pDev, lapic, pHlp, 0x200);
    val = get_highest_priority_int(lapic->irr);
    pHlp->pfnPrintf(pHlp, "    highest = %02X\n", val == ~0U ? 0 : val);
    val = apic_mem_readl(pDev, lapic, 0x320);
}

/* Print the more interesting LAPIC LVT entries. */
static DECLCALLBACK(void) lapicInfoLVT(APICDeviceInfo  *pDev, APICState *lapic, PCDBGFINFOHLP pHlp)
{
    uint32_t        val;
    static const char *dmodes[] = { "Fixed ", "Reserved", "SMI", "Reserved",
                                    "NMI", "INIT", "Reserved", "ExtINT" };

    val = apic_mem_readl(pDev, lapic, 0x320);
    pHlp->pfnPrintf(pHlp, "  LVT Timer : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    mode    = %s\n", val & (1 << 17) ? "periodic" : "one-shot");
    pHlp->pfnPrintf(pHlp, "    mask    = %d\n", (val >> 16) & 1);
    pHlp->pfnPrintf(pHlp, "    status  = %s\n", val & (1 << 12) ? "pending" : "idle");
    pHlp->pfnPrintf(pHlp, "    vector  = %02X\n", val & 0xff);
    val = apic_mem_readl(pDev, lapic, 0x350);
    pHlp->pfnPrintf(pHlp, "  LVT LINT0 : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    mask    = %d\n", (val >> 16) & 1);
    pHlp->pfnPrintf(pHlp, "    trigger = %s\n", val & (1 << 15) ? "level" : "edge");
    pHlp->pfnPrintf(pHlp, "    rem irr = %d\n", (val >> 14) & 1);
    pHlp->pfnPrintf(pHlp, "    polarty = %d\n", (val >> 13) & 1);
    pHlp->pfnPrintf(pHlp, "    status  = %s\n", val & (1 << 12) ? "pending" : "idle");
    pHlp->pfnPrintf(pHlp, "    delivry = %s\n", dmodes[(val >> 8) & 7]);
    pHlp->pfnPrintf(pHlp, "    vector  = %02X\n", val & 0xff);
    val = apic_mem_readl(pDev, lapic, 0x360);
    pHlp->pfnPrintf(pHlp, "  LVT LINT1 : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    mask    = %d\n", (val >> 16) & 1);
    pHlp->pfnPrintf(pHlp, "    trigger = %s\n", val & (1 << 15) ? "level" : "edge");
    pHlp->pfnPrintf(pHlp, "    rem irr = %d\n", (val >> 14) & 1);
    pHlp->pfnPrintf(pHlp, "    polarty = %d\n", (val >> 13) & 1);
    pHlp->pfnPrintf(pHlp, "    status  = %s\n", val & (1 << 12) ? "pending" : "idle");
    pHlp->pfnPrintf(pHlp, "    delivry = %s\n", dmodes[(val >> 8) & 7]);
    pHlp->pfnPrintf(pHlp, "    vector  = %02X\n", val & 0xff);
}

/* Print LAPIC timer state. */
static DECLCALLBACK(void) lapicInfoTimer(APICDeviceInfo  *pDev, APICState *lapic, PCDBGFINFOHLP pHlp)
{
    uint32_t        val;
    unsigned        divider;

    pHlp->pfnPrintf(pHlp, "Local APIC timer:\n");
    val = apic_mem_readl(pDev, lapic, 0x380);
    pHlp->pfnPrintf(pHlp, "  Initial count : %08X\n", val);
    val = apic_mem_readl(pDev, lapic, 0x390);
    pHlp->pfnPrintf(pHlp, "  Current count : %08X\n", val);
    val = apic_mem_readl(pDev, lapic, 0x3E0);
    pHlp->pfnPrintf(pHlp, "  Divide config : %08X\n", val);
    divider = ((val >> 1) & 0x04) | (val & 0x03);
    pHlp->pfnPrintf(pHlp, "    divider     = %d\n", divider == 7 ? 1 : 2 << divider);
}

/**
 * Info handler, device version. Dumps Local APIC(s) state according to given argument.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional.
 */
static DECLCALLBACK(void) lapicInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    APICDeviceInfo  *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    APICState       *lapic;

    lapic = getLapic(pDev);

    if (pszArgs == NULL || !strcmp(pszArgs, "basic"))
    {
        lapicInfoBasic(pDev, lapic, pHlp);
    }
    else if (!strcmp(pszArgs, "lvt"))
    {
        lapicInfoLVT(pDev, lapic, pHlp);
    }
    else if (!strcmp(pszArgs, "timer"))
    {
        lapicInfoTimer(pDev, lapic, pHlp);
    }
    else
    {
        pHlp->pfnPrintf(pHlp, "Invalid argument. Recognized arguments are 'basic', 'lvt', 'timer'.\n");
    }
}

/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) apicLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);

    SSMR3PutU32( pSSM, pDev->cCpus);
    SSMR3PutBool(pSSM, pDev->fIoApic);
    SSMR3PutU32( pSSM, pDev->enmVersion);
    AssertCompile(PDMAPICVERSION_APIC == 2);

    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) apicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);

    /* config */
    apicLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    /* save all APICs data, @todo: is it correct? */
    foreach_apic(pDev, 0xffffffff, apic_save(pSSM, apic));

    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) apicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);

    if (    uVersion != APIC_SAVED_STATE_VERSION
        &&  uVersion != APIC_SAVED_STATE_VERSION_VBOX_30
        &&  uVersion != APIC_SAVED_STATE_VERSION_ANCIENT)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* config */
    if (uVersion > APIC_SAVED_STATE_VERSION_VBOX_30) {
        uint32_t cCpus;
        int rc = SSMR3GetU32(pSSM, &cCpus); AssertRCReturn(rc, rc);
        if (cCpus != pDev->cCpus)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - cCpus: saved=%#x config=%#x"), cCpus, pDev->cCpus);
        bool fIoApic;
        rc = SSMR3GetBool(pSSM, &fIoApic); AssertRCReturn(rc, rc);
        if (fIoApic != pDev->fIoApic)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - fIoApic: saved=%RTbool config=%RTbool"), fIoApic, pDev->fIoApic);
        uint32_t uApicVersion;
        rc = SSMR3GetU32(pSSM, &uApicVersion); AssertRCReturn(rc, rc);
        if (uApicVersion != (uint32_t)pDev->enmVersion)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch - uApicVersion: saved=%#x config=%#x"), uApicVersion, pDev->enmVersion);
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* load all APICs data */ /** @todo: is it correct? */
    APIC_LOCK(pDev, VERR_INTERNAL_ERROR_3);
    foreach_apic(pDev, 0xffffffff,
                 if (apic_load(pSSM, apic, uVersion)) {
                      AssertFailed();
                      APIC_UNLOCK(pDev);
                      return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
                 }
                 );
    APIC_UNLOCK(pDev);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) apicReset(PPDMDEVINS pDevIns)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    TMTimerLock(pDev->paLapicsR3[0].pTimerR3, VERR_IGNORED);
    APIC_LOCK_VOID(pDev, VERR_IGNORED);

    /* Reset all APICs. */
    for (VMCPUID i = 0; i < pDev->cCpus; i++) {
        APICState *pApic = &pDev->CTX_SUFF(paLapics)[i];
        TMTimerStop(pApic->CTX_SUFF(pTimer));

        /* Clear LAPIC state as if an INIT IPI was sent. */
        apic_init_ipi(pDev, pApic);
        /* The IDs are not touched by apic_init_ipi() and must be reset now. */
        pApic->arb_id = pApic->id = i;
        Assert(pApic->id == pApic->phys_id);    /* The two should match again. */
        /* Reset should re-enable the APIC, see comment in msi.h */
        pApic->apicbase = VBOX_MSI_ADDR_BASE | MSR_IA32_APICBASE_ENABLE;
        if (pApic->phys_id == 0)
            pApic->apicbase |= MSR_IA32_APICBASE_BSP;

        /* Clear any pending APIC interrupt action flag. */
        cpuClearInterrupt(pDev, pApic);
    }
    /** @todo r=bird: Why is this done everytime, while the constructor first
     *        checks the CPUID?  Who is right? */
    pDev->pApicHlpR3->pfnChangeFeature(pDev->pDevInsR3, pDev->enmVersion);

    APIC_UNLOCK(pDev);
    TMTimerUnlock(pDev->paLapicsR3[0].pTimerR3);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) apicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    pDev->pDevInsRC   = PDMDEVINS_2_RCPTR(pDevIns);
    pDev->pApicHlpRC  = pDev->pApicHlpR3->pfnGetRCHelpers(pDevIns);
    pDev->paLapicsRC  = MMHyperR3ToRC(PDMDevHlpGetVM(pDevIns), pDev->paLapicsR3);
    pDev->pCritSectRC = pDev->pApicHlpR3->pfnGetRCCritSect(pDevIns);
    for (uint32_t i = 0; i < pDev->cCpus; i++)
        pDev->paLapicsR3[i].pTimerRC = TMTimerRCPtr(pDev->paLapicsR3[i].pTimerR3);
}

DECLINLINE(void) initApicData(APICState* apic, uint8_t id)
{
    int i;
    memset(apic, 0, sizeof(*apic));

    /* See comment in msi.h for LAPIC base info */
    apic->apicbase = VBOX_MSI_ADDR_BASE | MSR_IA32_APICBASE_ENABLE;
    /* Mark first CPU as BSP */
    if (id == 0)
        apic->apicbase |= MSR_IA32_APICBASE_BSP;
    for (i = 0; i < APIC_LVT_NB; i++)
        apic->lvt[i] = 1 << 16; /* mask LVT */
    apic->spurious_vec = 0xff;
    apic->phys_id = apic->id = id;
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) apicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMAPICREG      ApicReg;
    int             rc;
    uint32_t        i;
    bool            fIoApic;
    bool            fGCEnabled;
    bool            fR0Enabled;
    APICDeviceInfo *pDev = PDMINS_2_DATA(pDevIns, APICDeviceInfo *);
    uint32_t        cCpus;

    /*
     * Only single device instance.
     */
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "IOAPIC\0"
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "NumCPUs\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBoolDef(pCfg, "IOAPIC", &fIoApic, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));

    rc = CFGMR3QueryU32Def(pCfg, "NumCPUs", &cCpus, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query integer value \"NumCPUs\""));

    Log(("APIC: cCpus=%d fR0Enabled=%RTbool fGCEnabled=%RTbool fIoApic=%RTbool\n", cCpus, fR0Enabled, fGCEnabled, fIoApic));

    /** @todo Current implementation is limited to 32 CPUs due to the use of 32
     *        bits bitmasks. */
    if (cCpus > 32)
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Invalid value for \"NumCPUs\""));

    /*
     * Init the data.
     */
    pDev->pDevInsR3  = pDevIns;
    pDev->pDevInsR0  = PDMDEVINS_2_R0PTR(pDevIns);
    pDev->pDevInsRC  = PDMDEVINS_2_RCPTR(pDevIns);
    pDev->cCpus      = cCpus;
    pDev->fIoApic    = fIoApic;
    /* Use PDMAPICVERSION_X2APIC to activate x2APIC mode */
    pDev->enmVersion = PDMAPICVERSION_APIC;

    /* Disable locking in this device. */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    PVM pVM = PDMDevHlpGetVM(pDevIns);

    /*
     * We are not freeing this memory, as it's automatically released when guest exits.
     */
    rc = MMHyperAlloc(pVM, cCpus * sizeof(APICState), 1, MM_TAG_PDM_DEVICE_USER, (void **)&pDev->paLapicsR3);
    if (RT_FAILURE(rc))
        return VERR_NO_MEMORY;
    pDev->paLapicsR0 = MMHyperR3ToR0(pVM, pDev->paLapicsR3);
    pDev->paLapicsRC = MMHyperR3ToRC(pVM, pDev->paLapicsR3);

    for (i = 0; i < cCpus; i++)
        initApicData(&pDev->paLapicsR3[i], i);

    /*
     * Register the APIC.
     */
    ApicReg.u32Version              = PDM_APICREG_VERSION;
    ApicReg.pfnGetInterruptR3       = apicGetInterrupt;
    ApicReg.pfnHasPendingIrqR3      = apicHasPendingIrq;
    ApicReg.pfnSetBaseR3            = apicSetBase;
    ApicReg.pfnGetBaseR3            = apicGetBase;
    ApicReg.pfnSetTPRR3             = apicSetTPR;
    ApicReg.pfnGetTPRR3             = apicGetTPR;
    ApicReg.pfnWriteMSRR3           = apicWriteMSR;
    ApicReg.pfnReadMSRR3            = apicReadMSR;
    ApicReg.pfnBusDeliverR3         = apicBusDeliverCallback;
    ApicReg.pfnLocalInterruptR3     = apicLocalInterrupt;
    if (fGCEnabled) {
        ApicReg.pszGetInterruptRC   = "apicGetInterrupt";
        ApicReg.pszHasPendingIrqRC  = "apicHasPendingIrq";
        ApicReg.pszSetBaseRC        = "apicSetBase";
        ApicReg.pszGetBaseRC        = "apicGetBase";
        ApicReg.pszSetTPRRC         = "apicSetTPR";
        ApicReg.pszGetTPRRC         = "apicGetTPR";
        ApicReg.pszWriteMSRRC       = "apicWriteMSR";
        ApicReg.pszReadMSRRC        = "apicReadMSR";
        ApicReg.pszBusDeliverRC     = "apicBusDeliverCallback";
        ApicReg.pszLocalInterruptRC = "apicLocalInterrupt";
    } else {
        ApicReg.pszGetInterruptRC   = NULL;
        ApicReg.pszHasPendingIrqRC  = NULL;
        ApicReg.pszSetBaseRC        = NULL;
        ApicReg.pszGetBaseRC        = NULL;
        ApicReg.pszSetTPRRC         = NULL;
        ApicReg.pszGetTPRRC         = NULL;
        ApicReg.pszWriteMSRRC       = NULL;
        ApicReg.pszReadMSRRC        = NULL;
        ApicReg.pszBusDeliverRC     = NULL;
        ApicReg.pszLocalInterruptRC = NULL;
    }
    if (fR0Enabled) {
        ApicReg.pszGetInterruptR0   = "apicGetInterrupt";
        ApicReg.pszHasPendingIrqR0  = "apicHasPendingIrq";
        ApicReg.pszSetBaseR0        = "apicSetBase";
        ApicReg.pszGetBaseR0        = "apicGetBase";
        ApicReg.pszSetTPRR0         = "apicSetTPR";
        ApicReg.pszGetTPRR0         = "apicGetTPR";
        ApicReg.pszWriteMSRR0       = "apicWriteMSR";
        ApicReg.pszReadMSRR0        = "apicReadMSR";
        ApicReg.pszBusDeliverR0     = "apicBusDeliverCallback";
        ApicReg.pszLocalInterruptR0 = "apicLocalInterrupt";
    } else {
        ApicReg.pszGetInterruptR0   = NULL;
        ApicReg.pszHasPendingIrqR0  = NULL;
        ApicReg.pszSetBaseR0        = NULL;
        ApicReg.pszGetBaseR0        = NULL;
        ApicReg.pszSetTPRR0         = NULL;
        ApicReg.pszGetTPRR0         = NULL;
        ApicReg.pszWriteMSRR0       = NULL;
        ApicReg.pszReadMSRR0        = NULL;
        ApicReg.pszBusDeliverR0     = NULL;
        ApicReg.pszLocalInterruptR0 = NULL;
    }

    rc = PDMDevHlpAPICRegister(pDevIns, &ApicReg, &pDev->pApicHlpR3);
    AssertLogRelRCReturn(rc, rc);
    pDev->pCritSectR3 = pDev->pApicHlpR3->pfnGetR3CritSect(pDevIns);

    /*
     * The the CPUID feature bit.
     */
    /** @todo r=bird: See remark in the apicReset. */
    uint32_t u32Eax, u32Ebx, u32Ecx, u32Edx;
    PDMDevHlpGetCpuId(pDevIns, 0, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
    if (u32Eax >= 1) {
        if (   fIoApic                       /* If IOAPIC is enabled, enable Local APIC in any case */
            || (   u32Ebx == X86_CPUID_VENDOR_INTEL_EBX
                && u32Ecx == X86_CPUID_VENDOR_INTEL_ECX
                && u32Edx == X86_CPUID_VENDOR_INTEL_EDX /* GenuineIntel */)
            || (   u32Ebx == X86_CPUID_VENDOR_AMD_EBX
                && u32Ecx == X86_CPUID_VENDOR_AMD_ECX
                && u32Edx == X86_CPUID_VENDOR_AMD_EDX   /* AuthenticAMD */)) {
            LogRel(("Activating Local APIC\n"));
            pDev->pApicHlpR3->pfnChangeFeature(pDevIns, pDev->enmVersion);
        }
    }

    /*
     * Register the MMIO range.
     * @todo: shall reregister, if base changes.
     */
    uint32_t ApicBase = pDev->paLapicsR3[0].apicbase & ~0xfff;
    rc = PDMDevHlpMMIORegister(pDevIns, ApicBase, 0x1000, pDev,
                               apicMMIOWrite, apicMMIORead, NULL, "APIC Memory");
    if (RT_FAILURE(rc))
        return rc;

    if (fGCEnabled) {
        pDev->pApicHlpRC  = pDev->pApicHlpR3->pfnGetRCHelpers(pDevIns);
        pDev->pCritSectRC = pDev->pApicHlpR3->pfnGetRCCritSect(pDevIns);

        rc = PDMDevHlpMMIORegisterRC(pDevIns, ApicBase, 0x1000, 0,
                                     "apicMMIOWrite", "apicMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    if (fR0Enabled) {
        pDev->pApicHlpR0  = pDev->pApicHlpR3->pfnGetR0Helpers(pDevIns);
        pDev->pCritSectR0 = pDev->pApicHlpR3->pfnGetR0CritSect(pDevIns);

        rc = PDMDevHlpMMIORegisterR0(pDevIns, ApicBase, 0x1000, 0,
                                     "apicMMIOWrite", "apicMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Create the APIC timers.
     */
    for (i = 0; i < cCpus; i++) {
        APICState *pApic = &pDev->paLapicsR3[i];
        pApic->pszDesc = MMR3HeapAPrintf(pVM, MM_TAG_PDM_DEVICE_USER, "APIC Timer #%u", i);
        rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, apicTimerCallback, pApic,
                                    TMTIMER_FLAGS_NO_CRIT_SECT, pApic->pszDesc, &pApic->pTimerR3);
        if (RT_FAILURE(rc))
            return rc;
        pApic->pTimerR0 = TMTimerR0Ptr(pApic->pTimerR3);
        pApic->pTimerRC = TMTimerRCPtr(pApic->pTimerR3);
        TMR3TimerSetCritSect(pApic->pTimerR3, pDev->pCritSectR3);
    }

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, APIC_SAVED_STATE_VERSION, sizeof(*pDev),
                               apicLiveExec, apicSaveExec, apicLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register debugger info callback.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "lapic", "Display Local APIC state for current CPU. "
                              "Recognizes 'basic', 'lvt', 'timer' as arguments, defaulting to 'basic'.", lapicInfo);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pDev->StatMMIOReadGC,     STAMTYPE_COUNTER,  "/Devices/APIC/MMIOReadGC",   STAMUNIT_OCCURENCES, "Number of APIC MMIO reads in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pDev->StatMMIOReadHC,     STAMTYPE_COUNTER,  "/Devices/APIC/MMIOReadHC",   STAMUNIT_OCCURENCES, "Number of APIC MMIO reads in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pDev->StatMMIOWriteGC,    STAMTYPE_COUNTER,  "/Devices/APIC/MMIOWriteGC",  STAMUNIT_OCCURENCES, "Number of APIC MMIO writes in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pDev->StatMMIOWriteHC,    STAMTYPE_COUNTER,  "/Devices/APIC/MMIOWriteHC",  STAMUNIT_OCCURENCES, "Number of APIC MMIO writes in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pDev->StatClearedActiveIrq,STAMTYPE_COUNTER, "/Devices/APIC/MaskedActiveIRQ", STAMUNIT_OCCURENCES, "Number of cleared irqs.");
    for (i = 0; i < cCpus; i++) {
        APICState *pApic = &pDev->paLapicsR3[i];
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetInitialCount,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Calls to apicTimerSetInitialCount.",   "/Devices/APIC/%u/TimerSetInitialCount", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetInitialCountArm,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerSetRelative calls.",            "/Devices/APIC/%u/TimerSetInitialCount/Arm", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetInitialCountDisarm, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerStop calls.",                   "/Devices/APIC/%u/TimerSetInitialCount/Disasm", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvt,                STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Calls to apicTimerSetLvt.",            "/Devices/APIC/%u/TimerSetLvt", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtClearPeriodic,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Clearing APIC_LVT_TIMER_PERIODIC.",    "/Devices/APIC/%u/TimerSetLvt/ClearPeriodic", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtPostponed,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerStop postponed.",               "/Devices/APIC/%u/TimerSetLvt/Postponed", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtArmed,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerSet avoided.",                  "/Devices/APIC/%u/TimerSetLvt/Armed", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtArm,             STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerSet necessary.",                "/Devices/APIC/%u/TimerSetLvt/Arm", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtArmRetries,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "TMTimerSet retries.",                  "/Devices/APIC/%u/TimerSetLvt/ArmRetries", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pApic->StatTimerSetLvtNoRelevantChange,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "No relevant flags changed.",           "/Devices/APIC/%u/TimerSetLvt/NoRelevantChange", i);
    }
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
    /* szName */
    "apic",
    /* szRCMod */
    "VBoxDD2GC.gc",
    /* szR0Mod */
    "VBoxDD2R0.r0",
    /* pszDescription */
    "Advanced Programmable Interrupt Controller (APIC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
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

