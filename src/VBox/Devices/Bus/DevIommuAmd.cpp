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
#include <VBox/vmm/pdmdev.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/**
 * @name Commands.
 * In accordance with the AMD spec.
 * @{
 */
#define IOMMU_CMD_COMPLETION_WAIT                   0x01
#define IOMMU_CMD_INV_DEV_TAB_ENTRY                 0x02
#define IOMMU_CMD_INV_IOMMU_PAGES                   0x03
#define IOMMU_CMD_INV_IOTLB_PAGES                   0x04
#define IOMMU_CMD_INV_INTR_TABLE                    0x05
#define IOMMU_CMD_PREFETCH_IOMMU_PAGES              0x06
#define IOMMU_CMD_COMPLETE_PPR_REQ                  0x07
#define IOMMU_CMD_INV_IOMMU_ALL                     0x08
/** @} */

/**
 * @name Event codes.
 * In accordance with the AMD spec.
 * @{
 */
#define IOMMU_EVT_ILLEGAL_DEV_TAB_ENTRY             0x01
#define IOMMU_EVT_IO_PAGE_FAULT                     0x02
#define IOMMU_EVT_DEV_TAB_HARDWARE_ERROR            0x03
#define IOMMU_EVT_PAGE_TAB_HARDWARE_ERROR           0x04
#define IOMMU_EVT_ILLEGAL_COMMAND_ERROR             0x05
#define IOMMU_EVT_COMMAND_HARDWARE_ERROR            0x06
#define IOMMU_EVT_IOTLB_INV_TIMEOUT                 0x07
#define IOMMU_EVT_INVALID_DEVICE_REQUEST            0x08
#define IOMMU_EVT_INVALID_PPR_REQUEST               0x09
#define IOMMU_EVT_EVENT_COUNTER_ZERO                0x10
#define IOMMU_EVT_GUEST_EVENT_FAULT                 0x11
/** @} */

/**
 * @name IOMMU Capability Header.
 * In accordance with the AMD spec.
 * @{
 */
/** CapId: Capability ID. */
#define IOMMU_BF_CAPHDR_CAP_ID_SHIFT                0
#define IOMMU_BF_CAPHDR_CAP_ID_MASK                 UINT32_C(0x000000ff)
/** CapPtr: Capability Pointer. */
#define IOMMU_BF_CAPHDR_CAP_PTR_SHIFT               8
#define IOMMU_BF_CAPHDR_CAP_PTR_MASK                UINT32_C(0x0000ff00)
/** CapType: Capability Type. */
#define IOMMU_BF_CAPHDR_CAP_TYPE_SHIFT              16
#define IOMMU_BF_CAPHDR_CAP_TYPE_MASK               UINT32_C(0x00070000)
/** CapRev: Capability Revision. */
#define IOMMU_BF_CAPHDR_CAP_REV_SHIFT               19
#define IOMMU_BF_CAPHDR_CAP_REV_MASK                UINT32_C(0x00f80000)
/** IoTlbSup: IO TLB Support. */
#define IOMMU_BF_CAPHDR_IOTLB_SUP_SHIFT             24
#define IOMMU_BF_CAPHDR_IOTLB_SUP_MASK              UINT32_C(0x01000000)
/** HtTunnel: HyperTransport Tunnel translation support. */
#define IOMMU_BF_CAPHDR_HT_TUNNEL_SHIFT             25
#define IOMMU_BF_CAPHDR_HT_TUNNEL_MASK              UINT32_C(0x02000000)
/** NpCache: Not Present table entries Cached. */
#define IOMMU_BF_CAPHDR_NP_CACHE_SHIFT              26
#define IOMMU_BF_CAPHDR_NP_CACHE_MASK               UINT32_C(0x04000000)
/** EFRSup: Extended Feature Register (EFR) Supported. */
#define IOMMU_BF_CAPHDR_EFR_SUP_SHIFT               27
#define IOMMU_BF_CAPHDR_EFR_SUP_MASK                UINT32_C(0x08000000)
/** CapExt: Miscellaneous Information Register Supported . */
#define IOMMU_BF_CAPHDR_CAP_EXT_SHIFT               28
#define IOMMU_BF_CAPHDR_CAP_EXT_MASK                UINT32_C(0x10000000)
/** Bits 31:29 reserved. */
#define IOMMU_BF_CAPHDR_RSVD_29_31_SHIFT            29
#define IOMMU_BF_CAPHDR_RSVD_29_31_MASK             UINT32_C(0xe0000000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_CAPHDR_, UINT32_C(0), UINT32_MAX,
                            (CAP_ID, CAP_PTR, CAP_TYPE, CAP_REV, IOTLB_SUP, HT_TUNNEL, NP_CACHE, EFR_SUP, CAP_EXT, RSVD_29_31));
/** @} */

/**
 * @name IOMMU Base Address Low Register.
 * In accordance with the AMD spec.
 * @{
 */
/** Enable: Enables access to the address specified in the Base Address Register. */
#define IOMMU_BF_BASEADDR_LO_ENABLE_SHIFT           0
#define IOMMU_BF_BASEADDR_LO_ENABLE_MASK            UINT32_C(0x00000001)
/** Bits 13:1 reserved. */
#define IOMMU_BF_BASEADDR_LO_RSVD_1_13_SHIFT        1
#define IOMMU_BF_BASEADDR_LO_RSVD_1_13_MASK         UINT32_C(0x00003ffe)
/** Base Address[31:14]: Low Base address of IOMMU MMIO control registers. */
#define IOMMU_BF_BASEADDR_LO_ADDR_SHIFT             14
#define IOMMU_BF_BASEADDR_LO_ADDR_MASK              UINT32_C(0xffffc000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_BASEADDR_LO_, UINT32_C(0), UINT32_MAX,
                            (ENABLE, RSVD_1_13, ADDR));
/** @} */

/**
 * @name IOMMU Range Register.
 * In accordance with the AMD spec.
 * @{
 */
/** UnitID: HyperTransport Unit ID. */
#define IOMMU_BF_RANGE_UNIT_ID_SHIFT                0
#define IOMMU_BF_RANGE_UNIT_ID_MASK                 UINT32_C(0x0000001f)
/** Bits 6:5 reserved. */
#define IOMMU_BF_RANGE_RSVD_5_6_SHIFT               5
#define IOMMU_BF_RANGE_RSVD_5_6_MASK                UINT32_C(0x00000060)
/** RngValid: Range valid. */
#define IOMMU_BF_RANGE_VALID_SHIFT                  7
#define IOMMU_BF_RANGE_VALID_MASK                   UINT32_C(0x00000080)
/** BusNumber: Device range bus number. */
#define IOMMU_BF_RANGE_BUS_NUMBER_SHIFT             8
#define IOMMU_BF_RANGE_BUS_NUMBER_MASK              UINT32_C(0x0000ff00)
/** First Device. */
#define IOMMU_BF_RANGE_FIRST_DEVICE_SHIFT           16
#define IOMMU_BF_RANGE_FIRST_DEVICE_MASK            UINT32_C(0x00ff0000)
/** Last Device. */
#define IOMMU_BF_RANGE_LAST_DEVICE_SHIFT            24
#define IOMMU_BF_RANGE_LAST_DEVICE_MASK             UINT32_C(0xff000000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_RANGE_, UINT32_C(0), UINT32_MAX,
                            (UNIT_ID, RSVD_5_6, VALID, BUS_NUMBER, FIRST_DEVICE, LAST_DEVICE));
/** @} */

/**
 * @name IOMMU Miscellaneous Information Register 0.
 * In accordance with the AMD spec.
 * @{
 */
/** MsiNum: MSI message number. */
#define IOMMU_BF_MISCINFO_0_MSI_NUM_SHIFT           0
#define IOMMU_BF_MISCINFO_0_MSI_NUM_MASK            UINT32_C(0x0000001f)
/** GvaSize: Guest Virtual Address Size. */
#define IOMMU_BF_MISCINFO_0_GVA_SIZE_SHIFT          5
#define IOMMU_BF_MISCINFO_0_GVA_SIZE_MASK           UINT32_C(0x000000e0)
/** PaSize: Physical Address Size. */
#define IOMMU_BF_MISCINFO_0_PA_SIZE_SHIFT           8
#define IOMMU_BF_MISCINFO_0_PA_SIZE_MASK            UINT32_C(0x00007f00)
/** VaSize: Virtual Address Size. */
#define IOMMU_BF_MISCINFO_0_VA_SIZE_SHIFT           15
#define IOMMU_BF_MISCINFO_0_VA_SIZE_MASK            UINT32_C(0x003f8000)
/** HtAtsResv: HyperTransport ATS Response Address range Reserved. */
#define IOMMU_BF_MISCINFO_0_HT_ATS_RESV_SHIFT       22
#define IOMMU_BF_MISCINFO_0_HT_ATS_RESV_MASK        UINT32_C(0x00400000)
/** Bits 26:23 reserved. */
#define IOMMU_BF_MISCINFO_0_RSVD_23_26_SHIFT        23
#define IOMMU_BF_MISCINFO_0_RSVD_23_26_MASK         UINT32_C(0x07800000)
/** MsiNumPPR: Peripheral Page Request MSI message number. */
#define IOMMU_BF_MISCINFO_0_MSI_NUM_PPR_SHIFT       27
#define IOMMU_BF_MISCINFO_0_MSI_NUM_PPR_MASK        UINT32_C(0xf8000000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_MISCINFO_0_, UINT32_C(0), UINT32_MAX,
                            (MSI_NUM, GVA_SIZE, PA_SIZE, VA_SIZE, HT_ATS_RESV, RSVD_23_26, MSI_NUM_PPR));
/** @} */

/**
 * @name IOMMU Miscellaneous Information Register 1.
 * In accordance with the AMD spec.
 * @{
 */
/** MsiNumGA: MSI message number for guest virtual-APIC log. */
#define IOMMU_BF_MISCINFO_1_MSI_NUM_GA_SHIFT        0
#define IOMMU_BF_MISCINFO_1_MSI_NUM_GA_MASK         UINT32_C(0x0000001f)
/** Bits 31:5 reserved. */
#define IOMMU_BF_MISCINFO_1_RSVD_5_31_SHIFT         5
#define IOMMU_BF_MISCINFO_1_RSVD_5_31_MASK          UINT32_C(0xffffffe0)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_MISCINFO_1_, UINT32_C(0), UINT32_MAX,
                            (MSI_NUM_GA, RSVD_5_31));
/** @} */

/**
 * @name MSI Capability Header Register.
 * In accordance with the AMD spec.
 * @{
 */
/** MsiCapId: Capability ID. */
#define IOMMU_BF_MSI_CAPHDR_CAP_ID_SHIFT            0
#define IOMMU_BF_MSI_CAPHDR_CAP_ID_MASK             UINT32_C(0x000000ff)
/** MsiCapPtr: Pointer (PCI config offset) to the next capability. */
#define IOMMU_BF_MSI_CAPHDR_CAP_PTR_SHIFT           8
#define IOMMU_BF_MSI_CAPHDR_CAP_PTR_MASK            UINT32_C(0x0000ff00)
/** MsiEn: Message Signal Interrupt enable. */
#define IOMMU_BF_MSI_CAPHDR_EN_SHIFT                16
#define IOMMU_BF_MSI_CAPHDR_EN_MASK                 UINT32_C(0x00010000)
/** MsiMultMessCap: MSI Multi-Message Capability. */
#define IOMMU_BF_MSI_CAPHDR_MULTMESS_CAP_SHIFT      17
#define IOMMU_BF_MSI_CAPHDR_MULTMESS_CAP_MASK       UINT32_C(0x000e0000)
/** MsiMultMessEn: MSI Mult-Message Enable. */
#define IOMMU_BF_MSI_CAPHDR_MULTMESS_EN_SHIFT       20
#define IOMMU_BF_MSI_CAPHDR_MULTMESS_EN_MASK        UINT32_C(0x00700000)
/** Msi64BitEn: MSI 64-bit Enabled. */
#define IOMMU_BF_MSI_CAPHDR_64BIT_EN_SHIFT          23
#define IOMMU_BF_MSI_CAPHDR_64BIT_EN_MASK           UINT32_C(0x00800000)
/** Bits 31:24 reserved. */
#define IOMMU_BF_MSI_CAPHDR_RSVD_24_31_SHIFT        24
#define IOMMU_BF_MSI_CAPHDR_RSVD_24_31_MASK         UINT32_C(0xff000000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_MSI_CAPHDR_, UINT32_C(0), UINT32_MAX,
                            (CAP_ID, CAP_PTR, EN, MULTMESS_CAP, MULTMESS_EN, 64BIT_EN, RSVD_24_31));
/** @} */

/**
 * @name MSI Mapping Capability Header Register.
 * In accordance with the AMD spec.
 * @{
 */
/** MsiMapCapId: Capability ID. */
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_ID_SHIFT        0
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_ID_MASK         UINT32_C(0x000000ff)
/** MsiMapCapPtr: Pointer (PCI config offset) to the next capability. */
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_PTR_SHIFT       8
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_PTR_MASK        UINT32_C(0x0000ff00)
/** MsiMapEn: MSI mapping capability enable. */
#define IOMMU_BF_MSI_MAP_CAPHDR_EN_SHIFT            16
#define IOMMU_BF_MSI_MAP_CAPHDR_EN_MASK             UINT32_C(0x00010000)
/** MsiMapFixd: MSI interrupt mapping range is not programmable. */
#define IOMMU_BF_MSI_MAP_CAPHDR_FIXED_SHIFT         17
#define IOMMU_BF_MSI_MAP_CAPHDR_FIXED_MASK          UINT32_C(0x00020000)
/** Bits 18:28 reserved. */
#define IOMMU_BF_MSI_MAP_CAPHDR_RSVD_18_28_SHIFT    18
#define IOMMU_BF_MSI_MAP_CAPHDR_RSVD_18_28_MASK     UINT32_C(0x07fc0000)
/** MsiMapCapType: MSI mapping capability. */
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_TYPE_SHIFT      27
#define IOMMU_BF_MSI_MAP_CAPHDR_CAP_TYPE_MASK       UINT32_C(0xf8000000)
RT_BF_ASSERT_COMPILE_CHECKS(IOMMU_BF_MSI_MAP_CAPHDR_, UINT32_C(0), UINT32_MAX,
                            (CAP_ID, CAP_PTR, EN, FIXED, RSVD_18_28, CAP_TYPE));
/** @} */

/** @name Miscellaneous IOMMU defines.
 * @{ */
#define IOMMU_LOG_PFX                               "AMD_IOMMU"     /**< Log prefix string. */
#define IOMMU_SAVED_STATE_VERSION                   1               /**< The current saved state version. */
#define IOMMU_PCI_VENDOR_ID                         0x1022          /**< AMD's vendor ID. */
#define IOMMU_PCI_DEVICE_ID                         0xc0de          /**< VirtualBox IOMMU Device ID. */
#define IOMMU_PCI_REVISION_ID                       0x01            /**< VirtualBox IOMMU Device Revision ID. */
#define IOMMU_MMIO_SIZE                             _16K            /**< Size of the MMIO region in bytes. */
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The Device ID.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    uFunction : 3;  /**< Bits 2:0  - Function. */
        uint16_t    uDevice : 5;    /**< Bits 7:3  - Device. */
        uint16_t    uBus : 8;       /**< Bits 15:8 - Bus. */
    } n;
    /** The unsigned integer view. */
    uint16_t        u;
} DEVICE_ID_T;
AssertCompileSize(DEVICE_ID_T, 2);

/**
 * Device Table Entry (DTE).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1Valid : 1;                      /**< Bit  0       - V: Valid. */
        uint32_t    u1TranslationValid : 1;           /**< Bit  1       - TV: Translation information Valid. */
        uint32_t    u5Rsvd0 : 5;                      /**< Bits 6:2     - Reserved. */
        uint32_t    u2Had : 2;                        /**< Bits 8:7     - HAD: Host Access Dirty. */
        uint32_t    u3Mode : 3;                       /**< Bits 11:9    - Mode: Paging mode. */
        uint32_t    u20PageTableRootPtrLo : 20;       /**< Bits 31:12   - Page Table Root Pointer (Lo). */
        uint32_t    u20PageTableRootPtrHi : 20;       /**< Bits 51:32   - Page Table Root Pointer (Hi). */
        uint32_t    u1Ppr : 1;                        /**< Bit  52      - PPR: Peripheral Page Request. */
        uint32_t    u1GstPprRespPasid : 1;            /**< Bit  53      - GRPR: Guest PPR Response with PASID. */
        uint32_t    u1GstIoValid : 1;                 /**< Bit  54      - GIoV: Guest I/O Protection Valid. */
        uint32_t    u1GstTranslateValid : 1;          /**< Bit  55      - GV: Guest translation Valid. */
        uint32_t    u2GstCr3RootTblTranslated : 2;    /**< Bits 57:56   - GLX: Guest Levels Translated. */
        uint32_t    u3GstCr3TableRootPtrLo : 2;       /**< Bits 60:58   - GCR3 TRP: Guest CR3 Table Root Pointer (Lo). */
        uint32_t    u1IoRead : 1;                     /**< Bit  61      - IR: I/O Read permission. */
        uint32_t    u1IoWrite : 1;                    /**< Bit  62      - IW: I/O Write permission. */
        uint32_t    u1Rsvd0 : 1;                      /**< Bit  63      - Reserved. */
        uint32_t    u16DomainId : 1;                  /**< Bits 79:64   - Domain ID. */
        uint32_t    u16GstCr3TableRootPtrMed : 16;    /**< Bits 95:80   - GCR3 TRP: Guest CR3 Table Root Pointer (Mid). */
        uint32_t    u1IoTlbEnable : 1;                /**< Bit  96      - IOTLB Enable. */
        uint32_t    u1SuppressPfEvents : 1;           /**< Bit  97      - SE: Supress Page-fault events. */
        uint32_t    u1SuppressAllPfEvents : 1;        /**< Bit  98      - SA: Supress All Page-fault events. */
        uint32_t    u2IoCtl : 1;                      /**< Bits 100:99  - IoCtl: Port I/O Control. */
        uint32_t    u1Cache : 1;                      /**< Bit  101     - Cache: IOTLB Cache Hint. */
        uint32_t    u1SnoopDisable : 1;               /**< Bit  102     - SD: Snoop Disable. */
        uint32_t    u1AllowExclusion : 1;             /**< Bit  103     - EX: Allow Exclusion. */
        uint32_t    u2SysMgt : 2;                     /**< Bits 105:104 - SysMgt: System Management message enable. */
        uint32_t    u1Rsvd1 : 1;                      /**< Bit  106     - Reserved. */
        uint32_t    u21GstCr3TableRootPtrHi : 21;     /**< Bits 127:107 - GCR3 TRP: Guest CR3 Table Root Pointer (Hi). */
        uint32_t    u1IntrMapValid : 1;               /**< Bit  128     - IV: Interrupt map Valid. */
        uint32_t    u4IntrTableLength : 4;            /**< Bits 132:129 - IntTabLen: Interrupt Table Length. */
        uint32_t    u1IgnoreUnmappedIntrs : 1;        /**< Bits 133     - IG: Ignore unmapped interrupts. */
        uint32_t    u26IntrTableRootPtr : 26;         /**< Bits 159:134 - Interrupt Root Table Pointer (Lo). */
        uint32_t    u20IntrTableRootPtr : 20;         /**< Bits 179:160 - Interrupt Root Table Pointer (Hi). */
        uint32_t    u4Rsvd0 : 4;                      /**< Bits 183:180 - Reserved. */
        uint32_t    u1InitPassthru : 1;               /**< Bits 184     - INIT Pass-through. */
        uint32_t    u1ExtIntPassthru : 1;             /**< Bits 185     - External Interrupt Pass-through. */
        uint32_t    u1NmiPassthru : 1;                /**< Bits 186     - NMI Pass-through. */
        uint32_t    u1Rsvd2 : 1;                      /**< Bits 187     - Reserved. */
        uint32_t    u2IntrCtrl : 2;                   /**< Bits 189:188 - IntCtl: Interrupt Control. */
        uint32_t    u1Lint0Passthru : 1;              /**< Bit  190     - Lint0Pass: LINT0 (Legacy PIC NMI) Pass-thru. */
        uint32_t    u1Lint1Passthru : 1;              /**< Bit  191     - Lint1Pass: LINT1 (Legacy PIC NMI) Pass-thru. */
        uint32_t    u32Rsvd0;                         /**< Bits 223:192 - Reserved. */
        uint32_t    u22Rsvd0 : 22;                    /**< Bits 245:224 - Reserved. */
        uint32_t    u1AttrOverride : 1;               /**< Bit  246     - AttrV: Attribute Override. */
        uint32_t    u1Mode0FC: 1;                     /**< Bit  247     - Mode0FC. */
        uint32_t    u8SnoopAttr: 1;                   /**< Bits 255:248 - Snoop Attribute. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t        au32[8];
} DEV_TAB_ENTRY_T;
AssertCompileSize(DEV_TAB_ENTRY_T, 32);

/**
 * I/O Page Table Entry.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t    u1Present : 1;             /**< Bit  0      - PR: Present. */
        RT_GCC_EXTENSION uint64_t    u4Ign0 : 4;                /**< Bits 4:1    - Ignored. */
        RT_GCC_EXTENSION uint64_t    u1Accessed : 1;            /**< Bit  5      - A: Accessed. */
        RT_GCC_EXTENSION uint64_t    u1Dirty : 1;               /**< Bit  6      - D: Dirty. */
        RT_GCC_EXTENSION uint64_t    u2Ign0 : 2;                /**< Bits 8:7    - Ignored. */
        RT_GCC_EXTENSION uint64_t    u3NextLevel : 3;           /**< Bits 11:9   - Next Level: Next page translation level. */
        RT_GCC_EXTENSION uint64_t    u40PageAddr : 40;          /**< Bits 51:12  - Page address. */
        RT_GCC_EXTENSION uint64_t    u7Rsvd0 : 7;               /**< Bits 58:52  - Reserved. */
        RT_GCC_EXTENSION uint64_t    u1UntranslatedAccess : 1;  /**< Bit 59      - U: Untranslated Access Only. */
        RT_GCC_EXTENSION uint64_t    u1ForceCoherent : 1;       /**< Bit 60      - FC: Force Coherent. */
        RT_GCC_EXTENSION uint64_t    u1IoRead : 1;              /**< Bit 61      - IR: I/O Read permission. */
        RT_GCC_EXTENSION uint64_t    u1IoWrite : 1;             /**< Bit 62      - IW: I/O Wead permission. */
        RT_GCC_EXTENSION uint64_t    u1Ign0 : 1;                /**< Bit 63      - Ignored. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t        u;
} IOPTE_T;
AssertCompileSize(IOPTE_T, 8);

/**
 * I/O Page Directory Entry.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t    u1Present : 1;         /**< Bit  0      - PR: Present. */
        RT_GCC_EXTENSION uint64_t    u4Ign0 : 4;            /**< Bits 4:1    - Ignored. */
        RT_GCC_EXTENSION uint64_t    u1Accessed : 1;        /**< Bit  5      - A: Accessed. */
        RT_GCC_EXTENSION uint64_t    u3Ign0 : 3;            /**< Bits 8:6    - Ignored. */
        RT_GCC_EXTENSION uint64_t    u3NextLevel : 3;       /**< Bits 11:9   - Next Level: Next page translation level. */
        RT_GCC_EXTENSION uint64_t    u40PageAddr : 40;      /**< Bits 51:12  - Page address (Next Table Address). */
        RT_GCC_EXTENSION uint64_t    u9Rsvd0 : 9;           /**< Bits 60:52  - Reserved. */
        RT_GCC_EXTENSION uint64_t    u1IoRead : 1;          /**< Bit 61      - IR: I/O Read permission. */
        RT_GCC_EXTENSION uint64_t    u1IoWrite : 1;         /**< Bit 62      - IW: I/O Wead permission. */
        RT_GCC_EXTENSION uint64_t    u1Ign0 : 1;            /**< Bit 63      - Ignored. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t        u;
} IOPDE_T;
AssertCompileSize(IOPDE_T, 8);

/**
 * Interrupt Remapping Table Entry.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1RemapEnable : 1;      /**< Bit  0     - RemapEn: Remap Enable. */
        uint32_t    u1SuppressPf : 1;       /**< Bit  1     - SupIOPF: Supress I/O Page Fault. */
        uint32_t    u3IntrType : 1;         /**< Bits 4:2   - IntType: Interrupt Type. */
        uint32_t    u1ReqEoi : 1;           /**< Bit  5     - RqEoi: Request EOI. */
        uint32_t    u1DstMode : 1;          /**< Bit  6     - DM: Destination Mode. */
        uint32_t    u1GuestMode : 1;        /**< Bit  7     - GuestMode. */
        uint32_t    u8Dst : 8;              /**< Bits 15:8  - Destination. */
        uint32_t    u8Vector : 8;           /**< Bits 23:16 - Vector. */
        uint32_t    u8Rsvd0 : 8;            /**< Bits 31:24 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t        u;
} IRTE_T;
AssertCompileSize(IRTE_T, 4);

/**
 * Command: Generic Command Buffer Entry.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Operand1Lo;          /**< Bits 31:0   - Operand 1 (Lo). */
        uint32_t    u32Operand1Hi : 28;     /**< Bits 59:32  - Operand 1 (Hi). */
        uint32_t    u4Opcode : 4;           /**< Bits 63:60  - Op Code. */
        uint64_t    u64Operand2;            /**< Bits 127:64 - Operand 2. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_GENERIC_T;
AssertCompileSize(CMD_GENERIC_T, 16);

/**
 * Command: COMPLETION_WAIT.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1Store : 1;           /**< Bit  0      - S: Completion Store. */
        uint32_t    u1Interrupt : 1;       /**< Bit  1      - I: Completion Interrupt. */
        uint32_t    u1Flush : 1;           /**< Bit  2      - F: Flush Queue. */
        uint32_t    u29StoreAddrLo : 29;   /**< Bits 31:3   - Store Address (Lo). */
        uint32_t    u20StoreAddrHi : 20;   /**< Bits 51:32  - Store Address (Hi). */
        uint32_t    u8Rsvd0 : 8;           /**< Bits 59:52  - Reserved. */
        uint32_t    u4OpCode : 4;          /**< Bits 63:60  - OpCode (Command). */
        uint64_t    u64StoreData;          /**< Bits 127:64 - Store Data. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint32_t    au64[2];
} CMD_COMPLETION_WAIT_T;
AssertCompileSize(CMD_COMPLETION_WAIT_T, 16);

/**
 * Command: INVALIDATE_DEVTAB_ENTRY.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t                     u16DeviceId;       /**< Bits 15:0   - DeviceID. */
        uint16_t                     u16Rsvd0;          /**< Bits 31:16  - Reserved. */
        uint32_t                     u28Rsvd0 : 28;     /**< Bits 59:32  - Reserved. */
        uint32_t                     u4OpCode : 4;      /**< Bits 63:60  - Op Code (Command). */
        uint64_t                     u64Rsvd0;          /**< Bits 127:64 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_INV_DEV_TAB_ENTRY_T;
AssertCompileSize(CMD_INV_DEV_TAB_ENTRY_T, 16);

/**
 * Command: INVALIDATE_IOMMU_PAGES.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u20Pasid : 20;          /**< Bits 19:0   - PASID: Process Address-Space ID. */
        uint32_t    u12Rsvd0 : 12;          /**< Bits 31:20  - Reserved. */
        uint32_t    u16DomainId : 16;       /**< Bits 47:32  - Domain ID. */
        uint32_t    u12Rsvd1 : 12;          /**< Bits 59:48  - Reserved. */
        uint32_t    u4OpCode : 4;           /**< Bits 63:60  - Op Code (Command). */
        uint32_t    u1Size : 1;             /**< Bit  64     - S: Size. */
        uint32_t    u1PageDirEntries : 1;   /**< Bit  65     - PDE: Page Directory Entries. */
        uint32_t    u1GuestOrNested : 1;    /**< Bit  66     - GN: Guest (GPA) or Nested (GVA). */
        uint32_t    u9Rsvd0 : 9;            /**< Bits 75:67  - Reserved. */
        uint32_t    u20AddrLo : 20;         /**< Bits 95:76  - Address (Lo). */
        uint32_t    u32AddrHi;              /**< Bits 127:96 - Address (Hi). */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_INV_IOMMU_PAGES_T;
AssertCompileSize(CMD_INV_IOMMU_PAGES_T, 16);

/**
 * Command: INVALIDATE_IOTLB_PAGES.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;            /**< Bits 15:0   - Device ID. */
        uint8_t     u8PasidLo;              /**< Bits 23:16  - PASID: Process Address-Space ID (Lo). */
        uint8_t     u8MaxPend;              /**< Bits 31:24  - Maxpend: Maximum simultaneous in-flight transactions. */
        uint32_t    u16QueueId : 16;        /**< Bits 47:32  - Queue ID. */
        uint32_t    u12PasidHi : 12;        /**< Bits 59:48  - PASID: Process Address-Space ID (Hi). */
        uint32_t    u4OpCode : 4;           /**< Bits 63:60  - Op Code (Command). */
        uint32_t    u1Size : 1;             /**< Bit  64     - S: Size. */
        uint32_t    u1Rsvd0: 1;             /**< Bit  65     - Reserved. */
        uint32_t    u1GuestOrNested : 1;    /**< Bit  66     - GN: Guest (GPA) or Nested (GVA). */
        uint32_t    u1Rsvd1 : 1;            /**< Bit  67     - Reserved. */
        uint32_t    u2Type : 2;             /**< Bit  69:68  - Type. */
        uint32_t    u6Rsvd0 : 6;            /**< Bits 75:70  - Reserved. */
        uint32_t    u20AddrLo : 20;         /**< Bits 95:76  - Address (Lo). */
        uint32_t    u32AddrHi;              /**< Bits 127:96 - Address (Hi). */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_INV_IOTLB_PAGES_T;
AssertCompileSize(CMD_INV_IOTLB_PAGES_T, 16);

/**
 * Command: INVALIDATE_INTR_TABLE.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;        /**< Bits 15:0   - Device ID. */
        uint16_t    u16Rsvd0;           /**< Bits 31:16  - Reserved. */
        uint32_t    u32Rsvd0 : 28;      /**< Bits 59:32  - Reserved. */
        uint32_t    u4OpCode : 4;       /**< Bits 63:60  - Op Code (Command). */
        uint64_t    u64Rsvd0;           /**< Bits 127:64 - Reserved. */
    } u;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_INV_INTR_TABLE_T;
AssertCompileSize(CMD_INV_INTR_TABLE_T, 16);

/**
 * Command: COMPLETE_PPR_REQ.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;            /**< Bits 15:0    - Device ID. */
        uint16_t    u16Rsvd0;               /**< Bits 31:16   - Reserved. */
        uint32_t    u20Pasid : 20;          /**< Bits 51:32   - PASID: Process Address-Space ID. */
        uint32_t    u8Rsvd0 : 8;            /**< Bits 59:52   - Reserved. */
        uint32_t    u4OpCode : 4;           /**< Bits 63:60   - Op Code (Command). */
        uint32_t    u2Rsvd0 : 2;            /**< Bits 65:64   - Reserved. */
        uint32_t    u1GuestOrNested : 1;    /**< Bit  66      - GN: Guest (GPA) or Nested (GVA). */
        uint32_t    u29Rsvd0 : 29;          /**< Bits 95:67   - Reserved. */
        uint32_t    u16CompletionTag : 16;  /**< Bits 111:96  - Completion Tag. */
        uint32_t    u16Rsvd1 : 16;          /**< Bits 127:112 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_COMPLETE_PPR_REQ_T;
AssertCompileSize(CMD_COMPLETE_PPR_REQ_T, 16);

/**
 * Command: INV_IOMMU_ALL.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Rsvd0;           /**< Bits 31:0   - Reserved. */
        uint32_t    u28Rsvd0 : 28;      /**< Bits 59:32  - Reserved. */
        uint32_t    u4OpCode : 4;       /**< Bits 63:60  - Op Code (Command). */
        uint64_t    u64Rsvd0;           /**< Bits 127:64 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    au64[2];
} CMD_IOMMU_ALL_T;
AssertCompileSize(CMD_IOMMU_ALL_T, 16);

/**
 * Event Log Entry: Generic.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Operand1Lo;          /**< Bits 31:0   - Operand 1 (Lo). */
        uint32_t    u32Operand1Hi : 28;     /**< Bits 59:32  - Operand 1 (Hi). */
        uint32_t    u4EventCode : 4;        /**< Bits 63:60  - Event code. */
        uint32_t    u32Operand2Lo;          /**< Bits 95:64  - Operand 2 (Lo). */
        uint32_t    u32Operand2Hi;          /**< Bits 127:96 - Operand 2 (Hi). */
    } n;
    /** The 32-bit unsigned integer view.  */
    uint32_t    au32[4];
} EVT_GENERIC_T;
AssertCompileSize(EVT_GENERIC_T, 16);

/**
 * Event Log Entry: ILLEGAL_DEV_TAB_ENTRY.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;            /**< Bits 15:0   - Device ID. */
        uint16_t    u4PasidHi : 4;          /**< Bits 19:16  - PASID: Process Address-Space ID (Hi). */
        uint16_t    u12Rsvd0 : 12;          /**< Bits 31:20  - Reserved. */
        uint16_t    u16PasidLo;             /**< Bits 47:32  - PASID: Process Address-Space ID (Lo). */
        uint16_t    u1GuestOrNested : 1;    /**< Bit  48     - GN: Guest (GPA) or Nested (GVA). */
        uint16_t    u2Rsvd0 : 2;            /**< Bits 50:49  - Reserved. */
        uint16_t    u1Interrupt : 1;        /**< Bit  51     - I: Interrupt. */
        uint16_t    u1Rsvd0 : 1;            /**< Bit  52     - Reserved. */
        uint16_t    u1ReadWrite : 1;        /**< Bit  53     - RW: Read/Write. */
        uint16_t    u1Rsvd1 : 1;            /**< Bit  54     - Reserved. */
        uint16_t    u1RsvdZero : 1;         /**< Bit  55     - RZ: Reserved bit not Zero or invalid level encoding. */
        uint16_t    u1Translation : 1;      /**< Bit  56     - TN: Translation. */
        uint16_t    u3Rsvd0 : 3;            /**< Bits 59:57  - Reserved. */
        uint16_t    u4EventCode : 4;        /**< Bits 63:60  - Event code. */
        uint32_t    u2Rsvd1 : 2;            /**< Bits 65:64  - Reserved. */
        uint32_t    u30AddrLo : 2;          /**< Bits 95:66  - Address: Device Virtual Address (Lo). */
        uint32_t    u30AddrHi;              /**< Bits 127:96 - Address: Device Virtual Address (Hi). */
    } n;
    /** The 32-bit unsigned integer view.  */
    uint32_t    au32[4];
} EVT_ILLEGAL_DEV_TAB_ENTRY_T;
AssertCompileSize(EVT_ILLEGAL_DEV_TAB_ENTRY_T, 16);

/**
 * Event Log Entry: IO_PAGE_FAULT_EVENT.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;            /**< Bits 15:0   - Device ID. */
        uint16_t    u4PasidHi : 4;          /**< Bits 19:16  - PASID: Process Address-Space ID (Hi). */
        uint16_t    u16DomainOrPasidLo;     /**< Bits 47:32  - D/P: Domain ID or Process Address-Space ID (Lo). */
        uint16_t    u1GuestOrNested : 1;    /**< Bit  48     - GN: Guest (GPA) or Nested (GVA). */
        uint16_t    u1NoExecute : 1;        /**< Bit  49     - NX: No Execute. */
        uint16_t    u1User : 1;             /**< Bit  50     - US: User/Supervisor. */
        uint16_t    u1Interrupt : 1;        /**< Bit  51     - I: Interrupt. */
        uint16_t    u1Present : 1;          /**< Bit  52     - PR: Present. */
        uint16_t    u1ReadWrite : 1;        /**< Bit  53     - RW: Read/Write. */
        uint16_t    u1Perm : 1;             /**< Bit  54     - PE: Permission Indicator. */
        uint16_t    u1RsvdZero : 1;         /**< Bit  55     - RZ: Reserved bit not Zero or invalid level encoding. */
        uint16_t    u1Translation : 1;      /**< Bit  56     - TN: Translation. */
        uint16_t    u3Rsvd0 : 3;            /**< Bit  59:57  - Reserved. */
        uint16_t    u4EventCode : 4;        /**< Bits 63:60  - Event code. */
        uint64_t    u64Addr;                /**< Bits 127:64 - Address: Device Virtual Address. */
    } n;
    /** The 32-bit unsigned integer view.  */
    uint32_t    au32[4];
} EVT_IO_PAGE_FAULT_T;
AssertCompileSize(EVT_IO_PAGE_FAULT_T, 16);

/**
 * Event Log Entry: DEV_TAB_HARDWARE_ERROR.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;            /**< Bits 15:0   - Device ID. */
        uint16_t    u16Rsvd0;               /**< Bits 31:16  - Reserved. */
        uint32_t    u19Rsvd0 : 19;          /**< Bits 50:32  - Reserved. */
        uint32_t    u1Intr : 1;             /**< Bit  51     - I: Interrupt. */
        uint32_t    u1Rsvd0 : 1;            /**< Bit  52     - Reserved. */
        uint32_t    u1ReadWrite : 1;        /**< Bit  53     - RW: Read/Write. */
        uint32_t    u2Rsvd0 : 2;            /**< Bits 55:54  - Reserved. */
        uint32_t    u1Translation : 1;      /**< Bit  56     - TR: Translation. */
        uint32_t    u2Type : 2;             /**< Bits 58:57  - Type: The type of hardware error. */
        uint32_t    u1Rsvd1 : 1;            /**< Bit  59     - Reserved. */
        uint32_t    u4EventCode : 4;        /**< Bits 63:60  - Event code. */
        uint32_t    u4Rsvd0 : 4;            /**< Bits 67:64  - Reserved. */
        uint32_t    u28AddrLo : 28;         /**< Bits 95:68  - Address: System Physical Address (Lo). */
        uint32_t    u32AddrHi;              /**< Bits 127:96 - Address: System Physical Address (Hi). */
    } n;
    /** The 32-bit unsigned integer view.  */
    uint32_t    au32[4];
} EVT_DEV_TAB_HARDWARE_ERROR;
AssertCompileSize(EVT_DEV_TAB_HARDWARE_ERROR, 16);

/**
 * Event Log Entry: EVT_PAGE_TAB_HARDWARE_ERROR.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;                /**< Bits 15:0   - Device ID. */
        uint16_t    u16Rsvd0;                   /**< Bits 31:16  - Reserved. */
        uint32_t    u16DomainOrPasidLo : 16;    /**< Bits 47:32  - D/P: Domain ID or Process Address-Space ID (Lo). */
        uint32_t    u1GuestOrNested : 1;        /**< Bit  48     - GN: Guest (GPA) or Nested (GVA). */
        uint32_t    u2Rsvd0 : 2;                /**< Bits 50:49  - Reserved. */
        uint32_t    u1Interrupt : 1;            /**< Bit  51     - I: Interrupt. */
        uint32_t    u1Rsvd0 : 1;                /**< Bit  52     - Reserved. */
        uint32_t    u1ReadWrite : 1;            /**< Bit  53     - RW: Read/Write. */
        uint32_t    u2Rsvd1 : 2;                /**< Bit  55:54  - Reserved. */
        uint32_t    u1Translation : 1;          /**< Bit  56     - TR: Translation. */
        uint32_t    u2Type : 2;                 /**< Bits 58:57  - Type: The type of hardware error. */
        uint32_t    u1Rsvd1 : 1;                /**< Bit  59     - Reserved. */
        uint32_t    u4EventCode : 4;            /**< Bit  63:60  - Event code. */
        /** @todo r=ramshankar: Figure 55: PAGE_TAB_HARDWARE_ERROR says Addr[31:3] but
         *        table 58 mentions Addr[31:4]. Looks like a typo in the figure. Use
         *        table as it makes more sense and matches address size in
         *        EVT_DEV_TAB_HARDWARE_ERROR. See AMD AMD IOMMU spec (3.05-PUB, Jan
         *        2020). */
        uint32_t    u4Rsvd0 : 4;                /**< Bits 67:64  - Reserved. */
        uint32_t    u28AddrLo : 28;             /**< Bits 95:68  - Address: SPA of page table entry (Lo). */
        uint32_t    u32AddrHi;                  /**< Bits 127:96 - Address: SPA of page table entry (Hi). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[4];
} EVT_PAGE_TAB_HARDWARE_ERROR;
AssertCompileSize(EVT_PAGE_TAB_HARDWARE_ERROR, 16);

/**
 * Event Log Entry: ILLEGAL_COMMAND_ERROR.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Rsvd0;           /**< Bits 31:0   - Reserved. */
        uint32_t    u28Rsvd0 : 28;      /**< Bits 47:32  - Reserved. */
        uint32_t    u4EventCode : 4;    /**< Bits 63:60  - Event code. */
        uint32_t    u4Rsvd0 : 4;        /**< Bits 67:64  - Reserved. */
        uint32_t    u28AddrLo : 28;     /**< Bits 95:68  - Address: SPA of the invalid command (Lo). */
        uint32_t    u32AddrHi;          /**< Bits 127:96 - Address: SPA of the invalid command (Hi). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[4];
} EVT_ILLEGAL_COMMAND_ENTRY;
AssertCompileSize(EVT_ILLEGAL_COMMAND_ENTRY, 16);

/**
 * Event Log Entry: COMMAND_HARDWARE_ERROR.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Rsvd0;           /**< Bits 31:0  - Reserved. */
        uint32_t    u4Rsvd0 : 4;        /**< Bits 35:32 - Reserved. */
        uint32_t    u28AddrLo : 28;     /**< Bits 63:36 - Address: SPA of the attempted access (Lo). */
        uint32_t    u32AddrHi;          /**< Bits 95:64 - Address: SPA of the attempted access (Hi). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[3];
} EVT_COMMAND_HARDWARE_ERROR;
AssertCompileSize(EVT_COMMAND_HARDWARE_ERROR, 12);

/**
 * Event Log Entry: IOTLB_INV_TIMEOUT.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16DeviceId;        /**< Bits 15:0   - Device ID. */
        uint16_t    u16Rsvd0;           /**< Bits 31:16  - Reserved.*/
        uint32_t    u28Rsvd0 : 28;      /**< Bits 59:32  - Reserved. */
        uint32_t    u4EventCode : 4;    /**< Bits 63:60  - Event code. */
        uint32_t    u4Rsvd0 : 4;        /**< Bits 67:64  - Reserved. */
        uint32_t    u28AddrLo : 28;     /**< Bits 95:68  - Address: SPA of the invalidation command that timedout (Lo). */
        uint32_t    u32AddrHi;          /**< Bits 127:96 - Address: SPA of the invalidation command that timedout (Hi). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[4];
} EVT_IOTLB_INV_TIMEOUT;
AssertCompileSize(EVT_IOTLB_INV_TIMEOUT, 16);

/**
 * Event Log Entry: INVALID_DEVICE_REQUEST.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u16DeviceId : 16;       /***< Bits 15:0   - Device ID. */
        uint32_t    u4PasidHi : 4;          /***< Bits 19:16  - PASID: Process Address-Space ID (Hi). */
        uint32_t    u12Rsvd0 : 12;          /***< Bits 31:20  - Reserved. */
        uint32_t    u16PasidLo : 16;        /***< Bits 47:32  - PASID: Process Address-Space ID (Lo). */
        uint32_t    u1GuestOrNested : 1;    /***< Bit  48     - GN: Guest (GPA) or Nested (GVA). */
        uint32_t    u1User : 1;             /***< Bit  49     - US: User/Supervisor. */
        uint32_t    u6Rsvd0 : 6;            /***< Bits 55:50  - Reserved. */
        uint32_t    u1Translation: 1;       /***< Bit  56     - TR: Translation. */
        uint32_t    u3Type: 3;              /***< Bits 59:57  - Type: The type of hardware error. */
        uint32_t    u4EventCode : 4;        /***< Bits 63:60  - Event code. */
        uint64_t    u64Addr;                /***< Bits 127:64 - Address: Translation or access address. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[4];
} EVT_INVALID_DEVICE_REQUEST;
AssertCompileSize(EVT_INVALID_DEVICE_REQUEST, 16);

/**
 * Event Log Entry: EVENT_COUNTER_ZERO.
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u32Rsvd0;               /**< Bits 31:0   - Reserved. */
        uint32_t    u28Rsvd0 : 28;          /**< Bits 59:32  - Reserved. */
        uint32_t    u4EventCode : 4;        /**< Bits 63:60  - Event code. */
        uint32_t    u20CounterNoteHi : 20;  /**< Bits 83:64  - CounterNote: Counter value for the event counter register (Hi). */
        uint32_t    u12Rsvd0 : 12;          /**< Bits 95:84  - Reserved. */
        uint32_t    u32CounterNoteLo;       /**< Bits 127:96 - CounterNote: Counter value for the event cuonter register (Lo). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[4];
} EVT_EVENT_COUNTER_ZERO;
AssertCompileSize(EVT_EVENT_COUNTER_ZERO, 16);

/* Not needed as we can initialize from bitfields and set/get using PCI config PDM helpers. */
#if 0
/**
 * IOMMU Capability Header (PCI).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u8CapId : 8;        /**< Bits 7:0   - CapId: Capability ID. */
        uint32_t    u8CapPtr : 8;       /**< Bits 15:8  - CapPtr: Pointer (PCI config offset) to the next capability. */
        uint32_t    u3CapType : 3;      /**< Bits 18:16 - CapType: Capability Type. */
        uint32_t    u5CapRev : 5;       /**< Bits 23:19 - CapRev: Capability revision. */
        uint32_t    u1IoTlbSup : 1;     /**< Bit  24    - IotlbSup: IOTLB Support. */
        uint32_t    u1HtTunnel : 1;     /**< Bit  25    - HtTunnel: HyperTransport Tunnel translation support. */
        uint32_t    u1NpCache : 1;      /**< Bit  26    - NpCache: Not Present table entries are cached. */
        uint32_t    u1EfrSup : 1;       /**< Bit  27    - EFRSup: Extended Feature Register Support. */
        uint32_t    u1CapExt : 1;       /**< Bit  28    - CapExt: Misc. Information Register 1 Support. */
        uint32_t    u4Rsvd0 : 4;        /**< Bits 31:29 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    u32;
} IOMMU_CAP_HDR_T;
AssertCompileSize(IOMMU_CAP_HDR_T, 4);
#endif

/**
 * Device Table Base Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u9Size : 9;             /**< Bits 8:0   - Size: Size of the device table. */
        RT_GCC_EXTENSION uint64_t   u3Rsvd0 : 3;            /**< Bits 11:9  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40DevTabBase : 40;     /**< Bits 51:12 - DevTabBase: Device table base address. */
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;          /**< Bits 63:52 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} DEV_TAB_BAR_T;
AssertCompileSize(DEV_TAB_BAR_T, 8);

/**
 * Command Buffer Base Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;      /**< Bits 11:0  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40CmdBase : 40;    /**< Bits 51:12 - ComBase: Command buffer base address. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd0 : 4;        /**< Bits 55:52 - Reserved. */
        RT_GCC_EXTENSION uint64_t   u4CmdLen : 4;       /**< Bits 59:56 - ComLen: Command buffer length. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd1 : 4;        /**< Bits 63:60 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} CMD_BUF_BAR_T;
AssertCompileSize(CMD_BUF_BAR_T, 8);

/**
 * Event Log Base Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;      /**< Bits 11:0  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40EvtBase : 40;    /**< Bits 51:12 - EventBase: Event log base address. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd0 : 4;        /**< Bits 55:52 - Reserved. */
        RT_GCC_EXTENSION uint64_t   u4EvtLen : 4;       /**< Bits 59:56 - EventLen: Event log length. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd1 : 4;        /**< Bits 63:60 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} EVT_LOG_BAR_T;
AssertCompileSize(EVT_LOG_BAR_T, 8);

/**
 * IOMMU Control Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1IommuEn : 1;               /**< Bit  0     - IommuEn: IOMMU Enable. */
        uint32_t    u1HtTunEn : 1;               /**< Bit  1     - HtTunEn: HyperTransport Tunnel Enable. */
        uint32_t    u1EvtLogEn : 1;              /**< Bit  2     - EventLogEn: Event Log Enable. */
        uint32_t    u1EvtIntrEn : 1;             /**< Bit  3     - EventIntEn: Event Log Interrupt Enable. */
        uint32_t    u1CompWaitIntrEn : 1;        /**< Bit  4     - ComWaitIntEn: Completion Wait Interrupt Enable. */
        uint32_t    u3InvTimeOut : 3;            /**< Bits 7:5   - InvTimeOut: Invalidation Timeout. */
        uint32_t    u1PassPW : 1;                /**< Bit  8     - PassPW: Pass Posted Write. */
        uint32_t    u1ResPassPW : 1;             /**< Bit  9     - ResPassPW: Response Pass Posted Write. */
        uint32_t    u1Coherent : 1;              /**< Bit  10    - Coherent: HT read request packet Coherent bit. */
        uint32_t    u1Isoc : 1;                  /**< Bit  11    - Isoc: HT read request packet Isochronous bit. */
        uint32_t    u1CmdBufEn : 1;              /**< Bit  12    - CmdBufEn: Command Buffer Enable. */
        uint32_t    u1PprLogEn : 1;              /**< Bit  13    - PprLogEn: Peripheral Page Request (PPR) Log Enable. */
        uint32_t    u1PprIntrEn : 1;             /**< Bit  14    - PprIntrEn: Peripheral Page Request Interrupt Enable. */
        uint32_t    u1PprEn : 1;                 /**< Bit  15    - PprEn: Peripheral Page Request processing Enable. */
        uint32_t    u1GstTranslateEn : 1;        /**< Bit  16    - GTEn: Guest Translate Enable. */
        uint32_t    u1GstVirtApicEn : 1;         /**< Bit  17    - GAEn: Guest Virtual-APIC Enable. */
        uint32_t    u4Crw : 1;                   /**< Bits 21:18 - CRW: Intended for future use (not documented). */
        uint32_t    u1SmiFilterEn : 1;           /**< Bit  22    - SmiFEn: SMI Filter Enable. */
        uint32_t    u1SelfWriteBackDis : 1;      /**< Bit  23    - SlfWBDis: Self Write-Back Disable. */
        uint32_t    u1SmiFilterLogEn : 1;        /**< Bit  24    - SmiFLogEn: SMI Filter Log Enable. */
        uint32_t    u3GstVirtApicModeEn : 3;     /**< Bits 27:25 - GAMEn: Guest Virtual-APIC Mode Enable. */
        uint32_t    u1GstLogEn : 1;              /**< Bit  28    - GALogEn: Guest Virtual-APIC GA Log Enable. */
        uint32_t    u1GstIntrEn : 1;             /**< Bit  29    - GAIntEn: Guest Virtual-APIC Interrupt Enable. */
        uint32_t    u2DualPprLogEn : 2;          /**< Bits 31:30 - DualPprLogEn: Dual Peripheral Page Request Log Enable. */
        uint32_t    u2DualEvtLogEn : 2;          /**< Bits 33:32 - DualEventLogEn: Dual Event Log Enable. */
        uint32_t    u3DevTabSegEn : 3;           /**< Bits 36:34 - DevTblSegEn: Device Table Segment Enable. */
        uint32_t    u2PrivAbortEn : 2;           /**< Bits 38:37 - PrivAbrtEn: Privilege Abort Enable. */
        uint32_t    u1PprAutoRespEn : 1;         /**< Bit  39    - PprAutoRspEn: Peripheral Page Request Auto Response Enable. */
        uint32_t    u1MarcEn : 1;                /**< Bit  40    - MarcEn: Memory Address Routing and Control Enable. */
        uint32_t    u1BlockStopMarkEn : 1;       /**< Bit  41    - BlkStopMarkEn: Block StopMark messages Enable. */
        uint32_t    u1PprAutoRespAlwaysOnEn : 1; /**< Bit  42    - PprAutoRspAon:: PPR Auto Response - Always On Enable. */
        uint32_t    u1DomainIDPNE : 1;           /**< Bit  43    - DomainIDPE: Reserved (not documented). */
        uint32_t    u1Rsvd0 : 1;                 /**< Bit  44    - Reserved. */
        uint32_t    u1EnhancedPpr : 1;           /**< Bit  45    - EPHEn: Enhanced Peripheral Page Request Handling Enable. */
        uint32_t    u2HstAccDirtyBitUpdate : 2;  /**< Bits 47:46 - HADUpdate: Access and Dirty Bit updated in host page table. */
        uint32_t    u1GstDirtyUpdateDis : 1;     /**< Bit  48    - GDUpdateDis: Disable hardare update of Dirty bit in GPT. */
        uint32_t    u1Rsvd1 : 1;                 /**< Bit  49    - Reserved. */
        uint32_t    u1X2ApicEn : 1;              /**< Bit  50    - XTEn: Enable X2APIC. */
        uint32_t    u1X2ApicIntrGenEn : 1;       /**< Bit  51    - IntCapXTEn: Enable IOMMU X2APIC Interrupt generation. */
        uint32_t    u2Rsvd0 : 2;                 /**< Bits 53:52 - Reserved. */
        uint32_t    u1GstAccessUpdateDis : 1;    /**< Bit  54    - GAUpdateDis: Disable hardare update of Access bit in GPT. */
        uint32_t    u8Rsvd0 : 8;                 /**< Bits 63:55 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_CTRL_T;
AssertCompileSize(IOMMU_CTRL_T, 8);

/**
 * IOMMU Exclusion Base Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u1ExclEnable : 1;       /**< Bit 0      - ExEn: Exclusion Range Enable. */
        RT_GCC_EXTENSION uint64_t   u1AllowAll : 1;         /**< Bit 1      - Allow: Allow All Devices. */
        RT_GCC_EXTENSION uint64_t   u10Rsvd0 : 10;          /**< Bits 11:2  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40ExclRangeBase : 40;  /**< Bits 51:12 - Exclusion Range Base Address. */
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;          /**< Bits 63:52 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_EXCL_BASE_T;
AssertCompileSize(IOMMU_EXCL_BASE_T, 8);

/**
 * IOMMU Exclusion Range Limit Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;      /**< Bits 11:0  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40ExclLimit : 40;  /**< Bits 51:12 - Exclusion Range Limit. */
        RT_GCC_EXTENSION uint64_t   u12Rsvd1 : 12;      /**< Bits 63:52 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_EXCL_RANGE_LIMIT_T;
AssertCompileSize(IOMMU_EXCL_RANGE_LIMIT_T, 8);

/**
 * IOMMU Extended Feature Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1PrefetchSup : 1;            /**< Bit  0     - PreFSup: Prefetch Support. */
        uint32_t    u1PprSup : 1;                 /**< Bit  1     - PPRSup: Peripheral Page Request Support. */
        uint32_t    u1X2ApicSup : 1;              /**< Bit  2     - XTSup: x2Apic Support. */
        uint32_t    u1NoExecuteSup : 1;           /**< Bit  3     - NXSup: No-Execute and Privilege Level Support. */
        uint32_t    u1GstTranslateSup : 1;        /**< Bit  4     - GTSup: Guest Translations Support. */
        uint32_t    u1Rsvd0 : 1;                  /**< Bit  5     - Reserved. */
        uint32_t    u1InvAllSup : 1;              /**< Bit  6     - IASup: Invalidate-All Support. */
        uint32_t    u1GstVirtApicSup : 1;         /**< Bit  7     - GASup: Guest Virtual-APIC Support. */
        uint32_t    u1HwErrorSup : 1;             /**< Bit  8     - HESup: Hardware Error registers Support. */
        uint32_t    u1PerfCounterSup : 1;         /**< Bit  8     - PCSup: Performance Counter Support. */
        uint32_t    u2HostAddrTranslateSize : 2;  /**< Bits 11:10 - HATS: Host Address Translation Size. */
        uint32_t    u2GstAddrTranslateSize : 2;   /**< Bits 13:12 - GATS: Guest Address Translation Size. */
        uint32_t    u2GstCr3RootTblLevel : 2;     /**< Bits 15:14 - GLXSup: Guest CR3 Root Table Level (Max) Size Support. */
        uint32_t    u2SmiFilterSup : 2;           /**< Bits 17:16 - SmiFSup: SMI Filter Register Support. */
        uint32_t    u3SmiFilterCount : 3;         /**< Bits 20:18 - SmiFRC: SMI Filter Register Count. */
        uint32_t    u3GstVirtApicModeSup : 3;     /**< Bits 23:21 - GAMSup: Guest Virtual-APIC Modes Supported. */
        uint32_t    u2DualPprLogSup : 2;          /**< Bits 25:24 - DualPprLogSup: Dual Peripheral Page Request Log Support. */
        uint32_t    u2Rsvd0 : 2;                  /**< Bits 27:26 - Reserved. */
        uint32_t    u2DualEvtLogSup : 2;          /**< Bits 29:28 - DualEventLogSup: Dual Event Log Support. */
        uint32_t    u2Rsvd1 : 2;                  /**< Bits 31:30 - Reserved. */

        uint32_t    u5MaxPasidSup : 5;            /**< Bits 36:32 - PASMax: Maximum PASID Supported. */
        uint32_t    u1UserSupervisorSup : 1;      /**< Bit  37    - USSup: User/Supervisor Page Protection Support. */
        uint32_t    u2DevTabSegSup : 2;           /**< Bits 39:38 - DevTlbSegSup: Segmented Device Table Support. */
        uint32_t    u1PprLogOverflowWarn : 1;     /**< Bit  40    - PprOvrflwEarlySup: PPR Log Overflow Early Warning Support. */
        uint32_t    u1PprAutoRespSup : 1;         /**< Bit  41    - PprAutoRspSup: PPR Automatic Response Support. */
        uint32_t    u2MarcSup : 2;                /**< Bit  43:42 - MarcSup: Memory Access Routing and Control Support. */
        uint32_t    u1BlockStopMarkSup : 1;       /**< Bit  44    - BlkStopMarkSup: Block StopMark messages Support. */
        uint32_t    u1PerfOptSup : 1;             /**< Bit  45    - PerfOptSup: IOMMU Performance Optimization Support. */
        uint32_t    u1MsiCapMmioSup : 1;          /**< Bit  46    - MsiCapMmioSup: MSI Capability Register MMIO Access Support. */
        uint32_t    u1Rsvd1 : 1;                  /**< Bit  47    - Reserved. */
        uint32_t    u1GstIoSup : 1;               /**< Bit  48    - GIoSup: Guest I/O Protection Support. */
        uint32_t    u1HostAccessSup : 1;          /**< Bit  49    - HASup: Host Access Support. */
        uint32_t    u1EnhancedPprSup : 1;         /**< Bit  50    - EPHSup: Enhanced Peripheral Page Request Handling Support. */
        uint32_t    u1AttrForwardSup : 1;         /**< Bit  51    - AttrFWSup: Attribute Forward Support. */
        uint32_t    u1HostDirtySup : 1;           /**< Bit  52    - HDSup: Host Dirty Support. */
        uint32_t    u1Rsvd2 : 1;                  /**< Bit  53    - Reserved. */
        uint32_t    u1InvIoTlbTypeSup : 1;        /**< Bit  54    - InvIotlbTypeSup: Invalidate IOTLB Type Support. */
        uint32_t    u6Rsvd0 : 6;                  /**< Bit  60:55 - Reserved. */
        uint32_t    u1GstUpdateDisSup : 1;        /**< Bit  61    - GAUpdateDisSup: Disable hardware update on GPT Support. */
        uint32_t    u1ForcePhysDstSup : 1;        /**< Bit  62    - ForcePhyDestSup: Force Phys. Dst. Mode for Remapped Intr. */
        uint32_t    u1Rsvd3 : 1;                  /**< Bit  63    - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_EFR_T;
AssertCompileSize(IOMMU_EFR_T, 8);

/**
 * Peripheral Page Request Log Base Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;      /**< Bit 11:0   - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40PprLogBase : 40; /**< Bits 51:12 - PPRLogBase: Peripheral Page Request Log Base Address. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd0 : 4;        /**< Bits 55:52 - Reserved. */
        RT_GCC_EXTENSION uint64_t   u4PprLogLen : 4;    /**< Bits 59:56 - PPRLogLen: Peripheral Page Request Log Length. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd1 : 4;        /**< Bits 63:60 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} PPR_LOG_BAR_T;
AssertCompileSize(PPR_LOG_BAR_T, 8);

/**
 * IOMMU Hardware Event Upper Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u60FirstOperand : 60;   /**< Bits 59:0  - First event code dependent operand. */
        RT_GCC_EXTENSION uint64_t   u4EvtCode : 4;          /**< Bits 63:60 - Event Code. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_HW_EVT_HI_T;
AssertCompileSize(IOMMU_HW_EVT_HI_T, 8);

/**
 * IOMMU Hardware Event Lower Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef uint64_t IOMMU_HW_EVT_LO_T;

/**
 * IOMMU Hardware Event Status (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t   u1HwEventValid : 1;      /**< Bit 0      - HEV: Hardware Event Valid. */
        uint32_t   u1HwEventOverflow : 1;   /**< Bit 1      - HEO: Hardware Event Overflow. */
        uint32_t   u30Rsvd0 : 30;           /**< Bits 31:2  - Reserved. */
        uint32_t   u32Rsvd0;                /**< Bits 63:32 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_HW_EVT_STATUS_T;
AssertCompileSize(IOMMU_HW_EVT_STATUS_T, 8);

/**
 * Guest Virtual-APIC Log Base Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;      /**< Bit 11:0   - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40GALogBase : 40;  /**< Bits 51:12 - GALogBase: Guest Virtual-APIC Log Base Address. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd0 : 4;        /**< Bits 55:52 - Reserved. */
        RT_GCC_EXTENSION uint64_t   u4GALogLen : 4;     /**< Bits 59:56 - GALogLen: Guest Virtual-APIC Log Length. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd1 : 4;        /**< Bits 63:60 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} GALOG_BAR_T;
AssertCompileSize(GALOG_BAR_T, 8);

/**
 * Guest Virtual-APIC Log Tail Address Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u3Rsvd0 : 3;            /**< Bits 2:0   - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40GALogTailAddr : 48;  /**< Bits 51:3  - GATAddr: Guest Virtual-APIC Tail Log Address. */
        RT_GCC_EXTENSION uint64_t   u11Rsvd1 : 11;          /**< Bits 63:52 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} GALOG_TAIL_ADDR_T;
AssertCompileSize(GALOG_TAIL_ADDR_T, 8);

/**
 * PPR Log B Base Address Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to PPR_LOG_BAR_T.
 */
typedef PPR_LOG_BAR_T       PPR_LOG_B_BAR_T;

/**
 * Event Log B Base Address Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to EVT_LOG_BAR_T.
 */
typedef EVT_LOG_BAR_T       EVT_LOG_B_BAR_T;

/**
 * Device Table Segment Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        RT_GCC_EXTENSION uint64_t   u8Size : 8;             /**< Bits 7:0   - Size: Size of the Device Table segment. */
        RT_GCC_EXTENSION uint64_t   u4Rsvd0 : 4;            /**< Bits 11:8  - Reserved. */
        RT_GCC_EXTENSION uint64_t   u40DevTabBase : 40;     /**< Bits 51:12 - DevTabBase: Device Table Segment Base Address. */
        RT_GCC_EXTENSION uint64_t   u12Rsvd0 : 12;          /**< Bits 63:52 - Reserved. */
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} DEV_TAB_SEG_BAR_T;
AssertCompileSize(DEV_TAB_SEG_BAR_T, 8);

/**
 * Device-specific Feature Extension (DSFX) Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u24DevSpecFeatSup : 24;     /**< Bits 23:0  - DevSpecificFeatSupp: Implementation specific features. */
        uint32_t    u4RevMinor : 4;             /**< Bits 27:24 - RevMinor: Minor revision identifier. */
        uint32_t    u4RevMajor : 4;             /**< Bits 31:28 - RevMajor: Major revision identifier. */
        uint32_t    u32Rsvd0;                   /**< Bits 63:32 - Reserved.*/
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} DEV_SPECIFIC_FEAT_T;
AssertCompileSize(DEV_SPECIFIC_FEAT_T, 8);

/**
 * Device-specific Control Extension (DSCX) Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u24DevSpecFeatSup : 24;     /**< Bits 23:0  - DevSpecificFeatCntrl: Implementation specific control. */
        uint32_t    u4RevMinor : 4;             /**< Bits 27:24 - RevMinor: Minor revision identifier. */
        uint32_t    u4RevMajor : 4;             /**< Bits 31:28 - RevMajor: Major revision identifier. */
        uint32_t    u32Rsvd0;                   /**< Bits 63:32 - Reserved.*/
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} DEV_SPECIFIC_CTRL_T;
AssertCompileSize(DEV_SPECIFIC_CTRL_T, 8);

/**
 * Device-specific Status Extension (DSSX) Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u24DevSpecStatus : 24;      /**< Bits 23:0  - DevSpecificFeatStatus: Implementation specific status. */
        uint32_t    u4RevMinor : 4;             /**< Bits 27:24 - RevMinor: Minor revision identifier. */
        uint32_t    u4RevMajor : 4;             /**< Bits 31:28 - RevMajor: Major revision identifier. */
        uint32_t    u32Rsvd0;                   /**< Bits 63:32 - Reserved.*/
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} DEV_SPECIFIC_STATUS_T;
AssertCompileSize(DEV_SPECIFIC_STATUS_T, 8);

/**
 * MSI Information Register 0 and 1 (PCI) / MSI Vector Register 0 and 1 (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u5MsiNum : 5;           /**< Bits 4:0   - MsiNum: MSI Vector used for interrupt messages generated by the IOMMU. */
        uint32_t    u3GstVirtAddrSize: 3;   /**< Bits 7:5   - GVAsize: Guest Virtual Address Size. */
        uint32_t    u7PhysAddrSize : 7;     /**< Bits 14:8  - PAsize: Physical Address Size. */
        uint32_t    u7VirtAddrSize : 7;     /**< Bits 21:15 - VAsize: Virtual Address Size. */
        uint32_t    u1HtAtsResv: 1;         /**< Bit  22    - HtAtsResv: HyperTransport ATS Response Address range Reserved. */
        uint32_t    u4Rsvd0 : 4;            /**< Bits 26:23 - Reserved. */
        uint32_t    u5MsiNumPpr : 5;        /**< Bits 31:27 - MsiNumPPR: Peripheral Page Request MSI message number. */
        uint32_t    u5MsiNumGa : 5;         /**< Bits 36:32 - MsiNumGa: MSI message number for guest virtual-APIC log. */
        uint32_t    u27Rsvd0: 27;           /**< Bits 63:37 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} MSI_MISC_INFO_T;
AssertCompileSize(MSI_MISC_INFO_T, 8);
/** MSI Vector Register 0 and 1 (MMIO). */
typedef MSI_MISC_INFO_T       MSI_VECTOR_T;

/**
 * MSI Capability Header Register (PCI + MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u8MsiCapId : 8;         /**< Bits 7:0   - MsiCapId: Capability ID. */
        uint32_t    u8MsiCapPtr : 8;        /**< Bits 15:8  - MsiCapPtr: Pointer (PCI config offset) to the next capability. */
        uint32_t    u1MsiEnable : 1;        /**< Bit  16    - MsiEn: Message Signal Interrupt Enable. */
        uint32_t    u3MsiMultiMessCap : 3;  /**< Bits 19:17 - MsiMultMessCap: MSI Multi-Message Capability. */
        uint32_t    u3MsiMultiMessEn : 3;   /**< Bits 22:20 - MsiMultMessEn: MSI Multi-Message Enable. */
        uint32_t    u1Msi64BitEn : 1;       /**< Bit  23    - Msi64BitEn: MSI 64-bit Enable. */
        uint32_t    u8Rsvd0 : 8;            /**< Bits 31:24 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    u32;
} MSI_CAP_HDR_T;
AssertCompileSize(MSI_CAP_HDR_T, 4);

/**
 * MSI Address Register (PCI + MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u2Rsvd : 2;         /**< Bits 1:0   - Reserved. */
        uint32_t    u30MsiAddrLo : 30;  /**< Bits 31:2  - MsiAddr: MSI Address (Lo). */
        uint32_t    u32MsiAddrHi;       /**< Bits 63:32 - MsiAddr: MSI Address (Hi). */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} MSI_ADDR_T;
AssertCompileSize(MSI_ADDR_T, 8);

/**
 * MSI Data Register (PCI + MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint16_t    u16MsiData;     /**< Bits 15:0  - MsiData: MSI Data. */
        uint16_t    u16Rsvd0;       /**< Bits 31:16 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    u32;
} MSI_DATA_T;
AssertCompileSize(MSI_DATA_T, 4);

/**
 * MSI Mapping Capability Header Register (PCI + MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u8MsiMapCapId : 8;  /**< Bits 7:0   - MsiMapCapId: MSI Map capability ID. */
        uint32_t    u8Rsvd0 : 8;        /**< Bits 15:8  - Reserved. */
        uint32_t    u1MsiMapEn : 1;     /**< Bit  16    - MsiMapEn: MSI Map enable. */
        uint32_t    u1MsiMapFixed : 1;  /**< Bit  17    - MsiMapFixd: MSI Map fixed. */
        uint32_t    u9Rsvd0 : 9;        /**< Bits 26:18 - Reserved. */
        uint32_t    u5MapCapType : 5;   /**< Bits 31:27 - MsiMapCapType: MSI Mapping capability type. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    u32;
} MSI_MAP_CAP_HDR_T;
AssertCompileSize(MSI_MAP_CAP_HDR_T, 4);

/**
 * Performance Optimization Control Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u13Rsvd0 : 13;      /**< Bits 12:0  - Reserved. */
        uint32_t    u1PerfOptEn : 1;    /**< Bit  13    - PerfOptEn: Performance Optimization Enable. */
        uint32_t    u17Rsvd0 : 18;      /**< Bits 31:14 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    u32;
} IOMMU_PERF_OPT_CTRL_T;
AssertCompileSize(IOMMU_PERF_OPT_CTRL_T, 4);

/**
 * XT (x2APIC) IOMMU General Interrupt Control Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u2Rsvd0 : 2;                    /**< Bits 1:0   - Reserved.*/
        uint32_t    u1X2ApicIntrDstMode : 1;        /**< Bit  2     - Destination Mode for general interrupt.*/
        uint32_t    u4Rsvd0 : 4;                    /**< Bits 7:3   - Reserved.*/
        uint32_t    u24X2ApicIntrDstLo : 24;        /**< Bits 31:8  - Destination for general interrupt (Lo).*/
        uint32_t    u8X2ApicIntrVector : 8;         /**< Bits 39:32 - Vector for general interrupt.*/
        uint32_t    u1X2ApicIntrDeliveryMode : 1;   /**< Bit  40    - Delivery Mode for general interrupt.*/
        uint32_t    u15Rsvd0 : 15;                  /**< Bits 55:41 - Reserved.*/
        uint32_t    u7X2ApicIntrDstHi : 7;          /**< Bits 63:56 - Destination for general interrupt (Hi) .*/
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_XT_GEN_INTR_CTRL_T;
AssertCompileSize(IOMMU_XT_GEN_INTR_CTRL_T, 8);

/**
 * XT (x2APIC) IOMMU General Interrupt Control Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u2Rsvd0 : 2;                    /**< Bits 1:0   - Reserved.*/
        uint32_t    u1X2ApicIntrDstMode : 1;        /**< Bit  2     - Destination Mode for the interrupt.*/
        uint32_t    u4Rsvd0 : 4;                    /**< Bits 7:3   - Reserved.*/
        uint32_t    u24X2ApicIntrDstLo : 24;        /**< Bits 31:8  - Destination for the interrupt (Lo).*/
        uint32_t    u8X2ApicIntrVector : 8;         /**< Bits 39:32 - Vector for the interrupt.*/
        uint32_t    u1X2ApicIntrDeliveryMode : 1;   /**< Bit  40    - Delivery Mode for the interrupt.*/
        uint32_t    u15Rsvd0 : 15;                  /**< Bits 55:41 - Reserved.*/
        uint32_t    u7X2ApicIntrDstHi : 7;          /**< Bits 63:56 - Destination for the interrupt (Hi) .*/
    } n;
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_XT_INTR_CTRL_T;
AssertCompileSize(IOMMU_XT_INTR_CTRL_T, 8);

/**
 * XT (x2APIC) IOMMU PPR Interrupt Control Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to IOMMU_XT_INTR_CTRL_T.
 */
typedef IOMMU_XT_INTR_CTRL_T    IOMMU_XT_PPR_INTR_CTRL_T;

/**
 * XT (x2APIC) IOMMU GA (Guest Address) Log Control Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to IOMMU_XT_INTR_CTRL_T.
 */
typedef IOMMU_XT_INTR_CTRL_T    IOMMU_XT_GALOG_INTR_CTRL_T;

/**
 * IOMMU Reserved Register (MMIO).
 * In accordance with the AMD spec.
 * This register is reserved for hardware use (although RW?).
 */
typedef uint64_t    IOMMU_RSVD_REG_T;

/**
 * Command Buffer Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u4Rsvd0 : 4;    /**< Bits 3:0   - Reserved. */
        uint32_t    u15Ptr : 15;    /**< Bits 18:4  - Buffer pointer. */
        uint32_t    u13Rsvd0 : 13;  /**< Bits 31:19 - Reserved. */
        uint32_t    u32Rsvd0;       /**< Bits 63:32 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} CMD_BUF_HEAD_PTR_T;
AssertCompileSize(CMD_BUF_HEAD_PTR_T, 8);

/**
 * Command Buffer Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T    CMD_BUF_TAIL_PTR_T;

/**
 * Event Log Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T    EVT_LOG_HEAD_PTR_T;

/**
 * Event Log Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T    EVT_LOG_TAIL_PTR_T;

/**
 * IOMMU Status Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u1EvtOverflow : 1;          /**< Bit  0     - EventOverflow: Event log overflow. */
        uint32_t    u1EvtLogIntr : 1;           /**< Bit  1     - EventLogInt: Event log interrupt. */
        uint32_t    u1CompWaitIntr : 1;         /**< Bit  2     - ComWaitInt: Completion wait interrupt . */
        uint32_t    u1EvtLogRunning : 1;        /**< Bit  3     - EventLogRun: Event logging is running. */
        uint32_t    u1CmdBufRunning : 1;        /**< Bit  4     - CmdBufRun: Command buffer is running. */
        uint32_t    u1PprOverflow : 1;          /**< Bit  5     - PprOverflow: Peripheral Page Request Log (PPR) overflow. */
        uint32_t    u1PprIntr : 1;              /**< Bit  6     - PprInt: PPR interrupt. */
        uint32_t    u1PprLogRunning : 1;        /**< Bit  7     - PprLogRun: PPR logging is running. */
        uint32_t    u1GstLogRunning : 1;        /**< Bit  8     - GALogRun: Guest virtual-APIC logging is running. */
        uint32_t    u1GstLogOverflow : 1;       /**< Bit  9     - GALOverflow: Guest virtual-APIC log overflow. */
        uint32_t    u1GstLogIntr : 1;           /**< Bit  10    - GAInt: Guest virtual-APIC log interrupt. */
        uint32_t    u1PprOverflowB : 1;         /**< Bit  11    - PprOverflowB: PPR log B overflow. */
        uint32_t    u1PprLogActive : 1;         /**< Bit  12    - PprLogActive: PPR log A is active. */
        uint32_t    u2Rsvd0 : 2;                /**< Bits 14:13 - Reserved. */
        uint32_t    u1EvtOverflowB : 1;         /**< Bit  15    - EvtOverflowB: Event log B overflow. */
        uint32_t    u1EvtLogActive : 1;         /**< Bit  16    - EvtLogActive: Event log A active. */
        uint32_t    u1PprOverflowEarlyB : 1;    /**< Bit  17    - PprOverflowEarlyB: PPR log B overflow early warning. */
        uint32_t    u1PprOverflowEarly : 1;     /**< Bit  18    - PprOverflowEarly: PPR log overflow early warning. */
        uint32_t    u13Rsvd0 : 13;              /**< Bits 31:19 - Reserved. */
        uint32_t    u32Rsvd0;                   /**< Bits 63:32 - Reserved . */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} IOMMU_STATUS_T;
AssertCompileSize(IOMMU_STATUS_T, 8);

/**
 * PPR Log Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      PPR_LOG_HEAD_PTR_T;

/**
 * PPR Log Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      PPR_LOG_TAIL_PTR_T;

/**
 * Guest Virtual-APIC Log Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u2Rsvd0 : 2;            /**< Bits  2:0  - Reserved. */
        uint32_t    u12GALogPtr : 12;       /**< Bits 15:3  - Guest Virtual-APIC Log Head or Tail Pointer. */
        uint32_t    u16Rsvd0 : 16;          /**< Bits 31:16 - Reserved. */
        uint32_t    u32Rsvd0;               /**< Bits 63:32 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} GALOG_HEAD_PTR_T;
AssertCompileSize(GALOG_HEAD_PTR_T, 8);

/**
 * Guest Virtual-APIC Log Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to GALOG_HEAD_PTR_T.
 */
typedef GALOG_HEAD_PTR_T        GALOG_TAIL_PTR_T;

/**
 * PPR Log B Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      PPR_LOG_B_HEAD_PTR_T;

/**
 * PPR Log B Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      PPR_LOG_B_TAIL_PTR_T;

/**
 * Event Log B Head Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      EVT_LOG_B_HEAD_PTR_T;

/**
 * Event Log B Tail Pointer Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to CMD_BUF_HEAD_PTR_T.
 */
typedef CMD_BUF_HEAD_PTR_T      EVT_LOG_B_TAIL_PTR_T;

/**
 * PPR Log Auto Response Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u3AutoRespCode : 4;     /**< Bits 3:0   - PprAutoRespCode: PPR log Auto Response Code. */
        uint32_t    u1AutoRespMaskGen : 1;  /**< Bit  4     - PprAutoRespMaskGn: PPR log Auto Response Mask Gen. */
        uint32_t    u27Rsvd0 : 27;          /**< Bits 31:5  - Reserved. */
        uint32_t    u32Rsvd0;               /**< Bits 63:32 - Reserved.*/
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} PPR_LOG_AUTO_RESP_T;
AssertCompileSize(PPR_LOG_AUTO_RESP_T, 8);

/**
 * PPR Log Overflow Early Indicator Register (MMIO).
 * In accordance with the AMD spec.
 */
typedef union
{
    struct
    {
        uint32_t    u15Threshold : 15;  /**< Bits 14:0  - PprOvrflwEarlyThreshold: Overflow early indicator threshold. */
        uint32_t    u15Rsvd0 : 15;      /**< Bits 29:15 - Reserved. */
        uint32_t    u1IntrEn : 1;       /**< Bit  30    - PprOvrflwEarlyIntEn: Overflow early indicator interrupt enable. */
        uint32_t    u1Enable : 1;       /**< Bit  31    - PprOvrflwEarlyEn: Overflow early indicator enable. */
        uint32_t    u32Rsvd0;           /**< Bits 63:32 - Reserved. */
    } n;
    /** The 32-bit unsigned integer view. */
    uint32_t    au32[2];
    /** The 64-bit unsigned integer view. */
    uint64_t    u64;
} PPR_LOG_OVERFLOW_EARLY_T;
AssertCompileSize(PPR_LOG_OVERFLOW_EARLY_T, 8);

/**
 * PPR Log B Overflow Early Indicator Register (MMIO).
 * In accordance with the AMD spec.
 * Currently identical to PPR_LOG_OVERFLOW_EARLY_T.
 */
typedef PPR_LOG_OVERFLOW_EARLY_T        PPR_LOG_B_OVERFLOW_EARLY_T;


/**
 * The shared IOMMU device state.
 */
typedef struct IOMMU
{
    /** Whether this IOMMU is at the top of the PCI tree hierarchy or not. */
    bool                        fRootComplex;
    /** Alignment padding. */
    bool                        afPadding[3];
    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

    /** @name MMIO: Control and status registers.
     * @{ */
    DEV_TAB_BAR_T               DevTabBaseAddr;     /**< Device table base address register. */
    CMD_BUF_BAR_T               CmdBufBaseAddr;     /**< Command buffer base address register. */
    EVT_LOG_BAR_T               EvtLogBaseAddr;     /**< Event log base address register. */
    IOMMU_CTRL_T                IommuCtrl;          /**< IOMMU control register. */
    IOMMU_EXCL_BASE_T           ExclBase;           /**< IOMMU exclusion base register. */
    IOMMU_EXCL_RANGE_LIMIT_T    ExclRangeLimit;     /**< IOMMU exclusion range limit. */
    IOMMU_EFR_T                 ExtFeat;            /**< IOMMU extended feature register. */
    /** @} */

    /** @name MMIO: PPR Log registers.
     * @{ */
    PPR_LOG_BAR_T               PprLogBaseAddr;     /**< PPR Log base address register. */
    IOMMU_HW_EVT_HI_T           IommuHwEvtHi;       /**< IOMMU hardware event register (Hi). */
    IOMMU_HW_EVT_LO_T           IommuHwEvtLo;       /**< IOMMU hardware event register (Lo). */
    IOMMU_HW_EVT_STATUS_T       IommuHwEvtStatus;   /**< IOMMU hardware event status. */
    /** @} */

    /** @todo IOMMU: SMI filter. */

    /** @name MMIO: Guest Virtual-APIC Log registers.
     * @{ */
    GALOG_BAR_T                 GALogBaseAddr;      /**< Guest Virtual-APIC Log base address register. */
    GALOG_TAIL_ADDR_T           GALogTailAddr;      /**< Guest Virtual-APIC Log Tail address register. */
    /** @} */

    /** @name MMIO: Alternate PPR and Event Log registers.
     *  @{ */
    PPR_LOG_B_BAR_T             PprLogBBaseAddr;    /**< PPR Log B base address register. */
    EVT_LOG_B_BAR_T             EvtLogBBaseAddr;    /**< Event Log B base address register. */
    /** @} */

    /** @name MMIO: Device table segment registers.
     * @{ */
    DEV_TAB_SEG_BAR_T           DevTabSeg[7];       /**< Device Table Segment base address register. */
    /** @} */

    /** @name MMIO: Device-specific feature registers.
     * @{ */
    DEV_SPECIFIC_FEAT_T         DevSpecificFeat;    /**< Device-specific feature extension register (DSFX). */
    DEV_SPECIFIC_CTRL_T         DevSpecificCtrl;    /**< Device-specific control extension register (DSCX). */
    DEV_SPECIFIC_STATUS_T       DevSpecificStatus;  /**< Device-specific status extension register (DSSX). */
    /** @} */

    /** @name MMIO: MSI Capability Block registers.
     * @{ */
    MSI_MISC_INFO_T             MsiMiscInfo;        /**< MSI Misc. info registers / MSI Vector registers. */
    MSI_CAP_HDR_T               MsiCapHdr;          /**< MSI Capability header register. */
    MSI_ADDR_T                  MsiAddr;            /**< MSI Address register.*/
    MSI_DATA_T                  MsiData;            /**< MSI Data register. */
    MSI_MAP_CAP_HDR_T           MsiMapCapHdr;       /**< MSI Mapping capability header register. */
    /** @} */

    /** @name MMIO: Performance Optimization Control registers.
     *  @{ */
    IOMMU_PERF_OPT_CTRL_T       IommuPerfOptCtrl;   /**< IOMMU Performance optimization control register. */
    /** @} */

    /** @name MMIO: x2APIC Control registers.
     * @{ */
    IOMMU_XT_GEN_INTR_CTRL_T    IommuXtGenIntrCtrl;     /**< IOMMU X2APIC General interrupt control register. */
    IOMMU_XT_PPR_INTR_CTRL_T    IommuXtPprIntrCtrl;     /**< IOMMU X2APIC PPR interrupt control register. */
    IOMMU_XT_GALOG_INTR_CTRL_T  IommuXtGALogIntrCtrl;   /**< IOMMU X2APIC Guest Log interrupt control register. */
    /** @} */

    /** @todo MARC registers. */

    /** @name MMIO: Reserved register.
     *  @{ */
    IOMMU_RSVD_REG_T            IommuRsvdReg;       /**< IOMMU Reserved Register. */
    /** @} */

    /** @name MMIO: Command and Event Log pointer registers.
     * @{ */
    CMD_BUF_HEAD_PTR_T          CmdBufHeadPtr;      /**< Command buffer head pointer register. */
    CMD_BUF_TAIL_PTR_T          CmdBufTailPtr;      /**< Command buffer tail pointer register. */
    EVT_LOG_HEAD_PTR_T          EvtLogHeadPtr;      /**< Event log head pointer register. */
    EVT_LOG_TAIL_PTR_T          EvtLogTailPtr;      /**< Event log tail pointer register. */
    /** @} */

    /** @name MMIO: Command and Event Status register.
     * @{ */
    IOMMU_STATUS_T              IommuStatus;        /**< IOMMU status register. */
    /** @} */

    /** @name MMIO: PPR Log Head and Tail pointer registers.
     * @{ */
    PPR_LOG_HEAD_PTR_T          PprLogHeadPtr;      /**< IOMMU PPR log head pointer register. */
    PPR_LOG_TAIL_PTR_T          PprLogTailPtr;      /**< IOMMU PPR log tail pointer register. */
    /** @} */

    /** @name MMIO: Guest Virtual-APIC Log Head and Tail pointer registers.
     * @{ */
    GALOG_HEAD_PTR_T            GALogHeadPtr;       /**< Guest Virtual-APIC log head pointer register. */
    GALOG_TAIL_PTR_T            GALogTailPtr;       /**< Guest Virtual-APIC log tail pointer register. */
    /** @} */

    /** @name MMIO: PPR Log B Head and Tail pointer registers.
     *  @{ */
    PPR_LOG_B_HEAD_PTR_T        PprLogBHeadPtr;     /**< PPR log B head pointer register. */
    PPR_LOG_B_TAIL_PTR_T        PprLogBTailPtr;     /**< PPR log B tail pointer register. */
    /** @} */

    /** @name MMIO: Event Log B Head and Tail pointer registers.
     * @{ */
    EVT_LOG_B_HEAD_PTR_T        EvtLogBHeadPtr;     /**< Event log B head pointer register. */
    EVT_LOG_B_TAIL_PTR_T        EvtLogBTailPtr;     /**< Event log B tail pointer register. */
    /** @} */

    /** @name MMIO: PPR Log Overflow protection registers.
     * @{ */
    PPR_LOG_AUTO_RESP_T         PprLogAutoResp;         /**< PPR Log Auto Response register. */
    PPR_LOG_OVERFLOW_EARLY_T    PprLogOverflowEarly;    /**< PPR Log Overflow Early Indicator register. */
    PPR_LOG_B_OVERFLOW_EARLY_T  PprLogBOverflowEarly;   /**< PPR Log B Overflow Early Indicator register. */
    /** @} */

    /** @todo IOMMU: IOMMU Event counter registers. */
} IOMMU;
/** Pointer to the IOMMU device state. */
typedef struct IOMMU *PIOMMU;

/**
 * The ring-3 IOMMU device state.
 */
typedef struct IOMMUR3
{
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPR3     pIommuHlp;
} IOMMUR3;
/** Pointer to the ring-3 IOMMU device state. */
typedef IOMMUR3 *PIOMMUR3;

/**
 * The ring-0 IOMMU device state.
 */
typedef struct IOMMUR0
{
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPR0     pIommuHlp;
} IOMMUR0;
/** Pointer to the ring-0 IOMMU device state. */
typedef IOMMUR0 *PIOMMUR0;

/**
 * The raw-mode IOMMU device state.
 */
typedef struct IOMMURC
{
    /** The IOMMU helpers. */
    PCPDMIOMMUHLPRC     pIommuHlp;
} IOMMURC;
/** Pointer to the raw-mode IOMMU device state. */
typedef IOMMURC *PIOMMURC;

/** The IOMMU device state for the current context. */
typedef CTX_SUFF(IOMMU) IOMMUCC;
/** Pointer to the IOMMU device state for the current context. */
typedef CTX_SUFF(PIOMMU) PIOMMUCC;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    /** @todo IOMMU: MMIO write. */
    RT_NOREF5(pDevIns, pvUser, off, pv, cb);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuAmdMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    /** @todo IOMMU: MMIO read. */
    RT_NOREF5(pDevIns, pvUser, off, pv, cb);
    return VERR_NOT_IMPLEMENTED;
}


# ifdef IN_RING3
/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) iommuAmdR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    /** @todo IOMMU: Save state. */
    RT_NOREF2(pDevIns, pSSM);
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) iommuAmdR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /** @todo IOMMU: Load state. */
    RT_NOREF4(pDevIns, pSSM, uVersion, uPass);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuAmdR3Reset(PPDMDEVINS pDevIns)
{
    NOREF(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuAmdR3Destruct(PPDMDEVINS pDevIns)
{
    NOREF(pDevIns);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) iommuAmdR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    NOREF(iInstance);

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOMMU          pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUCC        pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUCC);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    int             rc;
    LogFlowFunc(("\n"));

    NOREF(pThisCC); /** @todo IOMMU: populate CC data. */

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "RootComplex|MmioBase", "");

    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "RootComplex", &pThis->fRootComplex, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("IOMMU: Failed to query \"RootComplex\""));

    uint64_t u64MmioBase;
    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioBase", &u64MmioBase, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("IOMMU: Failed to query \"MmioBase\""));
    /* Must be 16KB aligned when we don't support IOMMU performance counters.  */
    if (u64MmioBase & 0x3fff)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("IOMMU: \"MmioBase\" must be 16 KB aligned"));

    /*
     * Initialize the PCI configuration space.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    uint8_t const offCapHdr       = 0x40;
    uint8_t const offBaseAddrLo   = offCapHdr + 0x4;
    uint8_t const offBaseAddrHi   = offCapHdr + 0x8;
    uint8_t const offRange        = offCapHdr + 0xc;
    uint8_t const offMiscInfo0    = offCapHdr + 0x10;
    uint8_t const offMiscInfo1    = offCapHdr + 0x14;
    uint8_t const offMsiCapHdr    = offCapHdr + 0x24;
    uint8_t const offMsiAddrLo    = offCapHdr + 0x28;
    uint8_t const offMsiAddrHi    = offCapHdr + 0x2c;
    uint8_t const offMsiData      = offCapHdr + 0x30;
    uint8_t const offMsiMapCapHdr = offCapHdr + 0x34;

    /* Header. */
    PDMPciDevSetVendorId(pPciDev,           IOMMU_PCI_VENDOR_ID);   /* RO - AMD */
    PDMPciDevSetDeviceId(pPciDev,           IOMMU_PCI_DEVICE_ID);   /* RO - VirtualBox IOMMU device */
    PDMPciDevSetCommand(pPciDev,            0);                     /* RW - Command */
    PDMPciDevSetStatus(pPciDev,             0x5);                   /* RW - Status - CapList supported */
    PDMPciDevSetRevisionId(pPciDev,         IOMMU_PCI_REVISION_ID); /* RO - VirtualBox specific device implementation revision */
    PDMPciDevSetClassBase(pPciDev,          0x08);                  /* RO - System Base Peripheral */
    PDMPciDevSetClassSub(pPciDev,           0x06);                  /* RO - IOMMU */
    PDMPciDevSetClassProg(pPciDev,          0x00);                  /* RO - IOMMU Programming interface */
    PDMPciDevSetHeaderType(pPciDev,         0x00);                  /* RO - Single function, type 0. */
    PDMPciDevSetSubSystemId(pPciDev,        IOMMU_PCI_DEVICE_ID);   /* RO - AMD */
    PDMPciDevSetSubSystemVendorId(pPciDev,  IOMMU_PCI_VENDOR_ID);   /* RO - VirtualBox IOMMU device */
    PDMPciDevSetCapabilityList(pPciDev,     offCapHdr);             /* RO - Offset into capability registers. */
    PDMPciDevSetInterruptPin(pPciDev,       0x01);                  /* RO - INTA#. */
    PDMPciDevSetInterruptLine(pPciDev,      0x00);                  /* RW - For software compatibility; no effect on hardware. */

    /* Capability Header. */
    PDMPciDevSetDWord(pPciDev, offCapHdr,
                        RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_ID,     0xf)             /* RO - Secure Device capability block */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_PTR,    offMsiCapHdr)    /* RO - Offset to next capability block */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_TYPE,   0x3)             /* RO - IOMMU capability block */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_REV,    0x1)             /* RO - IOMMU interface revision */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_IOTLB_SUP,  0x0)             /* RO - Remote IOTLB support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_HT_TUNNEL,  0x0)             /* RO - HyperTransport Tunnel support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_NP_CACHE,   0x0)             /* RO - Cache Not-present page table entries */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_EFR_SUP,    0x1)             /* RO - Extended Feature Register support */
                      | RT_BF_MAKE(IOMMU_BF_CAPHDR_CAP_EXT,    0x1));           /* RO - Misc. Information Register support */

    /* Base Address Low Register. */
    PDMPciDevSetDWord(pPciDev, offBaseAddrLo,
                        RT_BF_MAKE(IOMMU_BF_BASEADDR_LO_ENABLE, 0x1)                    /* RW1S - Enable */
                      | RT_BF_MAKE(IOMMU_BF_BASEADDR_LO_ADDR,   (u64MmioBase >> 14)));  /* RO - Base address (Lo) */

    /* Base Address High Register. */
    PDMPciDevSetDWord(pPciDev, offBaseAddrHi, RT_HI_U32(u64MmioBase));      /* RO - Base address (Hi) */

    /* IOMMU Range Register. */
    PDMPciDevSetDWord(pPciDev, offRange, 0x0);                              /* RO - Range register. */

    /* Misc. Information Register 0. */
    PDMPciDevSetDWord(pPciDev, offMiscInfo0,
                        RT_BF_MAKE(IOMMU_BF_MISCINFO_0_MSI_NUM,     0x0)    /* RO - MSI number */
                      | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_GVA_SIZE,    0x2)    /* RO - Guest Virt. Addr size (2=48 bits) */
                      | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_PA_SIZE,     0x30)   /* RO - Physical Addr size (48 bits) */
                      | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_VA_SIZE,     0x40)   /* RO - Virt. Addr size (64 bits) */
                      | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_HT_ATS_RESV, 0x0)    /* RW - HT ATS reserved */
                      | RT_BF_MAKE(IOMMU_BF_MISCINFO_0_MSI_NUM_PPR, 0x0));  /* RW - PPR interrupt number */

    /* Misc. Information Register 1. */
    PDMPciDevSetDWord(pPciDev, offMiscInfo1, 0);

    /* MSI Capability Header register. */
    PDMPciDevSetDWord(pPciDev, offMsiCapHdr,
                        RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_CAP_ID,       0x5)             /* RO - Capability ID. */
                      | RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_CAP_PTR,      offMsiMapCapHdr) /* RO - Offset to mapping capability block */
                      | RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_EN,           0x0)             /* RW - MSI capability enable */
                      | RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_MULTMESS_CAP, 0x0)             /* RO - MSI multi-message capability */
                      | RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_MULTMESS_EN,  0x0)             /* RW - MSI multi-message enable */
                      | RT_BF_MAKE(IOMMU_BF_MSI_CAPHDR_64BIT_EN,     0x1));           /* RO - MSI 64-bit enable */

    /* MSI Address Lo. */
    PDMPciDevSetDWord(pPciDev, offMsiAddrLo, 0);                            /* RW - MSI message address (Lo). */

    /* MSI Address Hi. */
    PDMPciDevSetDWord(pPciDev, offMsiAddrHi, 0);                            /* RW - MSI message address (Hi). */

    /* MSI Data. */
    PDMPciDevSetDWord(pPciDev, offMsiData, 0);                              /* RW - MSI data. */

    /* MSI Mapping Capability Header register. */
    PDMPciDevSetDWord(pPciDev, offMsiMapCapHdr,
                        RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_ID,   0x8)       /* RO - Capability ID */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_PTR,  0x0)       /* RO - Offset to next capability (NULL) */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_EN,       0x1)       /* RO - MSI mapping capability enable */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_FIXED,    0x1)       /* RO - MSI mapping range is fixed */
                      | RT_BF_MAKE(IOMMU_BF_MSI_MAP_CAPHDR_CAP_TYPE, 0x15));    /* RO - MSI mapping capability */

    /*
     * Register the PCI device with PDM.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pDevIns->apPciDevs[0]);
    AssertRCReturn(rc, rc);

    /*
     * Map MMIO registers.
     */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, u64MmioBase, _16K, iommuAmdMmioWrite, iommuAmdMmioRead,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   "IOMMU-AMD", &pThis->hMmio);
    AssertRCReturn(rc, rc);

    /*
     * Register saved state.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, IOMMU_SAVED_STATE_VERSION, sizeof(IOMMU), NULL,
                                NULL, NULL, NULL,
                                NULL, iommuAmdR3SaveExec, NULL,
                                NULL, iommuAmdR3LoadExec, NULL);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

# else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) iommuAmdRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
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
    /* .fClass = */                 PDM_DEVREG_CLASS_BUS_ISA,   /* Instantiate after PDM_DEVREG_CLASS_BUS_PCI */
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

