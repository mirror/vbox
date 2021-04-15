/** @file
 * IOMMU - Input/Output Memory Management Unit (Intel).
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_iommu_intel_h
#define VBOX_INCLUDED_iommu_intel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assertcompile.h>
#include <iprt/types.h>


/**
 * @name MMIO register offsets.
 * In accordance with the Intel spec.
 * @{
 */
#define VTD_MMIO_OFF_VER_REG                                    0x000  /**< Version. */
#define VTD_MMIO_OFF_CAP_REG                                    0x008  /**< Capability. */
#define VTD_MMIO_OFF_ECAP_REG                                   0x010  /**< Extended Capability. */
#define VTD_MMIO_OFF_GCMD_REG                                   0x018  /**< Global Command. */
#define VTD_MMIO_OFF_GSTS_REG                                   0x01c  /**< Global Status. */
#define VTD_MMIO_OFF_RTADDR_REG                                 0x020  /**< Root Table Address. */
#define VTD_MMIO_OFF_CCMD_REG                                   0x028  /**< Context Command. */

#define VTD_MMIO_OFF_FSTS_REG                                   0x034  /**< Fault Status.*/
#define VTD_MMIO_OFF_FECTL_REG                                  0x038  /**< Fault Event Control.*/
#define VTD_MMIO_OFF_FEDATA_REG                                 0x03c  /**< Fault Event Data. */
#define VTD_MMIO_OFF_FEADDR_REG                                 0x040  /**< Fault Event Address. */
#define VTD_MMIO_OFF_FEUADDR_REG                                0x044  /**< Fault Event Upper Address. */

#define VTD_MMIO_OFF_AFLOG_REG                                  0x058  /**< Advance Fault Log. */

#define VTD_MMIO_OFF_PMEN_REG                                   0x064  /**< Protected Memory Enable (PMEN). */
#define VTD_MMIO_OFF_PLMBASE_REG                                0x068  /**< Protected Low Memory Base. */
#define VTD_MMIO_OFF_PLMLIMIT_REG                               0x06c  /**< Protected Low Memory Limit. */
#define VTD_MMIO_OFF_PHMBASE_REG                                0x070  /**< Protected High Memory Base. */
#define VTD_MMIO_OFF_PHMLIMIT_REG                               0x078  /**< Protected High Memory Limit. */

#define VTD_MMIO_OFF_IQH_REG                                    0x080  /**< Invalidation Queue Head. */
#define VTD_MMIO_OFF_IQT_REG                                    0x088  /**< Invalidation Queue Tail. */
#define VTD_MMIO_OFF_IQA_REG                                    0x090  /**< Invalidation Queue Address. */
#define VTD_MMIO_OFF_ICS_REG                                    0x09c  /**< Invalidation Completion Status. */
#define VTD_MMIO_OFF_IECTL_REG                                  0x0a0  /**< Invalidation Completion Event Control. */
#define VTD_MMIO_OFF_IEDATA_REG                                 0x0a4  /**< Invalidation Completion Event Data. */
#define VTD_MMIO_OFF_IEADDR_REG                                 0x0a8  /**< Invalidation Completion Event Address. */
#define VTD_MMIO_OFF_IEUADDR_REG                                0x0ac  /**< Invalidation Completion Event Upper Address. */
#define VTD_MMIO_OFF_IQERCD_REG                                 0x0b0  /**< Invalidation Queue Error Record. */

#define VTD_MMIO_OFF_IRTA_REG                                   0x0b8  /**< Interrupt Remapping Table Address. */

#define VTD_MMIO_OFF_PQH_REG                                    0x0c0  /**< Page Request Queue Head. */
#define VTD_MMIO_OFF_PQT_REG                                    0x0c8  /**< Page Request Queue Tail. */
#define VTD_MMIO_OFF_PQA_REG                                    0x0d0  /**< Page Request Queue Address. */
#define VTD_MMIO_OFF_PRS_REG                                    0x0dc  /**< Page Request Status. */
#define VTD_MMIO_OFF_PECTL_REG                                  0x0e0  /**< Page Request Event Control. */
#define VTD_MMIO_OFF_PEDATA_REG                                 0x0e4  /**< Page Request Event Data. */
#define VTD_MMIO_OFF_PEADDR_REG                                 0x0e8  /**< Page Request Event Address. */
#define VTD_MMIO_OFF_PEUADDR_REG                                0x0ec  /**< Page Request Event Upper Address. */

#define VTD_MMIO_OFF_MTRRCAP_REG                                0x100  /**< MTRR Capabliity. */
#define VTD_MMIO_OFF_MTRRDEF_REG                                0x108  /**< MTRR Default Type. */

#define VTD_MMIO_OFF_MTRR_FIX64_00000_REG                       0x120  /**< Fixed-range MTRR Register for 64K at 00000. */
#define VTD_MMIO_OFF_MTRR_FIX16K_80000_REG                      0x128  /**< Fixed-range MTRR Register for 16K at 80000. */
#define VTD_MMIO_OFF_MTRR_FIX16K_A0000_REG                      0x130  /**< Fixed-range MTRR Register for 16K at a0000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_C0000_REG                       0x138  /**< Fixed-range MTRR Register for  4K at c0000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_C8000_REG                       0x140  /**< Fixed-range MTRR Register for  4K at c8000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_D0000_REG                       0x148  /**< Fixed-range MTRR Register for  4K at d0000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_D8000_REG                       0x150  /**< Fixed-range MTRR Register for  4K at d8000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_E0000_REG                       0x158  /**< Fixed-range MTRR Register for  4K at e0000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_E8000_REG                       0x160  /**< Fixed-range MTRR Register for  4K at e8000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_F0000_REG                       0x168  /**< Fixed-range MTRR Register for  4K at f0000. */
#define VTD_MMIO_OFF_MTRR_FIX4K_F8000_REG                       0x170  /**< Fixed-range MTRR Register for  4K at f8000. */

#define VTD_MMIO_OFF_MTRR_PHYSBASE0_REG                         0x180  /**< Variable-range MTRR Base 0. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK0_REG                         0x188  /**< Variable-range MTRR Mask 0. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE1_REG                         0x190  /**< Variable-range MTRR Base 1. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK1_REG                         0x198  /**< Variable-range MTRR Mask 1. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE2_REG                         0x1a0  /**< Variable-range MTRR Base 2. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK2_REG                         0x1a8  /**< Variable-range MTRR Mask 2. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE3_REG                         0x1b0  /**< Variable-range MTRR Base 3. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK3_REG                         0x1b8  /**< Variable-range MTRR Mask 3. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE4_REG                         0x1c0  /**< Variable-range MTRR Base 4. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK4_REG                         0x1c8  /**< Variable-range MTRR Mask 4. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE5_REG                         0x1d0  /**< Variable-range MTRR Base 5. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK5_REG                         0x1d8  /**< Variable-range MTRR Mask 5. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE6_REG                         0x1e0  /**< Variable-range MTRR Base 6. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK6_REG                         0x1e8  /**< Variable-range MTRR Mask 6. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE7_REG                         0x1f0  /**< Variable-range MTRR Base 7. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK7_REG                         0x1f8  /**< Variable-range MTRR Mask 7. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE8_REG                         0x200  /**< Variable-range MTRR Base 8. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK8_REG                         0x208  /**< Variable-range MTRR Mask 8. */
#define VTD_MMIO_OFF_MTRR_PHYSBASE9_REG                         0x210  /**< Variable-range MTRR Base 9. */
#define VTD_MMIO_OFF_MTRR_PHYSMASK9_REG                         0x218  /**< Variable-range MTRR Mask 9. */

#define VTD_MMIO_OFF_VCCAP_REG                                  0xe00  /**< Virtual Command Capability. */
#define VTD_MMIO_OFF_VCMD_REG                                   0xe10  /**< Virtual Command. */
#define VTD_MMIO_OFF_VCMDRSVD_REG                               0xe18  /**< Reserved for future for Virtual Command. */
#define VTD_MMIO_OFF_VCRSP_REG                                  0xe20  /**< Virtual Command Response. */
#define VTD_MMIO_OFF_VCRSPRSVD_REG                              0xe28  /**< Reserved for future for Virtual Command Response. */
/** @} */


/** @name Root Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_0_ROOT_ENTRY_P_SHIFT                             0
#define VTD_BF_0_ROOT_ENTRY_P_MASK                              UINT64_C(0x0000000000000001)
/** R: Reserved (bits 11:1). */
#define VTD_BF_0_ROOT_ENTRY_RSVD_11_1_SHIFT                     1
#define VTD_BF_0_ROOT_ENTRY_RSVD_11_1_MASK                      UINT64_C(0x0000000000000ffe)
/** CTP: Context-Table Pointer. */
#define VTD_BF_0_ROOT_ENTRY_CTP_SHIFT                           12
#define VTD_BF_0_ROOT_ENTRY_CTP_MASK                            UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_ROOT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, RSVD_11_1, CTP));

/** Root Entry. */
typedef struct VTD_ROOT_ENTRY_T
{
    /** The qwords in the root entry. */
    uint64_t        au64[2];
} VTD_ROOT_ENTRY_T;
/** Pointer to a root entry. */
typedef VTD_ROOT_ENTRY_T *PVTD_ROOT_ENTRY_T;
/** Pointer to a const root entry. */
typedef VTD_ROOT_ENTRY_T const *PCVTD_ROOT_ENTRY_T;
/** @} */


/** @name Scalable-mode Root Entry.
 * In accordance with the Intel spec.
 * @{ */
/** LP: Lower Present. */
#define VTD_BF_0_SM_ROOT_ENTRY_LP_SHIFT                         0
#define VTD_BF_0_SM_ROOT_ENTRY_LP_MASK                          UINT64_C(0x0000000000000001)
/** R: Reserved (bits 11:1). */
#define VTD_BF_0_SM_ROOT_ENTRY_RSVD_11_1_SHIFT                  1
#define VTD_BF_0_SM_ROOT_ENTRY_RSVD_11_1_MASK                   UINT64_C(0x0000000000000ffe)
/** LCTP: Lower Context-Table Pointer */
#define VTD_BF_0_SM_ROOT_ENTRY_LCTP_SHIFT                       12
#define VTD_BF_0_SM_ROOT_ENTRY_LCTP_MASK                        UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_SM_ROOT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (LP, RSVD_11_1, LCTP));

/** UP: Upper Present. */
#define VTD_BF_1_SM_ROOT_ENTRY_UP_SHIFT                         0
#define VTD_BF_1_SM_ROOT_ENTRY_UP_MASK                          UINT64_C(0x0000000000000001)
/** R: Reserved (bits 11:1). */
#define VTD_BF_1_SM_ROOT_ENTRY_RSVD_11_1_SHIFT                  1
#define VTD_BF_1_SM_ROOT_ENTRY_RSVD_11_1_MASK                   UINT64_C(0x0000000000000ffe)
/** UCTP: Upper Context-Table Pointer. */
#define VTD_BF_1_SM_ROOT_ENTRY_UCTP_SHIFT                       12
#define VTD_BF_1_SM_ROOT_ENTRY_UCTP_MASK                        UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_SM_ROOT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (UP, RSVD_11_1, UCTP));

/** Scalable-mode root entry. */
typedef struct VTD_SM_ROOT_ENTRY_T
{
    /** The lower scalable-mode root entry. */
    uint64_t        uLower;
    /** The upper scalable-mode root entry. */
    uint64_t        uUpper;
} VTD_SM_ROOT_ENTRY_T;
/** Pointer to a scalable-mode root entry. */
typedef VTD_SM_ROOT_ENTRY_T *PVTD_SM_ROOT_ENTRY_T;
/** Pointer to a const scalable-mode root entry. */
typedef VTD_SM_ROOT_ENTRY_T const *PCVTD_SM_ROOT_ENTRY_T;
/** @} */


/** @name Context Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_0_CONTEXT_ENTRY_P_SHIFT                          0
#define VTD_BF_0_CONTEXT_ENTRY_P_MASK                           UINT64_C(0x0000000000000001)
/** FPD: Fault Processing Disable. */
#define VTD_BF_0_CONTEXT_ENTRY_FPD_SHIFT                        1
#define VTD_BF_0_CONTEXT_ENTRY_FPD_MASK                         UINT64_C(0x0000000000000002)
/** TT: Translation Type. */
#define VTD_BF_0_CONTEXT_ENTRY_TT_SHIFT                         2
#define VTD_BF_0_CONTEXT_ENTRY_TT_MASK                          UINT64_C(0x000000000000000c)
/** R: Reserved (bits 11:4). */
#define VTD_BF_0_CONTEXT_ENTRY_RSVD_11_4_SHIFT                  4
#define VTD_BF_0_CONTEXT_ENTRY_RSVD_11_4_MASK                   UINT64_C(0x0000000000000ff0)
/** SLPTPTR: Second Level Page Translation Pointer. */
#define VTD_BF_0_CONTEXT_ENTRY_SLPTPTR_SHIFT                    12
#define VTD_BF_0_CONTEXT_ENTRY_SLPTPTR_MASK                     UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_CONTEXT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, FPD, TT, RSVD_11_4, SLPTPTR));

/** AW: Address Width. */
#define VTD_BF_1_CONTEXT_ENTRY_AW_SHIFT                         0
#define VTD_BF_1_CONTEXT_ENTRY_AW_MASK                          UINT64_C(0x0000000000000007)
/** IGN: Ignored (bits 6:3). */
#define VTD_BF_1_CONTEXT_ENTRY_IGN_6_3_SHIFT                    3
#define VTD_BF_1_CONTEXT_ENTRY_IGN_6_3_MASK                     UINT64_C(0x0000000000000078)
/** R: Reserved (bit 7). */
#define VTD_BF_1_CONTEXT_ENTRY_RSVD_7_SHIFT                     7
#define VTD_BF_1_CONTEXT_ENTRY_RSVD_7_MASK                      UINT64_C(0x0000000000000080)
/** DID: Domain Identifier. */
#define VTD_BF_1_CONTEXT_ENTRY_DID_SHIFT                        8
#define VTD_BF_1_CONTEXT_ENTRY_DID_MASK                         UINT64_C(0x0000000000ffff00)
/** R: Reserved (bits 63:24). */
#define VTD_BF_1_CONTEXT_ENTRY_RSVD_63_24_SHIFT                 24
#define VTD_BF_1_CONTEXT_ENTRY_RSVD_63_24_MASK                  UINT64_C(0xffffffffff000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_CONTEXT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (AW, IGN_6_3, RSVD_7, DID, RSVD_63_24));

/** Context Entry. */
typedef struct VTD_CONTEXT_ENTRY_T
{
    /** The qwords in the context entry. */
    uint64_t        au64[2];
} VTD_CONTEXT_ENTRY_T;
/** Pointer to a context entry. */
typedef VTD_CONTEXT_ENTRY_T *PVTD_CONTEXT_ENTRY_T;
/** Pointer to a const context entry. */
typedef VTD_CONTEXT_ENTRY_T const *PCVTD_CONTEXT_ENTRY_T;
/** @} */


/** @name Scalable-mode Context Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_P_SHIFT                       0
#define VTD_BF_0_SM_CONTEXT_ENTRY_P_MASK                        UINT64_C(0x0000000000000001)
/** FPD: Fault Processing Disable. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_FPD_SHIFT                     1
#define VTD_BF_0_SM_CONTEXT_ENTRY_FPD_MASK                      UINT64_C(0x0000000000000002)
/** DTE: Device-TLB Enable. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_DTE_SHIFT                     2
#define VTD_BF_0_SM_CONTEXT_ENTRY_DTE_MASK                      UINT64_C(0x0000000000000004)
/** PASIDE: PASID Enable. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_PASIDE_SHIFT                  3
#define VTD_BF_0_SM_CONTEXT_ENTRY_PASIDE_MASK                   UINT64_C(0x0000000000000008)
/** PRE: Page Request Enable. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_PRE_SHIFT                     4
#define VTD_BF_0_SM_CONTEXT_ENTRY_PRE_MASK                      UINT64_C(0x0000000000000010)
/** R: Reserved (bits 8:5). */
#define VTD_BF_0_SM_CONTEXT_ENTRY_RSVD_8_5_SHIFT                5
#define VTD_BF_0_SM_CONTEXT_ENTRY_RSVD_8_5_MASK                 UINT64_C(0x00000000000001e0)
/** PDTS: PASID Directory Size. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_PDTS_SHIFT                    9
#define VTD_BF_0_SM_CONTEXT_ENTRY_PDTS_MASK                     UINT64_C(0x0000000000000e00)
/** PASIDDIRPTR: PASID Directory Pointer. */
#define VTD_BF_0_SM_CONTEXT_ENTRY_PASIDDIRPTR_SHIFT             12
#define VTD_BF_0_SM_CONTEXT_ENTRY_PASIDDIRPTR_MASK              UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_SM_CONTEXT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, FPD, DTE, PASIDE, PRE, RSVD_8_5, PDTS, PASIDDIRPTR));

/** RID_PASID: Requested Id to PASID assignment. */
#define VTD_BF_1_SM_CONTEXT_ENTRY_RID_PASID_SHIFT               0
#define VTD_BF_1_SM_CONTEXT_ENTRY_RID_PASID_MASK                UINT64_C(0x00000000000fffff)
/** RID_PRIV: Requested Id to PrivilegeModeRequested assignment. */
#define VTD_BF_1_SM_CONTEXT_ENTRY_RID_PRIV_SHIFT                20
#define VTD_BF_1_SM_CONTEXT_ENTRY_RID_PRIV_MASK                 UINT64_C(0x0000000000100000)
/** R: Reserved (bits 63:21). */
#define VTD_BF_1_SM_CONTEXT_ENTRY_RSVD_63_21_SHIFT              21
#define VTD_BF_1_SM_CONTEXT_ENTRY_RSVD_63_21_MASK               UINT64_C(0xffffffffffe00000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_SM_CONTEXT_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (RID_PASID, RID_PRIV, RSVD_63_21));

/** Context Entry. */
typedef struct VTD_SM_CONTEXT_ENTRY_T
{
    /** The qwords in the scalable-mode context entry. */
    uint64_t        au64[4];
} VTD_SM_CONTEXT_ENTRY_T;
/** Pointer to a scalable-mode context entry. */
typedef VTD_SM_CONTEXT_ENTRY_T *PVTD_SM_CONTEXT_ENTRY_T;
/** Pointer to a const scalable-mode context entry. */
typedef VTD_SM_CONTEXT_ENTRY_T const *PCVTD_SM_CONTEXT_ENTRY_T;
/** @} */


/** @name Scalable-mode PASID Directory Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_SM_PASID_DIR_ENTRY_P_SHIFT                       0
#define VTD_BF_SM_PASID_DIR_ENTRY_P_MASK                        UINT64_C(0x0000000000000001)
/** FPD: Fault Processing Disable. */
#define VTD_BF_SM_PASID_DIR_ENTRY_FPD_SHIFT                     1
#define VTD_BF_SM_PASID_DIR_ENTRY_FPD_MASK                      UINT64_C(0x0000000000000002)
/** R: Reserved (bits 11:2). */
#define VTD_BF_SM_PASID_DIR_ENTRY_RSVD_11_2_SHIFT               2
#define VTD_BF_SM_PASID_DIR_ENTRY_RSVD_11_2_MASK                UINT64_C(0x0000000000000ffc)
/** SMPTBLPTR: Scalable Mode PASID Table Pointer. */
#define VTD_BF_SM_PASID_DIR_ENTRY_SMPTBLPTR_SHIFT               12
#define VTD_BF_SM_PASID_DIR_ENTRY_SMPTBLPTR_MASK                UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_SM_PASID_DIR_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, FPD, RSVD_11_2, SMPTBLPTR));

/** Scalable-mode PASID Directory Entry. */
typedef struct VTD_SM_PASID_DIR_ENTRY_T
{
    /** The scalable-mode PASID directory entry. */
    uint64_t        u;
} VTD_SM_PASID_DIR_ENTRY_T;
/** Pointer to a scalable-mode PASID directory entry. */
typedef VTD_SM_PASID_DIR_ENTRY_T *PVTD_SM_PASID_DIR_ENTRY_T;
/** Pointer to a const scalable-mode PASID directory entry. */
typedef VTD_SM_PASID_DIR_ENTRY_T const *PCVTD_SM_PASID_DIR_ENTRY_T;
/** @} */


/** @name Scalable-mode PASID Table Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_P_SHIFT                     0
#define VTD_BF_0_SM_PASID_TBL_ENTRY_P_MASK                      UINT64_C(0x0000000000000001)
/** FPD: Fault Processing Disable. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_FPD_SHIFT                   1
#define VTD_BF_0_SM_PASID_TBL_ENTRY_FPD_MASK                    UINT64_C(0x0000000000000002)
/** AW: Address Width. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_AW_SHIFT                    2
#define VTD_BF_0_SM_PASID_TBL_ENTRY_AW_MASK                     UINT64_C(0x000000000000001c)
/** SLEE: Second-Level Execute Enable. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLEE_SHIFT                  5
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLEE_MASK                   UINT64_C(0x0000000000000020)
/** PGTT: PASID Granular Translation Type. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_PGTT_SHIFT                  6
#define VTD_BF_0_SM_PASID_TBL_ENTRY_PGTT_MASK                   UINT64_C(0x00000000000001c0)
/** SLADE: Second-Level Address/Dirty Enable. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLADE_SHIFT                 9
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLADE_MASK                  UINT64_C(0x0000000000000200)
/** R: Reserved (bits 11:10). */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_RSVD_11_10_SHIFT            10
#define VTD_BF_0_SM_PASID_TBL_ENTRY_RSVD_11_10_MASK             UINT64_C(0x0000000000000c00)
/** SLPTPTR: Second-Level Page Table Pointer. */
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLPTPTR_SHIFT               12
#define VTD_BF_0_SM_PASID_TBL_ENTRY_SLPTPTR_MASK                UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_SM_PASID_TBL_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, FPD, AW, SLEE, PGTT, SLADE, RSVD_11_10, SLPTPTR));

/** DID: Domain Identifer. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_DID_SHIFT                   0
#define VTD_BF_1_SM_PASID_TBL_ENTRY_DID_MASK                    UINT64_C(0x000000000000ffff)
/** R: Reserved (bits 22:16). */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_RSVD_22_16_SHIFT            16
#define VTD_BF_1_SM_PASID_TBL_ENTRY_RSVD_22_16_MASK             UINT64_C(0x00000000007f0000)
/** PWSNP: Page-Walk Snoop. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PWSNP_SHIFT                 23
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PWSNP_MASK                  UINT64_C(0x0000000000800000)
/** PGSNP: Page Snoop. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PGSNP_SHIFT                 24
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PGSNP_MASK                  UINT64_C(0x0000000001000000)
/** CD: Cache Disable. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_CD_SHIFT                    25
#define VTD_BF_1_SM_PASID_TBL_ENTRY_CD_MASK                     UINT64_C(0x0000000002000000)
/** EMTE: Extended Memory Type Enable. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_EMTE_SHIFT                  26
#define VTD_BF_1_SM_PASID_TBL_ENTRY_EMTE_MASK                   UINT64_C(0x0000000004000000)
/** EMT: Extended Memory Type. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_EMT_SHIFT                   27
#define VTD_BF_1_SM_PASID_TBL_ENTRY_EMT_MASK                    UINT64_C(0x0000000038000000)
/** PWT: Page-Level Write Through. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PWT_SHIFT                   30
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PWT_MASK                    UINT64_C(0x0000000040000000)
/** PCD: Page-Level Cache Disable. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PCD_SHIFT                   31
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PCD_MASK                    UINT64_C(0x0000000080000000)
/** PAT: Page Attribute Table. */
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PAT_SHIFT                   32
#define VTD_BF_1_SM_PASID_TBL_ENTRY_PAT_MASK                    UINT64_C(0xffffffff00000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_SM_PASID_TBL_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (DID, RSVD_22_16, PWSNP, PGSNP, CD, EMTE, EMT, PWT, PCD, PAT));

/** SRE: Supervisor Request Enable. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_SRE_SHIFT                   0
#define VTD_BF_2_SM_PASID_TBL_ENTRY_SRE_MASK                    UINT64_C(0x0000000000000001)
/** ERE: Execute Request Enable. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_ERE_SHIFT                   1
#define VTD_BF_2_SM_PASID_TBL_ENTRY_ERE_MASK                    UINT64_C(0x0000000000000002)
/** FLPM: First Level Paging Mode. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_FLPM_SHIFT                  2
#define VTD_BF_2_SM_PASID_TBL_ENTRY_FLPM_MASK                   UINT64_C(0x000000000000000c)
/** WPE: Write Protect Enable. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_WPE_SHIFT                   4
#define VTD_BF_2_SM_PASID_TBL_ENTRY_WPE_MASK                    UINT64_C(0x0000000000000010)
/** NXE: No-Execute Enable. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_NXE_SHIFT                   5
#define VTD_BF_2_SM_PASID_TBL_ENTRY_NXE_MASK                    UINT64_C(0x0000000000000020)
/** SMEP: Supervisor Mode Execute Prevent. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_SMPE_SHIFT                  6
#define VTD_BF_2_SM_PASID_TBL_ENTRY_SMPE_MASK                   UINT64_C(0x0000000000000040)
/** EAFE: Extended Accessed Flag Enable. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_EAFE_SHIFT                  7
#define VTD_BF_2_SM_PASID_TBL_ENTRY_EAFE_MASK                   UINT64_C(0x0000000000000080)
/** R: Reserved (bits 11:8). */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_RSVD_11_8_SHIFT             8
#define VTD_BF_2_SM_PASID_TBL_ENTRY_RSVD_11_8_MASK              UINT64_C(0x0000000000000f00)
/** FLPTPTR: First Level Page Table Pointer. */
#define VTD_BF_2_SM_PASID_TBL_ENTRY_FLPTPTR_SHIFT               12
#define VTD_BF_2_SM_PASID_TBL_ENTRY_FLPTPTR_MASK                UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_2_SM_PASID_TBL_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (SRE, ERE, FLPM, WPE, NXE, SMPE, EAFE, RSVD_11_8, FLPTPTR));

/** Scalable-mode PASID Table Entry. */
typedef struct VTD_SM_PASID_TBL_ENTRY_T
{
    /** The qwords in the scalable-mode PASID table entry. */
    uint64_t        au64[8];
} VTD_SM_PASID_TBL_ENTRY_T;
/** Pointer to a scalable-mode PASID table entry. */
typedef VTD_SM_PASID_TBL_ENTRY_T *PVTD_SM_PASID_TBL_ENTRY_T;
/** Pointer to a const scalable-mode PASID table entry. */
typedef VTD_SM_PASID_TBL_ENTRY_T const *PCVTD_SM_PASID_TBL_ENTRY_T;
/** @} */


/** @name First-Level Paging Entry.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_FLP_ENTRY_P_SHIFT                                0
#define VTD_BF_FLP_ENTRY_P_MASK                                 UINT64_C(0x0000000000000001)
/** R/W: Read/Write. */
#define VTD_BF_FLP_ENTRY_RW_SHIFT                               1
#define VTD_BF_FLP_ENTRY_RW_MASK                                UINT64_C(0x0000000000000002)
/** U/S: User/Supervisor. */
#define VTD_BF_FLP_ENTRY_US_SHIFT                               2
#define VTD_BF_FLP_ENTRY_US_MASK                                UINT64_C(0x0000000000000004)
/** PWT: Page-Level Write Through. */
#define VTD_BF_FLP_ENTRY_PWT_SHIFT                              3
#define VTD_BF_FLP_ENTRY_PWT_MASK                               UINT64_C(0x0000000000000008)
/** PC: Page-Level Cache Disable. */
#define VTD_BF_FLP_ENTRY_PCD_SHIFT                              4
#define VTD_BF_FLP_ENTRY_PCD_MASK                               UINT64_C(0x0000000000000010)
/** A: Accessed. */
#define VTD_BF_FLP_ENTRY_A_SHIFT                                5
#define VTD_BF_FLP_ENTRY_A_MASK                                 UINT64_C(0x0000000000000020)
/** IGN: Ignored (bit 6). */
#define VTD_BF_FLP_ENTRY_IGN_6_SHIFT                            6
#define VTD_BF_FLP_ENTRY_IGN_6_MASK                             UINT64_C(0x0000000000000040)
/** R: Reserved (bit 7). */
#define VTD_BF_FLP_ENTRY_RSVD_7_SHIFT                           7
#define VTD_BF_FLP_ENTRY_RSVD_7_MASK                            UINT64_C(0x0000000000000080)
/** IGN: Ignored (bits 9:8). */
#define VTD_BF_FLP_ENTRY_IGN_9_8_SHIFT                          8
#define VTD_BF_FLP_ENTRY_IGN_9_8_MASK                           UINT64_C(0x0000000000000300)
/** EA: Extended Accessed. */
#define VTD_BF_FLP_ENTRY_EA_SHIFT                               10
#define VTD_BF_FLP_ENTRY_EA_MASK                                UINT64_C(0x0000000000000400)
/** IGN: Ignored (bit 11). */
#define VTD_BF_FLP_ENTRY_IGN_11_SHIFT                           11
#define VTD_BF_FLP_ENTRY_IGN_11_MASK                            UINT64_C(0x0000000000000800)
/** ADDR: Address. */
#define VTD_BF_FLP_ENTRY_ADDR_SHIFT                             12
#define VTD_BF_FLP_ENTRY_ADDR_MASK                              UINT64_C(0x000ffffffffff000)
/** IGN: Ignored (bits 62:52). */
#define VTD_BF_FLP_ENTRY_IGN_62_52_SHIFT                        52
#define VTD_BF_FLP_ENTRY_IGN_62_52_MASK                         UINT64_C(0x7ff0000000000000)
/** XD: Execute Disabled. */
#define VTD_BF_FLP_ENTRY_XD_SHIFT                               63
#define VTD_BF_FLP_ENTRY_XD_MASK                                UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_FLP_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (P, RW, US, PWT, PCD, A, IGN_6, RSVD_7, IGN_9_8, EA, IGN_11, ADDR, IGN_62_52, XD));

/** First-Level Paging Entry. */
typedef struct VTD_FLP_ENTRY_T
{
    /** The first-level paging entry. */
    uint64_t        u;
} VTD_FLP_ENTRY_T;
/** Pointer to a first-level paging entry. */
typedef VTD_FLP_ENTRY_T *PVTD_FLP_ENTRY_T;
/** Pointer to a const first-level paging entry. */
typedef VTD_FLP_ENTRY_T const *PCVTD_FLP_ENTRY_T;
/** @} */


/** @name Second-Level Paging Entry.
 * In accordance with the Intel spec.
 * @{ */
/** R: Read. */
#define VTD_BF_SLP_ENTRY_R_SHIFT                                0
#define VTD_BF_SLP_ENTRY_R_MASK                                 UINT64_C(0x0000000000000001)
/** W: Write. */
#define VTD_BF_SLP_ENTRY_W_SHIFT                                1
#define VTD_BF_SLP_ENTRY_W_MASK                                 UINT64_C(0x0000000000000002)
/** X: Execute. */
#define VTD_BF_SLP_ENTRY_X_SHIFT                                2
#define VTD_BF_SLP_ENTRY_X_MASK                                 UINT64_C(0x0000000000000004)
/** IGN: Ignored (bits 6:3). */
#define VTD_BF_SLP_ENTRY_IGN_6_3_SHIFT                          3
#define VTD_BF_SLP_ENTRY_IGN_6_3_MASK                           UINT64_C(0x0000000000000078)
/** R: Reserved (bit 7). */
#define VTD_BF_SLP_ENTRY_RSVD_7_SHIFT                           7
#define VTD_BF_SLP_ENTRY_RSVD_7_MASK                            UINT64_C(0x0000000000000080)
/** A: Accessed. */
#define VTD_BF_SLP_ENTRY_A_SHIFT                                8
#define VTD_BF_SLP_ENTRY_A_MASK                                 UINT64_C(0x0000000000000100)
/** IGN: Ignored (bits 10:9). */
#define VTD_BF_SLP_ENTRY_IGN_10_9_SHIFT                         9
#define VTD_BF_SLP_ENTRY_IGN_10_9_MASK                          UINT64_C(0x0000000000000600)
/** R: Reserved (bit 11). */
#define VTD_BF_SLP_ENTRY_RSVD_11_SHIFT                          11
#define VTD_BF_SLP_ENTRY_RSVD_11_MASK                           UINT64_C(0x0000000000000800)
/** ADDR: Address. */
#define VTD_BF_SLP_ENTRY_ADDR_SHIFT                             12
#define VTD_BF_SLP_ENTRY_ADDR_MASK                              UINT64_C(0x000ffffffffff000)
/** IGN: Ignored (bits 61:52). */
#define VTD_BF_SLP_ENTRY_IGN_61_52_SHIFT                        52
#define VTD_BF_SLP_ENTRY_IGN_61_52_MASK                         UINT64_C(0x3ff0000000000000)
/** R: Reserved (bit 62). */
#define VTD_BF_SLP_ENTRY_RSVD_62_SHIFT                          62
#define VTD_BF_SLP_ENTRY_RSVD_62_MASK                           UINT64_C(0x4000000000000000)
/** IGN: Ignored (bit 63). */
#define VTD_BF_SLP_ENTRY_IGN_63_SHIFT                           63
#define VTD_BF_SLP_ENTRY_IGN_63_MASK                            UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_SLP_ENTRY_, UINT64_C(0), UINT64_MAX,
                            (R, W, X, IGN_6_3, RSVD_7, A, IGN_10_9, RSVD_11, ADDR, IGN_61_52, RSVD_62, IGN_63));

/** Second-Level Paging Entry. */
typedef struct VTD_SLP_ENTRY_T
{
    /** The second-level paging entry. */
    uint64_t        u;
} VTD_SLP_ENTRY_T;
/** Pointer to a second-level paging entry. */
typedef VTD_SLP_ENTRY_T *PVTD_SLP_ENTRY_T;
/** Pointer to a const second-level paging entry. */
typedef VTD_SLP_ENTRY_T const *PCVTD_SLP_ENTRY_T;
/** @} */


/** @name Fault Record.
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 11:0). */
#define VTD_BF_0_FAULT_RECORD_RSVD_11_0_SHIFT                   0
#define VTD_BF_0_FAULT_RECORD_RSVD_11_0_MASK                    UINT64_C(0x0000000000000fff)
/** FI: Fault Information. */
#define VTD_BF_0_FAULT_RECORD_FI_SHIFT                          12
#define VTD_BF_0_FAULT_RECORD_FI_MASK                           UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_FAULT_RECORD_, UINT64_C(0), UINT64_MAX,
                            (RSVD_11_0, FI));

/** SID: Source identifier. */
#define VTD_BF_1_FAULT_RECORD_SID_SHIFT                         0
#define VTD_BF_1_FAULT_RECORD_SID_MASK                          UINT64_C(0x000000000000ffff)
/** R: Reserved (bits 28:16). */
#define VTD_BF_1_FAULT_RECORD_RSVD_28_16_SHIFT                  16
#define VTD_BF_1_FAULT_RECORD_RSVD_28_16_MASK                   UINT64_C(0x000000001fff0000)
/** PRIV: Privilege Mode Requested. */
#define VTD_BF_1_FAULT_RECORD_PRIV_SHIFT                        29
#define VTD_BF_1_FAULT_RECORD_PRIV_MASK                         UINT64_C(0x0000000020000000)
/** EXE: Execute Permission Requested. */
#define VTD_BF_1_FAULT_RECORD_EXE_SHIFT                         30
#define VTD_BF_1_FAULT_RECORD_EXE_MASK                          UINT64_C(0x0000000040000000)
/** PP: PASID Present. */
#define VTD_BF_1_FAULT_RECORD_PP_SHIFT                          31
#define VTD_BF_1_FAULT_RECORD_PP_MASK                           UINT64_C(0x0000000080000000)
/** FR: Fault Reason. */
#define VTD_BF_1_FAULT_RECORD_FR_SHIFT                          32
#define VTD_BF_1_FAULT_RECORD_FR_MASK                           UINT64_C(0x000000ff00000000)
/** PV: PASID Value. */
#define VTD_BF_1_FAULT_RECORD_PV_SHIFT                          40
#define VTD_BF_1_FAULT_RECORD_PV_MASK                           UINT64_C(0x0fffff0000000000)
/** AT: Address Type. */
#define VTD_BF_1_FAULT_RECORD_AT_SHIFT                          60
#define VTD_BF_1_FAULT_RECORD_AT_MASK                           UINT64_C(0x3000000000000000)
/** T: Type. */
#define VTD_BF_1_FAULT_RECORD_T_SHIFT                           62
#define VTD_BF_1_FAULT_RECORD_T_MASK                            UINT64_C(0x4000000000000000)
/** R: Reserved (bit 127). */
#define VTD_BF_1_FAULT_RECORD_RSVD_63_SHIFT                     63
#define VTD_BF_1_FAULT_RECORD_RSVD_63_MASK                      UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_FAULT_RECORD_, UINT64_C(0), UINT64_MAX,
                            (SID, RSVD_28_16, PRIV, EXE, PP, FR, PV, AT, T, RSVD_63));

/** Fault record. */
typedef struct VTD_FAULT_RECORD_T
{
    /** The qwords in the fault record. */
    uint64_t        au64[2];
} VTD_FAULT_RECORD_T;
/** Pointer to a fault record. */
typedef VTD_FAULT_RECORD_T *PVTD_FAULT_RECORD_T;
/** Pointer to a const fault record. */
typedef VTD_FAULT_RECORD_T const *PCVTD_FAULT_RECORD_T;
/** @} */


/** @name Interrupt Remapping Table Entry (IRTE) for Remapped Interrupts.
 * In accordance with the Intel spec.
 * @{ */
/** P: Present. */
#define VTD_BF_0_IRTE_P_SHIFT                                   0
#define VTD_BF_0_IRTE_P_MASK                                    UINT64_C(0x0000000000000001)
/** FPD: Fault Processing Disable. */
#define VTD_BF_0_IRTE_FPD_SHIFT                                 1
#define VTD_BF_0_IRTE_FPD_MASK                                  UINT64_C(0x0000000000000002)
/** DM: Destination Mode (0=physical, 1=logical). */
#define VTD_BF_0_IRTE_DM_SHIFT                                  2
#define VTD_BF_0_IRTE_DM_MASK                                   UINT64_C(0x0000000000000004)
/** RH: Redirection Hint. */
#define VTD_BF_0_IRTE_RH_SHIFT                                  3
#define VTD_BF_0_IRTE_RH_MASK                                   UINT64_C(0x0000000000000008)
/** TM: Trigger Mode. */
#define VTD_BF_0_IRTE_TM_SHIFT                                  4
#define VTD_BF_0_IRTE_TM_MASK                                   UINT64_C(0x0000000000000010)
/** DLM: Delivery Mode. */
#define VTD_BF_0_IRTE_DLM_SHIFT                                 5
#define VTD_BF_0_IRTE_DLM_MASK                                  UINT64_C(0x00000000000000e0)
/** AVL: Available. */
#define VTD_BF_0_IRTE_AVAIL_SHIFT                               8
#define VTD_BF_0_IRTE_AVAIL_MASK                                UINT64_C(0x0000000000000f00)
/** R: Reserved (bits 14:12). */
#define VTD_BF_0_IRTE_RSVD_14_12_SHIFT                          12
#define VTD_BF_0_IRTE_RSVD_14_12_MASK                           UINT64_C(0x0000000000007000)
/** IM: IRTE Mode. */
#define VTD_BF_0_IRTE_IM_SHIFT                                  15
#define VTD_BF_0_IRTE_IM_MASK                                   UINT64_C(0x0000000000008000)
/** V: Vector. */
#define VTD_BF_0_IRTE_V_SHIFT                                   16
#define VTD_BF_0_IRTE_V_MASK                                    UINT64_C(0x0000000000ff0000)
/** R: Reserved (bits 31:24). */
#define VTD_BF_0_IRTE_RSVD_31_24_SHIFT                          24
#define VTD_BF_0_IRTE_RSVD_31_24_MASK                           UINT64_C(0x00000000ff000000)
/** DST: Desination Id. */
#define VTD_BF_0_IRTE_DST_SHIFT                                 32
#define VTD_BF_0_IRTE_DST_MASK                                  UINT64_C(0xffffffff00000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_IRTE_, UINT64_C(0), UINT64_MAX,
                            (P, FPD, DM, RH, TM, DLM, AVAIL, RSVD_14_12, IM, V, RSVD_31_24, DST));

/** SID: Source Identifier. */
#define VTD_BF_1_IRTE_SID_SHIFT                                 0
#define VTD_BF_1_IRTE_SID_MASK                                  UINT64_C(0x000000000000ffff)
/** SQ: Source-Id Qualifier. */
#define VTD_BF_1_IRTE_SQ_SHIFT                                  16
#define VTD_BF_1_IRTE_SQ_MASK                                   UINT64_C(0x0000000000030000)
/** SVT: Source Validation Type. */
#define VTD_BF_1_IRTE_SVT_SHIFT                                 18
#define VTD_BF_1_IRTE_SVT_MASK                                  UINT64_C(0x00000000000c0000)
/** R: Reserved (bits 127:84). */
#define VTD_BF_1_IRTE_RSVD_63_20_SHIFT                          20
#define VTD_BF_1_IRTE_RSVD_63_20_MASK                           UINT64_C(0xfffffffffff00000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_IRTE_, UINT64_C(0), UINT64_MAX,
                            (SID, SQ, SVT, RSVD_63_20));

/** Interrupt Remapping Table Entry (IRTE) for remapped interrupts. */
typedef struct VTD_IRTE_T
{
    /** The qwords in the IRTE. */
    uint64_t        au64[2];
} VTD_IRTE_T;
/** Pointer to an IRTE. */
typedef VTD_IRTE_T *PVTD_IRTE_T;
/** Pointer to a const IRTE. */
typedef VTD_IRTE_T const *PCVTD_IRTE_T;
/** @} */


/** @name Version Register (VER_REG).
 * In accordance with the Intel spec.
 *  @{ */
/** Min: Minor Version Number. */
#define VTD_BF_VER_REG_MIN_SHIFT                                0
#define VTD_BF_VER_REG_MIN_MASK                                 UINT32_C(0x0000000f)
/** Max: Major Version Number. */
#define VTD_BF_VER_REG_MAX_SHIFT                                4
#define VTD_BF_VER_REG_MAX_MASK                                 UINT32_C(0x000000f0)
/** R: Reserved (bits 31:8). */
#define VTD_BF_VER_REG_RSVD_31_8_SHIFT                          8
#define VTD_BF_VER_REG_RSVD_31_8_MASK                           UINT32_C(0xffffff00)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_VER_REG_, UINT32_C(0), UINT32_MAX,
                            (MIN, MAX, RSVD_31_8));
/** RW: Read/write mask. */
#define VTD_VER_REG_RW_MASK                                     UINT32_C(0)
/** @} */


/** @name Capability Register (CAP_REG).
 * In accordance with the Intel spec.
 * @{ */
/** ND: Number of domains supported. */
#define VTD_BF_CAP_REG_ND_SHIFT                                 0
#define VTD_BF_CAP_REG_ND_MASK                                  UINT64_C(0x0000000000000007)
/** AFL: Advanced Fault Logging. */
#define VTD_BF_CAP_REG_AFL_SHIFT                                3
#define VTD_BF_CAP_REG_AFL_MASK                                 UINT64_C(0x0000000000000008)
/** RWBF: Required Write-Buffer Flushing. */
#define VTD_BF_CAP_REG_RWBF_SHIFT                               4
#define VTD_BF_CAP_REG_RWBF_MASK                                UINT64_C(0x0000000000000010)
/** PLMR: Protected Low-Memory Region. */
#define VTD_BF_CAP_REG_PLMR_SHIFT                               5
#define VTD_BF_CAP_REG_PLMR_MASK                                UINT64_C(0x0000000000000020)
/** PHMR: Protected High-Memory Region. */
#define VTD_BF_CAP_REG_PHMR_SHIFT                               6
#define VTD_BF_CAP_REG_PHMR_MASK                                UINT64_C(0x0000000000000040)
/** CM: Caching Mode. */
#define VTD_BF_CAP_REG_CM_SHIFT                                 7
#define VTD_BF_CAP_REG_CM_MASK                                  UINT64_C(0x0000000000000080)
/** SAGAW: Supported Adjusted Guest Address Widths. */
#define VTD_BF_CAP_REG_SAGAW_SHIFT                              8
#define VTD_BF_CAP_REG_SAGAW_MASK                               UINT64_C(0x0000000000001f00)
/** R: Reserved (bits 15:13). */
#define VTD_BF_CAP_REG_RSVD_15_13_SHIFT                         13
#define VTD_BF_CAP_REG_RSVD_15_13_MASK                          UINT64_C(0x000000000000e000)
/** MGAW: Maximum Guest Address Width. */
#define VTD_BF_CAP_REG_MGAW_SHIFT                               16
#define VTD_BF_CAP_REG_MGAW_MASK                                UINT64_C(0x00000000003f0000)
/** ZLR: Zero Length Read. */
#define VTD_BF_CAP_REG_ZLR_SHIFT                                22
#define VTD_BF_CAP_REG_ZLR_MASK                                 UINT64_C(0x0000000000400000)
/** DEP: Deprecated MBZ. Reserved (bit 23). */
#define VTD_BF_CAP_REG_RSVD_23_SHIFT                            23
#define VTD_BF_CAP_REG_RSVD_23_MASK                             UINT64_C(0x0000000000800000)
/** FRO: Fault-recording Register Offset. */
#define VTD_BF_CAP_REG_FRO_SHIFT                                24
#define VTD_BF_CAP_REG_FRO_MASK                                 UINT64_C(0x00000003ff000000)
/** SLLPS: Second Level Large Page Support. */
#define VTD_BF_CAP_REG_SLLPS_SHIFT                              34
#define VTD_BF_CAP_REG_SLLPS_MASK                               UINT64_C(0x0000003c00000000)
/** R: Reserved (bit 38). */
#define VTD_BF_CAP_REG_RSVD_38_SHIFT                            38
#define VTD_BF_CAP_REG_RSVD_38_MASK                             UINT64_C(0x0000004000000000)
/** PSI: Page Selective Invalidation. */
#define VTD_BF_CAP_REG_PSI_SHIFT                                39
#define VTD_BF_CAP_REG_PSI_MASK                                 UINT64_C(0x0000008000000000)
/** NFR: Number of Fault-recording Registers. */
#define VTD_BF_CAP_REG_NFR_SHIFT                                40
#define VTD_BF_CAP_REG_NFR_MASK                                 UINT64_C(0x0000ff0000000000)
/** MAMV: Maximum Address Mask Value. */
#define VTD_BF_CAP_REG_MAMV_SHIFT                               48
#define VTD_BF_CAP_REG_MAMV_MASK                                UINT64_C(0x003f000000000000)
/** DWD: Write Draining. */
#define VTD_BF_CAP_REG_DWD_SHIFT                                54
#define VTD_BF_CAP_REG_DWD_MASK                                 UINT64_C(0x0040000000000000)
/** DRD: Read Draining. */
#define VTD_BF_CAP_REG_DRD_SHIFT                                55
#define VTD_BF_CAP_REG_DRD_MASK                                 UINT64_C(0x0080000000000000)
/** FL1GP: First Level 1 GB Page Support. */
#define VTD_BF_CAP_REG_FL1GP_SHIFT                              56
#define VTD_BF_CAP_REG_FL1GP_MASK                               UINT64_C(0x0100000000000000)
/** R: Reserved (bits 58:57). */
#define VTD_BF_CAP_REG_RSVD_58_57_SHIFT                         57
#define VTD_BF_CAP_REG_RSVD_58_57_MASK                          UINT64_C(0x0600000000000000)
/** PI: Posted Interrupt Support. */
#define VTD_BF_CAP_REG_PI_SHIFT                                 59
#define VTD_BF_CAP_REG_PI_MASK                                  UINT64_C(0x0800000000000000)
/** FL5LP: First Level 5-level Paging Support. */
#define VTD_BF_CAP_REG_FL5LP_SHIFT                              60
#define VTD_BF_CAP_REG_FL5LP_MASK                               UINT64_C(0x1000000000000000)
/** R: Reserved (bits 63:61). */
#define VTD_BF_CAP_REG_RSVD_63_61_SHIFT                         61
#define VTD_BF_CAP_REG_RSVD_63_61_MASK                          UINT64_C(0xe000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_CAP_REG_, UINT64_C(0), UINT64_MAX,
                            (ND, AFL, RWBF, PLMR, PHMR, CM, SAGAW, RSVD_15_13, MGAW, ZLR, RSVD_23, FRO, SLLPS, RSVD_38, PSI, NFR,
                             MAMV, DWD, DRD, FL1GP, RSVD_58_57, PI, FL5LP, RSVD_63_61));

/** RW: Read/write mask. */
#define VTD_CAP_REG_RW_MASK                                     UINT64_C(0)
/** @} */


/** @name Extended Capability Register (ECAP_REG).
 * In accordance with the Intel spec.
 * @{ */
/** C: Page-walk Coherence. */
#define VTD_BF_ECAP_REG_C_SHIFT                                 0
#define VTD_BF_ECAP_REG_C_MASK                                  UINT64_C(0x0000000000000001)
/** QI: Queued Invalidation Support. */
#define VTD_BF_ECAP_REG_QI_SHIFT                                1
#define VTD_BF_ECAP_REG_QI_MASK                                 UINT64_C(0x0000000000000002)
/** DT: Device-TLB Support. */
#define VTD_BF_ECAP_REG_DT_SHIFT                                2
#define VTD_BF_ECAP_REG_DT_MASK                                 UINT64_C(0x0000000000000004)
/** IR: Interrupt Remapping Support. */
#define VTD_BF_ECAP_REG_IR_SHIFT                                3
#define VTD_BF_ECAP_REG_IR_MASK                                 UINT64_C(0x0000000000000008)
/** EIM: Extended Interrupt Mode. */
#define VTD_BF_ECAP_REG_EIM_SHIFT                               4
#define VTD_BF_ECAP_REG_EIM_MASK                                UINT64_C(0x0000000000000010)
/** DEP: Deprecated MBZ. Reserved (bit 5). */
#define VTD_BF_ECAP_REG_RSVD_5_SHIFT                            5
#define VTD_BF_ECAP_REG_RSVD_5_MASK                             UINT64_C(0x0000000000000020)
/** PT: Pass Through. */
#define VTD_BF_ECAP_REG_PT_SHIFT                                6
#define VTD_BF_ECAP_REG_PT_MASK                                 UINT64_C(0x0000000000000040)
/** SC: Snoop Control. */
#define VTD_BF_ECAP_REG_SC_SHIFT                                7
#define VTD_BF_ECAP_REG_SC_MASK                                 UINT64_C(0x0000000000000080)
/** IRO: IOTLB Register Offset. */
#define VTD_BF_ECAP_REG_IRO_SHIFT                               8
#define VTD_BF_ECAP_REG_IRO_MASK                                UINT64_C(0x000000000003ff00)
/** R: Reserved (bits 19:18). */
#define VTD_BF_ECAP_REG_RSVD_19_18_SHIFT                        18
#define VTD_BF_ECAP_REG_RSVD_19_18_MASK                         UINT64_C(0x00000000000c0000)
/** MHMV: Maximum Handle Mask Value. */
#define VTD_BF_ECAP_REG_MHMV_SHIFT                              20
#define VTD_BF_ECAP_REG_MHMV_MASK                               UINT64_C(0x0000000000f00000)
/** DEP: Deprecated MBZ. Reserved (bit 24). */
#define VTD_BF_ECAP_REG_RSVD_24_SHIFT                           24
#define VTD_BF_ECAP_REG_RSVD_24_MASK                            UINT64_C(0x0000000001000000)
/** MTS: Memory Type Support. */
#define VTD_BF_ECAP_REG_MTS_SHIFT                               25
#define VTD_BF_ECAP_REG_MTS_MASK                                UINT64_C(0x0000000002000000)
/** NEST: Nested Translation Support. */
#define VTD_BF_ECAP_REG_NEST_SHIFT                              26
#define VTD_BF_ECAP_REG_NEST_MASK                               UINT64_C(0x0000000004000000)
/** R: Reserved (bit 27). */
#define VTD_BF_ECAP_REG_RSVD_27_SHIFT                           27
#define VTD_BF_ECAP_REG_RSVD_27_MASK                            UINT64_C(0x0000000008000000)
/** DEP: Deprecated MBZ. Reserved (bit 28). */
#define VTD_BF_ECAP_REG_RSVD_28_SHIFT                           28
#define VTD_BF_ECAP_REG_RSVD_28_MASK                            UINT64_C(0x0000000010000000)
/** PRS: Page Request Support. */
#define VTD_BF_ECAP_REG_PRS_SHIFT                               29
#define VTD_BF_ECAP_REG_PRS_MASK                                UINT64_C(0x0000000020000000)
/** ERS: Execute Request Support. */
#define VTD_BF_ECAP_REG_ERS_SHIFT                               30
#define VTD_BF_ECAP_REG_ERS_MASK                                UINT64_C(0x0000000040000000)
/** SRS: Supervisor Request Support. */
#define VTD_BF_ECAP_REG_SRS_SHIFT                               31
#define VTD_BF_ECAP_REG_SRS_MASK                                UINT64_C(0x0000000080000000)
/** R: Reserved (bit 32). */
#define VTD_BF_ECAP_REG_RSVD_32_SHIFT                           32
#define VTD_BF_ECAP_REG_RSVD_32_MASK                            UINT64_C(0x0000000100000000)
/** NWFS: No Write Flag Support. */
#define VTD_BF_ECAP_REG_NWFS_SHIFT                              33
#define VTD_BF_ECAP_REG_NWFS_MASK                               UINT64_C(0x0000000200000000)
/** EAFS: Extended Accessed Flags Support. */
#define VTD_BF_ECAP_REG_EAFS_SHIFT                              34
#define VTD_BF_ECAP_REG_EAFS_MASK                               UINT64_C(0x0000000400000000)
/** PSS: PASID Size Supported. */
#define VTD_BF_ECAP_REG_PSS_SHIFT                               35
#define VTD_BF_ECAP_REG_PSS_MASK                                UINT64_C(0x000000f800000000)
/** PASID: Process Address Space ID Support. */
#define VTD_BF_ECAP_REG_PASID_SHIFT                             40
#define VTD_BF_ECAP_REG_PASID_MASK                              UINT64_C(0x0000010000000000)
/** DIT: Device-TLB Invalidation Throttle. */
#define VTD_BF_ECAP_REG_DIT_SHIFT                               41
#define VTD_BF_ECAP_REG_DIT_MASK                                UINT64_C(0x0000020000000000)
/** PDS: Page-request Drain Support. */
#define VTD_BF_ECAP_REG_PDS_SHIFT                               42
#define VTD_BF_ECAP_REG_PDS_MASK                                UINT64_C(0x0000040000000000)
/** SMTS: Scalable-Mode Translation Support. */
#define VTD_BF_ECAP_REG_SMTS_SHIFT                              43
#define VTD_BF_ECAP_REG_SMTS_MASK                               UINT64_C(0x0000080000000000)
/** VCS: Virtual Command Support. */
#define VTD_BF_ECAP_REG_VCS_SHIFT                               44
#define VTD_BF_ECAP_REG_VCS_MASK                                UINT64_C(0x0000100000000000)
/** SLADS: Second-Level Accessed/Dirty Support. */
#define VTD_BF_ECAP_REG_SLADS_SHIFT                             45
#define VTD_BF_ECAP_REG_SLADS_MASK                              UINT64_C(0x0000200000000000)
/** SLTS: Second-Level Translation Support. */
#define VTD_BF_ECAP_REG_SLTS_SHIFT                              46
#define VTD_BF_ECAP_REG_SLTS_MASK                               UINT64_C(0x0000400000000000)
/** FLTS: First-Level Translation Support. */
#define VTD_BF_ECAP_REG_FLTS_SHIFT                              47
#define VTD_BF_ECAP_REG_FLTS_MASK                               UINT64_C(0x0000800000000000)
/** SMPWCS: Scalable-Mode Page-Walk Coherency Support. */
#define VTD_BF_ECAP_REG_SMPWCS_SHIFT                            48
#define VTD_BF_ECAP_REG_SMPWCS_MASK                             UINT64_C(0x0001000000000000)
/** RPS: RID-PASID Support. */
#define VTD_BF_ECAP_REG_RPS_SHIFT                               49
#define VTD_BF_ECAP_REG_RPS_MASK                                UINT64_C(0x0002000000000000)
/** R: Reserved (bits 63:50). */
#define VTD_BF_ECAP_REG_RSVD_63_50_SHIFT                        50
#define VTD_BF_ECAP_REG_RSVD_63_50_MASK                         UINT64_C(0xfffc000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_ECAP_REG_, UINT64_C(0), UINT64_MAX,
                            (C, QI, DT, IR, EIM, RSVD_5, PT, SC, IRO, RSVD_19_18, MHMV, RSVD_24, MTS, NEST, RSVD_27, RSVD_28,
                             PRS, ERS, SRS, RSVD_32, NWFS, EAFS, PSS, PASID, DIT, PDS, SMTS, VCS, SLADS, SLTS, FLTS, SMPWCS, RPS,
                             RSVD_63_50));

/** RW: Read/write mask. */
#define VTD_ECAP_REG_RW_MASK                                    UINT64_C(0)
/** @} */


/** @name Global Command Register (GCMD_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 22:0). */
#define VTD_BF_GCMD_REG_RSVD_22_0_SHIFT                         0
#define VTD_BF_GCMD_REG_RSVD_22_0_MASK                          UINT32_C(0x007fffff)
/** CFI: Compatibility Format Interrupt. */
#define VTD_BF_GCMD_REG_CFI_SHIFT                               23
#define VTD_BF_GCMD_REG_CFI_MASK                                UINT32_C(0x00800000)
/** SIRTP: Set Interrupt Table Remap Pointer. */
#define VTD_BF_GCMD_REG_SIRTP_SHIFT                             24
#define VTD_BF_GCMD_REG_SIRTP_MASK                              UINT32_C(0x01000000)
/** IRE: Interrupt Remap Enable. */
#define VTD_BF_GCMD_REG_IRE_SHIFT                               25
#define VTD_BF_GCMD_REG_IRE_MASK                                UINT32_C(0x02000000)
/** QIE: Queued Invalidation Enable. */
#define VTD_BF_GCMD_REG_QIE_SHIFT                               26
#define VTD_BF_GCMD_REG_QIE_MASK                                UINT32_C(0x04000000)
/** WBF: Write Buffer Flush. */
#define VTD_BF_GCMD_REG_WBF_SHIFT                               27
#define VTD_BF_GCMD_REG_WBF_MASK                                UINT32_C(0x08000000)
/** EAFL: Enable Advance Fault Logging. */
#define VTD_BF_GCMD_REG_EAFL_SHIFT                              28
#define VTD_BF_GCMD_REG_EAFL_MASK                               UINT32_C(0x10000000)
/** SFL: Set Fault Log. */
#define VTD_BF_GCMD_REG_SFL_SHIFT                               29
#define VTD_BF_GCMD_REG_SFL_MASK                                UINT32_C(0x20000000)
/** SRTP: Set Root Table Pointer. */
#define VTD_BF_GCMD_REG_SRTP_SHIFT                              30
#define VTD_BF_GCMD_REG_SRTP_MASK                               UINT32_C(0x40000000)
/** TE: Translation Enable. */
#define VTD_BF_GCMD_REG_TE_SHIFT                                31
#define VTD_BF_GCMD_REG_TE_MASK                                 UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_GCMD_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_22_0, CFI, SIRTP, IRE, QIE, WBF, EAFL, SFL, SRTP, TE));

/** RW: Read/write mask. */
#define VTD_GCMD_REG_RW_MASK                                    UINT32_C(0xff800000)
/** @} */


/** @name Global Status Register (GSTS_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 22:0). */
#define VTD_BF_GSTS_REG_RSVD_22_0_SHIFT                         0
#define VTD_BF_GSTS_REG_RSVD_22_0_MASK                          UINT32_C(0x007fffff)
/** CFIS: Compatibility Format Interrupt Status. */
#define VTD_BF_GSTS_REG_CFIS_SHIFT                              23
#define VTD_BF_GSTS_REG_CFIS_MASK                               UINT32_C(0x00800000)
/** IRTPS: Interrupt Remapping Table Pointer Status. */
#define VTD_BF_GSTS_REG_IRTPS_SHIFT                             24
#define VTD_BF_GSTS_REG_IRTPS_MASK                              UINT32_C(0x01000000)
/** IRES: Interrupt Remapping Enable Status. */
#define VTD_BF_GSTS_REG_IRES_SHIFT                              25
#define VTD_BF_GSTS_REG_IRES_MASK                               UINT32_C(0x02000000)
/** QIES: Queued Invalidation Enable Status. */
#define VTD_BF_GSTS_REG_QIES_SHIFT                              26
#define VTD_BF_GSTS_REG_QIES_MASK                               UINT32_C(0x04000000)
/** WBFS: Write Buffer Flush Status. */
#define VTD_BF_GSTS_REG_WBFS_SHIFT                              27
#define VTD_BF_GSTS_REG_WBFS_MASK                               UINT32_C(0x08000000)
/** AFLS: Advanced Fault Logging Status. */
#define VTD_BF_GSTS_REG_AFLS_SHIFT                              28
#define VTD_BF_GSTS_REG_AFLS_MASK                               UINT32_C(0x10000000)
/** FLS: Fault Log Status. */
#define VTD_BF_GSTS_REG_FLS_SHIFT                               29
#define VTD_BF_GSTS_REG_FLS_MASK                                UINT32_C(0x20000000)
/** RTPS: Root Table Pointer Status. */
#define VTD_BF_GSTS_REG_RTPS_SHIFT                              30
#define VTD_BF_GSTS_REG_RTPS_MASK                               UINT32_C(0x40000000)
/** TES: Translation Enable Status. */
#define VTD_BF_GSTS_REG_TES_SHIFT                               31
#define VTD_BF_GSTS_REG_TES_MASK                                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_GSTS_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_22_0, CFIS, IRTPS, IRES, QIES, WBFS, AFLS, FLS, RTPS, TES));

/** RW: Read/write mask. */
#define VTD_GSTS_REG_RW_MASK                                    UINT32_C(0)
/** @} */


/** @name Root Table Address Register (RTADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 9:0). */
#define VTD_BF_RTADDR_REG_RSVD_9_0_SHIFT                        0
#define VTD_BF_RTADDR_REG_RSVD_9_0_MASK                         UINT64_C(0x00000000000003ff)
/** TTM: Translation Table Mode. */
#define VTD_BF_RTADDR_REG_TTM_SHIFT                             10
#define VTD_BF_RTADDR_REG_TTM_MASK                              UINT64_C(0x0000000000000c00)
/** RTA: Root Table Address. */
#define VTD_BF_RTADDR_REG_RTA_SHIFT                             12
#define VTD_BF_RTADDR_REG_RTA_MASK                              UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_RTADDR_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_9_0, TTM, RTA));

/** RW: Read/write mask. */
#define VTD_RTADDR_REG_RW_MASK                                  UINT64_C(0xfffffffffffffc00)
/** @} */


/** @name Context Command Register (CCMD_REG).
 * In accordance with the Intel spec.
 * @{ */
/** DID: Domain-ID. */
#define VTD_BF_CCMD_REG_DID_SHIFT                               0
#define VTD_BF_CCMD_REG_DID_MASK                                UINT64_C(0x000000000000ffff)
/** SID: Source-ID. */
#define VTD_BF_CCMD_REG_SID_SHIFT                               16
#define VTD_BF_CCMD_REG_SID_MASK                                UINT64_C(0x00000000ffff0000)
/** FM: Function Mask. */
#define VTD_BF_CCMD_REG_FM_SHIFT                                32
#define VTD_BF_CCMD_REG_FM_MASK                                 UINT64_C(0x0000000300000000)
/** R: Reserved (bits 58:34). */
#define VTD_BF_CCMD_REG_RSVD_58_34_SHIFT                        34
#define VTD_BF_CCMD_REG_RSVD_58_34_MASK                         UINT64_C(0x07fffffc00000000)
/** CAIG: Context Actual Invalidation Granularity. */
#define VTD_BF_CCMD_REG_CAIG_SHIFT                              59
#define VTD_BF_CCMD_REG_CAIG_MASK                               UINT64_C(0x1800000000000000)
/** CIRG: Context Invalidation Request Granularity. */
#define VTD_BF_CCMD_REG_CIRG_SHIFT                              61
#define VTD_BF_CCMD_REG_CIRG_MASK                               UINT64_C(0x6000000000000000)
/** ICC: Invalidation Context Cache. */
#define VTD_BF_CCMD_REG_ICC_SHIFT                               63
#define VTD_BF_CCMD_REG_ICC_MASK                                UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_CCMD_REG_, UINT64_C(0), UINT64_MAX,
                            (DID, SID, FM, RSVD_58_34, CAIG, CIRG, ICC));

/** RW: Read/write mask. */
#define VTD_CCMD_REG_RW_MASK                                    (  VTD_BF_CCMD_REG_DID_MASK | VTD_BF_CCMD_REG_SID_MASK  \
                                                                 | VTD_BF_CCMD_REG_FM_MASK  | VTD_BF_CCMD_REG_CIRG_MASK \
                                                                 | VTD_BF_CCMD_REG_ICC_MASK)
/** @} */


/** @name IOTLB Invalidation Register (IOTLB_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 31:0). */
#define VTD_BF_IOTLB_REG_RSVD_31_0_SHIFT                        0
#define VTD_BF_IOTLB_REG_RSVD_31_0_MASK                         UINT64_C(0x00000000ffffffff)
/** DID: Domain-ID. */
#define VTD_BF_IOTLB_REG_DID_SHIFT                              32
#define VTD_BF_IOTLB_REG_DID_MASK                               UINT64_C(0x0000ffff00000000)
/** DW: Draining Writes. */
#define VTD_BF_IOTLB_REG_DW_SHIFT                               48
#define VTD_BF_IOTLB_REG_DW_MASK                                UINT64_C(0x0001000000000000)
/** DR: Draining Reads. */
#define VTD_BF_IOTLB_REG_DR_SHIFT                               49
#define VTD_BF_IOTLB_REG_DR_MASK                                UINT64_C(0x0002000000000000)
/** R: Reserved (bits 56:50). */
#define VTD_BF_IOTLB_REG_RSVD_56_50_SHIFT                       50
#define VTD_BF_IOTLB_REG_RSVD_56_50_MASK                        UINT64_C(0x01fc000000000000)
/** IAIG: IOTLB Actual Invalidation Granularity. */
#define VTD_BF_IOTLB_REG_IAIG_SHIFT                             57
#define VTD_BF_IOTLB_REG_IAIG_MASK                              UINT64_C(0x0600000000000000)
/** R: Reserved (bit 59). */
#define VTD_BF_IOTLB_REG_RSVD_59_SHIFT                          59
#define VTD_BF_IOTLB_REG_RSVD_59_MASK                           UINT64_C(0x0800000000000000)
/** IIRG: IOTLB Invalidation Request Granularity. */
#define VTD_BF_IOTLB_REG_IIRG_SHIFT                             60
#define VTD_BF_IOTLB_REG_IIRG_MASK                              UINT64_C(0x3000000000000000)
/** R: Reserved (bit 62). */
#define VTD_BF_IOTLB_REG_RSVD_62_SHIFT                          62
#define VTD_BF_IOTLB_REG_RSVD_62_MASK                           UINT64_C(0x4000000000000000)
/** IVT: Invalidate IOTLB. */
#define VTD_BF_IOTLB_REG_IVT_SHIFT                              63
#define VTD_BF_IOTLB_REG_IVT_MASK                               UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IOTLB_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_31_0, DID, DW, DR, RSVD_56_50, IAIG, RSVD_59, IIRG, RSVD_62, IVT));

/** RW: Read/write mask. */
#define VTD_IOTLB_REG_RW_MASK                                   (  VTD_BF_IOTLB_REG_DID_MASK | VTD_BF_IOTLB_REG_DW_MASK \
                                                                 | VTD_BF_IOTLB_REG_DR_MASK | VTD_BF_IOTLB_REG_IIRG_MASK \
                                                                 | VTD_BF_IOTLB_REG_IVT_MASK)
/** @} */


/** @name Invalidate Address Register (IVA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** AM: Address Mask. */
#define VTD_BF_IVA_REG_AM_SHIFT                                 0
#define VTD_BF_IVA_REG_AM_MASK                                  UINT64_C(0x000000000000003f)
/** IH: Invalidation Hint. */
#define VTD_BF_IVA_REG_IH_SHIFT                                 6
#define VTD_BF_IVA_REG_IH_MASK                                  UINT64_C(0x0000000000000040)
/** R: Reserved (bits 11:7). */
#define VTD_BF_IVA_REG_RSVD_11_7_SHIFT                          7
#define VTD_BF_IVA_REG_RSVD_11_7_MASK                           UINT64_C(0x0000000000000f80)
/** ADDR: Address. */
#define VTD_BF_IVA_REG_ADDR_SHIFT                               12
#define VTD_BF_IVA_REG_ADDR_MASK                                UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IVA_REG_, UINT64_C(0), UINT64_MAX,
                            (AM, IH, RSVD_11_7, ADDR));

/** RW: Read/write mask. */
#define VTD_IVA_REG_RW_MASK                                     (  VTD_BF_IVA_REG_AM_MASK | VTD_BF_IVA_REG_IH_MASK \
                                                                 | VTD_BF_IVA_REG_ADDR_MASK)
/** @} */


/** @name Fault Status Register (FSTS_REG).
 * In accordance with the Intel spec.
 * @{ */
/** PFO: Primary Fault Overflow. */
#define VTD_BF_FSTS_REG_PFO_SHIFT                               0
#define VTD_BF_FSTS_REG_PFO_MASK                                UINT32_C(0x00000001)
/** PPF: Primary Pending Fault. */
#define VTD_BF_FSTS_REG_PPF_SHIFT                               1
#define VTD_BF_FSTS_REG_PPF_MASK                                UINT32_C(0x00000002)
/** AFO: Advanced Fault Overflow. */
#define VTD_BF_FSTS_REG_AFO_SHIFT                               2
#define VTD_BF_FSTS_REG_AFO_MASK                                UINT32_C(0x00000004)
/** APF: Advanced Pending Fault. */
#define VTD_BF_FSTS_REG_APF_SHIFT                               3
#define VTD_BF_FSTS_REG_APF_MASK                                UINT32_C(0x00000008)
/** IQE: Invalidation Queue Error. */
#define VTD_BF_FSTS_REG_IQE_SHIFT                               4
#define VTD_BF_FSTS_REG_IQE_MASK                                UINT32_C(0x00000010)
/** ICE: Invalidation Completion Error. */
#define VTD_BF_FSTS_REG_ICE_SHIFT                               5
#define VTD_BF_FSTS_REG_ICE_MASK                                UINT32_C(0x00000020)
/** ITE: Invalidation Timeout Error. */
#define VTD_BF_FSTS_REG_ITE_SHIFT                               6
#define VTD_BF_FSTS_REG_ITE_MASK                                UINT32_C(0x00000040)
/** DEP: Deprecated MBZ. Reserved (bit 7). */
#define VTD_BF_FSTS_REG_RSVD_7_SHIFT                            7
#define VTD_BF_FSTS_REG_RSVD_7_MASK                             UINT32_C(0x00000080)
/** FRI: Fault Record Index. */
#define VTD_BF_FSTS_REG_FRI_SHIFT                               8
#define VTD_BF_FSTS_REG_FRI_MASK                                UINT32_C(0x0000ff00)
/** R: Reserved (bits 31:16). */
#define VTD_BF_FSTS_REG_RSVD_31_16_SHIFT                        16
#define VTD_BF_FSTS_REG_RSVD_31_16_MASK                         UINT32_C(0xffff0000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_FSTS_REG_, UINT32_C(0), UINT32_MAX,
                            (PFO, PPF, AFO, APF, IQE, ICE, ITE, RSVD_7, FRI, RSVD_31_16));

/** RW: Read/write mask. */
#define VTD_FSTS_REG_RW_MASK                                    (  VTD_BF_FSTS_REG_PFO_MASK | VTD_BF_FSTS_REG_AFO_MASK \
                                                                 | VTD_BF_FSTS_REG_APF_MASK | VTD_BF_FSTS_REG_IQE_MASK \
                                                                 | VTD_BF_FSTS_REG_ICE_MASK | VTD_BF_FSTS_REG_ITE_MASK)
/** RW1C: Read-only-status, Write-1-to-clear status mask. */
#define VTD_FSTS_REG_RW1C_MASK                                  (  VTD_BF_FSTS_REG_PFO_MASK | VTD_BF_FSTS_REG_AFO_MASK \
                                                                 | VTD_BF_FSTS_REG_APF_MASK | VTD_BF_FSTS_REG_IQE_MASK \
                                                                 | VTD_BF_FSTS_REG_ICE_MASK | VTD_BF_FSTS_REG_ITE_MASK)
/** @} */


/** @name Fault Event Control Register (FECTL_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 29:0). */
#define VTD_BF_FECTL_REG_RSVD_29_0_SHIFT                        0
#define VTD_BF_FECTL_REG_RSVD_29_0_MASK                         UINT32_C(0x3fffffff)
/** IP: Interrupt Pending. */
#define VTD_BF_FECTL_REG_IP_SHIFT                               30
#define VTD_BF_FECTL_REG_IP_MASK                                UINT32_C(0x40000000)
/** IM: Interrupt Mask. */
#define VTD_BF_FECTL_REG_IM_SHIFT                               31
#define VTD_BF_FECTL_REG_IM_MASK                                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_FECTL_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_29_0, IP, IM));

/** RW: Read/write mask. */
#define VTD_FECTL_REG_RW_MASK                                   VTD_BF_FECTL_REG_IM_MASK
/** @} */


/** @name Fault Event Data Register (FEDATA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IMD: Interrupt Message Data. */
#define VTD_BF_FEDATA_REG_IMD_SHIFT                             0
#define VTD_BF_FEDATA_REG_IMD_MASK                              UINT32_C(0x0000ffff)
/** R: Reserved (bits 31:16). VT-d specs. prior to 2021 had EIMD here. */
#define VTD_BF_FEDATA_REG_RSVD_31_16_SHIFT                      16
#define VTD_BF_FEDATA_REG_RSVD_31_16_MASK                       UINT32_C(0xffff0000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_FEDATA_REG_, UINT32_C(0), UINT32_MAX,
                            (IMD, RSVD_31_16));

/** RW: Read/write mask. */
#define VTD_FEDATA_REG_RW_MASK                                  VTD_BF_FEDATA_REG_IMD_MASK
/** @} */


/** @name Fault Event Address Register (FEADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 1:0). */
#define VTD_BF_FEADDR_REG_RSVD_1_0_SHIFT                        0
#define VTD_BF_FEADDR_REG_RSVD_1_0_MASK                         UINT32_C(0x00000003)
/** MA: Message Address. */
#define VTD_BF_FEADDR_REG_MA_SHIFT                              2
#define VTD_BF_FEADDR_REG_MA_MASK                               UINT32_C(0xfffffffc)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_FEADDR_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_1_0, MA));

/** RW: Read/write mask. */
#define VTD_FEADDR_REG_RW_MASK                                  VTD_BF_FEADDR_REG_MA_MASK
/** @} */


/** @name Fault Event Upper Address Register (FEUADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** MUA: Message Upper Address. */
#define VTD_BF_FEUADDR_REG_MA_SHIFT                             0
#define VTD_BF_FEUADDR_REG_MA_MASK                              UINT32_C(0xffffffff)

/** RW: Read/write mask. */
#define VTD_FEUADDR_REG_RW_MASK                                 VTD_BF_FEUADDR_REG_MA_MASK
/** @} */


/** @name Fault Recording Register (FRCD_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 11:0). */
#define VTD_BF_0_FRCD_REG_RSVD_11_0_SHIFT                       0
#define VTD_BF_0_FRCD_REG_RSVD_11_0_MASK                        UINT64_C(0x0000000000000fff)
/** FI: Fault Info. */
#define VTD_BF_0_FRCD_REG_FI_SHIFT                              12
#define VTD_BF_0_FRCD_REG_FI_MASK                               UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_FRCD_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_11_0, FI));

/** SID: Source Identifier. */
#define VTD_BF_1_FRCD_REG_SID_SHIFT                             0
#define VTD_BF_1_FRCD_REG_SID_MASK                              UINT64_C(0x000000000000ffff)
/** R: Reserved (bits 27:16). */
#define VTD_BF_1_FRCD_REG_RSVD_27_16_SHIFT                      16
#define VTD_BF_1_FRCD_REG_RSVD_27_16_MASK                       UINT64_C(0x000000000fff0000)
/** T2: Type bit 2. */
#define VTD_BF_1_FRCD_REG_T2_SHIFT                              28
#define VTD_BF_1_FRCD_REG_T2_MASK                               UINT64_C(0x0000000010000000)
/** PRIV: Privilege Mode. */
#define VTD_BF_1_FRCD_REG_PRIV_SHIFT                            29
#define VTD_BF_1_FRCD_REG_PRIV_MASK                             UINT64_C(0x0000000020000000)
/** EXE: Execute Permission Requested. */
#define VTD_BF_1_FRCD_REG_EXE_SHIFT                             30
#define VTD_BF_1_FRCD_REG_EXE_MASK                              UINT64_C(0x0000000040000000)
/** PP: PASID Present. */
#define VTD_BF_1_FRCD_REG_PP_SHIFT                              31
#define VTD_BF_1_FRCD_REG_PP_MASK                               UINT64_C(0x0000000080000000)
/** FR: Fault Reason. */
#define VTD_BF_1_FRCD_REG_FR_SHIFT                              32
#define VTD_BF_1_FRCD_REG_FR_MASK                               UINT64_C(0x000000ff00000000)
/** PV: PASID Value. */
#define VTD_BF_1_FRCD_REG_PV_SHIFT                              40
#define VTD_BF_1_FRCD_REG_PV_MASK                               UINT64_C(0x0fffff0000000000)
/** AT: Address Type. */
#define VTD_BF_1_FRCD_REG_AT_SHIFT                              60
#define VTD_BF_1_FRCD_REG_AT_MASK                               UINT64_C(0x3000000000000000)
/** T1: Type bit 1. */
#define VTD_BF_1_FRCD_REG_T1_SHIFT                              62
#define VTD_BF_1_FRCD_REG_T1_MASK                               UINT64_C(0x4000000000000000)
/** F: Fault. */
#define VTD_BF_1_FRCD_REG_F_SHIFT                               63
#define VTD_BF_1_FRCD_REG_F_MASK                                UINT64_C(0x8000000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_1_FRCD_REG_, UINT64_C(0), UINT64_MAX,
                            (SID, RSVD_27_16, T2, PRIV, EXE, PP, FR, PV, AT, T1, F));

/** RW: Read/write mask. */
#define VTD_FRCD_REG_LO_RW_MASK                                 UINT64_C(0)
#define VTD_FRCD_REG_HI_RW_MASK                                 VTD_BF_1_FRCD_REG_F_MASK
/** RW1C: Read-only-status, Write-1-to-clear status mask. */
#define VTD_FRCD_REG_LO_RW1C_MASK                               UINT64_C(0)
#define VTD_FRCD_REG_HI_RW1C_MASK                               VTD_BF_1_FRCD_REG_F_MASK
/** @} */


/** @name Advanced Fault Log Register (AFLOG_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 8:0). */
#define VTD_BF_0_AFLOG_REG_RSVD_8_0_SHIFT                       0
#define VTD_BF_0_AFLOG_REG_RSVD_8_0_MASK                        UINT64_C(0x00000000000001ff)
/** FLS: Fault Log Size. */
#define VTD_BF_0_AFLOG_REG_FLS_SHIFT                            9
#define VTD_BF_0_AFLOG_REG_FLS_MASK                             UINT64_C(0x0000000000000e00)
/** FLA: Fault Log Address. */
#define VTD_BF_0_AFLOG_REG_FLA_SHIFT                            12
#define VTD_BF_0_AFLOG_REG_FLA_MASK                             UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_0_AFLOG_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_8_0, FLS, FLA));

/** RW: Read/write mask. */
#define VTD_AFLOG_REG_RW_MASK                                   (VTD_BF_0_AFLOG_REG_FLS_MASK | VTD_BF_0_AFLOG_REG_FLA_MASK)
/** @} */


/** @name Protected Memory Enable Register (PMEN_REG).
 * In accordance with the Intel spec.
 * @{ */
/** PRS: Protected Region Status. */
#define VTD_BF_PMEN_REG_PRS_SHIFT                               0
#define VTD_BF_PMEN_REG_PRS_MASK                                UINT32_C(0x00000001)
/** R: Reserved (bits 30:1). */
#define VTD_BF_PMEN_REG_RSVD_30_1_SHIFT                         1
#define VTD_BF_PMEN_REG_RSVD_30_1_MASK                          UINT32_C(0x7ffffffe)
/** EPM: Enable Protected Memory. */
#define VTD_BF_PMEN_REG_EPM_SHIFT                               31
#define VTD_BF_PMEN_REG_EPM_MASK                                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PMEN_REG_, UINT32_C(0), UINT32_MAX,
                            (PRS, RSVD_30_1, EPM));

/** RW: Read/write mask. */
#define VTD_PMEN_REG_RW_MASK                                    VTD_BF_PMEN_REG_EPM_MASK
/** @} */


/** @name Invalidation Queue Head Register (IQH_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 3:0). */
#define VTD_BF_IQH_REG_RSVD_3_0_SHIFT                           0
#define VTD_BF_IQH_REG_RSVD_3_0_MASK                            UINT64_C(0x000000000000000f)
/** QH: Queue Head. */
#define VTD_BF_IQH_REG_QH_SHIFT                                 4
#define VTD_BF_IQH_REG_QH_MASK                                  UINT64_C(0x000000000007fff0)
/** R: Reserved (bits 63:19). */
#define VTD_BF_IQH_REG_RSVD_63_19_SHIFT                         19
#define VTD_BF_IQH_REG_RSVD_63_19_MASK                          UINT64_C(0xfffffffffff80000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IQH_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_3_0, QH, RSVD_63_19));

/** RW: Read/write mask. */
#define VTD_IQH_REG_RW_MASK                                     UINT64_C(0x0)
/** @} */


/** @name Invalidation Queue Tail Register (IQT_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 3:0). */
#define VTD_BF_IQT_REG_RSVD_3_0_SHIFT                           0
#define VTD_BF_IQT_REG_RSVD_3_0_MASK                            UINT64_C(0x000000000000000f)
/** QH: Queue Tail. */
#define VTD_BF_IQT_REG_QT_SHIFT                                 4
#define VTD_BF_IQT_REG_QT_MASK                                  UINT64_C(0x000000000007fff0)
/** R: Reserved (bits 63:19). */
#define VTD_BF_IQT_REG_RSVD_63_19_SHIFT                         19
#define VTD_BF_IQT_REG_RSVD_63_19_MASK                          UINT64_C(0xfffffffffff80000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IQT_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_3_0, QT, RSVD_63_19));

/** RW: Read/write mask. */
#define VTD_IQT_REG_RW_MASK                                     VTD_BF_IQT_REG_QT_MASK

/** IQT_REG.QT: Gets the queue tail. */
#define VTD_IQT_REG_GET_QT(a)                                   ((a) & (VTD_BF_IQT_REG_QT_MASK | VTD_BF_IQT_REG_RSVD_3_0_MASK))
/** @} */


/** @name Invalidation Queue Address Register (IQA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** QS: Queue Size. */
#define VTD_BF_IQA_REG_QS_SHIFT                                 0
#define VTD_BF_IQA_REG_QS_MASK                                  UINT64_C(0x0000000000000007)
/** R: Reserved (bits 10:3). */
#define VTD_BF_IQA_REG_RSVD_10_3_SHIFT                          3
#define VTD_BF_IQA_REG_RSVD_10_3_MASK                           UINT64_C(0x00000000000007f8)
/** DW: Descriptor Width. */
#define VTD_BF_IQA_REG_DW_SHIFT                                 11
#define VTD_BF_IQA_REG_DW_MASK                                  UINT64_C(0x0000000000000800)
/** IQA: Invalidation Queue Base Address. */
#define VTD_BF_IQA_REG_IQA_SHIFT                                12
#define VTD_BF_IQA_REG_IQA_MASK                                 UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IQA_REG_, UINT64_C(0), UINT64_MAX,
                            (QS, RSVD_10_3, DW, IQA));

/** RW: Read/write mask. */
#define VTD_IQA_REG_RW_MASK                                     (  VTD_BF_IQA_REG_QS_MASK | VTD_BF_IQA_REG_DW_MASK \
                                                                 | VTD_BF_IQA_REG_IQA_MASK)
/** DW: 128-bit descriptor. */
#define VTD_IQA_REG_DW_128_BIT                                  0
/** DW: 256-bit descriptor. */
#define VTD_IQA_REG_DW_256_BIT                                  1
/** @} */


/** @name Invalidation Completion Status Register (ICS_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IWC: Invalidation Wait Descriptor Complete. */
#define VTD_BF_ICS_REG_IWC_SHIFT                                0
#define VTD_BF_ICS_REG_IWC_MASK                                 UINT32_C(0x00000001)
/** R: Reserved (bits 31:1). */
#define VTD_BF_ICS_REG_RSVD_31_1_SHIFT                          1
#define VTD_BF_ICS_REG_RSVD_31_1_MASK                           UINT32_C(0xfffffffe)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_ICS_REG_, UINT32_C(0), UINT32_MAX,
                            (IWC, RSVD_31_1));

/** RW: Read/write mask. */
#define VTD_ICS_REG_RW_MASK                                     VTD_BF_ICS_REG_IWC_MASK
/** RW1C: Read-only-status, Write-1-to-clear status mask. */
#define VTD_ICS_REG_RW1C_MASK                                   VTD_BF_ICS_REG_IWC_MASK
/** @} */


/** @name Invalidation Event Control Register (IECTL_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 29:0). */
#define VTD_BF_IECTL_REG_RSVD_29_0_SHIFT                        0
#define VTD_BF_IECTL_REG_RSVD_29_0_MASK                         UINT32_C(0x3fffffff)
/** IP: Interrupt Pending. */
#define VTD_BF_IECTL_REG_IP_SHIFT                               30
#define VTD_BF_IECTL_REG_IP_MASK                                UINT32_C(0x40000000)
/** IM: Interrupt Mask. */
#define VTD_BF_IECTL_REG_IM_SHIFT                               31
#define VTD_BF_IECTL_REG_IM_MASK                                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IECTL_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_29_0, IP, IM));

/** RW: Read/write mask. */
#define VTD_IECTL_REG_RW_MASK                                   VTD_BF_IECTL_REG_IM_MASK
/** @} */


/** @name Invalidation Event Data Register (IEDATA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IMD: Interrupt Message Data. */
#define VTD_BF_IEDATA_REG_IMD_SHIFT                             0
#define VTD_BF_IEDATA_REG_IMD_MASK                              UINT32_C(0x0000ffff)
/** R: Reserved (bits 31:16). VT-d specs. prior to 2021 had EIMD here. */
#define VTD_BF_IEDATA_REG_RSVD_31_16_SHIFT                      16
#define VTD_BF_IEDATA_REG_RSVD_31_16_MASK                       UINT32_C(0xffff0000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IEDATA_REG_, UINT32_C(0), UINT32_MAX,
                            (IMD, RSVD_31_16));

/** RW: Read/write mask. */
#define VTD_IEDATA_REG_RW_MASK                                  VTD_BF_IEDATA_REG_IMD_MASK
/** @} */


/** @name Invalidation Event Address Register (IEADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 1:0). */
#define VTD_BF_IEADDR_REG_RSVD_1_0_SHIFT                        0
#define VTD_BF_IEADDR_REG_RSVD_1_0_MASK                         UINT32_C(0x00000003)
/** MA: Message Address. */
#define VTD_BF_IEADDR_REG_MA_SHIFT                              2
#define VTD_BF_IEADDR_REG_MA_MASK                               UINT32_C(0xfffffffc)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IEADDR_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_1_0, MA));

/** RW: Read/write mask. */
#define VTD_IEADDR_REG_RW_MASK                                  VTD_BF_IEADDR_REG_MA_MASK
/** @} */


/** @name Invalidation Event Upper Address Register (IEUADDR_REG).
 * @{ */
/** MUA: Message Upper Address. */
#define VTD_BF_IEUADDR_REG_MUA_SHIFT                            0
#define VTD_BF_IEUADDR_REG_MUA_MASK                             UINT32_C(0xffffffff)

/** RW: Read/write mask. */
#define VTD_IEUADDR_REG_RW_MASK                                 VTD_BF_IEUADDR_REG_MUA_MASK
/** @} */


/** @name Invalidation Queue Error Record Register (IQERCD_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IQEI: Invalidation Queue Error Info. */
#define VTD_BF_IQERCD_REG_IQEI_SHIFT                            0
#define VTD_BF_IQERCD_REG_IQEI_MASK                             UINT64_C(0x000000000000000f)
/** R: Reserved (bits 31:4). */
#define VTD_BF_IQERCD_REG_RSVD_31_4_SHIFT                       4
#define VTD_BF_IQERCD_REG_RSVD_31_4_MASK                        UINT64_C(0x00000000fffffff0)
/** ITESID: Invalidation Timeout Error Source Identifier. */
#define VTD_BF_IQERCD_REG_ITESID_SHIFT                          32
#define VTD_BF_IQERCD_REG_ITESID_MASK                           UINT64_C(0x0000ffff00000000)
/** ICESID: Invalidation Completion Error Source Identifier. */
#define VTD_BF_IQERCD_REG_ICESID_SHIFT                          48
#define VTD_BF_IQERCD_REG_ICESID_MASK                           UINT64_C(0xffff000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IQERCD_REG_, UINT64_C(0), UINT64_MAX,
                            (IQEI, RSVD_31_4, ITESID, ICESID));

/** RW: Read/write mask. */
#define VTD_IQERCD_REG_RW_MASK                                  UINT64_C(0)
/** @} */


/** @name Interrupt Remapping Table Address Register (IRTA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** S: Size. */
#define VTD_BF_IRTA_REG_S_SHIFT                                 0
#define VTD_BF_IRTA_REG_S_MASK                                  UINT64_C(0x000000000000000f)
/** R: Reserved (bits 10:4). */
#define VTD_BF_IRTA_REG_RSVD_10_4_SHIFT                         4
#define VTD_BF_IRTA_REG_RSVD_10_4_MASK                          UINT64_C(0x00000000000007f0)
/** EIME: Extended Interrupt Mode Enable. */
#define VTD_BF_IRTA_REG_EIME_SHIFT                              11
#define VTD_BF_IRTA_REG_EIME_MASK                               UINT64_C(0x0000000000000800)
/** IRTA: Interrupt Remapping Table Address. */
#define VTD_BF_IRTA_REG_IRTA_SHIFT                              12
#define VTD_BF_IRTA_REG_IRTA_MASK                               UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_IRTA_REG_, UINT64_C(0), UINT64_MAX,
                            (S, RSVD_10_4, EIME, IRTA));

/** RW: Read/write mask. */
#define VTD_IRTA_REG_RW_MASK                                    (  VTD_BF_IRTA_REG_S_MASK | VTD_BF_IRTA_REG_EIME_MASK \
                                                                 | VTD_BF_IRTA_REG_IRTA_MASK)
/** @} */


/** @name Page Request Queue Head Register (PQH_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 4:0). */
#define VTD_BF_PQH_REG_RSVD_4_0_SHIFT                           0
#define VTD_BF_PQH_REG_RSVD_4_0_MASK                            UINT64_C(0x000000000000001f)
/** PQH: Page Queue Head. */
#define VTD_BF_PQH_REG_PQH_SHIFT                                5
#define VTD_BF_PQH_REG_PQH_MASK                                 UINT64_C(0x000000000007ffe0)
/** R: Reserved (bits 63:19). */
#define VTD_BF_PQH_REG_RSVD_63_19_SHIFT                         19
#define VTD_BF_PQH_REG_RSVD_63_19_MASK                          UINT64_C(0xfffffffffff80000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PQH_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_4_0, PQH, RSVD_63_19));

/** RW: Read/write mask. */
#define VTD_PQH_REG_RW_MASK                                     VTD_BF_PQH_REG_PQH_MASK
/** @} */


/** @name Page Request Queue Tail Register (PQT_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 4:0). */
#define VTD_BF_PQT_REG_RSVD_4_0_SHIFT                           0
#define VTD_BF_PQT_REG_RSVD_4_0_MASK                            UINT64_C(0x000000000000001f)
/** PQT: Page Queue Tail. */
#define VTD_BF_PQT_REG_PQT_SHIFT                                5
#define VTD_BF_PQT_REG_PQT_MASK                                 UINT64_C(0x000000000007ffe0)
/** R: Reserved (bits 63:19). */
#define VTD_BF_PQT_REG_RSVD_63_19_SHIFT                         19
#define VTD_BF_PQT_REG_RSVD_63_19_MASK                          UINT64_C(0xfffffffffff80000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PQT_REG_, UINT64_C(0), UINT64_MAX,
                            (RSVD_4_0, PQT, RSVD_63_19));

/** RW: Read/write mask. */
#define VTD_PQT_REG_RW_MASK                                     VTD_BF_PQT_REG_PQT_MASK
/** @} */


/** @name Page Request Queue Address Register (PQA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** PQS: Page Queue Size. */
#define VTD_BF_PQA_REG_PQS_SHIFT                                0
#define VTD_BF_PQA_REG_PQS_MASK                                 UINT64_C(0x0000000000000007)
/** R: Reserved bits (11:3). */
#define VTD_BF_PQA_REG_RSVD_11_3_SHIFT                          3
#define VTD_BF_PQA_REG_RSVD_11_3_MASK                           UINT64_C(0x0000000000000ff8)
/** PQA: Page Request Queue Base Address. */
#define VTD_BF_PQA_REG_PQA_SHIFT                                12
#define VTD_BF_PQA_REG_PQA_MASK                                 UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PQA_REG_, UINT64_C(0), UINT64_MAX,
                            (PQS, RSVD_11_3, PQA));

/** RW: Read/write mask. */
#define VTD_PQA_REG_RW_MASK                                     (VTD_BF_PQA_REG_PQS_MASK | VTD_BF_PQA_REG_PQA_MASK)
/** @} */


/** @name Page Request Status Register (PRS_REG).
 * In accordance with the Intel spec.
 * @{ */
/** PPR: Pending Page Request. */
#define VTD_BF_PRS_REG_PPR_SHIFT                                0
#define VTD_BF_PRS_REG_PPR_MASK                                 UINT64_C(0x00000001)
/** PRO: Page Request Overflow. */
#define VTD_BF_PRS_REG_PRO_SHIFT                                1
#define VTD_BF_PRS_REG_PRO_MASK                                 UINT64_C(0x00000002)
/** R: Reserved (bits 31:2). */
#define VTD_BF_PRS_REG_RSVD_31_2_SHIFT                          2
#define VTD_BF_PRS_REG_RSVD_31_2_MASK                           UINT64_C(0xfffffffc)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PRS_REG_, UINT32_C(0), UINT32_MAX,
                            (PPR, PRO, RSVD_31_2));

/** RW: Read/write mask. */
#define VTD_PRS_REG_RW_MASK                                     (VTD_BF_PRS_REG_PPR_MASK | VTD_BF_PRS_REG_PRO_MASK)
/** RW1C: Read-only-status, Write-1-to-clear status mask. */
#define VTD_PRS_REG_RW1C_MASK                                   (VTD_BF_PRS_REG_PPR_MASK | VTD_BF_PRS_REG_PRO_MASK)
/** @} */


/** @name Page Request Event Control Register (PECTL_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 29:0). */
#define VTD_BF_PECTL_REG_RSVD_29_0_SHIFT                        0
#define VTD_BF_PECTL_REG_RSVD_29_0_MASK                         UINT32_C(0x3fffffff)
/** IP: Interrupt Pending. */
#define VTD_BF_PECTL_REG_IP_SHIFT                               30
#define VTD_BF_PECTL_REG_IP_MASK                                UINT32_C(0x40000000)
/** IM: Interrupt Mask. */
#define VTD_BF_PECTL_REG_IM_SHIFT                               31
#define VTD_BF_PECTL_REG_IM_MASK                                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PECTL_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_29_0, IP, IM));

/** RW: Read/write mask. */
#define VTD_PECTL_REG_RW_MASK                                   VTD_BF_PECTL_REG_IM_MASK
/** @} */


/** @name Page Request Event Data Register (PEDATA_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IMD: Interrupt Message Data. */
#define VTD_BF_PEDATA_REG_IMD_SHIFT                             0
#define VTD_BF_PEDATA_REG_IMD_MASK                              UINT32_C(0x0000ffff)
/** R: Reserved (bits 31:16). VT-d specs. prior to 2021 had EIMD here. */
#define VTD_BF_PEDATA_REG_RSVD_31_16_SHIFT                      16
#define VTD_BF_PEDATA_REG_RSVD_31_16_MASK                       UINT32_C(0xffff0000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PEDATA_REG_, UINT32_C(0), UINT32_MAX,
                            (IMD, RSVD_31_16));

/** RW: Read/write mask. */
#define VTD_PEDATA_REG_RW_MASK                                  VTD_BF_PEDATA_REG_IMD_MASK
/** @} */


/** @name Page Request Event Address Register (PEADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** R: Reserved (bits 1:0). */
#define VTD_BF_PEADDR_REG_RSVD_1_0_SHIFT                        0
#define VTD_BF_PEADDR_REG_RSVD_1_0_MASK                         UINT32_C(0x00000003)
/** MA: Message Address. */
#define VTD_BF_PEADDR_REG_MA_SHIFT                              2
#define VTD_BF_PEADDR_REG_MA_MASK                               UINT32_C(0xfffffffc)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_PEADDR_REG_, UINT32_C(0), UINT32_MAX,
                            (RSVD_1_0, MA));

/** RW: Read/write mask. */
#define VTD_PEADDR_REG_RW_MASK                                  VTD_BF_PEADDR_REG_MA_MASK
/** @} */



/** @name Page Request Event Upper Address Register (PEUADDR_REG).
 * In accordance with the Intel spec.
 * @{ */
/** MA: Message Address. */
#define VTD_BF_PEUADDR_REG_MUA_SHIFT                            0
#define VTD_BF_PEUADDR_REG_MUA_MASK                             UINT32_C(0xffffffff)

/** RW: Read/write mask. */
#define VTD_PEUADDR_REG_RW_MASK                                 VTD_BF_PEUADDR_REG_MUA_MASK
/** @} */


/** @name MTRR Capability Register (MTRRCAP_REG).
 * In accordance with the Intel spec.
 * @{ */
/** VCNT: Variable MTRR Count. */
#define VTD_BF_MTRRCAP_REG_VCNT_SHIFT                           0
#define VTD_BF_MTRRCAP_REG_VCNT_MASK                            UINT64_C(0x00000000000000ff)
/** FIX: Fixed range MTRRs Supported. */
#define VTD_BF_MTRRCAP_REG_FIX_SHIFT                            8
#define VTD_BF_MTRRCAP_REG_FIX_MASK                             UINT64_C(0x0000000000000100)
/** R: Reserved (bit 9). */
#define VTD_BF_MTRRCAP_REG_RSVD_9_SHIFT                         9
#define VTD_BF_MTRRCAP_REG_RSVD_9_MASK                          UINT64_C(0x0000000000000200)
/** WC: Write Combining. */
#define VTD_BF_MTRRCAP_REG_WC_SHIFT                             10
#define VTD_BF_MTRRCAP_REG_WC_MASK                              UINT64_C(0x0000000000000400)
/** R: Reserved (bits 63:11). */
#define VTD_BF_MTRRCAP_REG_RSVD_63_11_SHIFT                     11
#define VTD_BF_MTRRCAP_REG_RSVD_63_11_MASK                      UINT64_C(0xfffffffffffff800)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_MTRRCAP_REG_, UINT64_C(0), UINT64_MAX,
                            (VCNT, FIX, RSVD_9, WC, RSVD_63_11));

/** RW: Read/write mask. */
#define VTD_MTRRCAP_REG_RW_MASK                                 UINT64_C(0)
/** @} */


/** @name MTRR Default Type Register (MTRRDEF_REG).
 * In accordance with the Intel spec.
 * @{ */
/** TYPE: Default Memory Type. */
#define VTD_BF_MTRRDEF_REG_TYPE_SHIFT                           0
#define VTD_BF_MTRRDEF_REG_TYPE_MASK                            UINT64_C(0x00000000000000ff)
/** R: Reserved (bits 9:8). */
#define VTD_BF_MTRRDEF_REG_RSVD_9_8_SHIFT                       8
#define VTD_BF_MTRRDEF_REG_RSVD_9_8_MASK                        UINT64_C(0x0000000000000300)
/** FE: Fixed Range MTRR Enable. */
#define VTD_BF_MTRRDEF_REG_FE_SHIFT                             10
#define VTD_BF_MTRRDEF_REG_FE_MASK                              UINT64_C(0x0000000000000400)
/** E: MTRR Enable. */
#define VTD_BF_MTRRDEF_REG_E_SHIFT                              11
#define VTD_BF_MTRRDEF_REG_E_MASK                               UINT64_C(0x0000000000000800)
/** R: Reserved (bits 63:12). */
#define VTD_BF_MTRRDEF_REG_RSVD_63_12_SHIFT                     12
#define VTD_BF_MTRRDEF_REG_RSVD_63_12_MASK                      UINT64_C(0xfffffffffffff000)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_MTRRDEF_REG_, UINT64_C(0), UINT64_MAX,
                            (TYPE, RSVD_9_8, FE, E, RSVD_63_12));

/** RW: Read/write mask. */
#define VTD_MTRRDEF_REG_RW_MASK                                 (  VTD_BF_MTRRDEF_REG_TYPE_MASK | VTD_BF_MTRRDEF_REG_FE_MASK \
                                                                 | VTD_BF_MTRRDEF_REG_E_MASK)
/** @} */


/** @name Virtual Command Capability Register (VCCAP_REG).
 * In accordance with the Intel spec.
 * @{ */
/** PAS: PASID Support. */
#define VTD_BF_VCCAP_REG_PAS_SHIFT                              0
#define VTD_BF_VCCAP_REG_PAS_MASK                               UINT64_C(0x0000000000000001)
/** R: Reserved (bits 63:1). */
#define VTD_BF_VCCAP_REG_RSVD_63_1_SHIFT                        1
#define VTD_BF_VCCAP_REG_RSVD_63_1_MASK                         UINT64_C(0xfffffffffffffffe)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_VCCAP_REG_, UINT64_C(0), UINT64_MAX,
                            (PAS, RSVD_63_1));

/** RW: Read/write mask. */
#define VTD_VCCAP_REG_RW_MASK                                   UINT64_C(0)
/** @} */


/** @name Virtual Command Extended Operand Register (VCMD_EO_REG).
 * In accordance with the Intel spec.
 * @{ */
/** OB: Operand B. */
#define VTD_BF_VCMD_EO_REG_OB_SHIFT                             0
#define VTD_BF_VCMD_EO_REG_OB_MASK                              UINT32_C(0xffffffffffffffff)

/** RW: Read/write mask. */
#define VTD_VCMD_EO_REG_RW_MASK                                 VTD_BF_VCMD_EO_REG_OB_MASK
/** @} */


/** @name Virtual Command Register (VCMD_REG).
 * In accordance with the Intel spec.
 * @{ */
/** CMD: Command. */
#define VTD_BF_VCMD_REG_CMD_SHIFT                               0
#define VTD_BF_VCMD_REG_CMD_MASK                                UINT64_C(0x00000000000000ff)
/** OP: Operand. */
#define VTD_BF_VCMD_REG_OP_SHIFT                                8
#define VTD_BF_VCMD_REG_OP_MASK                                 UINT64_C(0xffffffffffffff00)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_VCMD_REG_, UINT64_C(0), UINT64_MAX,
                            (CMD, OP));

/** RW: Read/write mask. */
#define VTD_VCMD_REG_RW_MASK                                    (VTD_BF_VCMD_REG_CMD_MASK | VTD_BF_VCMD_REG_OP_MASK)
/** @} */


/** @name Virtual Command Response Register (VCRSP_REG).
 * In accordance with the Intel spec.
 * @{ */
/** IP: In Progress. */
#define VTD_BF_VCRSP_REG_IP_SHIFT                               0
#define VTD_BF_VCRSP_REG_IP_MASK                                UINT64_C(0x0000000000000001)
/** SC: Status Code. */
#define VTD_BF_VCRSP_REG_SC_SHIFT                               1
#define VTD_BF_VCRSP_REG_SC_MASK                                UINT64_C(0x0000000000000006)
/** R: Reserved (bits 7:3). */
#define VTD_BF_VCRSP_REG_RSVD_7_3_SHIFT                         3
#define VTD_BF_VCRSP_REG_RSVD_7_3_MASK                          UINT64_C(0x00000000000000f8)
/** RSLT: Result. */
#define VTD_BF_VCRSP_REG_RSLT_SHIFT                             8
#define VTD_BF_VCRSP_REG_RSLT_MASK                              UINT64_C(0xffffffffffffff00)
RT_BF_ASSERT_COMPILE_CHECKS(VTD_BF_VCRSP_REG_, UINT64_C(0), UINT64_MAX,
                            (IP, SC, RSVD_7_3, RSLT));

/** RW: Read/write mask. */
#define VTD_VCRSP_REG_RW_MASK                                   UINT64_C(0)
/** @} */


/** @name ACPI_DMAR_F_XXX: DMA Remapping Reporting Structure Flags.
 * In accordance with the Intel spec.
 * @{ */
/** INTR_REMAP: Interrupt remapping supported. */
#define ACPI_DMAR_F_INTR_REMAP                                  RT_BIT(0)
/** X2APIC_OPT_OUT: Request system software to opt-out of enabling x2APIC. */
#define ACPI_DMAR_F_X2APIC_OPT_OUT                              RT_BIT(1)
/** DMA_CTRL_PLATFORM_OPT_IN_FLAG: Firmware initiated DMA restricted to reserved
 *  memory regions (RMRR). */
#define ACPI_DMAR_F_DMA_CTRL_PLATFORM_OPT_IN                    RT_BIT(2)
/** @} */


/** @name ACPI_DRHD_F_XXX: DMA-Remapping Hardware Unit Definition Flags.
 * In accordance with the Intel spec.
 * @{ */
/** INCLUDE_PCI_ALL: All PCI devices under scope. */
#define ACPI_DRHD_F_INCLUDE_PCI_ALL                             RT_BIT(0)
/** @} */


/**
 * DRHD: DMA-Remapping Hardware Unit Definition.
 * In accordance with the Intel spec.
 */
#pragma pack(1)
typedef struct ACPIDRHD
{
    /** Type (must be 0=DRHD). */
    uint16_t        uType;
    /** Length (must be 16 + size of device scope structure). */
    uint16_t        cbLength;
    /** Flags, see ACPI_DRHD_F_XXX. */
    uint8_t         fFlags;
    /** Reserved (MBZ). */
    uint8_t         bRsvd;
    /** PCI segment number. */
    uint16_t        uPciSegment;
    /** Register Base Address (MMIO). */
    uint64_t        uRegBaseAddr;
    /* Device Scope[] Structures follow. */
} ACPIDRHD;
#pragma pack()
AssertCompileSize(ACPIDRHD, 16);
AssertCompileMemberOffset(ACPIDRHD, cbLength,     2);
AssertCompileMemberOffset(ACPIDRHD, fFlags,       4);
AssertCompileMemberOffset(ACPIDRHD, uPciSegment,  6);
AssertCompileMemberOffset(ACPIDRHD, uRegBaseAddr, 8);


/** @name ACPIDMARDEVSCOPE_TYPE_XXX: Device Type.
 * In accordance with the Intel spec.
 * @{ */
#define ACPIDMARDEVSCOPE_TYPE_PCI_ENDPOINT                      1
#define ACPIDMARDEVSCOPE_TYPE_PCI_SUB_HIERARCHY                 2
#define ACPIDMARDEVSCOPE_TYPE_IOAPIC                            3
#define ACPIDMARDEVSCOPE_TYPE_MSI_CAP_HPET                      4
#define ACPIDMARDEVSCOPE_TYPE_ACPI_NAMESPACE_DEV                5
/** @} */


/**
 * ACPI Device Scope Structure - PCI device path.
 * In accordance with the Intel spec.
 */
typedef struct ACPIDEVSCOPEPATH
{
    /** PCI device number. */
    uint8_t     uDevice;
    /** PCI function number.   */
    uint8_t     uFunction;
} ACPIDEVSCOPEPATH;
AssertCompileSize(ACPIDEVSCOPEPATH, 2);


/**
 * Device Scope Structure.
 * In accordance with the Intel spec.
 */
#pragma pack(1)
typedef struct ACPIDMARDEVSCOPE
{
    /** Type, see ACPIDMARDEVSCOPE_TYPE_XXX. */
    uint8_t             uType;
    /** Length (must be 6 + size of auPath field).  */
    uint8_t             cbLength;
    /** Reserved (MBZ). */
    uint8_t             abRsvd[2];
    /** Enumeration ID (for I/O APIC, HPET and ACPI namespace devices). */
    uint8_t             idEnum;
    /** First bus number for this device. */
    uint8_t             uStartBusNum;
    /** Hierarchical path from the Host Bridge to the device. */
    ACPIDEVSCOPEPATH    Path;
} ACPIDMARDEVSCOPE;
#pragma pack()
AssertCompileMemberOffset(ACPIDMARDEVSCOPE, cbLength,     1);
AssertCompileMemberOffset(ACPIDMARDEVSCOPE, idEnum,       4);
AssertCompileMemberOffset(ACPIDMARDEVSCOPE, uStartBusNum, 5);
AssertCompileMemberOffset(ACPIDMARDEVSCOPE, Path,         6);

/** ACPI DMAR revision (not the OEM revision field).
 *  In accordance with the Intel spec. */
#define ACPI_DMAR_REVISION                                      1


#endif /* !VBOX_INCLUDED_iommu_intel_h */
