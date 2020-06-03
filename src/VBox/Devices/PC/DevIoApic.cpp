/* $Id$ */
/** @file
 * IO APIC - Input/Output Advanced Programmable Interrupt Controller.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_IOAPIC
#include <VBox/log.h>
#include <VBox/vmm/hm.h>
#include <VBox/msi.h>
#include <VBox/vmm/pdmdev.h>

#include "VBoxDD.h"
#include <iprt/x86.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current IO APIC saved state version. */
#define IOAPIC_SAVED_STATE_VERSION              2
/** The saved state version used by VirtualBox 5.0 and
 *  earlier.  */
#define IOAPIC_SAVED_STATE_VERSION_VBOX_50      1

/** Implementation specified by the "Intel I/O Controller Hub 9
 *  (ICH9) Family" */
#define IOAPIC_VERSION_ICH9                     0x20
/** Implementation specified by the "82093AA I/O Advanced Programmable Interrupt
Controller" */
#define IOAPIC_VERSION_82093AA                  0x11

/** The default MMIO base physical address. */
#define IOAPIC_MMIO_BASE_PHYSADDR               UINT64_C(0xfec00000)
/** The size of the MMIO range. */
#define IOAPIC_MMIO_SIZE                        X86_PAGE_4K_SIZE
/** The mask for getting direct registers from physical address. */
#define IOAPIC_MMIO_REG_MASK                    0xff

/** The number of interrupt input pins. */
#define IOAPIC_NUM_INTR_PINS                    24
/** Maximum redirection entires. */
#define IOAPIC_MAX_RTE_INDEX                    (IOAPIC_NUM_INTR_PINS - 1)
/** Reduced RTEs used by SIO.A (82379AB). */
#define IOAPIC_REDUCED_MAX_RTE_INDEX            (16 - 1)

/** Version register - Gets the version. */
#define IOAPIC_VER_GET_VER(a_Reg)               ((a_Reg) & 0xff)
/** Version register - Gets the maximum redirection entry. */
#define IOAPIC_VER_GET_MRE(a_Reg)               (((a_Reg) >> 16) & 0xff)
/** Version register - Gets whether Pin Assertion Register (PRQ) is
 *  supported. */
#define IOAPIC_VER_HAS_PRQ(a_Reg)               RT_BOOL((a_Reg) & RT_BIT_32(15))

/** Index register - Valid write mask. */
#define IOAPIC_INDEX_VALID_WRITE_MASK           UINT32_C(0xff)

/** Arbitration register - Gets the ID. */
#define IOAPIC_ARB_GET_ID(a_Reg)                ((a_Reg) >> 24 & 0xf)

/** ID register - Gets the ID. */
#define IOAPIC_ID_GET_ID(a_Reg)                 ((a_Reg) >> 24 & 0xff)

/** Redirection table entry - Vector. */
#define IOAPIC_RTE_VECTOR                       UINT64_C(0xff)
/** Redirection table entry - Delivery mode. */
#define IOAPIC_RTE_DELIVERY_MODE                (RT_BIT_64(8) | RT_BIT_64(9) | RT_BIT_64(10))
/** Redirection table entry - Destination mode. */
#define IOAPIC_RTE_DEST_MODE                    RT_BIT_64(11)
/** Redirection table entry - Delivery status. */
#define IOAPIC_RTE_DELIVERY_STATUS              RT_BIT_64(12)
/** Redirection table entry - Interrupt input pin polarity. */
#define IOAPIC_RTE_POLARITY                     RT_BIT_64(13)
/** Redirection table entry - Remote IRR. */
#define IOAPIC_RTE_REMOTE_IRR                   RT_BIT_64(14)
/** Redirection table entry - Trigger Mode. */
#define IOAPIC_RTE_TRIGGER_MODE                 RT_BIT_64(15)
/** Redirection table entry - the mask bit number. */
#define IOAPIC_RTE_MASK_BIT                     16
/** Redirection table entry - the mask. */
#define IOAPIC_RTE_MASK                         RT_BIT_64(IOAPIC_RTE_MASK_BIT)
/** Redirection table entry - Extended Destination ID. */
#define IOAPIC_RTE_EXT_DEST_ID                  UINT64_C(0x00ff000000000000)
/** Redirection table entry - Destination. */
#define IOAPIC_RTE_DEST                         UINT64_C(0xff00000000000000)

/** Redirection table entry - Gets the destination. */
#define IOAPIC_RTE_GET_DEST(a_Reg)              ((a_Reg) >> 56 & 0xff)
/** Redirection table entry - Gets the mask flag. */
#define IOAPIC_RTE_GET_MASK(a_Reg)              (((a_Reg) >> IOAPIC_RTE_MASK_BIT) & 0x1)
/** Redirection table entry - Checks whether it's masked. */
#define IOAPIC_RTE_IS_MASKED(a_Reg)             ((a_Reg) & IOAPIC_RTE_MASK)
/** Redirection table entry - Gets the trigger mode. */
#define IOAPIC_RTE_GET_TRIGGER_MODE(a_Reg)      (((a_Reg) >> 15) & 0x1)
/** Redirection table entry - Gets the remote IRR flag. */
#define IOAPIC_RTE_GET_REMOTE_IRR(a_Reg)        (((a_Reg) >> 14) & 0x1)
/** Redirection table entry - Gets the interrupt pin polarity. */
#define IOAPIC_RTE_GET_POLARITY(a_Reg)          (((a_Reg) >> 13) & 0x1)
/** Redirection table entry - Gets the delivery status. */
#define IOAPIC_RTE_GET_DELIVERY_STATUS(a_Reg)   (((a_Reg) >> 12) & 0x1)
/** Redirection table entry - Gets the destination mode. */
#define IOAPIC_RTE_GET_DEST_MODE(a_Reg)         (((a_Reg) >> 11) & 0x1)
/** Redirection table entry - Gets the delivery mode. */
#define IOAPIC_RTE_GET_DELIVERY_MODE(a_Reg)     (((a_Reg) >> 8)  & 0x7)
/** Redirection table entry - Gets the vector. */
#define IOAPIC_RTE_GET_VECTOR(a_Reg)            ((a_Reg) & IOAPIC_RTE_VECTOR)

/** Redirection table entry - Valid write mask for 82093AA. */
#define IOAPIC_RTE_VALID_WRITE_MASK_82093AA     (  IOAPIC_RTE_DEST     | IOAPIC_RTE_MASK      | IOAPIC_RTE_TRIGGER_MODE \
                                                 | IOAPIC_RTE_POLARITY | IOAPIC_RTE_DEST_MODE | IOAPIC_RTE_DELIVERY_MODE \
                                                 | IOAPIC_RTE_VECTOR)
/** Redirection table entry - Valid read mask for 82093AA. */
#define IOAPIC_RTE_VALID_READ_MASK_82093AA      (  IOAPIC_RTE_DEST       | IOAPIC_RTE_MASK          | IOAPIC_RTE_TRIGGER_MODE \
                                                 | IOAPIC_RTE_REMOTE_IRR | IOAPIC_RTE_POLARITY      | IOAPIC_RTE_DELIVERY_STATUS \
                                                 | IOAPIC_RTE_DEST_MODE  | IOAPIC_RTE_DELIVERY_MODE | IOAPIC_RTE_VECTOR)

/** Redirection table entry - Valid write mask for ICH9. */
/** @note The remote IRR bit has been reverted to read-only as it turns out the
 *        ICH9 spec. is wrong, see @bugref{8386#c46}. */
#define IOAPIC_RTE_VALID_WRITE_MASK_ICH9        (  IOAPIC_RTE_DEST       | IOAPIC_RTE_MASK      | IOAPIC_RTE_TRIGGER_MODE \
                                                 /*| IOAPIC_RTE_REMOTE_IRR */| IOAPIC_RTE_POLARITY  | IOAPIC_RTE_DEST_MODE \
                                                 | IOAPIC_RTE_DELIVERY_MODE | IOAPIC_RTE_VECTOR)
/** Redirection table entry - Valid read mask (incl. ExtDestID) for ICH9. */
#define IOAPIC_RTE_VALID_READ_MASK_ICH9         (  IOAPIC_RTE_DEST            | IOAPIC_RTE_EXT_DEST_ID | IOAPIC_RTE_MASK \
                                                 | IOAPIC_RTE_TRIGGER_MODE    | IOAPIC_RTE_REMOTE_IRR  | IOAPIC_RTE_POLARITY \
                                                 | IOAPIC_RTE_DELIVERY_STATUS | IOAPIC_RTE_DEST_MODE   | IOAPIC_RTE_DELIVERY_MODE \
                                                 | IOAPIC_RTE_VECTOR)

/** Redirection table entry - Trigger mode edge. */
#define IOAPIC_RTE_TRIGGER_MODE_EDGE            0
/** Redirection table entry - Trigger mode level. */
#define IOAPIC_RTE_TRIGGER_MODE_LEVEL           1
/** Redirection table entry - Destination mode physical. */
#define IOAPIC_RTE_DEST_MODE_PHYSICAL           0
/** Redirection table entry - Destination mode logical. */
#define IOAPIC_RTE_DEST_MODE_LOGICAL            1


/** Index of indirect registers in the I/O APIC register table. */
#define IOAPIC_INDIRECT_INDEX_ID                0x0
#define IOAPIC_INDIRECT_INDEX_VERSION           0x1
#define IOAPIC_INDIRECT_INDEX_ARB               0x2     /* Older I/O APIC only. */
#define IOAPIC_INDIRECT_INDEX_REDIR_TBL_START   0x10    /* First valid RTE register index. */
#define IOAPIC_INDIRECT_INDEX_RTE_END           0x3F    /* Last valid RTE register index (24 RTEs). */
#define IOAPIC_REDUCED_INDIRECT_INDEX_RTE_END   0x2F    /* Last valid RTE register index (16 RTEs). */

/** Offset of direct registers in the I/O APIC MMIO space. */
#define IOAPIC_DIRECT_OFF_INDEX                 0x00
#define IOAPIC_DIRECT_OFF_DATA                  0x10
#define IOAPIC_DIRECT_OFF_EOI                   0x40    /* Newer I/O APIC only. */

/* Use PDM critsect for now for I/O APIC locking, see @bugref{8245#c121}. */
#define IOAPIC_WITH_PDM_CRITSECT
#ifdef IOAPIC_WITH_PDM_CRITSECT
# define IOAPIC_LOCK(a_pDevIns, a_pThis, a_pThisCC, rcBusy)  (a_pThisCC)->pIoApicHlp->pfnLock((a_pDevIns), (rcBusy))
# define IOAPIC_UNLOCK(a_pDevIns, a_pThis, a_pThisCC)        (a_pThisCC)->pIoApicHlp->pfnUnlock((a_pDevIns))
#else
# define IOAPIC_LOCK(a_pDevIns, a_pThis, a_pThisCC, rcBusy)  PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, (rcBusy))
# define IOAPIC_UNLOCK(a_pDevIns, a_pThis, a_pThisCC)        PDMDevHlpCritSectLeave((a_pDevIns), &(a_pThis)->CritSect)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The shared I/O APIC device state.
 */
typedef struct IOAPIC
{
    /** The ID register. */
    uint8_t volatile        u8Id;
    /** The index register. */
    uint8_t volatile        u8Index;
    /** Number of CPUs. */
    uint8_t                 cCpus;
    /** I/O APIC version. */
    uint8_t                 u8ApicVer;
    /** I/O APIC ID mask. */
    uint8_t                 u8IdMask;
    /** Maximum Redirection Table Entry (RTE) Entry. */
    uint8_t                 u8MaxRte;
    /** Last valid RTE indirect register index. */
    uint8_t                 u8LastRteRegIdx;
    /* Alignment padding. */
    uint8_t                 u8Padding0[1];
    /** Redirection table entry - Valid write mask. */
    uint64_t                u64RteWriteMask;
    /** Redirection table entry - Valid read mask. */
    uint64_t                u64RteReadMask;

    /** The redirection table registers. */
    uint64_t                au64RedirTable[IOAPIC_NUM_INTR_PINS];
    /** The IRQ tags and source IDs for each pin (tracing purposes). */
    uint32_t                au32TagSrc[IOAPIC_NUM_INTR_PINS];

    /** The internal IRR reflecting state of the interrupt lines. */
    uint32_t                uIrr;
    /** Alignment padding. */
    uint32_t                u32Padding2;

#ifndef IOAPIC_WITH_PDM_CRITSECT
    /** The critsect for updating to the RTEs. */
    PDMCRITSECT             CritSect;
#endif

    /** The MMIO region. */
    IOMMMIOHANDLE           hMmio;

#ifdef VBOX_WITH_STATISTICS
    /** Number of MMIO reads in RZ. */
    STAMCOUNTER             StatMmioReadRZ;
    /** Number of MMIO reads in R3. */
    STAMCOUNTER             StatMmioReadR3;

    /** Number of MMIO writes in RZ. */
    STAMCOUNTER             StatMmioWriteRZ;
    /** Number of MMIO writes in R3. */
    STAMCOUNTER             StatMmioWriteR3;

    /** Number of SetIrq calls in RZ. */
    STAMCOUNTER             StatSetIrqRZ;
    /** Number of SetIrq calls in R3. */
    STAMCOUNTER             StatSetIrqR3;

    /** Number of SetEoi calls in RZ. */
    STAMCOUNTER             StatSetEoiRZ;
    /** Number of SetEoi calls in R3. */
    STAMCOUNTER             StatSetEoiR3;

    /** Number of redundant edge-triggered interrupts. */
    STAMCOUNTER             StatRedundantEdgeIntr;
    /** Number of redundant level-triggered interrupts. */
    STAMCOUNTER             StatRedundantLevelIntr;
    /** Number of suppressed level-triggered interrupts (by remote IRR). */
    STAMCOUNTER             StatSuppressedLevelIntr;
    /** Number of returns to ring-3 due to EOI broadcast lock contention. */
    STAMCOUNTER             StatEoiContention;
    /** Number of returns to ring-3 due to Set RTE lock contention. */
    STAMCOUNTER             StatSetRteContention;
    /** Number of level-triggered interrupts dispatched to the local APIC(s). */
    STAMCOUNTER             StatLevelIrqSent;
    /** Number of EOIs received for level-triggered interrupts from the local
     *  APIC(s). */
    STAMCOUNTER             StatEoiReceived;
#endif
} IOAPIC;
AssertCompileMemberAlignment(IOAPIC, au64RedirTable, 8);
/** Pointer to shared IOAPIC data. */
typedef IOAPIC *PIOAPIC;
/** Pointer to const shared IOAPIC data. */
typedef IOAPIC const *PCIOAPIC;


/**
 * The I/O APIC device state for ring-3.
 */
typedef struct IOAPICR3
{
    /** The IOAPIC helpers. */
    R3PTRTYPE(PCPDMIOAPICHLP)   pIoApicHlp;
} IOAPICR3;
/** Pointer to the I/O APIC device state for ring-3. */
typedef IOAPICR3 *PIOAPICR3;


/**
 * The I/O APIC device state for ring-0.
 */
typedef struct IOAPICR0
{
    /** The IOAPIC helpers. */
    R0PTRTYPE(PCPDMIOAPICHLP)   pIoApicHlp;
} IOAPICR0;
/** Pointer to the I/O APIC device state for ring-0. */
typedef IOAPICR0 *PIOAPICR0;


/**
 * The I/O APIC device state for raw-mode.
 */
typedef struct IOAPICRC
{
    /** The IOAPIC helpers. */
    RCPTRTYPE(PCPDMIOAPICHLP)   pIoApicHlp;
} IOAPICRC;
/** Pointer to the I/O APIC device state for raw-mode. */
typedef IOAPICRC *PIOAPICRC;


/** The I/O APIC device state for the current context. */
typedef CTX_SUFF(IOAPIC) IOAPICCC;
/** Pointer to the I/O APIC device state for the current context. */
typedef CTX_SUFF(PIOAPIC) PIOAPICCC;


/**
 * xAPIC interrupt.
 */
typedef struct XAPICINTR
{
    /** The interrupt vector. */
    uint8_t         u8Vector;
    /** The destination (mask or ID). */
    uint8_t         u8Dest;
    /** The destination mode. */
    uint8_t         u8DestMode;
    /** Delivery mode. */
    uint8_t         u8DeliveryMode;
    /** Trigger mode. */
    uint8_t         u8TriggerMode;
    /** Redirection hint. */
    uint8_t         u8RedirHint;
    /** Polarity. */
    uint8_t         u8Polarity;
    /** Padding. */
    uint8_t         abPadding0;
} XAPICINTR;
/** Pointer to an I/O xAPIC interrupt struct. */
typedef XAPICINTR *PXAPICINTR;
/** Pointer to a const xAPIC interrupt struct. */
typedef XAPICINTR const *PCXAPICINTR;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * Gets the arbitration register.
 *
 * @returns The arbitration.
 */
DECLINLINE(uint32_t) ioapicGetArb(void)
{
    Log2(("IOAPIC: ioapicGetArb: returns 0\n"));
    return 0;
}


/**
 * Gets the version register.
 *
 * @returns The version.
 */
DECLINLINE(uint32_t) ioapicGetVersion(PCIOAPIC pThis)
{
    uint32_t uValue = RT_MAKE_U32(pThis->u8ApicVer, pThis->u8MaxRte);
    Log2(("IOAPIC: ioapicGetVersion: returns %#RX32\n", uValue));
    return uValue;
}


/**
 * Sets the ID register.
 *
 * @param   pThis       The shared I/O APIC device state.
 * @param   uValue      The value to set.
 */
DECLINLINE(void) ioapicSetId(PIOAPIC pThis, uint32_t uValue)
{
    Log2(("IOAPIC: ioapicSetId: uValue=%#RX32\n", uValue));
    ASMAtomicWriteU8(&pThis->u8Id, (uValue >> 24) & pThis->u8IdMask);
}


/**
 * Gets the ID register.
 *
 * @returns The ID.
 * @param   pThis       The shared I/O APIC device state.
 */
DECLINLINE(uint32_t) ioapicGetId(PCIOAPIC pThis)
{
    uint32_t uValue = (uint32_t)pThis->u8Id << 24;
    Log2(("IOAPIC: ioapicGetId: returns %#RX32\n", uValue));
    return uValue;
}


/**
 * Sets the index register.
 *
 * @param pThis     The shared I/O APIC device state.
 * @param uValue    The value to set.
 */
DECLINLINE(void) ioapicSetIndex(PIOAPIC pThis, uint32_t uValue)
{
    LogFlow(("IOAPIC: ioapicSetIndex: uValue=%#RX32\n", uValue));
    ASMAtomicWriteU8(&pThis->u8Index, uValue & IOAPIC_INDEX_VALID_WRITE_MASK);
}


/**
 * Gets the index register.
 *
 * @returns The index value.
 */
DECLINLINE(uint32_t) ioapicGetIndex(PCIOAPIC pThis)
{
    uint32_t const uValue = pThis->u8Index;
    LogFlow(("IOAPIC: ioapicGetIndex: returns %#x\n", uValue));
    return uValue;
}


/**
 * Converts an MSI message to an APIC interrupt.
 *
 * @param   pMsi    The MSI message to convert.
 * @param   pIntr   Where to store the APIC interrupt.
 */
DECLINLINE(void) ioapicGetApicIntrFromMsi(PCMSIMSG pMsi, PXAPICINTR pIntr)
{
    /*
     * Parse the message from the physical address and data
     * See Intel spec. 10.11.1 "Message Address Register Format".
     * See Intel spec. 10.11.2 "Message Data Register Format".
     */
    memset(pIntr, 0, sizeof(*pIntr));
    pIntr->u8Dest         = pMsi->MsiAddr.n.u8DestId;
    pIntr->u8DestMode     = pMsi->MsiAddr.n.u1DestMode;
    pIntr->u8RedirHint    = pMsi->MsiAddr.n.u1RedirHint;

    pIntr->u8Vector       = pMsi->MsiData.n.u8Vector;
    pIntr->u8TriggerMode  = pMsi->MsiData.n.u1TriggerMode;
    pIntr->u8DeliveryMode = pMsi->MsiData.n.u3DeliveryMode;
}

#if 0
/**
 * Convert an APIC interrupt to an MSI message.
 *
 * @param   pIntr   The APIC interrupt to convert.
 * @param   pMsi    Where to store the MSI message.
 */
DECLINLINE(void) ioapicGetMsiFromApicIntr(PCXAPICINTR pIntr, PMSIMSG pMsi)
{
    memset(pMsi, 0, sizeof(*pMsi));
    pMsi->MsiAddr.n.u12Addr        = VBOX_MSI_ADDR_BASE >> VBOX_MSI_ADDR_SHIFT;
    pMsi->MsiAddr.n.u8DestId       = pIntr->u8Dest;
    pMsi->MsiAddr.n.u1RedirHint    = pIntr->u8RedirHint;
    pMsi->MsiAddr.n.u1DestMode     = pIntr->u8DestMode;

    pMsi->MsiData.n.u8Vector       = pIntr->u8Vector;
    pMsi->MsiData.n.u3DeliveryMode = pIntr->u8DeliveryMode;
    pMsi->MsiData.n.u1TriggerMode  = pIntr->u8TriggerMode;

    /* pMsi->MsiData.n.u1Level     = ??? */
    /** @todo r=ramshankar: Level triggered MSIs don't make much sense though
     *        possible in theory? Maybe document this more explicitly... */
}
#endif

/**
 * Signals the next pending interrupt for the specified Redirection Table Entry
 * (RTE).
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared I/O APIC device state.
 * @param   pThisCC     The I/O APIC device state for the current context.
 * @param   idxRte      The index of the RTE (validated).
 *
 * @remarks It is the responsibility of the caller to verify that an interrupt is
 *          pending for the pin corresponding to the RTE before calling this
 *          function.
 */
static void ioapicSignalIntrForRte(PPDMDEVINS pDevIns, PIOAPIC pThis, PIOAPICCC pThisCC, uint8_t idxRte)
{
#ifndef IOAPIC_WITH_PDM_CRITSECT
    Assert(PDMCritSectIsOwner(&pThis->CritSect));
#endif

    /* Ensure the RTE isn't masked. */
    uint64_t const u64Rte = pThis->au64RedirTable[idxRte];
    if (!IOAPIC_RTE_IS_MASKED(u64Rte))
    {
        /* We cannot accept another level-triggered interrupt until remote IRR has been cleared. */
        uint8_t const u8TriggerMode = IOAPIC_RTE_GET_TRIGGER_MODE(u64Rte);
        if (u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_LEVEL)
        {
            uint8_t const u8RemoteIrr = IOAPIC_RTE_GET_REMOTE_IRR(u64Rte);
            if (u8RemoteIrr)
            {
                STAM_COUNTER_INC(&pThis->StatSuppressedLevelIntr);
                return;
            }
        }

        uint8_t const  u8Vector       = IOAPIC_RTE_GET_VECTOR(u64Rte);
        uint8_t const  u8DeliveryMode = IOAPIC_RTE_GET_DELIVERY_MODE(u64Rte);
        uint8_t const  u8DestMode     = IOAPIC_RTE_GET_DEST_MODE(u64Rte);
        uint8_t const  u8Polarity     = IOAPIC_RTE_GET_POLARITY(u64Rte);
        uint8_t const  u8Dest         = IOAPIC_RTE_GET_DEST(u64Rte);
        uint32_t const u32TagSrc      = pThis->au32TagSrc[idxRte];

        Log2(("IOAPIC: Signaling %s-triggered interrupt. Dest=%#x DestMode=%s Vector=%#x (%u)\n",
              u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_EDGE ? "edge" : "level", u8Dest,
              u8DestMode == IOAPIC_RTE_DEST_MODE_PHYSICAL ? "physical" : "logical", u8Vector, u8Vector));

        /*
         * Deliver to the local APIC via the system/3-wire-APIC bus.
         */
        int rc = pThisCC->pIoApicHlp->pfnApicBusDeliver(pDevIns,
                                                        u8Dest,
                                                        u8DestMode,
                                                        u8DeliveryMode,
                                                        u8Vector,
                                                        u8Polarity,
                                                        u8TriggerMode,
                                                        u32TagSrc);
        /* Can't reschedule to R3. */
        Assert(rc == VINF_SUCCESS || rc == VERR_APIC_INTR_DISCARDED);
#ifdef DEBUG_ramshankar
        if (rc == VERR_APIC_INTR_DISCARDED)
            AssertMsgFailed(("APIC: Interrupt discarded u8Vector=%#x (%u) u64Rte=%#RX64\n", u8Vector, u8Vector, u64Rte));
#endif

        /*
         * For level-triggered interrupts, we set the remote IRR bit to indicate
         * the local APIC has accepted the interrupt.
         *
         * For edge-triggered interrupts, we should not clear the IRR bit as it
         * should remain intact to reflect the state of the interrupt line.
         * The device will explicitly transition to inactive state via the
         * ioapicSetIrq() callback.
         */
        if (   u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_LEVEL
            && rc == VINF_SUCCESS)
        {
            Assert(u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_LEVEL);
            pThis->au64RedirTable[idxRte] |= IOAPIC_RTE_REMOTE_IRR;
            STAM_COUNTER_INC(&pThis->StatLevelIrqSent);
        }
    }
}


/**
 * Gets the redirection table entry.
 *
 * @returns The redirection table entry.
 * @param   pThis       The shared I/O APIC device state.
 * @param   uIndex      The index value.
 */
DECLINLINE(uint32_t) ioapicGetRedirTableEntry(PCIOAPIC pThis, uint32_t uIndex)
{
    uint8_t const idxRte = (uIndex - IOAPIC_INDIRECT_INDEX_REDIR_TBL_START) >> 1;
    AssertMsgReturn(idxRte < RT_ELEMENTS(pThis->au64RedirTable),
                    ("Invalid index %u, expected < %u\n", idxRte, RT_ELEMENTS(pThis->au64RedirTable)),
                    UINT32_MAX);
    uint32_t uValue;
    if (!(uIndex & 1))
        uValue = RT_LO_U32(pThis->au64RedirTable[idxRte]) & RT_LO_U32(pThis->u64RteReadMask);
    else
        uValue = RT_HI_U32(pThis->au64RedirTable[idxRte]) & RT_HI_U32(pThis->u64RteReadMask);

    LogFlow(("IOAPIC: ioapicGetRedirTableEntry: uIndex=%#RX32 idxRte=%u returns %#RX32\n", uIndex, idxRte, uValue));
    return uValue;
}


/**
 * Sets the redirection table entry.
 *
 * @returns Strict VBox status code (VINF_IOM_R3_MMIO_WRITE / VINF_SUCCESS).
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared I/O APIC device state.
 * @param   pThisCC     The I/O APIC device state for the current context.
 * @param   uIndex      The index value.
 * @param   uValue      The value to set.
 */
static VBOXSTRICTRC ioapicSetRedirTableEntry(PPDMDEVINS pDevIns, PIOAPIC pThis, PIOAPICCC pThisCC,
                                             uint32_t uIndex, uint32_t uValue)
{
    uint8_t const idxRte = (uIndex - IOAPIC_INDIRECT_INDEX_REDIR_TBL_START) >> 1;
    AssertMsgReturn(idxRte < RT_ELEMENTS(pThis->au64RedirTable),
                    ("Invalid index %u, expected < %u\n", idxRte, RT_ELEMENTS(pThis->au64RedirTable)),
                    VINF_SUCCESS);

    VBOXSTRICTRC rc = IOAPIC_LOCK(pDevIns, pThis, pThisCC, VINF_IOM_R3_MMIO_WRITE);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Write the low or high 32-bit value into the specified 64-bit RTE register,
         * update only the valid, writable bits.
         *
         * We need to preserve the read-only bits as it can have dire consequences
         * otherwise, see @bugref{8386#c24}.
         */
        uint64_t const u64Rte = pThis->au64RedirTable[idxRte];
        if (!(uIndex & 1))
        {
            uint32_t const u32RtePreserveLo = RT_LO_U32(u64Rte) & ~RT_LO_U32(pThis->u64RteWriteMask);
            uint32_t const u32RteNewLo      = (uValue & RT_LO_U32(pThis->u64RteWriteMask)) | u32RtePreserveLo;
            uint64_t const u64RteHi         = u64Rte & UINT64_C(0xffffffff00000000);
            pThis->au64RedirTable[idxRte]   = u64RteHi | u32RteNewLo;
        }
        else
        {
            uint32_t const u32RtePreserveHi = RT_HI_U32(u64Rte) & ~RT_HI_U32(pThis->u64RteWriteMask);
            uint32_t const u32RteLo         = RT_LO_U32(u64Rte);
            uint64_t const u64RteNewHi      = ((uint64_t)((uValue & RT_HI_U32(pThis->u64RteWriteMask)) | u32RtePreserveHi) << 32);
            pThis->au64RedirTable[idxRte]   = u64RteNewHi | u32RteLo;
        }

        /*
         * Signal the next pending interrupt for this RTE.
         */
        uint32_t const uPinMask = UINT32_C(1) << idxRte;
        if (pThis->uIrr & uPinMask)
            ioapicSignalIntrForRte(pDevIns, pThis, pThisCC, idxRte);

        IOAPIC_UNLOCK(pDevIns, pThis, pThisCC);
        LogFlow(("IOAPIC: ioapicSetRedirTableEntry: uIndex=%#RX32 idxRte=%u uValue=%#RX32\n", uIndex, idxRte, uValue));
    }
    else
        STAM_COUNTER_INC(&pThis->StatSetRteContention);

    return rc;
}


/**
 * Gets the data register.
 *
 * @returns The data value.
 * @param pThis     The shared I/O APIC device state.
 */
static uint32_t ioapicGetData(PCIOAPIC pThis)
{
    uint8_t const uIndex = pThis->u8Index;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    if (   uIndex >= IOAPIC_INDIRECT_INDEX_REDIR_TBL_START
        && uIndex <= pThis->u8LastRteRegIdx)
        return ioapicGetRedirTableEntry(pThis, uIndex);

    uint32_t uValue;
    switch (uIndex)
    {
        case IOAPIC_INDIRECT_INDEX_ID:
            uValue = ioapicGetId(pThis);
            break;

        case IOAPIC_INDIRECT_INDEX_VERSION:
            uValue = ioapicGetVersion(pThis);
            break;

        case IOAPIC_INDIRECT_INDEX_ARB:
            if (pThis->u8ApicVer == IOAPIC_VERSION_82093AA)
            {
                uValue = ioapicGetArb();
                break;
            }
            RT_FALL_THRU();

        default:
            uValue = UINT32_C(0xffffffff);
            Log2(("IOAPIC: Attempt to read register at invalid index %#x\n", uIndex));
            break;
    }
    return uValue;
}


/**
 * Sets the data register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared I/O APIC device state.
 * @param   pThisCC     The I/O APIC device state for the current context.
 * @param   uValue      The value to set.
 */
static VBOXSTRICTRC ioapicSetData(PPDMDEVINS pDevIns, PIOAPIC pThis, PIOAPICCC pThisCC, uint32_t uValue)
{
    uint8_t const uIndex = pThis->u8Index;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    LogFlow(("IOAPIC: ioapicSetData: uIndex=%#x uValue=%#RX32\n", uIndex, uValue));

    if (   uIndex >= IOAPIC_INDIRECT_INDEX_REDIR_TBL_START
        && uIndex <= pThis->u8LastRteRegIdx)
        return ioapicSetRedirTableEntry(pDevIns, pThis, pThisCC, uIndex, uValue);

    if (uIndex == IOAPIC_INDIRECT_INDEX_ID)
        ioapicSetId(pThis, uValue);
    else
        Log2(("IOAPIC: ioapicSetData: Invalid index %#RX32, ignoring write request with uValue=%#RX32\n", uIndex, uValue));

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIOAPICREG,pfnSetEoi}
 */
static DECLCALLBACK(VBOXSTRICTRC) ioapicSetEoi(PPDMDEVINS pDevIns, uint8_t u8Vector)
{
    PIOAPIC   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatSetEoi));
    LogFlow(("IOAPIC: ioapicSetEoi: u8Vector=%#x (%u)\n", u8Vector, u8Vector));

    bool fRemoteIrrCleared = false;
    VBOXSTRICTRC rc = IOAPIC_LOCK(pDevIns, pThis, pThisCC, VINF_IOM_R3_MMIO_WRITE);
    if (rc == VINF_SUCCESS)
    {
        for (uint8_t idxRte = 0; idxRte < RT_ELEMENTS(pThis->au64RedirTable); idxRte++)
        {
            uint64_t const u64Rte = pThis->au64RedirTable[idxRte];
            if (IOAPIC_RTE_GET_VECTOR(u64Rte) == u8Vector)
            {
#ifdef DEBUG_ramshankar
                /* This assertion may trigger when restoring saved-states created using the old, incorrect I/O APIC code. */
                Assert(IOAPIC_RTE_GET_REMOTE_IRR(u64Rte));
#endif
                pThis->au64RedirTable[idxRte] &= ~IOAPIC_RTE_REMOTE_IRR;
                fRemoteIrrCleared = true;
                STAM_COUNTER_INC(&pThis->StatEoiReceived);
                Log2(("IOAPIC: ioapicSetEoi: Cleared remote IRR, idxRte=%u vector=%#x (%u)\n", idxRte, u8Vector, u8Vector));

                /*
                 * Signal the next pending interrupt for this RTE.
                 */
                uint32_t const uPinMask = UINT32_C(1) << idxRte;
                if (pThis->uIrr & uPinMask)
                    ioapicSignalIntrForRte(pDevIns, pThis, pThisCC, idxRte);
            }
        }

        IOAPIC_UNLOCK(pDevIns, pThis, pThisCC);
        AssertMsg(fRemoteIrrCleared, ("Failed to clear remote IRR for vector %#x (%u)\n", u8Vector, u8Vector));
    }
    else
        STAM_COUNTER_INC(&pThis->StatEoiContention);

    return rc;
}


/**
 * @interface_method_impl{PDMIOAPICREG,pfnSetIrq}
 */
static DECLCALLBACK(void) ioapicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
#define IOAPIC_ASSERT_IRQ(a_idxRte, a_PinMask) do { \
        pThis->au32TagSrc[(a_idxRte)] = !pThis->au32TagSrc[(a_idxRte)] ? uTagSrc : RT_BIT_32(31); \
        pThis->uIrr |= a_PinMask; \
        ioapicSignalIntrForRte(pDevIns, pThis, pThisCC, (a_idxRte)); \
    } while (0)

    PIOAPIC   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    LogFlow(("IOAPIC: ioapicSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));

    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatSetIrq));

    if (RT_LIKELY((unsigned)iIrq < RT_ELEMENTS(pThis->au64RedirTable)))
    {
        int rc = IOAPIC_LOCK(pDevIns, pThis, pThisCC, VINF_SUCCESS);
        AssertRC(rc);

        uint8_t  const idxRte        = iIrq;
        uint32_t const uPinMask      = UINT32_C(1) << idxRte;
        uint32_t const u32RteLo      = RT_LO_U32(pThis->au64RedirTable[idxRte]);
        uint8_t  const u8TriggerMode = IOAPIC_RTE_GET_TRIGGER_MODE(u32RteLo);

        bool fActive = RT_BOOL(iLevel & 1);
        /** @todo Polarity is busted elsewhere, we need to fix that
         *        first. See @bugref{8386#c7}. */
#if 0
        uint8_t const u8Polarity = IOAPIC_RTE_GET_POLARITY(u32RteLo);
        fActive ^= u8Polarity; */
#endif
        if (!fActive)
        {
            pThis->uIrr &= ~uPinMask;
            IOAPIC_UNLOCK(pDevIns, pThis, pThisCC);
            return;
        }

        bool const     fFlipFlop = ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP);
        uint32_t const uPrevIrr  = pThis->uIrr & uPinMask;
        if (!fFlipFlop)
        {
            if (u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_EDGE)
            {
                /*
                 * For edge-triggered interrupts, we need to act only on a low to high edge transition.
                 * See ICH9 spec. 13.5.7 "REDIR_TBL: Redirection Table (LPC I/F-D31:F0)".
                 */
                if (!uPrevIrr)
                    IOAPIC_ASSERT_IRQ(idxRte, uPinMask);
                else
                {
                    STAM_COUNTER_INC(&pThis->StatRedundantEdgeIntr);
                    Log2(("IOAPIC: Redundant edge-triggered interrupt %#x (%u)\n", idxRte, idxRte));
                }
            }
            else
            {
                Assert(u8TriggerMode == IOAPIC_RTE_TRIGGER_MODE_LEVEL);

                /*
                 * For level-triggered interrupts, redundant interrupts are not a problem
                 * and will eventually be delivered anyway after an EOI, but our PDM devices
                 * should not typically call us with no change to the level.
                 */
                if (!uPrevIrr)
                { /* likely */ }
                else
                {
                    STAM_COUNTER_INC(&pThis->StatRedundantLevelIntr);
                    Log2(("IOAPIC: Redundant level-triggered interrupt %#x (%u)\n", idxRte, idxRte));
                }

                IOAPIC_ASSERT_IRQ(idxRte, uPinMask);
            }
        }
        else
        {
            /*
             * The device is flip-flopping the interrupt line, which implies we should de-assert
             * and assert the interrupt line. The interrupt line is left in the asserted state
             * after a flip-flop request. The de-assert is a NOP wrts to signaling an interrupt
             * hence just the assert is done.
             */
            IOAPIC_ASSERT_IRQ(idxRte, uPinMask);
        }

        IOAPIC_UNLOCK(pDevIns, pThis, pThisCC);
    }
#undef IOAPIC_ASSERT_IRQ
}


/**
 * @interface_method_impl{PDMIOAPICREG,pfnSendMsi}
 */
static DECLCALLBACK(void) ioapicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue, uint32_t uTagSrc)
{
    PIOAPICCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    LogFlow(("IOAPIC: ioapicSendMsi: GCPhys=%#RGp uValue=%#RX32\n", GCPhys, uValue));

    MSIMSG MsiMsg;
    MsiMsg.MsiAddr.u64 = GCPhys;
    MsiMsg.MsiData.u32 = uValue;

    XAPICINTR ApicIntr;
    ioapicGetApicIntrFromMsi(&MsiMsg, &ApicIntr);

    /*
     * Deliver to the local APIC via the system/3-wire-APIC bus.
     */
    int rc = pThisCC->pIoApicHlp->pfnApicBusDeliver(pDevIns,
                                                    ApicIntr.u8Dest,
                                                    ApicIntr.u8DestMode,
                                                    ApicIntr.u8DeliveryMode,
                                                    ApicIntr.u8Vector,
                                                    0 /* u8Polarity - N/A */,
                                                    ApicIntr.u8TriggerMode,
                                                    uTagSrc);
    /* Can't reschedule to R3. */
    Assert(rc == VINF_SUCCESS || rc == VERR_APIC_INTR_DISCARDED); NOREF(rc);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) ioapicMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PIOAPIC pThis = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioRead));
    Assert(cb == 4); RT_NOREF_PV(cb); /* registered for dwords only */
    RT_NOREF_PV(pvUser);

    VBOXSTRICTRC rc      = VINF_SUCCESS;
    uint32_t    *puValue = (uint32_t *)pv;
    uint32_t     offReg  = off & IOAPIC_MMIO_REG_MASK;
    switch (offReg)
    {
        case IOAPIC_DIRECT_OFF_INDEX:
            *puValue = ioapicGetIndex(pThis);
            break;

        case IOAPIC_DIRECT_OFF_DATA:
            *puValue = ioapicGetData(pThis);
            break;

        default:
            Log2(("IOAPIC: ioapicMmioRead: Invalid offset. off=%#RGp offReg=%#x\n", off, offReg));
            rc = VINF_IOM_MMIO_UNUSED_FF;
            break;
    }

    LogFlow(("IOAPIC: ioapicMmioRead: offReg=%#x, returns %#RX32\n", offReg, *puValue));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) ioapicMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    PIOAPIC   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    RT_NOREF_PV(pvUser);

    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioWrite));

    Assert(!(off & 3));
    Assert(cb == 4); RT_NOREF_PV(cb); /* registered for dwords only */

    VBOXSTRICTRC   rc     = VINF_SUCCESS;
    uint32_t const uValue = *(uint32_t const *)pv;
    uint32_t const offReg = off & IOAPIC_MMIO_REG_MASK;

    LogFlow(("IOAPIC: ioapicMmioWrite: pThis=%p off=%#RGp cb=%u uValue=%#RX32\n", pThis, off, cb, uValue));
    switch (offReg)
    {
        case IOAPIC_DIRECT_OFF_INDEX:
            ioapicSetIndex(pThis, uValue);
            break;

        case IOAPIC_DIRECT_OFF_DATA:
            rc = ioapicSetData(pDevIns, pThis, pThisCC, uValue);
            break;

        case IOAPIC_DIRECT_OFF_EOI:
            if (pThis->u8ApicVer == IOAPIC_VERSION_ICH9)
                rc = ioapicSetEoi(pDevIns, uValue);
            else
                Log(("IOAPIC: ioapicMmioWrite: Write to EOI register ignored!\n"));
            break;

        default:
            Log2(("IOAPIC: ioapicMmioWrite: Invalid offset. off=%#RGp offReg=%#x\n", off, offReg));
            break;
    }

    return rc;
}


#ifdef IN_RING3

/** @interface_method_impl{DBGFREGDESC,pfnGet} */
static DECLCALLBACK(int) ioapicR3DbgReg_GetIndex(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    RT_NOREF(pDesc);
    pValue->u32 = ioapicGetIndex(PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PCIOAPIC));
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnSet} */
static DECLCALLBACK(int) ioapicR3DbgReg_SetIndex(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    RT_NOREF(pDesc, pfMask);
    ioapicSetIndex(PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PIOAPIC), pValue->u8);
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnGet} */
static DECLCALLBACK(int) ioapicR3DbgReg_GetData(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    RT_NOREF(pDesc);
    pValue->u32 = ioapicGetData((PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PCIOAPIC)));
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnSet} */
static DECLCALLBACK(int) ioapicR3DbgReg_SetData(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    PPDMDEVINS pDevIns = (PPDMDEVINS)pvUser;
    PIOAPIC    pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC  pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    RT_NOREF(pDesc, pfMask);
    return VBOXSTRICTRC_VAL(ioapicSetData(pDevIns, pThis, pThisCC, pValue->u32));
}


/** @interface_method_impl{DBGFREGDESC,pfnGet} */
static DECLCALLBACK(int) ioapicR3DbgReg_GetVersion(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PCIOAPIC pThis = PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PCIOAPIC);
    RT_NOREF(pDesc);
    pValue->u32 = ioapicGetVersion(pThis);
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnGet} */
static DECLCALLBACK(int) ioapicR3DbgReg_GetArb(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    RT_NOREF(pvUser, pDesc);
    pValue->u32 = ioapicGetArb();
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnGet} */
static DECLCALLBACK(int) ioapicR3DbgReg_GetRte(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PCIOAPIC pThis = PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PCIOAPIC);
    Assert(pDesc->offRegister < RT_ELEMENTS(pThis->au64RedirTable));
    pValue->u64 = pThis->au64RedirTable[pDesc->offRegister];
    return VINF_SUCCESS;
}


/** @interface_method_impl{DBGFREGDESC,pfnSet} */
static DECLCALLBACK(int) ioapicR3DbgReg_SetRte(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    RT_NOREF(pfMask);
    PIOAPIC pThis = PDMDEVINS_2_DATA((PPDMDEVINS)pvUser, PIOAPIC);
    /* No locks, no checks, just do it. */
    Assert(pDesc->offRegister < RT_ELEMENTS(pThis->au64RedirTable));
    pThis->au64RedirTable[pDesc->offRegister] = pValue->u64;
    return VINF_SUCCESS;
}


/** IOREDTBLn sub fields. */
static DBGFREGSUBFIELD const g_aRteSubs[] =
{
    { "vector",       0,   8,  0,  0, NULL, NULL },
    { "dlvr_mode",    8,   3,  0,  0, NULL, NULL },
    { "dest_mode",    11,  1,  0,  0, NULL, NULL },
    { "dlvr_status",  12,  1,  0,  DBGFREGSUBFIELD_FLAGS_READ_ONLY, NULL, NULL },
    { "polarity",     13,  1,  0,  0, NULL, NULL },
    { "remote_irr",   14,  1,  0,  DBGFREGSUBFIELD_FLAGS_READ_ONLY, NULL, NULL },
    { "trigger_mode", 15,  1,  0,  0, NULL, NULL },
    { "mask",         16,  1,  0,  0, NULL, NULL },
    { "ext_dest_id",  48,  8,  0,  DBGFREGSUBFIELD_FLAGS_READ_ONLY, NULL, NULL },
    { "dest",         56,  8,  0,  0, NULL, NULL },
    DBGFREGSUBFIELD_TERMINATOR()
};


/** Register descriptors for DBGF. */
static DBGFREGDESC const g_aRegDesc[] =
{
    { "index",      DBGFREG_END, DBGFREGVALTYPE_U8,  0,  0, ioapicR3DbgReg_GetIndex, ioapicR3DbgReg_SetIndex,    NULL, NULL },
    { "data",       DBGFREG_END, DBGFREGVALTYPE_U32, 0,  0, ioapicR3DbgReg_GetData,  ioapicR3DbgReg_SetData,     NULL, NULL },
    { "version",    DBGFREG_END, DBGFREGVALTYPE_U32, DBGFREG_FLAGS_READ_ONLY, 0, ioapicR3DbgReg_GetVersion, NULL, NULL, NULL },
    { "arb",        DBGFREG_END, DBGFREGVALTYPE_U32, DBGFREG_FLAGS_READ_ONLY, 0, ioapicR3DbgReg_GetArb,     NULL, NULL, NULL },
    { "rte0",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  0, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte1",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  1, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte2",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  2, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte3",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  3, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte4",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  4, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte5",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  5, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte6",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  6, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte7",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  7, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte8",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  8, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte9",       DBGFREG_END, DBGFREGVALTYPE_U64, 0,  9, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte10",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 10, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte11",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 11, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte12",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 12, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte13",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 13, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte14",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 14, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte15",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 15, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte16",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 16, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte17",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 17, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte18",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 18, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte19",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 19, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte20",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 20, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte21",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 21, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte22",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 22, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    { "rte23",      DBGFREG_END, DBGFREGVALTYPE_U64, 0, 23, ioapicR3DbgReg_GetRte, ioapicR3DbgReg_SetRte, NULL, &g_aRteSubs[0] },
    DBGFREGDESC_TERMINATOR()
};


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) ioapicR3DbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);
    PCIOAPIC pThis = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    LogFlow(("IOAPIC: ioapicR3DbgInfo: pThis=%p pszArgs=%s\n", pThis, pszArgs));

    pHlp->pfnPrintf(pHlp, "I/O APIC at %#010x:\n", IOAPIC_MMIO_BASE_PHYSADDR);

    uint32_t const uId = ioapicGetId(pThis);
    pHlp->pfnPrintf(pHlp, "  ID                      = %#RX32\n", uId);
    pHlp->pfnPrintf(pHlp, "    ID                      = %#x\n",     IOAPIC_ID_GET_ID(uId));

    uint32_t const uVer = ioapicGetVersion(pThis);
    pHlp->pfnPrintf(pHlp, "  Version                 = %#RX32\n",  uVer);
    pHlp->pfnPrintf(pHlp, "    Version                 = %#x\n",     IOAPIC_VER_GET_VER(uVer));
    pHlp->pfnPrintf(pHlp, "    Pin Assert Reg. Support = %RTbool\n", IOAPIC_VER_HAS_PRQ(uVer));
    pHlp->pfnPrintf(pHlp, "    Max. Redirection Entry  = %u\n",      IOAPIC_VER_GET_MRE(uVer));

    if (pThis->u8ApicVer == IOAPIC_VERSION_82093AA)
    {
        uint32_t const uArb = ioapicGetArb();
        pHlp->pfnPrintf(pHlp, "  Arbitration             = %#RX32\n", uArb);
        pHlp->pfnPrintf(pHlp, "    Arbitration ID          = %#x\n",     IOAPIC_ARB_GET_ID(uArb));
    }

    pHlp->pfnPrintf(pHlp, "  Current index           = %#x\n",     ioapicGetIndex(pThis));

    pHlp->pfnPrintf(pHlp, "  I/O Redirection Table and IRR:\n");
    pHlp->pfnPrintf(pHlp, "  idx dst_mode dst_addr mask irr trigger rirr polar dlvr_st dlvr_mode vector\n");

    uint8_t const idxMaxRte = RT_MIN(pThis->u8MaxRte, RT_ELEMENTS(pThis->au64RedirTable) - 1);
    for (uint8_t idxRte = 0; idxRte <= idxMaxRte; idxRte++)
    {
        static const char * const s_apszDeliveryModes[] =
        {
            "Fixed ",
            "LowPri",
            "SMI   ",
            "Rsvd  ",
            "NMI   ",
            "INIT  ",
            "Rsvd  ",
            "ExtINT"
        };

        const uint64_t u64Rte = pThis->au64RedirTable[idxRte];
        const char    *pszDestMode       = IOAPIC_RTE_GET_DEST_MODE(u64Rte) == 0 ? "phys" : "log ";
        const uint8_t  uDest             = IOAPIC_RTE_GET_DEST(u64Rte);
        const uint8_t  uMask             = IOAPIC_RTE_GET_MASK(u64Rte);
        const char    *pszTriggerMode    = IOAPIC_RTE_GET_TRIGGER_MODE(u64Rte) == 0 ? "edge " : "level";
        const uint8_t  uRemoteIrr        = IOAPIC_RTE_GET_REMOTE_IRR(u64Rte);
        const char    *pszPolarity       = IOAPIC_RTE_GET_POLARITY(u64Rte) == 0 ? "acthi" : "actlo";
        const char    *pszDeliveryStatus = IOAPIC_RTE_GET_DELIVERY_STATUS(u64Rte) == 0 ? "idle" : "pend";
        const uint8_t  uDeliveryMode     = IOAPIC_RTE_GET_DELIVERY_MODE(u64Rte);
        Assert(uDeliveryMode < RT_ELEMENTS(s_apszDeliveryModes));
        const char    *pszDeliveryMode   = s_apszDeliveryModes[uDeliveryMode];
        const uint8_t  uVector           = IOAPIC_RTE_GET_VECTOR(u64Rte);

        pHlp->pfnPrintf(pHlp, "   %02d   %s      %02x     %u    %u   %s   %u   %s  %s     %s   %3u (%016llx)\n",
                        idxRte,
                        pszDestMode,
                        uDest,
                        uMask,
                        (pThis->uIrr >> idxRte) & 1,
                        pszTriggerMode,
                        uRemoteIrr,
                        pszPolarity,
                        pszDeliveryStatus,
                        pszDeliveryMode,
                        uVector,
                        u64Rte);
    }
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) ioapicR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PCIOAPIC        pThis = PDMDEVINS_2_DATA(pDevIns, PCIOAPIC);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    LogFlow(("IOAPIC: ioapicR3SaveExec\n"));

    pHlp->pfnSSMPutU32(pSSM, pThis->uIrr);
    pHlp->pfnSSMPutU8(pSSM,  pThis->u8Id);
    pHlp->pfnSSMPutU8(pSSM,  pThis->u8Index);
    for (uint8_t idxRte = 0; idxRte < RT_ELEMENTS(pThis->au64RedirTable); idxRte++)
        pHlp->pfnSSMPutU64(pSSM, pThis->au64RedirTable[idxRte]);

    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) ioapicR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PIOAPIC         pThis = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    LogFlow(("APIC: apicR3LoadExec: uVersion=%u uPass=%#x\n", uVersion, uPass));

    Assert(uPass == SSM_PASS_FINAL);
    NOREF(uPass);

    /* Weed out invalid versions. */
    if (   uVersion != IOAPIC_SAVED_STATE_VERSION
        && uVersion != IOAPIC_SAVED_STATE_VERSION_VBOX_50)
    {
        LogRel(("IOAPIC: ioapicR3LoadExec: Invalid/unrecognized saved-state version %u (%#x)\n", uVersion, uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (uVersion == IOAPIC_SAVED_STATE_VERSION)
        pHlp->pfnSSMGetU32(pSSM, &pThis->uIrr);

    pHlp->pfnSSMGetU8V(pSSM, &pThis->u8Id);
    pHlp->pfnSSMGetU8V(pSSM, &pThis->u8Index);
    for (uint8_t idxRte = 0; idxRte < RT_ELEMENTS(pThis->au64RedirTable); idxRte++)
        pHlp->pfnSSMGetU64(pSSM, &pThis->au64RedirTable[idxRte]);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) ioapicR3Reset(PPDMDEVINS pDevIns)
{
    PIOAPIC   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    LogFlow(("IOAPIC: ioapicR3Reset: pThis=%p\n", pThis));

    /* There might be devices threads calling ioapicSetIrq() in parallel, hence the lock. */
    IOAPIC_LOCK(pDevIns, pThis, pThisCC, VERR_IGNORED);

    pThis->uIrr    = 0;
    pThis->u8Index = 0;
    pThis->u8Id    = 0;

    for (uint8_t idxRte = 0; idxRte < RT_ELEMENTS(pThis->au64RedirTable); idxRte++)
    {
        pThis->au64RedirTable[idxRte] = IOAPIC_RTE_MASK;
        pThis->au32TagSrc[idxRte] = 0;
    }

    IOAPIC_UNLOCK(pDevIns, pThis, pThisCC);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) ioapicR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PIOAPICRC pThisRC = PDMINS_2_DATA_RC(pDevIns, PIOAPICRC);
    LogFlow(("IOAPIC: ioapicR3Relocate: pThis=%p offDelta=%RGi\n", PDMDEVINS_2_DATA(pDevIns, PIOAPIC), offDelta));

    pThisRC->pIoApicHlp += offDelta;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) ioapicR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PIOAPIC pThis = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    LogFlow(("IOAPIC: ioapicR3Destruct: pThis=%p\n", pThis));

# ifndef IOAPIC_WITH_PDM_CRITSECT
    /*
     * Destroy the RTE critical section.
     */
    if (PDMCritSectIsInitialized(&pThis->CritSect))
        PDMR3CritSectDelete(&pThis->CritSect);
# else
    RT_NOREF_PV(pThis);
# endif

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) ioapicR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOAPIC         pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC       pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    LogFlow(("IOAPIC: ioapicR3Construct: pThis=%p iInstance=%d\n", pThis, iInstance));
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "NumCPUs|ChipType", "");

    /* The number of CPUs is currently unused, but left in CFGM and saved-state in case an ID of 0 is
       upsets some guest which we haven't yet tested. */
    uint32_t cCpus;
    int rc = pHlp->pfnCFGMQueryU32Def(pCfg, "NumCPUs", &cCpus, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to query integer value \"NumCPUs\""));
    pThis->cCpus = (uint8_t)cCpus;

    char szChipType[16];
    rc = pHlp->pfnCFGMQueryStringDef(pCfg, "ChipType", &szChipType[0], sizeof(szChipType), "ICH9");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to query string value \"ChipType\""));

    if (!strcmp(szChipType, "ICH9"))
    {
        /* Newer 2007-ish I/O APIC integrated into ICH southbridges. */
        pThis->u8ApicVer       = IOAPIC_VERSION_ICH9;
        pThis->u8IdMask        = 0xff;
        pThis->u8MaxRte        = IOAPIC_MAX_RTE_INDEX;
        pThis->u8LastRteRegIdx = IOAPIC_INDIRECT_INDEX_RTE_END;
        pThis->u64RteWriteMask = IOAPIC_RTE_VALID_WRITE_MASK_ICH9;
        pThis->u64RteReadMask  = IOAPIC_RTE_VALID_READ_MASK_ICH9;
    }
    else if (!strcmp(szChipType, "82093AA"))
    {
        /* Older 1995-ish discrete I/O APIC, used in P6 class systems. */
        pThis->u8ApicVer       = IOAPIC_VERSION_82093AA;
        pThis->u8IdMask        = 0x0f;
        pThis->u8MaxRte        = IOAPIC_MAX_RTE_INDEX;
        pThis->u8LastRteRegIdx = IOAPIC_INDIRECT_INDEX_RTE_END;
        pThis->u64RteWriteMask = IOAPIC_RTE_VALID_WRITE_MASK_82093AA;
        pThis->u64RteReadMask  = IOAPIC_RTE_VALID_READ_MASK_82093AA;
    }
    else if (!strcmp(szChipType, "82379AB"))
    {
        /* Even older 1993-ish I/O APIC built into SIO.A, used in EISA and early PCI systems. */
        /* Exact same version and behavior as 82093AA, only the number of RTEs is different. */
        pThis->u8ApicVer       = IOAPIC_VERSION_82093AA;
        pThis->u8IdMask        = 0x0f;
        pThis->u8MaxRte        = IOAPIC_REDUCED_MAX_RTE_INDEX;
        pThis->u8LastRteRegIdx = IOAPIC_REDUCED_INDIRECT_INDEX_RTE_END;
        pThis->u64RteWriteMask = IOAPIC_RTE_VALID_WRITE_MASK_82093AA;
        pThis->u64RteReadMask  = IOAPIC_RTE_VALID_READ_MASK_82093AA;
    }
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES, RT_SRC_POS,
                                   N_("I/O APIC configuration error: The \"ChipType\" value \"%s\" is unsupported"), szChipType);
    Log2(("IOAPIC: cCpus=%u fRZEnabled=%RTbool szChipType=%s\n", cCpus, pDevIns->fR0Enabled | pDevIns->fRCEnabled, szChipType));

    /*
     * We will use our own critical section for the IOAPIC device.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

# ifndef IOAPIC_WITH_PDM_CRITSECT
    /*
     * Setup the critical section to protect concurrent writes to the RTEs.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "IOAPIC");
    AssertRCReturn(rc, rc);
# endif

    /*
     * Register the IOAPIC.
     */
    PDMIOAPICREG IoApicReg;
    IoApicReg.u32Version   = PDM_IOAPICREG_VERSION;
    IoApicReg.pfnSetIrq    = ioapicSetIrq;
    IoApicReg.pfnSendMsi   = ioapicSendMsi;
    IoApicReg.pfnSetEoi    = ioapicSetEoi;
    IoApicReg.u32TheEnd    = PDM_IOAPICREG_VERSION;
    rc = PDMDevHlpIoApicRegister(pDevIns, &IoApicReg, &pThisCC->pIoApicHlp);
    AssertRCReturn(rc, rc);

    /*
     * Register MMIO region.
     */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, IOAPIC_MMIO_BASE_PHYSADDR, IOAPIC_MMIO_SIZE, ioapicMmioWrite, ioapicMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "I/O APIC", &pThis->hMmio);
    AssertRCReturn(rc, rc);

    /*
     * Register the saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, IOAPIC_SAVED_STATE_VERSION, sizeof(*pThis), ioapicR3SaveExec, ioapicR3LoadExec);
    AssertRCReturn(rc, rc);

    /*
     * Register debugger info item.
     */
    rc = PDMDevHlpDBGFInfoRegister(pDevIns, "ioapic", "Display IO APIC state.", ioapicR3DbgInfo);
    AssertRCReturn(rc, rc);

    /*
     * Register debugger register access.
     */
    rc = PDMDevHlpDBGFRegRegister(pDevIns, g_aRegDesc);
    AssertRCReturn(rc, rc);

# ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadRZ,  STAMTYPE_COUNTER, "RZ/MmioReadRZ",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in RZ.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteRZ, STAMTYPE_COUNTER, "RZ/MmioWriteRZ", STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in RZ.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetIrqRZ,    STAMTYPE_COUNTER, "RZ/SetIrqRZ",    STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in RZ.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetEoiRZ,    STAMTYPE_COUNTER, "RZ/SetEoiRZ",    STAMUNIT_OCCURENCES, "Number of IOAPIC SetEoi calls in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadR3,  STAMTYPE_COUNTER, "R3/MmioReadR3",  STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO reads in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteR3, STAMTYPE_COUNTER, "R3/MmioWriteR3", STAMUNIT_OCCURENCES, "Number of IOAPIC MMIO writes in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetIrqR3,    STAMTYPE_COUNTER, "R3/SetIrqR3",    STAMUNIT_OCCURENCES, "Number of IOAPIC SetIrq calls in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetEoiR3,    STAMTYPE_COUNTER, "R3/SetEoiR3",    STAMUNIT_OCCURENCES, "Number of IOAPIC SetEoi calls in R3.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRedundantEdgeIntr,   STAMTYPE_COUNTER, "RedundantEdgeIntr",   STAMUNIT_OCCURENCES, "Number of redundant edge-triggered interrupts (no IRR change).");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRedundantLevelIntr,  STAMTYPE_COUNTER, "RedundantLevelIntr",  STAMUNIT_OCCURENCES, "Number of redundant level-triggered interrupts (no IRR change).");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSuppressedLevelIntr, STAMTYPE_COUNTER, "SuppressedLevelIntr", STAMUNIT_OCCURENCES, "Number of suppressed level-triggered interrupts by remote IRR.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatEoiContention,    STAMTYPE_COUNTER, "CritSect/ContentionSetEoi", STAMUNIT_OCCURENCES, "Number of times the critsect is busy during EOI writes causing trips to R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatSetRteContention, STAMTYPE_COUNTER, "CritSect/ContentionSetRte", STAMUNIT_OCCURENCES, "Number of times the critsect is busy during RTE writes causing trips to R3.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatLevelIrqSent, STAMTYPE_COUNTER, "LevelIntr/Sent", STAMUNIT_OCCURENCES, "Number of level-triggered interrupts sent to the local APIC(s).");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatEoiReceived,  STAMTYPE_COUNTER, "LevelIntr/Recv", STAMUNIT_OCCURENCES, "Number of EOIs received for level-triggered interrupts from the local APIC(s).");
# endif

    /*
     * Init. the device state.
     */
    LogRel(("IOAPIC: Using implementation 2.0! Chipset type %s\n", szChipType));
    ioapicR3Reset(pDevIns);

    return VINF_SUCCESS;
}

#else /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) ioapicRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOAPIC     pThis   = PDMDEVINS_2_DATA(pDevIns, PIOAPIC);
    PIOAPICCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOAPICCC);

    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    PDMIOAPICREG IoApicReg;
    IoApicReg.u32Version   = PDM_IOAPICREG_VERSION;
    IoApicReg.pfnSetIrq    = ioapicSetIrq;
    IoApicReg.pfnSendMsi   = ioapicSendMsi;
    IoApicReg.pfnSetEoi    = ioapicSetEoi;
    IoApicReg.u32TheEnd    = PDM_IOAPICREG_VERSION;
    rc = PDMDevHlpIoApicSetUpContext(pDevIns, &IoApicReg, &pThisCC->pIoApicHlp);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, ioapicMmioWrite, ioapicMmioRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * IO APIC device registration structure.
 */
const PDMDEVREG g_DeviceIOAPIC =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "ioapic",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE
                                    | PDM_DEVREG_FLAGS_REQUIRE_R0 | PDM_DEVREG_FLAGS_REQUIRE_RC,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(IOAPIC),
    /* .cbInstanceCC = */           sizeof(IOAPICCC),
    /* .cbInstanceRC = */           sizeof(IOAPICRC),
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "I/O Advanced Programmable Interrupt Controller (IO-APIC) Device",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           ioapicR3Construct,
    /* .pfnDestruct = */            ioapicR3Destruct,
    /* .pfnRelocate = */            ioapicR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               ioapicR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           ioapicRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           ioapicRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};


#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

