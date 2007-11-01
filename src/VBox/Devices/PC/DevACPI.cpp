/* $Id$ */
/** @file
 * Advanced Configuration and Power Interface (ACPI) Device.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_ACPI

#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#ifdef IN_RING3
#include <iprt/alloc.h>
#include <iprt/string.h>
#endif /* IN_RING3 */

#include "Builtins.h"

#ifdef  LOG_ENABLED
#define DEBUG_ACPI
#endif

/* the compiled DSL */
#if defined(IN_RING3) && !defined(VBOX_DEVICE_STRUCT_TESTCASE)
#include <vboxaml.hex>
#endif /* !IN_RING3 */

#define IO_READ_PROTO(name)                                             \
    PDMBOTHCBDECL(int) name (PPDMDEVINS pDevIns, void *pvUser,          \
                             RTIOPORT Port, uint32_t *pu32, unsigned cb)

#define IO_WRITE_PROTO(name)                                            \
    PDMBOTHCBDECL(int) name (PPDMDEVINS pDevIns, void *pvUser,          \
                             RTIOPORT Port, uint32_t u32, unsigned cb)

#define DEBUG_HEX       0x3000
#define DEBUG_CHR       0x3001

#define PM_TMR_FREQ     3579545
#define PM1a_EVT_BLK    0x00004000
#define PM1b_EVT_BLK    0x00000000              /**< not supported */
#define PM1a_CTL_BLK    0x00004004
#define PM1b_CTL_BLK    0x00000000              /**< not supported */
#define PM2_CTL_BLK     0x00000000              /**< not supported */
#define PM_TMR_BLK      0x00004008
#define GPE0_BLK        0x00004020
#define GPE1_BLK        0x00000000              /**< not supported */
#define BAT_INDEX       0x00004040
#define BAT_DATA        0x00004044
#define SYSI_INDEX      0x00004048
#define SYSI_DATA       0x0000404c
#define ACPI_RESET_BLK  0x00004050

/* PM1x status register bits */
#define TMR_STS         RT_BIT(0)
#define RSR1_STS        (RT_BIT(1) | RT_BIT(2) | RT_BIT(3))
#define BM_STS          RT_BIT(4)
#define GBL_STS         RT_BIT(5)
#define RSR2_STS        (RT_BIT(6) | RT_BIT(7))
#define PWRBTN_STS      RT_BIT(8)
#define SLPBTN_STS      RT_BIT(9)
#define RTC_STS         RT_BIT(10)
#define IGN_STS         RT_BIT(11)
#define RSR3_STS        (RT_BIT(12) | RT_BIT(13) | RT_BIT(14))
#define WAK_STS         RT_BIT(15)
#define RSR_STS         (RSR1_STS | RSR2_STS | RSR3_STS)

/* PM1x enable register bits */
#define TMR_EN          RT_BIT(0)
#define RSR1_EN         (RT_BIT(1) | RT_BIT(2) | RT_BIT(3) | RT_BIT(4))
#define GBL_EN          RT_BIT(5)
#define RSR2_EN         (RT_BIT(6) | RT_BIT(7))
#define PWRBTN_EN       RT_BIT(8)
#define SLPBTN_EN       RT_BIT(9)
#define RTC_EN          RT_BIT(10)
#define RSR3_EN         (RT_BIT(11) | RT_BIT(12) | RT_BIT(13) | RT_BIT(14) | RT_BIT(15))
#define RSR_EN          (RSR1_EN | RSR2_EN | RSR3_EN)
#define IGN_EN          0

/* PM1x control register bits */
#define SCI_EN          RT_BIT(0)
#define BM_RLD          RT_BIT(1)
#define GBL_RLS         RT_BIT(2)
#define RSR1_CNT        (RT_BIT(3) | RT_BIT(4) | RT_BIT(5) | RT_BIT(6) | RT_BIT(7) | RT_BIT(8))
#define IGN_CNT         RT_BIT(9)
#define SLP_TYPx_SHIFT  10
#define SLP_TYPx_MASK    7
#define SLP_EN          RT_BIT(13)
#define RSR2_CNT        (RT_BIT(14) | RT_BIT(15))
#define RSR_CNT         (RSR1_CNT | RSR2_CNT)

#define GPE0_BATTERY_INFO_CHANGED RT_BIT(0)

enum
{
    BAT_STATUS_STATE                    = 0x00, /**< BST battery state */
    BAT_STATUS_PRESENT_RATE             = 0x01, /**< BST battery present rate */
    BAT_STATUS_REMAINING_CAPACITY       = 0x02, /**< BST battery remaining capacity */
    BAT_STATUS_PRESENT_VOLTAGE          = 0x03, /**< BST battery present voltage */
    BAT_INFO_UNITS                      = 0x04, /**< BIF power unit */
    BAT_INFO_DESIGN_CAPACITY            = 0x05, /**< BIF design capacity */
    BAT_INFO_LAST_FULL_CHARGE_CAPACITY  = 0x06, /**< BIF last full charge capacity */
    BAT_INFO_TECHNOLOGY                 = 0x07, /**< BIF battery technology */
    BAT_INFO_DESIGN_VOLTAGE             = 0x08, /**< BIF design voltage */
    BAT_INFO_DESIGN_CAPACITY_OF_WARNING = 0x09, /**< BIF design capacity of warning */
    BAT_INFO_DESIGN_CAPACITY_OF_LOW     = 0x0A, /**< BIF design capacity of low */
    BAT_INFO_CAPACITY_GRANULARITY_1     = 0x0B, /**< BIF battery capacity granularity 1 */
    BAT_INFO_CAPACITY_GRANULARITY_2     = 0x0C, /**< BIF battery capacity granularity 2 */
    BAT_DEVICE_STATUS                   = 0x0D, /**< STA device status */
    BAT_POWER_SOURCE                    = 0x0E, /**< PSR power source */
    BAT_INDEX_LAST
};

enum
{
    SYSTEM_INFO_INDEX_MEMORY_LENGTH     = 0,
    SYSTEM_INFO_INDEX_USE_IOAPIC        = 1,
    SYSTEM_INFO_INDEX_LAST              = 2,
    SYSTEM_INFO_INDEX_INVALID           = 0x80,
    SYSTEM_INFO_INDEX_VALID             = 0x200
};

#define AC_OFFLINE                              0
#define AC_ONLINE                               1

#define BAT_TECH_PRIMARY                        1
#define BAT_TECH_SECONDARY                      2

#define BAT_STATUS_DISCHARGING_MASK             RT_BIT(0)
#define BAT_STATUS_CHARGING_MASK                RT_BIT(1)
#define BAT_STATUS_CRITICAL_MASK                RT_BIT(2)

#define STA_DEVICE_PRESENT_MASK                 RT_BIT(0)
#define STA_DEVICE_ENABLED_MASK                 RT_BIT(1)
#define STA_DEVICE_SHOW_IN_UI_MASK              RT_BIT(2)
#define STA_DEVICE_FUNCTIONING_PROPERLY_MASK    RT_BIT(3)
#define STA_BATTERY_PRESENT_MASK                RT_BIT(4)

struct ACPIState
{
    PCIDevice           dev;
    uint16_t            pm1a_en;
    uint16_t            pm1a_sts;
    uint16_t            pm1a_ctl;
    uint16_t            Alignment0;
    int64_t             pm_timer_initial;
    R3R0PTRTYPE(PTMTIMER) tsHC;
    GCPTRTYPE(PTMTIMER)   tsGC;

    uint32_t            gpe0_en;
    uint32_t            gpe0_sts;

    unsigned int        uBatteryIndex;
    uint32_t            au8BatteryInfo[13];

    unsigned int        uSystemInfoIndex;
    uint64_t            u64RamSize;

    /** Current ACPI S* state. We support S0 and S5 */
    uint32_t            uSleepState;
    uint8_t             au8RSDPPage[0x1000];
    /** This is a workaround for incorrect index field handling by Intels ACPICA.
     *  The system info _INI method writes to offset 0x200. We either observe a
     *  write request to index 0x80 (in that case we don't change the index) or a
     *  write request to offset 0x200 (in that case we divide the index value by
     *  4. Note that the _STA method is sometimes called prior to the _INI method
     *  (ACPI spec 6.3.7, _STA). See the special case for BAT_DEVICE_STATUS in
     *  acpiBatIndexWrite() for handling this. */
    uint8_t             u8IndexShift;
    uint8_t             u8UseIOApic;

    /** ACPI port base interface. */
    PDMIBASE            IBase;
    /** ACPI port interface. */
    PDMIACPIPORT        IACPIPort;
    /** Pointer to the device instance. */
    PPDMDEVINSR3        pDevIns;
    /** Pointer to the driver base interface */
    R3PTRTYPE(PPDMIBASE) pDrvBase;
    /** Pointer to the driver connector interface */
    R3PTRTYPE(PPDMIACPICONNECTOR) pDrv;
};

#pragma pack(1)

/** Generic Address Structure (see ACPIspec 3.0, 5.2.3.1) */
struct ACPIGENADDR
{
    uint8_t             u8AddressSpaceId;       /**< 0=sys, 1=IO, 2=PCICfg, 3=emb, 4=SMBus */
    uint8_t             u8RegisterBitWidth;     /**< size in bits of the given register */
    uint8_t             u8RegisterBitOffset;    /**< bit offset of register */
    uint8_t             u8AccessSize;           /**< 1=byte, 2=word, 3=dword, 4=qword */
    uint64_t            u64Address;             /**< 64-bit address of register */
};
AssertCompileSize(ACPIGENADDR, 12);

/** Root System Description Pointer */
struct ACPITBLRSDP
{
    uint8_t             au8Signature[8];        /**< 'RSD PTR ' */
    uint8_t             u8Checksum;             /**< checksum for the first 20 bytes */
    uint8_t             au8OemId[6];            /**< OEM-supplied identifier */
    uint8_t             u8Revision;             /**< revision number, currently 2 */
#define ACPI_REVISION   2                       /**< ACPI 3.0 */
    uint32_t            u32RSDT;                /**< phys addr of RSDT */
    uint32_t            u32Length;              /**< bytes of this table */
    uint64_t            u64XSDT;                /**< 64-bit phys addr of XSDT */
    uint8_t             u8ExtChecksum;          /**< checksum of entire table */
    uint8_t             u8Reserved[3];          /**< reserved */
};
AssertCompileSize(ACPITBLRSDP, 36);

/** System Description Table Header */
struct ACPITBLHEADER
{
    uint8_t             au8Signature[4];        /**< table identifier */
    uint32_t            u32Length;              /**< length of the table including header */
    uint8_t             u8Revision;             /**< revision number */
    uint8_t             u8Checksum;             /**< all fields inclusive this add to zero */
    uint8_t             au8OemId[6];            /**< OEM-supplied string */
    uint8_t             au8OemTabId[8];         /**< to identify the particular data table */
    uint32_t            u32OemRevision;         /**< OEM-supplied revision number */
    uint8_t             au8CreatorId[4];        /**< ID for the ASL compiler */
    uint32_t            u32CreatorRev;          /**< revision for the ASL compiler */
};
AssertCompileSize(ACPITBLHEADER, 36);

/** Root System Description Table */
struct ACPITBLRSDT
{
    ACPITBLHEADER       header;
    uint32_t            u32Entry[1];            /**< array of phys. addresses to other tables */
};
AssertCompileSize(ACPITBLRSDT, 40);

/** Extended System Description Table */
struct ACPITBLXSDT
{
    ACPITBLHEADER       header;
    uint64_t            u64Entry[1];            /**< array of phys. addresses to other tables */
};
AssertCompileSize(ACPITBLXSDT, 44);

/** Fixed ACPI Description Table */
struct ACPITBLFADT
{
    ACPITBLHEADER       header;
    uint32_t            u32FACS;                /**< phys. address of FACS */
    uint32_t            u32DSDT;                /**< phys. address of DSDT */
    uint8_t             u8IntModel;             /**< was eleminated in ACPI 2.0 */
#define INT_MODEL_DUAL_PIC        1             /**< for ACPI 2+ */
#define INT_MODEL_MULTIPLE_APIC   2
    uint8_t             u8PreferredPMProfile;   /**< preferred power management profile */
    uint16_t            u16SCIInt;              /**< system vector the SCI is wired in 8259 mode */
#define SCI_INT         9
    uint32_t            u32SMICmd;              /**< system port address of SMI command port */
#define SMI_CMD         0x0000442e
    uint8_t             u8AcpiEnable;           /**< SMICmd val to disable ownship of ACPIregs */
#define ACPI_ENABLE     0xa1
    uint8_t             u8AcpiDisable;          /**< SMICmd val to re-enable ownship of ACPIregs */
#define ACPI_DISABLE    0xa0
    uint8_t             u8S4BIOSReq;            /**< SMICmd val to enter S4BIOS state */
    uint8_t             u8PStateCnt;            /**< SMICmd val to assume processor performance
                                                     state control responsibility */
    uint32_t            u32PM1aEVTBLK;          /**< port addr of PM1a event regs block */
    uint32_t            u32PM1bEVTBLK;          /**< port addr of PM1b event regs block */
    uint32_t            u32PM1aCTLBLK;          /**< port addr of PM1a control regs block */
    uint32_t            u32PM1bCTLBLK;          /**< port addr of PM1b control regs block */
    uint32_t            u32PM2CTLBLK;           /**< port addr of PM2 control regs block */
    uint32_t            u32PMTMRBLK;            /**< port addr of PMTMR regs block */
    uint32_t            u32GPE0BLK;             /**< port addr of gen-purp event 0 regs block */
    uint32_t            u32GPE1BLK;             /**< port addr of gen-purp event 1 regs block */
    uint8_t             u8PM1EVTLEN;            /**< bytes decoded by PM1a_EVT_BLK. >= 4 */
    uint8_t             u8PM1CTLLEN;            /**< bytes decoded by PM1b_CNT_BLK. >= 2 */
    uint8_t             u8PM2CTLLEN;            /**< bytes decoded by PM2_CNT_BLK. >= 1 or 0 */
    uint8_t             u8PMTMLEN;              /**< bytes decoded by PM_TMR_BLK. ==4 */
    uint8_t             u8GPE0BLKLEN;           /**< bytes decoded by GPE0_BLK. %2==0 */
#define GPE0_BLK_LEN    2
    uint8_t             u8GPE1BLKLEN;           /**< bytes decoded by GPE1_BLK. %2==0 */
#define GPE1_BLK_LEN    0
    uint8_t             u8GPE1BASE;             /**< offset of GPE1 based events */
#define GPE1_BASE       0
    uint8_t             u8CSTCNT;               /**< SMICmd val to indicate OS supp for C states */
    uint16_t            u16PLVL2LAT;            /**< us to enter/exit C2. >100 => unsupported */
#define P_LVL2_LAT      101                     /**< C2 state not supported */
    uint16_t            u16PLVL3LAT;            /**< us to enter/exit C3. >1000 => unsupported */
#define P_LVL3_LAT      1001                    /**< C3 state not supported */
    uint16_t            u16FlushSize;           /**< # of flush strides to read to flush dirty
                                                     lines from any processors memory caches */
#define FLUSH_SIZE      0                       /**< Ignored if WBVIND set in FADT_FLAGS */
    uint16_t            u16FlushStride;         /**< cache line width */
#define FLUSH_STRIDE    0                       /**< Ignored if WBVIND set in FADT_FLAGS */
    uint8_t             u8DutyOffset;
    uint8_t             u8DutyWidth;
    uint8_t             u8DayAlarm;             /**< RTC CMOS RAM index of day-of-month alarm */
    uint8_t             u8MonAlarm;             /**< RTC CMOS RAM index of month-of-year alarm */
    uint8_t             u8Century;              /**< RTC CMOS RAM index of century */
    uint16_t            u16IAPCBOOTARCH;        /**< IA-PC boot architecture flags */
#define IAPC_BOOT_ARCH_LEGACY_DEV       RT_BIT(0)  /**< legacy devices present such as LPT
                                                     (COM too?) */
#define IAPC_BOOT_ARCH_8042             RT_BIT(1)  /**< legacy keyboard device present */
#define IAPC_BOOT_ARCH_NO_VGA           RT_BIT(2)  /**< VGA not present */
    uint8_t             u8Must0_0;              /**< must be 0 */
    uint32_t            u32Flags;               /**< fixed feature flags */
#define FADT_FL_WBINVD                  RT_BIT(0)  /**< emulation of WBINVD available */
#define FADT_FL_WBINVD_FLUSH            RT_BIT(1)
#define FADT_FL_PROC_C1                 RT_BIT(2)  /**< 1=C1 supported on all processors */
#define FADT_FL_P_LVL2_UP               RT_BIT(3)  /**< 1=C2 works on SMP and UNI systems */
#define FADT_FL_PWR_BUTTON              RT_BIT(4)  /**< 1=power button handled as ctrl method dev */
#define FADT_FL_SLP_BUTTON              RT_BIT(5)  /**< 1=sleep button handled as ctrl method dev */
#define FADT_FL_FIX_RTC                 RT_BIT(6)  /**< 0=RTC wake status in fixed register */
#define FADT_FL_RTC_S4                  RT_BIT(7)  /**< 1=RTC can wake system from S4 */
#define FADT_FL_TMR_VAL_EXT             RT_BIT(8)  /**< 1=TMR_VAL implemented as 32 bit */
#define FADT_FL_DCK_CAP                 RT_BIT(9)  /**< 0=system cannot support docking */
#define FADT_FL_RESET_REG_SUP           RT_BIT(10) /**< 1=system supports system resets */
#define FADT_FL_SEALED_CASE             RT_BIT(11) /**< 1=case is sealed */
#define FADT_FL_HEADLESS                RT_BIT(12) /**< 1=system cannot detect moni/keyb/mouse */
#define FADT_FL_CPU_SW_SLP              RT_BIT(13)
#define FADT_FL_PCI_EXT_WAK             RT_BIT(14) /**< 1=system supports PCIEXP_WAKE_STS */
#define FADT_FL_USE_PLATFORM_CLOCK      RT_BIT(15) /**< 1=system has ACPI PM timer */
#define FADT_FL_S4_RTC_STS_VALID        RT_BIT(16) /**< 1=RTC_STS flag is valid when waking from S4 */
#define FADT_FL_REMOVE_POWER_ON_CAPABLE RT_BIT(17) /**< 1=platform can remote power on */
#define FADT_FL_FORCE_APIC_CLUSTER_MODEL  RT_BIT(18)
#define FADT_FL_FORCE_APIC_PHYS_DEST_MODE RT_BIT(19)
    ACPIGENADDR         ResetReg;               /**< ext addr of reset register */
    uint8_t             u8ResetVal;             /**< ResetReg value to reset the system */
#define ACPI_RESET_REG_VAL  0x10
    uint8_t             au8Must0_1[3];          /**< must be 0 */
    uint64_t            u64XFACS;               /**< 64-bit phys address of FACS */
    uint64_t            u64XDSDT;               /**< 64-bit phys address of DSDT */
    ACPIGENADDR         X_PM1aEVTBLK;           /**< ext addr of PM1a event regs block */
    ACPIGENADDR         X_PM1bEVTBLK;           /**< ext addr of PM1b event regs block */
    ACPIGENADDR         X_PM1aCTLBLK;           /**< ext addr of PM1a control regs block */
    ACPIGENADDR         X_PM1bCTLBLK;           /**< ext addr of PM1b control regs block */
    ACPIGENADDR         X_PM2CTLBLK;            /**< ext addr of PM2 control regs block */
    ACPIGENADDR         X_PMTMRBLK;             /**< ext addr of PMTMR control regs block */
    ACPIGENADDR         X_GPE0BLK;              /**< ext addr of GPE1 regs block */
    ACPIGENADDR         X_GPE1BLK;              /**< ext addr of GPE1 regs block */
};
AssertCompileSize(ACPITBLFADT, 244);

/** Firmware ACPI Control Structure */
struct ACPITBLFACS
{
    uint8_t             au8Signature[4];        /**< 'FACS' */
    uint32_t            u32Length;              /**< bytes of entire FACS structure >= 64 */
    uint32_t            u32HWSignature;         /**< systems HW signature at last boot */
    uint32_t            u32FWVector;            /**< address of waking vector */
    uint32_t            u32GlobalLock;          /**< global lock to sync HW/SW */
    uint32_t            u32Flags;               /**< FACS flags */
    uint64_t            u64X_FWVector;          /**< 64-bit waking vector */
    uint8_t             u8Version;              /**< version of this table */
    uint8_t             au8Reserved[31];        /**< zero */
};
AssertCompileSize(ACPITBLFACS, 64);

/** Processor Local APIC Structure */
struct ACPITBLLAPIC
{
    uint8_t             u8Type;                 /**< 0 = LAPIC */
    uint8_t             u8Length;               /**< 8 */
    uint8_t             u8ProcId;               /**< processor ID */
    uint8_t             u8ApicId;               /**< local APIC ID */
    uint32_t            u32Flags;               /**< Flags */
#define LAPIC_ENABLED   0x1
};
AssertCompileSize(ACPITBLLAPIC, 8);

/** I/O APIC Structure */
struct ACPITBLIOAPIC
{
    uint8_t             u8Type;                 /**< 1 == I/O APIC */
    uint8_t             u8Length;               /**< 12 */
    uint8_t             u8IOApicId;             /**< I/O APIC ID */
    uint8_t             u8Reserved;             /**< 0 */
    uint32_t            u32Address;             /**< phys address to access I/O APIC */
    uint32_t            u32GSIB;                /**< global system interrupt number to start */
};
AssertCompileSize(ACPITBLIOAPIC, 12);

/** Multiple APIC Description Table */
struct ACPITBLMADT
{
    ACPITBLHEADER       header;
    uint32_t            u32LAPIC;               /**< local APIC address */
    uint32_t            u32Flags;               /**< Flags */
#define PCAT_COMPAT     0x1                     /**< system has also a dual-8259 setup */
    ACPITBLLAPIC        LApic;
    ACPITBLIOAPIC       IOApic;
};
AssertCompileSize(ACPITBLMADT, 64);

#pragma pack()


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
__BEGIN_DECLS
IO_READ_PROTO  (acpiPMTmrRead);
#ifdef IN_RING3
IO_READ_PROTO  (acpiPm1aEnRead);
IO_WRITE_PROTO (acpiPM1aEnWrite);
IO_READ_PROTO  (acpiPm1aStsRead);
IO_WRITE_PROTO (acpiPM1aStsWrite);
IO_READ_PROTO  (acpiPm1aCtlRead);
IO_WRITE_PROTO (acpiPM1aCtlWrite);
IO_WRITE_PROTO (acpiSmiWrite);
IO_WRITE_PROTO (acpiBatIndexWrite);
IO_READ_PROTO  (acpiBatDataRead);
IO_READ_PROTO  (acpiSysInfoDataRead);
IO_WRITE_PROTO (acpiSysInfoDataWrite);
IO_READ_PROTO  (acpiGpe0EnRead);
IO_WRITE_PROTO (acpiGpe0EnWrite);
IO_READ_PROTO  (acpiGpe0StsRead);
IO_WRITE_PROTO (acpiGpe0StsWrite);
IO_WRITE_PROTO (acpiResetWrite);
# ifdef DEBUG_ACPI
IO_WRITE_PROTO (acpiDhexWrite);
IO_WRITE_PROTO (acpiDchrWrite);
# endif
#endif
__END_DECLS

#ifdef IN_RING3

/* Simple acpiChecksum: all the bytes must add up to 0. */
static uint8_t acpiChecksum (const uint8_t * const data, uint32_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += data[i];
    return -sum;
}

static void acpiPrepareHeader (ACPITBLHEADER *header, const char au8Signature[4],
                               uint32_t u32Length, uint8_t u8Revision)
{
    memcpy(header->au8Signature, au8Signature, 4);
    header->u32Length             = RT_H2LE_U32(u32Length);
    header->u8Revision            = u8Revision;
    memcpy(header->au8OemId, "VBOX  ", 6);
    memcpy(header->au8OemTabId, "VBOX", 4);
    memcpy(header->au8OemTabId+4, au8Signature, 4);
    header->u32OemRevision        = RT_H2LE_U32(1);
    memcpy(header->au8CreatorId, "ASL ", 4);
    header->u32CreatorRev         = RT_H2LE_U32(0x61);
}

static void acpiWriteGenericAddr(ACPIGENADDR *g, uint8_t u8AddressSpaceId,
                                 uint8_t u8RegisterBitWidth, uint8_t u8RegisterBitOffset,
                                 uint8_t u8AccessSize, uint64_t u64Address)
{
    g->u8AddressSpaceId    = u8AddressSpaceId;
    g->u8RegisterBitWidth  = u8RegisterBitWidth;
    g->u8RegisterBitOffset = u8RegisterBitOffset;
    g->u8AccessSize        = u8AccessSize;
    g->u64Address          = RT_H2LE_U64(u64Address);
}

static void acpiPhyscpy (ACPIState *s, RTGCPHYS dst, const void * const src, size_t size)
{
    PDMDevHlpPhysWrite (s->pDevIns, dst, src, size);
}

/* Differentiated System Description Table (DSDT) */
static void acpiSetupDSDT (ACPIState *s, RTGCPHYS addr)
{
    acpiPhyscpy (s, addr, AmlCode, sizeof(AmlCode));
}

/* Firmware ACPI Control Structure (FACS) */
static void acpiSetupFACS (ACPIState *s, RTGCPHYS addr)
{
    ACPITBLFACS facs;

    memset (&facs, 0, sizeof(facs));
    memcpy (facs.au8Signature, "FACS", 4);
    facs.u32Length            = RT_H2LE_U32(sizeof(ACPITBLFACS));
    facs.u32HWSignature       = RT_H2LE_U32(0);
    facs.u32FWVector          = RT_H2LE_U32(0);
    facs.u32GlobalLock        = RT_H2LE_U32(0);
    facs.u32Flags             = RT_H2LE_U32(0);
    facs.u64X_FWVector        = RT_H2LE_U64(0);
    facs.u8Version            = 1;

    acpiPhyscpy (s, addr, (const uint8_t*)&facs, sizeof(facs));
}

/* Fixed ACPI Description Table (FADT aka FACP) */
static void acpiSetupFADT (ACPIState *s, RTGCPHYS addr, uint32_t facs_addr, uint32_t dsdt_addr)
{
    ACPITBLFADT fadt;

    memset (&fadt, 0, sizeof(fadt));
    acpiPrepareHeader (&fadt.header, "FACP", sizeof(fadt), 4);
    fadt.u32FACS              = RT_H2LE_U32(facs_addr);
    fadt.u32DSDT              = RT_H2LE_U32(dsdt_addr);
    fadt.u8IntModel           = INT_MODEL_DUAL_PIC;
    fadt.u8PreferredPMProfile = 0; /* unspecified */
    fadt.u16SCIInt            = RT_H2LE_U16(SCI_INT);
    fadt.u32SMICmd            = RT_H2LE_U32(SMI_CMD);
    fadt.u8AcpiEnable         = ACPI_ENABLE;
    fadt.u8AcpiDisable        = ACPI_DISABLE;
    fadt.u8S4BIOSReq          = 0;
    fadt.u8PStateCnt          = 0;
    fadt.u32PM1aEVTBLK        = RT_H2LE_U32(PM1a_EVT_BLK);
    fadt.u32PM1bEVTBLK        = RT_H2LE_U32(PM1b_EVT_BLK);
    fadt.u32PM1aCTLBLK        = RT_H2LE_U32(PM1a_CTL_BLK);
    fadt.u32PM1bCTLBLK        = RT_H2LE_U32(PM1b_CTL_BLK);
    fadt.u32PM2CTLBLK         = RT_H2LE_U32(PM2_CTL_BLK);
    fadt.u32PMTMRBLK          = RT_H2LE_U32(PM_TMR_BLK);
    fadt.u32GPE0BLK           = RT_H2LE_U32(GPE0_BLK);
    fadt.u32GPE1BLK           = RT_H2LE_U32(GPE1_BLK);
    fadt.u8PM1EVTLEN          = 4;
    fadt.u8PM1CTLLEN          = 2;
    fadt.u8PM2CTLLEN          = 0;
    fadt.u8PMTMLEN            = 4;
    fadt.u8GPE0BLKLEN         = GPE0_BLK_LEN;
    fadt.u8GPE1BLKLEN         = GPE1_BLK_LEN;
    fadt.u8GPE1BASE           = GPE1_BASE;
    fadt.u8CSTCNT             = 0;
    fadt.u16PLVL2LAT          = RT_H2LE_U16(P_LVL2_LAT);
    fadt.u16PLVL3LAT          = RT_H2LE_U16(P_LVL3_LAT);
    fadt.u16FlushSize         = RT_H2LE_U16(FLUSH_SIZE);
    fadt.u16FlushStride       = RT_H2LE_U16(FLUSH_STRIDE);
    fadt.u8DutyOffset         = 0;
    fadt.u8DutyWidth          = 0;
    fadt.u8DayAlarm           = 0;
    fadt.u8MonAlarm           = 0;
    fadt.u8Century            = 0;
    fadt.u16IAPCBOOTARCH      = RT_H2LE_U16(IAPC_BOOT_ARCH_LEGACY_DEV | IAPC_BOOT_ARCH_8042);
    /** @note WBINVD is required for ACPI versions newer than 1.0 */
    fadt.u32Flags             = RT_H2LE_U32(  FADT_FL_WBINVD | FADT_FL_SLP_BUTTON
                                            | FADT_FL_FIX_RTC | FADT_FL_TMR_VAL_EXT);
    acpiWriteGenericAddr(&fadt.ResetReg,     1,  8, 0, 1, ACPI_RESET_BLK);
    fadt.u8ResetVal           = ACPI_RESET_REG_VAL;
    fadt.u64XFACS             = RT_H2LE_U64((uint64_t)facs_addr);
    fadt.u64XDSDT             = RT_H2LE_U64((uint64_t)dsdt_addr);
    acpiWriteGenericAddr(&fadt.X_PM1aEVTBLK, 1, 32, 0, 2, PM1a_EVT_BLK);
    acpiWriteGenericAddr(&fadt.X_PM1bEVTBLK, 0,  0, 0, 0, PM1b_EVT_BLK);
    acpiWriteGenericAddr(&fadt.X_PM1aCTLBLK, 1, 16, 0, 2, PM1a_CTL_BLK);
    acpiWriteGenericAddr(&fadt.X_PM1bCTLBLK, 0,  0, 0, 0, PM1b_CTL_BLK);
    acpiWriteGenericAddr(&fadt.X_PM2CTLBLK,  0,  0, 0, 0,  PM2_CTL_BLK);
    acpiWriteGenericAddr(&fadt.X_PMTMRBLK,   1, 32, 0, 3,   PM_TMR_BLK);
    acpiWriteGenericAddr(&fadt.X_GPE0BLK,    1, 16, 0, 1,     GPE0_BLK);
    acpiWriteGenericAddr(&fadt.X_GPE1BLK,    0,  0, 0, 0,     GPE1_BLK);
    fadt.header.u8Checksum    = acpiChecksum ((uint8_t*)&fadt, sizeof(fadt));
    acpiPhyscpy (s, addr, &fadt, sizeof(fadt));
}

/*
 * Root System Description Table.
 * The RSDT and XSDT tables are basically identical. The only difference is 32 vs 64 bits
 * addresses for description headers. RSDT is for ACPI 1.0. XSDT for ACPI 2.0 and up.
 */
static int acpiSetupRSDT (ACPIState *s, RTGCPHYS addr, unsigned int nb_entries, uint32_t *addrs)
{
    ACPITBLRSDT *rsdt;
    const size_t size = sizeof(ACPITBLHEADER) + nb_entries * sizeof(rsdt->u32Entry[0]);

    rsdt = (ACPITBLRSDT*)RTMemAllocZ (size);
    if (!rsdt)
        return PDMDEV_SET_ERROR(s->pDevIns, VERR_NO_TMP_MEMORY, N_("Cannot allocate RSDT"));

    acpiPrepareHeader (&rsdt->header, "RSDT", size, 1);
    for (unsigned int i = 0; i < nb_entries; ++i)
    {
        rsdt->u32Entry[i] = RT_H2LE_U32(addrs[i]);
        Log(("Setup RSDT: [%d] = %x\n", i, rsdt->u32Entry[i]));
    }
    rsdt->header.u8Checksum = acpiChecksum ((uint8_t*)rsdt, size);
    acpiPhyscpy (s, addr, rsdt, size);
    RTMemFree (rsdt);
    return VINF_SUCCESS;
}

/* Extended System Description Table. */
static int acpiSetupXSDT (ACPIState *s, RTGCPHYS addr, unsigned int nb_entries, uint32_t *addrs)
{
    ACPITBLXSDT *xsdt;
    const size_t size = sizeof(ACPITBLHEADER) + nb_entries * sizeof(xsdt->u64Entry[0]);

    xsdt = (ACPITBLXSDT*)RTMemAllocZ (size);
    if (!xsdt)
        return VERR_NO_TMP_MEMORY;

    acpiPrepareHeader (&xsdt->header, "XSDT", size, 1 /* according to ACPI 3.0 specs */);
    for (unsigned int i = 0; i < nb_entries; ++i)
    {
        xsdt->u64Entry[i] = RT_H2LE_U64((uint64_t)addrs[i]);
        Log(("Setup XSDT: [%d] = %VX64\n", i, xsdt->u64Entry[i]));
    }
    xsdt->header.u8Checksum = acpiChecksum ((uint8_t*)xsdt, size);
    acpiPhyscpy (s, addr, xsdt, size);
    RTMemFree (xsdt);
    return VINF_SUCCESS;
}

/* Root System Description Pointer (RSDP) */
static void acpiSetupRSDP (ACPITBLRSDP *rsdp, uint32_t rsdt_addr, uint64_t xsdt_addr)
{
    memset(rsdp, 0, sizeof(*rsdp));

    /* ACPI 1.0 part (RSDT */
    memcpy(rsdp->au8Signature, "RSD PTR ", 8);
    memcpy(rsdp->au8OemId, "VBOX  ", 6);
    rsdp->u8Revision    = ACPI_REVISION;
    rsdp->u32RSDT       = RT_H2LE_U32(rsdt_addr);
    rsdp->u8Checksum    = acpiChecksum((uint8_t*)rsdp, RT_OFFSETOF(ACPITBLRSDP, u32Length));

    /* ACPI 2.0 part (XSDT) */
    rsdp->u32Length     = RT_H2LE_U32(sizeof(ACPITBLRSDP));
    rsdp->u64XSDT       = RT_H2LE_U64(xsdt_addr);
    rsdp->u8ExtChecksum = acpiChecksum ((uint8_t*)rsdp, sizeof(ACPITBLRSDP));
}

/* Multiple APIC Description Table. */
/** @todo All hardcoded, should set this up based on the actual VM config!!!!! */
/** @note APIC without IO-APIC hangs Windows Vista therefore we setup both */
static void acpiSetupMADT (ACPIState *s, RTGCPHYS addr)
{
    ACPITBLMADT madt;

    /* Don't call this function if u8UseIOApic==false! */
    Assert(s->u8UseIOApic);

    memset(&madt, 0, sizeof(madt));
    acpiPrepareHeader(&madt.header, "APIC", sizeof(madt), 2);

    madt.u32LAPIC          = RT_H2LE_U32(0xfee00000);
    madt.u32Flags          = RT_H2LE_U32(PCAT_COMPAT);

    madt.LApic.u8Type      = 0;
    madt.LApic.u8Length    = sizeof(ACPITBLLAPIC);
    madt.LApic.u8ProcId    = 0;
    madt.LApic.u8ApicId    = 0;
    madt.LApic.u32Flags    = RT_H2LE_U32(LAPIC_ENABLED);

    madt.IOApic.u8Type     = 1;
    madt.IOApic.u8Length   = sizeof(ACPITBLIOAPIC);
    madt.IOApic.u8IOApicId = 0;
    madt.IOApic.u8Reserved = 0;
    madt.IOApic.u32Address = RT_H2LE_U32(0xfec00000);
    madt.IOApic.u32GSIB    = RT_H2LE_U32(0);

    madt.header.u8Checksum = acpiChecksum ((uint8_t*)&madt, sizeof(madt));
    acpiPhyscpy (s, addr, &madt, sizeof(madt));
}

/* SCI IRQ */
DECLINLINE(void) acpiSetIrq (ACPIState *s, int level)
{
    if (s->pm1a_ctl & SCI_EN)
        PDMDevHlpPCISetIrq (s->pDevIns, -1, level);
}

DECLINLINE(uint32_t) pm1a_pure_en (uint32_t en)
{
    return en & ~(RSR_EN | IGN_EN);
}

DECLINLINE(uint32_t) pm1a_pure_sts (uint32_t sts)
{
    return sts & ~(RSR_STS | IGN_STS);
}

DECLINLINE(int) pm1a_level (ACPIState *s)
{
    return (pm1a_pure_en (s->pm1a_en) & pm1a_pure_sts (s->pm1a_sts)) != 0;
}

DECLINLINE(int) gpe0_level (ACPIState *s)
{
    return (s->gpe0_en & s->gpe0_sts) != 0;
}

static void update_pm1a (ACPIState *s, uint32_t sts, uint32_t en)
{
    int old_level, new_level;

    if (gpe0_level (s))
        return;

    old_level = pm1a_level (s);
    new_level = (pm1a_pure_en (en) & pm1a_pure_sts (sts)) != 0;

    s->pm1a_en = en;
    s->pm1a_sts = sts;

    if (new_level != old_level)
        acpiSetIrq (s, new_level);
}

static void update_gpe0 (ACPIState *s, uint32_t sts, uint32_t en)
{
    int old_level, new_level;

    if (pm1a_level (s))
        return;

    old_level = (s->gpe0_en & s->gpe0_sts) != 0;
    new_level = (en & sts) != 0;

    s->gpe0_en = en;
    s->gpe0_sts = sts;

    if (new_level != old_level)
        acpiSetIrq (s, new_level);
}

static int acpiPowerDown (ACPIState *s)
{
    int rc = PDMDevHlpVMPowerOff(s->pDevIns);
    if (VBOX_FAILURE (rc))
        AssertMsgFailed (("Could not power down the VM. rc = %Vrc\n", rc));
    return rc;
}

/** Converts a ACPI port interface pointer to an ACPI state pointer. */
#define IACPIPORT_2_ACPISTATE(pInterface) ( (ACPIState*)((uintptr_t)pInterface - RT_OFFSETOF(ACPIState, IACPIPort)) )

/**
 * Send an ACPI power off event.
 *
 * @returns VBox status code
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(int) acpiPowerButtonPress(PPDMIACPIPORT pInterface)
{
    ACPIState *s = IACPIPORT_2_ACPISTATE(pInterface);
    update_pm1a (s, s->pm1a_sts | PWRBTN_STS, s->pm1a_en);
    return VINF_SUCCESS;
}

/* PM1a_EVT_BLK enable */
static uint32_t acpiPm1aEnReadw (ACPIState *s, uint32_t addr)
{
    uint16_t val = s->pm1a_en;
    Log (("acpi: acpiPm1aEnReadw -> %#x\n", val));
    return val;
}

static void acpiPM1aEnWritew (ACPIState *s, uint32_t addr, uint32_t val)
{
    Log (("acpi: acpiPM1aEnWritew <- %#x (%#x)\n", val, val & ~(RSR_EN | IGN_EN)));
    val &= ~(RSR_EN | IGN_EN);
    update_pm1a (s, s->pm1a_sts, val);
}

/* PM1a_EVT_BLK status */
static uint32_t acpiPm1aStsReadw (ACPIState *s, uint32_t addr)
{
    uint16_t val = s->pm1a_sts;
    Log (("acpi: acpiPm1aStsReadw -> %#x\n", val));
    return val;
}

static void acpiPM1aStsWritew (ACPIState *s, uint32_t addr, uint32_t val)
{
    Log (("acpi: acpiPM1aStsWritew <- %#x (%#x)\n", val, val & ~(RSR_STS | IGN_STS)));
    val = s->pm1a_sts & ~(val & ~(RSR_STS | IGN_STS));
    update_pm1a (s, val, s->pm1a_en);
}

/* PM1a_CTL_BLK */
static uint32_t acpiPm1aCtlReadw (ACPIState *s, uint32_t addr)
{
    uint16_t val = s->pm1a_ctl;
    Log (("acpi: acpiPm1aCtlReadw -> %#x\n", val));
    return val;
}

static int acpiPM1aCtlWritew (ACPIState *s, uint32_t addr, uint32_t val)
{
    uint32_t uSleepState;

    Log (("acpi: acpiPM1aCtlWritew <- %#x (%#x)\n", val, val & ~(RSR_CNT | IGN_CNT)));
    s->pm1a_ctl = val & ~(RSR_CNT | IGN_CNT);

    uSleepState = (s->pm1a_ctl >> SLP_TYPx_SHIFT) & SLP_TYPx_MASK;
    if (uSleepState != s->uSleepState)
    {
        s->uSleepState = uSleepState;
        switch (uSleepState)
        {
            case 0x00:                  /* S0 */
                break;
            case 0x05:                  /* S5 */
                LogRel (("Entering S5 (power down)\n"));
                return acpiPowerDown (s);
            default:
                AssertMsgFailed (("Unknown sleep state %#x\n", uSleepState));
                break;
        }
    }
    return VINF_SUCCESS;
}

/* GPE0_BLK */
static uint32_t acpiGpe0EnReadb (ACPIState *s, uint32_t addr)
{
    uint8_t val = s->gpe0_en;
    Log (("acpi: acpiGpe0EnReadl -> %#x\n", val));
    return val;
}

static void acpiGpe0EnWriteb (ACPIState *s, uint32_t addr, uint32_t val)
{
    Log (("acpi: acpiGpe0EnWritel <- %#x\n", val));
    update_gpe0 (s, s->gpe0_sts, val);
}

static uint32_t acpiGpe0StsReadb (ACPIState *s, uint32_t addr)
{
    uint8_t val = s->gpe0_sts;
    Log (("acpi: acpiGpe0StsReadl -> %#x\n", val));
    return val;
}

static void acpiGpe0StsWriteb (ACPIState *s, uint32_t addr, uint32_t val)
{
    val = s->gpe0_sts & ~val;
    update_gpe0 (s, val, s->gpe0_en);
    Log (("acpi: acpiGpe0StsWritel <- %#x\n", val));
}

static int acpiResetWriteU8(ACPIState *s, uint32_t addr, uint32_t val)
{
    int rc = VINF_SUCCESS;

    Log(("ACPI: acpiResetWriteU8: %x %x\n", addr, val));
    if (val == ACPI_RESET_REG_VAL)
    {
# ifndef IN_RING3
        rc = VINF_IOM_HC_IOPORT_WRITE;
# else /* IN_RING3 */
        rc = PDMDevHlpVMReset(s->pDevIns);
# endif /* !IN_RING3 */
    }
    return rc;
}

/* SMI */
static void acpiSmiWriteU8 (ACPIState *s, uint32_t addr, uint32_t val)
{
    Log (("acpi: acpiSmiWriteU8 %#x\n", val));
    if (val == ACPI_ENABLE)
        s->pm1a_ctl |= SCI_EN;
    else if (val == ACPI_DISABLE)
        s->pm1a_ctl &= ~SCI_EN;
    else
        Log (("acpi: acpiSmiWriteU8 %#x <- unknown value\n", val));
}

static uint32_t find_rsdp_space (void)
{
    return 0xe0000;
}

static void acpiPMTimerReset (ACPIState *s)
{
    uint64_t interval, freq;

    freq = TMTimerGetFreq (s->CTXSUFF(ts));
    interval = ASMMultU64ByU32DivByU32 (0xffffffff, freq, PM_TMR_FREQ);
    Log (("interval = %RU64\n", interval));
    TMTimerSet (s->CTXSUFF(ts), TMTimerGet (s->CTXSUFF(ts)) + interval);
}

static DECLCALLBACK(void) acpiTimer (PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);

    Log (("acpi: pm timer sts %#x (%d), en %#x (%d)\n",
          s->pm1a_sts, (s->pm1a_sts & TMR_STS) != 0,
          s->pm1a_en, (s->pm1a_en & TMR_EN) != 0));

    update_pm1a (s, s->pm1a_sts | TMR_STS, s->pm1a_en);
    acpiPMTimerReset (s);
}

/**
 * _BST method.
 */
static void acpiFetchBatteryStatus (ACPIState *s)
{
    uint32_t           *p = s->au8BatteryInfo;
    bool               fPresent;              /* battery present? */
    PDMACPIBATCAPACITY hostRemainingCapacity; /* 0..100 */
    PDMACPIBATSTATE    hostBatteryState;      /* bitfield */
    uint32_t           hostPresentRate;       /* 0..1000 */
    int                rc;

    if (!s->pDrv)
        return;
    rc = s->pDrv->pfnQueryBatteryStatus (s->pDrv, &fPresent, &hostRemainingCapacity,
                                         &hostBatteryState, &hostPresentRate);
    AssertRC (rc);

    /* default values */
    p[BAT_STATUS_STATE]              = hostBatteryState;
    p[BAT_STATUS_PRESENT_RATE]       = hostPresentRate == ~0U ? 0xFFFFFFFF
                                                              : hostPresentRate * 50;  /* mW */
    p[BAT_STATUS_REMAINING_CAPACITY] = 50000; /* mWh */
    p[BAT_STATUS_PRESENT_VOLTAGE]    = 10000; /* mV */

    /* did we get a valid battery state? */
    if (hostRemainingCapacity != PDM_ACPI_BAT_CAPACITY_UNKNOWN)
        p[BAT_STATUS_REMAINING_CAPACITY] = hostRemainingCapacity * 500; /* mWh */
    if (hostBatteryState == PDM_ACPI_BAT_STATE_CHARGED)
        p[BAT_STATUS_PRESENT_RATE] = 0; /* mV */
}

/**
 * _BIF method.
 */
static void acpiFetchBatteryInfo (ACPIState *s)
{
    uint32_t *p = s->au8BatteryInfo;

    p[BAT_INFO_UNITS]                      = 0;     /* mWh */
    p[BAT_INFO_DESIGN_CAPACITY]            = 50000; /* mWh */
    p[BAT_INFO_LAST_FULL_CHARGE_CAPACITY]  = 50000; /* mWh */
    p[BAT_INFO_TECHNOLOGY]                 = BAT_TECH_PRIMARY;
    p[BAT_INFO_DESIGN_VOLTAGE]             = 10000; /* mV */
    p[BAT_INFO_DESIGN_CAPACITY_OF_WARNING] = 100;   /* mWh */
    p[BAT_INFO_DESIGN_CAPACITY_OF_LOW]     = 50;    /* mWh */
    p[BAT_INFO_CAPACITY_GRANULARITY_1]     = 1;     /* mWh */
    p[BAT_INFO_CAPACITY_GRANULARITY_2]     = 1;     /* mWh */
}

/**
 * _STA method.
 */
static uint32_t acpiGetBatteryDeviceStatus (ACPIState *s)
{
    bool               fPresent;              /* battery present? */
    PDMACPIBATCAPACITY hostRemainingCapacity; /* 0..100 */
    PDMACPIBATSTATE    hostBatteryState;      /* bitfield */
    uint32_t           hostPresentRate;       /* 0..1000 */
    int                rc;

    if (!s->pDrv)
        return 0;
    rc = s->pDrv->pfnQueryBatteryStatus (s->pDrv, &fPresent, &hostRemainingCapacity,
                                         &hostBatteryState, &hostPresentRate);
    AssertRC (rc);

    return fPresent
        ?   STA_DEVICE_PRESENT_MASK                     /* present */
          | STA_DEVICE_ENABLED_MASK                     /* enabled and decodes its resources */
          | STA_DEVICE_SHOW_IN_UI_MASK                  /* should be shown in UI */
          | STA_DEVICE_FUNCTIONING_PROPERLY_MASK        /* functioning properly */
          | STA_BATTERY_PRESENT_MASK                    /* battery is present */
        : 0;                                            /* device not present */
}

static uint32_t acpiGetPowerSource (ACPIState *s)
{
    PDMACPIPOWERSOURCE ps;

    /* query the current power source from the host driver */
    if (!s->pDrv)
        return AC_ONLINE;
    int rc = s->pDrv->pfnQueryPowerSource (s->pDrv, &ps);
    AssertRC (rc);
    return ps == PDM_ACPI_POWER_SOURCE_BATTERY ? AC_OFFLINE : AC_ONLINE;
}

IO_WRITE_PROTO (acpiBatIndexWrite)
{
    ACPIState *s = (ACPIState *)pvUser;

    switch (cb)
    {
        case 4:
            u32 >>= s->u8IndexShift;
            /* see comment at the declaration of u8IndexShift */
            if (s->u8IndexShift == 0 && u32 == (BAT_DEVICE_STATUS << 2))
            {
                s->u8IndexShift = 2;
                u32 >>= 2;
            }
            Assert (u32 < BAT_INDEX_LAST);
            s->uBatteryIndex = u32;
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_READ_PROTO (acpiBatDataRead)
{
    ACPIState *s = (ACPIState *)pvUser;

    switch (cb)
    {
        case 4:
            switch (s->uBatteryIndex)
            {
                case BAT_STATUS_STATE:
                    acpiFetchBatteryStatus(s);
                case BAT_STATUS_PRESENT_RATE:
                case BAT_STATUS_REMAINING_CAPACITY:
                case BAT_STATUS_PRESENT_VOLTAGE:
                    *pu32 = s->au8BatteryInfo[s->uBatteryIndex];
                    break;

                case BAT_INFO_UNITS:
                    acpiFetchBatteryInfo(s);
                case BAT_INFO_DESIGN_CAPACITY:
                case BAT_INFO_LAST_FULL_CHARGE_CAPACITY:
                case BAT_INFO_TECHNOLOGY:
                case BAT_INFO_DESIGN_VOLTAGE:
                case BAT_INFO_DESIGN_CAPACITY_OF_WARNING:
                case BAT_INFO_DESIGN_CAPACITY_OF_LOW:
                case BAT_INFO_CAPACITY_GRANULARITY_1:
                case BAT_INFO_CAPACITY_GRANULARITY_2:
                    *pu32 = s->au8BatteryInfo[s->uBatteryIndex];
                    break;

                case BAT_DEVICE_STATUS:
                    *pu32 = acpiGetBatteryDeviceStatus(s);
                    break;

                case BAT_POWER_SOURCE:
                    *pu32 = acpiGetPowerSource(s);
                    break;

                default:
                    AssertMsgFailed (("Invalid battery index %d\n", s->uBatteryIndex));
                    break;
            }
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
//    LogRel(("Query %04x => %08x\n", s->uBatteryIndex, *pu32));
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiSysInfoIndexWrite)
{
    ACPIState *s = (ACPIState *)pvUser;

    Log(("system_index = %d, %d\n", u32, u32 >> 2));
    switch (cb) {
    case 4:
        if (u32 == SYSTEM_INFO_INDEX_VALID || u32 == SYSTEM_INFO_INDEX_INVALID)
            s->uSystemInfoIndex = u32;
        else
        {
            u32 >>= s->u8IndexShift;
            Assert (u32 < SYSTEM_INFO_INDEX_LAST);
            s->uSystemInfoIndex = u32;
        }
        break;

    default:
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
        break;
    }
    return VINF_SUCCESS;
}

IO_READ_PROTO (acpiSysInfoDataRead)
{
    ACPIState *s = (ACPIState *)pvUser;

    switch (cb)
    {
        case 4:
            switch (s->uSystemInfoIndex)
            {
                case SYSTEM_INFO_INDEX_MEMORY_LENGTH:
                    *pu32 = s->u64RamSize;
                    break;

                case SYSTEM_INFO_INDEX_USE_IOAPIC:
                    *pu32 = s->u8UseIOApic;
                    break;

                default:
                    AssertMsgFailed (("Invalid system info index %d\n", s->uSystemInfoIndex));
                    break;
            }
            break;

        default:
            return VERR_IOM_IOPORT_UNUSED;
    }

    Log(("index %d val %d\n", s->uSystemInfoIndex, *pu32));
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiSysInfoDataWrite)
{
    ACPIState *s = (ACPIState *)pvUser;

    Log(("addr=%#x cb=%d u32=%#x si=%#x\n", Port, cb, u32, s->uSystemInfoIndex));

    if (cb == 4 && u32 == 0xbadc0de)
    {
        switch (s->uSystemInfoIndex)
        {
            case SYSTEM_INFO_INDEX_INVALID:
                s->u8IndexShift = 0;
                break;

            case SYSTEM_INFO_INDEX_VALID:
                s->u8IndexShift = 2;
                break;

            default:
                AssertMsgFailed(("Port=%#x cb=%d u32=%#x system_index=%#x\n",
                                Port, cb, u32, s->uSystemInfoIndex));
                break;
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
    return VINF_SUCCESS;
}

/* IO Helpers */
IO_READ_PROTO (acpiPm1aEnRead)
{
    switch (cb)
    {
        case 2:
            *pu32 = acpiPm1aEnReadw ((ACPIState*)pvUser, Port);
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

IO_READ_PROTO (acpiPm1aStsRead)
{
    switch (cb)
    {
        case 2:
            *pu32 = acpiPm1aStsReadw ((ACPIState*)pvUser, Port);
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

IO_READ_PROTO (acpiPm1aCtlRead)
{
    switch (cb)
    {
        case 2:
            *pu32 = acpiPm1aCtlReadw ((ACPIState*)pvUser, Port);
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiPM1aEnWrite)
{
    switch (cb)
    {
        case 2:
            acpiPM1aEnWritew ((ACPIState*)pvUser, Port, u32);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiPM1aStsWrite)
{
    switch (cb)
    {
        case 2:
            acpiPM1aStsWritew ((ACPIState*)pvUser, Port, u32);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiPM1aCtlWrite)
{
    switch (cb)
    {
        case 2:
            return acpiPM1aCtlWritew ((ACPIState*)pvUser, Port, u32);
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

/**
 * PMTMR readable from host/guest.
 */
IO_READ_PROTO (acpiPMTmrRead)
{
    if (cb == 4)
    {
        ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);
        int64_t now = TMTimerGet (s->CTXSUFF(ts));
        int64_t elapsed = now - s->pm_timer_initial;

        *pu32 = ASMMultU64ByU32DivByU32 (elapsed, PM_TMR_FREQ, TMTimerGetFreq (s->CTXSUFF(ts)));
        Log (("acpi: acpiPMTmrRead -> %#x\n", *pu32));
        return VINF_SUCCESS;
    }
    return VERR_IOM_IOPORT_UNUSED;
}

#ifdef IN_RING3

IO_READ_PROTO (acpiGpe0StsRead)
{
    switch (cb)
    {
        case 1:
            *pu32 = acpiGpe0StsReadb ((ACPIState*)pvUser, Port);
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

IO_READ_PROTO (acpiGpe0EnRead)
{
    switch (cb)
    {
        case 1:
            *pu32 = acpiGpe0EnReadb ((ACPIState*)pvUser, Port);
            break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiGpe0StsWrite)
{
    switch (cb)
    {
        case 1:
            acpiGpe0StsWriteb ((ACPIState*)pvUser, Port, u32);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiGpe0EnWrite)
{
    switch (cb)
    {
        case 1:
            acpiGpe0EnWriteb ((ACPIState*)pvUser, Port, u32);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiSmiWrite)
{
    switch (cb)
    {
        case 1:
            acpiSmiWriteU8 ((ACPIState*)pvUser, Port, u32);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiResetWrite)
{
    switch (cb)
    {
        case 1:
            return acpiResetWriteU8 ((ACPIState*)pvUser, Port, u32);
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

#ifdef DEBUG_ACPI

IO_WRITE_PROTO (acpiDhexWrite)
{
    switch (cb)
    {
        case 1:
            Log (("%#x\n", u32 & 0xff));
            break;
        case 2:
            Log (("%#6x\n", u32 & 0xffff));
        case 4:
            Log (("%#10x\n", u32));
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (acpiDchrWrite)
{
    switch (cb)
    {
        case 1:
            Log (("%c", u32 & 0xff));
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

#endif /* DEBUG_ACPI */


/**
 * Saved state structure description.
 */
static const SSMFIELD g_AcpiSavedStateFields[] =
{
    SSMFIELD_ENTRY (ACPIState, pm1a_en),
    SSMFIELD_ENTRY (ACPIState, pm1a_sts),
    SSMFIELD_ENTRY (ACPIState, pm1a_ctl),
    SSMFIELD_ENTRY (ACPIState, pm_timer_initial),
    SSMFIELD_ENTRY (ACPIState, gpe0_en),
    SSMFIELD_ENTRY (ACPIState, gpe0_sts),
    SSMFIELD_ENTRY (ACPIState, uBatteryIndex),
    SSMFIELD_ENTRY (ACPIState, uSystemInfoIndex),
    SSMFIELD_ENTRY (ACPIState, u64RamSize),
    SSMFIELD_ENTRY (ACPIState, u8IndexShift),
    SSMFIELD_ENTRY (ACPIState, u8UseIOApic),
    SSMFIELD_ENTRY (ACPIState, uSleepState),
    SSMFIELD_ENTRY_TERM ()
};

static DECLCALLBACK(int) acpi_save_state (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);
    return SSMR3PutStruct (pSSMHandle, s, &g_AcpiSavedStateFields[0]);
}

static DECLCALLBACK(int) acpi_load_state (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle,
                                          uint32_t u32Version)
{
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);
    int rc;

    if (u32Version != 4)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    rc = SSMR3GetStruct (pSSMHandle, s, &g_AcpiSavedStateFields[0]);
    if (VBOX_SUCCESS (rc))
    {
        acpiFetchBatteryStatus (s);
        acpiFetchBatteryInfo (s);
        acpiPMTimerReset (s);
    }
    return rc;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) acpiQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    ACPIState *pData = (ACPIState*)((uintptr_t)pInterface - RT_OFFSETOF(ACPIState, IBase));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        case PDMINTERFACE_ACPI_PORT:
            return &pData->IACPIPort;
        default:
            return NULL;
    }
}

/**
 * Create the ACPI tables.
 */
static int acpiPlantTables (ACPIState *s)
{
    int      rc;
    uint32_t rsdt_addr, xsdt_addr, fadt_addr, facs_addr, dsdt_addr, last_addr, apic_addr = 0;
    uint32_t addend = 0;
    uint32_t rsdt_addrs[4];
    uint32_t cAddr;
    size_t   rsdt_tbl_len = sizeof(ACPITBLHEADER);
    size_t   xsdt_tbl_len = sizeof(ACPITBLHEADER);

    cAddr = 1;           /* FADT */
    if (s->u8UseIOApic)
        cAddr++;         /* MADT */

    rsdt_tbl_len += cAddr*4;  /* each entry: 32 bits phys. address. */
    xsdt_tbl_len += cAddr*8;  /* each entry: 64 bits phys. address. */

    rc = CFGMR3QueryU64 (s->pDevIns->pCfgHandle, "RamSize", &s->u64RamSize);
    if (VBOX_FAILURE (rc))
        return PDMDEV_SET_ERROR(s->pDevIns, rc,
                                N_("Configuration error: Querying "
                                   "\"RamSize\" as integer failed"));

    if (s->u64RamSize > (0xffffffff - 0x10000))
        return PDMDEV_SET_ERROR(s->pDevIns, VERR_OUT_OF_RANGE,
                                N_("Configuration error: Invalid \"RamSize\", maximum allowed "
                                   "value is 4095MB"));

    rsdt_addr = 0;
    xsdt_addr = RT_ALIGN_32 (rsdt_addr + rsdt_tbl_len, 16);
    fadt_addr = RT_ALIGN_32 (xsdt_addr + xsdt_tbl_len, 16);
    facs_addr = RT_ALIGN_32 (fadt_addr + sizeof(ACPITBLFADT), 16);
    if (s->u8UseIOApic)
    {
        apic_addr = RT_ALIGN_32 (facs_addr + sizeof(ACPITBLFACS), 16);
        dsdt_addr = RT_ALIGN_32 (apic_addr + sizeof(ACPITBLMADT), 16);
    }
    else
    {
        dsdt_addr = RT_ALIGN_32 (facs_addr + sizeof(ACPITBLFACS), 16);
    }

    last_addr = RT_ALIGN_32 (dsdt_addr + sizeof(AmlCode), 16);
    if (last_addr > 0x10000)
        return PDMDEV_SET_ERROR(s->pDevIns, VERR_TOO_MUCH_DATA,
                                N_("Error: ACPI tables > 64KB!"));

    Log(("RSDP 0x%08X\n", find_rsdp_space()));
    addend = (uint32_t) s->u64RamSize - 0x10000;
    Log(("RSDT 0x%08X XSDT 0x%08X\n", rsdt_addr + addend, xsdt_addr + addend));
    Log(("FACS 0x%08X FADT 0x%08X\n", facs_addr + addend, fadt_addr + addend));
    Log(("DSDT 0x%08X\n", dsdt_addr + addend));
    acpiSetupRSDP ((ACPITBLRSDP*)s->au8RSDPPage, rsdt_addr + addend, xsdt_addr + addend);
    acpiSetupDSDT (s, dsdt_addr + addend);
    acpiSetupFACS (s, facs_addr + addend);
    acpiSetupFADT (s, fadt_addr + addend, facs_addr + addend, dsdt_addr + addend);

    rsdt_addrs[0] = fadt_addr + addend;
    if (s->u8UseIOApic)
    {
        acpiSetupMADT (s, apic_addr + addend);
        rsdt_addrs[1] = apic_addr + addend;
    }

    rc = acpiSetupRSDT (s, rsdt_addr + addend, cAddr, rsdt_addrs);
    if (VBOX_FAILURE(rc))
        return rc;
    return acpiSetupXSDT (s, xsdt_addr + addend, cAddr, rsdt_addrs);
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) acpiConstruct (PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int rc;
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);
    uint32_t rsdp_addr;
    PCIDevice *dev;
    bool fGCEnabled;
    bool fR0Enabled;

    /* Validate and read the configuration. */
    if (!CFGMR3AreValuesValid (pCfgHandle, "RamSize\0IOAPIC\0GCEnabled\0R0Enabled\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: Invalid config key for ACPI device"));

    s->pDevIns = pDevIns;

    /* query whether we are supposed to present an IOAPIC */
    rc = CFGMR3QueryU8 (pCfgHandle, "IOAPIC", &s->u8UseIOApic);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->u8UseIOApic = 1;
    else if (VBOX_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\"."));

    rc = CFGMR3QueryBool (pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"GCEnabled\"."));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("configuration error: failed to read R0Enabled as boolean."));

    /* */
    rsdp_addr = find_rsdp_space ();
    if (!rsdp_addr)
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY,
                                N_("Can not find space for RSDP. ACPI is disabled."));

    rc = acpiPlantTables (s);
    if (VBOX_FAILURE (rc))
        return rc;

    rc = PDMDevHlpROMRegister (pDevIns, rsdp_addr, 0x1000, s->au8RSDPPage, false /* fShadow */, "ACPI RSDP");
    if (VBOX_FAILURE (rc))
        return rc;

#define R(addr, cnt, writer, reader, description) \
    do {                                                                     \
        rc = PDMDevHlpIOPortRegister (pDevIns, addr, cnt, s, writer, reader, \
                                      NULL, NULL, description);              \
        if (VBOX_FAILURE (rc))                                               \
            return rc;                                                       \
    } while (0)
#define L (GPE0_BLK_LEN / 2)

    R (PM1a_EVT_BLK+2, 1, acpiPM1aEnWrite,       acpiPm1aEnRead,      "ACPI PM1a Enable");
    R (PM1a_EVT_BLK,   1, acpiPM1aStsWrite,      acpiPm1aStsRead,     "ACPI PM1a Status");
    R (PM1a_CTL_BLK,   1, acpiPM1aCtlWrite,      acpiPm1aCtlRead,     "ACPI PM1a Control");
    R (PM_TMR_BLK,     1, NULL,                  acpiPMTmrRead,       "ACPI PM Timer");
    R (SMI_CMD,        1, acpiSmiWrite,          NULL,                "ACPI SMI");
#ifdef DEBUG_ACPI
    R (DEBUG_HEX,      1, acpiDhexWrite,         NULL,                "ACPI Debug hex");
    R (DEBUG_CHR,      1, acpiDchrWrite,         NULL,                "ACPI Debug char");
#endif
    R (BAT_INDEX,      1, acpiBatIndexWrite,     NULL,                "ACPI Battery status index");
    R (BAT_DATA,       1, NULL,                  acpiBatDataRead,     "ACPI Battery status data");
    R (SYSI_INDEX,     1, acpiSysInfoIndexWrite, NULL,                "ACPI system info index");
    R (SYSI_DATA,      1, acpiSysInfoDataWrite,  acpiSysInfoDataRead, "ACPI system info data");
    R (GPE0_BLK + L,   L, acpiGpe0EnWrite,       acpiGpe0EnRead,      "ACPI GPE0 Enable");
    R (GPE0_BLK,       L, acpiGpe0StsWrite,      acpiGpe0StsRead,     "ACPI GPE0 Status");
    R (ACPI_RESET_BLK, 1, acpiResetWrite,        NULL,                "ACPI Reset");
#undef L
#undef R

    /* register GC stuff */
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC (pDevIns, PM_TMR_BLK, 1, 0, NULL, "acpiPMTmrRead",
                                        NULL, NULL, "ACPI PM Timer");
        AssertRCReturn(rc, rc);
    }

    /* register R0 stuff */
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0 (pDevIns, PM_TMR_BLK, 1, 0, NULL, "acpiPMTmrRead",
                                        NULL, NULL, "ACPI PM Timer");
        AssertRCReturn(rc, rc);
    }

    rc = PDMDevHlpTMTimerCreate (pDevIns, TMCLOCK_VIRTUAL_SYNC, acpiTimer, "ACPI Timer", &s->tsHC);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("pfnTMTimerCreate -> %Vrc\n", rc));
        return rc;
    }

    s->tsGC = TMTimerGCPtr (s->tsHC);
    s->pm_timer_initial = TMTimerGet (s->tsHC);
    acpiPMTimerReset (s);

    dev = &s->dev;
    dev->config[0x00] = 0x86;
    dev->config[0x01] = 0x80;

    dev->config[0x02] = 0x13;
    dev->config[0x03] = 0x71;

    dev->config[0x04] = 0x01;
    dev->config[0x05] = 0x00;

    dev->config[0x06] = 0x80;
    dev->config[0x07] = 0x02;
    dev->config[0x08] = 0x08;
    dev->config[0x09] = 0x00;

    dev->config[0x0a] = 0x80;
    dev->config[0x0b] = 0x06;

    dev->config[0x0e] = 0x80;
    dev->config[0x0f] = 0x00;

#if 0 /* The ACPI controller usually has no subsystem ID. */
    dev->config[0x2c] = 0x86;
    dev->config[0x2d] = 0x80;
    dev->config[0x2e] = 0x00;
    dev->config[0x2f] = 0x00;
#endif
    dev->config[0x3c] = SCI_INT;

    rc = PDMDevHlpPCIRegister (pDevIns, dev);
    if (VBOX_FAILURE (rc))
        return rc;

    rc = PDMDevHlpSSMRegister (pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 4, sizeof(*s),
                               NULL, acpi_save_state, NULL, NULL, acpi_load_state,  NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Interfaces
     */
    /* IBase */
    s->IBase.pfnQueryInterface         = acpiQueryInterface;
    /* IACPIPort */
    s->IACPIPort.pfnPowerButtonPress   = acpiPowerButtonPress;

   /*
    * Get the corresponding connector interface
    */
   rc = PDMDevHlpDriverAttach (pDevIns, 0, &s->IBase, &s->pDrvBase, "ACPI Driver Port");
   if (VBOX_SUCCESS (rc))
   {
       s->pDrv = (PPDMIACPICONNECTOR)s->pDrvBase->pfnQueryInterface (s->pDrvBase,
                                                                     PDMINTERFACE_ACPI_CONNECTOR);
       if (!s->pDrv)
           return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_MISSING_INTERFACE,
                                   N_("LUN #0 doesn't have an ACPI connector interface!\n"));
   }
   else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
   {
       Log (("acpi: %s/%d: warning: no driver attached to LUN #0!\n",
                   pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
       rc = VINF_SUCCESS;
   }
   else
       return PDMDEV_SET_ERROR(pDevIns, rc,
                               N_("Failed to attach LUN #0!"));

    return rc;
}

/**
 * Relocates the GC pointer members.
 */
static DECLCALLBACK(void) acpiRelocate (PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);
    s->tsGC = TMTimerGCPtr (s->tsHC);
}

static DECLCALLBACK(void) acpiReset (PPDMDEVINS pDevIns)
{
    ACPIState *s = PDMINS2DATA (pDevIns, ACPIState *);

    s->pm1a_en           = 0;
    s->pm1a_sts          = 0;
    s->pm1a_ctl          = 0;
    s->pm_timer_initial  = TMTimerGet (s->CTXSUFF(ts));
    acpiPMTimerReset(s);
    s->uBatteryIndex     = 0;
    s->uSystemInfoIndex  = 0;
    s->gpe0_en           = 0;
    s->gpe0_sts          = 0;
    s->uSleepState       = 0;

    acpiPlantTables(s);
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceACPI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "acpi",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Advanced Configuration and Power Interface",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_ACPI,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(ACPIState),
    /* pfnConstruct */
    acpiConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    acpiRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    acpiReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

