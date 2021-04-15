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
    } while (0);

/** Checks whether the MMIO offset is valid. */
#define DMAR_IS_MMIO_OFF_VALID(a_off)               (   (a_off) < DMAR_MMIO_GROUP_0_OFF_END \
                                                     || (a_off) - DMAR_MMIO_GROUP_1_OFF_FIRST < DMAR_MMIO_GROUP_1_SIZE)

/** @name DMAR implementation specifics.
 * @{ */
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
 *  We report 6 as the major version since we support queued invalidations as
 *  software may make assumptions based on that.
 *
 *  See Intel VT-d spec. 10.4.7 "Context Command Register" (CCMD_REG.CAIG). */
#define DMAR_VER_MAJOR                              6
/** DMAR implementation's minor version number (exposed to software). */
#define DMAR_VER_MINOR                              0
/** @} */

/** Release log prefix string. */
#define DMAR_LOG_PFX                                "Intel-IOMMU"
/** The current saved state version. */
#define DMAR_SAVED_STATE_VERSION                    1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The shared DMAR device state.
 */
typedef struct DMAR
{
    /** IOMMU device index. */
    uint32_t                    idxIommu;
    /** DMAR magic. */
    uint32_t                    u32Magic;

    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

    /** Registers (group 0). */
    uint8_t                     abRegs0[DMAR_MMIO_GROUP_0_SIZE];
    /** Registers (group 1). */
    uint8_t                     abRegs1[DMAR_MMIO_GROUP_1_SIZE];

    /** @name Register copies for a tiny bit faster and more convenient access.
     *  @{ */
    /** Copy of VER_REG. */
    uint8_t                     uVerReg;
    /** Alignment. */
    uint8_t                     abPadding[7];
    /** Copy of CAP_REG. */
    uint64_t                    fCap;
    /** Copy of ECAP_REG. */
    uint64_t                    fExtCap;
    /** @} */

#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER                 StatMmioReadR3;         /**< Number of MMIO reads in R3. */
    STAMCOUNTER                 StatMmioReadRZ;         /**< Number of MMIO reads in RZ. */
    STAMCOUNTER                 StatMmioWriteR3;        /**< Number of MMIO writes in R3. */
    STAMCOUNTER                 StatMmioWriteRZ;        /**< Number of MMIO writes in RZ. */

    STAMCOUNTER                 StatMsiRemapR3;         /**< Number of MSI remap requests in R3. */
    STAMCOUNTER                 StatMsiRemapRZ;         /**< Number of MSI remap requests in RZ. */

    STAMCOUNTER                 StatMemReadR3;          /**< Number of memory read translation requests in R3. */
    STAMCOUNTER                 StatMemReadRZ;          /**< Number of memory read translation requests in RZ. */
    STAMCOUNTER                 StatMemWriteR3;         /**< Number of memory write translation requests in R3. */
    STAMCOUNTER                 StatMemWriteRZ;         /**< Number of memory write translation requests in RZ. */

    STAMCOUNTER                 StatMemBulkReadR3;      /**< Number of memory read bulk translation requests in R3. */
    STAMCOUNTER                 StatMemBulkReadRZ;      /**< Number of memory read bulk translation requests in RZ. */
    STAMCOUNTER                 StatMemBulkWriteR3;     /**< Number of memory write bulk translation requests in R3. */
    STAMCOUNTER                 StatMemBulkWriteRZ;     /**< Number of memory write bulk translation requests in RZ. */
#endif
} DMAR;
/** Pointer to the DMAR device state. */
typedef DMAR *PDMAR;
/** Pointer to the const DMAR device state. */
typedef const DMAR *PCDMAR;

/**
 * The ring-3 DMAR device state.
 */
typedef struct DMARR3
{
    /** Device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The IOMMU helper. */
    R3PTRTYPE(PCPDMIOMMUHLPR3)  pIommuHlpR3;
} DMARR3;
/** Pointer to the ring-3 DMAR device state. */
typedef DMARR3 *PDMARR3;
/** Pointer to the const ring-3 DMAR device state. */
typedef const DMARR3 *PCDMARR3;

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
typedef const DMARR0 *PCDMARR0;

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
typedef const DMARRC *PCIDMARRC;

/** The DMAR device state for the current context. */
typedef CTX_SUFF(DMAR)  DMARCC;
/** Pointer to the DMAR device state for the current context. */
typedef CTX_SUFF(PDMAR) PDMARCC;


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


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/**
 * Gets the number of supported adjusted guest-address width (SAGAW) in bits given a
 * CAP_REG.SAGAW value.
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
 * Gets the supported adjusted guest-address width (SAGAW) given the maximum guest
 * address width (MGAW).
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
    uint16_t const offLast = offReg + cbReg - 1;
    AssertCompile(DMAR_MMIO_GROUP_0_OFF_FIRST == 0);
    AssertMsg(DMAR_IS_MMIO_OFF_VALID(offLast), ("off=%#x cb=%u\n", offReg, cbReg));

    uint8_t *const apbRegs[] = { &pThis->abRegs0[0], &pThis->abRegs1[0] };
    *pIdxGroup = !(offLast < DMAR_MMIO_GROUP_0_OFF_END);
    return apbRegs[*pIdxGroup];
}


/**
 * Writes a 64-bit register with the exactly the supplied value.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   uReg        The 64-bit value to write.
 */
DECLINLINE(void) dmarRegWriteRaw64(PDMAR pThis, uint16_t offReg, uint64_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = dmarRegGetGroup(pThis, offReg, sizeof(uint64_t), &idxGroup);
    NOREF(idxGroup);
    *(uint64_t *)(pabRegs + offReg) = uReg;
}


/**
 * Writes a 32-bit register with the exactly the supplied value.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   uReg        The 32-bit value to write.
 */
DECLINLINE(void) dmarRegWriteRaw32(PDMAR pThis, uint16_t offReg, uint32_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = dmarRegGetGroup(pThis, offReg, sizeof(uint32_t), &idxGroup);
    NOREF(idxGroup);
    *(uint32_t *)(pabRegs + offReg) = uReg;
}


/**
 * Reads a 64-bit register with exactly the value it contains.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 64-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
DECLINLINE(void) dmarRegReadRaw64(PDMAR pThis, uint16_t offReg, uint64_t *puReg, uint64_t *pfRwMask, uint64_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = dmarRegGetGroup(pThis, offReg, sizeof(uint64_t), &idxGroup);
    Assert(idxGroup < RT_ELEMENTS(g_apbRwMasks));
    uint8_t const *pabRwMasks   = g_apbRwMasks[idxGroup];
    uint8_t const *pabRw1cMasks = g_apbRw1cMasks[idxGroup];
    *puReg      = *(uint64_t *)(pabRegs      + offReg);
    *pfRwMask   = *(uint64_t *)(pabRwMasks   + offReg);
    *pfRw1cMask = *(uint64_t *)(pabRw1cMasks + offReg);
}


/**
 * Reads a 32-bit register with exactly the value it contains.
 *
 * @param   pThis       The shared DMAR device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 32-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
DECLINLINE(void) dmarRegReadRaw32(PDMAR pThis, uint16_t offReg, uint32_t *puReg, uint32_t *pfRwMask, uint32_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = dmarRegGetGroup(pThis, offReg, sizeof(uint32_t), &idxGroup);
    Assert(idxGroup < RT_ELEMENTS(g_apbRwMasks));
    uint8_t const *pabRwMasks   = g_apbRwMasks[idxGroup];
    uint8_t const *pabRw1cMasks = g_apbRw1cMasks[idxGroup];
    *puReg      = *(uint32_t *)(pabRegs      + offReg);
    *pfRwMask   = *(uint32_t *)(pabRwMasks   + offReg);
    *pfRw1cMask = *(uint32_t *)(pabRw1cMasks + offReg);
}


/**
 * Writes a 64-bit register as it would be when written by software.
 * This will preserve read-only bits, mask off reserved bits and clear RW1C bits.
 *
 * @returns The value that's actually written to the register.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 64-bit value to write.
 */
static uint64_t dmarRegWrite64(PDMAR pThis, uint16_t offReg, uint64_t uReg)
{
    /* Read current value from the 64-bit register. */
    uint64_t uCurReg;
    uint64_t fRwMask;
    uint64_t fRw1cMask;
    dmarRegReadRaw64(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);

    uint64_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint64_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint64_t const fRw1cBits = uReg & fRw1cMask;        /* Clear 1s written to RW1C bits. */
    uint64_t const uNewReg   = (fRoBits | fRwBits) & ~fRw1cBits;

    /* Write new value to the 64-bit register. */
    dmarRegWriteRaw64(pThis, offReg, uNewReg);
    return uNewReg;
}


/**
 * Writes a 32-bit register as it would be when written by software.
 * This will preserve read-only bits, mask off reserved bits and clear RW1C bits.
 *
 * @returns The value that's actually written to the register.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 32-bit value to write.
 */
static uint32_t dmarRegWrite32(PDMAR pThis, uint16_t offReg, uint32_t uReg)
{
    /* Read current value from the 32-bit register. */
    uint32_t uCurReg;
    uint32_t fRwMask;
    uint32_t fRw1cMask;
    dmarRegReadRaw32(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);

    uint32_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint32_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint32_t const fRw1cBits = uReg & fRw1cMask;        /* Clear 1s written to RW1C bits. */
    uint32_t const uNewReg   = (fRoBits | fRwBits) & ~fRw1cBits;

    /* Write new value to the 32-bit register. */
    dmarRegWriteRaw32(pThis, offReg, uNewReg);
    return uNewReg;
}


/**
 * Reads a 64-bit register as it would be when read by software.
 *
 * @returns The 64-bit register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint64_t dmarRegRead64(PDMAR pThis, uint16_t offReg)
{
    uint64_t uCurReg;
    uint64_t fRwMask;
    uint64_t fRw1cMask;
    dmarRegReadRaw64(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    NOREF(fRwMask); NOREF(fRw1cMask);
    return uCurReg;
}


/**
 * Reads a 32-bit register as it would be when read by software.
 *
 * @returns The 32-bit register value.
 * @param   pThis   The shared DMAR device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint32_t dmarRegRead32(PDMAR pThis, uint16_t offReg)
{
    uint32_t uCurReg;
    uint32_t fRwMask;
    uint32_t fRw1cMask;
    dmarRegReadRaw32(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    NOREF(fRwMask); NOREF(fRw1cMask);
    return uCurReg;
}


/**
 * Handles writes to IQT_REG.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   off         The MMIO register offset.
 * @param   uIqtReg     The value written to IQT_REG.
 */
static VBOXSTRICTRC dmarIqtRegWrite(PPDMDEVINS pDevIns, uint16_t off, uint64_t uIqtReg)
{
    /* We only care about the low dword of VTD_MMIO_OFF_IQT_REG. */
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    if (off == VTD_MMIO_OFF_IQT_REG)
    {
        /* Verify if the queue tail offset is aligned according to the descriptor width in IQA_REG. */
        uint16_t const offQueueTail = VTD_IQT_REG_GET_QT(uIqtReg);
        uint64_t const uIqaReg      = dmarRegRead64(pThis, VTD_MMIO_OFF_IQA_REG);
        uint8_t const  fDw          = RT_BF_GET(uIqaReg, VTD_BF_IQA_REG_DW);
        if (   fDw != VTD_IQA_REG_DW_256_BIT
            || !(offQueueTail & 0x1f))
        {
            /** @todo IOMMU: Figure out what to do here, like waking up worker thread or
             *        something. */
        }
        else
        {
            /* Raise invalidation queue error as queue tail not aligned to 256-bits. */
            /** @todo IOMMU: Raise error. */
        }
    }
    return VINF_SUCCESS;
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
    RT_NOREF7(pDevIns, idDevice, uIova, cbIova, fFlags, pGCPhysSpa, pcbContiguous);
    return VERR_NOT_IMPLEMENTED;
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
    RT_NOREF3(idDevice, pMsiIn, pMsiOut);
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
    STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatMsiRemap)); NOREF(pThis);

    return VERR_NOT_IMPLEMENTED;
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
        uint64_t const uRegWritten = cb == 8 ? dmarRegWrite64(pThis, offReg, *(uint64_t *)pv)
                                             : dmarRegWrite32(pThis, offReg, *(uint32_t *)pv);
        VBOXSTRICTRC rcStrict = VINF_SUCCESS;
        switch (off)
        {
            case VTD_MMIO_OFF_IQT_REG:
            case VTD_MMIO_OFF_IQT_REG + 4:
            {
                rcStrict = dmarIqtRegWrite(pDevIns, offReg, uRegWritten);
                break;
            }
        }

        LogFlowFunc(("offReg=%#x rc=%Rrc\n", offReg, VBOXSTRICTRC_VAL(rcStrict)));
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

        return VINF_SUCCESS;
    }

    return VINF_IOM_MMIO_UNUSED_FF;
}


#ifdef IN_RING3
/**
 * Initializes all registers in the DMAR unit.
 *
 * @param   pDevIns     The IOMMU device instance.
 */
static void dmarR3RegsInit(PPDMDEVINS pDevIns)
{
    PDMAR pThis = PDMDEVINS_2_DATA(pDevIns, PDMAR);
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

        uint8_t const fFl1gp  = 1;                              /* First-Level 1GB pages support. */
        uint8_t const fFl5lp  = 1;                              /* First-level 5-level paging support (PML5E). */
        uint8_t const fSl2mp  = fSlts & 1;                      /* Second-Level 2MB pages support. */
        uint8_t const fSl2gp  = fSlts & 1;                      /* Second-Level 1GB pages support. */
        uint8_t const fSllps  = fSl2mp                          /* Second-Level large page Support. */
                              | ((fSl2mp & fFl1gp) & RT_BIT(1));
        uint8_t const fMamv   = (fSl2gp ?                       /* Maximum address mask value (for second-level invalidations). */
                                X86_PAGE_1G_SHIFT : X86_PAGE_2M_SHIFT) - X86_PAGE_4K_SHIFT;
        uint8_t const fNd     = 2;                              /* Number of domains (0=16, 1=64, 2=256, 3=1K, 4=4K, 5=16K, 6=64K,
                                                                   7=Reserved). */
        uint8_t const fPsi    = 1;                              /* Page selective invalidation. */
        uint8_t const uMgaw   = cGstPhysAddrBits - 1;           /* Maximum guest address width. */
        uint8_t const uSagaw  = vtdCapRegGetSagaw(uMgaw);       /* Supported adjust guest address width. */
        uint16_t const offFro = DMAR_MMIO_OFF_FRCD_LO_REG >> 4; /* MMIO offset of FRCD registers. */

        pThis->fCap = RT_BF_MAKE(VTD_BF_CAP_REG_ND,      fNd)
                    | RT_BF_MAKE(VTD_BF_CAP_REG_AFL,     0)     /* Advanced fault logging not supported. */
                    | RT_BF_MAKE(VTD_BF_CAP_REG_RWBF,    0)     /* Software need not flush write-buffers. */
                    | RT_BF_MAKE(VTD_BF_CAP_REG_PLMR,    0)     /* Protected Low-Memory Region not supported. */
                    | RT_BF_MAKE(VTD_BF_CAP_REG_PHMR,    0)     /* Protected High-Memory Region not supported. */
                    | RT_BF_MAKE(VTD_BF_CAP_REG_CM,      1)     /** @todo Figure out if required when we impl. caching. */
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
                    | RT_BF_MAKE(VTD_BF_CAP_REG_ESIRTPS, 0)     /* Whether we invalidate interrupt cache on SIRTP flow. */
                    | RT_BF_MAKE(VTD_BF_CAP_REG_ESRTPS,  0);    /* Whether we invalidate translation cache on SRTP flow. */
        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_CAP_REG, pThis->fCap);
    }

    /* ECAP_REG */
    {
        uint8_t const  fQi    = 1;                              /* Queued invalidations. */
        uint8_t const  fIr    = !!(DMAR_ACPI_DMAR_FLAGS & ACPI_DMAR_F_INTR_REMAP);  /* Interrupt remapping support. */
        uint8_t const  fMhmv  = 0xf;                            /* Maximum handle mask value. */
        uint16_t const offIro = DMAR_MMIO_OFF_IVA_REG >> 4;     /* MMIO offset of IOTLB registers. */
        uint8_t const  fSrs   = 1;                              /* Supervisor request support. */
        uint8_t const  fEim   = 1;                              /* Extended interrupt mode.*/
        uint8_t const  fAdms  = 1;                              /* Abort DMA mode support. */

        pThis->fExtCap = RT_BF_MAKE(VTD_BF_ECAP_REG_C,      0)  /* Accesses don't snoop CPU cache. */
                       | RT_BF_MAKE(VTD_BF_ECAP_REG_QI,     1)
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
                       | RT_BF_MAKE(VTD_BF_ECAP_REG_SRS,    fSmts & fSrs)
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
                       | RT_BF_MAKE(VTD_BF_ECAP_REG_RPRIVS, 0); /** @todo figure out if we should/can support this? */
        dmarRegWriteRaw64(pThis, VTD_MMIO_OFF_ECAP_REG, pThis->fExtCap);
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
    Assert(!RT_BF_GET(pThis->fExtCap, VTD_BF_ECAP_REG_PRS));    /* PECTL_REG - Reserved if don't support PRS. */
    Assert(!RT_BF_GET(pThis->fExtCap, VTD_BF_ECAP_REG_MTS));    /* MTRRCAP_REG - Reserved if we don't support MTS. */
#endif
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuIntelR3Reset(PPDMDEVINS pDevIns)
{
    RT_NOREF1(pDevIns);
    LogFlowFunc(("\n"));

    dmarR3RegsInit(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuIntelR3Destruct(PPDMDEVINS pDevIns)
{
    RT_NOREF(pDevIns);
    LogFlowFunc(("\n"));
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

    /** @todo VTD: Chipset spec says PCI Express Capability Id. Relevant for us? */
    PDMPciDevSetStatus(pPciDev,            0);
    PDMPciDevSetCapabilityList(pPciDev,    0);

    /** @todo VTD: VTBAR at 0x180? */

    /*
     * Register the PCI function with PDM.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertLogRelRCReturn(rc, rc);

    /** @todo VTD: Register MSI but what's the MSI capability offset? */
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
                                   IOMMMIO_FLAGS_READ_DWORD_QWORD | IOMMMIO_FLAGS_WRITE_DWORD_QWORD_ZEROED,
                                   "Intel-IOMMU", &pThis->hMmio);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadR3,  STAMTYPE_COUNTER, "R3/MmioRead",  STAMUNIT_OCCURENCES, "Number of MMIO reads in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReadRZ,  STAMTYPE_COUNTER, "RZ/MmioRead",  STAMUNIT_OCCURENCES, "Number of MMIO reads in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteR3, STAMTYPE_COUNTER, "R3/MmioWrite", STAMUNIT_OCCURENCES, "Number of MMIO writes in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWriteRZ, STAMTYPE_COUNTER, "RZ/MmioWrite", STAMUNIT_OCCURENCES, "Number of MMIO writes in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapR3, STAMTYPE_COUNTER, "R3/MsiRemap", STAMUNIT_OCCURENCES, "Number of interrupt remap requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMsiRemapRZ, STAMTYPE_COUNTER, "RZ/MsiRemap", STAMUNIT_OCCURENCES, "Number of interrupt remap requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemReadR3,  STAMTYPE_COUNTER, "R3/MemRead",  STAMUNIT_OCCURENCES, "Number of memory read translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemReadRZ,  STAMTYPE_COUNTER, "RZ/MemRead",  STAMUNIT_OCCURENCES, "Number of memory read translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemWriteR3,  STAMTYPE_COUNTER, "R3/MemWrite",  STAMUNIT_OCCURENCES, "Number of memory write translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemWriteRZ,  STAMTYPE_COUNTER, "RZ/MemWrite",  STAMUNIT_OCCURENCES, "Number of memory write translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkReadR3,  STAMTYPE_COUNTER, "R3/MemBulkRead",  STAMUNIT_OCCURENCES, "Number of memory bulk read translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkReadRZ,  STAMTYPE_COUNTER, "RZ/MemBulkRead",  STAMUNIT_OCCURENCES, "Number of memory bulk read translation requests in RZ.");

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkWriteR3, STAMTYPE_COUNTER, "R3/MemBulkWrite", STAMUNIT_OCCURENCES, "Number of memory bulk write translation requests in R3.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMemBulkWriteRZ, STAMTYPE_COUNTER, "RZ/MemBulkWrite", STAMUNIT_OCCURENCES, "Number of memory bulk write translation requests in RZ.");
#endif

    /*
     * Initialize registers.
     */
    dmarR3RegsInit(pDevIns);

    /*
     * Log some of the features exposed to software.
     */
    uint32_t const uVerReg         = pThis->uVerReg;
    uint8_t const  cMaxGstAddrBits = RT_BF_GET(pThis->fCap, VTD_BF_CAP_REG_MGAW) + 1;
    uint8_t const  cSupGstAddrBits = vtdCapRegGetSagawBits(RT_BF_GET(pThis->fCap, VTD_BF_CAP_REG_SAGAW));
    uint16_t const offFrcd         = RT_BF_GET(pThis->fCap, VTD_BF_CAP_REG_FRO);
    uint16_t const offIva          = RT_BF_GET(pThis->fExtCap, VTD_BF_ECAP_REG_IRO);
    LogRel(("%s: VER=%u.%u CAP=%#RX64 ECAP=%#RX64 (MGAW=%u bits, SAGAW=%u bits, FRO=%#x, IRO=%#x) mapped at %#RGp\n", DMAR_LOG_PFX,
            RT_BF_GET(uVerReg, VTD_BF_VER_REG_MAX), RT_BF_GET(uVerReg, VTD_BF_VER_REG_MIN),
            pThis->fCap, pThis->fExtCap, cMaxGstAddrBits, cSupGstAddrBits, offFrcd, offIva, DMAR_MMIO_BASE_PHYSADDR));
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
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32Version == CTX_SUFF(PDM_IOMMUHLP)_VERSION, VERR_VERSION_MISMATCH);
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32TheEnd  == CTX_SUFF(PDM_IOMMUHLP)_VERSION, VERR_VERSION_MISMATCH);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp)->pfnLock,   VERR_INVALID_POINTER);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp)->pfnUnlock, VERR_INVALID_POINTER);

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

