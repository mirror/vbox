/* $Id$ */
/** @file
 * I/O Advanced Programmable Interrupt Controller (IO-APIC) Device.
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

#define DEBUG_IOAPIC
#define IOAPIC_NUM_PINS                 0x18


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct IOAPIC
{
    uint8_t                 id;
    uint8_t                 ioregsel;
    uint8_t                 cCpus;

    uint32_t                irr;
    uint64_t                ioredtbl[IOAPIC_NUM_PINS];
    /** The IRQ tags and source IDs for each pin (tracing purposes). */
    uint32_t                auTagSrc[IOAPIC_NUM_PINS];

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
} IOAPIC;
typedef IOAPIC *PIOAPIC;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


static void ioapic_service(PIOAPIC pThis)
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

    for (i = 0; i < IOAPIC_NUM_PINS; i++)
    {
        mask = 1 << i;
        if (pThis->irr & mask)
        {
            entry = pThis->ioredtbl[i];
            if (!(entry & APIC_LVT_MASKED))
            {
                trig_mode = ((entry >> 15) & 1);
                dest = entry >> 56;
                dest_mode = (entry >> 11) & 1;
                delivery_mode = (entry >> 8) & 7;
                polarity = (entry >> 13) & 1;
                uint32_t uTagSrc = pThis->auTagSrc[i];
                if (trig_mode == APIC_TRIGGER_EDGE)
                {
                    pThis->auTagSrc[i] = 0;
                    pThis->irr &= ~mask;
                }
                if (delivery_mode == APIC_DM_EXTINT)
                    /* malc: i'm still not so sure about ExtINT delivery */
                {
                    AssertMsgFailed(("Delivery mode ExtINT"));
                    vector = 0xff; /* incorrect but shuts up gcc. */
                }
                else
                    vector = entry & 0xff;

                int rc = pThis->CTX_SUFF(pIoApicHlp)->pfnApicBusDeliver(pThis->CTX_SUFF(pDevIns),
                                                                        dest,
                                                                        dest_mode,
                                                                        delivery_mode,
                                                                        vector,
                                                                        polarity,
                                                                        trig_mode,
                                                                        uTagSrc);
                /* We must be sure that attempts to reschedule in R3
                   never get here */
                Assert(rc == VINF_SUCCESS);
            }
        }
    }
}


static void ioapic_set_irq(PIOAPIC pThis, int vector, int level, uint32_t uTagSrc)
{
    if (vector >= 0 && vector < IOAPIC_NUM_PINS)
    {
        uint32_t mask = 1 << vector;
        uint64_t entry = pThis->ioredtbl[vector];

        if ((entry >> 15) & 1)
        {
            /* level triggered */
            if (level)
            {
                pThis->irr |= mask;
                if (!pThis->auTagSrc[vector])
                    pThis->auTagSrc[vector] = uTagSrc;
                else
                    pThis->auTagSrc[vector] = RT_BIT_32(31);

                ioapic_service(pThis);

                if ((level & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
                {
                    pThis->irr &= ~mask;
                    pThis->auTagSrc[vector] = 0;
                }
            }
            else
            {
                pThis->irr &= ~mask;
                pThis->auTagSrc[vector] = 0;
            }
        }
        else
        {
            /* edge triggered */
            if (level)
            {
                pThis->irr |= mask;
                if (!pThis->auTagSrc[vector])
                    pThis->auTagSrc[vector] = uTagSrc;
                else
                    pThis->auTagSrc[vector] = RT_BIT_32(31);

                ioapic_service(pThis);
            }
        }
    }
}


/**
 * Handles a read from the IOAPICID register.
 */
static int ioapic_IoApicId_r(PIOAPIC pThis, uint32_t *pu32Value)
{
    *pu32Value = (uint32_t)pThis->id << 24;
    return VINF_SUCCESS;
}


/**
 * Handles a write to the IOAPICID register.
 */
static int ioapic_IoApicId_w(PIOAPIC pThis, uint32_t u32Value)
{
    /* Note! Compared to the 82093AA spec, we've extended the IOAPIC
             identification from bits 27:24 to bits 31:24. */
    Log(("ioapic: IOAPICID %#x -> %#x\n", pThis->id, u32Value >> 24));
    pThis->id = u32Value >> 24;
    return VINF_SUCCESS;
}


/**
 * Handles a read from the IOAPICVER register.
 */
static int ioapic_IoApicVer_r(PIOAPIC pThis, uint32_t *pu32Value)
{
    *pu32Value = RT_MAKE_U32(0x11, IOAPIC_NUM_PINS - 1); /* (0x11 is the version.) */
    return VINF_SUCCESS;
}


/**
 * Handles a read from the IOAPICARB register.
 */
static int ioapic_IoApicArb_r(PIOAPIC pThis, uint32_t *pu32Value)
{
    *pu32Value = 0; /* (arbitration winner) */
    return VINF_SUCCESS;
}


/**
 * Handles a read from the IOREGSEL register.
 */
static int ioapic_IoRegSel_r(PIOAPIC pThis, uint32_t *pu32Value)
{
    *pu32Value = pThis->ioregsel;
    return VINF_SUCCESS;
}

/**
 * Handles a write to the IOREGSEL register.
 */
static int ioapic_IoRegSel_w(PIOAPIC pThis, uint32_t u32Value)
{
    Log2(("ioapic: IOREGSEL %#04x -> %#04x\n", pThis->ioregsel, u32Value & 0xff));
    /* Bits 7:0 are writable, the rest aren't. Confirmed on recent AMD box. */
    pThis->ioregsel = u32Value & 0xff;
    return VINF_SUCCESS;
}


/**
 * Handles a write to the IOWIN register.
 */
static int ioapic_IoWin_r(PIOAPIC pThis, uint32_t *pu32Value)
{
    int             rc = VINF_SUCCESS;
    uint32_t const  uIoRegSel = pThis->ioregsel;

    if (uIoRegSel == 0)
        rc = ioapic_IoApicId_r(pThis, pu32Value);
    else if (uIoRegSel == 1)
        rc = ioapic_IoApicVer_r(pThis, pu32Value);
    else if (uIoRegSel == 2)
        rc = ioapic_IoApicArb_r(pThis, pu32Value);
    /*
     * IOREDTBL0..IOREDTBL23.
     */
    else if (uIoRegSel - UINT32_C(0x10) < IOAPIC_NUM_PINS * 2)
    {
        uint32_t const  idxIoRedTbl = (uIoRegSel - UINT32_C(0x10)) >> 1;
        uint64_t        u64NewValue;
        if (!(uIoRegSel & 1))
            /** @todo r=bird: Do we need to emulate DELIVS or/and Remote IRR? */
            *pu32Value = RT_LODWORD(pThis->ioredtbl[idxIoRedTbl]);
        else
            *pu32Value = RT_HIDWORD(pThis->ioredtbl[idxIoRedTbl]);
    }
    else
    {
        Log(("ioapic: Attempt to read from register %#x.\n", uIoRegSel));
        *pu32Value = UINT32_MAX;
    }

    Log(("ioapic: IOWIN rd -> %#010x (%Rrc)\n", *pu32Value, rc));
    return rc;
}


/**
 * Handles a write to the IOWIN register.
 */
static int ioapic_IoWin_w(PIOAPIC pThis, uint32_t u32Value)
{
    int             rc = VINF_SUCCESS;
    uint32_t const  uIoRegSel = pThis->ioregsel;
    Log2(("ioapic: IOWIN[%#04x] = %#x\n", uIoRegSel, u32Value));

    /*
     * IOAPICID.
     */
    if (uIoRegSel == 0)
        rc = ioapic_IoApicId_w(pThis, u32Value);
    /*
     * IOREDTBL0..IOREDTBL23.
     */
    else if (uIoRegSel - UINT32_C(0x10) < IOAPIC_NUM_PINS * 2)
    {
        uint32_t const  idxIoRedTbl = (uIoRegSel - UINT32_C(0x10)) >> 1;
        uint64_t        u64NewValue;
        if (!(uIoRegSel & 1))
        {
            /*
             * Low DWORD.
             *
             * Have to do some sanity checks here because Linux 2.6 kernels
             * writes seemingly bogus value (u32Value = 0) in their
             * unlock_ExtINT_logic() function. Not sure what it's good for, but
             * we ran into trouble with INTVEC = 0.  Luckily the 82093AA specs
             * limits the INTVEC range to 0x10 thru 0xfe, so we use this to
             * ignore harmful values.
             *
             * Update: Looking at real hw (recent AMD), they don't reject
             * invalid vector numbers, at least not at this point. Could be that
             * some other code path needs to refuse something instead.  Results:
             *  - Writing 0 to lo+hi -> 0.
             *  - Writing ~0 to lo+hi -> 0xff0000000001afff.
             *  - Writing ~0 w/ DELMOD set to 011b or 110b (both reserved)
             *    results in DELMOD containing the reserved values.
             *  - Ditto with same + DELMOD in [0..7], DELMOD is stored as written.
             */
            if (   (u32Value & APIC_LVT_MASKED)
                || ((u32Value & UINT32_C(0xff)) - UINT32_C(0x10)) <= UINT32_C(0xee) /* (0xfe - 0x10 = 0xee) */ )
                u64NewValue = (pThis->ioredtbl[idxIoRedTbl] & (UINT64_C(0xffffffff00000000) | RT_BIT(14) | RT_BIT(12)))
                            | (u32Value & ~(RT_BIT(14) | RT_BIT(12)));
            else
            {
                LogRel(("IOAPIC GUEST BUG: bad vector writing %x(sel=%x) to %u\n", u32Value, uIoRegSel, idxIoRedTbl));
                u64NewValue = pThis->ioredtbl[idxIoRedTbl];
            }
        }
        else
        {
            /*
             * High DWORD.
             */
            u64NewValue = (pThis->ioredtbl[idxIoRedTbl] & UINT64_C(0x00000000ffffffff))
                        | ((uint64_t)(u32Value & UINT32_C(0xff000000)) << 32);
        }

        Log(("ioapic: IOREDTBL%u %#018llx -> %#018llx\n", idxIoRedTbl, pThis->ioredtbl[idxIoRedTbl], u64NewValue));
        pThis->ioredtbl[idxIoRedTbl] = u64NewValue;

        ioapic_service(pThis);
    }
    /*
     * Read-only or unknown registers. Log it.
     */
    else if (uIoRegSel == 1)
        Log(("ioapic: Attempt to write (%#x) to IOAPICVER.\n", u32Value));
    else if (uIoRegSel == 2)
        Log(("ioapic: Attempt to write (%#x) to IOAPICARB.\n", u32Value));
    else
        Log(("ioapic: Attempt to write (%#x) to register %#x.\n", u32Value, uIoRegSel));

    return rc;
}


PDMBOTHCBDECL(int) ioapicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    IOAPIC_LOCK(pThis, VINF_IOM_R3_MMIO_READ);

    STAM_COUNTER_INC(&CTXSUFF(pThis->StatMMIORead));

    /*
     * Pass it on to the register read handlers.
     * (See 0xff comments in ioapicMMIOWrite.)
     */
    int      rc;
    uint32_t offReg = GCPhysAddr & 0xff;
    if (offReg == 0)
        rc = ioapic_IoRegSel_r(pThis, (uint32_t *)pv);
    else if (offReg == 0x10)
        rc = ioapic_IoWin_r(pThis, (uint32_t *)pv);
    else
    {
        Log(("ioapicMMIORead: Invalid access: offReg=%#x\n", offReg));
        rc = VINF_IOM_MMIO_UNUSED_FF;
    }
    Log3(("ioapicMMIORead: @%#x -> %#x %Rrc\n", offReg, *(uint32_t *)pv, rc));

    IOAPIC_UNLOCK(pThis);
    return rc;
}

PDMBOTHCBDECL(int) ioapicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);

    STAM_COUNTER_INC(&CTXSUFF(pThis->StatMMIOWrite));
    IOAPIC_LOCK(pThis, VINF_IOM_R3_MMIO_WRITE);

    /*
     * Fetch the value.
     *
     * We've told IOM to only give us DWORD accesses.  Observations on AMD
     * indicates that unaligned writes get their missing bytes written as zero.
     */
    Assert(!(GCPhysAddr & 3)); Assert(cb == 4);
    uint32_t u32Value = *(uint32_t const *)pv;

    /*
     * The 0xff mask is because we don't really implement the APICBASE register
     * in the PIIX3, so if the guest tries to relocate the IOAPIC via PIIX3 we
     * won't know. The I/O APIC address is on the form FEC0xy00h, where xy is
     * programmable. Masking 0xff means we cover the y. The x would require
     * reregistering MMIO memory, which means the guest is out of luck there.
     */
    int      rc;
    uint32_t offReg = GCPhysAddr & 0xff;
    if (offReg == 0)
        rc = ioapic_IoRegSel_w(pThis, u32Value);
    else if (offReg == 0x10)
        rc = ioapic_IoWin_w(pThis, u32Value);
    else
    {
        Log(("ioapicMMIOWrite: Invalid access: offReg=%#x u32Value=%#x\n", offReg, u32Value));
        rc = VINF_SUCCESS;
    }
    Log3(("ioapicMMIOWrite: @%#x := %#x %Rrc\n", offReg, u32Value, rc));

    IOAPIC_UNLOCK(pThis);
    return rc;
}

PDMBOTHCBDECL(void) ioapicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    /* PDM lock is taken here; */ /** @todo add assertion */
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    STAM_COUNTER_INC(&pThis->CTXSUFF(StatSetIrq));
    LogFlow(("ioapicSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    ioapic_set_irq(pThis, iIrq, iLevel, uTagSrc);
}

PDMBOTHCBDECL(void) ioapicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCAddr, uint32_t uValue, uint32_t uTagSrc)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);

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
                                                            trigger_mode,
                                                            uTagSrc);
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
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    uint32_t     uVal;

    pHlp->pfnPrintf(pHlp, "I/O APIC at %#010x:\n", 0xfec00000);

    ioapic_IoApicId_r(pThis, &uVal);
    pHlp->pfnPrintf(pHlp, "  IOAPICID  : %#010x\n", uVal);
    pHlp->pfnPrintf(pHlp, "    APIC ID = %#04x\n", (uVal >> 24) & 0xff);

    ioapic_IoApicVer_r(pThis, &uVal);
    unsigned iLastRedir = RT_BYTE3(uVal);
    pHlp->pfnPrintf(pHlp, "  IOAPICVER : %#010x\n", uVal);
    pHlp->pfnPrintf(pHlp, "    version = %#04x\n", uVal & 0xff);
    pHlp->pfnPrintf(pHlp, "    redirs  = %u\n", iLastRedir + 1);

    ioapic_IoApicArb_r(pThis, &uVal);
    pHlp->pfnPrintf(pHlp, "  IOAPICARB : %#0108x\n", uVal);
    pHlp->pfnPrintf(pHlp, "    arb ID  = %#04x\n", RT_BYTE4(uVal));

    Assert(sizeof(pThis->ioredtbl) / sizeof(pThis->ioredtbl[0]) > iLastRedir);
    pHlp->pfnPrintf(pHlp, "I/O redirection table\n");
    pHlp->pfnPrintf(pHlp, " idx dst_mode dst_addr mask trigger rirr polarity dlvr_st dlvr_mode vector\n");
    for (unsigned i = 0; i <= iLastRedir; ++i)
    {
        static const char * const s_apszDModes[] =
        {
            "Fixed ", "LowPri", "SMI   ", "Resrvd", "NMI   ", "INIT  ", "Resrvd", "ExtINT"
        };

        pHlp->pfnPrintf(pHlp, "  %02d   %s      %02x     %d    %s   %d   %s  %s     %s   %3d (%016llx)\n",
                        i,
                        pThis->ioredtbl[i] & RT_BIT(11) ? "log " : "phys",          /* dest mode */
                        (int)(pThis->ioredtbl[i] >> 56),                            /* dest addr */
                        (int)(pThis->ioredtbl[i] >> 16) & 1,                        /* mask */
                        pThis->ioredtbl[i] & RT_BIT(15) ? "level" : "edge ",        /* trigger */
                        (int)(pThis->ioredtbl[i] >> 14) & 1,                        /* remote IRR */
                        pThis->ioredtbl[i] & RT_BIT(13) ? "activelo" : "activehi",  /* polarity */
                        pThis->ioredtbl[i] & RT_BIT(12) ? "pend" : "idle",          /* delivery status */
                        s_apszDModes[(pThis->ioredtbl[i] >> 8) & 0x07],             /* delivery mode */
                        (int)pThis->ioredtbl[i] & 0xff,                             /* vector */
                        pThis->ioredtbl[i]                                          /* entire register */
                        );
    }
}

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) ioapicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);

    SSMR3PutU8(pSSM, pThis->id);
    SSMR3PutU8(pSSM, pThis->ioregsel);
    for (unsigned i = 0; i < IOAPIC_NUM_PINS; i++)
        SSMR3PutU64(pSSM, pThis->ioredtbl[i]);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) ioapicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    if (uVersion != 1)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    SSMR3GetU8(pSSM, &pThis->id);
    SSMR3GetU8(pSSM, &pThis->ioregsel);
    for (unsigned i = 0; i < IOAPIC_NUM_PINS; i++)
        SSMR3GetU64(pSSM, &pThis->ioredtbl[i]);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ioapicReset(PPDMDEVINS pDevIns)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    pThis->pIoApicHlpR3->pfnLock(pDevIns, VERR_INTERNAL_ERROR);

    pThis->id       = pThis->cCpus;
    pThis->ioregsel = 0;
    pThis->irr      = 0;
    for (unsigned i = 0; i < IOAPIC_NUM_PINS; i++)
    {
        pThis->ioredtbl[i] = 1 << 16; /* mask LVT */
        pThis->auTagSrc[i] = 0;
    }

    IOAPIC_UNLOCK(pThis);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ioapicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pIoApicHlpRC = pThis->pIoApicHlpR3->pfnGetRCHelpers(pDevIns);
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) ioapicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PIOAPIC pThis = PDMINS_2_DATA(pDevIns, PIOAPIC);
    Assert(iInstance == 0);

    /*
     * Initialize the state data.
     */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    /* (the rest is done by the reset call at the end) */

    /* PDM provides locking via the IOAPIC helpers. */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "NumCPUs|RZEnabled", "");

    uint32_t cCpus;
    rc = CFGMR3QueryU32Def(pCfg, "NumCPUs", &cCpus, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query integer value \"NumCPUs\""));
    if (cCpus > UINT8_MAX - 2) /* ID 255 is broadcast and the IO-APIC needs one (ID=cCpus). */
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Max %u CPUs, %u specified"), UINT8_MAX - 1, cCpus);
    pThis->cCpus = (uint8_t)cCpus;

    bool fRZEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "RZEnabled", &fRZEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"RZEnabled\""));

    Log(("IOAPIC: cCpus=%u fRZEnabled=%RTbool\n", cCpus, fRZEnabled));

    /*
     * Register the IOAPIC and get helpers.
     */
    PDMIOAPICREG IoApicReg;
    IoApicReg.u32Version   = PDM_IOAPICREG_VERSION;
    IoApicReg.pfnSetIrqR3  = ioapicSetIrq;
    IoApicReg.pszSetIrqRC  = fRZEnabled ? "ioapicSetIrq"  : NULL;
    IoApicReg.pszSetIrqR0  = fRZEnabled ? "ioapicSetIrq"  : NULL;
    IoApicReg.pfnSendMsiR3 = ioapicSendMsi;
    IoApicReg.pszSendMsiRC = fRZEnabled ? "ioapicSendMsi" : NULL;
    IoApicReg.pszSendMsiR0 = fRZEnabled ? "ioapicSendMsi" : NULL;

    rc = PDMDevHlpIOAPICRegister(pDevIns, &IoApicReg, &pThis->pIoApicHlpR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("IOAPICRegister -> %Rrc\n", rc));
        return rc;
    }

    /*
     * Register MMIO callbacks and saved state.
     * Note! The write ZEROing was observed on a real AMD system.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, UINT32_C(0xfec00000), 0x1000, pThis,
                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED,
                               ioapicMMIOWrite, ioapicMMIORead, "I/O APIC Memory");
    if (RT_FAILURE(rc))
        return rc;

    if (fRZEnabled)
    {
        pThis->pIoApicHlpRC = pThis->pIoApicHlpR3->pfnGetRCHelpers(pDevIns);
        rc = PDMDevHlpMMIORegisterRC(pDevIns, UINT32_C(0xfec00000), 0x1000, NIL_RTRCPTR /*pvUser*/,
                                     "ioapicMMIOWrite", "ioapicMMIORead");
        AssertRCReturn(rc, rc);

        pThis->pIoApicHlpR0 = pThis->pIoApicHlpR3->pfnGetR0Helpers(pDevIns);
        rc = PDMDevHlpMMIORegisterR0(pDevIns, UINT32_C(0xfec00000), 0x1000, NIL_RTR0PTR /*pvUser*/,
                                     "ioapicMMIOWrite", "ioapicMMIORead");
        AssertRCReturn(rc, rc);
    }

    rc = PDMDevHlpSSMRegister(pDevIns, 1 /* version */, sizeof(*pThis), ioapicSaveExec, ioapicLoadExec);
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
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMMIOReadGC,     STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOReadGC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMMIOReadHC,     STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOReadHC",   STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMMIOWriteGC,    STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOWriteGC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMMIOWriteHC,    STAMTYPE_COUNTER,  "/Devices/IOAPIC/MMIOWriteHC",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in HC.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetIrqGC,       STAMTYPE_COUNTER,  "/Devices/IOAPIC/SetIrqGC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in GC.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetIrqHC,       STAMTYPE_COUNTER,  "/Devices/IOAPIC/SetIrqHC",     STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in HC.");
#endif

    /*
     * Reset the device state.
     */
    ioapicReset(pDevIns);

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
    sizeof(IOAPIC),
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
