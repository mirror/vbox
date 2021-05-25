/* $Id$ */
/** @file
 * IOMMU - Input/Output Memory Management Unit - Intel implementation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include "VBoxDD.h"
#include "DevIommuIntel.h"

#include <iprt/mem.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Gets the low uint32_t of a uint64_t or something equivalent.
 *
 * This is suitable for casting constants outside code (since RT_LO_U32 can't be
 * used as it asserts for correctness when compiling on certain compilers). */
#define DMAR_LO_U32(a)       (uint32_t)(UINT32_MAX & (a))

/** Gets the high uint32_t of a uint64_t or something equivalent.
 *
 * This is suitable for casting constants outside code (since RT_HI_U32 can't be
 * used as it asserts for correctness when compiling on certain compilers). */
#define DMAR_HI_U32(a)       (uint32_t)((a) >> 32)

/** Asserts MMIO access' offset and size are valid or returns appropriate error
 * code suitable for returning from MMIO access handlers. */
#define DMAR_ASSERT_MMIO_ACCESS_RET(a_off, a_cb) \
    do { \
         AssertReturn((a_cb) == 4 || (a_cb) == 8, VINF_IOM_MMIO_UNUSED_FF); \
         AssertReturn(!((a_off) & ((a_cb) - 1)), VINF_IOM_MMIO_UNUSED_FF); \
    } while (0)

/** Checks if the MMIO offset is valid. */
#define DMAR_IS_MMIO_OFF_VALID(a_off)               (   (a_off) < DMAR_MMIO_GROUP_0_OFF_END \
                                                     || (a_off) - DMAR_MMIO_GROUP_1_OFF_FIRST < DMAR_MMIO_GROUP_1_SIZE)

/** Acquires the DMAR lock but returns with the given busy error code on failure. */
#define DMAR_LOCK_RET(a_pDevIns, a_pThisCC, a_rcBusy) \
    do { \
        if ((a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnLock((a_pDevIns), (a_rcBusy)) == VINF_SUCCESS) \
        { /* likely */ } \
        else \
            return (a_rcBusy); \
    } while (0)

/** Acquires the DMAR lock (not expected to fail). */
#ifdef IN_RING3
# define DMAR_LOCK(a_pDevIns, a_pThisCC)            (a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnLock((a_pDevIns), VERR_IGNORED)
#else
# define DMAR_LOCK(a_pDevIns, a_pThisCC) \
    do { \
        int const rcLock = (a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnLock((a_pDevIns), VINF_SUCCESS); \
        AssertRC(rcLock); \
    } while (0)
#endif

/** Release the DMAR lock. */
#define DMAR_UNLOCK(a_pDevIns, a_pThisCC)           (a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnUnlock(a_pDevIns)

/** Asserts that the calling thread owns the DMAR lock. */
#define DMAR_ASSERT_LOCK_IS_OWNER(a_pDevIns, a_pThisCC) \
    do { \
        Assert((a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnLockIsOwner(a_pDevIns)); \
        RT_NOREF1(a_pThisCC); \
    } while (0)

/** Asserts that the calling thread does not own the DMAR lock. */
#define DMAR_ASSERT_LOCK_IS_NOT_OWNER(a_pDevIns, a_pThisCC) \
    do { \
        Assert((a_pThisCC)->CTX_SUFF(pIommuHlp)->pfnLockIsOwner(a_pDevIns) == false); \
        RT_NOREF1(a_pThisCC); \
    } while (0)

/** The number of fault recording registers our implementation supports.
 *  Normal guest operation shouldn't trigger faults anyway, so we only support the
 *  minimum number of registers (which is 1).
 *
 *  See Intel VT-d spec. 10.4.2 "Capability Register" (CAP_REG.NFR). */
#define DMAR_FRCD_REG_COUNT                         UINT32_C(1)

/** Offset of first register in group 0. */
#define DMAR_MMIO_GROUP_0_OFF_FIRST                 VTD_MMIO_OFF_VER_REG
/** Offset of last register in group 0 (inclusive). */
#define DMAR_MMIO_GROUP_0_OFF_LAST                  VTD_MMIO_OFF_MTRR_PHYSMASK9_REG
/** Last valid offset in group 0 (exclusive). */
#define DMAR_MMIO_GROUP_0_OFF_END                   (DMAR_MMIO_GROUP_0_OFF_LAST + 8 /* sizeof MTRR_PHYSMASK9_REG */)
/** Size of the group 0 (in bytes). */
#define DMAR_MMIO_GROUP_0_SIZE                      (DMAR_MMIO_GROUP_0_OFF_END - DMAR_MMIO_GROUP_0_OFF_FIRST)
/**< Implementation-specific MMIO offset of IVA_REG. */
#define DMAR_MMIO_OFF_IVA_REG                       0xe50
/**< Implementation-specific MMIO offset of IOTLB_REG. */
#define DMAR_MMIO_OFF_IOTLB_REG                     0xe58
/**< Implementation-specific MMIO offset of FRCD_LO_REG. */
#define DMAR_MMIO_OFF_FRCD_LO_REG                   0xe70
/**< Implementation-specific MMIO offset of FRCD_HI_REG. */
#define DMAR_MMIO_OFF_FRCD_HI_REG                   0xe78
AssertCompile(!(DMAR_MMIO_OFF_FRCD_LO_REG & 0xf));

/** Offset of first register in group 1. */
#define DMAR_MMIO_GROUP_1_OFF_FIRST                 VTD_MMIO_OFF_VCCAP_REG
/** Offset of last register in group 1 (inclusive). */
#define DMAR_MMIO_GROUP_1_OFF_LAST                  (DMAR_MMIO_OFF_FRCD_LO_REG + 8) * DMAR_FRCD_REG_COUNT
/** Last valid offset in group 1 (exclusive). */
#define DMAR_MMIO_GROUP_1_OFF_END                   (DMAR_MMIO_GROUP_1_OFF_LAST + 8 /* sizeof FRCD_HI_REG */)
/** Size of the group 1 (in bytes). */
#define DMAR_MMIO_GROUP_1_SIZE                      (DMAR_MMIO_GROUP_1_OFF_END - DMAR_MMIO_GROUP_1_OFF_FIRST)

/** DMAR implementation's major version number (exposed to software).
 *  We report 6 as the major version since we support queued-invalidations as
 *  software may make assumptions based on that.
 *
 *  See Intel VT-d spec. 10.4.7 "Context Command Register" (CCMD_REG.CAIG). */
#define DMAR_VER_MAJOR                              6
/** DMAR implementation's minor version number (exposed to software). */
#define DMAR_VER_MINOR                              0

/** Number of domain supported (0=16, 1=64, 2=256, 3=1K, 4=4K, 5=16K, 6=64K,
 *  7=Reserved). */
#define DMAR_ND                                     6

/** Release log prefix string. */
#define DMAR_LOG_PFX                                "Intel-IOMMU"
/** The current saved state version. */
#define DMAR_SAVED_STATE_VERSION                    1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * DMAR error diagnostics.
 * Sorted alphabetically so it's easier to add and locate items, no other reason.
 *
 * @note Members of this enum are used as array indices, so no gaps in enum
 *       values are not allowed. Update g_apszDmarDiagDesc when you modify
 *       fields in this enum.
 */
typedef enum
{
    /* No error, this must be zero! */
    kDmarDiag_None = 0,

    /* Address Translation Faults. */
    kDmarDiag_Atf_Lct_1,
    kDmarDiag_Atf_Lct_2,
    kDmarDiag_Atf_Lct_3,
    kDmarDiag_Atf_Lct_4_1,
    kDmarDiag_Atf_Lct_4_2,
    kDmarDiag_Atf_Lct_4_3,
    kDmarDiag_Atf_Lrt_1,
    kDmarDiag_Atf_Lrt_2,
    kDmarDiag_Atf_Lrt_3,
    kDmarDiag_Atf_Rta_1_1,
    kDmarDiag_Atf_Rta_1_2,
    kDmarDiag_Atf_Rta_1_3,

    /* CCMD_REG faults. */
    kDmarDiag_CcmdReg_NotSupported,
    kDmarDiag_CcmdReg_Qi_Enabled,
    kDmarDiag_CcmdReg_Ttm_Invalid,

    /* IQA_REG faults. */
    kDmarDiag_IqaReg_Dsc_Fetch_Error,
    kDmarDiag_IqaReg_Dw_128_Invalid,
    kDmarDiag_IqaReg_Dw_256_Invalid,

    /* Invalidation Queue Error Info. */
    kDmarDiag_Iqei_Dsc_Type_Invalid,
    kDmarDiag_Iqei_Inv_Wait_Dsc_0_1_Rsvd,
    kDmarDiag_Iqei_Inv_Wait_Dsc_2_3_Rsvd,
    kDmarDiag_Iqei_Inv_Wait_Dsc_Invalid,
    kDmarDiag_Iqei_Ttm_Rsvd,

    /* IQT_REG faults. */
    kDmarDiag_IqtReg_Qt_Invalid,
    kDmarDiag_IqtReg_Qt_NotAligned,

    /* Compatibility Format Interrupt Faults. */
    kDmarDiag_Ir_Cfi_Blocked,

    /* Remappable Format Interrupt Faults. */
    kDmarDiag_Ir_Rfi_Intr_Index_Invalid,
    kDmarDiag_Ir_Rfi_Irte_Mode_Invalid,
    kDmarDiag_Ir_Rfi_Irte_Not_Present,
    kDmarDiag_Ir_Rfi_Irte_Read_Failed,
    kDmarDiag_Ir_Rfi_Irte_Rsvd,
    kDmarDiag_Ir_Rfi_Irte_Svt_Bus,
    kDmarDiag_Ir_Rfi_Irte_Svt_Masked,
    kDmarDiag_Ir_Rfi_Irte_Svt_Rsvd,
    kDmarDiag_Ir_Rfi_Rsvd,

    /* Member for determining array index limit. */
    kDmarDiag_End,

    /* Usual 32-bit type size hack. */
    kDmarDiag_32Bit_Hack = 0x7fffffff
} DMARDIAG;
AssertCompileSize(DMARDIAG, 4);

/** DMAR diagnostic enum description expansion.
 * The below construct ensures typos in the input to this macro are caught
 * during compile time. */
#define DMARDIAG_DESC(a_Name)        RT_CONCAT(kDmarDiag_, a_Name) < kDmarDiag_End ? RT_STR(a_Name) : "Ignored"

/** DMAR diagnostics description for members in DMARDIAG. */
static const char *const g_apszDmarDiagDesc[] =
{
    DMARDIAG_DESC(None                      ),
    DMARDIAG_DESC(Atf_Lct_1                 ),
    DMARDIAG_DESC(Atf_Lct_2                 ),
    DMARDIAG_DESC(Atf_Lct_3                 ),
    DMARDIAG_DESC(Atf_Lct_4_1               ),
    DMARDIAG_DESC(Atf_Lct_4_2               ),
    DMARDIAG_DESC(Atf_Lct_4_3               ),
    DMARDIAG_DESC(Atf_Lrt_1                 ),
    DMARDIAG_DESC(Atf_Lrt_2                 ),
    DMARDIAG_DESC(Atf_Lrt_3                 ),
    DMARDIAG_DESC(Atf_Rta_1_1               ),
    DMARDIAG_DESC(Atf_Rta_1_2               ),
    DMARDIAG_DESC(Atf_Rta_1_3               ),
    DMARDIAG_DESC(CcmdReg_NotSupported      ),
    DMARDIAG_DESC(CcmdReg_Qi_Enabled        ),
    DMARDIAG_DESC(CcmdReg_Ttm_Invalid       ),
    DMARDIAG_DESC(IqaReg_Dsc_Fetch_Error    ),
    DMARDIAG_DESC(IqaReg_Dw_128_Invalid     ),
    DMARDIAG_DESC(IqaReg_Dw_256_Invalid     ),
    DMARDIAG_DESC(Iqei_Dsc_Type_Invalid     ),
    DMARDIAG_DESC(Iqei_Inv_Wait_Dsc_0_1_Rsvd),
    DMARDIAG_DESC(Iqei_Inv_Wait_Dsc_2_3_Rsvd),
    DMARDIAG_DESC(Iqei_Inv_Wait_Dsc_Invalid ),
    DMARDIAG_DESC(Iqei_Ttm_Rsvd             ),
    DMARDIAG_DESC(IqtReg_Qt_Invalid         ),
    DMARDIAG_DESC(IqtReg_Qt_NotAligned      ),
    DMARDIAG_DESC(Ir_Cfi_Blocked            ),
    DMARDIAG_DESC(Ir_Rfi_Intr_Index_Invalid ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Mode_Invalid  ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Not_Present   ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Read_Failed   ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Rsvd          ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Svt_Bus       ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Svt_Masked    ),
    DMARDIAG_DESC(Ir_Rfi_Irte_Svt_Rsvd      ),
    DMARDIAG_DESC(Ir_Rfi_Rsvd               ),
    /* kDmarDiag_End */
};
AssertCompile(RT_ELEMENTS(g_apszDmarDiagDesc) == kDmarDiag_End);
#undef DMARDIAG_DESC

/**
 * The shared DMAR device state.
 */
typedef struct DMAR
{
    /** IOMMU device index. */
    uint32_t                    idxIommu;
    /** DMAR magic. */
    uint32_t                    u32Magic;

    /** Registers (group 0). */
    uint8_t                     abRegs0[DMAR_MMIO_GROUP_0_SIZE];
    /** Registers (group 1). */
    uint8_t                     abRegs1[DMAR_MMIO_GROUP_1_SIZE];

    /** @name Lazily activated registers.
     * These are the active values for lazily activated registers. Software is free to
     * modify the actual register values while remapping/translation is enabled but they
     * take effect only when explicitly signaled by software, hence we need to hold the
     * active values separately.
     * @{ */
    /** Currently active IRTA_REG. */
    uint64_t                    uIrtaReg;
    /** Currently active RTADDR_REG. */
    uint64_t                    uRtaddrReg;
    /** @} */

    /** @name Register copies for a tiny bit faster and more convenient access.
     *  @{ */
    /** Copy of VER_REG. */
    uint8_t                     uVerReg;
    /** Alignment. */
    uint8_t                     abPadding[7];
    /** Copy of CAP_REG. */
    uint64_t                    fCapReg;
    /** Copy of ECAP_REG. */
    uint64_t                    fExtCapReg;
    /** @} */

    /** The event semaphore the invalidation-queue thread waits on. */
    SUPSEMEVENT                 hEvtInvQueue;
    /** Padding. */
    uint32_t                    uPadding0;
    /** Error diagnostic. */
    DMARDIAG                    enmDiag;
    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER                 StatMmioReadR3;         /**< Number of MMIO reads in R3. */
    STAMCOUNTER                 StatMmioReadRZ;         /**< Number of MMIO reads in RZ. */
    STAMCOUNTER                 StatMmioWriteR3;        /**< Number of MMIO writes in R3. */
    STAMCOUNTER                 StatMmioWriteRZ;        /**< Number of MMIO writes in RZ. */

    STAMCOUNTER                 StatMsiRemapCfiR3;      /**< Number of compatibility-format interrupts remap requests in R3. */
    STAMCOUNTER                 StatMsiRemapCfiRZ;      /**< Number of compatibility-format interrupts remap requests in RZ. */
    STAMCOUNTER                 StatMsiRemapRfiR3;      /**< Number of remappable-format interrupts remap requests in R3. */
    STAMCOUNTER                 StatMsiRemapRfiRZ;      /**< Number of remappable-format interrupts remap requests in RZ. */

    STAMCOUNTER                 StatMemReadR3;          /**< Number of memory read translation requests in R3. */
    STAMCOUNTER                 StatMemReadRZ;          /**< Number of memory read translation requests in RZ. */
    STAMCOUNTER                 StatMemWriteR3;         /**< Number of memory write translation requests in R3. */
    STAMCOUNTER                 StatMemWriteRZ;         /**< Number of memory write translation requests in RZ. */

    STAMCOUNTER                 StatMemBulkReadR3;      /**< Number of memory read bulk translation requests in R3. */
    STAMCOUNTER                 StatMemBulkReadRZ;      /**< Number of memory read bulk translation requests in RZ. */
    STAMCOUNTER                 StatMemBulkWriteR3;     /**< Number of memory write bulk translation requests in R3. */
    STAMCOUNTER                 StatMemBulkWriteRZ;     /**< Number of memory write bulk translation requests in RZ. */

    STAMCOUNTER                 StatCcInvDsc;           /**< Number of Context-cache descriptors processed. */
    STAMCOUNTER                 StatIotlbInvDsc;        /**< Number of IOTLB descriptors processed. */
    STAMCOUNTER                 StatDevtlbInvDsc;       /**< Number of Device-TLB descriptors processed. */
    STAMCOUNTER                 StatIecInvDsc;          /**< Number of Interrupt-Entry cache descriptors processed. */
    STAMCOUNTER                 StatInvWaitDsc;         /**< Number of Invalidation wait descriptors processed. */
    STAMCOUNTER                 StatPasidIotlbInvDsc;   /**< Number of PASID-based IOTLB descriptors processed. */
    STAMCOUNTER                 StatPasidCacheInvDsc;   /**< Number of PASID-cache descriptors processed. */
    STAMCOUNTER                 StatPasidDevtlbInvDsc;  /**< Number of PASID-based device-TLB descriptors processed. */
#endif
} DMAR;
/** Pointer to the DMAR device state. */
typedef DMAR *PDMAR;
/** Pointer to the const DMAR device state. */
typedef DMAR const *PCDMAR;
AssertCompileMemberAlignment(DMAR, abRegs0, 8);
AssertCompileMemberAlignment(DMAR, abRegs1, 8);

/**
 * The ring-3 DMAR device state.
 */
typedef struct DMARR3
{
    /** Device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The IOMMU helper. */
    R3PTRTYPE(PCPDMIOMMUHLPR3)  pIommuHlpR3;
    /** The invalidation-queue thread. */
    R3PTRTYPE(PPDMTHREAD)       pInvQueueThread;
} DMARR3;
/** Pointer to the ring-3 DMAR device state. */
typedef DMARR3 *PDMARR3;
/** Pointer to the const ring-3 DMAR device state. */
typedef DMARR3 const *PCDMARR3;

/**
 * The ring-0 DMAR device state.
 */
typedef struct DMARR0
{
    /** Device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** The IOMMU helper. */
    R0PTRTYPE(PCPDMIOMMUHLPR0)  pIommuHlpR0;
} DMARR0;
/** Pointer to the ring-0 IOMMU device state. */
typedef DMARR0 *PDMARR0;
/** Pointer to the const ring-0 IOMMU device state. */
typedef DMARR0 const *PCDMARR0;

/**
 * The raw-mode DMAR device state.
 */
typedef struct DMARRC
{
    /** Device instance. */
    PPDMDEVINSRC                pDevInsRC;
    /** The IOMMU helper. */
    RCPTRTYPE(PCPDMIOMMUHLPRC)  pIommuHlpRC;
} DMARRC;
/** Pointer to the raw-mode DMAR device state. */
typedef DMARRC *PDMARRC;
/** Pointer to the const raw-mode DMAR device state. */
typedef DMARRC const *PCIDMARRC;

/** The DMAR device state for the current context. */
typedef CTX_SUFF(DMAR)  DMARCC;
/** Pointer to the DMAR device state for the current context. */
typedef CTX_SUFF(PDMAR) PDMARCC;
/** Pointer to the const DMAR device state for the current context. */
typedef CTX_SUFF(PDMAR) const PCDMARCC;

/**
 * Type of DMAR originated events that generate interrupts.
 */
typedef enum DMAREVENTTYPE
{
    /** Invalidation completion event. */
    DMAREVENTTYPE_INV_COMPLETE = 0,
    /** Fault event. */
    DMAREVENTTYPE_FAULT
} DMAREVENTTYPE;


/**
 * DMA address map.
 * This structure holds information about a DMA address translation.
 */
typedef struct DMARADDRMAP
{
    /** The device ID (bus, device, function). */
    uint16_t        idDevice;
    uint16_t        uPadding0;
    /** The DMA remapping operation request type. */
    VTDREQTYPE      enmReqType;
    /** The DMA address being accessed. */
    uint64_t        uDmaAddr;
    /** The size of the DMA access (in bytes). */
    size_t          cbDma;
    /** The translated system-physical address (HPA). */
    RTGCPHYS        GCPhysSpa;
    /** The size of the contiguous translated region (in bytes). */
    size_t          cbContiguous;
} DMARADDRMAP;
/** Pointer to a DMA address map. */
typedef DMARADDRMAP *PDMARADDRMAP;
/** Pointer to a const DMA address map. */
typedef DMARADDRMAP const *PCDMARADDRMAP;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Read-write masks for DMAR registers (group 0).
 */
static uint32_t const g_au32RwMasks0[] =
{
    /* Offset  Register                  Low                                        High */
    /* 0x000   VER_REG               */  VTD_VER_REG_RW_MASK,
    /* 0x004   Reserved              */  0,
    /* 0x008   CAP_REG               */  DMAR_LO_U32(VTD_CAP_REG_RW_MASK),          DMAR_HI_U32(VTD_CAP_REG_RW_MASK),
    /* 0x010   ECAP_REG              */  DMAR_LO_U32(VTD_ECAP_REG_RW_MASK),         DMAR_HI_U32(VTD_ECAP_REG_RW_MASK),
    /* 0x018   GCMD_REG              */  VTD_GCMD_REG_RW_MASK,
    /* 0x01c   GSTS_REG              */  VTD_GSTS_REG_RW_MASK,
    /* 0x020   RTADDR_REG            */  DMAR_LO_U32(VTD_RTADDR_REG_RW_MASK),       DMAR_HI_U32(VTD_RTADDR_REG_RW_MASK),
    /* 0x028   CCMD_REG              */  DMAR_LO_U32(VTD_CCMD_REG_RW_MASK),         DMAR_HI_U32(VTD_CCMD_REG_RW_MASK),
    /* 0x030   Reserved              */  0,
    /* 0x034   FSTS_REG              */  VTD_FSTS_REG_RW_MASK,
    /* 0x038   FECTL_REG             */  VTD_FECTL_REG_RW_MASK,
    /* 0x03c   FEDATA_REG            */  VTD_FEDATA_REG_RW_MASK,
    /* 0x040   FEADDR_REG            */  VTD_FEADDR_REG_RW_MASK,
    /* 0x044   FEUADDR_REG           */  VTD_FEUADDR_REG_RW_MASK,
    /* 0x048   Reserved              */  0,                                         0,
    /* 0x050   Reserved              */  0,                                         0,
    /* 0x058   AFLOG_REG             */  DMAR_LO_U32(VTD_AFLOG_REG_RW_MASK),        DMAR_HI_U32(VTD_AFLOG_REG_RW_MASK),
    /* 0x060   Reserved              */  0,
    /* 0x064   PMEN_REG              */  0, /* RO as we don't support PLMR and PHMR. */
    /* 0x068   PLMBASE_REG           */  0, /* RO as we don't support PLMR. */
    /* 0x06c   PLMLIMIT_REG          */  0, /* RO as we don't support PLMR. */
    /* 0x070   PHMBASE_REG           */  0,                                         0, /* RO as we don't support PHMR. */
    /* 0x078   PHMLIMIT_REG          */  0,                                         0, /* RO as we don't support PHMR. */
    /* 0x080   IQH_REG               */  DMAR_LO_U32(VTD_IQH_REG_RW_MASK),          DMAR_HI_U32(VTD_IQH_REG_RW_MASK),
    /* 0x088   IQT_REG               */  DMAR_LO_U32(VTD_IQT_REG_RW_MASK),          DMAR_HI_U32(VTD_IQT_REG_RW_MASK),
    /* 0x090   IQA_REG               */  DMAR_LO_U32(VTD_IQA_REG_RW_MASK),          DMAR_HI_U32(VTD_IQA_REG_RW_MASK),
    /* 0x098   Reserved              */  0,
    /* 0x09c   ICS_REG               */  VTD_ICS_REG_RW_MASK,
    /* 0x0a0   IECTL_REG             */  VTD_IECTL_REG_RW_MASK,
    /* 0x0a4   IEDATA_REG            */  VTD_IEDATA_REG_RW_MASK,
    /* 0x0a8   IEADDR_REG            */  VTD_IEADDR_REG_RW_MASK,
    /* 0x0ac   IEUADDR_REG           */  VTD_IEUADDR_REG_RW_MASK,
    /* 0x0b0   IQERCD_REG            */  DMAR_LO_U32(VTD_IQERCD_REG_RW_MASK),       DMAR_HI_U32(VTD_IQERCD_REG_RW_MASK),
    /* 0x0b8   IRTA_REG              */  DMAR_LO_U32(VTD_IRTA_REG_RW_MASK),         DMAR_HI_U32(VTD_IRTA_REG_RW_MASK),
    /* 0x0c0   PQH_REG               */  DMAR_LO_U32(VTD_PQH_REG_RW_MASK),          DMAR_HI_U32(VTD_PQH_REG_RW_MASK),
    /* 0x0c8   PQT_REG               */  DMAR_LO_U32(VTD_PQT_REG_RW_MASK),          DMAR_HI_U32(VTD_PQT_REG_RW_MASK),
    /* 0x0d0   PQA_REG               */  DMAR_LO_U32(VTD_PQA_REG_RW_MASK),          DMAR_HI_U32(VTD_PQA_REG_RW_MASK),
    /* 0x0d8   Reserved              */  0,
    /* 0x0dc   PRS_REG               */  VTD_PRS_REG_RW_MASK,
    /* 0x0e0   PECTL_REG             */  VTD_PECTL_REG_RW_MASK,
    /* 0x0e4   PEDATA_REG            */  VTD_PEDATA_REG_RW_MASK,
    /* 0x0e8   PEADDR_REG            */  VTD_PEADDR_REG_RW_MASK,
    /* 0x0ec   PEUADDR_REG           */  VTD_PEUADDR_REG_RW_MASK,
    /* 0x0f0   Reserved              */  0,                                         0,
    /* 0x0f8   Reserved              */  0,                                         0,
    /* 0x100   MTRRCAP_REG           */  DMAR_LO_U32(VTD_MTRRCAP_REG_RW_MASK),      DMAR_HI_U32(VTD_MTRRCAP_REG_RW_MASK),
    /* 0x108   MTRRDEF_REG           */  0,                                         0, /* RO as we don't support MTS. */
    /* 0x110   Reserved              */  0,                                         0,
    /* 0x118   Reserved              */  0,                                         0,
    /* 0x120   MTRR_FIX64_00000_REG  */  0,                                         0, /* RO as we don't support MTS. */
    /* 0x128   MTRR_FIX16K_80000_REG */  0,                                         0,
    /* 0x130   MTRR_FIX16K_A0000_REG */  0,                                         0,
    /* 0x138   MTRR_FIX4K_C0000_REG  */  0,                                         0,
    /* 0x140   MTRR_FIX4K_C8000_REG  */  0,                                         0,
    /* 0x148   MTRR_FIX4K_D0000_REG  */  0,                                         0,
    /* 0x150   MTRR_FIX4K_D8000_REG  */  0,                                         0,
    /* 0x158   MTRR_FIX4K_E0000_REG  */  0,                                         0,
    /* 0x160   MTRR_FIX4K_E8000_REG  */  0,                                         0,
    /* 0x168   MTRR_FIX4K_F0000_REG  */  0,                                         0,
    /* 0x170   MTRR_FIX4K_F8000_REG  */  0,                                         0,
    /* 0x178   Reserved              */  0,                                         0,
    /* 0x180   MTRR_PHYSBASE0_REG    */  0,                                         0, /* RO as we don't support MTS. */
    /* 0x188   MTRR_PHYSMASK0_REG    */  0,                                         0,
    /* 0x190   MTRR_PHYSBASE1_REG    */  0,                                         0,
    /* 0x198   MTRR_PHYSMASK1_REG    */  0,                                         0,
    /* 0x1a0   MTRR_PHYSBASE2_REG    */  0,                                         0,
    /* 0x1a8   MTRR_PHYSMASK2_REG    */  0,                                         0,
    /* 0x1b0   MTRR_PHYSBASE3_REG    */  0,                                         0,
    /* 0x1b8   MTRR_PHYSMASK3_REG    */  0,                                         0,
    /* 0x1c0   MTRR_PHYSBASE4_REG    */  0,                                         0,
    /* 0x1c8   MTRR_PHYSMASK4_REG    */  0,                                         0,
    /* 0x1d0   MTRR_PHYSBASE5_REG    */  0,                                         0,
    /* 0x1d8   MTRR_PHYSMASK5_REG    */  0,                                         0,
    /* 0x1e0   MTRR_PHYSBASE6_REG    */  0,                                         0,
    /* 0x1e8   MTRR_PHYSMASK6_REG    */  0,                                         0,
    /* 0x1f0   MTRR_PHYSBASE7_REG    */  0,                                         0,
    /* 0x1f8   MTRR_PHYSMASK7_REG    */  0,                                         0,
    /* 0x200   MTRR_PHYSBASE8_REG    */  0,                                         0,
    /* 0x208   MTRR_PHYSMASK8_REG    */  0,                                         0,
    /* 0x210   MTRR_PHYSBASE9_REG    */  0,                                         0,
    /* 0x218   MTRR_PHYSMASK9_REG    */  0,                                         0,
};
AssertCompile(sizeof(g_au32RwMasks0) == DMAR_MMIO_GROUP_0_SIZE);

/**
 * Read-only Status, Write-1-to-clear masks for DMAR registers (group 0).
 */
static uint32_t const g_au32Rw1cMasks0[] =
{
    /* Offset  Register                  Low                        High */
    /* 0x000   VER_REG               */  0,
    /* 0x004   Reserved              */  0,
    /* 0x008   CAP_REG               */  0,                         0,
    /* 0x010   ECAP_REG              */  0,                         0,
    /* 0x018   GCMD_REG              */  0,
    /* 0x01c   GSTS_REG              */  0,
    /* 0x020   RTADDR_REG            */  0,                         0,
    /* 0x028   CCMD_REG              */  0,                         0,
    /* 0x030   Reserved              */  0,
    /* 0x034   FSTS_REG              */  VTD_FSTS_REG_RW1C_MASK,
    /* 0x038   FECTL_REG             */  0,
    /* 0x03c   FEDATA_REG            */  0,
    /* 0x040   FEADDR_REG            */  0,
    /* 0x044   FEUADDR_REG           */  0,
    /* 0x048   Reserved              */  0,                         0,
    /* 0x050   Reserved              */  0,                         0,
    /* 0x058   AFLOG_REG             */  0,                         0,
    /* 0x060   Reserved              */  0,
    /* 0x064   PMEN_REG              */  0,
    /* 0x068   PLMBASE_REG           */  0,
    /* 0x06c   PLMLIMIT_REG          */  0,
    /* 0x070   PHMBASE_REG           */  0,                         0,
    /* 0x078   PHMLIMIT_REG          */  0,                         0,
    /* 0x080   IQH_REG               */  0,                         0,
    /* 0x088   IQT_REG               */  0,                         0,
    /* 0x090   IQA_REG               */  0,                         0,
    /* 0x098   Reserved              */  0,
    /* 0x09c   ICS_REG               */  VTD_ICS_REG_RW1C_MASK,
    /* 0x0a0   IECTL_REG             */  0,
    /* 0x0a4   IEDATA_REG            */  0,
    /* 0x0a8   IEADDR_REG            */  0,
    /* 0x0ac   IEUADDR_REG           */  0,
    /* 0x0b0   IQERCD_REG            */  0,                         0,
    /* 0x0b8   IRTA_REG              */  0,                         0,
    /* 0x0c0   PQH_REG               */  0,                         0,
    /* 0x0c8   PQT_REG               */  0,                         0,
    /* 0x0d0   PQA_REG               */  0,                         0,
    /* 0x0d8   Reserved              */  0,
    /* 0x0dc   PRS_REG               */  0,
    /* 0x0e0   PECTL_REG             */  0,
    /* 0x0e4   PEDATA_REG            */  0,
    /* 0x0e8   PEADDR_REG            */  0,
    /* 0x0ec   PEUADDR_REG           */  0,
    /* 0x0f0   Reserved              */  0,                         0,
    /* 0x0f8   Reserved              */  0,                         0,
    /* 0x100   MTRRCAP_REG           */  0,                         0,
    /* 0x108   MTRRDEF_REG           */  0,                         0,
    /* 0x110   Reserved              */  0,                         0,
    /* 0x118   Reserved              */  0,                         0,
    /* 0x120   MTRR_FIX64_00000_REG  */  0,                         0,
    /* 0x128   MTRR_FIX16K_80000_REG */  0,                         0,
    /* 0x130   MTRR_FIX16K_A0000_REG */  0,                         0,
    /* 0x138   MTRR_FIX4K_C0000_REG  */  0,                         0,
    /* 0x140   MTRR_FIX4K_C8000_REG  */  0,                         0,
    /* 0x148   MTRR_FIX4K_D0000_REG  */  0,                         0,
    /* 0x150   MTRR_FIX4K_D8000_REG  */  0,                         0,
    /* 0x158   MTRR_FIX4K_E0000_REG  */  0,                         0,
    /* 0x160   MTRR_FIX4K_E8000_REG  */  0,                         0,
    /* 0x168   MTRR_FIX4K_F0000_REG  */  0,                         0,
    /* 0x170   MTRR_FIX4K_F8000_REG  */  0,                         0,
    /* 0x178   Reserved              */  0,                         0,
    /* 0x180   MTRR_PHYSBASE0_REG    */  0,                         0,
    /* 0x188   MTRR_PHYSMASK0_REG    */  0,                         0,
    /* 0x190   MTRR_PHYSBASE1_REG    */  0,                         0,
    /* 0x198   MTRR_PHYSMASK1_REG    */  0,                         0,
    /* 0x1a0   MTRR_PHYSBASE2_REG    */  0,                         0,
    /* 0x1a8   MTRR_PHYSMASK2_REG    */  0,                         0,
    /* 0x1b0   MTRR_PHYSBASE3_REG    */  0,                         0,
    /* 0x1b8   MTRR_PHYSMASK3_REG    */  0,                         0,
    /* 0x1c0   MTRR_PHYSBASE4_REG    */  0,                         0,
    /* 0x1c8   MTRR_PHYSMASK4_REG    */  0,                         0,
    /* 0x1d0   MTRR_PHYSBASE5_REG    */  0,                         0,
    /* 0x1d8   MTRR_PHYSMASK5_REG    */  0,                         0,
    /* 0x1e0   MTRR_PHYSBASE6_REG    */  0,                         0,
    /* 0x1e8   MTRR_PHYSMASK6_REG    */  0,                         0,
    /* 0x1f0   MTRR_PHYSBASE7_REG    */  0,                         0,
    /* 0x1f8   MTRR_PHYSMASK7_REG    */  0,                         0,
    /* 0x200   MTRR_PHYSBASE8_REG    */  0,                         0,
    /* 0x208   MTRR_PHYSMASK8_REG    */  0,                         0,
    /* 0x210   MTRR_PHYSBASE9_REG    */  0,                         0,
    /* 0x218   MTRR_PHYSMASK9_REG    */  0,                         0,
};
AssertCompile(sizeof(g_au32Rw1cMasks0) == DMAR_MMIO_GROUP_0_SIZE);

/**
 * Read-write masks for DMAR registers (group 1).
 */
static uint32_t const g_au32RwMasks1[] =
{
    /* Offset  Register                  Low                                        High */
    /* 0xe00   VCCAP_REG             */  DMAR_LO_U32(VTD_VCCAP_REG_RW_MASK),        DMAR_HI_U32(VTD_VCCAP_REG_RW_MASK),
    /* 0xe08   VCMD_EO_REG           */  DMAR_LO_U32(VTD_VCMD_EO_REG_RW_MASK),      DMAR_HI_U32(VTD_VCMD_EO_REG_RW_MASK),
    /* 0xe10   VCMD_REG              */  0,                                         0, /* RO: VCS not supported. */
    /* 0xe18   VCMDRSVD_REG          */  0,                                         0,
    /* 0xe20   VCRSP_REG             */  0,                                         0, /* RO: VCS not supported. */
    /* 0xe28   VCRSPRSVD_REG         */  0,                                         0,
    /* 0xe30   Reserved              */  0,                                         0,
    /* 0xe38   Reserved              */  0,                                         0,
    /* 0xe40   Reserved              */  0,                                         0,
    /* 0xe48   Reserved              */  0,                                         0,
    /* 0xe50   IVA_REG               */  DMAR_LO_U32(VTD_IVA_REG_RW_MASK),          DMAR_HI_U32(VTD_IVA_REG_RW_MASK),
    /* 0xe58   IOTLB_REG             */  DMAR_LO_U32(VTD_IOTLB_REG_RW_MASK),        DMAR_HI_U32(VTD_IOTLB_REG_RW_MASK),
    /* 0xe60   Reserved              */  0,                                         0,
    /* 0xe68   Reserved              */  0,                                         0,
    /* 0xe70   FRCD_REG_LO           */  DMAR_LO_U32(VTD_FRCD_REG_LO_RW_MASK),      DMAR_HI_U32(VTD_FRCD_REG_LO_RW_MASK),
    /* 0xe78   FRCD_REG_HI           */  DMAR_LO_U32(VTD_FRCD_REG_HI_RW_MASK),      DMAR_HI_U32(VTD_FRCD_REG_HI_RW_MASK),
};
AssertCompile(sizeof(g_au32RwMasks1) == DMAR_MMIO_GROUP_1_SIZE);
AssertCompile((DMAR_MMIO_OFF_FRCD_LO_REG - DMAR_MMIO_GROUP_1_OFF_FIRST) + DMAR_FRCD_REG_COUNT * 2 * sizeof(uint64_t) );

/**
 * Read-only Status, Write-1-to-clear masks for DMAR registers (group 1).
 */
static uint32_t const g_au32Rw1cMasks1[] =
{
    /* Offset  Register                  Low                                        High */
    /* 0xe00   VCCAP_REG             */  0,                                         0,
    /* 0xe08   VCMD_EO_REG           */  0,                                         0,
    /* 0xe10   VCMD_REG              */  0,                                         0,
    /* 0xe18   VCMDRSVD_REG          */  0,                                         0,
    /* 0xe20   VCRSP_REG             */  0,                                         0,
    /* 0xe28   VCRSPRSVD_REG         */  0,                                         0,
    /* 0xe30   Reserved              */  0,                                         0,
    /* 0xe38   Reserved              */  0,                                         0,
    /* 0xe40   Reserved              */  0,                                         0,
    /* 0xe48   Reserved              */  0,                                         0,
    /* 0xe50   IVA_REG               */  0,                                         0,
    /* 0xe58   IOTLB_REG             */  0,                                         0,
    /* 0xe60   Reserved              */  0,                                         0,
    /* 0xe68   Reserved              */  0,                                         0,
    /* 0xe70   FRCD_REG_LO           */  DMAR_LO_U32(VTD_FRCD_REG_LO_RW1C_MASK),    DMAR_HI_U32(VTD_FRCD_REG_LO_RW1C_MASK),
    /* 0xe78   FRCD_REG_HI           */  DMAR_LO_U32(VTD_FRCD_REG_HI_RW1C_MASK),    DMAR_HI_U32(VTD_FRCD_REG_HI_RW1C_MASK),
};
AssertCompile(sizeof(g_au32Rw1cMasks1) == DMAR_MMIO_GROUP_1_SIZE);

/** Array of RW masks for each register group. */
static uint8_t const *g_apbRwMasks[]   = { (uint8_t *)&g_au32RwMasks0[0], (uint8_t *)&g_au32RwMasks1[0] };

/** Array of RW1C masks for each register group. */
static uint8_t const *g_apbRw1cMasks[] = { (uint8_t *)&g_au32Rw1cMasks0[0], (uint8_t *)&g_au32Rw1cMasks1[0] };

/* Masks arrays must be identical in size (even bounds checking code assumes this). */
AssertCompile(sizeof(g_apbRw1cMasks) == sizeof(g_apbRwMasks));

/** Array of valid domain-ID bits. */
static uint16_t const g_auNdMask[] = { 0xf, 0x3f, 0xff, 0x3ff, 0xfff, 0x3fff, 0xffff, 0 };
AssertCompile(RT_ELEMENTS(g_auNdMask) >= DMAR_ND);


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/** @todo Add IOMMU struct size/alignment verification, see
 *        Devices/testcase/Makefile.kmk and
 *        Devices/testcase/tstDeviceStructSize[RC].cpp  */

/**
 * Returns the number of supported adjusted guest-address width (SAGAW) in bits
 * given a CAP_REG.SAGAW value.
 *
 * @returns Number of SAGAW bits.
 * @param   uSagaw  The CAP_REG.SAGAW value.
 */
static uint8_t vtdCapRegGetSagawBits(uint8_t uSagaw)
{
    if (RT_LIKELY(uSagaw > 0 && uSagaw < 4))
        return 30 + (uSagaw * 9);
    return 0;
}


/**
 * Returns the supported adjusted guest-address width (SAGAW) given the maximum
 * guest address width (MGAW).
 *
 * @returns The CAP_REG.SAGAW value.
 * @param   uMgaw  The CAP_REG.MGAW value.
 */
static uint8_t vtdCapRegGetSagaw(uint8_t uMgaw)
{
    switch (uMgaw + 1)
    {
        case 39:    return 1;
        case 48:    return 2;
        case 57:    return 3;
    }
    return 0;
}


/**
 * Returns whether the interrupt remapping (IR) fault is qualified or not.
 *
 * @returns @c true if qualified, @c false otherwise.
 * @param   enmIrFault      The interrupt remapping fault condition.
 */
static bool vtdIrFaultIsQualified(VTDIRFAULT enmIrFault)
{
    switch (enmIrFault)
    {
        case VTDIRFAULT_IRTE_NOT_PRESENT:
        case VTDIRFAULT_IRTE_PRESENT_RSVD:
        case VTDIRFAULT_IRTE_PRESENT_INVALID:
        case VTDIRFAULT_PID_READ_FAILED:
        case VTDIRFAULT_PID_RSVD:
            return true;
        default:
            return false;
    }
}


/**
 * Returns table translation mode's descriptive name.
 *
 * @returns The descriptive name.
 * @param   uTtm    The RTADDR_REG.TTM value.
 */
static const char* vtdRtaddrRegGetTtmDesc(uint8_t uTtm)
{
    Assert(!(uTtm & 3));
    static const char* s_apszTtmNames[] =
    {
        "Legacy Mode",
        "Scalable Mode",
        "Reserved",
        "Abort-DMA Mode"
    };
    return s_apszTtmNames[uTtm & (RT_ELEMENTS(s_apszTtmNames) - 1)];
}


/**
 * Gets the index of the group the register belongs to given its MMIO offset.
 *
 * @returns The group index.
 * @param   offReg      The MMIO offset of the register.
 * @param   cbReg       The size of the access being made (for bounds checking on
 *                      debug builds).
 */
DECLINLINE(uint8_t) dmarRegGetGroupIndex(uint16_t offReg, uint8_t cbReg)
{
    uint16_t const offLast = offReg + cbReg - 1;
    AssertCompile(DMAR_MMIO_GROUP_0_OFF_FIRST == 0);
    AssertMsg(DMAR_IS_MMIO_OFF_VALID(offLast), ("off=%#x cb=%u\n", offReg, cbReg));
    return !(offLast < DMAR_MMIO_GROUP_0_OFF_END);
}


/**
 * Gets the group the register belongs to given its MMIO offset.
 *
 * @returns Pointer to the first element of the register group.
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   cbReg       The size of the access being made (for bounds checking on
 *                      debug builds).
 * @param   pIdxGroup   Where to store the index of the register group the register
 *                      belongs to.
 */
DECLINLINE(uint8_t *) dmarRegGetGroup(PDMAR pThis, uint16_t offReg, uint8_t cbReg, uint8_t *pIdxGroup)
{
    *pIdxGroup = dmarRegGetGroupIndex(offReg, cbReg);
    uint8_t *apbRegs[] = { &pThis->abRegs0[0], &pThis->abRegs1[0] };
    return apbRegs[*pIdxGroup];
}


/**
 * Const/read-only version of dmarRegGetGroup.
 *
 * @copydoc dmarRegGetGroup
 */
DECLINLINE(uint8_t const*) dmarRegGetGroupRo(PCDMAR pThis, uint16_t offReg, uint8_t cbReg, uint8_t *pIdxGroup)
{
    *pIdxGroup = dmarRegGetGroupIndex(offReg, cbReg);
    uint8_t const *apbRegs[] = { &pThis->abRegs0[0], &pThis->abRegs1[0] };
    return apbRegs[*pIdxGroup];
}


/**
 * Writes a 32-bit register with the exactly the supplied value.
 *
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 32-bit value to write.
 */
static void dmarRegWriteRaw32(PDMAR pThis, uint16_t offReg, uint32_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = dmarRegGetGroup(pThis, offReg, sizeof(uint32_t), &idxGroup);
    NOREF(idxGroup);
    *(uint32_t *)(pabRegs + offReg) = uReg;
}


/**
 * Writes a 64-bit register with the exactly the supplied value.
 *
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 64-bit value to write.
 */
static void dmarRegWriteRaw64(PDMAR pThis, uint16_t offReg, uint64_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = dmarRegGetGroup(pThis, offReg, sizeof(uint64_t), &idxGroup);
    NOREF(idxGroup);
    *(uint64_t *)(pabRegs + offReg) = uReg;
}


/**
 * Reads a 32-bit register with exactly the value it contains.
 *
 * @returns The raw register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint32_t dmarRegReadRaw32(PCDMAR pThis, uint16_t offReg)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs = dmarRegGetGroupRo(pThis, offReg, sizeof(uint32_t), &idxGroup);
    NOREF(idxGroup);
    return *(uint32_t *)(pabRegs + offReg);
}


/**
 * Reads a 64-bit register with exactly the value it contains.
 *
 * @returns The raw register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint64_t dmarRegReadRaw64(PCDMAR pThis, uint16_t offReg)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs = dmarRegGetGroupRo(pThis, offReg, sizeof(uint64_t), &idxGroup);
    NOREF(idxGroup);
    return *(uint64_t *)(pabRegs + offReg);
}


/**
 * Reads a 32-bit register with exactly the value it contains along with their
 * corresponding masks
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 32-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
static void dmarRegReadRaw32Ex(PCDMAR pThis, uint16_t offReg, uint32_t *puReg, uint32_t *pfRwMask, uint32_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = dmarRegGetGroupRo(pThis, offReg, sizeof(uint32_t), &idxGroup);
    Assert(idxGroup < RT_ELEMENTS(g_apbRwMasks));
    uint8_t const *pabRwMasks   = g_apbRwMasks[idxGroup];
    uint8_t const *pabRw1cMasks = g_apbRw1cMasks[idxGroup];
    *puReg      = *(uint32_t *)(pabRegs      + offReg);
    *pfRwMask   = *(uint32_t *)(pabRwMasks   + offReg);
    *pfRw1cMask = *(uint32_t *)(pabRw1cMasks + offReg);
}


/**
 * Reads a 64-bit register with exactly the value it contains along with their
 * corresponding masks.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 64-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
static void dmarRegReadRaw64Ex(PCDMAR pThis, uint16_t offReg, uint64_t *puReg, uint64_t *pfRwMask, uint64_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = dmarRegGetGroupRo(pThis, offReg, sizeof(uint64_t), &idxGroup);
    Assert(idxGroup < RT_ELEMENTS(g_apbRwMasks));
    uint8_t const *pabRwMasks   = g_apbRwMasks[idxGroup];
    uint8_t const *pabRw1cMasks = g_apbRw1cMasks[idxGroup];
    *puReg      = *(uint64_t *)(pabRegs      + offReg);
    *pfRwMask   = *(uint64_t *)(pabRwMasks   + offReg);
    *pfRw1cMask = *(uint64_t *)(pabRw1cMasks + offReg);
}


/**
 * Writes a 32-bit register as it would be when written by software.
 * This will preserve read-only bits, mask off reserved bits and clear RW1C bits.
 *
 * @returns The value that's actually written to the register.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 32-bit value to write.
 * @param   puPrev  Where to store the register value prior to writing.
 */
static uint32_t dmarRegWrite32(PDMAR pThis, uint16_t offReg, uint32_t uReg, uint32_t *puPrev)
{
    /* Read current value from the 32-bit register. */
    uint32_t uCurReg;
    uint32_t fRwMask;
    uint32_t fRw1cMask;
    dmarRegReadRaw32Ex(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    *puPrev = uCurReg;

    uint32_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint32_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint32_t const fRw1cBits = uReg & fRw1cMask;        /* Clear 1s written to RW1C bits. */
    uint32_t const uNewReg   = (fRoBits | fRwBits) & ~fRw1cBits;

    /* Write new value to the 32-bit register. */
    dmarRegWriteRaw32(pThis, offReg, uNewReg);
    return uNewReg;
}


/**
 * Writes a 64-bit register as it would be when written by software.
 * This will preserve read-only bits, mask off reserved bits and clear RW1C bits.
 *
 * @returns The value that's actually written to the register.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 64-bit value to write.
 * @param   puPrev  Where to store the register value prior to writing.
 */
static uint64_t dmarRegWrite64(PDMAR pThis, uint16_t offReg, uint64_t uReg, uint64_t *puPrev)
{
    /* Read current value from the 64-bit register. */
    uint64_t uCurReg;
    uint64_t fRwMask;
    uint64_t fRw1cMask;
    dmarRegReadRaw64Ex(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    *puPrev = uCurReg;

    uint64_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint64_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint64_t const fRw1cBits = uReg & fRw1cMask;        /* Clear 1s written to RW1C bits. */
    uint64_t const uNewReg   = (fRoBits | fRwBits) & ~fRw1cBits;

    /* Write new value to the 64-bit register. */
    dmarRegWriteRaw64(pThis, offReg, uNewReg);
    return uNewReg;
}


/**
 * Reads a 32-bit register as it would be when read by software.
 *
 * @returns The register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint32_t dmarRegRead32(PCDMAR pThis, uint16_t offReg)
{
    return dmarRegReadRaw32(pThis, offReg);
}


/**
 * Reads a 64-bit register as it would be when read by software.
 *
 * @returns The register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint64_t dmarRegRead64(PCDMAR pThis, uint16_t offReg)
{
    return dmarRegReadRaw64(pThis, offReg);
}


/**
 * Modifies a 32-bit register.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   fAndMask    The AND mask (applied first).
 * @param   fOrMask     The OR mask.
 * @remarks This does NOT apply RO or RW1C masks while modifying the
 *          register.
 */
static void dmarRegChangeRaw32(PDMAR pThis, uint16_t offReg, uint32_t fAndMask, uint32_t fOrMask)
{
    uint32_t uReg = dmarRegReadRaw32(pThis, offReg);
    uReg = (uReg & fAndMask) | fOrMask;
    dmarRegWriteRaw32(pThis, offReg, uReg);
}


/**
 * Modifies a 64-bit register.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   fAndMask    The AND mask (applied first).
 * @param   fOrMask     The OR mask.
 * @remarks This does NOT apply RO or RW1C masks while modifying the
 *          register.
 */
static void dmarRegChangeRaw64(PDMAR pThis, uint16_t offReg, uint64_t fAndMask, uint64_t fOrMask)
{
    uint64_t uReg = dmarRegReadRaw64(pThis, offReg);
    uReg = (uReg & fAndMask) | fOrMask;
    dmarRegWriteRaw64(pThis, offReg, uReg);
}


/**
 * Checks if the invalidation-queue is empty.
 *
 * Extended version which optionally returns the current queue head and tail
 * offsets.
 *
 * @returns @c true if empty, @c false otherwise.
 * @param   pThis   The shared DMAR device state.
 * @param   poffQh  Where to store the queue head offset. Optional, can be NULL.
 * @param   poffQt  Where to store the queue tail offset. Optional, can be NULL.
 */
static bool dmarInvQueueIsEmptyEx(PCDMAR pThis, uint32_t *poffQh, uint32_t *poffQt)
{
    /* Read only the low-32 bits of the queue head and queue tail as high bits are all RsvdZ.*/
    uint32_t const uIqtReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IQT_REG);
    uint32_t const uIqhReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IQH_REG);

    /* Don't bother masking QT, QH since other bits are RsvdZ. */
    Assert(!(uIqtReg & ~VTD_BF_IQT_REG_QT_MASK));
    Assert(!(uIqhReg & ~VTD_BF_IQH_REG_QH_MASK));
    if (poffQh)
        *poffQh = uIqhReg;
    if (poffQt)
        *poffQt = uIqtReg;
    return uIqtReg == uIqhReg;
}


/**
 * Checks if the invalidation-queue is empty.
 *
 * @returns @c true if empty, @c false otherwise.
 * @param   pThis   The shared DMAR device state.
 */
static bool dmarInvQueueIsEmpty(PCDMAR pThis)
{
    return dmarInvQueueIsEmptyEx(pThis, NULL /* poffQh */,  NULL /* poffQt */);
}


/**
 * Checks if the invalidation-queue is capable of processing requests.
 *
 * @returns @c true if the invalidation-queue can process requests, @c false
 *          otherwise.
 * @param   pThis   The shared DMAR device state.
 */
static bool dmarInvQueueCanProcessRequests(PCDMAR pThis)
{
    /* Check if queued-invalidation is enabled. */
    uint32_t const uGstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
    if (uGstsReg & VTD_BF_GSTS_REG_QIES_MASK)
    {
        /* Check if there are no invalidation-queue or timeout errors. */
        uint32_t const uFstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FSTS_REG);
        if (!(uFstsReg & (VTD_BF_FSTS_REG_IQE_MASK | VTD_BF_FSTS_REG_ITE_MASK)))
            return true;
    }
    return false;
}


/**
 * Wakes up the invalidation-queue thread if there are requests to be processed.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void dmarInvQueueThreadWakeUpIfNeeded(PPDMDEVINS pDevIns)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
    Log4Func(("\n"));

    DMAR_ASSERT_LOCK_IS_OWNER(pDevIns, pThisCC);

    if (    dmarInvQueueCanProcessRequests(pThis)
        && !dmarInvQueueIsEmpty(pThis))
    {
        Log4Func(("Signaling the invalidation-queue thread\n"));
        PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEvtInvQueue);
    }
}


/**
 * Raises an event on behalf of the DMAR.
 *
 * These are events that are generated by the DMAR itself (like faults and
 * invalidation completion notifications).
 *
 * @param   pDevIns         The IOMMU device instance.
 * @param   enmEventType    The DMAR event type.
 *
 * @remarks The DMAR lock must be held while calling this function.
 */
static void dmarEventRaiseInterrupt(PPDMDEVINS pDevIns, DMAREVENTTYPE enmEventType)
{
    uint16_t offCtlReg;
    uint32_t fIntrMaskedMask;
    uint32_t fIntrPendingMask;
    uint16_t offMsiAddrLoReg;
    uint16_t offMsiAddrHiReg;
    uint16_t offMsiDataReg;
    switch (enmEventType)
    {
        case DMAREVENTTYPE_INV_COMPLETE:
        {
            offCtlReg        = VTD_MMIO_OFF_IECTL_REG;
            fIntrMaskedMask  = VTD_BF_IECTL_REG_IM_MASK;
            fIntrPendingMask = VTD_BF_IECTL_REG_IP_MASK;
            offMsiAddrLoReg  = VTD_MMIO_OFF_IEADDR_REG;
            offMsiAddrHiReg  = VTD_MMIO_OFF_IEUADDR_REG;
            offMsiDataReg    = VTD_MMIO_OFF_IEDATA_REG;
            break;
        }

        case DMAREVENTTYPE_FAULT:
        {
            offCtlReg        = VTD_MMIO_OFF_FECTL_REG;
            fIntrMaskedMask  = VTD_BF_FECTL_REG_IM_MASK;
            fIntrPendingMask = VTD_BF_FECTL_REG_IP_MASK;
            offMsiAddrLoReg  = VTD_MMIO_OFF_FEADDR_REG;
            offMsiAddrHiReg  = VTD_MMIO_OFF_FEUADDR_REG;
            offMsiDataReg    = VTD_MMIO_OFF_FEDATA_REG;
            break;
        }

        default:
        {
            /* Shouldn't ever happen. */
            AssertMsgFailedReturnVoid(("DMAR event type %#x unknown!\n", enmEventType));
        }
    }

    /* Check if software has masked the interrupt. */
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    uint32_t uCtlReg = dmarRegReadRaw32(pThis, offCtlReg);
    if (!(uCtlReg & fIntrMaskedMask))
    {
        /*
         * Interrupt is unmasked, raise it.
         * Interrupts generated by the DMAR have trigger mode and level as 0.
         * See Intel spec. 5.1.6 "Remapping Hardware Event Interrupt Programming".
         */
        MSIMSG Msi;
        Msi.Addr.au32[0] = dmarRegReadRaw32(pThis, offMsiAddrLoReg);
        Msi.Addr.au32[1] = (pThis->fExtCapReg & VTD_BF_ECAP_REG_EIM_MASK) ? dmarRegReadRaw32(pThis, offMsiAddrHiReg) : 0;
        Msi.Data.u32     = dmarRegReadRaw32(pThis, offMsiDataReg);
        Assert(Msi.Data.n.u1Level == 0);
        Assert(Msi.Data.n.u1TriggerMode == 0);

        PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
        pThisCC->CTX_SUFF(pIommuHlp)->pfnSendMsi(pDevIns, &Msi, 0 /* uTagSrc */);

        /* Clear interrupt pending bit. */
        uCtlReg &= ~fIntrPendingMask;
        dmarRegWriteRaw32(pThis, offCtlReg, uCtlReg);
    }
    else
    {
        /* Interrupt is masked, set the interrupt pending bit. */
        uCtlReg |= fIntrPendingMask;
        dmarRegWriteRaw32(pThis, offCtlReg, uCtlReg);
    }
}


/**
 * Raises an interrupt in response to a fault event.
 *
 * @param   pDevIns     The IOMMU device instance.
 *
 * @remarks This assumes the caller has already set the required status bits in the
 *          FSTS_REG (namely one or more of PPF, PFO, IQE, ICE or ITE bits).
 */
static void dmarFaultEventRaiseInterrupt(PPDMDEVINS pDevIns)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
    DMAR_ASSERT_LOCK_IS_OWNER(pDevIns, pThisCC);

#ifdef RT_STRICT
    {
        uint32_t const uFstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FSTS_REG);
        uint32_t const fFaultMask = VTD_BF_FSTS_REG_PPF_MASK | VTD_BF_FSTS_REG_PFO_MASK
                               /* | VTD_BF_FSTS_REG_APF_MASK | VTD_BF_FSTS_REG_AFO_MASK */    /* AFL not supported */
                               /* | VTD_BF_FSTS_REG_ICE_MASK | VTD_BF_FSTS_REG_ITE_MASK */    /* Device-TLBs not supported */
                                  | VTD_BF_FSTS_REG_IQE_MASK;
        Assert(uFstsReg & fFaultMask);
    }
#endif
    dmarEventRaiseInterrupt(pDevIns, DMAREVENTTYPE_FAULT);
}


#ifdef IN_RING3
/**
 * Raises an interrupt in response to an invalidation (complete) event.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void dmarR3InvEventRaiseInterrupt(PPDMDEVINS pDevIns)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
    DMAR_ASSERT_LOCK_IS_OWNER(pDevIns, pThisCC);

    uint32_t const uIcsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_ICS_REG);
    if (!(uIcsReg & VTD_BF_ICS_REG_IWC_MASK))
    {
        dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_ICS_REG, UINT32_MAX, VTD_BF_ICS_REG_IWC_MASK);
        dmarEventRaiseInterrupt(pDevIns, DMAREVENTTYPE_INV_COMPLETE);
    }
}
#endif /* IN_RING3 */


/**
 * Checks if a primary fault can be recorded.
 *
 * @returns @c true if the fault can be recorded, @c false otherwise.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pThis       The shared DMAR device state.
 *
 * @remarks Warning: This function has side-effects wrt the DMAR register state. Do
 *          NOT call it unless there is a fault condition!
 */
static bool dmarPrimaryFaultCanRecord(PPDMDEVINS pDevIns, PDMAR pThis)
{
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
    DMAR_ASSERT_LOCK_IS_OWNER(pDevIns, pThisCC);

    uint32_t uFstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FSTS_REG);
    if (uFstsReg & VTD_BF_FSTS_REG_PFO_MASK)
        return false;

    /*
     * If we add more FRCD registers, we'll have to loop through them here.
     * Since we support only one FRCD_REG, we don't support "compression of multiple faults",
     * nor do we need to increment FRI.
     *
     * See Intel VT-d spec. 7.2.1 "Primary Fault Logging".
     */
    AssertCompile(DMAR_FRCD_REG_COUNT == 1);
    uint64_t const uFrcdRegHi = dmarRegReadRaw64(pThis, DMAR_MMIO_OFF_FRCD_HI_REG);
    if (uFrcdRegHi & VTD_BF_1_FRCD_REG_F_MASK)
    {
        uFstsReg |= VTD_BF_FSTS_REG_PFO_MASK;
        dmarRegWriteRaw32(pThis, VTD_MMIO_OFF_FSTS_REG, uFstsReg);
        return false;
    }

    return true;
}


/**
 * Records a primary fault.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmDiag     The diagnostic reason.
 * @param   uFrcdHi     The FRCD_HI_REG value for this fault.
 * @param   uFrcdLo     The FRCD_LO_REG value for this fault.
 */
static void dmarPrimaryFaultRecord(PPDMDEVINS pDevIns, DMARDIAG enmDiag, uint64_t uFrcdHi, uint64_t uFrcdLo)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);

    DMAR_LOCK(pDevIns, pThisCC);

    /* Update the diagnostic reason. */
    pThis->enmDiag = enmDiag;

    /* We don't support advance fault logging. */
    Assert(!(dmarRegRead32(pThis, VTD_MMIO_OFF_GSTS_REG) & VTD_BF_GSTS_REG_AFLS_MASK));

    if (dmarPrimaryFaultCanRecord(pDevIns, pThis))
    {
        /* Update the fault recording registers with the fault information. */
        dmarRegWriteRaw64(pThis, DMAR_MMIO_OFF_FRCD_HI_REG, uFrcdHi);
        dmarRegWriteRaw64(pThis, DMAR_MMIO_OFF_FRCD_LO_REG, uFrcdLo);

        /* Set the Pending Primary Fault (PPF) field in the status register. */
        dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_FSTS_REG, UINT32_MAX, VTD_BF_FSTS_REG_PPF_MASK);

        /* Raise interrupt if necessary. */
        dmarFaultEventRaiseInterrupt(pDevIns);
    }

    DMAR_UNLOCK(pDevIns, pThisCC);
}


/**
 * Records an interrupt request fault.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmDiag     The diagnostic reason.
 * @param   enmIrFault  The interrupt fault reason.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   idxIntr     The interrupt index.
 */
static void dmarIrFaultRecord(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDIRFAULT enmIrFault, uint16_t idDevice, uint16_t idxIntr)
{
    uint64_t const uFrcdHi = RT_BF_MAKE(VTD_BF_1_FRCD_REG_SID, idDevice)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_FR,  enmIrFault)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_F,   1);
    uint64_t const uFrcdLo = (uint64_t)idxIntr << 48;
    dmarPrimaryFaultRecord(pDevIns, enmDiag, uFrcdHi, uFrcdLo);
}


/**
 * Records a qualified interrupt request fault.
 *
 * Qualified faults are those that can be suppressed by software using the FPD bit
 * in the IRTE.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmDiag     The diagnostic reason.
 * @param   enmIrFault  The interrupt fault reason.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   idxIntr     The interrupt index.
 * @param   pIrte       The IRTE that caused this fault.
 */
static void dmarIrFaultRecordQualified(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDIRFAULT enmIrFault, uint16_t idDevice,
                                       uint16_t idxIntr, PCVTD_IRTE_T pIrte)
{
    Assert(vtdIrFaultIsQualified(enmIrFault));
    Assert(pIrte);
    if (!(pIrte->au64[0] & VTD_BF_0_IRTE_FPD_MASK))
        return dmarIrFaultRecord(pDevIns, enmDiag, enmIrFault, idDevice, idxIntr);
}


/**
 * Records an address translation fault (extensive version).
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmDiag     The diagnostic reason.
 * @param   enmAtFault  The address translation fault reason.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   uFaultAddr  The page address of the faulted request.
 * @param   enmReqType  The type of the faulted request.
 * @param   uAddrType   The address type of the faulted request (only applicable
 *                      when device-TLB is supported).
 * @param   fHasPasid   Whether the faulted request has a PASID TLP prefix.
 * @param   uPasid      The PASID value when a PASID TLP prefix is present.
 * @param   fReqAttr    The attributes of the faulted requested (VTD_REQ_ATTR_XXX).
 */
static void dmarAtFaultRecordEx(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDATFAULT enmAtFault, uint16_t idDevice,
                                uint64_t uFaultAddr, VTDREQTYPE enmReqType, uint8_t uAddrType, bool fHasPasid, uint32_t uPasid,
                                uint8_t fReqAttr)
{
    uint8_t const fType1 = enmReqType & RT_BIT(1);
    uint8_t const fType2 = enmReqType & RT_BIT(0);
    uint8_t const fExec = fReqAttr & VTD_REQ_ATTR_EXE;
    uint8_t const fPriv = fReqAttr & VTD_REQ_ATTR_PRIV;
    uint64_t const uFrcdHi = RT_BF_MAKE(VTD_BF_1_FRCD_REG_SID,  idDevice)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_T2,   fType2)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_PP,   fHasPasid)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_EXE,  fExec)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_PRIV, fPriv)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_FR,   enmAtFault)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_PV,   uPasid)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_AT,   uAddrType)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_T1,   fType1)
                           | RT_BF_MAKE(VTD_BF_1_FRCD_REG_F,    1);
    uint64_t const uFrcdLo = uFaultAddr & X86_PAGE_BASE_MASK;
    dmarPrimaryFaultRecord(pDevIns, enmDiag, uFrcdHi, uFrcdLo);
}


/**
 * Records an address translation fault.
 *
 * This is to be used when Device-TLB, and PASIDs are not supported or for requests
 * where the device-TLB and PASID is not relevant/present.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmDiag     The diagnostic reason.
 * @param   enmAtFault  The address translation fault reason.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   uFaultAddr  The page address of the faulted request.
 * @param   enmReqType  The type of the faulted request.
 */
static void dmarAtFaultRecord(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDATFAULT enmAtFault, uint16_t idDevice,
                              uint64_t uFaultAddr, VTDREQTYPE enmReqType)
{
    dmarAtFaultRecordEx(pDevIns, enmDiag, enmAtFault, idDevice, uFaultAddr, enmReqType, 0 /* uAddrType */,
                          false /* fHasPasid */, 0 /* uPasid */, 0 /* fReqAttr */);
}


/**
 * Records a qualified address translation fault.
 *
 * Qualified faults are those that can be suppressed by software using the FPD bit
 * in the contex entry, scalable-mode context entry etc.
 *
 * This is to be used when Device-TLB, and PASIDs are not supported or for requests
 * where the device-TLB and PASID is not relevant/present.
 *
 * @param   pDevIns             The IOMMU device instance.
 * @param   enmDiag             The diagnostic reason.
 * @param   enmAtFault          The address translation fault reason.
 * @param   idDevice            The device ID (bus, device, function).
 * @param   uFaultAddr          The page address of the faulted request.
 * @param   enmReqType          The type of the faulted request.
 * @param   uPagingEntryQw0     The first qword of the paging entry.
 */
static void dmarAtFaultQualifiedRecord(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDATFAULT enmAtFault, uint16_t idDevice,
                                       uint64_t uFaultAddr, VTDREQTYPE enmReqType, uint64_t uPagingEntryQw0)
{
    AssertCompile(    VTD_BF_0_CONTEXT_ENTRY_FPD_MASK       == 0x2
                   && VTD_BF_0_SM_CONTEXT_ENTRY_FPD_MASK    == 0x2
                   && VTD_BF_0_SM_CONTEXT_ENTRY_FPD_MASK    == 0x2
                   && VTD_BF_SM_PASID_DIR_ENTRY_FPD_MASK    == 0x2
                   && VTD_BF_0_SM_PASID_TBL_ENTRY_FPD_MASK  == 0x2);
    if (!(uPagingEntryQw0 & VTD_BF_0_CONTEXT_ENTRY_FPD_MASK))
        dmarAtFaultRecordEx(pDevIns, enmDiag, enmAtFault, idDevice, uFaultAddr, enmReqType, 0 /* uAddrType */,
                            false /* fHasPasid */, 0 /* uPasid */, 0 /* fReqAttr */);
}


/**
 * Records an IQE fault.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   enmIqei     The IQE information.
 * @param   enmDiag     The diagnostic reason.
 */
static void dmarIqeFaultRecord(PPDMDEVINS pDevIns, DMARDIAG enmDiag, VTDIQEI enmIqei)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);

    DMAR_LOCK(pDevIns, pThisCC);

    /* Update the diagnostic reason. */
    pThis->enmDiag = enmDiag;

    /* Set the error bit. */
    uint32_t const fIqe = RT_BF_MAKE(VTD_BF_FSTS_REG_IQE, 1);
    dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_FSTS_REG, UINT32_MAX, fIqe);

    /* Set the error information. */
    uint64_t const fIqei = RT_BF_MAKE(VTD_BF_IQERCD_REG_IQEI, enmIqei);
    dmarRegChangeRaw64(pThis, VTD_MMIO_OFF_IQERCD_REG, UINT64_MAX, fIqei);

    dmarFaultEventRaiseInterrupt(pDevIns);

    DMAR_UNLOCK(pDevIns, pThisCC);
}


/**
 * Handles writes to GCMD_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uGcmdReg    The value written to GCMD_REG.
 */
static VBOXSTRICTRC dmarGcmdRegWrite(PPDMDEVINS pDevIns, uint32_t uGcmdReg)
{
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    uint32_t const uGstsReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
    uint32_t const fChanged   = uGstsReg ^ uGcmdReg;
    uint64_t const fExtCapReg = pThis->fExtCapReg;

    /* Queued-invalidation. */
    if (   (fExtCapReg & VTD_BF_ECAP_REG_QI_MASK)
        && (fChanged & VTD_BF_GCMD_REG_QIE_MASK))
    {
        if (uGcmdReg & VTD_BF_GCMD_REG_QIE_MASK)
        {
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_QIES_MASK);
            dmarInvQueueThreadWakeUpIfNeeded(pDevIns);
        }
        else
        {
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, ~VTD_BF_GSTS_REG_QIES_MASK, 0 /* fOrMask */);
            dmarRegWriteRaw32(pThis, VTD_MMIO_OFF_IQH_REG, 0);
        }
    }

    if (fExtCapReg & VTD_BF_ECAP_REG_IR_MASK)
    {
        /* Set Interrupt Remapping Table Pointer (SIRTP). */
        if (uGcmdReg & VTD_BF_GCMD_REG_SIRTP_MASK)
        {
            /** @todo Perform global invalidation of all interrupt-entry cache when ESIRTPS is
             *        supported. */
            pThis->uIrtaReg = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IRTA_REG);
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_IRTPS_MASK);
        }

        /* Interrupt remapping. */
        if (fChanged & VTD_BF_GCMD_REG_IRE_MASK)
        {
            if (uGcmdReg & VTD_BF_GCMD_REG_IRE_MASK)
                dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_IRES_MASK);
            else
                dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, ~VTD_BF_GSTS_REG_IRES_MASK, 0 /* fOrMask */);
        }

        /* Compatibility format interrupts. */
        if (fChanged & VTD_BF_GCMD_REG_CFI_MASK)
        {
            if (uGcmdReg & VTD_BF_GCMD_REG_CFI_MASK)
                dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_CFIS_MASK);
            else
                dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, ~VTD_BF_GSTS_REG_CFIS_MASK, 0 /* fOrMask */);
        }
    }

    /* Set Root Table Pointer (SRTP). */
    if (uGcmdReg & VTD_BF_GCMD_REG_SRTP_MASK)
    {
        /** @todo Perform global invalidation of all remapping translation caches when
         *        ESRTPS is supported. */
        pThis->uRtaddrReg = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_RTADDR_REG);
        dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_RTPS_MASK);
    }

    /* Translation (DMA remapping). */
    if (fChanged & VTD_BF_GCMD_REG_TE_MASK)
    {
        if (uGcmdReg & VTD_BF_GCMD_REG_TE_MASK)
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, UINT32_MAX, VTD_BF_GSTS_REG_TES_MASK);
        else
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_GSTS_REG, ~VTD_BF_GSTS_REG_TES_MASK, 0 /* fOrMask */);
    }

    return VINF_SUCCESS;
}


/**
 * Handles writes to CCMD_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   offReg      The MMIO register offset.
 * @param   cbReg       The size of the MMIO access (in bytes).
 * @param   uCcmdReg    The value written to CCMD_REG.
 */
static VBOXSTRICTRC dmarCcmdRegWrite(PPDMDEVINS pDevIns, uint16_t offReg, uint8_t cbReg, uint64_t uCcmdReg)
{
    /* At present, we only care about responding to high 32-bits writes, low 32-bits are data. */
    if (offReg + cbReg > VTD_MMIO_OFF_CCMD_REG + 4)
    {
        /* Check if we need to invalidate the context-context. */
        bool const fIcc = RT_BF_GET(uCcmdReg, VTD_BF_CCMD_REG_ICC);
        if (fIcc)
        {
            PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
            uint8_t const uMajorVersion = RT_BF_GET(pThis->uVerReg, VTD_BF_VER_REG_MAX);
            if (uMajorVersion < 6)
            {
                /* Register-based invalidation can only be used when queued-invalidations are not enabled. */
                uint32_t const uGstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
                if (!(uGstsReg & VTD_BF_GSTS_REG_QIES_MASK))
                {
                    /* Verify table translation mode is legacy. */
                    uint8_t const fTtm = RT_BF_GET(pThis->uRtaddrReg, VTD_BF_RTADDR_REG_TTM);
                    if (fTtm == VTD_TTM_LEGACY_MODE)
                    {
                        /** @todo Invalidate. */
                        return VINF_SUCCESS;
                    }
                    pThis->enmDiag = kDmarDiag_CcmdReg_Ttm_Invalid;
                }
                else
                    pThis->enmDiag = kDmarDiag_CcmdReg_Qi_Enabled;
            }
            else
                pThis->enmDiag = kDmarDiag_CcmdReg_NotSupported;
            dmarRegChangeRaw64(pThis, VTD_MMIO_OFF_GSTS_REG, ~VTD_BF_CCMD_REG_CAIG_MASK, 0 /* fOrMask */);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Handles writes to FECTL_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uFectlReg   The value written to FECTL_REG.
 */
static VBOXSTRICTRC dmarFectlRegWrite(PPDMDEVINS pDevIns, uint32_t uFectlReg)
{
    /*
     * If software unmasks the interrupt when the interrupt is pending, we must raise
     * the interrupt now (which will consequently clear the interrupt pending (IP) bit).
     */
    if (    (uFectlReg & VTD_BF_FECTL_REG_IP_MASK)
        && ~(uFectlReg & VTD_BF_FECTL_REG_IM_MASK))
        dmarEventRaiseInterrupt(pDevIns, DMAREVENTTYPE_FAULT);
    return VINF_SUCCESS;
}


/**
 * Handles writes to FSTS_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uFstsReg    The value written to FSTS_REG.
 * @param   uPrev       The value in FSTS_REG prior to writing it.
 */
static VBOXSTRICTRC dmarFstsRegWrite(PPDMDEVINS pDevIns, uint32_t uFstsReg, uint32_t uPrev)
{
    /*
     * If software clears other status bits in FSTS_REG (pertaining to primary fault logging),
     * the interrupt pending (IP) bit must be cleared.
     *
     * See Intel VT-d spec. 10.4.10 "Fault Event Control Register".
     */
    uint32_t const fChanged = uPrev ^ uFstsReg;
    if (fChanged & (  VTD_BF_FSTS_REG_ICE_MASK | VTD_BF_FSTS_REG_ITE_MASK
                    | VTD_BF_FSTS_REG_IQE_MASK | VTD_BF_FSTS_REG_PFO_MASK))
    {
        PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
        dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_FECTL_REG, ~VTD_BF_FECTL_REG_IP_MASK, 0 /* fOrMask */);
    }
    return VINF_SUCCESS;
}


/**
 * Handles writes to IQT_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   offReg      The MMIO register offset.
 * @param   uIqtReg     The value written to IQT_REG.
 */
static VBOXSTRICTRC dmarIqtRegWrite(PPDMDEVINS pDevIns, uint16_t offReg, uint64_t uIqtReg)
{
    /* We only care about the low 32-bits, high 32-bits are reserved. */
    Assert(offReg == VTD_MMIO_OFF_IQT_REG);
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);

    /* Paranoia. */
    Assert(!(uIqtReg & ~VTD_BF_IQT_REG_QT_MASK));

    uint32_t const offQt   = uIqtReg;
    uint64_t const uIqaReg = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQA_REG);
    uint8_t const  fDw     = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_DW);

    /* If the descriptor width is 256-bits, the queue tail offset must be aligned accordingly. */
    if (   fDw != VTD_IQA_REG_DW_256_BIT
        || !(offQt & RT_BIT(4)))
        dmarInvQueueThreadWakeUpIfNeeded(pDevIns);
    else
    {
        /* Hardware treats bit 4 as RsvdZ in this situation, so clear it. */
        dmarRegChangeRaw32(pThis, offReg, ~RT_BIT(4), 0 /* fOrMask */);
        dmarIqeFaultRecord(pDevIns, kDmarDiag_IqtReg_Qt_NotAligned, VTDIQEI_QUEUE_TAIL_MISALIGNED);
    }
    return VINF_SUCCESS;
}


/**
 * Handles writes to IQA_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   offReg      The MMIO register offset.
 * @param   uIqaReg     The value written to IQA_REG.
 */
static VBOXSTRICTRC dmarIqaRegWrite(PPDMDEVINS pDevIns, uint16_t offReg, uint64_t uIqaReg)
{
    /* At present, we only care about the low 32-bits, high 32-bits are data. */
    Assert(offReg == VTD_MMIO_OFF_IQA_REG); NOREF(offReg);

    /** @todo What happens if IQA_REG is written when dmarInvQueueCanProcessRequests
     *        returns true? The Intel VT-d spec. doesn't state anywhere that it
     *        cannot happen or that it's ignored when it does happen. */

    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    uint8_t const fDw = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_DW);
    if (fDw == VTD_IQA_REG_DW_256_BIT)
    {
        bool const fSupports256BitDw = (pThis->fExtCapReg & (VTD_BF_ECAP_REG_SMTS_MASK | VTD_BF_ECAP_REG_ADMS_MASK));
        if (fSupports256BitDw)
        { /* likely */ }
        else
            dmarIqeFaultRecord(pDevIns, kDmarDiag_IqaReg_Dw_256_Invalid, VTDIQEI_INVALID_DESCRIPTOR_WIDTH);
    }
    /* else: 128-bit descriptor width is validated lazily, see explanation in dmarR3InvQueueProcessRequests. */

    return VINF_SUCCESS;
}


/**
 * Handles writes to ICS_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uIcsReg     The value written to ICS_REG.
 */
static VBOXSTRICTRC dmarIcsRegWrite(PPDMDEVINS pDevIns, uint32_t uIcsReg)
{
    /*
     * If the IP field is set when software services the interrupt condition,
     * (by clearing the IWC field), the IP field must be cleared.
     */
    if (!(uIcsReg & VTD_BF_ICS_REG_IWC_MASK))
    {
        PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
        dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_IECTL_REG, ~VTD_BF_IECTL_REG_IP_MASK, 0 /* fOrMask */);
    }
    return VINF_SUCCESS;
}


/**
 * Handles writes to IECTL_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uIectlReg   The value written to IECTL_REG.
 */
static VBOXSTRICTRC dmarIectlRegWrite(PPDMDEVINS pDevIns, uint32_t uIectlReg)
{
    /*
     * If software unmasks the interrupt when the interrupt is pending, we must raise
     * the interrupt now (which will consequently clear the interrupt pending (IP) bit).
     */
    if (    (uIectlReg & VTD_BF_IECTL_REG_IP_MASK)
        && ~(uIectlReg & VTD_BF_IECTL_REG_IM_MASK))
        dmarEventRaiseInterrupt(pDevIns, DMAREVENTTYPE_INV_COMPLETE);
    return VINF_SUCCESS;
}


/**
 * Handles writes to FRCD_REG (High 64-bits).
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   offReg      The MMIO register offset.
 * @param   cbReg       The size of the MMIO access (in bytes).
 * @param   uFrcdHiReg  The value written to FRCD_REG.
 * @param   uPrev       The value in FRCD_REG prior to writing it.
 */
static VBOXSTRICTRC dmarFrcdHiRegWrite(PPDMDEVINS pDevIns, uint16_t offReg, uint8_t cbReg, uint64_t uFrcdHiReg, uint64_t uPrev)
{
    /* We only care about responding to high 32-bits, low 32-bits are read-only. */
    if (offReg + cbReg > DMAR_MMIO_OFF_FRCD_HI_REG + 4)
    {
        /*
         * If software cleared the RW1C F (fault) bit in all FRCD_REGs, hardware clears the
         * Primary Pending Fault (PPF) and the interrupt pending (IP) bits. Our implementation
         * has only 1 FRCD register.
         *
         * See Intel VT-d spec. 10.4.10 "Fault Event Control Register".
         */
        AssertCompile(DMAR_FRCD_REG_COUNT == 1);
        uint64_t const fChanged = uPrev ^ uFrcdHiReg;
        if (fChanged & VTD_BF_1_FRCD_REG_F_MASK)
        {
            Assert(!(uFrcdHiReg & VTD_BF_1_FRCD_REG_F_MASK));  /* Software should only ever be able to clear this bit. */
            PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_FSTS_REG, ~VTD_BF_FSTS_REG_PPF_MASK, 0 /* fOrMask */);
            dmarRegChangeRaw32(pThis, VTD_MMIO_OFF_FECTL_REG, ~VTD_BF_FECTL_REG_IP_MASK, 0 /* fOrMask */);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Performs a PCI target abort for a DMA remapping (DR) operation.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void dmarDrTargetAbort(PPDMDEVINS pDevIns)
{
    /** @todo r=ramshankar: I don't know for sure if a PCI target abort is caused or not
     *        as the Intel VT-d spec. is vague. Wording seems to suggest it does, but
     *        who knows. */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    uint16_t const u16Status = PDMPciDevGetStatus(pPciDev) | VBOX_PCI_STATUS_SIG_TARGET_ABORT;
    PDMPciDevSetStatus(pPciDev, u16Status);
}


/**
 * Reads a root entry from guest memory.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   uRtaddrReg      The current RTADDR_REG value.
 * @param   idxRootEntry    The index of the root entry to read.
 * @param   pRootEntry      Where to store the read root entry.
 */
static int dmarDrReadRootEntry(PPDMDEVINS pDevIns, uint64_t uRtaddrReg, uint8_t idxRootEntry, PVTD_ROOT_ENTRY_T pRootEntry)
{
    size_t const   cbRootEntry     = sizeof(*pRootEntry);
    RTGCPHYS const GCPhysRootEntry = (uRtaddrReg & VTD_BF_RTADDR_REG_RTA_MASK) + (idxRootEntry * cbRootEntry);
    return PDMDevHlpPhysReadMeta(pDevIns, GCPhysRootEntry, pRootEntry, cbRootEntry);
}


/**
 * Reads a context entry from guest memory.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   GCPhysCtxTable  The physical address of the context table.
 * @param   idxCtxEntry     The index of the context entry to read.
 * @param   pCtxEntry       Where to store the read context entry.
 */
static int dmarDrReadCtxEntry(PPDMDEVINS pDevIns, RTGCPHYS GCPhysCtxTable, uint8_t idxCtxEntry, PVTD_CONTEXT_ENTRY_T pCtxEntry)
{
    size_t const   cbCtxEntry     = sizeof(*pCtxEntry);
    RTGCPHYS const GCPhysCtxEntry = GCPhysCtxTable + (idxCtxEntry * cbCtxEntry);
    return PDMDevHlpPhysReadMeta(pDevIns, GCPhysCtxEntry, pCtxEntry, cbCtxEntry);
}


/**
 * Handles remapping of DMA address requests in legacy mode.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   uRtaddrReg      The current RTADDR_REG value.
 * @param   pAddrRemap      The DMA address remap info.
 */
static int dmarDrLegacyModeRemapAddr(PPDMDEVINS pDevIns, uint64_t uRtaddrReg, PDMARADDRMAP pAddrRemap)
{
    uint8_t const idxRootEntry = RT_HI_U8(pAddrRemap->idDevice);
    VTD_ROOT_ENTRY_T RootEntry;
    int rc = dmarDrReadRootEntry(pDevIns, uRtaddrReg, idxRootEntry, &RootEntry);
    if (RT_SUCCESS(rc))
    {
        uint64_t const uRootEntryQword0 = RootEntry.au64[0];
        uint64_t const uRootEntryQword1 = RootEntry.au64[1];
        bool const fRootEntryPresent = RT_BF_GET(uRootEntryQword0, VTD_BF_0_ROOT_ENTRY_P);
        if (fRootEntryPresent)
        {
            if (   !(uRootEntryQword0 & ~VTD_ROOT_ENTRY_0_VALID_MASK)
                && !(uRootEntryQword1 & ~VTD_ROOT_ENTRY_1_VALID_MASK))
            {
                RTGCPHYS const GCPhysCtxTable = RT_BF_GET(uRootEntryQword0, VTD_BF_0_ROOT_ENTRY_CTP);
                uint8_t const idxCtxEntry = RT_LO_U8(pAddrRemap->idDevice);
                VTD_CONTEXT_ENTRY_T CtxEntry;
                rc = dmarDrReadCtxEntry(pDevIns, GCPhysCtxTable, idxCtxEntry, &CtxEntry);
                if (RT_SUCCESS(rc))
                {
                    uint64_t const uCtxEntryQword0 = CtxEntry.au64[0];
                    uint64_t const uCtxEntryQword1 = CtxEntry.au64[1];
                    bool const fCtxEntryPresent = RT_BF_GET(uCtxEntryQword0, VTD_BF_0_CONTEXT_ENTRY_P);
                    if (fCtxEntryPresent)
                    {
                        if (   !(uCtxEntryQword0 & ~VTD_CONTEXT_ENTRY_0_VALID_MASK)
                            && !(uCtxEntryQword1 & ~VTD_CONTEXT_ENTRY_1_VALID_MASK))
                        {
                            /** @todo Handle context entry validation and processing. */
                            return VERR_NOT_IMPLEMENTED;
                        }
                        else
                            dmarAtFaultQualifiedRecord(pDevIns, kDmarDiag_Atf_Lct_3, VTDATFAULT_LCT_3, pAddrRemap->idDevice,
                                                       pAddrRemap->uDmaAddr, pAddrRemap->enmReqType, uCtxEntryQword0);
                    }
                    else
                        dmarAtFaultQualifiedRecord(pDevIns, kDmarDiag_Atf_Lct_2, VTDATFAULT_LCT_2, pAddrRemap->idDevice,
                                                   pAddrRemap->uDmaAddr, pAddrRemap->enmReqType, uCtxEntryQword0);
                }
                else
                    dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Lct_1, VTDATFAULT_LCT_1, pAddrRemap->idDevice, pAddrRemap->uDmaAddr,
                                      pAddrRemap->enmReqType);
            }
            else
                dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Lrt_3, VTDATFAULT_LRT_3, pAddrRemap->idDevice, pAddrRemap->uDmaAddr,
                                  pAddrRemap->enmReqType);
        }
        else
            dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Lrt_2, VTDATFAULT_LRT_2, pAddrRemap->idDevice, pAddrRemap->uDmaAddr,
                              pAddrRemap->enmReqType);
    }
    else
        dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Lrt_1, VTDATFAULT_LRT_1, pAddrRemap->idDevice, pAddrRemap->uDmaAddr,
                          pAddrRemap->enmReqType);
    return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
}


/**
 * Handles remapping of DMA address requests in scalable mode.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   uRtaddrReg      The current RTADDR_REG value.
 * @param   pAddrRemap      The DMA address remap info.
 */
static int dmarDrScalableModeRemapAddr(PPDMDEVINS pDevIns, uint64_t uRtaddrReg, PDMARADDRMAP pAddrRemap)
{
    PCDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    if (pThis->fExtCapReg & VTD_BF_ECAP_REG_SMTS_MASK)
    {
        RT_NOREF1(uRtaddrReg);
        return VERR_NOT_IMPLEMENTED;
    }

    dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Rta_1_3, VTDATFAULT_RTA_1_3, pAddrRemap->idDevice, pAddrRemap->uDmaAddr,
                      pAddrRemap->enmReqType);
    return VERR_IOMMU_ADDR_TRANSLATION_FAILED;
}


/**
 * Memory access bulk (one or more 4K pages) request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   idDevice        The device ID (bus, device, function).
 * @param   cIovas          The number of addresses being accessed.
 * @param   pauIovas        The I/O virtual addresses for each page being accessed.
 * @param   fFlags          The access flags, see PDMIOMMU_MEM_F_XXX.
 * @param   paGCPhysSpa     Where to store the translated physical addresses.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuIntelMemBulkAccess(PPDMDEVINS pDevIns, uint16_t idDevice, size_t cIovas, uint64_t const *pauIovas,
                                                 uint32_t fFlags, PRTGCPHYS paGCPhysSpa)
{
    RT_NOREF6(pDevIns, idDevice, cIovas, pauIovas, fFlags, paGCPhysSpa);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Memory access transaction from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   idDevice        The device ID (bus, device, function).
 * @param   uIova           The I/O virtual address being accessed.
 * @param   cbIova          The size of the access.
 * @param   fFlags          The access flags, see PDMIOMMU_MEM_F_XXX.
 * @param   pGCPhysSpa      Where to store the translated system physical address.
 * @param   pcbContiguous   Where to store the number of contiguous bytes translated
 *                          and permission-checked.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuIntelMemAccess(PPDMDEVINS pDevIns, uint16_t idDevice, uint64_t uIova, size_t cbIova,
                                             uint32_t fFlags, PRTGCPHYS pGCPhysSpa, size_t *pcbContiguous)
{
    /* Validate. */
    AssertPtr(pDevIns);
    AssertPtr(pGCPhysSpa);
    AssertPtr(pcbContiguous);
    Assert(cbIova > 0);         /** @todo Are we going to support ZLR (zero-length reads to write-only pages)? */
    Assert(!(fFlags & ~PDMIOMMU_MEM_F_VALID_MASK));

    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);

    DMAR_LOCK(pDevIns, pThisCC);
    uint32_t const uGstsReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
    uint64_t const uRtaddrReg = pThis->uRtaddrReg;
    DMAR_UNLOCK(pDevIns, pThisCC);

    if (uGstsReg & VTD_BF_GSTS_REG_TES_MASK)
    {
        VTDREQTYPE enmReqType;
        if (fFlags & PDMIOMMU_MEM_F_READ)
        {
            enmReqType = VTDREQTYPE_READ;
            STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMemRead));
        }
        else
        {
            enmReqType = VTDREQTYPE_WRITE;
            STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMemWrite));
        }

        DMARADDRMAP AddrRemap;
        AddrRemap.idDevice     = idDevice;
        AddrRemap.enmReqType   = enmReqType;
        AddrRemap.uDmaAddr     = uIova;
        AddrRemap.cbDma        = cbIova;
        AddrRemap.GCPhysSpa    = NIL_RTGCPHYS;
        AddrRemap.cbContiguous = 0;

        int rc;
        uint8_t const fTtm = RT_BF_GET(uRtaddrReg, VTD_BF_RTADDR_REG_TTM);
        switch (fTtm)
        {
            case VTD_TTM_LEGACY_MODE:
            {
                rc = dmarDrLegacyModeRemapAddr(pDevIns, uRtaddrReg, &AddrRemap);
                break;
            }

            case VTD_TTM_SCALABLE_MODE:
            {
                rc = dmarDrScalableModeRemapAddr(pDevIns, uRtaddrReg, &AddrRemap);
                break;
            }

            case VTD_TTM_ABORT_DMA_MODE:
            {
                rc = VERR_IOMMU_ADDR_TRANSLATION_FAILED;
                if (pThis->fExtCapReg & VTD_BF_ECAP_REG_ADMS_MASK)
                    dmarDrTargetAbort(pDevIns);
                else
                    dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Rta_1_1, VTDATFAULT_RTA_1_1, idDevice, uIova, enmReqType);
                break;
            }

            default:
            {
                rc = VERR_IOMMU_ADDR_TRANSLATION_FAILED;
                dmarAtFaultRecord(pDevIns, kDmarDiag_Atf_Rta_1_2, VTDATFAULT_RTA_1_2, idDevice, uIova, enmReqType);
                break;
            }
        }

        *pcbContiguous = AddrRemap.cbContiguous;
        *pGCPhysSpa    = AddrRemap.GCPhysSpa;
        return rc;
    }

    *pGCPhysSpa    = uIova;
    *pcbContiguous = cbIova;
    return VINF_SUCCESS;
}


/**
 * Reads an IRTE from guest memory.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uIrtaReg    The IRTA_REG.
 * @param   idxIntr     The interrupt index.
 * @param   pIrte       Where to store the read IRTE.
 */
static int dmarIrReadIrte(PPDMDEVINS pDevIns, uint64_t uIrtaReg, uint16_t idxIntr, PVTD_IRTE_T pIrte)
{
    Assert(idxIntr < VTD_IRTA_REG_GET_ENTRY_COUNT(uIrtaReg));

    size_t const   cbIrte     = sizeof(*pIrte);
    RTGCPHYS const GCPhysIrte = (uIrtaReg & VTD_BF_IRTA_REG_IRTA_MASK) + (idxIntr * cbIrte);
    return PDMDevHlpPhysReadMeta(pDevIns, GCPhysIrte, pIrte, cbIrte);
}


/**
 * Remaps the source MSI to the destination MSI given the IRTE.
 *
 * @param   fExtIntrMode    Whether extended interrupt mode is enabled (i.e
 *                          IRTA_REG.EIME).
 * @param   pIrte           The IRTE used for the remapping.
 * @param   pMsiIn          The source MSI (currently unused).
 * @param   pMsiOut         Where to store the remapped MSI.
 */
static void dmarIrRemapFromIrte(bool fExtIntrMode, PCVTD_IRTE_T pIrte, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    NOREF(pMsiIn);
    uint64_t const uIrteQword0 = pIrte->au64[0];

    /*
     * Let's start with a clean slate and preserve unspecified bits if the need arises.
     * For instance, address bits 1:0 is supposed to be "ignored" by remapping hardware,
     * but it's not clear if hardware zeroes out these bits in the remapped MSI or if
     * it copies it from the source MSI.
     */
    RT_ZERO(*pMsiOut);
    pMsiOut->Addr.n.u1DestMode  = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_DM);
    pMsiOut->Addr.n.u1RedirHint = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_RH);
    pMsiOut->Addr.n.u12Addr     = VBOX_MSI_ADDR_BASE >> VBOX_MSI_ADDR_SHIFT;
    if (fExtIntrMode)
    {
        /*
         * Apparently the DMAR stuffs the high 24-bits of the destination ID into the
         * high 24-bits of the upper 32-bits of the message address, see @bugref{9967#c22}.
         */
        uint32_t const idDest = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_DST);
        pMsiOut->Addr.n.u8DestId = idDest;
        pMsiOut->Addr.n.u32Rsvd0 = idDest & UINT32_C(0xffffff00);
    }
    else
        pMsiOut->Addr.n.u8DestId = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_DST_XAPIC);

    pMsiOut->Data.n.u8Vector       = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_V);
    pMsiOut->Data.n.u3DeliveryMode = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_DLM);
    pMsiOut->Data.n.u1Level        = 1;
    pMsiOut->Data.n.u1TriggerMode  = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_TM);
}


/**
 * Handles remapping of interrupts in remappable interrupt format.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   uIrtaReg    The IRTA_REG.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 */
static int dmarIrRemapIntr(PPDMDEVINS pDevIns, uint64_t uIrtaReg, uint16_t idDevice, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    Assert(pMsiIn->Addr.dmar_remap.fIntrFormat == VTD_INTR_FORMAT_REMAPPABLE);

    /* Validate reserved bits in the interrupt request. */
    AssertCompile(VTD_REMAPPABLE_MSI_ADDR_VALID_MASK == UINT32_MAX);
    if (!(pMsiIn->Data.u32 & ~VTD_REMAPPABLE_MSI_DATA_VALID_MASK))
    {
        /* Compute the index into the interrupt remap table. */
        uint16_t const uHandleHi       = RT_BF_GET(pMsiIn->Addr.au32[0], VTD_BF_REMAPPABLE_MSI_ADDR_HANDLE_HI);
        uint16_t const uHandleLo       = RT_BF_GET(pMsiIn->Addr.au32[0], VTD_BF_REMAPPABLE_MSI_ADDR_HANDLE_LO);
        uint16_t const uHandle         = uHandleLo | (uHandleHi << 15);
        bool const     fSubHandleValid = RT_BF_GET(pMsiIn->Addr.au32[0], VTD_BF_REMAPPABLE_MSI_ADDR_SHV);
        uint16_t const idxIntr         = fSubHandleValid
                                       ? uHandle + RT_BF_GET(pMsiIn->Data.u32, VTD_BF_REMAPPABLE_MSI_DATA_SUBHANDLE)
                                       : uHandle;

        /* Validate the index. */
        uint32_t const cEntries = VTD_IRTA_REG_GET_ENTRY_COUNT(uIrtaReg);
        if (idxIntr < cEntries)
        {
            /** @todo Implement and read IRTE from interrupt-entry cache here. */

            /* Read the interrupt remap table entry (IRTE) at the index. */
            VTD_IRTE_T Irte;
            int rc = dmarIrReadIrte(pDevIns, uIrtaReg, idxIntr, &Irte);
            if (RT_SUCCESS(rc))
            {
                /* Check if the IRTE is present (this must be done -before- checking reserved bits). */
                uint64_t const uIrteQword0 = Irte.au64[0];
                uint64_t const uIrteQword1 = Irte.au64[1];
                bool const fPresent = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_P);
                if (fPresent)
                {
                    /* Validate reserved bits in the IRTE. */
                    bool const     fExtIntrMode  = RT_BF_GET(uIrtaReg, VTD_BF_IRTA_REG_EIME);
                    uint64_t const fQw0ValidMask = fExtIntrMode ? VTD_IRTE_0_X2APIC_VALID_MASK : VTD_IRTE_0_XAPIC_VALID_MASK;
                    if (   !(uIrteQword0 & ~fQw0ValidMask)
                        && !(uIrteQword1 & ~VTD_IRTE_1_VALID_MASK))
                    {
                        /* Validate requester id (the device ID) as configured in the IRTE. */
                        bool     fSrcValid;
                        DMARDIAG enmIrDiag;
                        uint8_t const fSvt = RT_BF_GET(uIrteQword1, VTD_BF_1_IRTE_SVT);
                        switch (fSvt)
                        {
                            case VTD_IRTE_SVT_NONE:
                            {
                                fSrcValid = true;
                                enmIrDiag = kDmarDiag_None;
                                break;
                            }

                            case VTD_IRTE_SVT_VALIDATE_MASK:
                            {
                                static uint16_t const s_afValidMasks[] = { 0xffff, 0xfffb, 0xfff9, 0xfff8 };
                                uint8_t const idxMask     = RT_BF_GET(uIrteQword1, VTD_BF_1_IRTE_SQ) & 3;
                                uint16_t const fValidMask = s_afValidMasks[idxMask];
                                uint16_t const idSource   = RT_BF_GET(uIrteQword1, VTD_BF_1_IRTE_SID);
                                fSrcValid = (idDevice & fValidMask) == (idSource & fValidMask);
                                enmIrDiag = kDmarDiag_Ir_Rfi_Irte_Svt_Masked;
                                break;
                            }

                            case VTD_IRTE_SVT_VALIDATE_BUS_RANGE:
                            {
                                uint16_t const idSource   = RT_BF_GET(uIrteQword1, VTD_BF_1_IRTE_SID);
                                uint8_t const uBusFirst   = RT_HI_U8(idSource);
                                uint8_t const uBusLast    = RT_LO_U8(idSource);
                                uint8_t const idDeviceBus = idDevice >> VBOX_PCI_BUS_SHIFT;
                                fSrcValid = (idDeviceBus >= uBusFirst && idDeviceBus <= uBusLast);
                                enmIrDiag = kDmarDiag_Ir_Rfi_Irte_Svt_Bus;
                                break;
                            }

                            default:
                            {
                                fSrcValid = false;
                                enmIrDiag = kDmarDiag_Ir_Rfi_Irte_Svt_Bus;
                                break;
                            }
                        }

                        if (fSrcValid)
                        {
                            uint8_t const fPostedMode = RT_BF_GET(uIrteQword0, VTD_BF_0_IRTE_IM);
                            if (!fPostedMode)
                            {
                                dmarIrRemapFromIrte(fExtIntrMode, &Irte, pMsiIn, pMsiOut);
                                return VINF_SUCCESS;
                            }
                            dmarIrFaultRecordQualified(pDevIns, kDmarDiag_Ir_Rfi_Irte_Mode_Invalid,
                                                         VTDIRFAULT_IRTE_PRESENT_RSVD, idDevice, idxIntr, &Irte);
                        }
                        else
                            dmarIrFaultRecordQualified(pDevIns, enmIrDiag, VTDIRFAULT_IRTE_PRESENT_RSVD, idDevice, idxIntr,
                                                         &Irte);
                    }
                    else
                        dmarIrFaultRecordQualified(pDevIns, kDmarDiag_Ir_Rfi_Irte_Rsvd, VTDIRFAULT_IRTE_PRESENT_RSVD,
                                                     idDevice, idxIntr, &Irte);
                }
                else
                    dmarIrFaultRecordQualified(pDevIns, kDmarDiag_Ir_Rfi_Irte_Not_Present, VTDIRFAULT_IRTE_NOT_PRESENT,
                                                 idDevice, idxIntr, &Irte);
            }
            else
                dmarIrFaultRecord(pDevIns, kDmarDiag_Ir_Rfi_Irte_Read_Failed, VTDIRFAULT_IRTE_READ_FAILED, idDevice, idxIntr);
        }
        else
            dmarIrFaultRecord(pDevIns, kDmarDiag_Ir_Rfi_Intr_Index_Invalid, VTDIRFAULT_INTR_INDEX_INVALID, idDevice, idxIntr);
    }
    else
        dmarIrFaultRecord(pDevIns, kDmarDiag_Ir_Rfi_Rsvd, VTDIRFAULT_REMAPPABLE_INTR_RSVD, idDevice, 0 /* idxIntr */);
    return VERR_IOMMU_INTR_REMAP_DENIED;
}


/**
 * Interrupt remap request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 */
static DECLCALLBACK(int) iommuIntelMsiRemap(PPDMDEVINS pDevIns, uint16_t idDevice, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    /* Validate. */
    Assert(pDevIns);
    Assert(pMsiIn);
    Assert(pMsiOut);
    RT_NOREF1(idDevice);

    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);

    /* Lock and read all registers required for interrupt remapping up-front. */
    DMAR_LOCK(pDevIns, pThisCC);
    uint32_t const uGstsReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
    uint64_t const uIrtaReg = pThis->uIrtaReg;
    DMAR_UNLOCK(pDevIns, pThisCC);

    /* Check if interrupt remapping is enabled. */
    if (uGstsReg & VTD_BF_GSTS_REG_IRES_MASK)
    {
        bool const fIsRemappable = RT_BF_GET(pMsiIn->Addr.au32[0], VTD_BF_REMAPPABLE_MSI_ADDR_INTR_FMT);
        if (!fIsRemappable)
        {
            /* Handle compatibility format interrupts. */
            STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMsiRemapCfi));

            /* If EIME is enabled or CFIs are  disabled, block the interrupt. */
            if (    (uIrtaReg & VTD_BF_IRTA_REG_EIME_MASK)
                || !(uGstsReg & VTD_BF_GSTS_REG_CFIS_MASK))
            {
                dmarIrFaultRecord(pDevIns, kDmarDiag_Ir_Cfi_Blocked, VTDIRFAULT_CFI_BLOCKED, idDevice, 0 /* idxIntr */);
                return VERR_IOMMU_INTR_REMAP_DENIED;
            }

            /* Interrupt isn't subject to remapping, pass-through the interrupt. */
            *pMsiOut = *pMsiIn;
            return VINF_SUCCESS;
        }

        /* Handle remappable format interrupts. */
        STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMsiRemapRfi));
        return dmarIrRemapIntr(pDevIns, uIrtaReg, idDevice, pMsiIn, pMsiOut);
    }

    /* Interrupt-remapping isn't enabled, all interrupts are pass-through.  */
    *pMsiOut = *pMsiIn;
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) dmarMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    RT_NOREF1(pvUser);
    DMAR_ASSERT_MMIO_ACCESS_RET(off, cb);

    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioWrite));

    uint16_t const offReg  = off;
    uint16_t const offLast = offReg + cb - 1;
    if (DMAR_IS_MMIO_OFF_VALID(offLast))
    {
        PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
        DMAR_LOCK_RET(pDevIns, pThisCC, VINF_IOM_R3_MMIO_WRITE);

        uint64_t uPrev = 0;
        uint64_t const uRegWritten = cb == 8 ? dmarRegWrite64(pThis, offReg, *(uint64_t *)pv, &uPrev)
                                             : dmarRegWrite32(pThis, offReg, *(uint32_t *)pv, (uint32_t *)&uPrev);
        VBOXSTRICTRC rcStrict = VINF_SUCCESS;
        switch (off)
        {
            case VTD_MMIO_OFF_GCMD_REG:             /* 32-bit */
            {
                rcStrict = dmarGcmdRegWrite(pDevIns, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_CCMD_REG:             /* 64-bit */
            case VTD_MMIO_OFF_CCMD_REG + 4:
            {
                rcStrict = dmarCcmdRegWrite(pDevIns, offReg, cb, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_FSTS_REG:             /* 32-bit */
            {
                rcStrict = dmarFstsRegWrite(pDevIns, uRegWritten, uPrev);
                break;
            }

            case VTD_MMIO_OFF_FECTL_REG:            /* 32-bit */
            {
                rcStrict = dmarFectlRegWrite(pDevIns, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_IQT_REG:              /* 64-bit */
            /*   VTD_MMIO_OFF_IQT_REG + 4: */       /* High 32-bits reserved. */
            {
                rcStrict = dmarIqtRegWrite(pDevIns, offReg, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_IQA_REG:              /* 64-bit */
            /*   VTD_MMIO_OFF_IQA_REG + 4: */       /* High 32-bits data. */
            {
                rcStrict = dmarIqaRegWrite(pDevIns, offReg, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_ICS_REG:              /* 32-bit */
            {
                rcStrict = dmarIcsRegWrite(pDevIns, uRegWritten);
                break;
            }

            case VTD_MMIO_OFF_IECTL_REG:            /* 32-bit */
            {
                rcStrict = dmarIectlRegWrite(pDevIns, uRegWritten);
                break;
            }

            case DMAR_MMIO_OFF_FRCD_HI_REG:         /* 64-bit */
            case DMAR_MMIO_OFF_FRCD_HI_REG + 4:
            {
                rcStrict = dmarFrcdHiRegWrite(pDevIns, offReg, cb, uRegWritten, uPrev);
                break;
            }
        }

        DMAR_UNLOCK(pDevIns, pThisCC);
        LogFlowFunc(("offReg=%#x uRegWritten=%#RX64 rc=%Rrc\n", offReg, uRegWritten, VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    return VINF_IOM_MMIO_UNUSED_FF;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) dmarMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    RT_NOREF1(pvUser);
    DMAR_ASSERT_MMIO_ACCESS_RET(off, cb);

    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMmioRead));

    uint16_t const offReg  = off;
    uint16_t const offLast = offReg + cb - 1;
    if (DMAR_IS_MMIO_OFF_VALID(offLast))
    {
        PCDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARCC);
        DMAR_LOCK_RET(pDevIns, pThisCC, VINF_IOM_R3_MMIO_READ);

        if (cb == 8)
        {
            *(uint64_t *)pv = dmarRegRead64(pThis, offReg);
            LogFlowFunc(("offReg=%#x pv=%#RX64\n", offReg, *(uint64_t *)pv));
        }
        else
        {
            *(uint32_t *)pv = dmarRegRead32(pThis, offReg);
            LogFlowFunc(("offReg=%#x pv=%#RX32\n", offReg, *(uint32_t *)pv));
        }

        DMAR_UNLOCK(pDevIns, pThisCC);
        return VINF_SUCCESS;
    }

    return VINF_IOM_MMIO_UNUSED_FF;
}


#ifdef IN_RING3
/**
 * Process requests in the invalidation queue.
 *
 * @param   pDevIns     The IOMMU device instance.
 * @param   pvRequests  The requests to process.
 * @param   cbRequests  The size of all requests (in bytes).
 * @param   fDw         The descriptor width (VTD_IQA_REG_DW_128_BIT or
 *                      VTD_IQA_REG_DW_256_BIT).
 * @param   fTtm        The table translation mode. Must not be VTD_TTM_RSVD.
 */
static void dmarR3InvQueueProcessRequests(PPDMDEVINS pDevIns, void const *pvRequests, uint32_t cbRequests, uint8_t fDw,
                                          uint8_t fTtm)
{
#define DMAR_IQE_FAULT_RECORD_RET(a_enmDiag, a_enmIqei) \
    do \
    { \
        dmarIqeFaultRecord(pDevIns, (a_enmDiag), (a_enmIqei)); \
        return; \
    } while (0)

    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARR3);

    DMAR_ASSERT_LOCK_IS_NOT_OWNER(pDevIns, pThisR3);
    Assert(fTtm != VTD_TTM_RSVD);       /* Should've beeen handled by caller. */

    /*
     * The below check is redundant since we check both TTM and DW for each
     * descriptor type we process. However, the error reported by hardware
     * may differ hence this is kept commented out but not removed from the code
     * if we need to change this in the future.
     *
     * In our implementation, we would report the descriptor type as invalid,
     * while on real hardware it may report descriptor width as invalid.
     * The Intel VT-d spec. is not clear which error takes preceedence.
     */
#if 0
    /*
     * Verify that 128-bit descriptors are not used when operating in scalable mode.
     * We don't check this while software writes IQA_REG but defer it until now because
     * RTADDR_REG can be updated lazily (via GCMD_REG.SRTP). The 256-bit descriptor check
     * -IS- performed when software writes IQA_REG since it only requires checking against
     * immutable hardware features.
     */
    if (   fTtm != VTD_TTM_SCALABLE_MODE
        || fDw != VTD_IQA_REG_DW_128_BIT)
    { /* likely */ }
    else
        DMAR_IQE_FAULT_RECORD_RET(kDmarDiag_IqaReg_Dw_128_Invalid, VTDIQEI_INVALID_DESCRIPTOR_WIDTH);
#endif

    /*
     * Process requests in FIFO order.
     */
    uint8_t const cbDsc = fDw == VTD_IQA_REG_DW_256_BIT ? 32 : 16;
    for (uint32_t offDsc = 0; offDsc < cbRequests; offDsc += cbDsc)
    {
        uint64_t const *puDscQwords = (uint64_t const *)((uintptr_t)pvRequests + offDsc);
        uint64_t const  uQword0     = puDscQwords[0];
        uint64_t const  uQword1     = puDscQwords[1];
        uint8_t const   fDscType    = VTD_GENERIC_INV_DSC_GET_TYPE(uQword0);
        switch (fDscType)
        {
            case VTD_INV_WAIT_DSC_TYPE:
            {
                /* Validate descriptor type. */
                if (   fTtm == VTD_TTM_LEGACY_MODE
                    || fDw == VTD_IQA_REG_DW_256_BIT)
                { /* likely */  }
                else
                    DMAR_IQE_FAULT_RECORD_RET(kDmarDiag_Iqei_Inv_Wait_Dsc_Invalid, VTDIQEI_INVALID_DESCRIPTOR_TYPE);

                /* Validate reserved bits. */
                uint64_t const fValidMask0 = !(pThis->fExtCapReg & VTD_BF_ECAP_REG_PDS_MASK)
                                           ? VTD_INV_WAIT_DSC_0_VALID_MASK & ~VTD_BF_0_INV_WAIT_DSC_PD_MASK
                                           : VTD_INV_WAIT_DSC_0_VALID_MASK;
                if (   !(uQword0 & ~fValidMask0)
                    && !(uQword1 & ~VTD_INV_WAIT_DSC_1_VALID_MASK))
                { /* likely */ }
                else
                    DMAR_IQE_FAULT_RECORD_RET(kDmarDiag_Iqei_Inv_Wait_Dsc_0_1_Rsvd, VTDIQEI_RSVD_FIELD_VIOLATION);

                if (fDw == VTD_IQA_REG_DW_256_BIT)
                {
                    if (   !puDscQwords[2]
                        && !puDscQwords[3])
                    { /* likely */ }
                    else
                        DMAR_IQE_FAULT_RECORD_RET(kDmarDiag_Iqei_Inv_Wait_Dsc_2_3_Rsvd, VTDIQEI_RSVD_FIELD_VIOLATION);
                }

                /* Perform status write (this must be done prior to generating the completion interrupt). */
                bool const fSw = RT_BF_GET(uQword0, VTD_BF_0_INV_WAIT_DSC_SW);
                if (fSw)
                {
                    uint32_t const uStatus      = RT_BF_GET(uQword0, VTD_BF_0_INV_WAIT_DSC_STDATA);
                    RTGCPHYS const GCPhysStatus = uQword1 & VTD_BF_1_INV_WAIT_DSC_STADDR_MASK;
                    int const rc = PDMDevHlpPhysWrite(pDevIns, GCPhysStatus, (void const*)&uStatus, sizeof(uStatus));
                    AssertRC(rc);
                }

                /* Generate invalidation event interrupt. */
                bool const fIf = RT_BF_GET(uQword0, VTD_BF_0_INV_WAIT_DSC_IF);
                if (fIf)
                {
                    DMAR_LOCK(pDevIns, pThisR3);
                    dmarR3InvEventRaiseInterrupt(pDevIns);
                    DMAR_UNLOCK(pDevIns, pThisR3);
                }

                STAM_COUNTER_INC(&pThis->StatInvWaitDsc);
                break;
            }

            case VTD_CC_INV_DSC_TYPE:           STAM_COUNTER_INC(&pThis->StatCcInvDsc);             break;
            case VTD_IOTLB_INV_DSC_TYPE:        STAM_COUNTER_INC(&pThis->StatIotlbInvDsc);          break;
            case VTD_DEV_TLB_INV_DSC_TYPE:      STAM_COUNTER_INC(&pThis->StatDevtlbInvDsc);         break;
            case VTD_IEC_INV_DSC_TYPE:          STAM_COUNTER_INC(&pThis->StatIecInvDsc);            break;
            case VTD_P_IOTLB_INV_DSC_TYPE:      STAM_COUNTER_INC(&pThis->StatPasidIotlbInvDsc);     break;
            case VTD_PC_INV_DSC_TYPE:           STAM_COUNTER_INC(&pThis->StatPasidCacheInvDsc);     break;
            case VTD_P_DEV_TLB_INV_DSC_TYPE:    STAM_COUNTER_INC(&pThis->StatPasidDevtlbInvDsc);    break;
            default:
            {
                /* Stop processing further requests. */
                LogFunc(("Invalid descriptor type: %#x\n", fDscType));
                DMAR_IQE_FAULT_RECORD_RET(kDmarDiag_Iqei_Dsc_Type_Invalid, VTDIQEI_INVALID_DESCRIPTOR_TYPE);
            }
        }
    }
#undef DMAR_IQE_FAULT_RECORD_RET
}


/**
 * The invalidation-queue thread.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pThread     The command thread.
 */
static DECLCALLBACK(int) dmarR3InvQueueThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    NOREF(pThread);
    LogFlowFunc(("\n"));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    /*
     * Pre-allocate the maximum size of the invalidation queue allowed by the spec.
     * This prevents trashing the heap as well as deal with out-of-memory situations
     * up-front while starting the VM. It also simplifies the code from having to
     * dynamically grow/shrink the allocation based on how software sizes the queue.
     * Guests normally don't alter the queue size all the time, but that's not an
     * assumption we can make.
     */
    uint8_t const cMaxPages = 1 << VTD_BF_IQA_REG_QS_MASK;
    size_t const  cbMaxQs   = cMaxPages << X86_PAGE_SHIFT;
    void *pvRequests = RTMemAllocZ(cbMaxQs);
    AssertPtrReturn(pvRequests, VERR_NO_MEMORY);

    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARR3);

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /*
         * Sleep until we are woken up.
         */
        {
            int const rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->hEvtInvQueue, RT_INDEFINITE_WAIT);
            AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
            if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                break;
        }

        DMAR_LOCK(pDevIns, pThisR3);
        if (dmarInvQueueCanProcessRequests(pThis))
        {
            uint32_t offQueueHead;
            uint32_t offQueueTail;
            bool const fIsEmpty = dmarInvQueueIsEmptyEx(pThis, &offQueueHead, &offQueueTail);
            if (!fIsEmpty)
            {
                /*
                 * Get the current queue size, descriptor width, queue base address and the
                 * table translation mode while the lock is still held.
                 */
                uint64_t const uIqaReg        = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQA_REG);
                uint8_t const  cQueuePages    = 1 << (uIqaReg & VTD_BF_IQA_REG_QS_MASK);
                uint32_t const cbQueue        = cQueuePages << X86_PAGE_SHIFT;
                uint8_t const  fDw            = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_DW);
                uint8_t const  fTtm           = RT_BF_GET(pThis->uRtaddrReg, VTD_BF_RTADDR_REG_TTM);
                RTGCPHYS const GCPhysRequests = (uIqaReg & VTD_BF_IQA_REG_IQA_MASK) + offQueueHead;

                /* Paranoia. */
                Assert(cbQueue <= cbMaxQs);
                Assert(!(offQueueTail & ~VTD_BF_IQT_REG_QT_MASK));
                Assert(!(offQueueHead & ~VTD_BF_IQH_REG_QH_MASK));
                Assert(fDw != VTD_IQA_REG_DW_256_BIT || !(offQueueTail & RT_BIT(4)));
                Assert(fDw != VTD_IQA_REG_DW_256_BIT || !(offQueueHead & RT_BIT(4)));
                Assert(offQueueHead < cbQueue);

                /*
                 * A table translation mode of "reserved" isn't valid for any descriptor type.
                 * However, RTADDR_REG can be modified in parallel to invalidation-queue processing,
                 * but if ESRTPS is support, we will perform a global invalidation when software
                 * changes RTADDR_REG, or it's the responsibility of software to do it explicitly.
                 * So caching TTM while reading all descriptors should not be a problem.
                 *
                 * Also, validate the queue tail offset as it's mutable by software.
                 */
                if (   fTtm != VTD_TTM_RSVD
                    && offQueueTail < cbQueue)
                {
                    /* Don't hold the lock while reading (a potentially large amount of) requests */
                    DMAR_UNLOCK(pDevIns, pThisR3);

                    int      rc;
                    uint32_t cbRequests;
                    if (offQueueTail > offQueueHead)
                    {
                        /* The requests have not wrapped around, read them in one go. */
                        cbRequests = offQueueTail - offQueueHead;
                        rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysRequests, pvRequests, cbRequests);
                    }
                    else
                    {
                        /* The requests have wrapped around, read forward and wrapped-around. */
                        uint32_t const cbForward = cbQueue - offQueueHead;
                        rc  = PDMDevHlpPhysReadMeta(pDevIns, GCPhysRequests, pvRequests, cbForward);

                        uint32_t const cbWrapped = offQueueTail;
                        if (   RT_SUCCESS(rc)
                            && cbWrapped > 0)
                        {
                            rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysRequests + cbForward,
                                                       (void *)((uintptr_t)pvRequests + cbForward), cbWrapped);
                        }
                        cbRequests = cbForward + cbWrapped;
                    }

                    /* Re-acquire the lock since we need to update device state. */
                    DMAR_LOCK(pDevIns, pThisR3);

                    if (RT_SUCCESS(rc))
                    {
                        /* Indicate to software we've fetched all requests. */
                        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_IQH_REG, offQueueTail);

                        /* Don't hold the lock while processing requests. */
                        DMAR_UNLOCK(pDevIns, pThisR3);

                        /* Process all requests. */
                        Assert(cbRequests <= cbQueue);
                        dmarR3InvQueueProcessRequests(pDevIns, pvRequests, cbRequests, fDw, fTtm);

                        /*
                         * We've processed all requests and the lock shouldn't be held at this point.
                         * Using 'continue' here allows us to skip re-acquiring the lock just to release
                         * it again before going back to the thread loop. It's a bit ugly but it certainly
                         * helps with performance.
                         */
                        DMAR_ASSERT_LOCK_IS_NOT_OWNER(pDevIns, pThisR3);
                        continue;
                    }
                    else
                        dmarIqeFaultRecord(pDevIns, kDmarDiag_IqaReg_Dsc_Fetch_Error, VTDIQEI_FETCH_DESCRIPTOR_ERR);
                }
                else
                {
                    if (fTtm == VTD_TTM_RSVD)
                        dmarIqeFaultRecord(pDevIns, kDmarDiag_Iqei_Ttm_Rsvd, VTDIQEI_INVALID_TTM);
                    else
                    {
                        Assert(offQueueTail >= cbQueue);
                        dmarIqeFaultRecord(pDevIns, kDmarDiag_IqtReg_Qt_Invalid, VTDIQEI_INVALID_TAIL_PTR);
                    }
                }
            }
        }
        DMAR_UNLOCK(pDevIns, pThisR3);
    }

    RTMemFree(pvRequests);
    pvRequests = NULL;

    LogFlowFunc(("Invalidation-queue thread terminating\n"));
    return VINF_SUCCESS;
}


/**
 * Wakes up the invalidation-queue thread so it can respond to a state
 * change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   pThread     The invalidation-queue thread.
 *
 * @thread EMT.
 */
static DECLCALLBACK(int) dmarR3InvQueueThreadWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    LogFlowFunc(("\n"));
    PCDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    return PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEvtInvQueue);
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) dmarR3DbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCDMAR      pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARR3    pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARR3);
    bool const fVerbose = RTStrCmp(pszArgs, "verbose") == 0;

    /*
     * We lock the device to get a consistent register state as it is
     * ASSUMED pHlp->pfnPrintf is expensive, so we copy the registers (the
     * ones we care about here) into temporaries and release the lock ASAP.
     *
     * Order of register being read and outputted is in accordance with the
     * spec. for no particular reason.
     * See Intel VT-d spec. 10.4 "Register Descriptions".
     */
    DMAR_LOCK(pDevIns, pThisR3);

    DMARDIAG const enmDiag      = pThis->enmDiag;
    uint32_t const uVerReg      = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_VER_REG);
    uint64_t const uCapReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_CAP_REG);
    uint64_t const uEcapReg     = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_ECAP_REG);
    uint32_t const uGcmdReg     = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GCMD_REG);
    uint32_t const uGstsReg     = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_GSTS_REG);
    uint64_t const uRtaddrReg   = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_RTADDR_REG);
    uint64_t const uCcmdReg     = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_CCMD_REG);
    uint32_t const uFstsReg     = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FSTS_REG);
    uint32_t const uFectlReg    = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FECTL_REG);
    uint32_t const uFedataReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FEDATA_REG);
    uint32_t const uFeaddrReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FEADDR_REG);
    uint32_t const uFeuaddrReg  = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_FEUADDR_REG);
    uint64_t const uAflogReg    = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_AFLOG_REG);
    uint32_t const uPmenReg     = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PMEN_REG);
    uint32_t const uPlmbaseReg  = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PLMBASE_REG);
    uint32_t const uPlmlimitReg = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PLMLIMIT_REG);
    uint64_t const uPhmbaseReg  = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_PHMBASE_REG);
    uint64_t const uPhmlimitReg = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_PHMLIMIT_REG);
    uint64_t const uIqhReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQH_REG);
    uint64_t const uIqtReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQT_REG);
    uint64_t const uIqaReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQA_REG);
    uint32_t const uIcsReg      = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_ICS_REG);
    uint32_t const uIectlReg    = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IECTL_REG);
    uint32_t const uIedataReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IEDATA_REG);
    uint32_t const uIeaddrReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IEADDR_REG);
    uint32_t const uIeuaddrReg  = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_IEUADDR_REG);
    uint64_t const uIqercdReg   = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IQERCD_REG);
    uint64_t const uIrtaReg     = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_IRTA_REG);
    uint64_t const uPqhReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_PQH_REG);
    uint64_t const uPqtReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_PQT_REG);
    uint64_t const uPqaReg      = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_PQA_REG);
    uint32_t const uPrsReg      = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PRS_REG);
    uint32_t const uPectlReg    = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PECTL_REG);
    uint32_t const uPedataReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PEDATA_REG);
    uint32_t const uPeaddrReg   = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PEADDR_REG);
    uint32_t const uPeuaddrReg  = dmarRegReadRaw32(pThis, VTD_MMIO_OFF_PEUADDR_REG);
    uint64_t const uMtrrcapReg  = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_MTRRCAP_REG);
    uint64_t const uMtrrdefReg  = dmarRegReadRaw64(pThis, VTD_MMIO_OFF_MTRRDEF_REG);

    DMAR_UNLOCK(pDevIns, pThisR3);

    const char *const pszDiag = enmDiag < RT_ELEMENTS(g_apszDmarDiagDesc) ? g_apszDmarDiagDesc[enmDiag] : "(Unknown)";
    pHlp->pfnPrintf(pHlp, "Intel-IOMMU:\n");
    pHlp->pfnPrintf(pHlp, " Diag         = %s\n", pszDiag);

    /*
     * Non-verbose output.
     */
    if (!fVerbose)
    {
        pHlp->pfnPrintf(pHlp, " VER_REG      = %#RX32\n", uVerReg);
        pHlp->pfnPrintf(pHlp, " CAP_REG      = %#RX64\n", uCapReg);
        pHlp->pfnPrintf(pHlp, " ECAP_REG     = %#RX64\n", uEcapReg);
        pHlp->pfnPrintf(pHlp, " GCMD_REG     = %#RX32\n", uGcmdReg);
        pHlp->pfnPrintf(pHlp, " GSTS_REG     = %#RX32\n", uGstsReg);
        pHlp->pfnPrintf(pHlp, " RTADDR_REG   = %#RX64\n", uRtaddrReg);
        pHlp->pfnPrintf(pHlp, " CCMD_REG     = %#RX64\n", uCcmdReg);
        pHlp->pfnPrintf(pHlp, " FSTS_REG     = %#RX32\n", uFstsReg);
        pHlp->pfnPrintf(pHlp, " FECTL_REG    = %#RX32\n", uFectlReg);
        pHlp->pfnPrintf(pHlp, " FEDATA_REG   = %#RX32\n", uFedataReg);
        pHlp->pfnPrintf(pHlp, " FEADDR_REG   = %#RX32\n", uFeaddrReg);
        pHlp->pfnPrintf(pHlp, " FEUADDR_REG  = %#RX32\n", uFeuaddrReg);
        pHlp->pfnPrintf(pHlp, " AFLOG_REG    = %#RX64\n", uAflogReg);
        pHlp->pfnPrintf(pHlp, " PMEN_REG     = %#RX32\n", uPmenReg);
        pHlp->pfnPrintf(pHlp, " PLMBASE_REG  = %#RX32\n", uPlmbaseReg);
        pHlp->pfnPrintf(pHlp, " PLMLIMIT_REG = %#RX32\n", uPlmlimitReg);
        pHlp->pfnPrintf(pHlp, " PHMBASE_REG  = %#RX64\n", uPhmbaseReg);
        pHlp->pfnPrintf(pHlp, " PHMLIMIT_REG = %#RX64\n", uPhmlimitReg);
        pHlp->pfnPrintf(pHlp, " IQH_REG      = %#RX64\n", uIqhReg);
        pHlp->pfnPrintf(pHlp, " IQT_REG      = %#RX64\n", uIqtReg);
        pHlp->pfnPrintf(pHlp, " IQA_REG      = %#RX64\n", uIqaReg);
        pHlp->pfnPrintf(pHlp, " ICS_REG      = %#RX32\n", uIcsReg);
        pHlp->pfnPrintf(pHlp, " IECTL_REG    = %#RX32\n", uIectlReg);
        pHlp->pfnPrintf(pHlp, " IEDATA_REG   = %#RX32\n", uIedataReg);
        pHlp->pfnPrintf(pHlp, " IEADDR_REG   = %#RX32\n", uIeaddrReg);
        pHlp->pfnPrintf(pHlp, " IEUADDR_REG  = %#RX32\n", uIeuaddrReg);
        pHlp->pfnPrintf(pHlp, " IQERCD_REG   = %#RX64\n", uIqercdReg);
        pHlp->pfnPrintf(pHlp, " IRTA_REG     = %#RX64\n", uIrtaReg);
        pHlp->pfnPrintf(pHlp, " PQH_REG      = %#RX64\n", uPqhReg);
        pHlp->pfnPrintf(pHlp, " PQT_REG      = %#RX64\n", uPqtReg);
        pHlp->pfnPrintf(pHlp, " PQA_REG      = %#RX64\n", uPqaReg);
        pHlp->pfnPrintf(pHlp, " PRS_REG      = %#RX32\n", uPrsReg);
        pHlp->pfnPrintf(pHlp, " PECTL_REG    = %#RX32\n", uPectlReg);
        pHlp->pfnPrintf(pHlp, " PEDATA_REG   = %#RX32\n", uPedataReg);
        pHlp->pfnPrintf(pHlp, " PEADDR_REG   = %#RX32\n", uPeaddrReg);
        pHlp->pfnPrintf(pHlp, " PEUADDR_REG  = %#RX32\n", uPeuaddrReg);
        pHlp->pfnPrintf(pHlp, " MTRRCAP_REG  = %#RX64\n", uMtrrcapReg);
        pHlp->pfnPrintf(pHlp, " MTRRDEF_REG  = %#RX64\n", uMtrrdefReg);
        pHlp->pfnPrintf(pHlp, "\n");
        return;
    }

    /*
     * Verbose output.
     */
    pHlp->pfnPrintf(pHlp, " VER_REG      = %#RX32\n", uVerReg);
    {
        pHlp->pfnPrintf(pHlp, "   MAJ          = %#x\n", RT_BF_GET(uVerReg, VTD_BF_VER_REG_MAX));
        pHlp->pfnPrintf(pHlp, "   MIN          = %#x\n", RT_BF_GET(uVerReg, VTD_BF_VER_REG_MIN));
    }
    pHlp->pfnPrintf(pHlp, " CAP_REG      = %#RX64\n", uCapReg);
    {
        uint8_t const uSagaw = RT_BF_GET(uCapReg, VTD_BF_CAP_REG_SAGAW);
        uint8_t const uMgaw  = RT_BF_GET(uCapReg, VTD_BF_CAP_REG_MGAW);
        uint8_t const uNfr   = RT_BF_GET(uCapReg, VTD_BF_CAP_REG_NFR);
        pHlp->pfnPrintf(pHlp, "   ND           = %u\n",         RT_BF_GET(uCapReg, VTD_BF_CAP_REG_ND));
        pHlp->pfnPrintf(pHlp, "   AFL          = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_AFL));
        pHlp->pfnPrintf(pHlp, "   RWBF         = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_RWBF));
        pHlp->pfnPrintf(pHlp, "   PLMR         = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_PLMR));
        pHlp->pfnPrintf(pHlp, "   PHMR         = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_PHMR));
        pHlp->pfnPrintf(pHlp, "   CM           = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_CM));
        pHlp->pfnPrintf(pHlp, "   SAGAW        = %#x (%u bits)\n", uSagaw, vtdCapRegGetSagawBits(uSagaw));
        pHlp->pfnPrintf(pHlp, "   MGAW         = %#x (%u bits)\n", uMgaw, uMgaw + 1);
        pHlp->pfnPrintf(pHlp, "   ZLR          = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_ZLR));
        pHlp->pfnPrintf(pHlp, "   FRO          = %#x bytes\n",  RT_BF_GET(uCapReg, VTD_BF_CAP_REG_FRO));
        pHlp->pfnPrintf(pHlp, "   SLLPS        = %#x\n",        RT_BF_GET(uCapReg, VTD_BF_CAP_REG_SLLPS));
        pHlp->pfnPrintf(pHlp, "   PSI          = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_PSI));
        pHlp->pfnPrintf(pHlp, "   NFR          = %u (%u FRCD register%s)\n", uNfr, uNfr + 1, uNfr > 0 ? "s" : "");
        pHlp->pfnPrintf(pHlp, "   MAMV         = %#x\n",        RT_BF_GET(uCapReg, VTD_BF_CAP_REG_MAMV));
        pHlp->pfnPrintf(pHlp, "   DWD          = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_DWD));
        pHlp->pfnPrintf(pHlp, "   DRD          = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_DRD));
        pHlp->pfnPrintf(pHlp, "   FL1GP        = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_FL1GP));
        pHlp->pfnPrintf(pHlp, "   PI           = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_PI));
        pHlp->pfnPrintf(pHlp, "   FL5LP        = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_FL5LP));
        pHlp->pfnPrintf(pHlp, "   ESIRTPS      = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_ESIRTPS));
        pHlp->pfnPrintf(pHlp, "   ESRTPS       = %RTbool\n",    RT_BF_GET(uCapReg, VTD_BF_CAP_REG_ESRTPS));
    }
    pHlp->pfnPrintf(pHlp, " ECAP_REG     = %#RX64\n", uEcapReg);
    {
        uint8_t const uPss = RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_PSS);
        pHlp->pfnPrintf(pHlp, "   C            = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_C));
        pHlp->pfnPrintf(pHlp, "   QI           = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_QI));
        pHlp->pfnPrintf(pHlp, "   DT           = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_DT));
        pHlp->pfnPrintf(pHlp, "   IR           = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_IR));
        pHlp->pfnPrintf(pHlp, "   EIM          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_EIM));
        pHlp->pfnPrintf(pHlp, "   PT           = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_PT));
        pHlp->pfnPrintf(pHlp, "   SC           = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SC));
        pHlp->pfnPrintf(pHlp, "   IRO          = %#x bytes\n",  RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_IRO));
        pHlp->pfnPrintf(pHlp, "   MHMV         = %#x\n",        RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_MHMV));
        pHlp->pfnPrintf(pHlp, "   MTS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_MTS));
        pHlp->pfnPrintf(pHlp, "   NEST         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_NEST));
        pHlp->pfnPrintf(pHlp, "   PRS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_PRS));
        pHlp->pfnPrintf(pHlp, "   ERS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_ERS));
        pHlp->pfnPrintf(pHlp, "   SRS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SRS));
        pHlp->pfnPrintf(pHlp, "   NWFS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_NWFS));
        pHlp->pfnPrintf(pHlp, "   EAFS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_EAFS));
        pHlp->pfnPrintf(pHlp, "   PSS          = %u (%u bits)\n", uPss, uPss > 0 ? uPss + 1 : 0);
        pHlp->pfnPrintf(pHlp, "   PASID        = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_PASID));
        pHlp->pfnPrintf(pHlp, "   DIT          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_DIT));
        pHlp->pfnPrintf(pHlp, "   PDS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_PDS));
        pHlp->pfnPrintf(pHlp, "   SMTS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SMTS));
        pHlp->pfnPrintf(pHlp, "   VCS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_VCS));
        pHlp->pfnPrintf(pHlp, "   SLADS        = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SLADS));
        pHlp->pfnPrintf(pHlp, "   SLTS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SLTS));
        pHlp->pfnPrintf(pHlp, "   FLTS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_FLTS));
        pHlp->pfnPrintf(pHlp, "   SMPWCS       = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_SMPWCS));
        pHlp->pfnPrintf(pHlp, "   RPS          = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_RPS));
        pHlp->pfnPrintf(pHlp, "   ADMS         = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_ADMS));
        pHlp->pfnPrintf(pHlp, "   RPRIVS       = %RTbool\n",    RT_BF_GET(uEcapReg, VTD_BF_ECAP_REG_RPRIVS));
    }
    pHlp->pfnPrintf(pHlp, " GCMD_REG     = %#RX32\n", uGcmdReg);
    {
        uint8_t const fCfi = RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_CFI);
        pHlp->pfnPrintf(pHlp, "   CFI          = %u (%s)\n", fCfi, fCfi ? "Passthrough" : "Blocked");
        pHlp->pfnPrintf(pHlp, "   SIRTP        = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_SIRTP));
        pHlp->pfnPrintf(pHlp, "   IRE          = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_IRE));
        pHlp->pfnPrintf(pHlp, "   QIE          = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_QIE));
        pHlp->pfnPrintf(pHlp, "   WBF          = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_WBF));
        pHlp->pfnPrintf(pHlp, "   EAFL         = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_SFL));
        pHlp->pfnPrintf(pHlp, "   SFL          = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_SFL));
        pHlp->pfnPrintf(pHlp, "   SRTP         = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_SRTP));
        pHlp->pfnPrintf(pHlp, "   TE           = %u\n", RT_BF_GET(uGcmdReg, VTD_BF_GCMD_REG_TE));
    }
    pHlp->pfnPrintf(pHlp, " GSTS_REG     = %#RX32\n", uGstsReg);
    {
        uint8_t const fCfis = RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_CFIS);
        pHlp->pfnPrintf(pHlp, "   CFIS         = %u (%s)\n", fCfis, fCfis ? "Passthrough" : "Blocked");
        pHlp->pfnPrintf(pHlp, "   IRTPS        = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_IRTPS));
        pHlp->pfnPrintf(pHlp, "   IRES         = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_IRES));
        pHlp->pfnPrintf(pHlp, "   QIES         = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_QIES));
        pHlp->pfnPrintf(pHlp, "   WBFS         = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_WBFS));
        pHlp->pfnPrintf(pHlp, "   AFLS         = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_AFLS));
        pHlp->pfnPrintf(pHlp, "   FLS          = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_FLS));
        pHlp->pfnPrintf(pHlp, "   RTPS         = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_RTPS));
        pHlp->pfnPrintf(pHlp, "   TES          = %u\n", RT_BF_GET(uGstsReg, VTD_BF_GSTS_REG_TES));
    }
    pHlp->pfnPrintf(pHlp, " RTADDR_REG   = %#RX64\n", uRtaddrReg);
    {
        uint8_t const uTtm = RT_BF_GET(uRtaddrReg, VTD_BF_RTADDR_REG_TTM);
        pHlp->pfnPrintf(pHlp, "   RTA          = %#RX64\n",   uRtaddrReg & VTD_BF_RTADDR_REG_RTA_MASK);
        pHlp->pfnPrintf(pHlp, "   TTM          = %u (%s)\n",  uTtm, vtdRtaddrRegGetTtmDesc(uTtm));
    }
    pHlp->pfnPrintf(pHlp, " CCMD_REG     = %#RX64\n", uCcmdReg);
    pHlp->pfnPrintf(pHlp, " FSTS_REG     = %#RX32\n", uFstsReg);
    {
        pHlp->pfnPrintf(pHlp, "   PFO          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_PFO));
        pHlp->pfnPrintf(pHlp, "   PPF          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_PPF));
        pHlp->pfnPrintf(pHlp, "   AFO          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_AFO));
        pHlp->pfnPrintf(pHlp, "   APF          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_APF));
        pHlp->pfnPrintf(pHlp, "   IQE          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_IQE));
        pHlp->pfnPrintf(pHlp, "   ICS          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_ICE));
        pHlp->pfnPrintf(pHlp, "   ITE          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_ITE));
        pHlp->pfnPrintf(pHlp, "   FRI          = %u\n",  RT_BF_GET(uFstsReg, VTD_BF_FSTS_REG_FRI));
    }
    pHlp->pfnPrintf(pHlp, " FECTL_REG    = %#RX32\n", uFectlReg);
    {
        pHlp->pfnPrintf(pHlp, "   IM           = %RTbool\n",  RT_BF_GET(uFectlReg, VTD_BF_FECTL_REG_IM));
        pHlp->pfnPrintf(pHlp, "   IP           = %RTbool\n",  RT_BF_GET(uFectlReg, VTD_BF_FECTL_REG_IP));
    }
    pHlp->pfnPrintf(pHlp, " FEDATA_REG   = %#RX32\n", uFedataReg);
    pHlp->pfnPrintf(pHlp, " FEADDR_REG   = %#RX32\n", uFeaddrReg);
    pHlp->pfnPrintf(pHlp, " FEUADDR_REG  = %#RX32\n", uFeuaddrReg);
    pHlp->pfnPrintf(pHlp, " AFLOG_REG    = %#RX64\n", uAflogReg);
    pHlp->pfnPrintf(pHlp, " PMEN_REG     = %#RX32\n", uPmenReg);
    pHlp->pfnPrintf(pHlp, " PLMBASE_REG  = %#RX32\n", uPlmbaseReg);
    pHlp->pfnPrintf(pHlp, " PLMLIMIT_REG = %#RX32\n", uPlmlimitReg);
    pHlp->pfnPrintf(pHlp, " PHMBASE_REG  = %#RX64\n", uPhmbaseReg);
    pHlp->pfnPrintf(pHlp, " PHMLIMIT_REG = %#RX64\n", uPhmlimitReg);
    pHlp->pfnPrintf(pHlp, " IQH_REG      = %#RX64\n", uIqhReg);
    pHlp->pfnPrintf(pHlp, " IQT_REG      = %#RX64\n", uIqtReg);
    pHlp->pfnPrintf(pHlp, " IQA_REG      = %#RX64\n", uIqaReg);
    {
        uint8_t const  fDw = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_DW);
        uint8_t const  fQs = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_QS);
        uint8_t const  cQueuePages = 1 << fQs;
        pHlp->pfnPrintf(pHlp, "   DW           = %u (%s)\n",  fDw, fDw == VTD_IQA_REG_DW_128_BIT ? "128-bit" : "256-bit");
        pHlp->pfnPrintf(pHlp, "   QS           = %u (%u page%s)\n", fQs, cQueuePages, cQueuePages > 1 ? "s" : "");
    }
    pHlp->pfnPrintf(pHlp, " ICS_REG      = %#RX32\n", uIcsReg);
    {
        pHlp->pfnPrintf(pHlp, "   IWC          = %u\n",       RT_BF_GET(uIcsReg, VTD_BF_ICS_REG_IWC));
    }
    pHlp->pfnPrintf(pHlp, " IECTL_REG    = %#RX32\n", uIectlReg);
    {
        pHlp->pfnPrintf(pHlp, "   IM           = %RTbool\n",  RT_BF_GET(uIectlReg, VTD_BF_IECTL_REG_IM));
        pHlp->pfnPrintf(pHlp, "   IP           = %RTbool\n",  RT_BF_GET(uIectlReg, VTD_BF_IECTL_REG_IP));
    }
    pHlp->pfnPrintf(pHlp, " IEDATA_REG   = %#RX32\n", uIedataReg);
    pHlp->pfnPrintf(pHlp, " IEADDR_REG   = %#RX32\n", uIeaddrReg);
    pHlp->pfnPrintf(pHlp, " IEUADDR_REG  = %#RX32\n", uIeuaddrReg);
    pHlp->pfnPrintf(pHlp, " IQERCD_REG   = %#RX64\n", uIqercdReg);
    {
        pHlp->pfnPrintf(pHlp, "   ICESID       = %#RX32\n",   RT_BF_GET(uIqercdReg, VTD_BF_IQERCD_REG_ICESID));
        pHlp->pfnPrintf(pHlp, "   ITESID       = %#RX32\n",   RT_BF_GET(uIqercdReg, VTD_BF_IQERCD_REG_ITESID));
        pHlp->pfnPrintf(pHlp, "   IQEI         = %#RX32\n",   RT_BF_GET(uIqercdReg, VTD_BF_IQERCD_REG_IQEI));
    }
    pHlp->pfnPrintf(pHlp, " IRTA_REG     = %#RX64\n", uIrtaReg);
    {
        uint32_t const cIrtEntries = VTD_IRTA_REG_GET_ENTRY_COUNT(uIrtaReg);
        uint32_t const cbIrt       = sizeof(VTD_IRTE_T) * cIrtEntries;
        pHlp->pfnPrintf(pHlp, "   IRTA         = %#RX64\n",   uIrtaReg & VTD_BF_IRTA_REG_IRTA_MASK);
        pHlp->pfnPrintf(pHlp, "   EIME         = %RTbool\n",  RT_BF_GET(uIrtaReg, VTD_BF_IRTA_REG_EIME));
        pHlp->pfnPrintf(pHlp, "   S            = %u entries (%u bytes)\n", cIrtEntries, cbIrt);
    }
    pHlp->pfnPrintf(pHlp, " PQH_REG      = %#RX64\n", uPqhReg);
    pHlp->pfnPrintf(pHlp, " PQT_REG      = %#RX64\n", uPqtReg);
    pHlp->pfnPrintf(pHlp, " PQA_REG      = %#RX64\n", uPqaReg);
    pHlp->pfnPrintf(pHlp, " PRS_REG      = %#RX32\n", uPrsReg);
    pHlp->pfnPrintf(pHlp, " PECTL_REG    = %#RX32\n", uPectlReg);
    pHlp->pfnPrintf(pHlp, " PEDATA_REG   = %#RX32\n", uPedataReg);
    pHlp->pfnPrintf(pHlp, " PEADDR_REG   = %#RX32\n", uPeaddrReg);
    pHlp->pfnPrintf(pHlp, " PEUADDR_REG  = %#RX32\n", uPeuaddrReg);
    pHlp->pfnPrintf(pHlp, " MTRRCAP_REG  = %#RX64\n", uMtrrcapReg);
    pHlp->pfnPrintf(pHlp, " MTRRDEF_REG  = %#RX64\n", uMtrrdefReg);
    pHlp->pfnPrintf(pHlp, "\n");
}


/**
 * Initializes all registers in the DMAR unit.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void dmarR3RegsInit(PPDMDEVINS pDevIns)
{
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);

    /*
     * Wipe all registers (required on reset).
     */
    RT_ZERO(pThis->abRegs0);
    RT_ZERO(pThis->abRegs1);

    /*
     * Initialize registers not mutable by software prior to initializing other registers.
     */
    /* VER_REG */
    {
        pThis->uVerReg = RT_BF_MAKE(VTD_BF_VER_REG_MIN, DMAR_VER_MINOR)
                       | RT_BF_MAKE(VTD_BF_VER_REG_MAX, DMAR_VER_MAJOR);
        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_VER_REG, pThis->uVerReg);
    }

    uint8_t const fFlts  = 1;                    /* First-Level translation support. */
    uint8_t const fSlts  = 1;                    /* Second-Level translation support. */
    uint8_t const fPt    = 1;                    /* Pass-Through support. */
    uint8_t const fSmts  = fFlts & fSlts & fPt;  /* Scalable mode translation support.*/
    uint8_t const fNest  = 0;                    /* Nested translation support. */

    /* CAP_REG */
    {
        uint8_t cGstPhysAddrBits;
        uint8_t cGstLinearAddrBits;
        PDMDevHlpCpuGetGuestAddrWidths(pDevIns, &cGstPhysAddrBits, &cGstLinearAddrBits);

        uint8_t const fFl1gp   = 1;                                /* First-Level 1GB pages support. */
        uint8_t const fFl5lp   = 1;                                /* First-level 5-level paging support (PML5E). */
        uint8_t const fSl2mp   = fSlts & 1;                        /* Second-Level 2MB pages support. */
        uint8_t const fSl2gp   = fSlts & 1;                        /* Second-Level 1GB pages support. */
        uint8_t const fSllps   = fSl2mp                            /* Second-Level large page Support. */
                               | ((fSl2mp & fFl1gp) & RT_BIT(1));
        uint8_t const fMamv    = (fSl2gp ? X86_PAGE_1G_SHIFT       /* Maximum address mask value (for 2nd-level invalidations). */
                                         : X86_PAGE_2M_SHIFT)
                               - X86_PAGE_4K_SHIFT;
        uint8_t const fNd      = DMAR_ND;                          /* Number of domains supported. */
        uint8_t const fPsi     = 1;                                /* Page selective invalidation. */
        uint8_t const uMgaw    = cGstPhysAddrBits - 1;             /* Maximum guest address width. */
        uint8_t const uSagaw   = vtdCapRegGetSagaw(uMgaw);         /* Supported adjust guest address width. */
        uint16_t const offFro  = DMAR_MMIO_OFF_FRCD_LO_REG >> 4;   /* MMIO offset of FRCD registers. */
        uint8_t const fEsrtps  = 1;                                /* Enhanced SRTPS (auto invalidate cache on SRTP). */
        uint8_t const fEsirtps = 1;                                /* Enhanced SIRTPS (auto invalidate cache on SIRTP). */
        AssertCompile(DMAR_ND <= 6);

        pThis->fCapReg = RT_BF_MAKE(VTD_BF_CAP_REG_ND,      fNd)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_AFL,     0)     /* Advanced fault logging not supported. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_RWBF,    0)     /* Software need not flush write-buffers. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_PLMR,    0)     /* Protected Low-Memory Region not supported. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_PHMR,    0)     /* Protected High-Memory Region not supported. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_CM,      1)     /* Software should invalidate on mapping structure changes. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_SAGAW,   fSlts & uSagaw)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_MGAW,    uMgaw)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_ZLR,     1)     /** @todo Figure out if/how to support zero-length reads. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_FRO,     offFro)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_SLLPS,   fSlts & fSllps)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_PSI,     fPsi)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_NFR,     DMAR_FRCD_REG_COUNT - 1)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_MAMV,    fPsi & fMamv)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_DWD,     1)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_DRD,     1)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_FL1GP,   fFlts & fFl1gp)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_PI,      0)     /* Posted Interrupts not supported. */
                       | RT_BF_MAKE(VTD_BF_CAP_REG_FL5LP,   fFlts & fFl5lp)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_ESIRTPS, fEsirtps)
                       | RT_BF_MAKE(VTD_BF_CAP_REG_ESRTPS,  fEsrtps);
        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_CAP_REG, pThis->fCapReg);
    }

    /* ECAP_REG */
    {
        uint8_t const  fQi     = 1;                                /* Queued-invalidations. */
        uint8_t const  fIr     = !!(DMAR_ACPI_DMAR_FLAGS & ACPI_DMAR_F_INTR_REMAP);      /* Interrupt remapping support. */
        uint8_t const  fMhmv   = 0xf;                              /* Maximum handle mask value. */
        uint16_t const offIro  = DMAR_MMIO_OFF_IVA_REG >> 4;       /* MMIO offset of IOTLB registers. */
        uint8_t const  fEim    = 1;                                /* Extended interrupt mode.*/
        uint8_t const  fAdms   = 1;                                /* Abort DMA mode support. */

        pThis->fExtCapReg = RT_BF_MAKE(VTD_BF_ECAP_REG_C,      0)  /* Accesses don't snoop CPU cache. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_QI,     fQi)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_DT,     0)  /* Device-TLBs not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_IR,     fQi & fIr)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_EIM,    fIr & fEim)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_PT,     fPt)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SC,     0)  /* Snoop control not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_IRO,    offIro)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_MHMV,   fIr & fMhmv)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_MTS,    0)  /* Memory type not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_NEST,   fNest)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_PRS,    0)  /* 0 as DT not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_ERS,    0)  /* Execute request not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SRS,    0)  /* Supervisor request not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_NWFS,   0)  /* 0 as DT not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_EAFS,   0)  /** @todo figure out if EAFS is required? */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_PSS,    0)  /* 0 as PASID not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_PASID,  0)  /* PASID support. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_DIT,    0)  /* 0 as DT not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_PDS,    0)  /* 0 as DT not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SMTS,   fSmts)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_VCS,    0)  /* 0 as PASID not supported (commands seem PASID specific). */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SLADS,  0)  /* Second-level accessed/dirty not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SLTS,   fSlts)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_FLTS,   fFlts)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_SMPWCS, 0)  /* 0 as PASID not supported. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_RPS,    0)  /* We don't support RID_PASID field in SM context entry. */
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_ADMS,   fAdms)
                          | RT_BF_MAKE(VTD_BF_ECAP_REG_RPRIVS, 0); /* 0 as SRS not supported. */
        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_ECAP_REG, pThis->fExtCapReg);
    }

    /*
     * Initialize registers mutable by software.
     */
    /* FECTL_REG */
    {
        uint32_t const uCtl = RT_BF_MAKE(VTD_BF_FECTL_REG_IM, 1);
        dmarRegWriteRaw32(pThis, VTD_MMIO_OFF_FECTL_REG, uCtl);
    }

    /* ICETL_REG */
    {
        uint32_t const uCtl = RT_BF_MAKE(VTD_BF_IECTL_REG_IM, 1);
        dmarRegWriteRaw32(pThis, VTD_MMIO_OFF_IECTL_REG, uCtl);
    }

#ifdef VBOX_STRICT
    Assert(!RT_BF_GET(pThis->fExtCapReg, VTD_BF_ECAP_REG_PRS));    /* PECTL_REG - Reserved if don't support PRS. */
    Assert(!RT_BF_GET(pThis->fExtCapReg, VTD_BF_ECAP_REG_MTS));    /* MTRRCAP_REG - Reserved if we don't support MTS. */
#endif
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuIntelR3Reset(PPDMDEVINS pDevIns)
{
    PCDMARR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARR3);
    LogFlowFunc(("\n"));

    DMAR_LOCK(pDevIns, pThisR3);
    dmarR3RegsInit(pDevIns);
    DMAR_UNLOCK(pDevIns, pThisR3);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuIntelR3Destruct(PPDMDEVINS pDevIns)
{
    PDMAR    pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PCDMARR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PCDMARR3);
    LogFlowFunc(("\n"));

    DMAR_LOCK(pDevIns, pThisR3);

    if (pThis->hEvtInvQueue != NIL_SUPSEMEVENT)
    {
        PDMDevHlpSUPSemEventClose(pDevIns, pThis->hEvtInvQueue);
        pThis->hEvtInvQueue = NIL_SUPSEMEVENT;
    }

    DMAR_UNLOCK(pDevIns, pThisR3);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RT_NOREF(pCfg);

    PDMAR   pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PDMARR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PDMARR3);
    pThisR3->pDevInsR3 = pDevIns;

    LogFlowFunc(("iInstance=%d\n", iInstance));
    NOREF(iInstance);

    /*
     * Register the IOMMU with PDM.
     */
    PDMIOMMUREGR3 IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version       = PDM_IOMMUREGCC_VERSION;
    IommuReg.pfnMemAccess     = iommuIntelMemAccess;
    IommuReg.pfnMemBulkAccess = iommuIntelMemBulkAccess;
    IommuReg.pfnMsiRemap      = iommuIntelMsiRemap;
    IommuReg.u32TheEnd        = PDM_IOMMUREGCC_VERSION;
    int rc = PDMDevHlpIommuRegister(pDevIns, &IommuReg, &pThisR3->CTX_SUFF(pIommuHlp), &pThis->idxIommu);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to register ourselves as an IOMMU device"));
    if (pThisR3->CTX_SUFF(pIommuHlp)->u32Version != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper version mismatch; got %#x expected %#x"),
                                   pThisR3->CTX_SUFF(pIommuHlp)->u32Version, PDM_IOMMUHLPR3_VERSION);
    if (pThisR3->CTX_SUFF(pIommuHlp)->u32TheEnd != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper end-version mismatch; got %#x expected %#x"),
                                   pThisR3->CTX_SUFF(pIommuHlp)->u32TheEnd, PDM_IOMMUHLPR3_VERSION);
    AssertPtr(pThisR3->pIommuHlpR3->pfnLock);
    AssertPtr(pThisR3->pIommuHlpR3->pfnUnlock);
    AssertPtr(pThisR3->pIommuHlpR3->pfnLockIsOwner);
    AssertPtr(pThisR3->pIommuHlpR3->pfnSendMsi);

    /*
     * Use PDM's critical section (via helpers) for the IOMMU device.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Initialize PCI configuration registers.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    /* Header. */
    PDMPciDevSetVendorId(pPciDev,          DMAR_PCI_VENDOR_ID);         /* Intel */
    PDMPciDevSetDeviceId(pPciDev,          DMAR_PCI_DEVICE_ID);         /* VirtualBox DMAR device */
    PDMPciDevSetRevisionId(pPciDev,        DMAR_PCI_REVISION_ID);       /* VirtualBox specific device implementation revision */
    PDMPciDevSetClassBase(pPciDev,         VBOX_PCI_CLASS_SYSTEM);      /* System Base Peripheral */
    PDMPciDevSetClassSub(pPciDev,          VBOX_PCI_SUB_SYSTEM_OTHER);  /* Other */
    PDMPciDevSetHeaderType(pPciDev,        0);                          /* Single function, type 0 */
    PDMPciDevSetSubSystemId(pPciDev,       DMAR_PCI_DEVICE_ID);         /* VirtualBox DMAR device */
    PDMPciDevSetSubSystemVendorId(pPciDev, DMAR_PCI_VENDOR_ID);         /* Intel */

    /** @todo Chipset spec says PCI Express Capability Id. Relevant for us? */
    PDMPciDevSetStatus(pPciDev,            0);
    PDMPciDevSetCapabilityList(pPciDev,    0);

    /** @todo VTBAR at 0x180? */

    /*
     * Register the PCI function with PDM.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertLogRelRCReturn(rc, rc);

    /** @todo Register MSI but what's the MSI capability offset? */
#if 0
    /*
     * Register MSI support for the PCI device.
     * This must be done -after- registering it as a PCI device!
     */
#endif

    /*
     * Register MMIO region.
     */
    AssertCompile(!(DMAR_MMIO_BASE_PHYSADDR & X86_PAGE_4K_OFFSET_MASK));
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, DMAR_MMIO_BASE_PHYSADDR, DMAR_MMIO_SIZE, dmarMmioWrite, dmarMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD_QWORD | IOMMMIO_FLAGS_WRITE_DWORD_QWORD_ZEROED, "Intel-IOMMU",
                                   &pThis->hMmio);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register debugger info items.
     */
    rc = PDMDevHlpDBGFInfoRegister(pDevIns, "iommu", "Display IOMMU state.", dmarR3DbgInfo);
    AssertLogRelRCReturn(rc, rc);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadR3,  STAMTYPE_COUNTER, "R3/MmioRead",  STAMUNIT_OCCURENCES, "Number of MMIO reads in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadRZ,  STAMTYPE_COUNTER, "RZ/MmioRead",  STAMUNIT_OCCURENCES, "Number of MMIO reads in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteR3, STAMTYPE_COUNTER, "R3/MmioWrite", STAMUNIT_OCCURENCES, "Number of MMIO writes in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteRZ, STAMTYPE_COUNTER, "RZ/MmioWrite", STAMUNIT_OCCURENCES, "Number of MMIO writes in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapCfiR3, STAMTYPE_COUNTER, "R3/MsiRemapCfi", STAMUNIT_OCCURENCES, "Number of compatibility-format interrupt remap requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapCfiRZ, STAMTYPE_COUNTER, "RZ/MsiRemapCfi", STAMUNIT_OCCURENCES, "Number of compatibility-format interrupt remap requests in RZ.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapRfiR3, STAMTYPE_COUNTER, "R3/MsiRemapRfi", STAMUNIT_OCCURENCES, "Number of remappable-format interrupt remap requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapRfiRZ, STAMTYPE_COUNTER, "RZ/MsiRemapRfi", STAMUNIT_OCCURENCES, "Number of remappable-format interrupt remap requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemReadR3,  STAMTYPE_COUNTER, "R3/MemRead",  STAMUNIT_OCCURENCES, "Number of memory read translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemReadRZ,  STAMTYPE_COUNTER, "RZ/MemRead",  STAMUNIT_OCCURENCES, "Number of memory read translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemWriteR3,  STAMTYPE_COUNTER, "R3/MemWrite",  STAMUNIT_OCCURENCES, "Number of memory write translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemWriteRZ,  STAMTYPE_COUNTER, "RZ/MemWrite",  STAMUNIT_OCCURENCES, "Number of memory write translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkReadR3,  STAMTYPE_COUNTER, "R3/MemBulkRead",  STAMUNIT_OCCURENCES, "Number of memory bulk read translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkReadRZ,  STAMTYPE_COUNTER, "RZ/MemBulkRead",  STAMUNIT_OCCURENCES, "Number of memory bulk read translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkWriteR3, STAMTYPE_COUNTER, "R3/MemBulkWrite", STAMUNIT_OCCURENCES, "Number of memory bulk write translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkWriteRZ, STAMTYPE_COUNTER, "RZ/MemBulkWrite", STAMUNIT_OCCURENCES, "Number of memory bulk write translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCcInvDsc,          STAMTYPE_COUNTER, "R3/QI/CcInv",          STAMUNIT_OCCURENCES, "Number of cc_inv_dsc processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIotlbInvDsc,       STAMTYPE_COUNTER, "R3/QI/IotlbInv",       STAMUNIT_OCCURENCES, "Number of iotlb_inv_dsc processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatDevtlbInvDsc,      STAMTYPE_COUNTER, "R3/QI/DevtlbInv",      STAMUNIT_OCCURENCES, "Number of dev_tlb_inv_dsc processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIecInvDsc,         STAMTYPE_COUNTER, "R3/QI/IecInv",         STAMUNIT_OCCURENCES, "Number of iec_inv processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatInvWaitDsc,        STAMTYPE_COUNTER, "R3/QI/InvWait",        STAMUNIT_OCCURENCES, "Number of inv_wait_dsc processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPasidIotlbInvDsc,  STAMTYPE_COUNTER, "R3/QI/PasidIotlbInv",  STAMUNIT_OCCURENCES, "Number of p_iotlb_inv_dsc processed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPasidCacheInvDsc,  STAMTYPE_COUNTER, "R3/QI/PasidCacheInv",  STAMUNIT_OCCURENCES, "Number of pc_inv_dsc pprocessed.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPasidDevtlbInvDsc, STAMTYPE_COUNTER, "R3/QI/PasidDevtlbInv", STAMUNIT_OCCURENCES, "Number of p_dev_tlb_inv_dsc processed.");
#endif

    /*
     * Initialize registers.
     */
    dmarR3RegsInit(pDevIns);

    /*
     * Create invalidation-queue thread and semaphore.
     */
    char szInvQueueThread[32];
    RT_ZERO(szInvQueueThread);
    RTStrPrintf(szInvQueueThread, sizeof(szInvQueueThread), "IOMMU-QI-%u", iInstance);
    rc = PDMDevHlpThreadCreate(pDevIns, &pThisR3->pInvQueueThread, pThis, dmarR3InvQueueThread, dmarR3InvQueueThreadWakeUp,
                               0 /* cbStack */, RTTHREADTYPE_IO, szInvQueueThread);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->hEvtInvQueue);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Log some of the features exposed to software.
     */
    uint32_t const uVerReg         = pThis->uVerReg;
    uint8_t const  cMaxGstAddrBits = RT_BF_GET(pThis->fCapReg, VTD_BF_CAP_REG_MGAW) + 1;
    uint8_t const  cSupGstAddrBits = vtdCapRegGetSagawBits(RT_BF_GET(pThis->fCapReg, VTD_BF_CAP_REG_SAGAW));
    uint16_t const offFrcd         = RT_BF_GET(pThis->fCapReg, VTD_BF_CAP_REG_FRO);
    uint16_t const offIva          = RT_BF_GET(pThis->fExtCapReg, VTD_BF_ECAP_REG_IRO);
    LogRel(("%s: VER=%u.%u CAP=%#RX64 ECAP=%#RX64 (MGAW=%u bits, SAGAW=%u bits, FRO=%#x, IRO=%#x) mapped at %#RGp\n",
            DMAR_LOG_PFX, RT_BF_GET(uVerReg, VTD_BF_VER_REG_MAX), RT_BF_GET(uVerReg, VTD_BF_VER_REG_MIN),
            pThis->fCapReg, pThis->fExtCapReg, cMaxGstAddrBits, cSupGstAddrBits, offFrcd, offIva, DMAR_MMIO_BASE_PHYSADDR));

    return VINF_SUCCESS;
}

#else

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDMAR   pThis   = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    PDMARCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDMARCC);
    pThisCC->CTX_SUFF(pDevIns) = pDevIns;

    /* We will use PDM's critical section (via helpers) for the IOMMU device. */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Set up the MMIO RZ handlers. */
    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, dmarMmioWrite, dmarMmioRead, NULL /* pvUser */);
    AssertRCReturn(rc, rc);

    /* Set up the IOMMU RZ callbacks. */
    PDMIOMMUREGCC IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version       = PDM_IOMMUREGCC_VERSION;
    IommuReg.idxIommu         = pThis->idxIommu;
    IommuReg.pfnMemAccess     = iommuIntelMemAccess;
    IommuReg.pfnMemBulkAccess = iommuIntelMemBulkAccess;
    IommuReg.pfnMsiRemap      = iommuIntelMsiRemap;
    IommuReg.u32TheEnd        = PDM_IOMMUREGCC_VERSION;

    rc = PDMDevHlpIommuSetUpContext(pDevIns, &IommuReg, &pThisCC->CTX_SUFF(pIommuHlp));
    AssertRCReturn(rc, rc);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp), VERR_IOMMU_IPE_1);
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32Version == CTX_MID(PDM_IOMMUHLP,_VERSION), VERR_VERSION_MISMATCH);
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32TheEnd  == CTX_MID(PDM_IOMMUHLP,_VERSION), VERR_VERSION_MISMATCH);
    AssertPtr(pThisCC->CTX_SUFF(pIommuHlp)->pfnLock);
    AssertPtr(pThisCC->CTX_SUFF(pIommuHlp)->pfnUnlock);
    AssertPtr(pThisCC->CTX_SUFF(pIommuHlp)->pfnLockIsOwner);
    AssertPtr(pThisCC->CTX_SUFF(pIommuHlp)->pfnSendMsi);

    return VINF_SUCCESS;
}

#endif


/**
 * The device registration structure.
 */
PDMDEVREG const g_DeviceIommuIntel =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "iommu-intel",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PCI_BUILTIN,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DMAR),
    /* .cbInstanceCC = */           sizeof(DMARCC),
    /* .cbInstanceRC = */           sizeof(DMARRC),
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "IOMMU (Intel)",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           iommuIntelR3Construct,
    /* .pfnDestruct = */            iommuIntelR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               iommuIntelR3Reset,
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
    /* .pfnConstruct = */           iommuIntelRZConstruct,
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
    /* .pfnConstruct = */           iommuIntelRZConstruct,
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

