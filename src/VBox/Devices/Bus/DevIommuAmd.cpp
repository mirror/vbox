/* $Id$ */
/** @file
 * IOMMU - Input/Output Memory Management Unit - AMD implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_IOMMU
#include <VBox/msi.h>
#include <VBox/iommu-amd.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/AssertGuest.h>

#include <iprt/x86.h>
#include <iprt/alloc.h>
#include <iprt/string.h>

#include "VBoxDD.h"
#include "DevIommuAmd.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Release log prefix string. */
#define IOMMU_LOG_PFX                               "IOMMU-AMD"
/** The current saved state version. */
#define IOMMU_SAVED_STATE_VERSION                   1
/** The IOTLB entry magic. */
#define IOMMU_IOTLBE_MAGIC                          0x10acce55

#ifndef DEBUG_ramshankar
/** Temporary, make permanent later (get rid of define entirely and remove old
 *  code). This allow ssub-qword accesses to qword registers. Write accesses
 *  seems to work (needs testing one sub-path of the code), Read accesses not yet
 *  converted. */
# define IOMMU_NEW_REGISTER_ACCESS
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Acquires the IOMMU PDM lock.
 * This will make a long jump to ring-3 to acquire the lock if necessary.
 */
#define IOMMU_LOCK(a_pDevIns)  \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), (a_pDevIns)->CTX_SUFF(pCritSectRo), VINF_SUCCESS); \
        if (RT_LIKELY(rcLock == VINF_SUCCESS)) \
        { /* likely */ } \
        else \
            return rcLock; \
    } while (0)

/**
 * Acquires the IOMMU PDM lock (asserts on failure rather than returning an error).
 * This will make a long jump to ring-3 to acquire the lock if necessary.
 */
#define IOMMU_LOCK_NORET(a_pDevIns)  \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), (a_pDevIns)->CTX_SUFF(pCritSectRo), VINF_SUCCESS); \
        AssertRC(rcLock); \
    } while (0)

/**
 * Releases the IOMMU PDM lock.
 */
#define IOMMU_UNLOCK(a_pDevIns) \
    do { \
        PDMDevHlpCritSectLeave((a_pDevIns), (a_pDevIns)->CTX_SUFF(pCritSectRo)); \
    } while (0)

/**
 * Asserts that the critsect is owned by this thread.
 */
#define IOMMU_ASSERT_LOCKED(a_pDevIns) \
    do { \
        Assert(PDMDevHlpCritSectIsOwner(pDevIns, pDevIns->CTX_SUFF(pCritSectRo))); \
    }  while (0)

/**
 * Asserts that the critsect is not owned by this thread.
 */
#define IOMMU_ASSERT_NOT_LOCKED(a_pDevIns) \
    do { \
        Assert(!PDMDevHlpCritSectIsOwner(pDevIns, pDevIns->CTX_SUFF(pCritSectRo))); \
    }  while (0)

/**
 * IOMMU operations (transaction) types.
 */
typedef enum IOMMUOP
{
    /** Address translation request. */
    IOMMUOP_TRANSLATE_REQ = 0,
    /** Memory read request. */
    IOMMUOP_MEM_READ,
    /** Memory write request. */
    IOMMUOP_MEM_WRITE,
    /** Interrupt request. */
    IOMMUOP_INTR_REQ,
    /** Command. */
    IOMMUOP_CMD
} IOMMUOP;
AssertCompileSize(IOMMUOP, 4);

/**
 * I/O page walk result.
 */
typedef struct
{
    /** The translated system physical address. */
    RTGCPHYS        GCPhysSpa;
    /** The number of offset bits in the system physical address. */
    uint8_t         cShift;
    /** The I/O permissions allowed by the translation (IOMMU_IO_PERM_XXX). */
    uint8_t         fIoPerm;
    /** Padding. */
    uint8_t         abPadding[2];
} IOWALKRESULT;
/** Pointer to an I/O walk result struct. */
typedef IOWALKRESULT *PIOWALKRESULT;
/** Pointer to a const I/O walk result struct. */
typedef IOWALKRESULT *PCIOWALKRESULT;

/**
 * IOMMU I/O TLB Entry.
 * Keep this as small and aligned as possible.
 */
typedef struct
{
    /** The translated system physical address (SPA) of the page. */
    RTGCPHYS        GCPhysSpa;
    /** The index of the 4K page within a large page. */
    uint32_t        idxSubPage;
    /** The I/O access permissions (IOMMU_IO_PERM_XXX). */
    uint8_t         fIoPerm;
    /** The number of offset bits in the translation indicating page size. */
    uint8_t         cShift;
    /** Alignment padding. */
    uint8_t         afPadding[2];
} IOTLBE_T;
AssertCompileSize(IOTLBE_T, 16);
/** Pointer to an IOMMU I/O TLB entry struct. */
typedef IOTLBE_T *PIOTLBE_T;
/** Pointer to a const IOMMU I/O TLB entry struct. */
typedef IOTLBE_T const *PCIOTLBE_T;

/**
 * The shared IOMMU device state.
 */
typedef struct IOMMU
{
    /** IOMMU device index (0 is at the top of the PCI tree hierarchy). */
    uint32_t                    idxIommu;
    /** Alignment padding. */
    uint32_t                    uPadding0;

    /** Whether the command thread is sleeping. */
    bool volatile               fCmdThreadSleeping;
    /** Alignment padding. */
    uint8_t                     afPadding0[3];
    /** Whether the command thread has been signaled for wake up. */
    bool volatile               fCmdThreadSignaled;
    /** Alignment padding. */
    uint8_t                     afPadding1[3];

    /** The event semaphore the command thread waits on. */
    SUPSEMEVENT                 hEvtCmdThread;
    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

    /** @name PCI: Base capability block registers.
     * @{ */
    IOMMU_BAR_T                 IommuBar;               /**< IOMMU base address register. */
    /** @} */

    /** @name MMIO: Control and status registers.
     * @{ */
    DEV_TAB_BAR_T               aDevTabBaseAddrs[8];    /**< Device table base address registers. */
    CMD_BUF_BAR_T               CmdBufBaseAddr;         /**< Command buffer base address register. */
    EVT_LOG_BAR_T               EvtLogBaseAddr;         /**< Event log base address register. */
    IOMMU_CTRL_T                Ctrl;                   /**< IOMMU control register. */
    IOMMU_EXCL_RANGE_BAR_T      ExclRangeBaseAddr;      /**< IOMMU exclusion range base register. */
    IOMMU_EXCL_RANGE_LIMIT_T    ExclRangeLimit;         /**< IOMMU exclusion range limit. */
    IOMMU_EXT_FEAT_T            ExtFeat;                /**< IOMMU extended feature register. */
    /** @} */

    /** @name MMIO: Peripheral Page Request (PPR) Log registers.
     * @{ */
    PPR_LOG_BAR_T               PprLogBaseAddr;         /**< PPR Log base address register. */
    IOMMU_HW_EVT_HI_T           HwEvtHi;                /**< IOMMU hardware event register (Hi). */
    IOMMU_HW_EVT_LO_T           HwEvtLo;                /**< IOMMU hardware event register (Lo). */
    IOMMU_HW_EVT_STATUS_T       HwEvtStatus;            /**< IOMMU hardware event status. */
    /** @} */

    /** @todo IOMMU: SMI filter. */

    /** @name MMIO: Guest Virtual-APIC Log registers.
     * @{ */
    GALOG_BAR_T                 GALogBaseAddr;          /**< Guest Virtual-APIC Log base address register. */
    GALOG_TAIL_ADDR_T           GALogTailAddr;          /**< Guest Virtual-APIC Log Tail address register. */
    /** @} */

    /** @name MMIO: Alternate PPR and Event Log registers.
     *  @{ */
    PPR_LOG_B_BAR_T             PprLogBBaseAddr;        /**< PPR Log B base address register. */
    EVT_LOG_B_BAR_T             EvtLogBBaseAddr;        /**< Event Log B base address register. */
    /** @} */

    /** @name MMIO: Device-specific feature registers.
     * @{ */
    DEV_SPECIFIC_FEAT_T         DevSpecificFeat;        /**< Device-specific feature extension register (DSFX). */
    DEV_SPECIFIC_CTRL_T         DevSpecificCtrl;        /**< Device-specific control extension register (DSCX). */
    DEV_SPECIFIC_STATUS_T       DevSpecificStatus;      /**< Device-specific status extension register (DSSX). */
    /** @} */

    /** @name MMIO: MSI Capability Block registers.
     * @{ */
    MSI_MISC_INFO_T             MiscInfo;               /**< MSI Misc. info registers / MSI Vector registers. */
    /** @} */

    /** @name MMIO: Performance Optimization Control registers.
     *  @{ */
    IOMMU_PERF_OPT_CTRL_T       PerfOptCtrl;            /**< IOMMU Performance optimization control register. */
    /** @} */

    /** @name MMIO: x2APIC Control registers.
     * @{ */
    IOMMU_XT_GEN_INTR_CTRL_T    XtGenIntrCtrl;          /**< IOMMU X2APIC General interrupt control register. */
    IOMMU_XT_PPR_INTR_CTRL_T    XtPprIntrCtrl;          /**< IOMMU X2APIC PPR interrupt control register. */
    IOMMU_XT_GALOG_INTR_CTRL_T  XtGALogIntrCtrl;        /**< IOMMU X2APIC Guest Log interrupt control register. */
    /** @} */

    /** @name MMIO: Memory Address Routing & Control (MARC) registers.
     * @{ */
    MARC_APER_T                 aMarcApers[4];          /**< MARC Aperture Registers. */
    /** @} */

    /** @name MMIO: Reserved register.
     *  @{ */
    IOMMU_RSVD_REG_T            RsvdReg;                /**< IOMMU Reserved Register. */
    /** @} */

    /** @name MMIO: Command and Event Log pointer registers.
     * @{ */
    CMD_BUF_HEAD_PTR_T          CmdBufHeadPtr;          /**< Command buffer head pointer register. */
    CMD_BUF_TAIL_PTR_T          CmdBufTailPtr;          /**< Command buffer tail pointer register. */
    EVT_LOG_HEAD_PTR_T          EvtLogHeadPtr;          /**< Event log head pointer register. */
    EVT_LOG_TAIL_PTR_T          EvtLogTailPtr;          /**< Event log tail pointer register. */
    /** @} */

    /** @name MMIO: Command and Event Status register.
     * @{ */
    IOMMU_STATUS_T              Status;                 /**< IOMMU status register. */
    /** @} */

    /** @name MMIO: PPR Log Head and Tail pointer registers.
     * @{ */
    PPR_LOG_HEAD_PTR_T          PprLogHeadPtr;          /**< IOMMU PPR log head pointer register. */
    PPR_LOG_TAIL_PTR_T          PprLogTailPtr;          /**< IOMMU PPR log tail pointer register. */
    /** @} */

    /** @name MMIO: Guest Virtual-APIC Log Head and Tail pointer registers.
     * @{ */
    GALOG_HEAD_PTR_T            GALogHeadPtr;           /**< Guest Virtual-APIC log head pointer register. */
    GALOG_TAIL_PTR_T            GALogTailPtr;           /**< Guest Virtual-APIC log tail pointer register. */
    /** @} */

    /** @name MMIO: PPR Log B Head and Tail pointer registers.
     *  @{ */
    PPR_LOG_B_HEAD_PTR_T        PprLogBHeadPtr;         /**< PPR log B head pointer register. */
    PPR_LOG_B_TAIL_PTR_T        PprLogBTailPtr;         /**< PPR log B tail pointer register. */
    /** @} */

    /** @name MMIO: Event Log B Head and Tail pointer registers.
     * @{ */
    EVT_LOG_B_HEAD_PTR_T        EvtLogBHeadPtr;         /**< Event log B head pointer register. */
    EVT_LOG_B_TAIL_PTR_T        EvtLogBTailPtr;         /**< Event log B tail pointer register. */
    /** @} */

    /** @name MMIO: PPR Log Overflow protection registers.
     * @{ */
    PPR_LOG_AUTO_RESP_T         PprLogAutoResp;         /**< PPR Log Auto Response register. */
    PPR_LOG_OVERFLOW_EARLY_T    PprLogOverflowEarly;    /**< PPR Log Overflow Early Indicator register. */
    PPR_LOG_B_OVERFLOW_EARLY_T  PprLogBOverflowEarly;   /**< PPR Log B Overflow Early Indicator register. */
    /** @} */

    /** @todo IOMMU: IOMMU Event counter registers. */

#ifdef VBOX_WITH_STATISTICS
    /** @name IOMMU: Stat counters.
     * @{ */
    STAMCOUNTER             StatMmioReadR3;             /**< Number of MMIO reads in R3. */
    STAMCOUNTER             StatMmioReadRZ;             /**< Number of MMIO reads in RZ. */

    STAMCOUNTER             StatMmioWriteR3;            /**< Number of MMIO writes in R3. */
    STAMCOUNTER             StatMmioWriteRZ;            /**< Number of MMIO writes in RZ. */

    STAMCOUNTER             StatMsiRemapR3;             /**< Number of MSI remap requests in R3. */
    STAMCOUNTER             StatMsiRemapRZ;             /**< Number of MSI remap requests in RZ. */

    STAMCOUNTER             StatCmd;                    /**< Number of commands processed. */
    STAMCOUNTER             StatCmdCompWait;            /**< Number of Completion Wait commands processed. */
    STAMCOUNTER             StatCmdInvDte;              /**< Number of Invalidate DTE commands processed. */
    STAMCOUNTER             StatCmdInvIommuPages;       /**< Number of Invalidate IOMMU pages commands processed. */
    STAMCOUNTER             StatCmdInvIotlbPages;       /**< Number of Invalidate IOTLB pages commands processed. */
    STAMCOUNTER             StatCmdInvIntrTable;        /**< Number of Invalidate Interrupt Table commands processed. */
    STAMCOUNTER             StatCmdPrefIommuPages;      /**< Number of Prefetch IOMMU Pages commands processed. */
    STAMCOUNTER             StatCmdCompletePprReq;      /**< Number of Complete PPR Requests commands processed. */
    STAMCOUNTER             StatCmdInvIommuAll;         /**< Number of Invalidate IOMMU All commands processed. */
    /** @} */
#endif
} IOMMU;
/** Pointer to the IOMMU device state. */
typedef struct IOMMU *PIOMMU;
/** Pointer to the const IOMMU device state. */
typedef const struct IOMMU *PCIOMMU;
AssertCompileMemberAlignment(IOMMU, fCmdThreadSleeping, 4);
AssertCompileMemberAlignment(IOMMU, fCmdThreadSignaled, 4);
AssertCompileMemberAlignment(IOMMU, hEvtCmdThread, 8);
AssertCompileMemberAlignment(IOMMU, hMmio, 8);
AssertCompileMemberAlignment(IOMMU, IommuBar, 8);

/**
 * The ring-3 IOMMU device state.
 */
typedef struct IOMMUR3
{
    /** Device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPR3             pIommuHlpR3;
    /** The command thread handle. */
    R3PTRTYPE(PPDMTHREAD)       pCmdThread;
} IOMMUR3;
/** Pointer to the ring-3 IOMMU device state. */
typedef IOMMUR3 *PIOMMUR3;

/**
 * The ring-0 IOMMU device state.
 */
typedef struct IOMMUR0
{
    /** Device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPR0             pIommuHlpR0;
} IOMMUR0;
/** Pointer to the ring-0 IOMMU device state. */
typedef IOMMUR0 *PIOMMUR0;

/**
 * The raw-mode IOMMU device state.
 */
typedef struct IOMMURC
{
    /** Device instance. */
    PPDMDEVINSR0                pDevInsRC;
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPRC             pIommuHlpRC;
} IOMMURC;
/** Pointer to the raw-mode IOMMU device state. */
typedef IOMMURC *PIOMMURC;

/** The IOMMU device state for the current context. */
typedef CTX_SUFF(IOMMU)  IOMMUCC;
/** Pointer to the IOMMU device state for the current context. */
typedef CTX_SUFF(PIOMMU) PIOMMUCC;

/**
 * IOMMU register access.
 */
typedef struct IOMMUREGACC
{
    const char   *pszName;
    VBOXSTRICTRC (*pfnRead)(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value);
    VBOXSTRICTRC (*pfnWrite)(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t  u64Value);
    uint8_t      cb;
} IOMMUREGACC;
/** Pointer to an IOMMU register access. */
typedef IOMMUREGACC *PIOMMUREGACC;
/** Pointer to a const IOMMU register access. */
typedef IOMMUREGACC const *PCIOMMUREGACC;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * An array of the number of device table segments supported.
 * Indexed by u2DevTabSegSup.
 */
static uint8_t const g_acDevTabSegs[] = { 0, 2, 4, 8 };

/**
 * An array of the masks to select the device table segment index from a device ID.
 */
static uint16_t const g_auDevTabSegMasks[] = { 0x0, 0x8000, 0xc000, 0xe000 };

/**
 * An array of the shift values to select the device table segment index from a
 * device ID.
 */
static uint8_t const g_auDevTabSegShifts[] = { 0, 15, 14, 13 };

/**
 * The maximum size (inclusive) of each device table segment (0 to 7).
 * Indexed by the device table segment index.
 */
static uint16_t const g_auDevTabSegMaxSizes[] = { 0x1ff, 0xff, 0x7f, 0x7f, 0x3f, 0x3f, 0x3f, 0x3f };


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/**
 * Gets the maximum number of buffer entries for the given buffer length.
 *
 * @returns Number of buffer entries.
 * @param   uEncodedLen     The length (power-of-2 encoded).
 */
DECLINLINE(uint32_t) iommuAmdGetBufMaxEntries(uint8_t uEncodedLen)
{
    Assert(uEncodedLen > 7);
    return 2 << (uEncodedLen - 1);
}


/**
 * Gets the total length of the buffer given a base register's encoded length.
 *
 * @returns The length of the buffer in bytes.
 * @param   uEncodedLen     The length (power-of-2 encoded).
 */
DECLINLINE(uint32_t) iommuAmdGetTotalBufLength(uint8_t uEncodedLen)
{
    Assert(uEncodedLen > 7);
    return (2 << (uEncodedLen - 1)) << 4;
}


/**
 * Gets the number of (unconsumed) entries in the event log.
 *
 * @returns The number of entries in the event log.
 * @param   pThis     The IOMMU device state.
 */
static uint32_t iommuAmdGetEvtLogEntryCount(PIOMMU pThis)
{
    uint32_t const idxTail = pThis->EvtLogTailPtr.n.off >> IOMMU_EVT_GENERIC_SHIFT;
    uint32_t const idxHead = pThis->EvtLogHeadPtr.n.off >> IOMMU_EVT_GENERIC_SHIFT;
    if (idxTail >= idxHead)
        return idxTail - idxHead;

    uint32_t const cMaxEvts = iommuAmdGetBufMaxEntries(pThis->EvtLogBaseAddr.n.u4Len);
    return cMaxEvts - idxHead + idxTail;
}


/**
 * Gets the number of (unconsumed) commands in the command buffer.
 *
 * @returns The number of commands in the command buffer.
 * @param   pThis     The IOMMU device state.
 */
static uint32_t iommuAmdGetCmdBufEntryCount(PIOMMU pThis)
{
    uint32_t const idxTail = pThis->CmdBufTailPtr.n.off >> IOMMU_CMD_GENERIC_SHIFT;
    uint32_t const idxHead = pThis->CmdBufHeadPtr.n.off >> IOMMU_CMD_GENERIC_SHIFT;
    if (idxTail >= idxHead)
        return idxTail - idxHead;

    uint32_t const cMaxCmds = iommuAmdGetBufMaxEntries(pThis->CmdBufBaseAddr.n.u4Len);
    return cMaxCmds - idxHead + idxTail;
}


DECL_FORCE_INLINE(IOMMU_STATUS_T) iommuAmdGetStatus(PCIOMMU pThis)
{
    IOMMU_STATUS_T Status;
    Status.u64 = ASMAtomicReadU64((volatile uint64_t *)&pThis->Status.u64);
    return Status;
}


DECL_FORCE_INLINE(IOMMU_CTRL_T) iommuAmdGetCtrl(PCIOMMU pThis)
{
    IOMMU_CTRL_T Ctrl;
    Ctrl.u64 = ASMAtomicReadU64((volatile uint64_t *)&pThis->Ctrl.u64);
    return Ctrl;
}


/**
 * Returns whether MSI is enabled for the IOMMU.
 *
 * @returns Whether MSI is enabled.
 * @param   pDevIns     The IOMMU device instance.
 *
 * @note There should be a PCIDevXxx function for this.
 */
static bool iommuAmdIsMsiEnabled(PPDMDEVINS pDevIns)
{
    MSI_CAP_HDR_T MsiCapHdr;
    MsiCapHdr.u32 = PDMPciDevGetDWord(pDevIns->apPciDevs[0], IOMMU_PCI_OFF_MSI_CAP_HDR);
    return MsiCapHdr.n.u1MsiEnable;
}


/**
 * Signals a PCI target abort.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void iommuAmdSetPciTargetAbort(PPDMDEVINS pDevIns)
{
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    uint16_t const u16Status = PDMPciDevGetStatus(pPciDev) | VBOX_PCI_STATUS_SIG_TARGET_ABORT;
    PDMPciDevSetStatus(pPciDev, u16Status);
}


/**
 * Wakes up the command thread if there are commands to be processed or if
 * processing is requested to be stopped by software.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void iommuAmdCmdThreadWakeUpIfNeeded(PPDMDEVINS pDevIns)
{
    IOMMU_ASSERT_LOCKED(pDevIns);
    Log5Func(("\n"));

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (Status.n.u1CmdBufRunning)
    {
        Log5Func(("Signaling command thread\n"));
        PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEvtCmdThread);
    }
}


/**
 * Reads the Device Table Base Address Register.
 */
static VBOXSTRICTRC iommuAmdDevTabBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->aDevTabBaseAddrs[0].u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Command Buffer Base Address Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->CmdBufBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Event Log Base Address Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->EvtLogBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Control Register.
 */
static VBOXSTRICTRC iommuAmdCtrl_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->Ctrl.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Exclusion Range Base Address Register.
 */
static VBOXSTRICTRC iommuAmdExclRangeBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->ExclRangeBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the Exclusion Range Limit Register.
 */
static VBOXSTRICTRC iommuAmdExclRangeLimit_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->ExclRangeLimit.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the Extended Feature Register.
 */
static VBOXSTRICTRC iommuAmdExtFeat_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->ExtFeat.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the PPR Log Base Address Register.
 */
static VBOXSTRICTRC iommuAmdPprLogBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->PprLogBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Writes the Hardware Event Register (Hi).
 */
static VBOXSTRICTRC iommuAmdHwEvtHi_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->HwEvtHi.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Hardware Event Register (Lo).
 */
static VBOXSTRICTRC iommuAmdHwEvtLo_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->HwEvtLo;
    return VINF_SUCCESS;
}


/**
 * Reads the Hardware Event Status Register.
 */
static VBOXSTRICTRC iommuAmdHwEvtStatus_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->HwEvtStatus.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the GA Log Base Address Register.
 */
static VBOXSTRICTRC iommuAmdGALogBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->GALogBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the PPR Log B Base Address Register.
 */
static VBOXSTRICTRC iommuAmdPprLogBBaseAddr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->PprLogBBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads to the Event Log B Base Address Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogBBaseAddr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->EvtLogBBaseAddr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Device Table Segment Base Address Register.
 */
static VBOXSTRICTRC iommuAmdDevTabSegBar_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns);

    /* Figure out which segment is being written. */
    uint8_t const offSegment = (offReg - IOMMU_MMIO_OFF_DEV_TAB_SEG_FIRST) >> 3;
    uint8_t const idxSegment = offSegment + 1;
    Assert(idxSegment < RT_ELEMENTS(pThis->aDevTabBaseAddrs));

    *pu64Value = pThis->aDevTabBaseAddrs[idxSegment].u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Device Specific Feature Extension (DSFX) Register.
 */
static VBOXSTRICTRC iommuAmdDevSpecificFeat_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->DevSpecificFeat.u64;
    return VINF_SUCCESS;
}

/**
 * Reads the Device Specific Control Extension (DSCX) Register.
 */
static VBOXSTRICTRC iommuAmdDevSpecificCtrl_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->DevSpecificCtrl.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Device Specific Status Extension (DSSX) Register.
 */
static VBOXSTRICTRC iommuAmdDevSpecificStatus_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->DevSpecificStatus.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the MSI Vector Register 0 (32-bit) or the MSI Vector Register 1 (32-bit).
 */
static VBOXSTRICTRC iommuAmdDevMsiVector_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    if (offReg == IOMMU_MMIO_OFF_MSI_VECTOR_0)
        *pu64Value = pThis->MiscInfo.au32[0];
    else
    {
        AssertMsg(offReg == IOMMU_MMIO_OFF_MSI_VECTOR_1, ("%#x\n", offReg));
        *pu64Value = pThis->MiscInfo.au32[1];
    }
    return VINF_SUCCESS;
}


#ifdef IOMMU_NEW_REGISTER_ACCESS
/**
 * Reads the MSI Capability Header Register (32-bit) or the MSI Address (Lo)
 * Register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiCapHdrOrAddrLo_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pThis);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    if (offReg == IOMMU_MMIO_OFF_MSI_CAP_HDR)
        *pu64Value = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR);
    else
    {
        AssertMsg(offReg == IOMMU_MMIO_OFF_MSI_ADDR_LO, ("%#x\n", offReg));
        *pu64Value = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO);
    }
    return VINF_SUCCESS;
}


/**
 * Reads the MSI Address (Hi) Register (32-bit) or the MSI data register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiAddrHiOrData_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pThis);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    if (offReg == IOMMU_MMIO_OFF_MSI_ADDR_HI)
        *pu64Value = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI);
    else
    {
        AssertMsg(offReg == IOMMU_MMIO_OFF_MSI_DATA, ("%#x\n", offReg));
        *pu64Value = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA);
    }
    return VINF_SUCCESS;
}
#endif

/**
 * Reads the Command Buffer Head Pointer Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufHeadPtr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->CmdBufHeadPtr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Command Buffer Tail Pointer Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufTailPtr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->CmdBufTailPtr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Event Log Head Pointer Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogHeadPtr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->EvtLogHeadPtr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Event Log Tail Pointer Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogTailPtr_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->EvtLogTailPtr.u64;
    return VINF_SUCCESS;
}


/**
 * Reads the Status Register.
 */
static VBOXSTRICTRC iommuAmdStatus_r(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t *pu64Value)
{
    RT_NOREF(pDevIns, offReg);
    *pu64Value = pThis->Status.u64;
    return VINF_SUCCESS;
}

#ifndef IOMMU_NEW_REGISTER_ACCESS
static VBOXSTRICTRC iommuAmdIgnore_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, pThis, offReg, u64Value);
    return VINF_SUCCESS;
}
#endif


/**
 * Writes the Device Table Base Address Register.
 */
static VBOXSTRICTRC iommuAmdDevTabBar_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_DEV_TAB_BAR_VALID_MASK;

    /* Update the register. */
    pThis->aDevTabBaseAddrs[0].u64 = u64Value;
    return VINF_SUCCESS;
}


/**
 * Writes the Command Buffer Base Address Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufBar_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /*
     * While this is not explicitly specified like the event log base address register,
     * the AMD spec. does specify "CmdBufRun must be 0b to modify the command buffer registers properly".
     * Inconsistent specs :/
     */
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (Status.n.u1CmdBufRunning)
    {
        LogFunc(("Setting CmdBufBar (%#RX64) when command buffer is running -> Ignored\n", u64Value));
        return VINF_SUCCESS;
    }

    /* Mask out all unrecognized bits. */
    CMD_BUF_BAR_T CmdBufBaseAddr;
    CmdBufBaseAddr.u64 = u64Value & IOMMU_CMD_BUF_BAR_VALID_MASK;

    /* Validate the length. */
    if (CmdBufBaseAddr.n.u4Len >= 8)
    {
        /* Update the register. */
        pThis->CmdBufBaseAddr.u64 = CmdBufBaseAddr.u64;

        /*
         * Writing the command buffer base address, clears the command buffer head and tail pointers.
         * See AMD spec. 2.4 "Commands".
         */
        pThis->CmdBufHeadPtr.u64 = 0;
        pThis->CmdBufTailPtr.u64 = 0;
    }
    else
        LogFunc(("Command buffer length (%#x) invalid -> Ignored\n", CmdBufBaseAddr.n.u4Len));

    return VINF_SUCCESS;
}


/**
 * Writes the Event Log Base Address Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogBar_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /*
     * IOMMU behavior is undefined when software writes this register when event logging is running.
     * In our emulation, we ignore the write entirely.
     * See AMD IOMMU spec. "Event Log Base Address Register".
     */
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (Status.n.u1EvtLogRunning)
    {
        LogFunc(("Setting EvtLogBar (%#RX64) when event logging is running -> Ignored\n", u64Value));
        return VINF_SUCCESS;
    }

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_EVT_LOG_BAR_VALID_MASK;
    EVT_LOG_BAR_T EvtLogBaseAddr;
    EvtLogBaseAddr.u64 = u64Value;

    /* Validate the length. */
    if (EvtLogBaseAddr.n.u4Len >= 8)
    {
        /* Update the register. */
        pThis->EvtLogBaseAddr.u64 = EvtLogBaseAddr.u64;

        /*
         * Writing the event log base address, clears the event log head and tail pointers.
         * See AMD spec. 2.5 "Event Logging".
         */
        pThis->EvtLogHeadPtr.u64 = 0;
        pThis->EvtLogTailPtr.u64 = 0;
    }
    else
        LogFunc(("Event log length (%#x) invalid -> Ignored\n", EvtLogBaseAddr.n.u4Len));

    return VINF_SUCCESS;
}


/**
 * Writes the Control Register.
 */
static VBOXSTRICTRC iommuAmdCtrl_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_CTRL_VALID_MASK;
    IOMMU_CTRL_T NewCtrl;
    NewCtrl.u64 = u64Value;

    /* Ensure the device table segments are within limits. */
    if (NewCtrl.n.u3DevTabSegEn <= pThis->ExtFeat.n.u2DevTabSegSup)
    {
        IOMMU_CTRL_T const OldCtrl = iommuAmdGetCtrl(pThis);

        /* Update the register. */
        ASMAtomicWriteU64(&pThis->Ctrl.u64, NewCtrl.u64);

        bool const fNewIommuEn = NewCtrl.n.u1IommuEn;
        bool const fOldIommuEn = OldCtrl.n.u1IommuEn;

        /* Enable or disable event logging when the bit transitions. */
        bool const fOldEvtLogEn = OldCtrl.n.u1EvtLogEn;
        bool const fNewEvtLogEn = NewCtrl.n.u1EvtLogEn;
        if (   fOldEvtLogEn != fNewEvtLogEn
            || fOldIommuEn != fNewIommuEn)
        {
            if (   fNewIommuEn
                && fNewEvtLogEn)
            {
                ASMAtomicAndU64(&pThis->Status.u64, ~IOMMU_STATUS_EVT_LOG_OVERFLOW);
                ASMAtomicOrU64(&pThis->Status.u64, IOMMU_STATUS_EVT_LOG_RUNNING);
            }
            else
                ASMAtomicAndU64(&pThis->Status.u64, ~IOMMU_STATUS_EVT_LOG_RUNNING);
        }

        /* Enable or disable command buffer processing when the bit transitions. */
        bool const fOldCmdBufEn = OldCtrl.n.u1CmdBufEn;
        bool const fNewCmdBufEn = NewCtrl.n.u1CmdBufEn;
        if (   fOldCmdBufEn != fNewCmdBufEn
            || fOldIommuEn != fNewIommuEn)
        {
            if (   fNewCmdBufEn
                && fNewIommuEn)
            {
                ASMAtomicOrU64(&pThis->Status.u64, IOMMU_STATUS_CMD_BUF_RUNNING);
                LogFunc(("Command buffer enabled\n"));

                /* Wake up the command thread to start processing commands. */
                iommuAmdCmdThreadWakeUpIfNeeded(pDevIns);
            }
            else
            {
                ASMAtomicAndU64(&pThis->Status.u64, ~IOMMU_STATUS_CMD_BUF_RUNNING);
                LogFunc(("Command buffer disabled\n"));
            }
        }
    }
    else
    {
        LogFunc(("Invalid number of device table segments enabled, exceeds %#x (%#RX64) -> Ignored!\n",
                 pThis->ExtFeat.n.u2DevTabSegSup, NewCtrl.u64));
    }

    return VINF_SUCCESS;
}


/**
 * Writes to the Exclusion Range Base Address Register.
 */
static VBOXSTRICTRC iommuAmdExclRangeBar_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);
    pThis->ExclRangeBaseAddr.u64 = u64Value & IOMMU_EXCL_RANGE_BAR_VALID_MASK;
    return VINF_SUCCESS;
}


/**
 * Writes to the Exclusion Range Limit Register.
 */
static VBOXSTRICTRC iommuAmdExclRangeLimit_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);
    u64Value &= IOMMU_EXCL_RANGE_LIMIT_VALID_MASK;
    u64Value |= UINT64_C(0xfff);
    pThis->ExclRangeLimit.u64 = u64Value;
    return VINF_SUCCESS;
}


/**
 * Writes the Hardware Event Register (Hi).
 */
static VBOXSTRICTRC iommuAmdHwEvtHi_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    /** @todo IOMMU: Why the heck is this marked read/write by the AMD IOMMU spec? */
    RT_NOREF(pDevIns, offReg);
    LogFlowFunc(("Writing %#RX64 to hardware event (Hi) register!\n", u64Value));
    pThis->HwEvtHi.u64 = u64Value;
    return VINF_SUCCESS;
}


/**
 * Writes the Hardware Event Register (Lo).
 */
static VBOXSTRICTRC iommuAmdHwEvtLo_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    /** @todo IOMMU: Why the heck is this marked read/write by the AMD IOMMU spec? */
    RT_NOREF(pDevIns, offReg);
    LogFlowFunc(("Writing %#RX64 to hardware event (Lo) register!\n", u64Value));
    pThis->HwEvtLo = u64Value;
    return VINF_SUCCESS;
}


/**
 * Writes the Hardware Event Status Register.
 */
static VBOXSTRICTRC iommuAmdHwEvtStatus_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_HW_EVT_STATUS_VALID_MASK;

    /*
     * The two bits (HEO and HEV) are RW1C (Read/Write 1-to-Clear; writing 0 has no effect).
     * If the current status bits or the bits being written are both 0, we've nothing to do.
     * The Overflow bit (bit 1) is only valid when the Valid bit (bit 0) is 1.
     */
    uint64_t HwStatus = pThis->HwEvtStatus.u64;
    if (!(HwStatus & RT_BIT(0)))
        return VINF_SUCCESS;
    if (u64Value & HwStatus & RT_BIT_64(0))
        HwStatus &= ~RT_BIT_64(0);
    if (u64Value & HwStatus & RT_BIT_64(1))
        HwStatus &= ~RT_BIT_64(1);

    /* Update the register. */
    pThis->HwEvtStatus.u64 = HwStatus;
    return VINF_SUCCESS;
}


/**
 * Writes the Device Table Segment Base Address Register.
 */
static VBOXSTRICTRC iommuAmdDevTabSegBar_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns);

    /* Figure out which segment is being written. */
    uint8_t const offSegment = (offReg - IOMMU_MMIO_OFF_DEV_TAB_SEG_FIRST) >> 3;
    uint8_t const idxSegment = offSegment + 1;
    Assert(idxSegment < RT_ELEMENTS(pThis->aDevTabBaseAddrs));

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_DEV_TAB_SEG_BAR_VALID_MASK;
    DEV_TAB_BAR_T DevTabSegBar;
    DevTabSegBar.u64 = u64Value;

    /* Validate the size. */
    uint16_t const uSegSize    = DevTabSegBar.n.u9Size;
    uint16_t const uMaxSegSize = g_auDevTabSegMaxSizes[idxSegment];
    if (uSegSize <= uMaxSegSize)
    {
        /* Update the register. */
        pThis->aDevTabBaseAddrs[idxSegment].u64 = u64Value;
    }
    else
        LogFunc(("Device table segment (%u) size invalid (%#RX32) -> Ignored\n", idxSegment, uSegSize));

    return VINF_SUCCESS;
}


#ifndef IOMMU_NEW_REGISTER_ACCESS
/**
 * Writes the MSI Capability Header Register.
 */
static VBOXSTRICTRC iommuAmdMsiCapHdr_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis, offReg);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    MSI_CAP_HDR_T MsiCapHdr;
    MsiCapHdr.u32 = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR);
    MsiCapHdr.n.u1MsiEnable = RT_BOOL(u64Value & IOMMU_MSI_CAP_HDR_MSI_EN_MASK);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR, MsiCapHdr.u32);
    return VINF_SUCCESS;
}


/**
 * Writes the MSI Address (Lo) Register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiAddrLo_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis, offReg);
    Assert(!RT_HI_U32(u64Value));
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO, u64Value & VBOX_MSI_ADDR_VALID_MASK);
    return VINF_SUCCESS;
}


/**
 * Writes the MSI Address (Hi) Register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiAddrHi_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis, offReg);
    Assert(!RT_HI_U32(u64Value));
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI, u64Value);
    return VINF_SUCCESS;
}


/**
 * Writes the MSI Data Register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiData_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis, offReg);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA, u64Value & VBOX_MSI_DATA_VALID_MASK);
    return VINF_SUCCESS;
}
#else
/**
 * Writes the MSI Capability Header Register (32-bit) or the MSI Address (Lo)
 * Register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiCapHdrOrAddrLo_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    if (offReg == IOMMU_MMIO_OFF_MSI_CAP_HDR)
    {
        /* MsiMultMessEn not supported, so only MsiEn is the writable bit. */
        MSI_CAP_HDR_T MsiCapHdr;
        MsiCapHdr.u32           = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR);
        MsiCapHdr.n.u1MsiEnable = RT_BOOL(u64Value & IOMMU_MSI_CAP_HDR_MSI_EN_MASK);
        PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR, MsiCapHdr.u32);
    }
    else
    {
        AssertMsg(offReg == IOMMU_MMIO_OFF_MSI_ADDR_LO, ("%#x\n", offReg));
        uint32_t const uMsiAddrLo = u64Value & VBOX_MSI_ADDR_VALID_MASK;
        PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO, uMsiAddrLo);
    }
    return VINF_SUCCESS;
}


/**
 * Writes the MSI Address (Hi) Register (32-bit) or the MSI data register (32-bit).
 */
static VBOXSTRICTRC iommuAmdMsiAddrHiOrData_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pThis);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);
    if (offReg == IOMMU_MMIO_OFF_MSI_ADDR_HI)
        PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI, u64Value);
    else
    {
        AssertMsg(offReg == IOMMU_MMIO_OFF_MSI_DATA, ("%#x\n", offReg));
        uint32_t const uMsiData = u64Value & VBOX_MSI_DATA_VALID_MASK;
        PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA, uMsiData);
    }
    return VINF_SUCCESS;
}
#endif


/**
 * Writes the Command Buffer Head Pointer Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufHeadPtr_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /*
     * IOMMU behavior is undefined when software writes this register when the command buffer is running.
     * In our emulation, we ignore the write entirely.
     * See AMD IOMMU spec. 3.3.13 "Command and Event Log Pointer Registers".
     */
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (Status.n.u1CmdBufRunning)
    {
        LogFunc(("Setting CmdBufHeadPtr (%#RX64) when command buffer is running -> Ignored\n", u64Value));
        return VINF_SUCCESS;
    }

    /*
     * IOMMU behavior is undefined when software writes a value outside the buffer length.
     * In our emulation, we ignore the write entirely.
     */
    uint32_t const offBuf = u64Value & IOMMU_CMD_BUF_HEAD_PTR_VALID_MASK;
    uint32_t const cbBuf  = iommuAmdGetTotalBufLength(pThis->CmdBufBaseAddr.n.u4Len);
    Assert(cbBuf <= _512K);
    if (offBuf >= cbBuf)
    {
        LogFunc(("Setting CmdBufHeadPtr (%#RX32) to a value that exceeds buffer length (%#RX23) -> Ignored\n", offBuf, cbBuf));
        return VINF_SUCCESS;
    }

    /* Update the register. */
    pThis->CmdBufHeadPtr.au32[0] = offBuf;

    iommuAmdCmdThreadWakeUpIfNeeded(pDevIns);

    Log5Func(("Set CmdBufHeadPtr to %#RX32\n", offBuf));
    return VINF_SUCCESS;
}


/**
 * Writes the Command Buffer Tail Pointer Register.
 */
static VBOXSTRICTRC iommuAmdCmdBufTailPtr_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /*
     * IOMMU behavior is undefined when software writes a value outside the buffer length.
     * In our emulation, we ignore the write entirely.
     * See AMD IOMMU spec. 3.3.13 "Command and Event Log Pointer Registers".
     */
    uint32_t const offBuf = u64Value & IOMMU_CMD_BUF_TAIL_PTR_VALID_MASK;
    uint32_t const cbBuf  = iommuAmdGetTotalBufLength(pThis->CmdBufBaseAddr.n.u4Len);
    Assert(cbBuf <= _512K);
    if (offBuf >= cbBuf)
    {
        LogFunc(("Setting CmdBufTailPtr (%#RX32) to a value that exceeds buffer length (%#RX32) -> Ignored\n", offBuf, cbBuf));
        return VINF_SUCCESS;
    }

    /*
     * IOMMU behavior is undefined if software advances the tail pointer equal to or beyond the
     * head pointer after adding one or more commands to the buffer.
     *
     * However, we cannot enforce this strictly because it's legal for software to shrink the
     * command queue (by reducing the offset) as well as wrap around the pointer (when head isn't
     * at 0). Software might even make the queue empty by making head and tail equal which is
     * allowed. I don't think we can or should try too hard to prevent software shooting itself
     * in the foot here. As long as we make sure the offset value is within the circular buffer
     * bounds (which we do by masking bits above) it should be sufficient.
     */
    pThis->CmdBufTailPtr.au32[0] = offBuf;

    iommuAmdCmdThreadWakeUpIfNeeded(pDevIns);

    Log5Func(("Set CmdBufTailPtr to %#RX32\n", offBuf));
    return VINF_SUCCESS;
}


/**
 * Writes the Event Log Head Pointer Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogHeadPtr_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /*
     * IOMMU behavior is undefined when software writes a value outside the buffer length.
     * In our emulation, we ignore the write entirely.
     * See AMD IOMMU spec. 3.3.13 "Command and Event Log Pointer Registers".
     */
    uint32_t const offBuf = u64Value & IOMMU_EVT_LOG_HEAD_PTR_VALID_MASK;
    uint32_t const cbBuf  = iommuAmdGetTotalBufLength(pThis->EvtLogBaseAddr.n.u4Len);
    Assert(cbBuf <= _512K);
    if (offBuf >= cbBuf)
    {
        LogFunc(("Setting EvtLogHeadPtr (%#RX32) to a value that exceeds buffer length (%#RX32) -> Ignored\n", offBuf, cbBuf));
        return VINF_SUCCESS;
    }

    /* Update the register. */
    pThis->EvtLogHeadPtr.au32[0] = offBuf;

    LogFlowFunc(("Set EvtLogHeadPtr to %#RX32\n", offBuf));
    return VINF_SUCCESS;
}


/**
 * Writes the Event Log Tail Pointer Register.
 */
static VBOXSTRICTRC iommuAmdEvtLogTailPtr_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);
    NOREF(pThis);

    /*
     * IOMMU behavior is undefined when software writes this register when the event log is running.
     * In our emulation, we ignore the write entirely.
     * See AMD IOMMU spec. 3.3.13 "Command and Event Log Pointer Registers".
     */
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (Status.n.u1EvtLogRunning)
    {
        LogFunc(("Setting EvtLogTailPtr (%#RX64) when event log is running -> Ignored\n", u64Value));
        return VINF_SUCCESS;
    }

    /*
     * IOMMU behavior is undefined when software writes a value outside the buffer length.
     * In our emulation, we ignore the write entirely.
     */
    uint32_t const offBuf = u64Value & IOMMU_EVT_LOG_TAIL_PTR_VALID_MASK;
    uint32_t const cbBuf  = iommuAmdGetTotalBufLength(pThis->EvtLogBaseAddr.n.u4Len);
    Assert(cbBuf <= _512K);
    if (offBuf >= cbBuf)
    {
        LogFunc(("Setting EvtLogTailPtr (%#RX32) to a value that exceeds buffer length (%#RX32) -> Ignored\n", offBuf, cbBuf));
        return VINF_SUCCESS;
    }

    /* Update the register. */
    pThis->EvtLogTailPtr.au32[0] = offBuf;

    LogFlowFunc(("Set EvtLogTailPtr to %#RX32\n", offBuf));
    return VINF_SUCCESS;
}


/**
 * Writes the Status Register.
 */
static VBOXSTRICTRC iommuAmdStatus_w(PPDMDEVINS pDevIns, PIOMMU pThis, uint32_t offReg, uint64_t u64Value)
{
    RT_NOREF(pDevIns, offReg);

    /* Mask out all unrecognized bits. */
    u64Value &= IOMMU_STATUS_VALID_MASK;

    /*
     * Compute RW1C (read-only, write-1-to-clear) bits and preserve the rest (which are read-only).
     * Writing 0 to an RW1C bit has no effect. Writing 1 to an RW1C bit, clears the bit if it's already 1.
     */
    IOMMU_STATUS_T const OldStatus = iommuAmdGetStatus(pThis);
    uint64_t const fOldRw1cBits = (OldStatus.u64 &  IOMMU_STATUS_RW1C_MASK);
    uint64_t const fOldRoBits   = (OldStatus.u64 & ~IOMMU_STATUS_RW1C_MASK);
    uint64_t const fNewRw1cBits = (u64Value      &  IOMMU_STATUS_RW1C_MASK);

    uint64_t const uNewStatus = (fOldRw1cBits & ~fNewRw1cBits) | fOldRoBits;

    /* Update the register. */
    ASMAtomicWriteU64(&pThis->Status.u64, uNewStatus);
    return VINF_SUCCESS;
}

#ifdef IOMMU_NEW_REGISTER_ACCESS
/**
 * Register access table 0.
 * The MMIO offset of each entry must be a multiple of 8!
 */
static const IOMMUREGACC g_aRegAccess0[] =
{
    /* MMIO off.   Register name                           Read function                Write function               Reg. size */
    { /* 0x00  */  "DEV_TAB_BAR",                          iommuAmdDevTabBar_r,         iommuAmdDevTabBar_w,         8 },
    { /* 0x08  */  "CMD_BUF_BAR",                          iommuAmdCmdBufBar_r,         iommuAmdCmdBufBar_w,         8 },
    { /* 0x10  */  "EVT_LOG_BAR",                          iommuAmdEvtLogBar_r,         iommuAmdEvtLogBar_w,         8 },
    { /* 0x18  */  "CTRL",                                 iommuAmdCtrl_r,              iommuAmdCtrl_w,              8 },
    { /* 0x20  */  "EXCL_BAR",                             iommuAmdExclRangeBar_r,      iommuAmdExclRangeBar_w,      8 },
    { /* 0x28  */  "EXCL_RANGE_LIMIT",                     iommuAmdExclRangeLimit_r,    iommuAmdExclRangeLimit_w,    8 },
    { /* 0x30  */  "EXT_FEAT",                             iommuAmdExtFeat_r,           NULL,                        8 },
    { /* 0x38  */  "PPR_LOG_BAR",                          iommuAmdPprLogBar_r,         NULL,                        8 },
    { /* 0x40  */  "HW_EVT_HI",                            iommuAmdHwEvtHi_r,           iommuAmdHwEvtHi_w,           8 },
    { /* 0x48  */  "HW_EVT_LO",                            iommuAmdHwEvtLo_r,           iommuAmdHwEvtLo_w,           8 },
    { /* 0x50  */  "HW_EVT_STATUS",                        iommuAmdHwEvtStatus_r,       iommuAmdHwEvtStatus_w,       8 },
    { /* 0x58  */  NULL,                                   NULL,                        NULL,                        0 },

    { /* 0x60  */  "SMI_FLT_0",                            NULL,                        NULL,                        8 },
    { /* 0x68  */  "SMI_FLT_1",                            NULL,                        NULL,                        8 },
    { /* 0x70  */  "SMI_FLT_2",                            NULL,                        NULL,                        8 },
    { /* 0x78  */  "SMI_FLT_3",                            NULL,                        NULL,                        8 },
    { /* 0x80  */  "SMI_FLT_4",                            NULL,                        NULL,                        8 },
    { /* 0x88  */  "SMI_FLT_5",                            NULL,                        NULL,                        8 },
    { /* 0x90  */  "SMI_FLT_6",                            NULL,                        NULL,                        8 },
    { /* 0x98  */  "SMI_FLT_7",                            NULL,                        NULL,                        8 },
    { /* 0xa0  */  "SMI_FLT_8",                            NULL,                        NULL,                        8 },
    { /* 0xa8  */  "SMI_FLT_9",                            NULL,                        NULL,                        8 },
    { /* 0xb0  */  "SMI_FLT_10",                           NULL,                        NULL,                        8 },
    { /* 0xb8  */  "SMI_FLT_11",                           NULL,                        NULL,                        8 },
    { /* 0xc0  */  "SMI_FLT_12",                           NULL,                        NULL,                        8 },
    { /* 0xc8  */  "SMI_FLT_13",                           NULL,                        NULL,                        8 },
    { /* 0xd0  */  "SMI_FLT_14",                           NULL,                        NULL,                        8 },
    { /* 0xd8  */  "SMI_FLT_15",                           NULL,                        NULL,                        8 },

    { /* 0xe0  */  "GALOG_BAR",                            iommuAmdGALogBar_r,          NULL,                        8 },
    { /* 0xe8  */  "GALOG_TAIL_ADDR",                      NULL,                        NULL,                        8 },
    { /* 0xf0  */  "PPR_LOG_B_BAR",                        iommuAmdPprLogBBaseAddr_r,   NULL,                        8 },
    { /* 0xf8  */  "PPR_EVT_B_BAR",                        iommuAmdEvtLogBBaseAddr_r,   NULL,                        8 },

    { /* 0x100 */  "DEV_TAB_SEG_1",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x108 */  "DEV_TAB_SEG_2",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x110 */  "DEV_TAB_SEG_3",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x118 */  "DEV_TAB_SEG_4",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x120 */  "DEV_TAB_SEG_5",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x128 */  "DEV_TAB_SEG_6",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },
    { /* 0x130 */  "DEV_TAB_SEG_7",                        iommuAmdDevTabSegBar_r,      iommuAmdDevTabSegBar_w,      8 },

    { /* 0x138 */  "DEV_SPECIFIC_FEAT",                    iommuAmdDevSpecificFeat_r,   NULL,                        8 },
    { /* 0x140 */  "DEV_SPECIFIC_CTRL",                    iommuAmdDevSpecificCtrl_r,   NULL,                        8 },
    { /* 0x148 */  "DEV_SPECIFIC_STATUS",                  iommuAmdDevSpecificStatus_r, NULL,                        8 },

    { /* 0x150 */  "MSI_VECTOR_0 or MSI_VECTOR_1",         iommuAmdDevMsiVector_r,      NULL,                        4 },
    { /* 0x158 */  "MSI_CAP_HDR or MSI_ADDR_LO",           iommuAmdMsiCapHdrOrAddrLo_r, iommuAmdMsiCapHdrOrAddrLo_w, 4 },
    { /* 0x160 */  "MSI_ADDR_HI or MSI_DATA",              iommuAmdMsiAddrHiOrData_r,   iommuAmdMsiAddrHiOrData_w,   4 },
    { /* 0x168 */  "MSI_MAPPING_CAP_HDR or PERF_OPT_CTRL", NULL,                        NULL,                        4 },

    { /* 0x170 */  "XT_GEN_INTR_CTRL",                     NULL,                        NULL,                        8 },
    { /* 0x178 */  "XT_PPR_INTR_CTRL",                     NULL,                        NULL,                        8 },
    { /* 0x180 */  "XT_GALOG_INT_CTRL",                    NULL,                        NULL,                        8 },
};
AssertCompile(RT_ELEMENTS(g_aRegAccess0) == (IOMMU_MMIO_OFF_QWORD_TABLE_0_END - IOMMU_MMIO_OFF_QWORD_TABLE_0_START) / 8);

/**
 * Register access table 1.
 * The MMIO offset of each entry must be a multiple of 8!
 */
static const IOMMUREGACC g_aRegAccess1[] =
{
    /* MMIO offset    Register name      Read function    Write function    Register size. */
    { /* 0x200 */  "MARC_APER_BAR_0",    NULL,            NULL,             8 },
    { /* 0x208 */  "MARC_APER_RELOC_0",  NULL,            NULL,             8 },
    { /* 0x210 */  "MARC_APER_LEN_0",    NULL,            NULL,             8 },
    { /* 0x218 */  "MARC_APER_BAR_1",    NULL,            NULL,             8 },
    { /* 0x220 */  "MARC_APER_RELOC_1",  NULL,            NULL,             8 },
    { /* 0x228 */  "MARC_APER_LEN_1",    NULL,            NULL,             8 },
    { /* 0x230 */  "MARC_APER_BAR_2",    NULL,            NULL,             8 },
    { /* 0x238 */  "MARC_APER_RELOC_2",  NULL,            NULL,             8 },
    { /* 0x240 */  "MARC_APER_LEN_2",    NULL,            NULL,             8 },
    { /* 0x248 */  "MARC_APER_BAR_3",    NULL,            NULL,             8 },
    { /* 0x250 */  "MARC_APER_RELOC_3",  NULL,            NULL,             8 },
    { /* 0x258 */  "MARC_APER_LEN_3",    NULL,            NULL,             8 }
};
AssertCompile(RT_ELEMENTS(g_aRegAccess1) == (IOMMU_MMIO_OFF_QWORD_TABLE_1_END - IOMMU_MMIO_OFF_QWORD_TABLE_1_START) / 8);

/**
 * Register access table 2.
 * The MMIO offset of each entry must be a multiple of 8!
 */
static const IOMMUREGACC g_aRegAccess2[] =
{
    /* MMIO offset    Register name               Read Function             Write function           Register size (bytes) */
    { /* 0x1ff8 */    "RSVD_REG",                 NULL,                     NULL,                    8 },

    { /* 0x2000 */    "CMD_BUF_HEAD_PTR",         iommuAmdCmdBufHeadPtr_r,  iommuAmdCmdBufHeadPtr_w, 8 },
    { /* 0x2008 */    "CMD_BUF_TAIL_PTR",         iommuAmdCmdBufTailPtr_r , iommuAmdCmdBufTailPtr_w, 8 },
    { /* 0x2010 */    "EVT_LOG_HEAD_PTR",         iommuAmdEvtLogHeadPtr_r,  iommuAmdEvtLogHeadPtr_w, 8 },
    { /* 0x2018 */    "EVT_LOG_TAIL_PTR",         iommuAmdEvtLogTailPtr_r,  iommuAmdEvtLogTailPtr_w, 8 },

    { /* 0x2020 */    "STATUS",                   iommuAmdStatus_r,         iommuAmdStatus_w,        8 },
    { /* 0x2028 */    NULL,                       NULL,                     NULL,                    0 },

    { /* 0x2030 */    "PPR_LOG_HEAD_PTR",         NULL,                     NULL,                    8 },
    { /* 0x2038 */    "PPR_LOG_TAIL_PTR",         NULL,                     NULL,                    8 },

    { /* 0x2040 */    "GALOG_HEAD_PTR",           NULL,                     NULL,                    8 },
    { /* 0x2048 */    "GALOG_TAIL_PTR",           NULL,                     NULL,                    8 },

    { /* 0x2050 */    "PPR_LOG_B_HEAD_PTR",       NULL,                     NULL,                    8 },
    { /* 0x2058 */    "PPR_LOG_B_TAIL_PTR",       NULL,                     NULL,                    8 },

    { /* 0x2060 */    NULL,                       NULL,                     NULL,                    0 },
    { /* 0x2068 */    NULL,                       NULL,                     NULL,                    0 },

    { /* 0x2070 */    "EVT_LOG_B_HEAD_PTR",       NULL,                     NULL,                    8 },
    { /* 0x2078 */    "EVT_LOG_B_TAIL_PTR",       NULL,                     NULL,                    8 },

    { /* 0x2080 */    "PPR_LOG_AUTO_RESP",        NULL,                     NULL,                    8 },
    { /* 0x2088 */    "PPR_LOG_OVERFLOW_EARLY",   NULL,                     NULL,                    8 },
    { /* 0x2090 */    "PPR_LOG_B_OVERFLOW_EARLY", NULL,                     NULL,                    8 }
};
AssertCompile(RT_ELEMENTS(g_aRegAccess2) == (IOMMU_MMIO_OFF_QWORD_TABLE_2_END - IOMMU_MMIO_OFF_QWORD_TABLE_2_START) / 8);
#endif


/**
 * Writes an IOMMU register (32-bit and 64-bit).
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   off         MMIO byte offset to the register.
 * @param   cb          The size of the write access.
 * @param   uValue      The value being written.
 *
 * @thread  EMT.
 */
static VBOXSTRICTRC iommuAmdWriteRegister(PPDMDEVINS pDevIns, uint32_t off, uint8_t cb, uint64_t uValue)
{
    /*
     * Validate the access in case of IOM bug or incorrect assumption.
     */
    Assert(off < IOMMU_MMIO_REGION_SIZE);
    AssertMsgReturn(cb == 4 || cb == 8, ("Invalid access size %u\n", cb), VINF_SUCCESS);
    AssertMsgReturn(!(off & 3), ("Invalid offset %#x\n", off), VINF_SUCCESS);

    Log5Func(("off=%#x cb=%u uValue=%#RX64\n", off, cb, uValue));

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
#ifndef IOMMU_NEW_REGISTER_ACCESS
    switch (off)
    {
        case IOMMU_MMIO_OFF_DEV_TAB_BAR:         return iommuAmdDevTabBar_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_CMD_BUF_BAR:         return iommuAmdCmdBufBar_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EVT_LOG_BAR:         return iommuAmdEvtLogBar_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_CTRL:                return iommuAmdCtrl_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EXCL_BAR:            return iommuAmdExclRangeBar_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EXCL_RANGE_LIMIT:    return iommuAmdExclRangeLimit_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EXT_FEAT:            return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_PPR_LOG_BAR:         return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_HW_EVT_HI:           return iommuAmdHwEvtHi_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_HW_EVT_LO:           return iommuAmdHwEvtLo_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_HW_EVT_STATUS:       return iommuAmdHwEvtStatus_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_GALOG_BAR:
        case IOMMU_MMIO_OFF_GALOG_TAIL_ADDR:     return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_PPR_LOG_B_BAR:
        case IOMMU_MMIO_OFF_PPR_EVT_B_BAR:       return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_DEV_TAB_SEG_1:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_2:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_3:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_4:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_5:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_6:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_7:       return iommuAmdDevTabSegBar_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_DEV_SPECIFIC_FEAT:
        case IOMMU_MMIO_OFF_DEV_SPECIFIC_CTRL:
        case IOMMU_MMIO_OFF_DEV_SPECIFIC_STATUS: return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_MSI_VECTOR_0:
        case IOMMU_MMIO_OFF_MSI_VECTOR_1:        return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_MSI_CAP_HDR:
        {
            VBOXSTRICTRC rcStrict = iommuAmdMsiCapHdr_w(pDevIns, pThis, off, (uint32_t)uValue);
            if (cb == 4 || RT_FAILURE(rcStrict))
                return rcStrict;
            uValue >>= 32;
            RT_FALL_THRU();
        }
        case IOMMU_MMIO_OFF_MSI_ADDR_LO:         return iommuAmdMsiAddrLo_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_MSI_ADDR_HI:
        {
            VBOXSTRICTRC rcStrict = iommuAmdMsiAddrHi_w(pDevIns, pThis, off, (uint32_t)uValue);
            if (cb == 4 || RT_FAILURE(rcStrict))
                return rcStrict;
            uValue >>= 32;
            RT_FALL_THRU();
        }
        case IOMMU_MMIO_OFF_MSI_DATA:            return iommuAmdMsiData_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_MSI_MAPPING_CAP_HDR: return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_PERF_OPT_CTRL:       return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_XT_GEN_INTR_CTRL:
        case IOMMU_MMIO_OFF_XT_PPR_INTR_CTRL:
        case IOMMU_MMIO_OFF_XT_GALOG_INT_CTRL:   return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_MARC_APER_BAR_0:
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_0:
        case IOMMU_MMIO_OFF_MARC_APER_LEN_0:
        case IOMMU_MMIO_OFF_MARC_APER_BAR_1:
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_1:
        case IOMMU_MMIO_OFF_MARC_APER_LEN_1:
        case IOMMU_MMIO_OFF_MARC_APER_BAR_2:
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_2:
        case IOMMU_MMIO_OFF_MARC_APER_LEN_2:
        case IOMMU_MMIO_OFF_MARC_APER_BAR_3:
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_3:
        case IOMMU_MMIO_OFF_MARC_APER_LEN_3:     return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_RSVD_REG:            return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_CMD_BUF_HEAD_PTR:    return iommuAmdCmdBufHeadPtr_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_CMD_BUF_TAIL_PTR:    return iommuAmdCmdBufTailPtr_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EVT_LOG_HEAD_PTR:    return iommuAmdEvtLogHeadPtr_w(pDevIns, pThis, off, uValue);
        case IOMMU_MMIO_OFF_EVT_LOG_TAIL_PTR:    return iommuAmdEvtLogTailPtr_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_STATUS:              return iommuAmdStatus_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_PPR_LOG_HEAD_PTR:
        case IOMMU_MMIO_OFF_PPR_LOG_TAIL_PTR:

        case IOMMU_MMIO_OFF_GALOG_HEAD_PTR:
        case IOMMU_MMIO_OFF_GALOG_TAIL_PTR:

        case IOMMU_MMIO_OFF_PPR_LOG_B_HEAD_PTR:
        case IOMMU_MMIO_OFF_PPR_LOG_B_TAIL_PTR:

        case IOMMU_MMIO_OFF_EVT_LOG_B_HEAD_PTR:
        case IOMMU_MMIO_OFF_EVT_LOG_B_TAIL_PTR:  return iommuAmdIgnore_w(pDevIns, pThis, off, uValue);

        case IOMMU_MMIO_OFF_PPR_LOG_AUTO_RESP:
        case IOMMU_MMIO_OFF_PPR_LOG_OVERFLOW_EARLY:
        case IOMMU_MMIO_OFF_PPR_LOG_B_OVERFLOW_EARLY:

        /* Not implemented. */
        case IOMMU_MMIO_OFF_SMI_FLT_FIRST:
        case IOMMU_MMIO_OFF_SMI_FLT_LAST:
        {
            LogFunc(("Writing unsupported register: SMI filter %u -> Ignored\n", (off - IOMMU_MMIO_OFF_SMI_FLT_FIRST) >> 3));
            return VINF_SUCCESS;
        }

        /* Unknown. */
        default:
        {
            LogFunc(("Writing unknown register %u (%#x) with %#RX64 -> Ignored\n", off, off, uValue));
            return VINF_SUCCESS;
        }
    }
#else
    /*
     * Figure out which table the register belongs to and validate its index.
     */
    PCIOMMUREGACC pReg;
    if (off < IOMMU_MMIO_OFF_QWORD_TABLE_0_END)
    {
        uint32_t const idxReg = off >> 3;
        Assert(idxReg < RT_ELEMENTS(g_aRegAccess0));
        pReg = &g_aRegAccess0[idxReg];
    }
    else if (   off <  IOMMU_MMIO_OFF_QWORD_TABLE_1_END
             && off >= IOMMU_MMIO_OFF_QWORD_TABLE_1_START)
    {
        uint32_t const idxReg = (off - IOMMU_MMIO_OFF_QWORD_TABLE_1_START) >> 3;
        Assert(idxReg < RT_ELEMENTS(g_aRegAccess1));
        pReg = &g_aRegAccess1[idxReg];
    }
    else if (   off <  IOMMU_MMIO_OFF_QWORD_TABLE_2_END
             && off >= IOMMU_MMIO_OFF_QWORD_TABLE_2_START)
    {
        uint32_t const idxReg = (off - IOMMU_MMIO_OFF_QWORD_TABLE_2_START) >> 3;
        Assert(idxReg < RT_ELEMENTS(g_aRegAccess2));
        pReg = &g_aRegAccess2[idxReg];
    }
    else
    {
        LogFunc(("Writing unknown register %u (%#x) with %#RX64 -> Ignored\n", off, off, uValue));
        return VINF_SUCCESS;
    }

    /*
     * Ensure the register is writable and proceed.
     * If a write handler doesn't exist, it's either a reserved or read-only register.
     */
    if (pReg->pfnWrite)
    {
        /*
         * If the write access is aligned and matches the register size, dispatch right away.
         * This handles all aligned, 32-bit writes as well as aligned 64-bit writes.
         */
        if (   cb == pReg->cb
            && !(off & (cb - 1)))
            return pReg->pfnWrite(pDevIns, pThis, off, uValue);

        /*
         * A 32-bit write for a 64-bit register.
         * We shouldn't get sizes other than 32 bits here as we've specified so with IOM.
         */
        Assert(cb == 4);
        if (!(off & 7))
        {
            /*
             * Lower 32 bits of the register is being written.
             * Merge with higher 32 bits (after reading the full value from the register).
             */
            uint64_t u64Read;
            if (pReg->pfnRead)
            {
                VBOXSTRICTRC rcStrict = pReg->pfnRead(pDevIns, pThis, off, &u64Read);
                if (RT_FAILURE(rcStrict))
                {
                    LogFunc(("Reading off %#x during split write failed! rc=%Rrc\n -> Ignored", off, VBOXSTRICTRC_VAL(rcStrict)));
                    return rcStrict;
                }
            }
            else
                u64Read = 0;

            uValue = (u64Read & UINT64_C(0xffffffff00000000)) | uValue;
            return pReg->pfnWrite(pDevIns, pThis, off, uValue);
        }

        /*
         * Higher 32 bits of the register is being written.
         * Merge with lower 32 bits (after reading the full value from the register).
         */
        Assert(!(off & 3));
        Assert(off & 7);
        Assert(off > 4);
        uint64_t u64Read;
        if (pReg->pfnRead)
        {
            VBOXSTRICTRC rcStrict = pReg->pfnRead(pDevIns, pThis, off - 4, &u64Read);
            if (RT_FAILURE(rcStrict))
            {
                LogFunc(("Reading off %#x during split write failed! rc=%Rrc\n -> Ignored", off, VBOXSTRICTRC_VAL(rcStrict)));
                return rcStrict;
            }
        }
        else
            u64Read = 0;

        uValue = (uValue << 32) | (u64Read & UINT64_C(0xffffffff));
        return pReg->pfnWrite(pDevIns, pThis, off - 4, uValue);
    }
    else
        LogFunc(("Writing reserved or read-only register off=%#x (cb=%u) with %#RX64 -> Ignored\n", off, cb, uValue));

    return VINF_SUCCESS;
#endif
}


/**
 * Reads an IOMMU register (64-bit) given its MMIO offset.
 *
 * All reads are 64-bit but reads to 32-bit registers that are aligned on an 8-byte
 * boundary include the lower half of the subsequent register.
 *
 * This is because most registers are 64-bit and aligned on 8-byte boundaries but
 * some are really 32-bit registers aligned on an 8-byte boundary. We cannot assume
 * software will only perform 32-bit reads on those 32-bit registers that are
 * aligned on 8-byte boundaries.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   off         The MMIO offset of the register in bytes.
 * @param   puResult    Where to store the value being read.
 *
 * @thread  EMT.
 */
static VBOXSTRICTRC iommuAmdReadRegister(PPDMDEVINS pDevIns, uint32_t off, uint64_t *puResult)
{
    Assert(off < IOMMU_MMIO_REGION_SIZE);
    Assert(!(off & 7) || !(off & 3));

    PIOMMU      pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PCPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    Log5Func(("off=%#x\n", off));

    /** @todo IOMMU: fine-grained locking? */
    uint64_t uReg;
    switch (off)
    {
        case IOMMU_MMIO_OFF_DEV_TAB_BAR:              uReg = pThis->aDevTabBaseAddrs[0].u64;    break;
        case IOMMU_MMIO_OFF_CMD_BUF_BAR:              uReg = pThis->CmdBufBaseAddr.u64;         break;
        case IOMMU_MMIO_OFF_EVT_LOG_BAR:              uReg = pThis->EvtLogBaseAddr.u64;         break;
        case IOMMU_MMIO_OFF_CTRL:                     uReg = pThis->Ctrl.u64;                   break;
        case IOMMU_MMIO_OFF_EXCL_BAR:                 uReg = pThis->ExclRangeBaseAddr.u64;      break;
        case IOMMU_MMIO_OFF_EXCL_RANGE_LIMIT:         uReg = pThis->ExclRangeLimit.u64;         break;
        case IOMMU_MMIO_OFF_EXT_FEAT:                 uReg = pThis->ExtFeat.u64;                break;

        case IOMMU_MMIO_OFF_PPR_LOG_BAR:              uReg = pThis->PprLogBaseAddr.u64;         break;
        case IOMMU_MMIO_OFF_HW_EVT_HI:                uReg = pThis->HwEvtHi.u64;                break;
        case IOMMU_MMIO_OFF_HW_EVT_LO:                uReg = pThis->HwEvtLo;                    break;
        case IOMMU_MMIO_OFF_HW_EVT_STATUS:            uReg = pThis->HwEvtStatus.u64;            break;

        case IOMMU_MMIO_OFF_GALOG_BAR:                uReg = pThis->GALogBaseAddr.u64;          break;
        case IOMMU_MMIO_OFF_GALOG_TAIL_ADDR:          uReg = pThis->GALogTailAddr.u64;          break;

        case IOMMU_MMIO_OFF_PPR_LOG_B_BAR:            uReg = pThis->PprLogBBaseAddr.u64;        break;
        case IOMMU_MMIO_OFF_PPR_EVT_B_BAR:            uReg = pThis->EvtLogBBaseAddr.u64;        break;

        case IOMMU_MMIO_OFF_DEV_TAB_SEG_1:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_2:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_3:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_4:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_5:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_6:
        case IOMMU_MMIO_OFF_DEV_TAB_SEG_7:
        {
            uint8_t const offDevTabSeg = (off - IOMMU_MMIO_OFF_DEV_TAB_SEG_FIRST) >> 3;
            uint8_t const idxDevTabSeg = offDevTabSeg + 1;
            Assert(idxDevTabSeg < RT_ELEMENTS(pThis->aDevTabBaseAddrs));
            uReg = pThis->aDevTabBaseAddrs[idxDevTabSeg].u64;
            break;
        }

        case IOMMU_MMIO_OFF_DEV_SPECIFIC_FEAT:        uReg = pThis->DevSpecificFeat.u64;        break;
        case IOMMU_MMIO_OFF_DEV_SPECIFIC_CTRL:        uReg = pThis->DevSpecificCtrl.u64;        break;
        case IOMMU_MMIO_OFF_DEV_SPECIFIC_STATUS:      uReg = pThis->DevSpecificStatus.u64;      break;

        case IOMMU_MMIO_OFF_MSI_VECTOR_0:             uReg = pThis->MiscInfo.u64;               break;
        case IOMMU_MMIO_OFF_MSI_VECTOR_1:             uReg = pThis->MiscInfo.au32[1];           break;
        case IOMMU_MMIO_OFF_MSI_CAP_HDR:
        {
            uint32_t const uMsiCapHdr = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR);
            uint32_t const uMsiAddrLo = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO);
            uReg = RT_MAKE_U64(uMsiCapHdr, uMsiAddrLo);
            break;
        }
        case IOMMU_MMIO_OFF_MSI_ADDR_LO:
        {
            uReg = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO);
            break;
        }
        case IOMMU_MMIO_OFF_MSI_ADDR_HI:
        {
            uint32_t const uMsiAddrHi = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI);
            uint32_t const uMsiData   = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA);
            uReg = RT_MAKE_U64(uMsiAddrHi, uMsiData);
            break;
        }
        case IOMMU_MMIO_OFF_MSI_DATA:
        {
            uReg = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA);
            break;
        }
        case IOMMU_MMIO_OFF_MSI_MAPPING_CAP_HDR:
        {
            /*
             * The PCI spec. lists MSI Mapping Capability 08H as related to HyperTransport capability.
             * The AMD IOMMU spec. fails to mention it explicitly and lists values for this register as
             * though HyperTransport is supported. We don't support HyperTransport, we thus just return
             * 0 for this register.
             */
            uReg = RT_MAKE_U64(0, pThis->PerfOptCtrl.u32);
            break;
        }

        case IOMMU_MMIO_OFF_PERF_OPT_CTRL:            uReg = pThis->PerfOptCtrl.u32;            break;

        case IOMMU_MMIO_OFF_XT_GEN_INTR_CTRL:         uReg = pThis->XtGenIntrCtrl.u64;          break;
        case IOMMU_MMIO_OFF_XT_PPR_INTR_CTRL:         uReg = pThis->XtPprIntrCtrl.u64;          break;
        case IOMMU_MMIO_OFF_XT_GALOG_INT_CTRL:        uReg = pThis->XtGALogIntrCtrl.u64;        break;

        case IOMMU_MMIO_OFF_MARC_APER_BAR_0:          uReg = pThis->aMarcApers[0].Base.u64;     break;
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_0:        uReg = pThis->aMarcApers[0].Reloc.u64;    break;
        case IOMMU_MMIO_OFF_MARC_APER_LEN_0:          uReg = pThis->aMarcApers[0].Length.u64;   break;
        case IOMMU_MMIO_OFF_MARC_APER_BAR_1:          uReg = pThis->aMarcApers[1].Base.u64;     break;
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_1:        uReg = pThis->aMarcApers[1].Reloc.u64;    break;
        case IOMMU_MMIO_OFF_MARC_APER_LEN_1:          uReg = pThis->aMarcApers[1].Length.u64;   break;
        case IOMMU_MMIO_OFF_MARC_APER_BAR_2:          uReg = pThis->aMarcApers[2].Base.u64;     break;
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_2:        uReg = pThis->aMarcApers[2].Reloc.u64;    break;
        case IOMMU_MMIO_OFF_MARC_APER_LEN_2:          uReg = pThis->aMarcApers[2].Length.u64;   break;
        case IOMMU_MMIO_OFF_MARC_APER_BAR_3:          uReg = pThis->aMarcApers[3].Base.u64;     break;
        case IOMMU_MMIO_OFF_MARC_APER_RELOC_3:        uReg = pThis->aMarcApers[3].Reloc.u64;    break;
        case IOMMU_MMIO_OFF_MARC_APER_LEN_3:          uReg = pThis->aMarcApers[3].Length.u64;   break;

        case IOMMU_MMIO_OFF_RSVD_REG:                 uReg = pThis->RsvdReg;                    break;

        case IOMMU_MMIO_OFF_CMD_BUF_HEAD_PTR:         uReg = pThis->CmdBufHeadPtr.u64;          break;
        case IOMMU_MMIO_OFF_CMD_BUF_TAIL_PTR:         uReg = pThis->CmdBufTailPtr.u64;          break;
        case IOMMU_MMIO_OFF_EVT_LOG_HEAD_PTR:         uReg = pThis->EvtLogHeadPtr.u64;          break;
        case IOMMU_MMIO_OFF_EVT_LOG_TAIL_PTR:         uReg = pThis->EvtLogTailPtr.u64;          break;

        case IOMMU_MMIO_OFF_STATUS:                   uReg = pThis->Status.u64;                 break;

        case IOMMU_MMIO_OFF_PPR_LOG_HEAD_PTR:         uReg = pThis->PprLogHeadPtr.u64;          break;
        case IOMMU_MMIO_OFF_PPR_LOG_TAIL_PTR:         uReg = pThis->PprLogTailPtr.u64;          break;

        case IOMMU_MMIO_OFF_GALOG_HEAD_PTR:           uReg = pThis->GALogHeadPtr.u64;           break;
        case IOMMU_MMIO_OFF_GALOG_TAIL_PTR:           uReg = pThis->GALogTailPtr.u64;           break;

        case IOMMU_MMIO_OFF_PPR_LOG_B_HEAD_PTR:       uReg = pThis->PprLogBHeadPtr.u64;         break;
        case IOMMU_MMIO_OFF_PPR_LOG_B_TAIL_PTR:       uReg = pThis->PprLogBTailPtr.u64;         break;

        case IOMMU_MMIO_OFF_EVT_LOG_B_HEAD_PTR:       uReg = pThis->EvtLogBHeadPtr.u64;         break;
        case IOMMU_MMIO_OFF_EVT_LOG_B_TAIL_PTR:       uReg = pThis->EvtLogBTailPtr.u64;         break;

        case IOMMU_MMIO_OFF_PPR_LOG_AUTO_RESP:        uReg = pThis->PprLogAutoResp.u64;         break;
        case IOMMU_MMIO_OFF_PPR_LOG_OVERFLOW_EARLY:   uReg = pThis->PprLogOverflowEarly.u64;    break;
        case IOMMU_MMIO_OFF_PPR_LOG_B_OVERFLOW_EARLY: uReg = pThis->PprLogBOverflowEarly.u64;   break;

        /* Not implemented. */
        case IOMMU_MMIO_OFF_SMI_FLT_FIRST:
        case IOMMU_MMIO_OFF_SMI_FLT_LAST:
        {
            LogFunc(("Reading unsupported register: SMI filter %u\n", (off - IOMMU_MMIO_OFF_SMI_FLT_FIRST) >> 3));
            uReg = 0;
            break;
        }

        /* Unknown. */
        default:
        {
            LogFunc(("Reading unknown register %u (%#x) -> 0\n", off, off));
            uReg = 0;
            return VINF_IOM_MMIO_UNUSED_00;
        }
    }

    *puResult = uReg;
    return VINF_SUCCESS;
}


/**
 * Raises the MSI interrupt for the IOMMU device.
 *
 * @param   pDevIns     The IOMMU device instance.
 *
 * @thread  Any.
 * @remarks The IOMMU lock may or may not be held.
 */
static void iommuAmdRaiseMsiInterrupt(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("\n"));
    if (iommuAmdIsMsiEnabled(pDevIns))
    {
        LogFunc(("Raising MSI\n"));
        PDMDevHlpPCISetIrq(pDevIns, 0, PDM_IRQ_LEVEL_HIGH);
    }
}


/**
 * Clears the MSI interrupt for the IOMMU device.
 *
 * @param   pDevIns     The IOMMU device instance.
 *
 * @thread  Any.
 * @remarks The IOMMU lock may or may not be held.
 */
static void iommuAmdClearMsiInterrupt(PPDMDEVINS pDevIns)
{
    if (iommuAmdIsMsiEnabled(pDevIns))
        PDMDevHlpPCISetIrq(pDevIns, 0, PDM_IRQ_LEVEL_LOW);
}


/**
 * Writes an entry to the event log in memory.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pEvent      The event to log.
 *
 * @thread  Any.
 */
static int iommuAmdWriteEvtLogEntry(PPDMDEVINS pDevIns, PCEVT_GENERIC_T pEvent)
{
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    IOMMU_ASSERT_LOCKED(pDevIns);

    /* Check if event logging is active and the log has not overflowed. */
    IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
    if (   Status.n.u1EvtLogRunning
        && !Status.n.u1EvtOverflow)
    {
        uint32_t const cbEvt = sizeof(*pEvent);

        /* Get the offset we need to write the event to in memory (circular buffer offset). */
        uint32_t const offEvt = pThis->EvtLogTailPtr.n.off;
        Assert(!(offEvt & ~IOMMU_EVT_LOG_TAIL_PTR_VALID_MASK));

        /* Ensure we have space in the event log. */
        uint32_t const cMaxEvts = iommuAmdGetBufMaxEntries(pThis->EvtLogBaseAddr.n.u4Len);
        uint32_t const cEvts    = iommuAmdGetEvtLogEntryCount(pThis);
        if (cEvts + 1 < cMaxEvts)
        {
            /* Write the event log entry to memory. */
            RTGCPHYS const GCPhysEvtLog      = pThis->EvtLogBaseAddr.n.u40Base << X86_PAGE_4K_SHIFT;
            RTGCPHYS const GCPhysEvtLogEntry = GCPhysEvtLog + offEvt;
            int rc = PDMDevHlpPCIPhysWrite(pDevIns, GCPhysEvtLogEntry, pEvent, cbEvt);
            if (RT_FAILURE(rc))
                LogFunc(("Failed to write event log entry at %#RGp. rc=%Rrc\n", GCPhysEvtLogEntry, rc));

            /* Increment the event log tail pointer. */
            uint32_t const cbEvtLog = iommuAmdGetTotalBufLength(pThis->EvtLogBaseAddr.n.u4Len);
            pThis->EvtLogTailPtr.n.off = (offEvt + cbEvt) % cbEvtLog;

            /* Indicate that an event log entry was written. */
            ASMAtomicOrU64(&pThis->Status.u64, IOMMU_STATUS_EVT_LOG_INTR);

            /* Check and signal an interrupt if software wants to receive one when an event log entry is written. */
            IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
            if (Ctrl.n.u1EvtIntrEn)
                iommuAmdRaiseMsiInterrupt(pDevIns);
        }
        else
        {
            /* Indicate that the event log has overflowed. */
            ASMAtomicOrU64(&pThis->Status.u64, IOMMU_STATUS_EVT_LOG_OVERFLOW);

            /* Check and signal an interrupt if software wants to receive one when the event log has overflowed. */
            IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
            if (Ctrl.n.u1EvtIntrEn)
                iommuAmdRaiseMsiInterrupt(pDevIns);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Sets an event in the hardware error registers.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   pEvent      The event.
 *
 * @thread  Any.
 */
static void iommuAmdSetHwError(PPDMDEVINS pDevIns, PCEVT_GENERIC_T pEvent)
{
    IOMMU_ASSERT_LOCKED(pDevIns);

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    if (pThis->ExtFeat.n.u1HwErrorSup)
    {
        if (pThis->HwEvtStatus.n.u1Valid)
            pThis->HwEvtStatus.n.u1Overflow = 1;
        pThis->HwEvtStatus.n.u1Valid = 1;
        pThis->HwEvtHi.u64 = RT_MAKE_U64(pEvent->au32[0], pEvent->au32[1]);
        pThis->HwEvtLo     = RT_MAKE_U64(pEvent->au32[2], pEvent->au32[3]);
        Assert(   pThis->HwEvtHi.n.u4EvtCode == IOMMU_EVT_DEV_TAB_HW_ERROR
               || pThis->HwEvtHi.n.u4EvtCode == IOMMU_EVT_PAGE_TAB_HW_ERROR
               || pThis->HwEvtHi.n.u4EvtCode == IOMMU_EVT_COMMAND_HW_ERROR);
    }
}


/**
 * Initializes a PAGE_TAB_HARDWARE_ERROR event.
 *
 * @param   uDevId              The device ID.
 * @param   uDomainId           The domain ID.
 * @param   GCPhysPtEntity      The system physical address of the page table
 *                              entity.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtPageTabHwErr    Where to store the initialized event.
 */
static void iommuAmdInitPageTabHwErrorEvent(uint16_t uDevId, uint16_t uDomainId, RTGCPHYS GCPhysPtEntity, IOMMUOP enmOp,
                                            PEVT_PAGE_TAB_HW_ERR_T pEvtPageTabHwErr)
{
    memset(pEvtPageTabHwErr, 0, sizeof(*pEvtPageTabHwErr));
    pEvtPageTabHwErr->n.u16DevId           = uDevId;
    pEvtPageTabHwErr->n.u16DomainOrPasidLo = uDomainId;
    pEvtPageTabHwErr->n.u1GuestOrNested    = 0;
    pEvtPageTabHwErr->n.u1Interrupt        = RT_BOOL(enmOp == IOMMUOP_INTR_REQ);
    pEvtPageTabHwErr->n.u1ReadWrite        = RT_BOOL(enmOp == IOMMUOP_MEM_WRITE);
    pEvtPageTabHwErr->n.u1Translation      = RT_BOOL(enmOp == IOMMUOP_TRANSLATE_REQ);
    pEvtPageTabHwErr->n.u2Type             = enmOp == IOMMUOP_CMD ? HWEVTTYPE_DATA_ERROR : HWEVTTYPE_TARGET_ABORT;
    pEvtPageTabHwErr->n.u4EvtCode          = IOMMU_EVT_PAGE_TAB_HW_ERROR;
    pEvtPageTabHwErr->n.u64Addr            = GCPhysPtEntity;
}


/**
 * Raises a PAGE_TAB_HARDWARE_ERROR event.
 *
 * @param   pDevIns             The IOMMU device instance.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtPageTabHwErr    The page table hardware error event.
 *
 * @thread  Any.
 */
static void iommuAmdRaisePageTabHwErrorEvent(PPDMDEVINS pDevIns, IOMMUOP enmOp, PEVT_PAGE_TAB_HW_ERR_T pEvtPageTabHwErr)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_PAGE_TAB_HW_ERR_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtPageTabHwErr;

    IOMMU_LOCK_NORET(pDevIns);

    iommuAmdSetHwError(pDevIns, (PCEVT_GENERIC_T)pEvent);
    iommuAmdWriteEvtLogEntry(pDevIns, (PCEVT_GENERIC_T)pEvent);
    if (enmOp != IOMMUOP_CMD)
        iommuAmdSetPciTargetAbort(pDevIns);

    IOMMU_UNLOCK(pDevIns);

    LogFunc(("Raised PAGE_TAB_HARDWARE_ERROR. uDevId=%#x uDomainId=%#x GCPhysPtEntity=%#RGp enmOp=%u u2Type=%u\n",
         pEvtPageTabHwErr->n.u16DevId, pEvtPageTabHwErr->n.u16DomainOrPasidLo, pEvtPageTabHwErr->n.u64Addr, enmOp,
         pEvtPageTabHwErr->n.u2Type));
}


/**
 * Initializes a COMMAND_HARDWARE_ERROR event.
 *
 * @param   GCPhysAddr      The system physical address the IOMMU attempted to access.
 * @param   pEvtCmdHwErr    Where to store the initialized event.
 */
static void iommuAmdInitCmdHwErrorEvent(RTGCPHYS GCPhysAddr, PEVT_CMD_HW_ERR_T pEvtCmdHwErr)
{
    memset(pEvtCmdHwErr, 0, sizeof(*pEvtCmdHwErr));
    pEvtCmdHwErr->n.u2Type    = HWEVTTYPE_DATA_ERROR;
    pEvtCmdHwErr->n.u4EvtCode = IOMMU_EVT_COMMAND_HW_ERROR;
    pEvtCmdHwErr->n.u64Addr   = GCPhysAddr;
}


/**
 * Raises a COMMAND_HARDWARE_ERROR event.
 *
 * @param   pDevIns         The IOMMU device instance.
 * @param   pEvtCmdHwErr    The command hardware error event.
 *
 * @thread  Any.
 */
static void iommuAmdRaiseCmdHwErrorEvent(PPDMDEVINS pDevIns, PCEVT_CMD_HW_ERR_T pEvtCmdHwErr)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_CMD_HW_ERR_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtCmdHwErr;
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    IOMMU_LOCK_NORET(pDevIns);

    iommuAmdSetHwError(pDevIns, (PCEVT_GENERIC_T)pEvent);
    iommuAmdWriteEvtLogEntry(pDevIns, (PCEVT_GENERIC_T)pEvent);
    ASMAtomicAndU64(&pThis->Status.u64, ~IOMMU_STATUS_CMD_BUF_RUNNING);

    IOMMU_UNLOCK(pDevIns);

    LogFunc(("Raised COMMAND_HARDWARE_ERROR. GCPhysCmd=%#RGp u2Type=%u\n", pEvtCmdHwErr->n.u64Addr, pEvtCmdHwErr->n.u2Type));
}


/**
 * Initializes a DEV_TAB_HARDWARE_ERROR event.
 *
 * @param   uDevId              The device ID.
 * @param   GCPhysDte           The system physical address of the failed device table
 *                              access.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtDevTabHwErr     Where to store the initialized event.
 */
static void iommuAmdInitDevTabHwErrorEvent(uint16_t uDevId, RTGCPHYS GCPhysDte, IOMMUOP enmOp,
                                           PEVT_DEV_TAB_HW_ERROR_T pEvtDevTabHwErr)
{
    memset(pEvtDevTabHwErr, 0, sizeof(*pEvtDevTabHwErr));
    pEvtDevTabHwErr->n.u16DevId      = uDevId;
    pEvtDevTabHwErr->n.u1Intr        = RT_BOOL(enmOp == IOMMUOP_INTR_REQ);
    /** @todo IOMMU: Any other transaction type that can set read/write bit? */
    pEvtDevTabHwErr->n.u1ReadWrite   = RT_BOOL(enmOp == IOMMUOP_MEM_WRITE);
    pEvtDevTabHwErr->n.u1Translation = RT_BOOL(enmOp == IOMMUOP_TRANSLATE_REQ);
    pEvtDevTabHwErr->n.u2Type        = enmOp == IOMMUOP_CMD ? HWEVTTYPE_DATA_ERROR : HWEVTTYPE_TARGET_ABORT;
    pEvtDevTabHwErr->n.u4EvtCode     = IOMMU_EVT_DEV_TAB_HW_ERROR;
    pEvtDevTabHwErr->n.u64Addr       = GCPhysDte;
}


/**
 * Raises a DEV_TAB_HARDWARE_ERROR event.
 *
 * @param   pDevIns             The IOMMU device instance.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtDevTabHwErr     The device table hardware error event.
 *
 * @thread  Any.
 */
static void iommuAmdRaiseDevTabHwErrorEvent(PPDMDEVINS pDevIns, IOMMUOP enmOp, PEVT_DEV_TAB_HW_ERROR_T pEvtDevTabHwErr)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_DEV_TAB_HW_ERROR_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtDevTabHwErr;

    IOMMU_LOCK_NORET(pDevIns);

    iommuAmdSetHwError(pDevIns, (PCEVT_GENERIC_T)pEvent);
    iommuAmdWriteEvtLogEntry(pDevIns, (PCEVT_GENERIC_T)pEvent);
    if (enmOp != IOMMUOP_CMD)
        iommuAmdSetPciTargetAbort(pDevIns);

    IOMMU_UNLOCK(pDevIns);

    LogFunc(("Raised DEV_TAB_HARDWARE_ERROR. uDevId=%#x GCPhysDte=%#RGp enmOp=%u u2Type=%u\n", pEvtDevTabHwErr->n.u16DevId,
             pEvtDevTabHwErr->n.u64Addr, enmOp, pEvtDevTabHwErr->n.u2Type));
}


/**
 * Initializes an ILLEGAL_COMMAND_ERROR event.
 *
 * @param   GCPhysCmd       The system physical address of the failed command
 *                          access.
 * @param   pEvtIllegalCmd  Where to store the initialized event.
 */
static void iommuAmdInitIllegalCmdEvent(RTGCPHYS GCPhysCmd, PEVT_ILLEGAL_CMD_ERR_T pEvtIllegalCmd)
{
    Assert(!(GCPhysCmd & UINT64_C(0xf)));
    memset(pEvtIllegalCmd, 0, sizeof(*pEvtIllegalCmd));
    pEvtIllegalCmd->n.u4EvtCode = IOMMU_EVT_ILLEGAL_CMD_ERROR;
    pEvtIllegalCmd->n.u64Addr   = GCPhysCmd;
}


/**
 * Raises an ILLEGAL_COMMAND_ERROR event.
 *
 * @param   pDevIns         The IOMMU device instance.
 * @param   pEvtIllegalCmd  The illegal command error event.
 */
static void iommuAmdRaiseIllegalCmdEvent(PPDMDEVINS pDevIns, PCEVT_ILLEGAL_CMD_ERR_T pEvtIllegalCmd)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_ILLEGAL_DTE_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtIllegalCmd;
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    IOMMU_LOCK_NORET(pDevIns);

    iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
    ASMAtomicAndU64(&pThis->Status.u64, ~IOMMU_STATUS_CMD_BUF_RUNNING);

    IOMMU_UNLOCK(pDevIns);

    LogFunc(("Raised ILLEGAL_COMMAND_ERROR. Addr=%#RGp\n", pEvtIllegalCmd->n.u64Addr));
}


/**
 * Initializes an ILLEGAL_DEV_TABLE_ENTRY event.
 *
 * @param   uDevId          The device ID.
 * @param   uIova           The I/O virtual address.
 * @param   fRsvdNotZero    Whether reserved bits are not zero. Pass @c false if the
 *                          event was caused by an invalid level encoding in the
 *                          DTE.
 * @param   enmOp           The IOMMU operation being performed.
 * @param   pEvtIllegalDte  Where to store the initialized event.
 */
static void iommuAmdInitIllegalDteEvent(uint16_t uDevId, uint64_t uIova, bool fRsvdNotZero, IOMMUOP enmOp,
                                        PEVT_ILLEGAL_DTE_T pEvtIllegalDte)
{
    memset(pEvtIllegalDte, 0, sizeof(*pEvtIllegalDte));
    pEvtIllegalDte->n.u16DevId      = uDevId;
    pEvtIllegalDte->n.u1Interrupt   = RT_BOOL(enmOp == IOMMUOP_INTR_REQ);
    pEvtIllegalDte->n.u1ReadWrite   = RT_BOOL(enmOp == IOMMUOP_MEM_WRITE);
    pEvtIllegalDte->n.u1RsvdNotZero = fRsvdNotZero;
    pEvtIllegalDte->n.u1Translation = RT_BOOL(enmOp == IOMMUOP_TRANSLATE_REQ);
    pEvtIllegalDte->n.u4EvtCode     = IOMMU_EVT_ILLEGAL_DEV_TAB_ENTRY;
    pEvtIllegalDte->n.u64Addr       = uIova & ~UINT64_C(0x3);
    /** @todo r=ramshankar: Not sure why the last 2 bits are marked as reserved by the
     *        IOMMU spec here but not for this field for I/O page fault event. */
    Assert(!(uIova & UINT64_C(0x3)));
}


/**
 * Raises an ILLEGAL_DEV_TABLE_ENTRY event.
 *
 * @param   pDevIns         The IOMMU instance data.
 * @param   enmOp           The IOMMU operation being performed.
 * @param   pEvtIllegalDte  The illegal device table entry event.
 * @param   enmEvtType      The illegal device table entry event type.
 *
 * @thread  Any.
 */
static void iommuAmdRaiseIllegalDteEvent(PPDMDEVINS pDevIns, IOMMUOP enmOp, PCEVT_ILLEGAL_DTE_T pEvtIllegalDte,
                                         EVT_ILLEGAL_DTE_TYPE_T enmEvtType)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_ILLEGAL_DTE_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtIllegalDte;

    IOMMU_LOCK_NORET(pDevIns);

    iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
    if (enmOp != IOMMUOP_CMD)
        iommuAmdSetPciTargetAbort(pDevIns);

    IOMMU_UNLOCK(pDevIns);

    LogFunc(("Raised ILLEGAL_DTE_EVENT. uDevId=%#x uIova=%#RX64 enmOp=%u enmEvtType=%u\n", pEvtIllegalDte->n.u16DevId,
             pEvtIllegalDte->n.u64Addr, enmOp, enmEvtType));
    NOREF(enmEvtType);
}


/**
 * Initializes an IO_PAGE_FAULT event.
 *
 * @param   uDevId              The device ID.
 * @param   uDomainId           The domain ID.
 * @param   uIova               The I/O virtual address being accessed.
 * @param   fPresent            Transaction to a page marked as present (including
 *                              DTE.V=1) or interrupt marked as remapped
 *                              (IRTE.RemapEn=1).
 * @param   fRsvdNotZero        Whether reserved bits are not zero. Pass @c false if
 *                              the I/O page fault was caused by invalid level
 *                              encoding.
 * @param   fPermDenied         Permission denied for the address being accessed.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtIoPageFault     Where to store the initialized event.
 */
static void iommuAmdInitIoPageFaultEvent(uint16_t uDevId, uint16_t uDomainId, uint64_t uIova, bool fPresent, bool fRsvdNotZero,
                                         bool fPermDenied, IOMMUOP enmOp, PEVT_IO_PAGE_FAULT_T pEvtIoPageFault)
{
    Assert(!fPermDenied || fPresent);
    memset(pEvtIoPageFault, 0, sizeof(*pEvtIoPageFault));
    pEvtIoPageFault->n.u16DevId            = uDevId;
    //pEvtIoPageFault->n.u4PasidHi         = 0;
    pEvtIoPageFault->n.u16DomainOrPasidLo  = uDomainId;
    //pEvtIoPageFault->n.u1GuestOrNested   = 0;
    //pEvtIoPageFault->n.u1NoExecute       = 0;
    //pEvtIoPageFault->n.u1User            = 0;
    pEvtIoPageFault->n.u1Interrupt         = RT_BOOL(enmOp == IOMMUOP_INTR_REQ);
    pEvtIoPageFault->n.u1Present           = fPresent;
    pEvtIoPageFault->n.u1ReadWrite         = RT_BOOL(enmOp == IOMMUOP_MEM_WRITE);
    pEvtIoPageFault->n.u1PermDenied        = fPermDenied;
    pEvtIoPageFault->n.u1RsvdNotZero       = fRsvdNotZero;
    pEvtIoPageFault->n.u1Translation       = RT_BOOL(enmOp == IOMMUOP_TRANSLATE_REQ);
    pEvtIoPageFault->n.u4EvtCode           = IOMMU_EVT_IO_PAGE_FAULT;
    pEvtIoPageFault->n.u64Addr             = uIova;
}


/**
 * Raises an IO_PAGE_FAULT event.
 *
 * @param   pDevIns             The IOMMU instance data.
 * @param   pDte                The device table entry. Optional, can be NULL
 *                              depending on @a enmOp.
 * @param   pIrte               The interrupt remapping table entry. Optional, can
 *                              be NULL depending on @a enmOp.
 * @param   enmOp               The IOMMU operation being performed.
 * @param   pEvtIoPageFault     The I/O page fault event.
 * @param   enmEvtType          The I/O page fault event type.
 *
 * @thread  Any.
 */
static void iommuAmdRaiseIoPageFaultEvent(PPDMDEVINS pDevIns, PCDTE_T pDte, PCIRTE_T pIrte, IOMMUOP enmOp,
                                          PCEVT_IO_PAGE_FAULT_T pEvtIoPageFault, EVT_IO_PAGE_FAULT_TYPE_T enmEvtType)
{
    AssertCompile(sizeof(EVT_GENERIC_T) == sizeof(EVT_IO_PAGE_FAULT_T));
    PCEVT_GENERIC_T pEvent = (PCEVT_GENERIC_T)pEvtIoPageFault;

    IOMMU_LOCK_NORET(pDevIns);

    bool fSuppressEvtLogging = false;
    if (   enmOp == IOMMUOP_MEM_READ
        || enmOp == IOMMUOP_MEM_WRITE)
    {
        if (   pDte
            && pDte->n.u1Valid)
        {
            fSuppressEvtLogging = pDte->n.u1SuppressAllPfEvents;
            /** @todo IOMMU: Implement DTE.SE bit, i.e. device ID specific I/O page fault
             *        suppression. Perhaps will be possible when we complete IOTLB/cache
             *        handling. */
        }
    }
    else if (enmOp == IOMMUOP_INTR_REQ)
    {
        if (   pDte
            && pDte->n.u1IntrMapValid)
            fSuppressEvtLogging = !pDte->n.u1IgnoreUnmappedIntrs;

        if (   !fSuppressEvtLogging
            && pIrte)
            fSuppressEvtLogging = pIrte->n.u1SuppressIoPf;
    }
    /* else: Events are never suppressed for commands. */

    switch (enmEvtType)
    {
        case kIoPageFaultType_PermDenied:
        {
            /* Cannot be triggered by a command. */
            Assert(enmOp != IOMMUOP_CMD);
            RT_FALL_THRU();
        }
        case kIoPageFaultType_DteRsvdPagingMode:
        case kIoPageFaultType_PteInvalidPageSize:
        case kIoPageFaultType_PteInvalidLvlEncoding:
        case kIoPageFaultType_SkippedLevelIovaNotZero:
        case kIoPageFaultType_PteRsvdNotZero:
        case kIoPageFaultType_PteValidNotSet:
        case kIoPageFaultType_DteTranslationDisabled:
        case kIoPageFaultType_PasidInvalidRange:
        {
            /*
             * For a translation request, the IOMMU doesn't signal an I/O page fault nor does it
             * create an event log entry. See AMD spec. 2.1.3.2 "I/O Page Faults".
             */
            if (enmOp != IOMMUOP_TRANSLATE_REQ)
            {
                if (!fSuppressEvtLogging)
                    iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
                if (enmOp != IOMMUOP_CMD)
                    iommuAmdSetPciTargetAbort(pDevIns);
            }
            break;
        }

        case kIoPageFaultType_UserSupervisor:
        {
            /* Access is blocked and only creates an event log entry. */
            if (!fSuppressEvtLogging)
                iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
            break;
        }

        case kIoPageFaultType_IrteAddrInvalid:
        case kIoPageFaultType_IrteRsvdNotZero:
        case kIoPageFaultType_IrteRemapEn:
        case kIoPageFaultType_IrteRsvdIntType:
        case kIoPageFaultType_IntrReqAborted:
        case kIoPageFaultType_IntrWithPasid:
        {
            /* Only trigerred by interrupt requests. */
            Assert(enmOp == IOMMUOP_INTR_REQ);
            if (!fSuppressEvtLogging)
                iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
            iommuAmdSetPciTargetAbort(pDevIns);
            break;
        }

        case kIoPageFaultType_SmiFilterMismatch:
        {
            /* Not supported and probably will never be, assert. */
            AssertMsgFailed(("kIoPageFaultType_SmiFilterMismatch - Upstream SMI requests not supported/implemented."));
            break;
        }

        case kIoPageFaultType_DevId_Invalid:
        {
            /* Cannot be triggered by a command. */
            Assert(enmOp != IOMMUOP_CMD);
            Assert(enmOp != IOMMUOP_TRANSLATE_REQ); /** @todo IOMMU: We don't support translation requests yet. */
            if (!fSuppressEvtLogging)
                iommuAmdWriteEvtLogEntry(pDevIns, pEvent);
            if (   enmOp == IOMMUOP_MEM_READ
                || enmOp == IOMMUOP_MEM_WRITE)
                iommuAmdSetPciTargetAbort(pDevIns);
            break;
        }
    }

    IOMMU_UNLOCK(pDevIns);
}


/**
 * Returns whether the I/O virtual address is to be excluded from translation and
 * permission checks.
 *
 * @returns @c true if the DVA is excluded, @c false otherwise.
 * @param   pThis   The IOMMU device state.
 * @param   pDte    The device table entry.
 * @param   uIova   The I/O virtual address.
 *
 * @remarks Ensure the exclusion range is enabled prior to calling this function.
 *
 * @thread  Any.
 */
static bool iommuAmdIsDvaInExclRange(PCIOMMU pThis, PCDTE_T pDte, uint64_t uIova)
{
    /* Ensure the exclusion range is enabled. */
    Assert(pThis->ExclRangeBaseAddr.n.u1ExclEnable);

    /* Check if the IOVA falls within the exclusion range. */
    uint64_t const uIovaExclFirst = pThis->ExclRangeBaseAddr.n.u40ExclRangeBase << X86_PAGE_4K_SHIFT;
    uint64_t const uIovaExclLast  = pThis->ExclRangeLimit.n.u52ExclLimit;
    if (uIovaExclLast - uIova >= uIovaExclFirst)
    {
        /* Check if device access to addresses in the exclusion range can be forwarded untranslated. */
        if (    pThis->ExclRangeBaseAddr.n.u1AllowAll
            ||  pDte->n.u1AllowExclusion)
            return true;
    }
    return false;
}


/**
 * Reads a device table entry from guest memory given the device ID.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uDevId      The device ID.
 * @param   enmOp       The IOMMU operation being performed.
 * @param   pDte        Where to store the device table entry.
 *
 * @thread  Any.
 */
static int iommuAmdReadDte(PPDMDEVINS pDevIns, uint16_t uDevId, IOMMUOP enmOp, PDTE_T pDte)
{
    PCIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);

    uint8_t const idxSegsEn = Ctrl.n.u3DevTabSegEn;
    Assert(idxSegsEn < RT_ELEMENTS(g_auDevTabSegShifts));
    Assert(idxSegsEn < RT_ELEMENTS(g_auDevTabSegMasks));

    uint8_t const idxSeg = (uDevId & g_auDevTabSegMasks[idxSegsEn]) >> g_auDevTabSegShifts[idxSegsEn];
    Assert(idxSeg < RT_ELEMENTS(pThis->aDevTabBaseAddrs));

    RTGCPHYS const GCPhysDevTab = pThis->aDevTabBaseAddrs[idxSeg].n.u40Base << X86_PAGE_4K_SHIFT;
    uint16_t const offDte       = (uDevId & ~g_auDevTabSegMasks[idxSegsEn]) * sizeof(DTE_T);
    RTGCPHYS const GCPhysDte    = GCPhysDevTab + offDte;

    Assert(!(GCPhysDevTab & X86_PAGE_4K_OFFSET_MASK));
    int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhysDte, pDte, sizeof(*pDte));
    if (RT_FAILURE(rc))
    {
        LogFunc(("Failed to read device table entry at %#RGp. rc=%Rrc -> DevTabHwError\n", GCPhysDte, rc));

        EVT_DEV_TAB_HW_ERROR_T EvtDevTabHwErr;
        iommuAmdInitDevTabHwErrorEvent(uDevId, GCPhysDte, enmOp, &EvtDevTabHwErr);
        iommuAmdRaiseDevTabHwErrorEvent(pDevIns, enmOp, &EvtDevTabHwErr);
        return VERR_IOMMU_IPE_1;
    }

    return rc;
}


/**
 * Walks the I/O page table to translate the I/O virtual address to a system
 * physical address.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   uIova           The I/O virtual address to translate. Must be 4K aligned.
 * @param   uDevId          The device ID.
 * @param   fAccess         The access permissions (IOMMU_IO_PERM_XXX). This is the
 *                          permissions for the access being made.
 * @param   pDte            The device table entry.
 * @param   enmOp           The IOMMU operation being performed.
 * @param   pWalkResult     Where to store the results of the I/O page walk. This is
 *                          only updated when VINF_SUCCESS is returned.
 *
 * @thread  Any.
 */
static int iommuAmdWalkIoPageTable(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uIova, uint8_t fAccess, PCDTE_T pDte,
                                   IOMMUOP enmOp, PIOWALKRESULT pWalkResult)
{
    Assert(pDte->n.u1Valid);
    Assert(!(uIova & X86_PAGE_4K_OFFSET_MASK));

    /* If the translation is not valid, raise an I/O page fault. */
    if (pDte->n.u1TranslationValid)
    { /* likely */ }
    else
    {
        /** @todo r=ramshankar: The AMD IOMMU spec. says page walk is terminated but
         *        doesn't explicitly say whether an I/O page fault is raised. From other
         *        places in the spec. it seems early page walk terminations (starting with
         *        the DTE) return the state computed so far and raises an I/O page fault. So
         *        returning an invalid translation rather than skipping translation. */
        LogFunc(("Translation valid bit not set -> IOPF\n"));
        EVT_IO_PAGE_FAULT_T EvtIoPageFault;
        iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, false /* fPresent */, false /* fRsvdNotZero */,
                                     false /* fPermDenied */, enmOp, &EvtIoPageFault);
        iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                      kIoPageFaultType_DteTranslationDisabled);
        return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
    }

    /* If the root page table level is 0, translation is skipped and access is controlled by the permission bits. */
    uint8_t const uMaxLevel = pDte->n.u3Mode;
    if (uMaxLevel != 0)
    { /* likely */ }
    else
    {
        uint8_t const fDtePerm = (pDte->au64[0] >> IOMMU_IO_PERM_SHIFT) & IOMMU_IO_PERM_MASK;
        if ((fAccess & fDtePerm) != fAccess)
        {
            LogFunc(("Access denied for IOVA (%#RX64). fAccess=%#x fDtePerm=%#x\n", uIova, fAccess, fDtePerm));
            return VERR_IOMMU_ADDR_ACCESS_DENIED;
        }
        pWalkResult->GCPhysSpa = uIova;
        pWalkResult->cShift    = 0;
        pWalkResult->fIoPerm   = fDtePerm;
        return VINF_SUCCESS;
    }

    /* If the root page table level exceeds the allowed host-address translation level, page walk is terminated. */
    if (uMaxLevel <= IOMMU_MAX_HOST_PT_LEVEL)
    { /* likely */ }
    else
    {
        /** @todo r=ramshankar: I cannot make out from the AMD IOMMU spec. if I should be
         *        raising an ILLEGAL_DEV_TABLE_ENTRY event or an IO_PAGE_FAULT event here.
         *        I'm just going with I/O page fault. */
        LogFunc(("Invalid root page table level %#x -> IOPF\n", uMaxLevel));
        EVT_IO_PAGE_FAULT_T EvtIoPageFault;
        iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                     false /* fPermDenied */, enmOp, &EvtIoPageFault);
        iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                      kIoPageFaultType_PteInvalidLvlEncoding);
        return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
    }

    /* Check permissions bits of the root page table. */
    uint8_t const fRootPtePerm  = (pDte->au64[0] >> IOMMU_IO_PERM_SHIFT) & IOMMU_IO_PERM_MASK;
    if ((fAccess & fRootPtePerm) == fAccess)
    { /* likely */ }
    else
    {
        LogFunc(("Permission denied (fAccess=%#x fRootPtePerm=%#x) -> IOPF\n", fAccess, fRootPtePerm));
        EVT_IO_PAGE_FAULT_T EvtIoPageFault;
        iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                     true /* fPermDenied */, enmOp, &EvtIoPageFault);
        iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault, kIoPageFaultType_PermDenied);
        return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
    }

    /** @todo r=ramshankar: IOMMU: Consider splitting the rest of this into a separate
     *        function called iommuAmdWalkIoPageDirectory() and call it for multi-page
     *        accesses from the 2nd page. We can avoid re-checking the DTE root-page
     *        table entry every time. Not sure if it's worth optimizing that case now
     *        or if at all. */

    /* The virtual address bits indexing table. */
    static uint8_t const  s_acIovaLevelShifts[] = { 0, 12, 21, 30, 39, 48, 57, 0 };
    static uint64_t const s_auIovaLevelMasks[]  = { UINT64_C(0x0000000000000000),
                                                    UINT64_C(0x00000000001ff000),
                                                    UINT64_C(0x000000003fe00000),
                                                    UINT64_C(0x0000007fc0000000),
                                                    UINT64_C(0x0000ff8000000000),
                                                    UINT64_C(0x01ff000000000000),
                                                    UINT64_C(0xfe00000000000000),
                                                    UINT64_C(0x0000000000000000) };
    AssertCompile(RT_ELEMENTS(s_acIovaLevelShifts) == RT_ELEMENTS(s_auIovaLevelMasks));
    AssertCompile(RT_ELEMENTS(s_acIovaLevelShifts) > IOMMU_MAX_HOST_PT_LEVEL);

    /* Traverse the I/O page table starting with the page directory in the DTE. */
    IOPTENTITY_T PtEntity;
    PtEntity.u64 = pDte->au64[0];
    for (;;)
    {
        /* Figure out the system physical address of the page table at the current level. */
        uint8_t const uLevel = PtEntity.n.u3NextLevel;

        /* Read the page table entity at the current level. */
        {
            Assert(uLevel > 0 && uLevel < RT_ELEMENTS(s_acIovaLevelShifts));
            Assert(uLevel <= IOMMU_MAX_HOST_PT_LEVEL);
            uint16_t const idxPte         = (uIova >> s_acIovaLevelShifts[uLevel]) & UINT64_C(0x1ff);
            uint64_t const offPte         = idxPte << 3;
            RTGCPHYS const GCPhysPtEntity = (PtEntity.u64 & IOMMU_PTENTITY_ADDR_MASK) + offPte;
            int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhysPtEntity, &PtEntity.u64, sizeof(PtEntity));
            if (RT_FAILURE(rc))
            {
                LogFunc(("Failed to read page table entry at %#RGp. rc=%Rrc -> PageTabHwError\n", GCPhysPtEntity, rc));
                EVT_PAGE_TAB_HW_ERR_T EvtPageTabHwErr;
                iommuAmdInitPageTabHwErrorEvent(uDevId, pDte->n.u16DomainId, GCPhysPtEntity, enmOp, &EvtPageTabHwErr);
                iommuAmdRaisePageTabHwErrorEvent(pDevIns, enmOp, &EvtPageTabHwErr);
                return VERR_IOMMU_IPE_2;
            }
        }

        /* Check present bit. */
        if (PtEntity.n.u1Present)
        { /* likely */ }
        else
        {
            LogFunc(("Page table entry not present -> IOPF\n"));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, false /* fPresent */, false /* fRsvdNotZero */,
                                         false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault, kIoPageFaultType_PermDenied);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* Check permission bits. */
        uint8_t const fPtePerm  = (PtEntity.u64 >> IOMMU_IO_PERM_SHIFT) & IOMMU_IO_PERM_MASK;
        if ((fAccess & fPtePerm) == fAccess)
        { /* likely */ }
        else
        {
            LogFunc(("Page table entry permission denied (fAccess=%#x fPtePerm=%#x) -> IOPF\n", fAccess, fPtePerm));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                         true /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault, kIoPageFaultType_PermDenied);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* If this is a PTE, we're at the final level and we're done. */
        uint8_t const uNextLevel = PtEntity.n.u3NextLevel;
        if (uNextLevel == 0)
        {
            /* The page size of the translation is the default (4K). */
            pWalkResult->GCPhysSpa = PtEntity.u64 & IOMMU_PTENTITY_ADDR_MASK;
            pWalkResult->cShift    = X86_PAGE_4K_SHIFT;
            pWalkResult->fIoPerm   = fPtePerm;
            return VINF_SUCCESS;
        }
        if (uNextLevel == 7)
        {
            /* The default page size of the translation is overridden. */
            RTGCPHYS const GCPhysPte = PtEntity.u64 & IOMMU_PTENTITY_ADDR_MASK;
            uint8_t        cShift    = X86_PAGE_4K_SHIFT;
            while (GCPhysPte & RT_BIT_64(cShift++))
                ;

            /* The page size must be larger than the default size and lower than the default size of the higher level. */
            Assert(uLevel < IOMMU_MAX_HOST_PT_LEVEL);   /* PTE at level 6 handled outside the loop, uLevel should be <= 5. */
            if (   cShift > s_acIovaLevelShifts[uLevel]
                && cShift < s_acIovaLevelShifts[uLevel + 1])
            {
                pWalkResult->GCPhysSpa = GCPhysPte;
                pWalkResult->cShift    = cShift;
                pWalkResult->fIoPerm   = fPtePerm;
                return VINF_SUCCESS;
            }

            LogFunc(("Page size invalid cShift=%#x -> IOPF\n", cShift));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                         false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                          kIoPageFaultType_PteInvalidPageSize);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* Validate the next level encoding of the PDE. */
#if IOMMU_MAX_HOST_PT_LEVEL < 6
        if (uNextLevel <= IOMMU_MAX_HOST_PT_LEVEL)
        { /* likely */ }
        else
        {
            LogFunc(("Next level of PDE invalid uNextLevel=%#x -> IOPF\n", uNextLevel));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                         false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                          kIoPageFaultType_PteInvalidLvlEncoding);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }
#else
        Assert(uNextLevel <= IOMMU_MAX_HOST_PT_LEVEL);
#endif

        /* Validate level transition. */
        if (uNextLevel < uLevel)
        { /* likely */ }
        else
        {
            LogFunc(("Next level (%#x) must be less than the current level (%#x) -> IOPF\n", uNextLevel, uLevel));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                         false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                          kIoPageFaultType_PteInvalidLvlEncoding);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* Ensure IOVA bits of skipped levels are zero. */
        Assert(uLevel > 0);
        uint64_t uIovaSkipMask = 0;
        for (unsigned idxLevel = uLevel - 1; idxLevel > uNextLevel; idxLevel--)
            uIovaSkipMask |= s_auIovaLevelMasks[idxLevel];
        if (!(uIova & uIovaSkipMask))
        { /* likely */ }
        else
        {
            LogFunc(("IOVA of skipped levels are not zero %#RX64 (SkipMask=%#RX64) -> IOPF\n", uIova, uIovaSkipMask));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, uIova, true /* fPresent */, false /* fRsvdNotZero */,
                                         false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                          kIoPageFaultType_SkippedLevelIovaNotZero);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* Continue with traversing the page directory at this level. */
    }
}


/**
 * Looks up an I/O virtual address from the device table.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU instance data.
 * @param   uDevId      The device ID.
 * @param   uIova       The I/O virtual address to lookup.
 * @param   cbAccess    The size of the access.
 * @param   fAccess     The access permissions (IOMMU_IO_PERM_XXX). This is the
 *                      permissions for the access being made.
 * @param   enmOp       The IOMMU operation being performed.
 * @param   pGCPhysSpa  Where to store the translated system physical address. Only
 *                      valid when translation succeeds and VINF_SUCCESS is
 *                      returned!
 *
 * @thread  Any.
 */
static int iommuAmdLookupDeviceTable(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uIova, size_t cbAccess, uint8_t fAccess,
                                     IOMMUOP enmOp, PRTGCPHYS pGCPhysSpa)
{
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    /* Read the device table entry from memory. */
    DTE_T Dte;
    int rc = iommuAmdReadDte(pDevIns, uDevId, enmOp, &Dte);
    if (RT_SUCCESS(rc))
    {
        /* If the DTE is not valid, addresses are forwarded without translation */
        if (Dte.n.u1Valid)
        { /* likely */ }
        else
        {
            /** @todo IOMMU: Add to IOLTB cache. */
            *pGCPhysSpa = uIova;
            return VINF_SUCCESS;
        }

        /* Validate bits 127:0 of the device table entry when DTE.V is 1. */
        uint64_t const fRsvd0 = Dte.au64[0] & ~(IOMMU_DTE_QWORD_0_VALID_MASK & ~IOMMU_DTE_QWORD_0_FEAT_MASK);
        uint64_t const fRsvd1 = Dte.au64[1] & ~(IOMMU_DTE_QWORD_1_VALID_MASK & ~IOMMU_DTE_QWORD_1_FEAT_MASK);
        if (RT_LIKELY(   !fRsvd0
                      && !fRsvd1))
        { /* likely */ }
        else
        {
            LogFunc(("Invalid reserved bits in DTE (u64[0]=%#RX64 u64[1]=%#RX64) -> Illegal DTE\n", fRsvd0, fRsvd1));
            EVT_ILLEGAL_DTE_T Event;
            iommuAmdInitIllegalDteEvent(uDevId, uIova, true /* fRsvdNotZero */, enmOp, &Event);
            iommuAmdRaiseIllegalDteEvent(pDevIns, enmOp, &Event, kIllegalDteType_RsvdNotZero);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        /* If the IOVA is subject to address exclusion, addresses are forwarded without translation. */
        if (   !pThis->ExclRangeBaseAddr.n.u1ExclEnable
            || !iommuAmdIsDvaInExclRange(pThis, &Dte, uIova))
        { /* likely */ }
        else
        {
            /** @todo IOMMU: Add to IOLTB cache. */
            *pGCPhysSpa = uIova;
            return VINF_SUCCESS;
        }

        /** @todo IOMMU: Perhaps do the <= 4K access case first, if the generic loop
         *        below gets too expensive and when we have iommuAmdWalkIoPageDirectory. */

        uint64_t uBaseIova   = uIova & X86_PAGE_4K_BASE_MASK;
        uint64_t offIova     = uIova & X86_PAGE_4K_OFFSET_MASK;
        uint64_t cbRemaining = cbAccess;
        for (;;)
        {
            /* Walk the I/O page tables to translate the IOVA and check permission for the access. */
            IOWALKRESULT WalkResult;
            rc = iommuAmdWalkIoPageTable(pDevIns, uDevId, uBaseIova, fAccess, &Dte, enmOp, &WalkResult);
            if (RT_SUCCESS(rc))
            {
                /** @todo IOMMU: Split large pages into 4K IOTLB entries and add to IOTLB cache. */

                /* Store the translated base address before continuing to check permissions for any more pages. */
                if (cbRemaining == cbAccess)
                {
                    RTGCPHYS const offSpa = ~(UINT64_C(0xffffffffffffffff) << WalkResult.cShift);
                    *pGCPhysSpa = WalkResult.GCPhysSpa | offSpa;
                }

                uint64_t const cbPhysPage = UINT64_C(1) << WalkResult.cShift;
                if (cbRemaining > cbPhysPage - offIova)
                {
                    cbRemaining -= (cbPhysPage - offIova);
                    uBaseIova   += cbPhysPage;
                    offIova      = 0;
                }
                else
                    break;
            }
            else
            {
                LogFunc(("I/O page table walk failed. uIova=%#RX64 uBaseIova=%#RX64 fAccess=%u rc=%Rrc\n", uIova,
                     uBaseIova, fAccess, rc));
                *pGCPhysSpa = NIL_RTGCPHYS;
                return rc;
            }
        }

        return rc;
    }

    LogFunc(("Failed to read device table entry. uDevId=%#x rc=%Rrc\n", uDevId, rc));
    return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
}


/**
 * Memory read request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uDevId      The device ID (bus, device, function).
 * @param   uIova       The I/O virtual address being read.
 * @param   cbRead      The number of bytes being read.
 * @param   pGCPhysSpa  Where to store the translated system physical address.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuAmdDeviceMemRead(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uIova, size_t cbRead,
                                               PRTGCPHYS pGCPhysSpa)
{
    /* Validate. */
    Assert(pDevIns);
    Assert(pGCPhysSpa);
    Assert(cbRead > 0);

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    LogFlowFunc(("uDevId=%#x uIova=%#RX64 cbRead=%u\n", uDevId, uIova, cbRead));

    /* Addresses are forwarded without translation when the IOMMU is disabled. */
    IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
    if (Ctrl.n.u1IommuEn)
    {
        /** @todo IOMMU: IOTLB cache lookup. */

        /* Lookup the IOVA from the device table. */
        return iommuAmdLookupDeviceTable(pDevIns, uDevId, uIova, cbRead, IOMMU_IO_PERM_READ, IOMMUOP_MEM_READ, pGCPhysSpa);
    }

    *pGCPhysSpa = uIova;
    return VINF_SUCCESS;
}


/**
 * Memory write request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uDevId      The device ID (bus, device, function).
 * @param   uIova       The I/O virtual address being written.
 * @param   cbWrite     The number of bytes being written.
 * @param   pGCPhysSpa  Where to store the translated physical address.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuAmdDeviceMemWrite(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uIova, size_t cbWrite,
                                                PRTGCPHYS pGCPhysSpa)
{
    /* Validate. */
    Assert(pDevIns);
    Assert(pGCPhysSpa);
    Assert(cbWrite > 0);

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    LogFlowFunc(("uDevId=%#x uIova=%#RX64 cbWrite=%u\n", uDevId, uIova, cbWrite));

    /* Addresses are forwarded without translation when the IOMMU is disabled. */
    IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
    if (Ctrl.n.u1IommuEn)
    {
        /** @todo IOMMU: IOTLB cache lookup. */

        /* Lookup the IOVA from the device table. */
        return iommuAmdLookupDeviceTable(pDevIns, uDevId, uIova, cbWrite, IOMMU_IO_PERM_WRITE, IOMMUOP_MEM_WRITE, pGCPhysSpa);
    }

    *pGCPhysSpa = uIova;
    return VINF_SUCCESS;
}


/**
 * Reads an interrupt remapping table entry from guest memory given its DTE.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uDevId      The device ID.
 * @param   pDte        The device table entry.
 * @param   GCPhysIn    The source MSI address.
 * @param   uDataIn     The source MSI data.
 * @param   enmOp       The IOMMU operation being performed.
 * @param   pIrte       Where to store the interrupt remapping table entry.
 *
 * @thread  Any.
 */
static int iommuAmdReadIrte(PPDMDEVINS pDevIns, uint16_t uDevId, PCDTE_T pDte, RTGCPHYS GCPhysIn, uint32_t uDataIn,
                            IOMMUOP enmOp, PIRTE_T pIrte)
{
    /* Ensure the IRTE length is valid. */
    Assert(pDte->n.u4IntrTableLength < IOMMU_DTE_INTR_TAB_LEN_MAX);

    RTGCPHYS const GCPhysIntrTable = pDte->au64[2] & IOMMU_DTE_IRTE_ROOT_PTR_MASK;
    uint16_t const cbIntrTable     = IOMMU_GET_INTR_TAB_LEN(pDte);
    uint16_t const offIrte         = (uDataIn & IOMMU_MSI_DATA_IRTE_OFFSET_MASK) * sizeof(IRTE_T);
    RTGCPHYS const GCPhysIrte      = GCPhysIntrTable + offIrte;

    /* Ensure the IRTE falls completely within the interrupt table. */
    if (offIrte + sizeof(IRTE_T) <= cbIntrTable)
    { /* likely */ }
    else
    {
        LogFunc(("IRTE exceeds table length (GCPhysIntrTable=%#RGp cbIntrTable=%u offIrte=%#x uDataIn=%#x) -> IOPF\n",
                 GCPhysIntrTable, cbIntrTable, offIrte, uDataIn));

        EVT_IO_PAGE_FAULT_T EvtIoPageFault;
        iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, GCPhysIn, false /* fPresent */, false /* fRsvdNotZero */,
                                     false /* fPermDenied */, enmOp, &EvtIoPageFault);
        iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, NULL /* pIrte */, enmOp, &EvtIoPageFault,
                                      kIoPageFaultType_IrteAddrInvalid);
        return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
    }

    /* Read the IRTE from memory. */
    Assert(!(GCPhysIrte & 3));
    int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhysIrte, pIrte, sizeof(*pIrte));
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    /** @todo The IOMMU spec. does not tell what kind of error is reported in this
     *        situation. Is it an I/O page fault or a device table hardware error?
     *        There's no interrupt table hardware error event, but it's unclear what
     *        we should do here. */
    LogFunc(("Failed to read interrupt table entry at %#RGp. rc=%Rrc -> ???\n", GCPhysIrte, rc));
    return VERR_IOMMU_IPE_4;
}


/**
 * Remaps the interrupt using the interrupt remapping table.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU instance data.
 * @param   uDevId      The device ID.
 * @param   pDte        The device table entry.
 * @param   enmOp       The IOMMU operation being performed.
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 *
 * @thread  Any.
 */
static int iommuAmdRemapIntr(PPDMDEVINS pDevIns, uint16_t uDevId, PCDTE_T pDte, IOMMUOP enmOp, PCMSIMSG pMsiIn,
                             PMSIMSG pMsiOut)
{
    Assert(pDte->n.u2IntrCtrl == IOMMU_INTR_CTRL_REMAP);

    IRTE_T Irte;
    int rc = iommuAmdReadIrte(pDevIns, uDevId, pDte, pMsiIn->Addr.u64, pMsiIn->Data.u32, enmOp, &Irte);
    if (RT_SUCCESS(rc))
    {
        if (Irte.n.u1RemapEnable)
        {
            if (!Irte.n.u1GuestMode)
            {
                if (Irte.n.u3IntrType <= VBOX_MSI_DELIVERY_MODE_LOWEST_PRIO)
                {
                    /* Preserve all bits from the source MSI address that don't map 1:1 from the IRTE. */
                    pMsiOut->Addr.u64 = pMsiIn->Addr.u64;
                    pMsiOut->Addr.n.u1DestMode = Irte.n.u1DestMode;
                    pMsiOut->Addr.n.u8DestId   = Irte.n.u8Dest;

                    /* Preserve all bits from the source MSI data that don't map 1:1 from the IRTE. */
                    pMsiOut->Data.u32 = pMsiIn->Data.u32;
                    pMsiOut->Data.n.u8Vector       = Irte.n.u8Vector;
                    pMsiOut->Data.n.u3DeliveryMode = Irte.n.u3IntrType;

                    return VINF_SUCCESS;
                }

                LogFunc(("Interrupt type (%#x) invalid -> IOPF\n", Irte.n.u3IntrType));
                EVT_IO_PAGE_FAULT_T EvtIoPageFault;
                iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, pMsiIn->Addr.u64, Irte.n.u1RemapEnable,
                                             true /* fRsvdNotZero */, false /* fPermDenied */, enmOp, &EvtIoPageFault);
                iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, &Irte, enmOp, &EvtIoPageFault, kIoPageFaultType_IrteRsvdIntType);
                return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
            }

            LogFunc(("Guest mode not supported -> IOPF\n"));
            EVT_IO_PAGE_FAULT_T EvtIoPageFault;
            iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, pMsiIn->Addr.u64, Irte.n.u1RemapEnable,
                                         true /* fRsvdNotZero */, false /* fPermDenied */, enmOp, &EvtIoPageFault);
            iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, &Irte, enmOp, &EvtIoPageFault, kIoPageFaultType_IrteRsvdNotZero);
            return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
        }

        LogFunc(("Remapping disabled -> IOPF\n"));
        EVT_IO_PAGE_FAULT_T EvtIoPageFault;
        iommuAmdInitIoPageFaultEvent(uDevId, pDte->n.u16DomainId, pMsiIn->Addr.u64, Irte.n.u1RemapEnable,
                                     false /* fRsvdNotZero */, false /* fPermDenied */, enmOp, &EvtIoPageFault);
        iommuAmdRaiseIoPageFaultEvent(pDevIns, pDte, &Irte, enmOp, &EvtIoPageFault, kIoPageFaultType_IrteRemapEn);
        return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
    }

    return rc;
}


/**
 * Looks up an MSI interrupt from the interrupt remapping table.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU instance data.
 * @param   uDevId      The device ID.
 * @param   enmOp       The IOMMU operation being performed.
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 *
 * @thread  Any.
 */
static int iommuAmdLookupIntrTable(PPDMDEVINS pDevIns, uint16_t uDevId, IOMMUOP enmOp, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    /* Read the device table entry from memory. */
    LogFlowFunc(("uDevId=%#x enmOp=%u\n", uDevId, enmOp));

    DTE_T Dte;
    int rc = iommuAmdReadDte(pDevIns, uDevId, enmOp, &Dte);
    if (RT_SUCCESS(rc))
    {
        /* If the DTE is not valid, all interrupts are forwarded without remapping. */
        if (Dte.n.u1IntrMapValid)
        {
            /* Validate bits 255:128 of the device table entry when DTE.IV is 1. */
            uint64_t const fRsvd0 = Dte.au64[2] & ~IOMMU_DTE_QWORD_2_VALID_MASK;
            uint64_t const fRsvd1 = Dte.au64[3] & ~IOMMU_DTE_QWORD_3_VALID_MASK;
            if (RT_LIKELY(   !fRsvd0
                          && !fRsvd1))
            { /* likely */ }
            else
            {
                LogFunc(("Invalid reserved bits in DTE (u64[2]=%#RX64 u64[3]=%#RX64) -> Illegal DTE\n", fRsvd0,
                     fRsvd1));
                EVT_ILLEGAL_DTE_T Event;
                iommuAmdInitIllegalDteEvent(uDevId, pMsiIn->Addr.u64, true /* fRsvdNotZero */, enmOp, &Event);
                iommuAmdRaiseIllegalDteEvent(pDevIns, enmOp, &Event, kIllegalDteType_RsvdNotZero);
                return VERR_IOMMU_INTR_REMAP_FAILED;
            }

            /*
             * LINT0/LINT1 pins cannot be driven by PCI(e) devices. Perhaps for a Southbridge
             * that's connected through HyperTransport it might be possible; but for us, it
             * doesn't seem we need to specially handle these pins.
             */

            /*
             * Validate the MSI source address.
             *
             * 64-bit MSIs are supported by the PCI and AMD IOMMU spec. However as far as the
             * CPU is concerned, the MSI region is fixed and we must ensure no other device
             * claims the region as I/O space.
             *
             * See PCI spec. 6.1.4. "Message Signaled Interrupt (MSI) Support".
             * See AMD IOMMU spec. 2.8 "IOMMU Interrupt Support".
             * See Intel spec. 10.11.1 "Message Address Register Format".
             */
            if ((pMsiIn->Addr.u64 & VBOX_MSI_ADDR_ADDR_MASK) == VBOX_MSI_ADDR_BASE)
            {
                /*
                 * The IOMMU remaps fixed and arbitrated interrupts using the IRTE.
                 * See AMD IOMMU spec. "2.2.5.1 Interrupt Remapping Tables, Guest Virtual APIC Not Enabled".
                 */
                uint8_t const u8DeliveryMode = pMsiIn->Data.n.u3DeliveryMode;
                bool fPassThru = false;
                switch (u8DeliveryMode)
                {
                    case VBOX_MSI_DELIVERY_MODE_FIXED:
                    case VBOX_MSI_DELIVERY_MODE_LOWEST_PRIO:
                    {
                        uint8_t const uIntrCtrl = Dte.n.u2IntrCtrl;
                        if (uIntrCtrl == IOMMU_INTR_CTRL_TARGET_ABORT)
                        {
                            LogFunc(("IntCtl=0: Target aborting fixed/arbitrated interrupt -> Target abort\n"));
                            iommuAmdSetPciTargetAbort(pDevIns);
                            return VERR_IOMMU_INTR_REMAP_DENIED;
                        }

                        if (uIntrCtrl == IOMMU_INTR_CTRL_FWD_UNMAPPED)
                        {
                            fPassThru = true;
                            break;
                        }

                        if (uIntrCtrl == IOMMU_INTR_CTRL_REMAP)
                        {
                            /* Validate the encoded interrupt table length when IntCtl specifies remapping. */
                            uint8_t const uIntrTabLen = Dte.n.u4IntrTableLength;
                            if (uIntrTabLen < IOMMU_DTE_INTR_TAB_LEN_MAX)
                            {
                                /*
                                 * We don't support guest interrupt remapping yet. When we do, we'll need to
                                 * check Ctrl.u1GstVirtApicEn and use the guest Virtual APIC Table Root Pointer
                                 * in the DTE rather than the Interrupt Root Table Pointer. Since the caller
                                 * already reads the control register, add that as a parameter when we eventually
                                 * support guest interrupt remapping. For now, just assert.
                                 */
                                PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
                                Assert(!pThis->ExtFeat.n.u1GstVirtApicSup);
                                NOREF(pThis);

                                return iommuAmdRemapIntr(pDevIns, uDevId, &Dte, enmOp, pMsiIn, pMsiOut);
                            }

                            LogFunc(("Invalid interrupt table length %#x -> Illegal DTE\n", uIntrTabLen));
                            EVT_ILLEGAL_DTE_T Event;
                            iommuAmdInitIllegalDteEvent(uDevId, pMsiIn->Addr.u64, false /* fRsvdNotZero */, enmOp, &Event);
                            iommuAmdRaiseIllegalDteEvent(pDevIns, enmOp, &Event, kIllegalDteType_RsvdIntTabLen);
                            return VERR_IOMMU_INTR_REMAP_FAILED;
                        }

                        /* Paranoia. */
                        Assert(uIntrCtrl == IOMMU_INTR_CTRL_RSVD);

                        LogFunc(("IntCtl mode invalid %#x -> Illegal DTE\n", uIntrCtrl));

                        EVT_ILLEGAL_DTE_T Event;
                        iommuAmdInitIllegalDteEvent(uDevId, pMsiIn->Addr.u64, true /* fRsvdNotZero */, enmOp, &Event);
                        iommuAmdRaiseIllegalDteEvent(pDevIns, enmOp, &Event, kIllegalDteType_RsvdIntCtl);
                        return VERR_IOMMU_INTR_REMAP_FAILED;
                    }

                    /* SMIs are passed through unmapped. We don't implement SMI filters. */
                    case VBOX_MSI_DELIVERY_MODE_SMI:        fPassThru = true;                   break;
                    case VBOX_MSI_DELIVERY_MODE_NMI:        fPassThru = Dte.n.u1NmiPassthru;    break;
                    case VBOX_MSI_DELIVERY_MODE_INIT:       fPassThru = Dte.n.u1InitPassthru;   break;
                    case VBOX_MSI_DELIVERY_MODE_EXT_INT:    fPassThru = Dte.n.u1ExtIntPassthru; break;
                    default:
                    {
                        LogFunc(("MSI data delivery mode invalid %#x -> Target abort\n", u8DeliveryMode));
                        iommuAmdSetPciTargetAbort(pDevIns);
                        return VERR_IOMMU_INTR_REMAP_FAILED;
                    }
                }

                if (fPassThru)
                {
                    *pMsiOut = *pMsiIn;
                    return VINF_SUCCESS;
                }

                iommuAmdSetPciTargetAbort(pDevIns);
                return VERR_IOMMU_INTR_REMAP_DENIED;
            }
            else
            {
                LogFunc(("MSI address region invalid %#RX64\n", pMsiIn->Addr.u64));
                return VERR_IOMMU_INTR_REMAP_FAILED;
            }
        }
        else
        {
            /** @todo IOMMU: Add to interrupt remapping cache. */
            LogFlowFunc(("DTE interrupt map not valid\n"));
            *pMsiOut = *pMsiIn;
            return VINF_SUCCESS;
        }
    }

    LogFunc(("Failed to read device table entry. uDevId=%#x rc=%Rrc\n", uDevId, rc));
    return VERR_IOMMU_INTR_REMAP_FAILED;
}


/**
 * Interrupt remap request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uDevId      The device ID (bus, device, function).
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 */
static DECLCALLBACK(int) iommuAmdDeviceMsiRemap(PPDMDEVINS pDevIns, uint16_t uDevId, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    /* Validate. */
    Assert(pDevIns);
    Assert(pMsiIn);
    Assert(pMsiOut);

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMsiRemap));

    LogFlowFunc(("uDevId=%#x\n", uDevId));

    /* Interrupts are forwarded with remapping when the IOMMU is disabled. */
    IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
    if (Ctrl.n.u1IommuEn)
    {
        /** @todo Cache? */

        return iommuAmdLookupIntrTable(pDevIns, uDevId, IOMMUOP_INTR_REQ, pMsiIn, pMsiOut);
    }

    *pMsiOut = *pMsiIn;
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(cb == 4 || cb == 8);
    Assert(!(off & (cb - 1)));

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioWrite)); NOREF(pThis);

    uint64_t const uValue = cb == 8 ? *(uint64_t const *)pv : *(uint32_t const *)pv;
    return iommuAmdWriteRegister(pDevIns, off, cb, uValue);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(cb == 4 || cb == 8);
    Assert(!(off & (cb - 1)));

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioRead)); NOREF(pThis);

    uint64_t uResult;
    VBOXSTRICTRC rcStrict = iommuAmdReadRegister(pDevIns, off, &uResult);
    if (cb == 8)
        *(uint64_t *)pv = uResult;
    else
        *(uint32_t *)pv = (uint32_t)uResult;

    return rcStrict;
}

# ifdef IN_RING3

/**
 * Processes an IOMMU command.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   pCmd            The command to process.
 * @param   GCPhysCmd       The system physical address of the command.
 * @param   pEvtError       Where to store the error event in case of failures.
 *
 * @thread  Command thread.
 */
static int iommuAmdR3ProcessCmd(PPDMDEVINS pDevIns, PCCMD_GENERIC_T pCmd, RTGCPHYS GCPhysCmd, PEVT_GENERIC_T pEvtError)
{
    IOMMU_ASSERT_NOT_LOCKED(pDevIns);

    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    STAM_COUNTER_INC(&pThis->StatCmd);

    uint8_t const bCmd = pCmd->n.u4Opcode;
    switch (bCmd)
    {
        case IOMMU_CMD_COMPLETION_WAIT:
        {
            STAM_COUNTER_INC(&pThis->StatCmdCompWait);

            PCCMD_COMWAIT_T pCmdComWait = (PCCMD_COMWAIT_T)pCmd;
            AssertCompile(sizeof(*pCmdComWait) == sizeof(*pCmd));

            /* Validate reserved bits in the command. */
            if (!(pCmdComWait->au64[0] & ~IOMMU_CMD_COM_WAIT_QWORD_0_VALID_MASK))
            {
                /* If Completion Store is requested, write the StoreData to the specified address. */
                if (pCmdComWait->n.u1Store)
                {
                    RTGCPHYS const GCPhysStore = RT_MAKE_U64(pCmdComWait->n.u29StoreAddrLo << 3, pCmdComWait->n.u20StoreAddrHi);
                    uint64_t const u64Data     = pCmdComWait->n.u64StoreData;
                    int rc = PDMDevHlpPCIPhysWrite(pDevIns, GCPhysStore, &u64Data, sizeof(u64Data));
                    if (RT_FAILURE(rc))
                    {
                        LogFunc(("Cmd(%#x): Failed to write StoreData (%#RX64) to %#RGp, rc=%Rrc\n", bCmd, u64Data,
                             GCPhysStore, rc));
                        iommuAmdInitCmdHwErrorEvent(GCPhysStore, (PEVT_CMD_HW_ERR_T)pEvtError);
                        return VERR_IOMMU_CMD_HW_ERROR;
                    }
                }

                /* If the command requests an interrupt and completion wait interrupts are enabled, raise it. */
                if (pCmdComWait->n.u1Interrupt)
                {
                    IOMMU_LOCK(pDevIns);
                    ASMAtomicOrU64(&pThis->Status.u64, IOMMU_STATUS_COMPLETION_WAIT_INTR);
                    IOMMU_CTRL_T const Ctrl = iommuAmdGetCtrl(pThis);
                    bool const fRaiseInt = Ctrl.n.u1CompWaitIntrEn;
                    IOMMU_UNLOCK(pDevIns);

                    if (fRaiseInt)
                        iommuAmdRaiseMsiInterrupt(pDevIns);
                }
                return VINF_SUCCESS;
            }
            iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
            return VERR_IOMMU_CMD_INVALID_FORMAT;
        }

        case IOMMU_CMD_INV_DEV_TAB_ENTRY:
        {
            /** @todo IOMMU: Implement this once we implement IOTLB. Pretend success until
             *        then. */
            STAM_COUNTER_INC(&pThis->StatCmdInvDte);
            return VINF_SUCCESS;
        }

        case IOMMU_CMD_INV_IOMMU_PAGES:
        {
            /** @todo IOMMU: Implement this once we implement IOTLB. Pretend success until
             *        then. */
            STAM_COUNTER_INC(&pThis->StatCmdInvIommuPages);
            return VINF_SUCCESS;
        }

        case IOMMU_CMD_INV_IOTLB_PAGES:
        {
            STAM_COUNTER_INC(&pThis->StatCmdInvIotlbPages);

            uint32_t const uCapHdr = PDMPciDevGetDWord(pDevIns->apPciDevs[0], IOMMU_PCI_OFF_CAP_HDR);
            if (RT_BF_GET(uCapHdr, IOMMU_BF_CAPHDR_IOTLB_SUP))
            {
                /** @todo IOMMU: Implement remote IOTLB invalidation. */
                return VERR_NOT_IMPLEMENTED;
            }
            iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
            return VERR_IOMMU_CMD_NOT_SUPPORTED;
        }

        case IOMMU_CMD_INV_INTR_TABLE:
        {
            /** @todo IOMMU: Implement this once we implement IOTLB. Pretend success until
             *        then. */
            STAM_COUNTER_INC(&pThis->StatCmdInvIntrTable);
            return VINF_SUCCESS;
        }

        case IOMMU_CMD_PREFETCH_IOMMU_PAGES:
        {
            STAM_COUNTER_INC(&pThis->StatCmdPrefIommuPages);
            if (pThis->ExtFeat.n.u1PrefetchSup)
            {
                /** @todo IOMMU: Implement prefetch. Pretend success until then. */
                return VINF_SUCCESS;
            }
            iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
            return VERR_IOMMU_CMD_NOT_SUPPORTED;
        }

        case IOMMU_CMD_COMPLETE_PPR_REQ:
        {
            STAM_COUNTER_INC(&pThis->StatCmdCompletePprReq);

            /* We don't support PPR requests yet. */
            Assert(!pThis->ExtFeat.n.u1PprSup);
            iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
            return VERR_IOMMU_CMD_NOT_SUPPORTED;
        }

        case IOMMU_CMD_INV_IOMMU_ALL:
        {
            STAM_COUNTER_INC(&pThis->StatCmdInvIommuAll);

            if (pThis->ExtFeat.n.u1InvAllSup)
            {
                /** @todo IOMMU: Invalidate all. Pretend success until then. */
                return VINF_SUCCESS;
            }
            iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
            return VERR_IOMMU_CMD_NOT_SUPPORTED;
        }
    }

    STAM_COUNTER_DEC(&pThis->StatCmd);
    LogFunc(("Cmd(%#x): Unrecognized\n", bCmd));
    iommuAmdInitIllegalCmdEvent(GCPhysCmd, (PEVT_ILLEGAL_CMD_ERR_T)pEvtError);
    return VERR_IOMMU_CMD_NOT_SUPPORTED;
}


/**
 * The IOMMU command thread.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pThread     The command thread.
 */
static DECLCALLBACK(int) iommuAmdR3CmdThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /*
         * Sleep perpetually until we are woken up to process commands.
         */
        {
            ASMAtomicWriteBool(&pThis->fCmdThreadSleeping, true);
            bool fSignaled = ASMAtomicXchgBool(&pThis->fCmdThreadSignaled, false);
            if (!fSignaled)
            {
                Assert(ASMAtomicReadBool(&pThis->fCmdThreadSleeping));
                int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->hEvtCmdThread, RT_INDEFINITE_WAIT);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    break;
                Log5Func(("Woken up with rc=%Rrc\n", rc));
                ASMAtomicWriteBool(&pThis->fCmdThreadSignaled, false);
            }
            ASMAtomicWriteBool(&pThis->fCmdThreadSleeping, false);
        }

        /*
         * Fetch and process IOMMU commands.
         */
        /** @todo r=ramshankar: This employs a simplistic method of fetching commands (one
         *        at a time) and is expensive due to calls to PGM for fetching guest memory.
         *        We could optimize by fetching a bunch of commands at a time reducing
         *        number of calls to PGM. In the longer run we could lock the memory and
         *        mappings and accessing them directly. */
        IOMMU_LOCK(pDevIns);

        IOMMU_STATUS_T const Status = iommuAmdGetStatus(pThis);
        if (Status.n.u1CmdBufRunning)
        {
            /* Get the offset we need to read the command from memory (circular buffer offset). */
            uint32_t const cbCmdBuf = iommuAmdGetTotalBufLength(pThis->CmdBufBaseAddr.n.u4Len);
            uint32_t offHead = pThis->CmdBufHeadPtr.n.off;
            Assert(!(offHead & ~IOMMU_CMD_BUF_HEAD_PTR_VALID_MASK));
            Assert(offHead < cbCmdBuf);
            while (offHead != pThis->CmdBufTailPtr.n.off)
            {
                /* Read the command from memory. */
                CMD_GENERIC_T Cmd;
                RTGCPHYS const GCPhysCmd = (pThis->CmdBufBaseAddr.n.u40Base << X86_PAGE_4K_SHIFT) + offHead;
                int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhysCmd, &Cmd, sizeof(Cmd));
                if (RT_SUCCESS(rc))
                {
                    /* Increment the command buffer head pointer. */
                    offHead = (offHead + sizeof(CMD_GENERIC_T)) % cbCmdBuf;
                    pThis->CmdBufHeadPtr.n.off = offHead;

                    /* Process the fetched command. */
                    EVT_GENERIC_T EvtError;
                    IOMMU_UNLOCK(pDevIns);
                    rc = iommuAmdR3ProcessCmd(pDevIns, &Cmd, GCPhysCmd, &EvtError);
                    IOMMU_LOCK(pDevIns);
                    if (RT_FAILURE(rc))
                    {
                        if (   rc == VERR_IOMMU_CMD_NOT_SUPPORTED
                            || rc == VERR_IOMMU_CMD_INVALID_FORMAT)
                        {
                            Assert(EvtError.n.u4EvtCode == IOMMU_EVT_ILLEGAL_CMD_ERROR);
                            iommuAmdRaiseIllegalCmdEvent(pDevIns, (PCEVT_ILLEGAL_CMD_ERR_T)&EvtError);
                        }
                        else if (rc == VERR_IOMMU_CMD_HW_ERROR)
                        {
                            Assert(EvtError.n.u4EvtCode == IOMMU_EVT_COMMAND_HW_ERROR);
                            LogFunc(("Raising command hardware error. Cmd=%#x -> COMMAND_HW_ERROR\n", Cmd.n.u4Opcode));
                            iommuAmdRaiseCmdHwErrorEvent(pDevIns, (PCEVT_CMD_HW_ERR_T)&EvtError);
                        }
                        break;
                    }
                }
                else
                {
                    LogFunc(("Failed to read command at %#RGp. rc=%Rrc -> COMMAND_HW_ERROR\n", GCPhysCmd, rc));
                    EVT_CMD_HW_ERR_T EvtCmdHwErr;
                    iommuAmdInitCmdHwErrorEvent(GCPhysCmd, &EvtCmdHwErr);
                    iommuAmdRaiseCmdHwErrorEvent(pDevIns, &EvtCmdHwErr);
                    break;
                }
            }
        }

        IOMMU_UNLOCK(pDevIns);
    }

    LogFlowFunc(("Command thread terminating\n"));
    return VINF_SUCCESS;
}


/**
 * Wakes up the command thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pThread     The command thread.
 */
static DECLCALLBACK(int) iommuAmdR3CmdThreadWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    LogFlowFunc(("\n"));
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    return PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEvtCmdThread);
}


/**
 * @callback_method_impl{FNPCICONFIGREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdR3PciConfigRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t uAddress,
                                                          unsigned cb, uint32_t *pu32Value)
{
    /** @todo IOMMU: PCI config read stat counter. */
    VBOXSTRICTRC rcStrict = PDMDevHlpPCIConfigRead(pDevIns, pPciDev, uAddress, cb, pu32Value);
    Log3Func(("uAddress=%#x (cb=%u) -> %#x. rc=%Rrc\n", uAddress, cb, *pu32Value, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * @callback_method_impl{FNPCICONFIGWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdR3PciConfigWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t uAddress,
                                                           unsigned cb, uint32_t u32Value)
{
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);

    /*
     * Discard writes to read-only registers that are specific to the IOMMU.
     * Other common PCI registers are handled by the generic code, see devpciR3IsConfigByteWritable().
     * See PCI spec. 6.1. "Configuration Space Organization".
     */
    switch (uAddress)
    {
        case IOMMU_PCI_OFF_CAP_HDR:         /* All bits are read-only. */
        case IOMMU_PCI_OFF_RANGE_REG:       /* We don't have any devices integrated with the IOMMU. */
        case IOMMU_PCI_OFF_MISCINFO_REG_0:  /* We don't support MSI-X. */
        case IOMMU_PCI_OFF_MISCINFO_REG_1:  /* We don't support guest-address translation. */
        {
            LogFunc(("PCI config write (%#RX32) to read-only register %#x -> Ignored\n", u32Value, uAddress));
            return VINF_SUCCESS;
        }
    }

    IOMMU_LOCK(pDevIns);

    VBOXSTRICTRC rcStrict = VERR_IOMMU_IPE_3;
    switch (uAddress)
    {
        case IOMMU_PCI_OFF_BASE_ADDR_REG_LO:
        {
            if (pThis->IommuBar.n.u1Enable)
            {
                rcStrict = VINF_SUCCESS;
                LogFunc(("Writing Base Address (Lo) when it's already enabled -> Ignored\n"));
                break;
            }

            pThis->IommuBar.au32[0] = u32Value & IOMMU_BAR_VALID_MASK;
            if (pThis->IommuBar.n.u1Enable)
            {
                Assert(pThis->hMmio != NIL_IOMMMIOHANDLE);  /* Paranoia. Ensure we have a valid IOM MMIO handle. */
                Assert(!pThis->ExtFeat.n.u1PerfCounterSup); /* Base is 16K aligned when performance counters aren't supported. */
                RTGCPHYS const GCPhysMmioBase     = RT_MAKE_U64(pThis->IommuBar.au32[0] & 0xffffc000, pThis->IommuBar.au32[1]);
                RTGCPHYS const GCPhysMmioBasePrev = PDMDevHlpMmioGetMappingAddress(pDevIns, pThis->hMmio);

                /* If the MMIO region is already mapped at the specified address, we're done. */
                Assert(GCPhysMmioBase != NIL_RTGCPHYS);
                if (GCPhysMmioBasePrev == GCPhysMmioBase)
                {
                    rcStrict = VINF_SUCCESS;
                    break;
                }

                /* Unmap the previous MMIO region (which is at a different address). */
                if (GCPhysMmioBasePrev != NIL_RTGCPHYS)
                {
                    LogFlowFunc(("Unmapping previous MMIO region at %#RGp\n", GCPhysMmioBasePrev));
                    rcStrict = PDMDevHlpMmioUnmap(pDevIns, pThis->hMmio);
                    if (RT_FAILURE(rcStrict))
                    {
                        LogFunc(("Failed to unmap MMIO region at %#RGp. rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
                        break;
                    }
                }

                /* Map the newly specified MMIO region. */
                LogFlowFunc(("Mapping MMIO region at %#RGp\n", GCPhysMmioBase));
                rcStrict = PDMDevHlpMmioMap(pDevIns, pThis->hMmio, GCPhysMmioBase);
                if (RT_FAILURE(rcStrict))
                {
                    LogFunc(("Failed to unmap MMIO region at %#RGp. rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
                    break;
                }
            }
            else
                rcStrict = VINF_SUCCESS;
            break;
        }

        case IOMMU_PCI_OFF_BASE_ADDR_REG_HI:
        {
            if (!pThis->IommuBar.n.u1Enable)
                pThis->IommuBar.au32[1] = u32Value;
            else
            {
                rcStrict = VINF_SUCCESS;
                LogFunc(("Writing Base Address (Hi) when it's already enabled -> Ignored\n"));
            }
            break;
        }

        case IOMMU_PCI_OFF_MSI_CAP_HDR:
        {
            u32Value |= RT_BIT(23);     /* 64-bit MSI addressess must always be enabled for IOMMU. */
            RT_FALL_THRU();
        }
        default:
        {
            rcStrict = PDMDevHlpPCIConfigWrite(pDevIns, pPciDev, uAddress, cb, u32Value);
            break;
        }
    }

    IOMMU_UNLOCK(pDevIns);

    Log3Func(("uAddress=%#x (cb=%u) with %#x. rc=%Rrc\n", uAddress, cb, u32Value, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) iommuAmdR3DbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCIOMMU    pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PCPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    bool fVerbose;
    if (   pszArgs
        && !strncmp(pszArgs, RT_STR_TUPLE("verbose")))
        fVerbose = true;
    else
        fVerbose = false;

    pHlp->pfnPrintf(pHlp, "AMD-IOMMU:\n");
    /* Device Table Base Addresses (all segments). */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aDevTabBaseAddrs); i++)
    {
        DEV_TAB_BAR_T const DevTabBar = pThis->aDevTabBaseAddrs[i];
        pHlp->pfnPrintf(pHlp, "  Device Table BAR %u                      = %#RX64\n", i, DevTabBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Size                                    = %#x (%u bytes)\n", DevTabBar.n.u9Size,
                            IOMMU_GET_DEV_TAB_LEN(&DevTabBar));
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", DevTabBar.n.u40Base << X86_PAGE_4K_SHIFT);
        }
    }
    /* Command Buffer Base Address Register. */
    {
        CMD_BUF_BAR_T const CmdBufBar = pThis->CmdBufBaseAddr;
        uint8_t const  uEncodedLen = CmdBufBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  Command Buffer BAR                      = %#RX64\n", CmdBufBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", CmdBufBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* Event Log Base Address Register. */
    {
        EVT_LOG_BAR_T const EvtLogBar = pThis->EvtLogBaseAddr;
        uint8_t const  uEncodedLen = EvtLogBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  Event Log BAR                           = %#RX64\n", EvtLogBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", EvtLogBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* IOMMU Control Register. */
    {
        IOMMU_CTRL_T const Ctrl = pThis->Ctrl;
        pHlp->pfnPrintf(pHlp, "  Control                                 = %#RX64\n", Ctrl.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    IOMMU enable                            = %RTbool\n", Ctrl.n.u1IommuEn);
            pHlp->pfnPrintf(pHlp, "    HT Tunnel translation enable            = %RTbool\n", Ctrl.n.u1HtTunEn);
            pHlp->pfnPrintf(pHlp, "    Event log enable                        = %RTbool\n", Ctrl.n.u1EvtLogEn);
            pHlp->pfnPrintf(pHlp, "    Event log interrupt enable              = %RTbool\n", Ctrl.n.u1EvtIntrEn);
            pHlp->pfnPrintf(pHlp, "    Completion wait interrupt enable        = %RTbool\n", Ctrl.n.u1EvtIntrEn);
            pHlp->pfnPrintf(pHlp, "    Invalidation timeout                    = %u\n",      Ctrl.n.u3InvTimeOut);
            pHlp->pfnPrintf(pHlp, "    Pass posted write                       = %RTbool\n", Ctrl.n.u1PassPW);
            pHlp->pfnPrintf(pHlp, "    Respose Pass posted write               = %RTbool\n", Ctrl.n.u1ResPassPW);
            pHlp->pfnPrintf(pHlp, "    Coherent                                = %RTbool\n", Ctrl.n.u1Coherent);
            pHlp->pfnPrintf(pHlp, "    Isochronous                             = %RTbool\n", Ctrl.n.u1Isoc);
            pHlp->pfnPrintf(pHlp, "    Command buffer enable                   = %RTbool\n", Ctrl.n.u1CmdBufEn);
            pHlp->pfnPrintf(pHlp, "    PPR log enable                          = %RTbool\n", Ctrl.n.u1PprLogEn);
            pHlp->pfnPrintf(pHlp, "    PPR interrupt enable                    = %RTbool\n", Ctrl.n.u1PprIntrEn);
            pHlp->pfnPrintf(pHlp, "    PPR enable                              = %RTbool\n", Ctrl.n.u1PprEn);
            pHlp->pfnPrintf(pHlp, "    Guest translation eanble                = %RTbool\n", Ctrl.n.u1GstTranslateEn);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC enable               = %RTbool\n", Ctrl.n.u1GstVirtApicEn);
            pHlp->pfnPrintf(pHlp, "    CRW                                     = %#x\n",     Ctrl.n.u4Crw);
            pHlp->pfnPrintf(pHlp, "    SMI filter enable                       = %RTbool\n", Ctrl.n.u1SmiFilterEn);
            pHlp->pfnPrintf(pHlp, "    Self-writeback disable                  = %RTbool\n", Ctrl.n.u1SelfWriteBackDis);
            pHlp->pfnPrintf(pHlp, "    SMI filter log enable                   = %RTbool\n", Ctrl.n.u1SmiFilterLogEn);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC mode enable          = %#x\n",     Ctrl.n.u3GstVirtApicModeEn);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC GA log enable        = %RTbool\n", Ctrl.n.u1GstLogEn);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC interrupt enable     = %RTbool\n", Ctrl.n.u1GstIntrEn);
            pHlp->pfnPrintf(pHlp, "    Dual PPR log enable                     = %#x\n",     Ctrl.n.u2DualPprLogEn);
            pHlp->pfnPrintf(pHlp, "    Dual event log enable                   = %#x\n",     Ctrl.n.u2DualEvtLogEn);
            pHlp->pfnPrintf(pHlp, "    Device table segmentation enable        = %#x\n",     Ctrl.n.u3DevTabSegEn);
            pHlp->pfnPrintf(pHlp, "    Privilege abort enable                  = %#x\n",     Ctrl.n.u2PrivAbortEn);
            pHlp->pfnPrintf(pHlp, "    PPR auto response enable                = %RTbool\n", Ctrl.n.u1PprAutoRespEn);
            pHlp->pfnPrintf(pHlp, "    MARC enable                             = %RTbool\n", Ctrl.n.u1MarcEn);
            pHlp->pfnPrintf(pHlp, "    Block StopMark enable                   = %RTbool\n", Ctrl.n.u1BlockStopMarkEn);
            pHlp->pfnPrintf(pHlp, "    PPR auto response always-on enable      = %RTbool\n", Ctrl.n.u1PprAutoRespAlwaysOnEn);
            pHlp->pfnPrintf(pHlp, "    Domain IDPNE                            = %RTbool\n", Ctrl.n.u1DomainIDPNE);
            pHlp->pfnPrintf(pHlp, "    Enhanced PPR handling                   = %RTbool\n", Ctrl.n.u1EnhancedPpr);
            pHlp->pfnPrintf(pHlp, "    Host page table access/dirty bit update = %#x\n",     Ctrl.n.u2HstAccDirtyBitUpdate);
            pHlp->pfnPrintf(pHlp, "    Guest page table dirty bit disable      = %RTbool\n", Ctrl.n.u1GstDirtyUpdateDis);
            pHlp->pfnPrintf(pHlp, "    x2APIC enable                           = %RTbool\n", Ctrl.n.u1X2ApicEn);
            pHlp->pfnPrintf(pHlp, "    x2APIC interrupt enable                 = %RTbool\n", Ctrl.n.u1X2ApicIntrGenEn);
            pHlp->pfnPrintf(pHlp, "    Guest page table access bit update      = %RTbool\n", Ctrl.n.u1GstAccessUpdateDis);
        }
    }
    /* Exclusion Base Address Register. */
    {
        IOMMU_EXCL_RANGE_BAR_T const ExclRangeBar = pThis->ExclRangeBaseAddr;
        pHlp->pfnPrintf(pHlp, "  Exclusion BAR                           = %#RX64\n", ExclRangeBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Exclusion enable                        = %RTbool\n", ExclRangeBar.n.u1ExclEnable);
            pHlp->pfnPrintf(pHlp, "    Allow all devices                       = %RTbool\n", ExclRangeBar.n.u1AllowAll);
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n",
                            ExclRangeBar.n.u40ExclRangeBase << X86_PAGE_4K_SHIFT);
        }
    }
    /* Exclusion Range Limit Register. */
    {
        IOMMU_EXCL_RANGE_LIMIT_T const ExclRangeLimit = pThis->ExclRangeLimit;
        pHlp->pfnPrintf(pHlp, "  Exclusion Range Limit                   = %#RX64\n", ExclRangeLimit.u64);
        if (fVerbose)
            pHlp->pfnPrintf(pHlp, "    Range limit                             = %#RX64\n", ExclRangeLimit.n.u52ExclLimit);
    }
    /* Extended Feature Register. */
    {
        IOMMU_EXT_FEAT_T ExtFeat = pThis->ExtFeat;
        pHlp->pfnPrintf(pHlp, "  Extended Feature Register               = %#RX64\n", ExtFeat.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Prefetch support                        = %RTbool\n", ExtFeat.n.u1PrefetchSup);
            pHlp->pfnPrintf(pHlp, "    PPR support                             = %RTbool\n",  ExtFeat.n.u1PprSup);
            pHlp->pfnPrintf(pHlp, "    x2APIC support                          = %RTbool\n",  ExtFeat.n.u1X2ApicSup);
            pHlp->pfnPrintf(pHlp, "    NX and privilege level support          = %RTbool\n",  ExtFeat.n.u1NoExecuteSup);
            pHlp->pfnPrintf(pHlp, "    Guest translation support               = %RTbool\n",  ExtFeat.n.u1GstTranslateSup);
            pHlp->pfnPrintf(pHlp, "    Invalidate-All command support          = %RTbool\n",  ExtFeat.n.u1InvAllSup);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC support              = %RTbool\n",  ExtFeat.n.u1GstVirtApicSup);
            pHlp->pfnPrintf(pHlp, "    Hardware error register support         = %RTbool\n",  ExtFeat.n.u1HwErrorSup);
            pHlp->pfnPrintf(pHlp, "    Performance counters support            = %RTbool\n",  ExtFeat.n.u1PerfCounterSup);
            pHlp->pfnPrintf(pHlp, "    Host address translation size           = %#x\n",      ExtFeat.n.u2HostAddrTranslateSize);
            pHlp->pfnPrintf(pHlp, "    Guest address translation size          = %#x\n",      ExtFeat.n.u2GstAddrTranslateSize);
            pHlp->pfnPrintf(pHlp, "    Guest CR3 root table level support      = %#x\n",      ExtFeat.n.u2GstCr3RootTblLevel);
            pHlp->pfnPrintf(pHlp, "    SMI filter register support             = %#x\n",      ExtFeat.n.u2SmiFilterSup);
            pHlp->pfnPrintf(pHlp, "    SMI filter register count               = %#x\n",      ExtFeat.n.u3SmiFilterCount);
            pHlp->pfnPrintf(pHlp, "    Guest virtual-APIC modes support        = %#x\n",      ExtFeat.n.u3GstVirtApicModeSup);
            pHlp->pfnPrintf(pHlp, "    Dual PPR log support                    = %#x\n",      ExtFeat.n.u2DualPprLogSup);
            pHlp->pfnPrintf(pHlp, "    Dual event log support                  = %#x\n",      ExtFeat.n.u2DualEvtLogSup);
            pHlp->pfnPrintf(pHlp, "    Maximum PASID                           = %#x\n",      ExtFeat.n.u5MaxPasidSup);
            pHlp->pfnPrintf(pHlp, "    User/supervisor page protection support = %RTbool\n",  ExtFeat.n.u1UserSupervisorSup);
            pHlp->pfnPrintf(pHlp, "    Device table segments supported         = %#x (%u)\n", ExtFeat.n.u2DevTabSegSup,
                            g_acDevTabSegs[ExtFeat.n.u2DevTabSegSup]);
            pHlp->pfnPrintf(pHlp, "    PPR log overflow early warning support  = %RTbool\n",  ExtFeat.n.u1PprLogOverflowWarn);
            pHlp->pfnPrintf(pHlp, "    PPR auto response support               = %RTbool\n",  ExtFeat.n.u1PprAutoRespSup);
            pHlp->pfnPrintf(pHlp, "    MARC support                            = %#x\n",      ExtFeat.n.u2MarcSup);
            pHlp->pfnPrintf(pHlp, "    Block StopMark message support          = %RTbool\n",  ExtFeat.n.u1BlockStopMarkSup);
            pHlp->pfnPrintf(pHlp, "    Performance optimization support        = %RTbool\n",  ExtFeat.n.u1PerfOptSup);
            pHlp->pfnPrintf(pHlp, "    MSI capability MMIO access support      = %RTbool\n",  ExtFeat.n.u1MsiCapMmioSup);
            pHlp->pfnPrintf(pHlp, "    Guest I/O protection support            = %RTbool\n",  ExtFeat.n.u1GstIoSup);
            pHlp->pfnPrintf(pHlp, "    Host access support                     = %RTbool\n",  ExtFeat.n.u1HostAccessSup);
            pHlp->pfnPrintf(pHlp, "    Enhanced PPR handling support           = %RTbool\n",  ExtFeat.n.u1EnhancedPprSup);
            pHlp->pfnPrintf(pHlp, "    Attribute forward supported             = %RTbool\n",  ExtFeat.n.u1AttrForwardSup);
            pHlp->pfnPrintf(pHlp, "    Host dirty support                      = %RTbool\n",  ExtFeat.n.u1HostDirtySup);
            pHlp->pfnPrintf(pHlp, "    Invalidate IOTLB type support           = %RTbool\n",  ExtFeat.n.u1InvIoTlbTypeSup);
            pHlp->pfnPrintf(pHlp, "    Guest page table access bit hw disable  = %RTbool\n",  ExtFeat.n.u1GstUpdateDisSup);
            pHlp->pfnPrintf(pHlp, "    Force physical dest for remapped intr.  = %RTbool\n",  ExtFeat.n.u1ForcePhysDstSup);
        }
    }
    /* PPR Log Base Address Register. */
    {
        PPR_LOG_BAR_T PprLogBar = pThis->PprLogBaseAddr;
        uint8_t const  uEncodedLen = PprLogBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  PPR Log BAR                             = %#RX64\n",   PprLogBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", PprLogBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* Hardware Event (Hi) Register. */
    {
        IOMMU_HW_EVT_HI_T HwEvtHi = pThis->HwEvtHi;
        pHlp->pfnPrintf(pHlp, "  Hardware Event (Hi)                     = %#RX64\n",   HwEvtHi.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    First operand                           = %#RX64\n", HwEvtHi.n.u60FirstOperand);
            pHlp->pfnPrintf(pHlp, "    Event code                              = %#RX8\n",  HwEvtHi.n.u4EvtCode);
        }
    }
    /* Hardware Event (Lo) Register. */
    pHlp->pfnPrintf(pHlp, "  Hardware Event (Lo)                     = %#RX64\n", pThis->HwEvtLo);
    /* Hardware Event Status. */
    {
        IOMMU_HW_EVT_STATUS_T HwEvtStatus = pThis->HwEvtStatus;
        pHlp->pfnPrintf(pHlp, "  Hardware Event Status                   = %#RX64\n",    HwEvtStatus.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Valid                                   = %RTbool\n", HwEvtStatus.n.u1Valid);
            pHlp->pfnPrintf(pHlp, "    Overflow                                = %RTbool\n", HwEvtStatus.n.u1Overflow);
        }
    }
    /* Guest Virtual-APIC Log Base Address Register. */
    {
        GALOG_BAR_T const GALogBar = pThis->GALogBaseAddr;
        uint8_t const  uEncodedLen = GALogBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  Guest Log BAR                           = %#RX64\n",    GALogBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", GALogBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* Guest Virtual-APIC Log Tail Address Register. */
    {
        GALOG_TAIL_ADDR_T GALogTail = pThis->GALogTailAddr;
        pHlp->pfnPrintf(pHlp, "  Guest Log Tail Address                  = %#RX64\n",   GALogTail.u64);
        if (fVerbose)
            pHlp->pfnPrintf(pHlp, "    Tail address                            = %#RX64\n", GALogTail.n.u40GALogTailAddr);
    }
    /* PPR Log B Base Address Register. */
    {
        PPR_LOG_B_BAR_T PprLogBBar = pThis->PprLogBBaseAddr;
        uint8_t const uEncodedLen  = PprLogBBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  PPR Log B BAR                           = %#RX64\n",   PprLogBBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", PprLogBBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* Event Log B Base Address Register. */
    {
        EVT_LOG_B_BAR_T EvtLogBBar = pThis->EvtLogBBaseAddr;
        uint8_t const  uEncodedLen = EvtLogBBar.n.u4Len;
        uint32_t const cEntries    = iommuAmdGetBufMaxEntries(uEncodedLen);
        uint32_t const cbBuffer    = iommuAmdGetTotalBufLength(uEncodedLen);
        pHlp->pfnPrintf(pHlp, "  Event Log B BAR                         = %#RX64\n",   EvtLogBBar.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Base address                            = %#RX64\n", EvtLogBBar.n.u40Base << X86_PAGE_4K_SHIFT);
            pHlp->pfnPrintf(pHlp, "    Length                                  = %u (%u entries, %u bytes)\n", uEncodedLen,
                            cEntries, cbBuffer);
        }
    }
    /* Device-Specific Feature Extension Register. */
    {
        DEV_SPECIFIC_FEAT_T const DevSpecificFeat = pThis->DevSpecificFeat;
        pHlp->pfnPrintf(pHlp, "  Device-specific Feature                 = %#RX64\n",   DevSpecificFeat.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Feature                                 = %#RX32\n", DevSpecificFeat.n.u24DevSpecFeat);
            pHlp->pfnPrintf(pHlp, "    Minor revision ID                       = %#x\n",    DevSpecificFeat.n.u4RevMinor);
            pHlp->pfnPrintf(pHlp, "    Major revision ID                       = %#x\n",    DevSpecificFeat.n.u4RevMajor);
        }
    }
    /* Device-Specific Control Extension Register. */
    {
        DEV_SPECIFIC_CTRL_T const DevSpecificCtrl = pThis->DevSpecificCtrl;
        pHlp->pfnPrintf(pHlp, "  Device-specific Control                 = %#RX64\n",   DevSpecificCtrl.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Control                                 = %#RX32\n", DevSpecificCtrl.n.u24DevSpecCtrl);
            pHlp->pfnPrintf(pHlp, "    Minor revision ID                       = %#x\n",    DevSpecificCtrl.n.u4RevMinor);
            pHlp->pfnPrintf(pHlp, "    Major revision ID                       = %#x\n",    DevSpecificCtrl.n.u4RevMajor);
        }
    }
    /* Device-Specific Status Extension Register. */
    {
        DEV_SPECIFIC_STATUS_T const DevSpecificStatus = pThis->DevSpecificStatus;
        pHlp->pfnPrintf(pHlp, "  Device-specific Status                  = %#RX64\n",   DevSpecificStatus.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Status                                  = %#RX32\n", DevSpecificStatus.n.u24DevSpecStatus);
            pHlp->pfnPrintf(pHlp, "    Minor revision ID                       = %#x\n",    DevSpecificStatus.n.u4RevMinor);
            pHlp->pfnPrintf(pHlp, "    Major revision ID                       = %#x\n",    DevSpecificStatus.n.u4RevMajor);
        }
    }
    /* Miscellaneous Information Register (Lo and Hi). */
    {
        MSI_MISC_INFO_T const MiscInfo = pThis->MiscInfo;
        pHlp->pfnPrintf(pHlp, "  Misc. Info. Register                    = %#RX64\n",    MiscInfo.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Event Log MSI number                    = %#x\n",     MiscInfo.n.u5MsiNumEvtLog);
            pHlp->pfnPrintf(pHlp, "    Guest Virtual-Address Size              = %#x\n",     MiscInfo.n.u3GstVirtAddrSize);
            pHlp->pfnPrintf(pHlp, "    Physical Address Size                   = %#x\n",     MiscInfo.n.u7PhysAddrSize);
            pHlp->pfnPrintf(pHlp, "    Virtual-Address Size                    = %#x\n",     MiscInfo.n.u7VirtAddrSize);
            pHlp->pfnPrintf(pHlp, "    HT Transport ATS Range Reserved         = %RTbool\n", MiscInfo.n.u1HtAtsResv);
            pHlp->pfnPrintf(pHlp, "    PPR MSI number                          = %#x\n",     MiscInfo.n.u5MsiNumPpr);
            pHlp->pfnPrintf(pHlp, "    GA Log MSI number                       = %#x\n",     MiscInfo.n.u5MsiNumGa);
        }
    }
    /* MSI Capability Header. */
    {
        MSI_CAP_HDR_T MsiCapHdr;
        MsiCapHdr.u32 = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_CAP_HDR);
        pHlp->pfnPrintf(pHlp, "  MSI Capability Header                   = %#RX32\n",    MsiCapHdr.u32);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Capability ID                           = %#x\n",     MsiCapHdr.n.u8MsiCapId);
            pHlp->pfnPrintf(pHlp, "    Capability Ptr (PCI config offset)      = %#x\n",     MsiCapHdr.n.u8MsiCapPtr);
            pHlp->pfnPrintf(pHlp, "    Enable                                  = %RTbool\n", MsiCapHdr.n.u1MsiEnable);
            pHlp->pfnPrintf(pHlp, "    Multi-message capability                = %#x\n",     MsiCapHdr.n.u3MsiMultiMessCap);
            pHlp->pfnPrintf(pHlp, "    Multi-message enable                    = %#x\n",     MsiCapHdr.n.u3MsiMultiMessEn);
        }
    }
    /* MSI Address Register (Lo and Hi). */
    {
        uint32_t const uMsiAddrLo = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO);
        uint32_t const uMsiAddrHi = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI);
        MSIADDR MsiAddr;
        MsiAddr.u64 = RT_MAKE_U64(uMsiAddrLo, uMsiAddrHi);
        pHlp->pfnPrintf(pHlp, "  MSI Address                             = %#RX64\n",   MsiAddr.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Destination mode                        = %#x\n",    MsiAddr.n.u1DestMode);
            pHlp->pfnPrintf(pHlp, "    Redirection hint                        = %#x\n",    MsiAddr.n.u1RedirHint);
            pHlp->pfnPrintf(pHlp, "    Destination Id                          = %#x\n",    MsiAddr.n.u8DestId);
            pHlp->pfnPrintf(pHlp, "    Address                                 = %#RX32\n", MsiAddr.n.u12Addr);
            pHlp->pfnPrintf(pHlp, "    Address (Hi) / Rsvd?                    = %#RX32\n", MsiAddr.n.u32Rsvd0);
        }
    }
    /* MSI Data. */
    {
        MSIDATA MsiData;
        MsiData.u32 = PDMPciDevGetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA);
        pHlp->pfnPrintf(pHlp, "  MSI Data                                = %#RX32\n", MsiData.u32);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Vector                                  = %#x (%u)\n", MsiData.n.u8Vector,
                            MsiData.n.u8Vector);
            pHlp->pfnPrintf(pHlp, "    Delivery mode                           = %#x\n", MsiData.n.u3DeliveryMode);
            pHlp->pfnPrintf(pHlp, "    Level                                   = %#x\n", MsiData.n.u1Level);
            pHlp->pfnPrintf(pHlp, "    Trigger mode                            = %s\n",  MsiData.n.u1TriggerMode ?
                                                                                         "level" : "edge");
        }
    }
    /* MSI Mapping Capability Header (HyperTransport, reporting all 0s currently). */
    {
        MSI_MAP_CAP_HDR_T MsiMapCapHdr;
        MsiMapCapHdr.u32 = 0;
        pHlp->pfnPrintf(pHlp, "  MSI Mapping Capability Header           = %#RX32\n",    MsiMapCapHdr.u32);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Capability ID                           = %#x\n",     MsiMapCapHdr.n.u8MsiMapCapId);
            pHlp->pfnPrintf(pHlp, "    Map enable                              = %RTbool\n", MsiMapCapHdr.n.u1MsiMapEn);
            pHlp->pfnPrintf(pHlp, "    Map fixed                               = %RTbool\n", MsiMapCapHdr.n.u1MsiMapFixed);
            pHlp->pfnPrintf(pHlp, "    Map capability type                     = %#x\n",     MsiMapCapHdr.n.u5MapCapType);
        }
    }
    /* Performance Optimization Control Register. */
    {
        IOMMU_PERF_OPT_CTRL_T const PerfOptCtrl = pThis->PerfOptCtrl;
        pHlp->pfnPrintf(pHlp, "  Performance Optimization Control        = %#RX32\n",    PerfOptCtrl.u32);
        if (fVerbose)
            pHlp->pfnPrintf(pHlp, "    Enable                                  = %RTbool\n", PerfOptCtrl.n.u1PerfOptEn);
    }
    /* XT (x2APIC) General Interrupt Control Register. */
    {
        IOMMU_XT_GEN_INTR_CTRL_T const XtGenIntrCtrl = pThis->XtGenIntrCtrl;
        pHlp->pfnPrintf(pHlp, "  XT General Interrupt Control            = %#RX64\n", XtGenIntrCtrl.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Interrupt destination mode              = %s\n",
                            !XtGenIntrCtrl.n.u1X2ApicIntrDstMode ? "physical" : "logical");
            pHlp->pfnPrintf(pHlp, "    Interrupt destination                   = %#RX64\n",
                            RT_MAKE_U64(XtGenIntrCtrl.n.u24X2ApicIntrDstLo, XtGenIntrCtrl.n.u7X2ApicIntrDstHi));
            pHlp->pfnPrintf(pHlp, "    Interrupt vector                        = %#x\n", XtGenIntrCtrl.n.u8X2ApicIntrVector);
            pHlp->pfnPrintf(pHlp, "    Interrupt delivery mode                 = %s\n",
                            !XtGenIntrCtrl.n.u8X2ApicIntrVector ? "fixed" : "arbitrated");
        }
    }
    /* XT (x2APIC) PPR Interrupt Control Register. */
    {
        IOMMU_XT_PPR_INTR_CTRL_T const XtPprIntrCtrl = pThis->XtPprIntrCtrl;
        pHlp->pfnPrintf(pHlp, "  XT PPR Interrupt Control                = %#RX64\n", XtPprIntrCtrl.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "   Interrupt destination mode               = %s\n",
                            !XtPprIntrCtrl.n.u1X2ApicIntrDstMode ? "physical" : "logical");
            pHlp->pfnPrintf(pHlp, "   Interrupt destination                    = %#RX64\n",
                            RT_MAKE_U64(XtPprIntrCtrl.n.u24X2ApicIntrDstLo, XtPprIntrCtrl.n.u7X2ApicIntrDstHi));
            pHlp->pfnPrintf(pHlp, "   Interrupt vector                         = %#x\n", XtPprIntrCtrl.n.u8X2ApicIntrVector);
            pHlp->pfnPrintf(pHlp, "   Interrupt delivery mode                  = %s\n",
                            !XtPprIntrCtrl.n.u8X2ApicIntrVector ? "fixed" : "arbitrated");
        }
    }
    /* XT (X2APIC) GA Log Interrupt Control Register. */
    {
        IOMMU_XT_GALOG_INTR_CTRL_T const XtGALogIntrCtrl = pThis->XtGALogIntrCtrl;
        pHlp->pfnPrintf(pHlp, "  XT PPR Interrupt Control                = %#RX64\n", XtGALogIntrCtrl.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Interrupt destination mode              = %s\n",
                            !XtGALogIntrCtrl.n.u1X2ApicIntrDstMode ? "physical" : "logical");
            pHlp->pfnPrintf(pHlp, "    Interrupt destination                   = %#RX64\n",
                            RT_MAKE_U64(XtGALogIntrCtrl.n.u24X2ApicIntrDstLo, XtGALogIntrCtrl.n.u7X2ApicIntrDstHi));
            pHlp->pfnPrintf(pHlp, "    Interrupt vector                        = %#x\n", XtGALogIntrCtrl.n.u8X2ApicIntrVector);
            pHlp->pfnPrintf(pHlp, "    Interrupt delivery mode                 = %s\n",
                            !XtGALogIntrCtrl.n.u8X2ApicIntrVector ? "fixed" : "arbitrated");
        }
    }
    /* MARC Registers. */
    {
        for (unsigned i = 0; i < RT_ELEMENTS(pThis->aMarcApers); i++)
        {
            pHlp->pfnPrintf(pHlp, " MARC Aperature %u:\n", i);
            MARC_APER_BAR_T const MarcAperBar = pThis->aMarcApers[i].Base;
            pHlp->pfnPrintf(pHlp, "   Base    = %#RX64\n", MarcAperBar.n.u40MarcBaseAddr << X86_PAGE_4K_SHIFT);

            MARC_APER_RELOC_T const MarcAperReloc = pThis->aMarcApers[i].Reloc;
            pHlp->pfnPrintf(pHlp, "   Reloc   = %#RX64 (addr: %#RX64, read-only: %RTbool, enable: %RTbool)\n",
                            MarcAperReloc.u64, MarcAperReloc.n.u40MarcRelocAddr << X86_PAGE_4K_SHIFT,
                            MarcAperReloc.n.u1ReadOnly, MarcAperReloc.n.u1RelocEn);

            MARC_APER_LEN_T const MarcAperLen = pThis->aMarcApers[i].Length;
            pHlp->pfnPrintf(pHlp, "   Length  = %u pages\n", MarcAperLen.n.u40MarcLength);
        }
    }
    /* Reserved Register. */
    pHlp->pfnPrintf(pHlp, "  Reserved Register                       = %#RX64\n", pThis->RsvdReg);
    /* Command Buffer Head Pointer Register. */
    {
        CMD_BUF_HEAD_PTR_T const CmdBufHeadPtr = pThis->CmdBufHeadPtr;
        pHlp->pfnPrintf(pHlp, "  Command Buffer Head Pointer             = %#RX64 (off: %#x)\n", CmdBufHeadPtr.u64,
                        CmdBufHeadPtr.n.off);
    }
    /* Command Buffer Tail Pointer Register. */
    {
        CMD_BUF_HEAD_PTR_T const CmdBufTailPtr = pThis->CmdBufTailPtr;
        pHlp->pfnPrintf(pHlp, "  Command Buffer Tail Pointer             = %#RX64 (off: %#x)\n", CmdBufTailPtr.u64,
                        CmdBufTailPtr.n.off);
    }
    /* Event Log Head Pointer Register. */
    {
        EVT_LOG_HEAD_PTR_T const EvtLogHeadPtr = pThis->EvtLogHeadPtr;
        pHlp->pfnPrintf(pHlp, "  Event Log Head Pointer                  = %#RX64 (off: %#x)\n", EvtLogHeadPtr.u64,
                        EvtLogHeadPtr.n.off);
    }
    /* Event Log Tail Pointer Register. */
    {
        EVT_LOG_TAIL_PTR_T const EvtLogTailPtr = pThis->EvtLogTailPtr;
        pHlp->pfnPrintf(pHlp, "  Event Log Head Pointer                  = %#RX64 (off: %#x)\n", EvtLogTailPtr.u64,
                        EvtLogTailPtr.n.off);
    }
    /* Status Register. */
    {
        IOMMU_STATUS_T const Status = pThis->Status;
        pHlp->pfnPrintf(pHlp, "  Status Register                         = %#RX64\n", Status.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Event log overflow                      = %RTbool\n", Status.n.u1EvtOverflow);
            pHlp->pfnPrintf(pHlp, "    Event log interrupt                     = %RTbool\n", Status.n.u1EvtLogIntr);
            pHlp->pfnPrintf(pHlp, "    Completion wait interrupt               = %RTbool\n", Status.n.u1CompWaitIntr);
            pHlp->pfnPrintf(pHlp, "    Event log running                       = %RTbool\n", Status.n.u1EvtLogRunning);
            pHlp->pfnPrintf(pHlp, "    Command buffer running                  = %RTbool\n", Status.n.u1CmdBufRunning);
            pHlp->pfnPrintf(pHlp, "    PPR overflow                            = %RTbool\n", Status.n.u1PprOverflow);
            pHlp->pfnPrintf(pHlp, "    PPR interrupt                           = %RTbool\n", Status.n.u1PprIntr);
            pHlp->pfnPrintf(pHlp, "    PPR log running                         = %RTbool\n", Status.n.u1PprLogRunning);
            pHlp->pfnPrintf(pHlp, "    Guest log running                       = %RTbool\n", Status.n.u1GstLogRunning);
            pHlp->pfnPrintf(pHlp, "    Guest log interrupt                     = %RTbool\n", Status.n.u1GstLogIntr);
            pHlp->pfnPrintf(pHlp, "    PPR log B overflow                      = %RTbool\n", Status.n.u1PprOverflowB);
            pHlp->pfnPrintf(pHlp, "    PPR log active                          = %RTbool\n", Status.n.u1PprLogActive);
            pHlp->pfnPrintf(pHlp, "    Event log B overflow                    = %RTbool\n", Status.n.u1EvtOverflowB);
            pHlp->pfnPrintf(pHlp, "    Event log active                        = %RTbool\n", Status.n.u1EvtLogActive);
            pHlp->pfnPrintf(pHlp, "    PPR log B overflow early warning        = %RTbool\n", Status.n.u1PprOverflowEarlyB);
            pHlp->pfnPrintf(pHlp, "    PPR log overflow early warning          = %RTbool\n", Status.n.u1PprOverflowEarly);
        }
    }
    /* PPR Log Head Pointer. */
    {
        PPR_LOG_HEAD_PTR_T const PprLogHeadPtr = pThis->PprLogHeadPtr;
        pHlp->pfnPrintf(pHlp, "  PPR Log Head Pointer                    = %#RX64 (off: %#x)\n", PprLogHeadPtr.u64,
                        PprLogHeadPtr.n.off);
    }
    /* PPR Log Tail Pointer. */
    {
        PPR_LOG_TAIL_PTR_T const PprLogTailPtr = pThis->PprLogTailPtr;
        pHlp->pfnPrintf(pHlp, "  PPR Log Tail Pointer                    = %#RX64 (off: %#x)\n", PprLogTailPtr.u64,
                        PprLogTailPtr.n.off);
    }
    /* Guest Virtual-APIC Log Head Pointer. */
    {
        GALOG_HEAD_PTR_T const GALogHeadPtr = pThis->GALogHeadPtr;
        pHlp->pfnPrintf(pHlp, "  Guest Virtual-APIC Log Head Pointer     = %#RX64 (off: %#x)\n", GALogHeadPtr.u64,
                        GALogHeadPtr.n.u12GALogPtr);
    }
    /* Guest Virtual-APIC Log Tail Pointer. */
    {
        GALOG_HEAD_PTR_T const GALogTailPtr = pThis->GALogTailPtr;
        pHlp->pfnPrintf(pHlp, "  Guest Virtual-APIC Log Tail Pointer     = %#RX64 (off: %#x)\n", GALogTailPtr.u64,
                        GALogTailPtr.n.u12GALogPtr);
    }
    /* PPR Log B Head Pointer. */
    {
        PPR_LOG_B_HEAD_PTR_T const PprLogBHeadPtr = pThis->PprLogBHeadPtr;
        pHlp->pfnPrintf(pHlp, "  PPR Log B Head Pointer                  = %#RX64 (off: %#x)\n", PprLogBHeadPtr.u64,
                        PprLogBHeadPtr.n.off);
    }
    /* PPR Log B Tail Pointer. */
    {
        PPR_LOG_B_TAIL_PTR_T const PprLogBTailPtr = pThis->PprLogBTailPtr;
        pHlp->pfnPrintf(pHlp, "  PPR Log B Tail Pointer                  = %#RX64 (off: %#x)\n", PprLogBTailPtr.u64,
                        PprLogBTailPtr.n.off);
    }
    /* Event Log B Head Pointer. */
    {
        EVT_LOG_B_HEAD_PTR_T const EvtLogBHeadPtr = pThis->EvtLogBHeadPtr;
        pHlp->pfnPrintf(pHlp, "  Event Log B Head Pointer                = %#RX64 (off: %#x)\n", EvtLogBHeadPtr.u64,
                        EvtLogBHeadPtr.n.off);
    }
    /* Event Log B Tail Pointer. */
    {
        EVT_LOG_B_TAIL_PTR_T const EvtLogBTailPtr = pThis->EvtLogBTailPtr;
        pHlp->pfnPrintf(pHlp, "  Event Log B Tail Pointer                = %#RX64 (off: %#x)\n", EvtLogBTailPtr.u64,
                        EvtLogBTailPtr.n.off);
    }
    /* PPR Log Auto Response Register. */
    {
        PPR_LOG_AUTO_RESP_T const PprLogAutoResp = pThis->PprLogAutoResp;
        pHlp->pfnPrintf(pHlp, "  PPR Log Auto Response Register          = %#RX64\n",     PprLogAutoResp.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Code                                    = %#x\n",      PprLogAutoResp.n.u4AutoRespCode);
            pHlp->pfnPrintf(pHlp, "    Mask Gen.                               = %RTbool\n",  PprLogAutoResp.n.u1AutoRespMaskGen);
        }
    }
    /* PPR Log Overflow Early Warning Indicator Register. */
    {
        PPR_LOG_OVERFLOW_EARLY_T const PprLogOverflowEarly = pThis->PprLogOverflowEarly;
        pHlp->pfnPrintf(pHlp, "  PPR Log overflow early warning          = %#RX64\n",    PprLogOverflowEarly.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Threshold                               = %#x\n",     PprLogOverflowEarly.n.u15Threshold);
            pHlp->pfnPrintf(pHlp, "    Interrupt enable                        = %RTbool\n", PprLogOverflowEarly.n.u1IntrEn);
            pHlp->pfnPrintf(pHlp, "    Enable                                  = %RTbool\n", PprLogOverflowEarly.n.u1Enable);
        }
    }
    /* PPR Log Overflow Early Warning Indicator Register. */
    {
        PPR_LOG_OVERFLOW_EARLY_T const PprLogBOverflowEarly = pThis->PprLogBOverflowEarly;
        pHlp->pfnPrintf(pHlp, "  PPR Log B overflow early warning        = %#RX64\n",    PprLogBOverflowEarly.u64);
        if (fVerbose)
        {
            pHlp->pfnPrintf(pHlp, "    Threshold                               = %#x\n",     PprLogBOverflowEarly.n.u15Threshold);
            pHlp->pfnPrintf(pHlp, "    Interrupt enable                        = %RTbool\n", PprLogBOverflowEarly.n.u1IntrEn);
            pHlp->pfnPrintf(pHlp, "    Enable                                  = %RTbool\n", PprLogBOverflowEarly.n.u1Enable);
        }
    }
}


/**
 * Dumps the DTE via the info callback helper.
 *
 * @param   pHlp        The info helper.
 * @param   pDte        The device table entry.
 * @param   pszPrefix   The string prefix.
 */
static void iommuAmdR3DbgInfoDteWorker(PCDBGFINFOHLP pHlp, PCDTE_T pDte, const char *pszPrefix)
{
    AssertReturnVoid(pHlp);
    AssertReturnVoid(pDte);
    AssertReturnVoid(pszPrefix);

    pHlp->pfnPrintf(pHlp, "%sValid                      = %RTbool\n", pszPrefix, pDte->n.u1Valid);
    pHlp->pfnPrintf(pHlp, "%sTranslation Valid          = %RTbool\n", pszPrefix, pDte->n.u1TranslationValid);
    pHlp->pfnPrintf(pHlp, "%sHost Access Dirty          = %#x\n",     pszPrefix, pDte->n.u2Had);
    pHlp->pfnPrintf(pHlp, "%sPaging Mode                = %u\n",      pszPrefix, pDte->n.u3Mode);
    pHlp->pfnPrintf(pHlp, "%sPage Table Root Ptr        = %#RX64 (addr=%#RGp)\n", pszPrefix, pDte->n.u40PageTableRootPtrLo,
                    pDte->n.u40PageTableRootPtrLo << 12);
    pHlp->pfnPrintf(pHlp, "%sPPR enable                 = %RTbool\n", pszPrefix, pDte->n.u1Ppr);
    pHlp->pfnPrintf(pHlp, "%sGuest PPR Resp w/ PASID    = %RTbool\n", pszPrefix, pDte->n.u1GstPprRespPasid);
    pHlp->pfnPrintf(pHlp, "%sGuest I/O Prot Valid       = %RTbool\n", pszPrefix, pDte->n.u1GstIoValid);
    pHlp->pfnPrintf(pHlp, "%sGuest Translation Valid    = %RTbool\n", pszPrefix, pDte->n.u1GstTranslateValid);
    pHlp->pfnPrintf(pHlp, "%sGuest Levels Translated    = %#x\n",     pszPrefix, pDte->n.u2GstMode);
    pHlp->pfnPrintf(pHlp, "%sGuest Root Page Table Ptr  = %#x %#x %#x (addr=%#RGp)\n", pszPrefix,
                    pDte->n.u3GstCr3TableRootPtrLo, pDte->n.u16GstCr3TableRootPtrMid, pDte->n.u21GstCr3TableRootPtrHi,
                      (pDte->n.u21GstCr3TableRootPtrHi  << 31)
                    | (pDte->n.u16GstCr3TableRootPtrMid << 15)
                    | (pDte->n.u3GstCr3TableRootPtrLo   << 12));
    pHlp->pfnPrintf(pHlp, "%sI/O Read                   = %s\n",      pszPrefix, pDte->n.u1IoRead ? "allowed" : "denied");
    pHlp->pfnPrintf(pHlp, "%sI/O Write                  = %s\n",      pszPrefix, pDte->n.u1IoWrite ? "allowed" : "denied");
    pHlp->pfnPrintf(pHlp, "%sReserved (MBZ)             = %#x\n",     pszPrefix, pDte->n.u1Rsvd0);
    pHlp->pfnPrintf(pHlp, "%sDomain ID                  = %u (%#x)\n", pszPrefix, pDte->n.u16DomainId, pDte->n.u16DomainId);
    pHlp->pfnPrintf(pHlp, "%sIOTLB Enable               = %RTbool\n", pszPrefix, pDte->n.u1IoTlbEnable);
    pHlp->pfnPrintf(pHlp, "%sSuppress I/O PFs           = %RTbool\n", pszPrefix, pDte->n.u1SuppressPfEvents);
    pHlp->pfnPrintf(pHlp, "%sSuppress all I/O PFs       = %RTbool\n", pszPrefix, pDte->n.u1SuppressAllPfEvents);
    pHlp->pfnPrintf(pHlp, "%sPort I/O Control           = %#x\n",     pszPrefix, pDte->n.u2IoCtl);
    pHlp->pfnPrintf(pHlp, "%sIOTLB Cache Hint           = %s\n",      pszPrefix, pDte->n.u1Cache ? "no caching" : "cache");
    pHlp->pfnPrintf(pHlp, "%sSnoop Disable              = %RTbool\n", pszPrefix, pDte->n.u1SnoopDisable);
    pHlp->pfnPrintf(pHlp, "%sAllow Exclusion            = %RTbool\n", pszPrefix, pDte->n.u1AllowExclusion);
    pHlp->pfnPrintf(pHlp, "%sSysMgt Message Enable      = %RTbool\n", pszPrefix, pDte->n.u2SysMgt);
    pHlp->pfnPrintf(pHlp, "\n");

    pHlp->pfnPrintf(pHlp, "%sInterrupt Map Valid        = %RTbool\n", pszPrefix, pDte->n.u1IntrMapValid);
    uint8_t const uIntrTabLen = pDte->n.u4IntrTableLength;
    if (uIntrTabLen < IOMMU_DTE_INTR_TAB_LEN_MAX)
    {
        uint16_t const cEntries    = IOMMU_GET_INTR_TAB_ENTRIES(pDte);
        uint16_t const cbIntrTable = IOMMU_GET_INTR_TAB_LEN(pDte);
        pHlp->pfnPrintf(pHlp, "%sInterrupt Table Length     = %#x (%u entries, %u bytes)\n", pszPrefix, uIntrTabLen, cEntries,
                        cbIntrTable);
    }
    else
        pHlp->pfnPrintf(pHlp, "%sInterrupt Table Length     = %#x (invalid!)\n", pszPrefix, uIntrTabLen);
    pHlp->pfnPrintf(pHlp, "%sIgnore Unmapped Interrupts = %RTbool\n", pszPrefix, pDte->n.u1IgnoreUnmappedIntrs);
    pHlp->pfnPrintf(pHlp, "%sInterrupt Table Root Ptr   = %#RX64 (addr=%#RGp)\n", pszPrefix,
                    pDte->n.u46IntrTableRootPtr, pDte->au64[2] & IOMMU_DTE_IRTE_ROOT_PTR_MASK);
    pHlp->pfnPrintf(pHlp, "%sReserved (MBZ)             = %#x\n",     pszPrefix, pDte->n.u4Rsvd0);
    pHlp->pfnPrintf(pHlp, "%sINIT passthru              = %RTbool\n", pszPrefix, pDte->n.u1InitPassthru);
    pHlp->pfnPrintf(pHlp, "%sExtInt passthru            = %RTbool\n", pszPrefix, pDte->n.u1ExtIntPassthru);
    pHlp->pfnPrintf(pHlp, "%sNMI passthru               = %RTbool\n", pszPrefix, pDte->n.u1NmiPassthru);
    pHlp->pfnPrintf(pHlp, "%sReserved (MBZ)             = %#x\n",     pszPrefix, pDte->n.u1Rsvd2);
    pHlp->pfnPrintf(pHlp, "%sInterrupt Control          = %#x\n",     pszPrefix, pDte->n.u2IntrCtrl);
    pHlp->pfnPrintf(pHlp, "%sLINT0 passthru             = %RTbool\n", pszPrefix, pDte->n.u1Lint0Passthru);
    pHlp->pfnPrintf(pHlp, "%sLINT1 passthru             = %RTbool\n", pszPrefix, pDte->n.u1Lint1Passthru);
    pHlp->pfnPrintf(pHlp, "%sReserved (MBZ)             = %#x\n",     pszPrefix, pDte->n.u32Rsvd0);
    pHlp->pfnPrintf(pHlp, "%sReserved (MBZ)             = %#x\n",     pszPrefix, pDte->n.u22Rsvd0);
    pHlp->pfnPrintf(pHlp, "%sAttribute Override Valid   = %RTbool\n", pszPrefix, pDte->n.u1AttrOverride);
    pHlp->pfnPrintf(pHlp, "%sMode0FC                    = %#x\n",     pszPrefix, pDte->n.u1Mode0FC);
    pHlp->pfnPrintf(pHlp, "%sSnoop Attribute            = %#x\n",     pszPrefix, pDte->n.u8SnoopAttr);
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) iommuAmdR3DbgInfoDte(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    if (pszArgs)
    {
        uint16_t uDevId = 0;
        int rc = RTStrToUInt16Full(pszArgs, 0 /* uBase */, &uDevId);
        if (RT_SUCCESS(rc))
        {
            DTE_T Dte;
            rc = iommuAmdReadDte(pDevIns, uDevId, IOMMUOP_TRANSLATE_REQ,  &Dte);
            if (RT_SUCCESS(rc))
            {
                iommuAmdR3DbgInfoDteWorker(pHlp, &Dte, " ");
                return;
            }

            pHlp->pfnPrintf(pHlp, "Failed to read DTE for device ID %u (%#x). rc=%Rrc\n", uDevId, uDevId, rc);
        }
        else
            pHlp->pfnPrintf(pHlp, "Failed to parse a valid 16-bit device ID. rc=%Rrc\n", rc);
    }
    else
        pHlp->pfnPrintf(pHlp, "Missing device ID.\n");
}


#if 0
/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) iommuAmdR3DbgInfoDevTabs(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);

    PCIOMMU    pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PCPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    uint8_t cTables = 0;
    for (uint8_t i = 0; i < RT_ELEMENTS(pThis->aDevTabBaseAddrs); i++)
    {
        DEV_TAB_BAR_T  DevTabBar    = pThis->aDevTabBaseAddrs[i];
        RTGCPHYS const GCPhysDevTab = DevTabBar.n.u40Base << X86_PAGE_4K_SHIFT;
        if (GCPhysDevTab)
            ++cTables;
    }

    pHlp->pfnPrintf(pHlp, "AMD-IOMMU Device Tables:\n");
    pHlp->pfnPrintf(pHlp, " Tables active: %u\n", cTables);
    if (!cTables)
        return;

    for (uint8_t i = 0; i < RT_ELEMENTS(pThis->aDevTabBaseAddrs); i++)
    {
        DEV_TAB_BAR_T  DevTabBar    = pThis->aDevTabBaseAddrs[i];
        RTGCPHYS const GCPhysDevTab = DevTabBar.n.u40Base << X86_PAGE_4K_SHIFT;
        if (GCPhysDevTab)
        {
            uint32_t const cbDevTab = IOMMU_GET_DEV_TAB_LEN(&DevTabBar);
            uint32_t const cDtes    = cbDevTab / sizeof(DTE_T);
            pHlp->pfnPrintf(pHlp, " Table %u (base=%#RGp size=%u bytes entries=%u):\n", i, GCPhysDevTab, cbDevTab, cDtes);

            void *pvDevTab = RTMemAllocZ(cbDevTab);
            if (RT_LIKELY(pvDevTab))
            {
                int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhysDevTab, pvDevTab, cbDevTab);
                if (RT_SUCCESS(rc))
                {
                    for (uint32_t idxDte = 0; idxDte < cDtes; idxDte++)
                    {
                        PCDTE_T pDte = (PCDTE_T)((char *)pvDevTab + idxDte * sizeof(DTE_T));
                        if (   pDte->n.u1Valid
                            || pDte->n.u1IntrMapValid)
                        {
                            pHlp->pfnPrintf(pHlp, " DTE %u:\n", idxDte);
                            iommuAmdR3DbgInfoDteWorker(pHlp, pDte, " ");
                        }
                    }
                    pHlp->pfnPrintf(pHlp, "\n");
                }
                else
                {
                    pHlp->pfnPrintf(pHlp, " Failed to read table at %#RGp of size %u bytes. rc=%Rrc!\n", GCPhysDevTab,
                                    cbDevTab, rc);
                }

                RTMemFree(pvDevTab);
            }
            else
            {
                pHlp->pfnPrintf(pHlp, " Allocating %u bytes for reading the device table failed!\n", cbDevTab);
                return;
            }
        }
    }
}
#endif

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) iommuAmdR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    /** @todo IOMMU: Save state. */
    RT_NOREF2(pDevIns, pSSM);
    LogFlowFunc(("\n"));
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) iommuAmdR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /** @todo IOMMU: Load state. */
    RT_NOREF4(pDevIns, pSSM, uVersion, uPass);
    LogFlowFunc(("\n"));
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuAmdR3Reset(PPDMDEVINS pDevIns)
{
    /*
     * Resets read-write portion of the IOMMU state.
     *
     * State data not initialized here is expected to be initialized during
     * device construction and remain read-only through the lifetime of the VM.
     */
    PIOMMU     pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    IOMMU_LOCK_NORET(pDevIns);

    LogFlowFunc(("\n"));

    memset(&pThis->aDevTabBaseAddrs[0], 0, sizeof(pThis->aDevTabBaseAddrs));

    pThis->CmdBufBaseAddr.u64        = 0;
    pThis->CmdBufBaseAddr.n.u4Len    = 8;

    pThis->EvtLogBaseAddr.u64        = 0;
    pThis->EvtLogBaseAddr.n.u4Len    = 8;

    pThis->Ctrl.u64                  = 0;
    pThis->Ctrl.n.u1Coherent         = 1;
    Assert(!pThis->ExtFeat.n.u1BlockStopMarkSup);

    pThis->ExclRangeBaseAddr.u64     = 0;
    pThis->ExclRangeLimit.u64        = 0;

    pThis->PprLogBaseAddr.u64        = 0;
    pThis->PprLogBaseAddr.n.u4Len    = 8;

    pThis->HwEvtHi.u64               = 0;
    pThis->HwEvtLo                   = 0;
    pThis->HwEvtStatus.u64           = 0;

    pThis->GALogBaseAddr.u64         = 0;
    pThis->GALogBaseAddr.n.u4Len     = 8;
    pThis->GALogTailAddr.u64         = 0;

    pThis->PprLogBBaseAddr.u64       = 0;
    pThis->PprLogBBaseAddr.n.u4Len   = 8;

    pThis->EvtLogBBaseAddr.u64       = 0;
    pThis->EvtLogBBaseAddr.n.u4Len   = 8;

    pThis->PerfOptCtrl.u32           = 0;

    pThis->XtGenIntrCtrl.u64         = 0;
    pThis->XtPprIntrCtrl.u64         = 0;
    pThis->XtGALogIntrCtrl.u64       = 0;

    memset(&pThis->aMarcApers[0], 0, sizeof(pThis->aMarcApers));

    pThis->CmdBufHeadPtr.u64         = 0;
    pThis->CmdBufTailPtr.u64         = 0;
    pThis->EvtLogHeadPtr.u64         = 0;
    pThis->EvtLogTailPtr.u64         = 0;

    pThis->Status.u64                = 0;

    pThis->PprLogHeadPtr.u64         = 0;
    pThis->PprLogTailPtr.u64         = 0;

    pThis->GALogHeadPtr.u64          = 0;
    pThis->GALogTailPtr.u64          = 0;

    pThis->PprLogBHeadPtr.u64        = 0;
    pThis->PprLogBTailPtr.u64        = 0;

    pThis->EvtLogBHeadPtr.u64        = 0;
    pThis->EvtLogBTailPtr.u64        = 0;

    pThis->PprLogAutoResp.u64        = 0;
    pThis->PprLogOverflowEarly.u64   = 0;
    pThis->PprLogBOverflowEarly.u64  = 0;

    pThis->IommuBar.u64              = 0;
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_BASE_ADDR_REG_LO, 0);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_BASE_ADDR_REG_HI, 0);

    PDMPciDevSetCommand(pPciDev, VBOX_PCI_COMMAND_MASTER);

    IOMMU_UNLOCK(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuAmdR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PIOMMU pThis = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    LogFlowFunc(("\n"));

    /* Close the command thread semaphore. */
    if (pThis->hEvtCmdThread != NIL_SUPSEMEVENT)
    {
        PDMDevHlpSUPSemEventClose(pDevIns, pThis->hEvtCmdThread);
        pThis->hEvtCmdThread = NIL_SUPSEMEVENT;
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) iommuAmdR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    RT_NOREF(pCfg);

    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUCC);
    pThisCC->pDevInsR3 = pDevIns;

    LogFlowFunc(("iInstance=%d\n", iInstance));

    /*
     * Register the IOMMU with PDM.
     */
    PDMIOMMUREGR3 IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version  = PDM_IOMMUREGCC_VERSION;
    IommuReg.pfnMemRead  = iommuAmdDeviceMemRead;
    IommuReg.pfnMemWrite = iommuAmdDeviceMemWrite;
    IommuReg.pfnMsiRemap = iommuAmdDeviceMsiRemap;
    IommuReg.u32TheEnd   = PDM_IOMMUREGCC_VERSION;
    int rc = PDMDevHlpIommuRegister(pDevIns, &IommuReg, &pThisCC->CTX_SUFF(pIommuHlp), &pThis->idxIommu);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to register ourselves as an IOMMU device"));
    if (pThisCC->CTX_SUFF(pIommuHlp)->u32Version != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper version mismatch; got %#x expected %#x"),
                                   pThisCC->CTX_SUFF(pIommuHlp)->u32Version, PDM_IOMMUHLPR3_VERSION);
    if (pThisCC->CTX_SUFF(pIommuHlp)->u32TheEnd != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper end-version mismatch; got %#x expected %#x"),
                                   pThisCC->CTX_SUFF(pIommuHlp)->u32TheEnd, PDM_IOMMUHLPR3_VERSION);

    /*
     * Initialize read-only PCI configuration space.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    /* Header. */
    PDMPciDevSetVendorId(pPciDev,          IOMMU_PCI_VENDOR_ID);       /* AMD */
    PDMPciDevSetDeviceId(pPciDev,          IOMMU_PCI_DEVICE_ID);       /* VirtualBox IOMMU device */
    PDMPciDevSetCommand(pPciDev,           VBOX_PCI_COMMAND_MASTER);   /* Enable bus master (as we directly access main memory) */
    PDMPciDevSetStatus(pPciDev,            VBOX_PCI_STATUS_CAP_LIST);  /* Capability list supported */
    PDMPciDevSetRevisionId(pPciDev,        IOMMU_PCI_REVISION_ID);     /* VirtualBox specific device implementation revision */
    PDMPciDevSetClassBase(pPciDev,         VBOX_PCI_CLASS_SYSTEM);     /* System Base Peripheral */
    PDMPciDevSetClassSub(pPciDev,          VBOX_PCI_SUB_SYSTEM_IOMMU); /* IOMMU */
    PDMPciDevSetClassProg(pPciDev,         0x0);                       /* IOMMU Programming interface */
    PDMPciDevSetHeaderType(pPciDev,        0x0);                       /* Single function, type 0 */
    PDMPciDevSetSubSystemId(pPciDev,       IOMMU_PCI_DEVICE_ID);       /* AMD */
    PDMPciDevSetSubSystemVendorId(pPciDev, IOMMU_PCI_VENDOR_ID);       /* VirtualBox IOMMU device */
    PDMPciDevSetCapabilityList(pPciDev,    IOMMU_PCI_OFF_CAP_HDR);     /* Offset into capability registers */
    PDMPciDevSetInterruptPin(pPciDev,      0x1);                       /* INTA#. */
    PDMPciDevSetInterruptLine(pPciDev,     0x0);                       /* For software compatibility; no effect on hardware */

    /* Capability Header. */
    /* NOTE! Fields (e.g, EFR) must match what we expose in the ACPI tables. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_CAP_HDR,
                        RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_ID,    0xf)     /* RO - Secure Device capability block */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_PTR,   IOMMU_PCI_OFF_MSI_CAP_HDR)  /* RO - Offset to next capability */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_TYPE,  0x3)     /* RO - IOMMU capability block */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_REV,   0x1)     /* RO - IOMMU interface revision */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_IOTLB_SUP, 0x0)     /* RO - Remote IOTLB support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_HT_TUNNEL, 0x0)     /* RO - HyperTransport Tunnel support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_NP_CACHE,  0x0)     /* RO - Cache NP page table entries */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_EFR_SUP,   0x1)     /* RO - Extended Feature Register support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_EXT,   0x1));   /* RO - Misc. Information Register support */

    /* Base Address Register. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_BASE_ADDR_REG_LO, 0x0);   /* RW - Base address (Lo) and enable bit */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_BASE_ADDR_REG_HI, 0x0);   /* RW - Base address (Hi) */

    /* IOMMU Range Register. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_RANGE_REG, 0x0);          /* RW - Range register (implemented as RO by us) */

    /* Misc. Information Register. */
    /* NOTE! Fields (e.g, GVA size) must match what we expose in the ACPI tables. */
    uint32_t const  uMiscInfoReg0 = RT_BF_MAKE(IOMMU_BF_MISCINFO_0_MSI_NUM,      0)   /* RO - MSI number */
                                  | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_GVA_SIZE,     2)   /* RO - Guest Virt. Addr size (2=48 bits) */
                                  | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_PA_SIZE,     48)   /* RO - Physical Addr size (48 bits) */
                                  | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_VA_SIZE,     64)   /* RO - Virt. Addr size (64 bits) */
                                  | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_HT_ATS_RESV,  0)   /* RW - HT ATS reserved */
                                  | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_MSI_NUM_PPR,  0);  /* RW - PPR interrupt number */
    uint32_t const uMiscInfoReg1  = 0;
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MISCINFO_REG_0, uMiscInfoReg0);
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MISCINFO_REG_1, uMiscInfoReg1);

    /* MSI Capability Header register. */
    PDMMSIREG MsiReg;
    RT_ZERO(MsiReg);
    MsiReg.cMsiVectors    = 1;
    MsiReg.iMsiCapOffset  = IOMMU_PCI_OFF_MSI_CAP_HDR;
    MsiReg.iMsiNextOffset = 0; /* IOMMU_PCI_OFF_MSI_MAP_CAP_HDR */
    MsiReg.fMsi64bit      = 1; /* 64-bit addressing support is mandatory; See AMD spec. 2.8 "IOMMU Interrupt Support". */

    /* MSI Address (Lo, Hi) and MSI data are read-write PCI config registers handled by our generic PCI config space code. */
#if 0
    /* MSI Address Lo. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_LO, 0);         /* RW - MSI message address (Lo) */
    /* MSI Address Hi. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_ADDR_HI, 0);         /* RW - MSI message address (Hi) */
    /* MSI Data. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_DATA, 0);            /* RW - MSI data */
#endif

#if 0
    /** @todo IOMMU: I don't know if we need to support this, enable later if
     *        required. */
    /* MSI Mapping Capability Header register. */
    PDMPciDevSetDWord(pPciDev, IOMMU_PCI_OFF_MSI_MAP_CAP_HDR,
                        RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_ID,   0x8)       /* RO - Capability ID */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_PTR,  0x0)       /* RO - Offset to next capability (NULL) */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_EN,       0x1)       /* RO - MSI mapping capability enable */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_FIXED,    0x1)       /* RO - MSI mapping range is fixed */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_TYPE, 0x15));    /* RO - MSI mapping capability */
    /* When implementing don't forget to copy this to its MMIO shadow register (MsiMapCapHdr) in iommuAmdR3Init. */
#endif

    /*
     * Register the PCI function with PDM.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register MSI support for the PCI device.
     * This must be done -after- register it as a PCI device!
     */
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
    AssertRCReturn(rc, rc);

    /*
     * Intercept PCI config. space accesses.
     */
    rc = PDMDevHlpPCIInterceptConfigAccesses(pDevIns, pPciDev, iommuAmdR3PciConfigRead, iommuAmdR3PciConfigWrite);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Create the MMIO region.
     * Mapping of the region is done when software configures it via PCI config space.
     */
    rc = PDMDevHlpMmioCreate(pDevIns, IOMMU_MMIO_REGION_SIZE, pPciDev, 0 /* iPciRegion */, iommuAmdMmioWrite, iommuAmdMmioRead,
                             NULL /* pvUser */, IOMMMIO_FLAGS_READ_DWORD_QWORD | IOMMMIO_FLAGS_WRITE_DWORD_QWORD_READ_MISSING,
                             "AMD-IOMMU", &pThis->hMmio);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register saved state.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, IOMMU_SAVED_STATE_VERSION, sizeof(IOMMU), NULL,
                                NULL, NULL, NULL,
                                NULL, iommuAmdR3SaveExec, NULL,
                                NULL, iommuAmdR3LoadExec, NULL);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register debugger info items.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "iommu",    "Display IOMMU state.", iommuAmdR3DbgInfo);
    PDMDevHlpDBGFInfoRegister(pDevIns, "iommudte", "Display the DTE for a device. Arguments: DeviceID.", iommuAmdR3DbgInfoDte);
#if 0
    PDMDevHlpDBGFInfoRegister(pDevIns, "iommudevtabs", "Display IOMMU device tables.", iommuAmdR3DbgInfoDevTabs);
#endif

# ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadR3,  STAMTYPE_COUNTER, "R3/MmioReadR3",  STAMUNIT_OCCURENCES, "Number of MMIO reads in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadRZ,  STAMTYPE_COUNTER, "RZ/MmioReadRZ",  STAMUNIT_OCCURENCES, "Number of MMIO reads in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteR3, STAMTYPE_COUNTER, "R3/MmioWriteR3", STAMUNIT_OCCURENCES, "Number of MMIO writes in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteRZ, STAMTYPE_COUNTER, "RZ/MmioWriteRZ", STAMUNIT_OCCURENCES, "Number of MMIO writes in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapR3, STAMTYPE_COUNTER, "R3/MsiRemapR3", STAMUNIT_OCCURENCES, "Number of interrupt remap requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapRZ, STAMTYPE_COUNTER, "RZ/MsiRemapRZ", STAMUNIT_OCCURENCES, "Number of interrupt remap requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmd, STAMTYPE_COUNTER, "R3/Commands", STAMUNIT_OCCURENCES, "Number of commands processed (total).");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdCompWait, STAMTYPE_COUNTER, "R3/Commands/CompWait", STAMUNIT_OCCURENCES, "Number of Completion Wait commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdInvDte, STAMTYPE_COUNTER, "R3/Commands/InvDte", STAMUNIT_OCCURENCES, "Number of Invalidate DTE commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdInvIommuPages, STAMTYPE_COUNTER, "R3/Commands/InvIommuPages", STAMUNIT_OCCURENCES, "Number of Invalidate IOMMU Pages commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdInvIotlbPages, STAMTYPE_COUNTER, "R3/Commands/InvIotlbPages", STAMUNIT_OCCURENCES, "Number of Invalidate IOTLB Pages commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdInvIntrTable, STAMTYPE_COUNTER, "R3/Commands/InvIntrTable", STAMUNIT_OCCURENCES, "Number of Invalidate Interrupt Table commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdPrefIommuPages, STAMTYPE_COUNTER, "R3/Commands/PrefIommuPages", STAMUNIT_OCCURENCES, "Number of Prefetch IOMMU Pages commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdCompletePprReq, STAMTYPE_COUNTER, "R3/Commands/CompletePprReq", STAMUNIT_OCCURENCES, "Number of Complete PPR Requests commands processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCmdInvIommuAll, STAMTYPE_COUNTER, "R3/Commands/InvIommuAll", STAMUNIT_OCCURENCES, "Number of Invalidate IOMMU All commands processed.");
# endif

    /*
     * Create the command thread and its event semaphore.
     */
    char szDevIommu[64];
    RT_ZERO(szDevIommu);
    RTStrPrintf(szDevIommu, sizeof(szDevIommu), "IOMMU-%u", iInstance);
    rc = PDMDevHlpThreadCreate(pDevIns, &pThisCC->pCmdThread, pThis, iommuAmdR3CmdThread, iommuAmdR3CmdThreadWakeUp,
                               0 /* cbStack */, RTTHREADTYPE_IO, szDevIommu);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->hEvtCmdThread);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Initialize read-only registers.
     * NOTE! Fields here must match their corresponding field in the ACPI tables.
     */
    /** @todo Don't remove the =0 assignment for now. It's just there so it's easier
     *        for me to see existing features that we might want to implement. Do it
     *        later. */
    pThis->ExtFeat.u64 = 0;
    pThis->ExtFeat.n.u1PrefetchSup           = 0;
    pThis->ExtFeat.n.u1PprSup                = 0;
    pThis->ExtFeat.n.u1X2ApicSup             = 0;
    pThis->ExtFeat.n.u1NoExecuteSup          = 0;
    pThis->ExtFeat.n.u1GstTranslateSup       = 0;
    pThis->ExtFeat.n.u1InvAllSup             = 1;
    pThis->ExtFeat.n.u1GstVirtApicSup        = 0;
    pThis->ExtFeat.n.u1HwErrorSup            = 1;
    pThis->ExtFeat.n.u1PerfCounterSup        = 0;
    AssertCompile((IOMMU_MAX_HOST_PT_LEVEL & 0x3) < 3);
    pThis->ExtFeat.n.u2HostAddrTranslateSize = (IOMMU_MAX_HOST_PT_LEVEL & 0x3);
    pThis->ExtFeat.n.u2GstAddrTranslateSize  = 0;   /* Requires GstTranslateSup */
    pThis->ExtFeat.n.u2GstCr3RootTblLevel    = 0;   /* Requires GstTranslateSup */
    pThis->ExtFeat.n.u2SmiFilterSup          = 0;
    pThis->ExtFeat.n.u3SmiFilterCount        = 0;
    pThis->ExtFeat.n.u3GstVirtApicModeSup    = 0;   /* Requires GstVirtApicSup */
    pThis->ExtFeat.n.u2DualPprLogSup         = 0;
    pThis->ExtFeat.n.u2DualEvtLogSup         = 0;
    pThis->ExtFeat.n.u5MaxPasidSup           = 0;   /* Requires GstTranslateSup */
    pThis->ExtFeat.n.u1UserSupervisorSup     = 0;
    AssertCompile(IOMMU_MAX_DEV_TAB_SEGMENTS <= 3);
    pThis->ExtFeat.n.u2DevTabSegSup          = IOMMU_MAX_DEV_TAB_SEGMENTS;
    pThis->ExtFeat.n.u1PprLogOverflowWarn    = 0;
    pThis->ExtFeat.n.u1PprAutoRespSup        = 0;
    pThis->ExtFeat.n.u2MarcSup               = 0;
    pThis->ExtFeat.n.u1BlockStopMarkSup      = 0;
    pThis->ExtFeat.n.u1PerfOptSup            = 0;
    pThis->ExtFeat.n.u1MsiCapMmioSup         = 1;
    pThis->ExtFeat.n.u1GstIoSup              = 0;
    pThis->ExtFeat.n.u1HostAccessSup         = 0;
    pThis->ExtFeat.n.u1EnhancedPprSup        = 0;
    pThis->ExtFeat.n.u1AttrForwardSup        = 0;
    pThis->ExtFeat.n.u1HostDirtySup          = 0;
    pThis->ExtFeat.n.u1InvIoTlbTypeSup       = 0;
    pThis->ExtFeat.n.u1GstUpdateDisSup       = 0;
    pThis->ExtFeat.n.u1ForcePhysDstSup       = 0;

    pThis->RsvdReg = 0;

    pThis->DevSpecificFeat.u64   = 0;
    pThis->DevSpecificFeat.n.u4RevMajor = IOMMU_DEVSPEC_FEAT_MAJOR_VERSION;
    pThis->DevSpecificFeat.n.u4RevMinor = IOMMU_DEVSPEC_FEAT_MINOR_VERSION;

    pThis->DevSpecificCtrl.u64 = 0;
    pThis->DevSpecificCtrl.n.u4RevMajor = IOMMU_DEVSPEC_CTRL_MAJOR_VERSION;
    pThis->DevSpecificCtrl.n.u4RevMinor = IOMMU_DEVSPEC_CTRL_MINOR_VERSION;

    pThis->DevSpecificStatus.u64 = 0;
    pThis->DevSpecificStatus.n.u4RevMajor = IOMMU_DEVSPEC_STATUS_MAJOR_VERSION;
    pThis->DevSpecificStatus.n.u4RevMinor = IOMMU_DEVSPEC_STATUS_MINOR_VERSION;

    pThis->MiscInfo.u64 = RT_MAKE_U64(uMiscInfoReg0, uMiscInfoReg1);

    /*
     * Initialize parts of the IOMMU state as it would during reset.
     * Must be called -after- initializing PCI config. space registers.
     */
    iommuAmdR3Reset(pDevIns);

    return VINF_SUCCESS;
}

# else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) iommuAmdRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUCC);

    pThisCC->CTX_SUFF(pDevIns) = pDevIns;

    /* Set up the MMIO RZ handlers. */
    int rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, iommuAmdMmioWrite, iommuAmdMmioRead, NULL /* pvUser */);
    AssertRCReturn(rc, rc);

    /* Set up the IOMMU RZ callbacks. */
    PDMIOMMUREGCC IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version  = PDM_IOMMUREGCC_VERSION;
    IommuReg.idxIommu    = pThis->idxIommu;
    IommuReg.pfnMemRead  = iommuAmdDeviceMemRead;
    IommuReg.pfnMemWrite = iommuAmdDeviceMemWrite;
    IommuReg.pfnMsiRemap = iommuAmdDeviceMsiRemap;
    IommuReg.u32TheEnd   = PDM_IOMMUREGCC_VERSION;
    rc = PDMDevHlpIommuSetUpContext(pDevIns, &IommuReg, &pThisCC->CTX_SUFF(pIommuHlp));
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

# endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceIommuAmd =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "iommu-amd",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PCI_BUILTIN,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(IOMMU),
    /* .cbInstanceCC = */           sizeof(IOMMUCC),
    /* .cbInstanceRC = */           sizeof(IOMMURC),
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "IOMMU (AMD)",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           iommuAmdR3Construct,
    /* .pfnDestruct = */            iommuAmdR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               iommuAmdR3Reset,
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
    /* .pfnConstruct = */           iommuAmdRZConstruct,
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
    /* .pfnConstruct = */           iommuAmdRZConstruct,
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

