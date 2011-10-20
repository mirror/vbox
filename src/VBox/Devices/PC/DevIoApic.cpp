/* $Id$ */
/** @file
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
/** @def IOAPIC_LOCK
 * Acquires the PDM lock. */
#define IOAPIC_LOCK(pThis, rc) \
    do { \
        int rc2 = (pThis)->CTX_SUFF(pIoApicHlp)->pfnLock((pThis)->CTX_SUFF(pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)

/** @def IOAPIC_UNLOCK
 * Releases the PDM lock. */
#define IOAPIC_UNLOCK(pThis) (pThis)->CTX_SUFF(pIoApicHlp)->pfnUnlock((pThis)->CTX_SUFF(pDevIns))


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

#define DEBUG_IOAPIC
#define IOAPIC_NUM_PINS                 0x18


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
struct IOAPICState {
    uint8_t id;
    uint8_t ioregsel;

    uint32_t irr;
    uint64_t ioredtbl[IOAPIC_NUM_PINS];

    /** The device instance - R3 Ptr. */
    PPDMDEVINSR3            pDevInsR3;
    /** The IOAPIC helpers - R3 Ptr. */
    PCPDMIOAPICHLPR3        pIoApicHlpR3;

    /** The device instance - R0 Ptr. */
    PPDMDEVINSR0            pDevInsR0;
    /** The IOAPIC helpers - R0 Ptr. */
    PCPDMIOAPICHLPR0        pIoApicHlpR0;

    /** The device instance - RC Ptr. */
    PPDMDEVINSRC            pDevInsRC;
    /** The IOAPIC helpers - RC Ptr. */
    PCPDMIOAPICHLPRC        pIoApicHlpRC;

# ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             StatMMIOReadGC;
    STAMCOUNTER             StatMMIOReadHC;
    STAMCOUNTER             StatMMIOWriteGC;
    STAMCOUNTER             StatMMIOWriteHC;
    STAMCOUNTER             StatSetIrqGC;
    STAMCOUNTER             StatSetIrqHC;
# endif
};

typedef struct IOAPICState IOAPICState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


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
                    /* malc: i'm still not so sure about ExtINT delivery */
                {
                    AssertMsgFailed(("Delivery mode ExtINT"));
                    vector = 0xff; /* incorrect but shuts up gcc. */
                }
                else
                    vector = entry & 0xff;

                int rc = s->CTX_SUFF(pIoApicHlp)->pfnApicBusDeliver(s->CTX_SUFF(pDevIns),
                                                           dest,
                                                           dest_mode,
                                                           delivery_mode,
                                                           vector,
                                                           polarity,
                                                           trig_mode);
                /* We must be sure that attempts to reschedule in R3
                   never get here */
                Assert(rc == VINF_SUCCESS);
            }
        }
    }
}


static void ioapic_set_irq(void *opaque, int vector, int level)
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
                if ((level & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
                    s->irr &= ~mask;
                }
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

static uint32_t ioapic_mem_readl(void *opaque, RTGCPHYS addr)
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

static void ioapic_mem_writel(void *opaque, RTGCPHYS addr, uint32_t val)
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
                        /* According to IOAPIC spec, vectors should be from 0x10 to 0xfe */
                        uint8_t vec = val & 0xff;
                        if ((val & APIC_LVT_MASKED) ||
                            ((vec >= 0x10) && (vec < 0xff)))
                        {
                            s->ioredtbl[index] &= ~0xffffffffULL;
                            s->ioredtbl[index] |= val;
                        }
                        else
                        {
                            /*
                             * Linux 2.6 kernels has pretty strange function
                             * unlock_ExtINT_logic() which writes
                             * absolutely bogus (all 0) value into the vector
                             * with pretty vague explanation why.
                             * So we just ignore such writes.
                             */
                            LogRel(("IOAPIC GUEST BUG: bad vector writing %x(sel=%x) to %d\n", val, s->ioregsel, index));
                        }
                    }
                    ioapic_service(s);
                }
        }
    }
}

#ifdef IN_RING3

static void ioapic_save(SSMHANDLE *f, void *opaque)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int i;

    SSMR3PutU8(f, s->id);
    SSMR3PutU8(f, s->ioregsel);
    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        SSMR3PutU64(f, s->ioredtbl[i]);
    }
}

static int ioapic_load(SSMHANDLE *f, void *opaque, int version_id)
{
    IOAPICState *s = (IOAPICState*)opaque;
    int i;

    if (version_id != 1)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    SSMR3GetU8(f, &s->id);
    SSMR3GetU8(f, &s->ioregsel);
    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        SSMR3GetU64(f, &s->ioredtbl[i]);
    }
    return 0;
}

static void ioapic_reset(void *opaque)
{
    IOAPICState *s = (IOAPICState*)opaque;
    PPDMDEVINSR3        pDevIns    = s->pDevInsR3;
    PCPDMIOAPICHLPR3    pIoApicHlp = s->pIoApicHlpR3;
    int i;

    memset(s, 0, sizeof(*s));
    for(i = 0; i < IOAPIC_NUM_PINS; i++)
        s->ioredtbl[i] = 1 << 16; /* mask LVT */

    if (pDevIns)
    {
        s->pDevInsR3 = pDevIns;
        s->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
        s->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    }
    if (pIoApicHlp)
    {
        s->pIoApicHlpR3 = pIoApicHlp;
        s->pIoApicHlpRC = s->pIoApicHlpR3->pfnGetRCHelpers(pDevIns);
        s->pIoApicHlpR0 = s->pIoApicHlpR3->pfnGetR0Helpers(pDevIns);
    }
}

#endif /* IN_RING3 */


/* IOAPIC */

PDMBOTHCBDECL(int) ioapicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    IOAPIC_LOCK(s, VINF_IOM_HC_MMIO_READ);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIORead));
    switch (cb) {
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

PDMBOTHCBDECL(int) ioapicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);

    STAM_COUNTER_INC(&CTXSUFF(s->StatMMIOWrite));
    IOAPIC_LOCK(s, VINF_IOM_HC_MMIO_WRITE);
    switch (cb)
    {
        case 1: ioapic_mem_writel(s, GCPhysAddr, *(uint8_t  const *)pv); break;
        case 2: ioapic_mem_writel(s, GCPhysAddr, *(uint16_t const *)pv); break;
        case 4: ioapic_mem_writel(s, GCPhysAddr, *(uint32_t const *)pv); break;

        default:
            IOAPIC_UNLOCK(s);
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    IOAPIC_UNLOCK(s);
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(void) ioapicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    /* PDM lock is taken here; @todo add assertion */
    IOAPICState *pThis = PDMINS_2_DATA(pDevIns, IOAPICState *);
    STAM_COUNTER_INC(&pThis->CTXSUFF(StatSetIrq));
    LogFlow(("ioapicSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    ioapic_set_irq(pThis, iIrq, iLevel);
}

PDMBOTHCBDECL(void) ioapicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCAddr, uint32_t uValue)
{
    IOAPICState *pThis = PDMINS_2_DATA(pDevIns, IOAPICState *);

    LogFlow(("ioapicSendMsi: Address=%p uValue=%\n", GCAddr, uValue));

    uint8_t  dest = (GCAddr & VBOX_MSI_ADDR_DEST_ID_MASK) >> VBOX_MSI_ADDR_DEST_ID_SHIFT;
    uint8_t  vector_num = (uValue & VBOX_MSI_DATA_VECTOR_MASK) >> VBOX_MSI_DATA_VECTOR_SHIFT;
    uint8_t  dest_mode = (GCAddr >> VBOX_MSI_ADDR_DEST_MODE_SHIFT) & 0x1;
    uint8_t  trigger_mode = (uValue >> VBOX_MSI_DATA_TRIGGER_SHIFT) & 0x1;
    uint8_t  delivery_mode = (uValue >> VBOX_MSI_DATA_DELIVERY_MODE_SHIFT) & 0x7;
#if 0
    /*
     * This bit indicates whether the message should be directed to the
     * processor with the lowest interrupt priority among
     * processors that can receive the interrupt, ignored ATM.
     */
    uint8_t  redir_hint = (GCAddr >> VBOX_MSI_ADDR_REDIRECTION_SHIFT) & 0x1;
#endif
    int rc = pThis->CTX_SUFF(pIoApicHlp)->pfnApicBusDeliver(pDevIns,
                                                            dest,
                                                            dest_mode,
                                                            delivery_mode,
                                                            vector_num,
                                                            0 /* polarity, n/a */,
                                                            trigger_mode);
    /* We must be sure that attempts to reschedule in R3
       never get here */
    Assert(rc == VINF_SUCCESS);
}

#ifdef IN_RING3

/**
 * Info handler, device version. Dumps I/O APIC state.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) ioapicInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    uint32_t    val;
    unsigned    i;
    unsigned    max_redir;

    pHlp->pfnPrintf(pHlp, "I/O APIC at %08X:\n", 0xfec00000);
    val = s->id << 24;  /* Would be nice to call ioapic_mem_readl() directly, but that's not so simple. */
    pHlp->pfnPrintf(pHlp, "  IOAPICID  : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    APIC ID = %02X\n", (val >> 24) & 0xff);
    val = 0x11 | ((IOAPIC_NUM_PINS - 1) << 16);
    max_redir = (val >> 16) & 0xff;
    pHlp->pfnPrintf(pHlp, "  IOAPICVER : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    version = %02X\n", val & 0xff);
    pHlp->pfnPrintf(pHlp, "    redirs  = %d\n", ((val >> 16) & 0xff) + 1);
    val = 0;
    pHlp->pfnPrintf(pHlp, "  IOAPICARB : %08X\n", val);
    pHlp->pfnPrintf(pHlp, "    arb ID  = %02X\n", (val >> 24) & 0xff);
    Assert(sizeof(s->ioredtbl) / sizeof(s->ioredtbl[0]) > max_redir);
    pHlp->pfnPrintf(pHlp, "I/O redirection table\n");
    pHlp->pfnPrintf(pHlp, " idx dst_mode dst_addr mask trigger rirr polarity dlvr_st dlvr_mode vector\n");
    for (i = 0; i <= max_redir; ++i)
    {
        static const char *dmodes[] = { "Fixed ", "LowPri", "SMI   ", "Resrvd",
                                        "NMI   ", "INIT  ", "Resrvd", "ExtINT" };

        pHlp->pfnPrintf(pHlp, "  %02d   %s      %02X     %d    %s   %d   %s  %s     %s   %3d (%016llX)\n",
                        i,
                        s->ioredtbl[i] & (1 << 11) ? "log " : "phys",           /* dest mode */
                        (int)(s->ioredtbl[i] >> 56),                            /* dest addr */
                        (int)(s->ioredtbl[i] >> 16) & 1,                        /* mask */
                        s->ioredtbl[i] & (1 << 15) ? "level" : "edge ",         /* trigger */
                        (int)(s->ioredtbl[i] >> 14) & 1,                        /* remote IRR */
                        s->ioredtbl[i] & (1 << 13) ? "activelo" : "activehi",   /* polarity */
                        s->ioredtbl[i] & (1 << 12) ? "pend" : "idle",           /* delivery status */
                        dmodes[(s->ioredtbl[i] >> 8) & 0x07],                   /* delivery mode */
                        (int)s->ioredtbl[i] & 0xff,                             /* vector */
                        s->ioredtbl[i]                                          /* entire register */
                        );
    }
}

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) ioapicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    ioapic_save(pSSM, s);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) ioapicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);

    if (ioapic_load(pSSM, s, uVersion)) {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ioapicReset(PPDMDEVINS pDevIns)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    s->pIoApicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);
    ioapic_reset(s);
    IOAPIC_UNLOCK(s);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ioapicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    s->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    s->pIoApicHlpRC = s->pIoApicHlpR3->pfnGetRCHelpers(pDevIns);
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) ioapicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    IOAPICState *s = PDMINS_2_DATA(pDevIns, IOAPICState *);
    PDMIOAPICREG IoApicReg;
    bool         fGCEnabled;
    bool         fR0Enabled;
    int          rc;
    uint32_t     cCpus;

    Assert(iInstance == 0);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "NumCPUs\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

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

    Log(("IOAPIC: fR0Enabled=%RTbool fGCEnabled=%RTbool\n", fR0Enabled, fGCEnabled));

    /*
     * Initialize the state data.
     */
    s->pDevInsR3 = pDevIns;
    s->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    s->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    ioapic_reset(s);
    s->id = cCpus;

    /* PDM provides locking via the IOAPIC helpers. */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register the IOAPIC and get helpers.
     */
    IoApicReg.u32Version  = PDM_IOAPICREG_VERSION;
    IoApicReg.pfnSetIrqR3 = ioapicSetIrq;
    IoApicReg.pszSetIrqRC = fGCEnabled ? "ioapicSetIrq" : NULL;
    IoApicReg.pszSetIrqR0 = fR0Enabled ? "ioapicSetIrq" : NULL;
    IoApicReg.pfnSendMsiR3 = ioapicSendMsi;
    IoApicReg.pszSendMsiRC = fGCEnabled ? "ioapicSendMsi" : NULL;
    IoApicReg.pszSendMsiR0 = fR0Enabled ? "ioapicSendMsi" : NULL;

    rc = PDMDevHlpIOAPICRegister(pDevIns, &IoApicReg, &s->pIoApicHlpR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("IOAPICRegister -> %Rrc\n", rc));
        return rc;
    }

    /*
     * Register MMIO callbacks and saved state.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, 0xfec00000, 0x1000, s,
                               ioapicMMIOWrite, ioapicMMIORead, NULL, "I/O APIC Memory");
    if (RT_FAILURE(rc))
        return rc;

    if (fGCEnabled) {
        s->pIoApicHlpRC = s->pIoApicHlpR3->pfnGetRCHelpers(pDevIns);

        rc = PDMDevHlpMMIORegisterRC(pDevIns, 0xfec00000, 0x1000, 0,
                                     "ioapicMMIOWrite", "ioapicMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    if (fR0Enabled) {
        s->pIoApicHlpR0 = s->pIoApicHlpR3->pfnGetR0Helpers(pDevIns);

        rc = PDMDevHlpMMIORegisterR0(pDevIns, 0xfec00000, 0x1000, 0,
                                     "ioapicMMIOWrite", "ioapicMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    rc = PDMDevHlpSSMRegister(pDevIns, 1 /* version */, sizeof(*s), ioapicSaveExec, ioapicLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register debugger info callback.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "ioapic", "Display I/O APIC state.", ioapicInfo);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOReadGC,     STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOReadGC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOReadHC,     STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOReadHC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOWriteGC,    STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOWriteGC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatMMIOWriteHC,    STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOWriteHC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatSetIrqGC,       STAMTYPE_COUNTER,  "/Devices/IOAPIC/SetIrqGC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &s->StatSetIrqHC,       STAMTYPE_COUNTER,  "/Devices/IOAPIC/SetIrqHC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in HC.");
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
    /* szName */
    "ioapic",
    /* szRCMod */
    "VBoxDD2GC.gc",
    /* szR0Mod */
    "VBoxDD2R0.r0",
    /* pszDescription */
    "I/O Advanced Programmable Interrupt Controller (IO-APIC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
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
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
