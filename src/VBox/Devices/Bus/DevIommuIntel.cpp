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
#include <VBox/vmm/pdmdev.h>
#include <VBox/iommu-intel.h>

#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def VTD_LO_U32
 * Gets the low uint32_t of a uint64_t or something equivalent.
 *
 * This is suitable for casting constants outside code (since RT_LO_U32 can't be
 * used as it asserts for correctness when compiling on certain compilers). */
#define VTD_LO_U32(a)       (uint32_t)(UINT32_MAX & (a))

/** @def VTD_HI_U32
 * Gets the high uint32_t of a uint64_t or something equivalent.
 *
 * This is suitable for casting constants outside code (since RT_HI_U32 can't be
 * used as it asserts for correctness when compiling on certain compilers). */
#define VTD_HI_U32(a)       (uint32_t)((a) >> 32)

/** Release log prefix string. */
#define IOMMU_LOG_PFX                               "Intel-IOMMU"

/** The current saved state version. */
#define IOMMU_SAVED_STATE_VERSION                   1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The shared IOMMU device state.
 */
typedef struct IOMMU
{
    /** IOMMU device index (0 is at the top of the PCI tree hierarchy). */
    uint32_t                    idxIommu;
    /** IOMMU magic. */
    uint32_t                    u32Magic;

    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

    /** IOMMU registers (group 0). */
    uint8_t                     abRegs0[VTD_MMIO_GROUP_0_SIZE];
    /** IOMMU registers (group 1). */
    uint8_t                     abRegs1[VTD_MMIO_GROUP_1_SIZE];
} IOMMU;
/** Pointer to the IOMMU device state. */
typedef IOMMU *PIOMMU;
/** Pointer to the const IOMMU device state. */
typedef const IOMMU *PCIOMMU;

/**
 * The ring-3 IOMMU device state.
 */
typedef struct IOMMUR3
{
    /** Device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The IOMMU helper. */
    R3PTRTYPE(PCPDMIOMMUHLPR3)  pIommuHlpR3;
} IOMMUR3;
/** Pointer to the ring-3 IOMMU device state. */
typedef IOMMUR3 *PIOMMUR3;
/** Pointer to the const ring-3 IOMMU device state. */
typedef const IOMMUR3 *PCIOMMUR3;

/**
 * The ring-0 IOMMU device state.
 */
typedef struct IOMMUR0
{
    /** Device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** The IOMMU helper. */
    R0PTRTYPE(PCPDMIOMMUHLPR0)  pIommuHlpR0;
} IOMMUR0;
/** Pointer to the ring-0 IOMMU device state. */
typedef IOMMUR0 *PIOMMUR0;
/** Pointer to the const ring-0 IOMMU device state. */
typedef const IOMMUR0 *PCIOMMUR0;

/**
 * The raw-mode IOMMU device state.
 */
typedef struct IOMMURC
{
    /** Device instance. */
    PPDMDEVINSRC                pDevInsRC;
    /** The IOMMU helper. */
    RCPTRTYPE(PCPDMIOMMUHLPRC)  pIommuHlpRC;
} IOMMURC;
/** Pointer to the raw-mode IOMMU device state. */
typedef IOMMURC *PIOMMURC;
/** Pointer to the const raw-mode IOMMU device state. */
typedef const IOMMURC *CPIOMMURC;

/** The IOMMU device state for the current context. */
typedef CTX_SUFF(IOMMU)  IOMMUCC;
/** Pointer to the IOMMU device state for the current context. */
typedef CTX_SUFF(PIOMMU) PIOMMUCC;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Read-write masks for IOMMU registers (group 0).
 */
static const uint32_t g_au32RwMasks0[] =
{
    /* Offset  Register                  Low                                        High */
    /* 0x000   VER_REG               */  VTD_VER_REG_RW_MASK,
    /* 0x004   Reserved              */  0,
    /* 0x008   CAP_REG               */  VTD_LO_U32(VTD_CAP_REG_RW_MASK),           VTD_HI_U32(VTD_CAP_REG_RW_MASK),
    /* 0x010   ECAP_REG              */  VTD_LO_U32(VTD_ECAP_REG_RW_MASK),          VTD_HI_U32(VTD_ECAP_REG_RW_MASK),
    /* 0x018   GCMD_REG              */  VTD_GCMD_REG_RW_MASK,
    /* 0x01c   GSTS_REG              */  VTD_GSTS_REG_RW_MASK,
    /* 0x020   RTADDR_REG            */  VTD_LO_U32(VTD_RTADDR_REG_RW_MASK),        VTD_HI_U32(VTD_RTADDR_REG_RW_MASK),
    /* 0x028   CCMD_REG              */  VTD_LO_U32(VTD_CCMD_REG_RW_MASK),          VTD_HI_U32(VTD_CCMD_REG_RW_MASK),
    /* 0x030   Reserved              */  0,
    /* 0x034   FSTS_REG              */  VTD_FSTS_REG_RW_MASK,
    /* 0x038   FECTL_REG             */  VTD_FECTL_REG_RW_MASK,
    /* 0x03c   FEDATA_REG            */  VTD_FEDATA_REG_RW_MASK,
    /* 0x040   FEADDR_REG            */  VTD_FEADDR_REG_RW_MASK,
    /* 0x044   FEUADDR_REG           */  VTD_FEUADDR_REG_RW_MASK,
    /* 0x048   Reserved              */  0,                                         0,
    /* 0x050   Reserved              */  0,                                         0,
    /* 0x058   AFLOG_REG             */  VTD_LO_U32(VTD_AFLOG_REG_RW_MASK),         VTD_HI_U32(VTD_AFLOG_REG_RW_MASK),
    /* 0x060   Reserved              */  0,
    /* 0x064   PMEN_REG              */  0, /* RO as we don't support PLMR and PHMR. */
    /* 0x068   PLMBASE_REG           */  0, /* RO as we don't support PLMR. */
    /* 0x06c   PLMLIMIT_REG          */  0, /* RO as we don't support PLMR. */
    /* 0x070   PHMBASE_REG           */  0,                                         0, /* RO as we don't support PHMR. */
    /* 0x078   PHMLIMIT_REG          */  0,                                         0, /* RO as we don't support PHMR. */
    /* 0x080   IQH_REG               */  VTD_LO_U32(VTD_IQH_REG_RW_MASK),           VTD_HI_U32(VTD_IQH_REG_RW_MASK),
    /* 0x088   IQT_REG               */  VTD_LO_U32(VTD_IQT_REG_RW_MASK),           VTD_HI_U32(VTD_IQT_REG_RW_MASK),
    /* 0x090   IQA_REG               */  VTD_LO_U32(VTD_IQA_REG_RW_MASK),           VTD_HI_U32(VTD_IQA_REG_RW_MASK),
    /* 0x098   Reserved              */  0,
    /* 0x09c   ICS_REG               */  VTD_ICS_REG_RW_MASK,
    /* 0x0a0   IECTL_REG             */  VTD_IECTL_REG_RW_MASK,
    /* 0x0a4   IEDATA_REG            */  VTD_IEDATA_REG_RW_MASK,
    /* 0x0a8   IEADDR_REG            */  VTD_IEADDR_REG_RW_MASK,
    /* 0x0ac   IEUADDR_REG           */  VTD_IEUADDR_REG_RW_MASK,
    /* 0x0b0   IQERCD_REG            */  VTD_LO_U32(VTD_IQERCD_REG_RW_MASK),        VTD_HI_U32(VTD_IQERCD_REG_RW_MASK),
    /* 0x0b8   IRTA_REG              */  VTD_LO_U32(VTD_IRTA_REG_RW_MASK),          VTD_HI_U32(VTD_IRTA_REG_RW_MASK),
    /* 0x0c0   PQH_REG               */  VTD_LO_U32(VTD_PQH_REG_RW_MASK),           VTD_HI_U32(VTD_PQH_REG_RW_MASK),
    /* 0x0c8   PQT_REG               */  VTD_LO_U32(VTD_PQT_REG_RW_MASK),           VTD_HI_U32(VTD_PQT_REG_RW_MASK),
    /* 0x0d0   PQA_REG               */  VTD_LO_U32(VTD_PQA_REG_RW_MASK),           VTD_HI_U32(VTD_PQA_REG_RW_MASK),
    /* 0x0d8   Reserved              */  0,
    /* 0x0dc   PRS_REG               */  VTD_PRS_REG_RW_MASK,
    /* 0x0e0   PECTL_REG             */  VTD_PECTL_REG_RW_MASK,
    /* 0x0e4   PEDATA_REG            */  VTD_PEDATA_REG_RW_MASK,
    /* 0x0e8   PEADDR_REG            */  VTD_PEADDR_REG_RW_MASK,
    /* 0x0ec   PEUADDR_REG           */  VTD_PEUADDR_REG_RW_MASK,
    /* 0x0f0   Reserved              */  0,                                         0,
    /* 0x0f8   Reserved              */  0,                                         0,
    /* 0x100   MTRRCAP_REG           */  VTD_LO_U32(VTD_MTRRCAP_REG_RW_MASK),       VTD_HI_U32(VTD_MTRRCAP_REG_RW_MASK),
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
AssertCompile(sizeof(g_au32RwMasks0) == VTD_MMIO_GROUP_0_SIZE);

/**
 * Read-only Status, Write-1-to-clear masks for IOMMU registers (group 0).
 */
static const uint32_t g_au32Rw1cMasks0[] =
{
    /* Offset  Register                  Low                      High */
    /* 0x000   VER_REG               */  0,
    /* 0x004   Reserved              */  0,
    /* 0x008   CAP_REG               */  0,                       0,
    /* 0x010   ECAP_REG              */  0,                       0,
    /* 0x018   GCMD_REG              */  0,
    /* 0x01c   GSTS_REG              */  0,
    /* 0x020   RTADDR_REG            */  0,                       0,
    /* 0x028   CCMD_REG              */  0,                       0,
    /* 0x030   Reserved              */  0,
    /* 0x034   FSTS_REG              */  VTD_FSTS_REG_RW1C_MASK,
    /* 0x038   FECTL_REG             */  0,
    /* 0x03c   FEDATA_REG            */  0,
    /* 0x040   FEADDR_REG            */  0,
    /* 0x044   FEUADDR_REG           */  0,
    /* 0x048   Reserved              */  0,                       0,
    /* 0x050   Reserved              */  0,                       0,
    /* 0x058   AFLOG_REG             */  0,                       0,
    /* 0x060   Reserved              */  0,
    /* 0x064   PMEN_REG              */  0,
    /* 0x068   PLMBASE_REG           */  0,
    /* 0x06c   PLMLIMIT_REG          */  0,
    /* 0x070   PHMBASE_REG           */  0,                       0,
    /* 0x078   PHMLIMIT_REG          */  0,                       0,
    /* 0x080   IQH_REG               */  0,                       0,
    /* 0x088   IQT_REG               */  0,                       0,
    /* 0x090   IQA_REG               */  0,                       0,
    /* 0x098   Reserved              */  0,
    /* 0x09c   ICS_REG               */  VTD_ICS_REG_RW1C_MASK,
    /* 0x0a0   IECTL_REG             */  0,
    /* 0x0a4   IEDATA_REG            */  0,
    /* 0x0a8   IEADDR_REG            */  0,
    /* 0x0ac   IEUADDR_REG           */  0,
    /* 0x0b0   IQERCD_REG            */  0,                       0,
    /* 0x0b8   IRTA_REG              */  0,                       0,
    /* 0x0c0   PQH_REG               */  0,                       0,
    /* 0x0c8   PQT_REG               */  0,                       0,
    /* 0x0d0   PQA_REG               */  0,                       0,
    /* 0x0d8   Reserved              */  0,
    /* 0x0dc   PRS_REG               */  0,
    /* 0x0e0   PECTL_REG             */  0,
    /* 0x0e4   PEDATA_REG            */  0,
    /* 0x0e8   PEADDR_REG            */  0,
    /* 0x0ec   PEUADDR_REG           */  0,
    /* 0x0f0   Reserved              */  0,                       0,
    /* 0x0f8   Reserved              */  0,                       0,
    /* 0x100   MTRRCAP_REG           */  0,                       0,
    /* 0x108   MTRRDEF_REG           */  0,                       0,
    /* 0x110   Reserved              */  0,                       0,
    /* 0x118   Reserved              */  0,                       0,
    /* 0x120   MTRR_FIX64_00000_REG  */  0,                       0,
    /* 0x128   MTRR_FIX16K_80000_REG */  0,                       0,
    /* 0x130   MTRR_FIX16K_A0000_REG */  0,                       0,
    /* 0x138   MTRR_FIX4K_C0000_REG  */  0,                       0,
    /* 0x140   MTRR_FIX4K_C8000_REG  */  0,                       0,
    /* 0x148   MTRR_FIX4K_D0000_REG  */  0,                       0,
    /* 0x150   MTRR_FIX4K_D8000_REG  */  0,                       0,
    /* 0x158   MTRR_FIX4K_E0000_REG  */  0,                       0,
    /* 0x160   MTRR_FIX4K_E8000_REG  */  0,                       0,
    /* 0x168   MTRR_FIX4K_F0000_REG  */  0,                       0,
    /* 0x170   MTRR_FIX4K_F8000_REG  */  0,                       0,
    /* 0x178   Reserved              */  0,                       0,
    /* 0x180   MTRR_PHYSBASE0_REG    */  0,                       0,
    /* 0x188   MTRR_PHYSMASK0_REG    */  0,                       0,
    /* 0x190   MTRR_PHYSBASE1_REG    */  0,                       0,
    /* 0x198   MTRR_PHYSMASK1_REG    */  0,                       0,
    /* 0x1a0   MTRR_PHYSBASE2_REG    */  0,                       0,
    /* 0x1a8   MTRR_PHYSMASK2_REG    */  0,                       0,
    /* 0x1b0   MTRR_PHYSBASE3_REG    */  0,                       0,
    /* 0x1b8   MTRR_PHYSMASK3_REG    */  0,                       0,
    /* 0x1c0   MTRR_PHYSBASE4_REG    */  0,                       0,
    /* 0x1c8   MTRR_PHYSMASK4_REG    */  0,                       0,
    /* 0x1d0   MTRR_PHYSBASE5_REG    */  0,                       0,
    /* 0x1d8   MTRR_PHYSMASK5_REG    */  0,                       0,
    /* 0x1e0   MTRR_PHYSBASE6_REG    */  0,                       0,
    /* 0x1e8   MTRR_PHYSMASK6_REG    */  0,                       0,
    /* 0x1f0   MTRR_PHYSBASE7_REG    */  0,                       0,
    /* 0x1f8   MTRR_PHYSMASK7_REG    */  0,                       0,
    /* 0x200   MTRR_PHYSBASE8_REG    */  0,                       0,
    /* 0x208   MTRR_PHYSMASK8_REG    */  0,                       0,
    /* 0x210   MTRR_PHYSBASE9_REG    */  0,                       0,
    /* 0x218   MTRR_PHYSMASK9_REG    */  0,                       0,
};
AssertCompile(sizeof(g_au32Rw1cMasks0) == VTD_MMIO_GROUP_0_SIZE);

/**
 * Read-write masks for IOMMU registers (group 1).
 */
static const uint32_t g_au32RwMasks1[] =
{
    /* Offset  Register                  Low                                 High */
    /* 0xe00   VCCAP_REG             */  VTD_LO_U32(VTD_VCCAP_REG_RW_MASK),  VTD_HI_U32(VTD_VCCAP_REG_RW_MASK),
    /* 0xe08   Reserved              */  0,                                  0,
    /* 0xe10   VCMD_REG              */  0,                                  0, /* RO as we don't support VCS. */
    /* 0xe18   Reserved              */  0,                                  0,
    /* 0xe20   VCRSP_REG             */  0,                                  0, /* RO as we don't support VCS. */
};
AssertCompile(sizeof(g_au32RwMasks1) == VTD_MMIO_GROUP_1_SIZE);

/**
 * Read-only Status, Write-1-to-clear masks for IOMMU registers (group 1).
 */
static const uint32_t g_au32Rw1cMasks1[] =
{
    /* Offset  Register                  Low  High */
    /* 0xe00   VCCAP_REG             */  0,   0,
    /* 0xe08   Reserved              */  0,   0,
    /* 0xe10   VCMD_REG              */  0,   0,
    /* 0xe18   Reserved              */  0,   0,
    /* 0xe20   VCRSP_REG             */  0,   0,
};
AssertCompile(sizeof(g_au32Rw1cMasks1) == VTD_MMIO_GROUP_1_SIZE);

/** Array of RW masks for all register groups. */
static const uint8_t *g_apbRwMasks[]   = { (uint8_t *)&g_au32RwMasks0[0], (uint8_t *)&g_au32RwMasks1[0] };

/** Array of RW1C masks for all register groups. */
static const uint8_t *g_apbRw1cMasks[] = { (uint8_t *)&g_au32Rw1cMasks0[0], (uint8_t *)&g_au32Rw1cMasks1[0] };


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * Gets the group the register belongs to given its MMIO offset.
 *
 * @returns Pointer to the first element of the register group.
 * @param   pThis       The shared IOMMU device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   cbReg       The size of the access being made (for bounds check).
 * @param   pIdxGroup   Where to store the index of the register group the register
 *                      belongs to.
 */
DECLINLINE(uint8_t *) iommuIntelRegGetGroup(PIOMMU pThis, uint16_t offReg, uint8_t cbReg, uint8_t *pIdxGroup)
{
    AssertCompile(VTD_MMIO_GROUP_0_OFF_FIRST == 0);
    AssertMsg(   offReg + cbReg <= VTD_MMIO_GROUP_0_OFF_END
              || (offReg >= VTD_MMIO_GROUP_1_OFF_FIRST && offReg + cbReg <= VTD_MMIO_GROUP_1_OFF_END), ("%#x\n", offReg));
    NOREF(cbReg);

    uint8_t *apbRegs[] = { &pThis->abRegs0[0], &pThis->abRegs1[0] };
    *pIdxGroup = !(offReg < VTD_MMIO_GROUP_0_OFF_END);
    return apbRegs[*pIdxGroup];
}


/**
 * Writes a 64-bit register with the exactly the supplied value.
 *
 * @param   pThis       The shared IOMMU device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   uReg        The 64-bit value to write.
 */
DECLINLINE(void) iommuIntelRegWriteRaw64(PIOMMU pThis, uint16_t offReg, uint64_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = iommuIntelRegGetGroup(pThis, offReg, sizeof(uint64_t), &idxGroup);
    NOREF(idxGroup);
    *(uint64_t *)(pabRegs + offReg) = uReg;
}


/**
 * Writes a 32-bit register with the exactly the supplied value.
 *
 * @param   pThis       The shared IOMMU device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   uReg        The 32-bit value to write.
 */
DECLINLINE(void) iommuIntelRegWriteRaw32(PIOMMU pThis, uint16_t offReg, uint32_t uReg)
{
    uint8_t idxGroup;
    uint8_t *pabRegs = iommuIntelRegGetGroup(pThis, offReg, sizeof(uint32_t), &idxGroup);
    NOREF(idxGroup);
    *(uint32_t *)(pabRegs + offReg) = uReg;
}


/**
 * Reads a 64-bit register with exactly the value it contains.
 *
 * @param   pThis       The shared IOMMU device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 64-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
DECLINLINE(void) iommuIntelRegReadRaw64(PIOMMU pThis, uint16_t offReg, uint64_t *puReg, uint64_t *pfRwMask, uint64_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = iommuIntelRegGetGroup(pThis, offReg, sizeof(uint64_t), &idxGroup);
    uint8_t const *pabRwMasks   = g_apbRwMasks[idxGroup];
    uint8_t const *pabRw1cMasks = g_apbRw1cMasks[idxGroup];
    *puReg      = *(uint64_t *)(pabRegs      + offReg);
    *pfRwMask   = *(uint64_t *)(pabRwMasks   + offReg);
    *pfRw1cMask = *(uint64_t *)(pabRw1cMasks + offReg);
}


/**
 * Reads a 32-bit register with exactly the value it contains.
 *
 * @param   pThis       The shared IOMMU device state.
 * @param   offReg      The MMIO offset of the register.
 * @param   puReg       Where to store the raw 32-bit register value.
 * @param   pfRwMask    Where to store the RW mask corresponding to this register.
 * @param   pfRw1cMask  Where to store the RW1C mask corresponding to this register.
 */
DECLINLINE(void) iommuIntelRegReadRaw32(PIOMMU pThis, uint16_t offReg, uint32_t *puReg, uint32_t *pfRwMask, uint32_t *pfRw1cMask)
{
    uint8_t idxGroup;
    uint8_t const *pabRegs      = iommuIntelRegGetGroup(pThis, offReg, sizeof(uint32_t), &idxGroup);
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
 * @param   pThis   The shared IOMMU device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 64-bit value to write.
 */
static void iommuIntelRegWrite64(PIOMMU pThis, uint16_t offReg, uint64_t uReg)
{
    /* Read current value from the 64-bit register. */
    uint64_t uCurReg;
    uint64_t fRwMask;
    uint64_t fRw1cMask;
    iommuIntelRegReadRaw64(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);

    uint64_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint64_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint64_t const fRw1cBits = ~(fRw1cMask & uReg);     /* Clear newly written RW1C bits. */
    uint64_t const uNewReg   = (fRoBits | fRwBits) & fRw1cBits;

    /* Write new value to the 64-bit register. */
    iommuIntelRegWriteRaw64(pThis, offReg, uNewReg);
}


/**
 * Writes a 32-bit register as it would be when written by software.
 * This will preserve read-only bits, mask off reserved bits and clear RW1C bits.
 *
 * @param   pThis   The shared IOMMU device state.
 * @param   offReg  The MMIO offset of the register.
 * @param   uReg    The 32-bit value to write.
 */
static void iommuIntelRegWrite32(PIOMMU pThis, uint16_t offReg, uint64_t uReg)
{
    /* Read current value from the 32-bit register. */
    uint32_t uCurReg;
    uint32_t fRwMask;
    uint32_t fRw1cMask;
    iommuIntelRegReadRaw32(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);

    uint32_t const fRoBits   = uCurReg & ~fRwMask;      /* Preserve current read-only and reserved bits. */
    uint32_t const fRwBits   = uReg & fRwMask;          /* Merge newly written read/write bits. */
    uint32_t const fRw1cBits = ~(fRw1cMask & uReg);     /* Clear newly written RW1C bits. */
    uint32_t const uNewReg   = (fRoBits | fRwBits) & fRw1cBits;

    /* Write new value to the 32-bit register. */
    iommuIntelRegWriteRaw32(pThis, offReg, uNewReg);
}


/**
 * Reads a 64-bit register as it would be when read by software.
 *
 * @returns The 64-bit register value.
 * @param   pThis   The shared IOMMU device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint64_t iommuIntelRegRead64(PIOMMU pThis, uint16_t offReg)
{
    uint64_t uCurReg;
    uint64_t fRwMask;
    uint64_t fRw1cMask;
    iommuIntelRegReadRaw64(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    NOREF(fRwMask); NOREF(fRw1cMask);
    return uCurReg;
}


/**
 * Reads a 32-bit register as it would be when read by software.
 *
 * @returns The 32-bit register value.
 * @param   pThis   The shared IOMMU device state.
 * @param   offReg  The MMIO offset of the register.
 */
static uint32_t iommuIntelRegRead32(PIOMMU pThis, uint16_t offReg)
{
    uint32_t uCurReg;
    uint32_t fRwMask;
    uint32_t fRw1cMask;
    iommuIntelRegReadRaw32(pThis, offReg, &uCurReg, &fRwMask, &fRw1cMask);
    NOREF(fRwMask); NOREF(fRw1cMask);
    return uCurReg;
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
    RT_NOREF4(pDevIns, idDevice, pMsiIn, pMsiOut);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuIntelMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    RT_NOREF5(pDevIns, pvUser, off, pv, cb);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuIntelMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    RT_NOREF1(pvUser);

    AssertReturn(!(off & 3), VINF_IOM_MMIO_UNUSED_FF);
    AssertReturn(cb == 4 || cb == 8, VINF_IOM_MMIO_UNUSED_FF);

    PIOMMU         pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    uint16_t const offReg  = off;
    uint16_t const offLast = offReg + cb - 1;
    if (   offLast < VTD_MMIO_GROUP_0_OFF_END
        || offLast - VTD_MMIO_GROUP_1_OFF_FIRST < VTD_MMIO_GROUP_1_SIZE)
    {
        if (cb == 8)
            *(uint64_t *)pv = iommuIntelRegRead64(pThis, offReg);
        else
            *(uint32_t *)pv = iommuIntelRegRead32(pThis, offReg);
        return VINF_SUCCESS;
    }

    return VINF_IOM_MMIO_UNUSED_FF;
}


#ifdef IN_RING3
/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuIntelR3Reset(PPDMDEVINS pDevIns)
{
    RT_NOREF1(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuIntelR3Destruct(PPDMDEVINS pDevIns)
{
    RT_NOREF(pDevIns);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RT_NOREF(pCfg);

    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUR3);
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


    return VERR_NOT_IMPLEMENTED;
}

#else

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUCC);
    pThisCC->CTX_SUFF(pDevIns) = pDevIns;

    /* We will use PDM's critical section (via helpers) for the IOMMU device. */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Set up the MMIO RZ handlers. */
    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, iommuIntelMmioWrite, iommuIntelMmioRead, NULL /* pvUser */);
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
const PDMDEVREG g_DeviceIommuIntel =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "iommu-intel",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PCI_BUILTIN,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(IOMMU),
    /* .cbInstanceCC = */           sizeof(IOMMUCC),
    /* .cbInstanceRC = */           sizeof(IOMMURC),
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

