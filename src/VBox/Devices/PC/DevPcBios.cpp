/* $Id$ */
/** @file
 * PC BIOS Device.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PC_BIOS
#include <VBox/pdmdev.h>
#include <VBox/mm.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include "../Builtins.h"
#include "../Builtins2.h"
#include "DevPcBios.h"


/** @page pg_devbios_cmos_assign    CMOS Assignments (BIOS)
 *
 * The BIOS uses a CMOS to store configuration data.
 * It is currently used as follows:
 *
 * @verbatim
    Base memory:
         0x15
         0x16
    Extended memory:
         0x17
         0x18
         0x30
         0x31
    Amount of memory above 16M:
         0x34
         0x35
    Boot device (BOCHS bios specific):
         0x3d
         0x38
         0x3c
    PXE debug:
         0x3f
    Floppy drive type:
         0x10
    Equipment byte:
         0x14
    First HDD:
         0x19
         0x1e - 0x25
    Second HDD:
         0x1a
         0x26 - 0x2d
    Third HDD:
         0x67 - 0x6e
    Fourth HDD:
         0x70 - 0x77
    Extended:
         0x12
    First Sata HDD:
         0x40 - 0x47
    Second Sata HDD:
         0x48 - 0x4f
    Third Sata HDD:
         0x50 - 0x57
    Fourth Sata HDD:
         0x58 - 0x5f
    Number of CPUs:
         0x60
    RAM above 4G (in 64KB units):
         0x61 - 0x65
@endverbatim
 *
 * @todo Mark which bits are compatible with which BIOSes and
 *       which are our own definitions.
 *
 * @todo r=bird: Is the 0x61 - 0x63 range defined by AMI,
 *       PHOENIX or AWARD? If not I'd say 64MB units is a bit
 *       too big, besides it forces unnecessary math stuff onto
 *       the BIOS.
 *       nike: The way how values encoded are defined by Bochs/QEmu BIOS,
 *       although for them position in CMOS is different:
 *         0x5b - 0x5c: RAM above 4G
 *         0x5f: number of CPUs
 *        Unfortunately for us those positions in our CMOS are already taken
 *        by 4th SATA drive configuration.
 *
 */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * The boot device.
 */
typedef enum DEVPCBIOSBOOT
{
    DEVPCBIOSBOOT_NONE,
    DEVPCBIOSBOOT_FLOPPY,
    DEVPCBIOSBOOT_HD,
    DEVPCBIOSBOOT_DVD,
    DEVPCBIOSBOOT_LAN
} DEVPCBIOSBOOT;

/**
 * PC Bios instance data structure.
 */
typedef struct DEVPCBIOS
{
    /** Pointer back to the device instance. */
    PPDMDEVINS      pDevIns;

    /** Boot devices (ordered). */
    DEVPCBIOSBOOT   aenmBootDevice[4];
    /** RAM size (in bytes). */
    uint64_t        cbRam;
    /** RAM hole size (in bytes). */
    uint32_t        cbRamHole;
    /** Bochs shutdown index. */
    uint32_t        iShutdown;
    /** Floppy device. */
    char           *pszFDDevice;
    /** Harddisk device. */
    char           *pszHDDevice;
    /** Sata harddisk device. */
    char           *pszSataDevice;
    /** LUN of the four harddisks which are emulated as IDE. */
    uint32_t        iSataHDLUN[4];
    /** Bios message buffer. */
    char            szMsg[256];
    /** Bios message buffer index. */
    uint32_t        iMsg;
    /** The system BIOS ROM data. */
    uint8_t        *pu8PcBios;
    /** The size of the system BIOS ROM. */
    uint64_t        cbPcBios;
    /** The name of the BIOS ROM file. */
    char           *pszPcBiosFile;
    /** The LAN boot ROM data. */
    uint8_t        *pu8LanBoot;
    /** The name of the LAN boot ROM file. */
    char           *pszLanBootFile;
    /** The DMI tables. */
    uint8_t        au8DMIPage[0x1000];
    /** The boot countdown (in seconds). */
    uint8_t        uBootDelay;
    /** I/O-APIC enabled? */
    uint8_t        u8IOAPIC;
    /** PXE debug logging enabled? */
    uint8_t        u8PXEDebug;
    /** Number of logical CPUs in guest */
    uint16_t       cCpus;
} DEVPCBIOS, *PDEVPCBIOS;

#pragma pack(1)

/** DMI header */
typedef struct DMIHDR
{
    uint8_t         u8Type;
    uint8_t         u8Length;
    uint16_t        u16Handle;
} *PDMIHDR;
AssertCompileSize(DMIHDR, 4);

/** DMI BIOS information */
typedef struct DMIBIOSINF
{
    DMIHDR          header;
    uint8_t         u8Vendor;
    uint8_t         u8Version;
    uint16_t        u16Start;
    uint8_t         u8Release;
    uint8_t         u8ROMSize;
    uint64_t        u64Characteristics;
    uint8_t         u8CharacteristicsByte1;
    uint8_t         u8CharacteristicsByte2;
    uint8_t         u8ReleaseMajor;
    uint8_t         u8ReleaseMinor;
    uint8_t         u8FirmwareMajor;
    uint8_t         u8FirmwareMinor;
} *PDMIBIOSINF;
AssertCompileSize(DMIBIOSINF, 0x18);

/** DMI system information */
typedef struct DMISYSTEMINF
{
    DMIHDR          header;
    uint8_t         u8Manufacturer;
    uint8_t         u8ProductName;
    uint8_t         u8Version;
    uint8_t         u8SerialNumber;
    uint8_t         au8Uuid[16];
    uint8_t         u8WakeupType;
    uint8_t         u8SKUNumber;
    uint8_t         u8Family;
} *PDMISYSTEMINF;
AssertCompileSize(DMISYSTEMINF, 0x1b);

/** MPS floating pointer structure */
typedef struct MPSFLOATPTR
{
    uint8_t         au8Signature[4];
    uint32_t        u32MPSAddr;
    uint8_t         u8Length;
    uint8_t         u8SpecRev;
    uint8_t         u8Checksum;
    uint8_t         au8Feature[5];
} *PMPSFLOATPTR;
AssertCompileSize(MPSFLOATPTR, 16);

/** MPS config table header */
typedef struct MPSCFGTBLHEADER
{
    uint8_t         au8Signature[4];
    uint16_t        u16Length;
    uint8_t         u8SpecRev;
    uint8_t         u8Checksum;
    uint8_t         au8OemId[8];
    uint8_t         au8ProductId[12];
    uint32_t        u32OemTablePtr;
    uint16_t        u16OemTableSize;
    uint16_t        u16EntryCount;
    uint32_t        u32AddrLocalApic;
    uint16_t        u16ExtTableLength;
    uint8_t         u8ExtTableChecksxum;
    uint8_t         u8Reserved;
} *PMPSCFGTBLHEADER;
AssertCompileSize(MPSCFGTBLHEADER, 0x2c);

/** MPS processor entry */
typedef struct MPSPROCENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8LocalApicId;
    uint8_t         u8LocalApicVersion;
    uint8_t         u8CPUFlags;
    uint32_t        u32CPUSignature;
    uint32_t        u32CPUFeatureFlags;
    uint32_t        u32Reserved[2];
} *PMPSPROCENTRY;
AssertCompileSize(MPSPROCENTRY, 20);

/** MPS bus entry */
typedef struct MPSBUSENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8BusId;
    uint8_t         au8BusTypeStr[6];
} *PMPSBUSENTRY;
AssertCompileSize(MPSBUSENTRY, 8);

/** MPS I/O-APIC entry */
typedef struct MPSIOAPICENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8Id;
    uint8_t         u8Version;
    uint8_t         u8Flags;
    uint32_t        u32Addr;
} *PMPSIOAPICENTRY;
AssertCompileSize(MPSIOAPICENTRY, 8);

/** MPS I/O-Interrupt entry */
typedef struct MPSIOINTERRUPTENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8Type;
    uint16_t        u16Flags;
    uint8_t         u8SrcBusId;
    uint8_t         u8SrcBusIrq;
    uint8_t         u8DstIOAPICId;
    uint8_t         u8DstIOAPICInt;
} *PMPSIOIRQENTRY;
AssertCompileSize(MPSIOINTERRUPTENTRY, 8);

#pragma pack()

/* Attempt to guess the LCHS disk geometry from the MS-DOS master boot
 * record (partition table). */
static int biosGuessDiskLCHS(PPDMIBLOCK pBlock, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    uint8_t aMBR[512], *p;
    int rc;
    uint32_t iEndHead, iEndSector, cLCHSCylinders, cLCHSHeads, cLCHSSectors;

    if (!pBlock)
        return VERR_INVALID_PARAMETER;
    rc = pBlock->pfnRead(pBlock, 0, aMBR, sizeof(aMBR));
    if (RT_FAILURE(rc))
        return rc;
    /* Test MBR magic number. */
    if (aMBR[510] != 0x55 || aMBR[511] != 0xaa)
        return VERR_INVALID_PARAMETER;
    for (uint32_t i = 0; i < 4; i++)
    {
        /* Figure out the start of a partition table entry. */
        p = &aMBR[0x1be + i * 16];
        iEndHead = p[5];
        iEndSector = p[6] & 63;
        if ((p[12] | p[13] | p[14] | p[15]) && iEndSector & iEndHead)
        {
            /* Assumption: partition terminates on a cylinder boundary. */
            cLCHSHeads = iEndHead + 1;
            cLCHSSectors = iEndSector;
            cLCHSCylinders = RT_MIN(1024, pBlock->pfnGetSize(pBlock) / (512 * cLCHSHeads * cLCHSSectors));
            if (cLCHSCylinders >= 1)
            {
                pLCHSGeometry->cCylinders = cLCHSCylinders;
                pLCHSGeometry->cHeads = cLCHSHeads;
                pLCHSGeometry->cSectors = cLCHSSectors;
                Log(("%s: LCHS=%d %d %d\n", __FUNCTION__, cLCHSCylinders, cLCHSHeads, cLCHSSectors));
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_PARAMETER;
}


/**
 * Write to CMOS memory.
 * This is used by the init complete code.
 */
static void pcbiosCmosWrite(PPDMDEVINS pDevIns, int off, uint32_t u32Val)
{
    Assert(off < 128);
    Assert(u32Val < 256);

#if 1
    int rc = PDMDevHlpCMOSWrite(pDevIns, off, u32Val);
    AssertRC(rc);
#else
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    IOMIOPortWrite(pVM, 0x70, off, 1);
    IOMIOPortWrite(pVM, 0x71, u32Val, 1);
    IOMIOPortWrite(pVM, 0x70, 0, 1);
#endif
}

/* -=-=-=-=-=-=- based on code from pc.c -=-=-=-=-=-=- */

/**
 * Initializes the CMOS data for one harddisk.
 */
static void pcbiosCmosInitHardDisk(PPDMDEVINS pDevIns, int offType, int offInfo, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    Log2(("%s: offInfo=%#x: LCHS=%d/%d/%d\n", __FUNCTION__, offInfo, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    if (offType)
        pcbiosCmosWrite(pDevIns, offType, 48);
    /* Cylinders low */
    pcbiosCmosWrite(pDevIns, offInfo + 0, RT_MIN(pLCHSGeometry->cCylinders, 1024) & 0xff);
    /* Cylinders high */
    pcbiosCmosWrite(pDevIns, offInfo + 1, RT_MIN(pLCHSGeometry->cCylinders, 1024) >> 8);
    /* Heads */
    pcbiosCmosWrite(pDevIns, offInfo + 2, pLCHSGeometry->cHeads);
    /* Landing zone low */
    pcbiosCmosWrite(pDevIns, offInfo + 3, 0xff);
    /* Landing zone high */
    pcbiosCmosWrite(pDevIns, offInfo + 4, 0xff);
    /* Write precomp low */
    pcbiosCmosWrite(pDevIns, offInfo + 5, 0xff);
    /* Write precomp high */
    pcbiosCmosWrite(pDevIns, offInfo + 6, 0xff);
    /* Sectors */
    pcbiosCmosWrite(pDevIns, offInfo + 7, pLCHSGeometry->cSectors);
}

/**
 * Set logical CHS geometry for a hard disk
 *
 * @returns VBox status code.
 * @param   pBase         Base interface for the device.
 * @param   pHardDisk     The hard disk.
 * @param   pLCHSGeometry Where to store the geometry settings.
 */
static int setLogicalDiskGeometry(PPDMIBASE pBase, PPDMIBLOCKBIOS pHardDisk, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDMMEDIAGEOMETRY LCHSGeometry;
    int rc = VINF_SUCCESS;

    rc = pHardDisk->pfnGetLCHSGeometry(pHardDisk, &LCHSGeometry);
    if (   rc == VERR_PDM_GEOMETRY_NOT_SET
        || LCHSGeometry.cCylinders == 0
        || LCHSGeometry.cCylinders > 1024
        || LCHSGeometry.cHeads == 0
        || LCHSGeometry.cHeads > 255
        || LCHSGeometry.cSectors == 0
        || LCHSGeometry.cSectors > 63)
    {
        PPDMIBLOCK pBlock;
        pBlock = (PPDMIBLOCK)pBase->pfnQueryInterface(pBase, PDMINTERFACE_BLOCK);
        /* No LCHS geometry, autodetect and set. */
        rc = biosGuessDiskLCHS(pBlock, &LCHSGeometry);
        if (RT_FAILURE(rc))
        {
            /* Try if PCHS geometry works, otherwise fall back. */
            rc = pHardDisk->pfnGetPCHSGeometry(pHardDisk, &LCHSGeometry);
        }
        if (   RT_FAILURE(rc)
            || LCHSGeometry.cCylinders == 0
            || LCHSGeometry.cCylinders > 1024
            || LCHSGeometry.cHeads == 0
            || LCHSGeometry.cHeads > 16
            || LCHSGeometry.cSectors == 0
            || LCHSGeometry.cSectors > 63)
        {
            uint64_t cSectors = pBlock->pfnGetSize(pBlock) / 512;
            if (cSectors / 16 / 63 <= 1024)
            {
                LCHSGeometry.cCylinders = RT_MAX(cSectors / 16 / 63, 1);
                LCHSGeometry.cHeads = 16;
            }
            else if (cSectors / 32 / 63 <= 1024)
            {
                LCHSGeometry.cCylinders = RT_MAX(cSectors / 32 / 63, 1);
                LCHSGeometry.cHeads = 32;
            }
            else if (cSectors / 64 / 63 <= 1024)
            {
                LCHSGeometry.cCylinders = cSectors / 64 / 63;
                LCHSGeometry.cHeads = 64;
            }
            else if (cSectors / 128 / 63 <= 1024)
            {
                LCHSGeometry.cCylinders = cSectors / 128 / 63;
                LCHSGeometry.cHeads = 128;
            }
            else
            {
                LCHSGeometry.cCylinders = RT_MIN(cSectors / 255 / 63, 1024);
                LCHSGeometry.cHeads = 255;
            }
            LCHSGeometry.cSectors = 63;

        }
        rc = pHardDisk->pfnSetLCHSGeometry(pHardDisk, &LCHSGeometry);
        if (rc == VERR_VD_IMAGE_READ_ONLY)
        {
            LogRel(("DevPcBios: ATA failed to update LCHS geometry, read only\n"));
            rc = VINF_SUCCESS;
        }
        else if (rc == VERR_PDM_GEOMETRY_NOT_SET)
        {
            LogRel(("DevPcBios: ATA failed to update LCHS geometry, backend refused\n"));
            rc = VINF_SUCCESS;
        }
    }

    *pLCHSGeometry = LCHSGeometry;

    return rc;
}

/**
 * Get BIOS boot code from enmBootDevice in order
 *
 * @todo r=bird: This is a rather silly function since the conversion is 1:1.
 */
static uint8_t getBiosBootCode(PDEVPCBIOS pThis, unsigned iOrder)
{
    switch (pThis->aenmBootDevice[iOrder])
    {
        case DEVPCBIOSBOOT_NONE:
            return 0;
        case DEVPCBIOSBOOT_FLOPPY:
            return 1;
        case DEVPCBIOSBOOT_HD:
            return 2;
        case DEVPCBIOSBOOT_DVD:
            return 3;
        case DEVPCBIOSBOOT_LAN:
            return 4;
        default:
            AssertMsgFailed(("aenmBootDevice[%d]=%d\n", iOrder, pThis->aenmBootDevice[iOrder]));
            return 0;
    }
}


/**
 * Init complete notification.
 * This routine will write information needed by the bios to the CMOS.
 *
 * @returns VBOX status code.
 * @param   pDevIns     The device instance.
 * @see     http://www.brl.ntt.co.jp/people/takehiko/interrupt/CMOS.LST.txt for
 *          a description of standard and non-standard CMOS registers.
 */
static DECLCALLBACK(int) pcbiosInitComplete(PPDMDEVINS pDevIns)
{
    PDEVPCBIOS      pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
    uint32_t        u32;
    unsigned        i;
    PVM             pVM = PDMDevHlpGetVM(pDevIns);
    PPDMIBLOCKBIOS  apHDs[4] = {0};
    PPDMIBLOCKBIOS  apFDs[2] = {0};
    AssertRelease(pVM);
    LogFlow(("pcbiosInitComplete:\n"));

    /*
     * Memory sizes.
     */
    /* base memory. */
    u32 = pThis->cbRam > 640 ? 640 : (uint32_t)pThis->cbRam / _1K; /* <-- this test is wrong, but it doesn't matter since we never assign less than 1MB */
    pcbiosCmosWrite(pDevIns, 0x15, u32 & 0xff);                                 /* 15h - Base Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x16, u32 >> 8);                                   /* 16h - Base Memory in K, High Byte */

    /* Extended memory, up to 65MB */
    u32 = pThis->cbRam >= 65 * _1M ? 0xffff : ((uint32_t)pThis->cbRam - _1M) / _1K;
    pcbiosCmosWrite(pDevIns, 0x17, u32 & 0xff);                                 /* 17h - Extended Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x18, u32 >> 8);                                   /* 18h - Extended Memory in K, High Byte */
    pcbiosCmosWrite(pDevIns, 0x30, u32 & 0xff);                                 /* 30h - Extended Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x31, u32 >> 8);                                   /* 31h - Extended Memory in K, High Byte */

    /* Bochs BIOS specific? Anyway, it's the amount of memory above 16MB
       and below 4GB (as it can only hold 4GB+16M). We have to chop off the
       top 2MB or it conflict with what the ACPI tables return. (Should these
       be adjusted, we still have to chop it at 0xfffc0000 or it'll conflict
       with the high BIOS mapping.) */
    uint64_t const offRamHole = _4G - pThis->cbRamHole;
    if (pThis->cbRam > 16 * _1M)
        u32 = (uint32_t)( (RT_MIN(RT_MIN(pThis->cbRam, offRamHole), UINT32_C(0xffe00000)) - 16U * _1M) / _64K );
    else
        u32 = 0;
    pcbiosCmosWrite(pDevIns, 0x34, u32 & 0xff);
    pcbiosCmosWrite(pDevIns, 0x35, u32 >> 8);

    /* Bochs/VBox BIOS specific way of specifying memory above 4GB in 64KB units.
       Bochs got these in a different location which we've already used for SATA,
       it also lacks the last two. */
    uint64_t c64KBAbove4GB;
    if (pThis->cbRam <= offRamHole)
        c64KBAbove4GB = 0;
    else
    {
        c64KBAbove4GB = (pThis->cbRam - offRamHole) / _64K;
        /* Make sure it doesn't hit the limits of the current BIOS code.   */
        AssertLogRelMsgReturn((c64KBAbove4GB >> (3 * 8)) < 255, ("%#RX64\n", c64KBAbove4GB), VERR_OUT_OF_RANGE);
    }
    pcbiosCmosWrite(pDevIns, 0x61,  c64KBAbove4GB        & 0xff);
    pcbiosCmosWrite(pDevIns, 0x62, (c64KBAbove4GB >>  8) & 0xff);
    pcbiosCmosWrite(pDevIns, 0x63, (c64KBAbove4GB >> 16) & 0xff);
    pcbiosCmosWrite(pDevIns, 0x64, (c64KBAbove4GB >> 24) & 0xff);
    pcbiosCmosWrite(pDevIns, 0x65, (c64KBAbove4GB >> 32) & 0xff);

    /*
     * Number of CPUs.
     */
    pcbiosCmosWrite(pDevIns, 0x60, pThis->cCpus & 0xff);

    /*
     * Bochs BIOS specifics - boot device.
     * We do both new and old (ami-style) settings.
     * See rombios.c line ~7215 (int19_function).
     */

    uint8_t reg3d = getBiosBootCode(pThis, 0) | (getBiosBootCode(pThis, 1) << 4);
    uint8_t reg38 = /* pcbiosCmosRead(pDevIns, 0x38) | */ getBiosBootCode(pThis, 2) << 4;
    /* This is an extension. Bochs BIOS normally supports only 3 boot devices. */
    uint8_t reg3c = getBiosBootCode(pThis, 3) | (pThis->uBootDelay << 4);
    pcbiosCmosWrite(pDevIns, 0x3d, reg3d);
    pcbiosCmosWrite(pDevIns, 0x38, reg38);
    pcbiosCmosWrite(pDevIns, 0x3c, reg3c);

    /*
     * PXE debug option.
     */
    pcbiosCmosWrite(pDevIns, 0x3f, pThis->u8PXEDebug);

    /*
     * Floppy drive type.
     */
    for (i = 0; i < RT_ELEMENTS(apFDs); i++)
    {
        PPDMIBASE pBase;
        int rc = PDMR3QueryLun(pVM, pThis->pszFDDevice, 0, i, &pBase);
        if (RT_SUCCESS(rc))
            apFDs[i] = (PPDMIBLOCKBIOS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_BLOCK_BIOS);
    }
    u32 = 0;
    if (apFDs[0])
        switch (apFDs[0]->pfnGetType(apFDs[0]))
        {
            case PDMBLOCKTYPE_FLOPPY_360:   u32 |= 1 << 4; break;
            case PDMBLOCKTYPE_FLOPPY_1_20:  u32 |= 2 << 4; break;
            case PDMBLOCKTYPE_FLOPPY_720:   u32 |= 3 << 4; break;
            case PDMBLOCKTYPE_FLOPPY_1_44:  u32 |= 4 << 4; break;
            case PDMBLOCKTYPE_FLOPPY_2_88:  u32 |= 5 << 4; break;
            default:                        AssertFailed(); break;
        }
    if (apFDs[1])
        switch (apFDs[1]->pfnGetType(apFDs[1]))
        {
            case PDMBLOCKTYPE_FLOPPY_360:   u32 |= 1; break;
            case PDMBLOCKTYPE_FLOPPY_1_20:  u32 |= 2; break;
            case PDMBLOCKTYPE_FLOPPY_720:   u32 |= 3; break;
            case PDMBLOCKTYPE_FLOPPY_1_44:  u32 |= 4; break;
            case PDMBLOCKTYPE_FLOPPY_2_88:  u32 |= 5; break;
            default:                        AssertFailed(); break;
        }
    pcbiosCmosWrite(pDevIns, 0x10, u32);                                        /* 10h - Floppy Drive Type */

    /*
     * Equipment byte.
     */
    u32 = !!apFDs[0] + !!apFDs[1];
    switch (u32)
    {
        case 1: u32 = 0x01; break;      /* floppy installed, 2 drives. */
        default:u32 = 0;    break;      /* floppy not installed. */
    }
    u32 |= RT_BIT(1);                      /* math coprocessor installed  */
    u32 |= RT_BIT(2);                      /* keyboard enabled (or mouse?) */
    u32 |= RT_BIT(3);                      /* display enabled (monitory type is 0, i.e. vga) */
    pcbiosCmosWrite(pDevIns, 0x14, u32);                                        /* 14h - Equipment Byte */

    /*
     * Harddisks.
     */
    for (i = 0; i < RT_ELEMENTS(apHDs); i++)
    {
        PPDMIBASE pBase;
        int rc = PDMR3QueryLun(pVM, pThis->pszHDDevice, 0, i, &pBase);
        if (RT_SUCCESS(rc))
            apHDs[i] = (PPDMIBLOCKBIOS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_BLOCK_BIOS);
        if (   apHDs[i]
            && (   apHDs[i]->pfnGetType(apHDs[i]) != PDMBLOCKTYPE_HARD_DISK
                || !apHDs[i]->pfnIsVisible(apHDs[i])))
            apHDs[i] = NULL;
        if (apHDs[i])
        {
            PDMMEDIAGEOMETRY LCHSGeometry;
            int rc = setLogicalDiskGeometry(pBase, apHDs[i], &LCHSGeometry);
            AssertRC(rc);

            if (i < 4)
            {
                /* Award BIOS extended drive types for first to fourth disk.
                 * Used by the BIOS for setting the logical geometry. */
                int offType, offInfo;
                switch (i)
                {
                    case 0:
                        offType = 0x19;
                        offInfo = 0x1e;
                        break;
                    case 1:
                        offType = 0x1a;
                        offInfo = 0x26;
                        break;
                    case 2:
                        offType = 0x00;
                        offInfo = 0x67;
                        break;
                    case 3:
                    default:
                        offType = 0x00;
                        offInfo = 0x70;
                        break;
                }
                pcbiosCmosInitHardDisk(pDevIns, offType, offInfo,
                                       &LCHSGeometry);
            }
            LogRel(("DevPcBios: ATA LUN#%d LCHS=%u/%u/%u\n", i, LCHSGeometry.cCylinders, LCHSGeometry.cHeads, LCHSGeometry.cSectors));
        }
    }

    /* 0Fh means extended and points to 19h, 1Ah */
    u32 = (apHDs[0] ? 0xf0 : 0) | (apHDs[1] ? 0x0f : 0);
    pcbiosCmosWrite(pDevIns, 0x12, u32);

    /*
     * Sata Harddisks.
     */
    if (pThis->pszSataDevice)
    {
        /* Clear pointers to IDE controller. */
        for (i = 0; i < RT_ELEMENTS(apHDs); i++)
            apHDs[i] = NULL;

        for (i = 0; i < RT_ELEMENTS(apHDs); i++)
        {
            PPDMIBASE pBase;
            int rc = PDMR3QueryLun(pVM, pThis->pszSataDevice, 0, pThis->iSataHDLUN[i], &pBase);
            if (RT_SUCCESS(rc))
                apHDs[i] = (PPDMIBLOCKBIOS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_BLOCK_BIOS);
            if (   apHDs[i]
                && (   apHDs[i]->pfnGetType(apHDs[i]) != PDMBLOCKTYPE_HARD_DISK
                    || !apHDs[i]->pfnIsVisible(apHDs[i])))
                apHDs[i] = NULL;
            if (apHDs[i])
            {
                PDMMEDIAGEOMETRY LCHSGeometry;
                int rc = setLogicalDiskGeometry(pBase, apHDs[i], &LCHSGeometry);
                AssertRC(rc);

                if (i < 4)
                {
                    /* Award BIOS extended drive types for first to fourth disk.
                     * Used by the BIOS for setting the logical geometry. */
                    int offInfo;
                    switch (i)
                    {
                        case 0:
                            offInfo = 0x40;
                            break;
                        case 1:
                            offInfo = 0x48;
                            break;
                        case 2:
                            offInfo = 0x50;
                            break;
                        case 3:
                        default:
                            offInfo = 0x58;
                            break;
                    }
                    pcbiosCmosInitHardDisk(pDevIns, 0x00, offInfo,
                                           &LCHSGeometry);
                }
                LogRel(("DevPcBios: SATA LUN#%d LCHS=%u/%u/%u\n", i, LCHSGeometry.cCylinders, LCHSGeometry.cHeads, LCHSGeometry.cSectors));
            }
        }
    }

    LogFlow(("%s: returns VINF_SUCCESS\n", __FUNCTION__));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) pcbiosIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pDevIns);
    NOREF(pvUser);
    NOREF(Port);
    NOREF(pu32);
    NOREF(cb);
    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) pcbiosIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    /*
     * Bochs BIOS Panic
     */
    if (    cb == 2
        &&  (   Port == 0x400
             || Port == 0x401))
    {
        Log(("pcbios: PC BIOS panic at rombios.c(%d)\n", u32));
        AssertReleaseMsgFailed(("PC BIOS panic at rombios.c(%d)\n", u32));
        return VERR_INTERNAL_ERROR;
    }

    /*
     * Bochs BIOS char printing.
     */
    if (    cb == 1
        &&  (   Port == 0x402
             || Port == 0x403))
    {
        PDEVPCBIOS pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
        /* The raw version. */
        switch (u32)
        {
            case '\r': Log2(("pcbios: <return>\n")); break;
            case '\n': Log2(("pcbios: <newline>\n")); break;
            case '\t': Log2(("pcbios: <tab>\n")); break;
            default:   Log2(("pcbios: %c (%02x)\n", u32, u32)); break;
        }

        /* The readable, buffered version. */
        if (u32 == '\n' || u32 == '\r')
        {
            pThis->szMsg[pThis->iMsg] = '\0';
            if (pThis->iMsg)
                Log(("pcbios: %s\n", pThis->szMsg));
            pThis->iMsg = 0;
        }
        else
        {
            if (pThis->iMsg >= sizeof(pThis->szMsg)-1)
            {
                pThis->szMsg[pThis->iMsg] = '\0';
                Log(("pcbios: %s\n", pThis->szMsg));
                pThis->iMsg = 0;
            }
            pThis->szMsg[pThis->iMsg] = (char )u32;
            pThis->szMsg[++pThis->iMsg] = '\0';
        }
        return VINF_SUCCESS;
    }

    /*
     * Bochs BIOS shutdown request.
     */
    if (cb == 1 && Port == 0x8900)
    {
        static const unsigned char szShutdown[] = "Shutdown";
        PDEVPCBIOS pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
        if (u32 == szShutdown[pThis->iShutdown])
        {
            pThis->iShutdown++;
            if (pThis->iShutdown == 8)
            {
                pThis->iShutdown = 0;
                LogRel(("8900h shutdown request.\n"));
                return PDMDevHlpVMPowerOff(pDevIns);
            }
        }
        else
            pThis->iShutdown = 0;
        return VINF_SUCCESS;
    }

    /* not in use. */
    return VINF_SUCCESS;
}


/**
 * Construct the DMI table.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pTable      Where to create the DMI table.
 * @param   cbMax       The max size of the DMI table.
 * @param   pUuid       Pointer to the UUID to use if the DmiUuid
 *                      configuration string isn't present.
 * @param   pCfgHandle  The handle to our config node.
 */
static int pcbiosPlantDMITable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, PRTUUID pUuid, PCFGMNODE pCfgHandle)
{
    char *pszStr = (char *)pTable;
    int iStrNr;
    int rc;
    char *pszDmiBIOSVendor, *pszDmiBIOSVersion, *pszDmiBIOSReleaseDate;
    int  iDmiBIOSReleaseMajor, iDmiBIOSReleaseMinor, iDmiBIOSFirmwareMajor, iDmiBIOSFirmwareMinor;
    char *pszDmiSystemVendor, *pszDmiSystemProduct, *pszDmiSystemVersion, *pszDmiSystemSerial, *pszDmiSystemUuid, *pszDmiSystemFamily;

#define SETSTRING(memb, str) \
    do { \
        if (!str[0]) \
          memb = 0; /* empty string */ \
        else \
        { \
          memb = iStrNr++; \
          size_t _len = strlen(str) + 1; \
          size_t _max = (size_t)(pszStr + _len - (char *)pTable) + 5; /* +1 for strtab terminator +4 for end-of-table entry */ \
          if (_max > cbMax) \
            return PDMDevHlpVMSetError(pDevIns, VERR_TOO_MUCH_DATA, RT_SRC_POS, \
                    N_("One of the DMI strings is too long. Check all bios/Dmi* configuration entries. At least %zu bytes are needed but there is no space for more than %d bytes"), _max, cbMax); \
          memcpy(pszStr, str, _len); \
          pszStr += _len; \
        } \
    } while (0)
#define READCFGSTR(name, variable, default_value) \
    do { \
        rc = CFGMR3QueryStringAlloc(pCfgHandle, name, & variable); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            variable = MMR3HeapStrDup(PDMDevHlpGetVM(pDevIns), MM_TAG_CFGM, default_value); \
        else if (RT_FAILURE(rc)) \
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                    N_("Configuration error: Querying \"" name "\" as a string failed")); \
        else if (!strcmp(variable, "<EMPTY>")) \
            variable[0] = '\0'; \
    } while (0)
#define READCFGINT(name, variable, default_value) \
    do { \
        rc = CFGMR3QueryS32(pCfgHandle, name, & variable); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            variable = default_value; \
        else if (RT_FAILURE(rc)) \
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                    N_("Configuration error: Querying \"" name "\" as a Int failed")); \
    } while (0)


    /*
     * Don't change this information otherwise Windows guests will demand re-activation!
     */
    READCFGSTR("DmiBIOSVendor",        pszDmiBIOSVendor,      "innotek GmbH");
    READCFGSTR("DmiBIOSVersion",       pszDmiBIOSVersion,     "VirtualBox");
    READCFGSTR("DmiBIOSReleaseDate",   pszDmiBIOSReleaseDate, "12/01/2006");
    READCFGINT("DmiBIOSReleaseMajor",  iDmiBIOSReleaseMajor,   0);
    READCFGINT("DmiBIOSReleaseMinor",  iDmiBIOSReleaseMinor,   0);
    READCFGINT("DmiBIOSFirmwareMajor", iDmiBIOSFirmwareMajor,  0);
    READCFGINT("DmiBIOSFirmwareMinor", iDmiBIOSFirmwareMinor,  0);
    READCFGSTR("DmiSystemVendor",      pszDmiSystemVendor,    "innotek GmbH");
    READCFGSTR("DmiSystemProduct",     pszDmiSystemProduct,   "VirtualBox");
    READCFGSTR("DmiSystemVersion",     pszDmiSystemVersion,   "1.2");
    READCFGSTR("DmiSystemSerial",      pszDmiSystemSerial,    "0");
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "DmiSystemUuid", &pszDmiSystemUuid);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pszDmiSystemUuid = NULL;
    else if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"DmiUuid\" as a string failed"));
    READCFGSTR("DmiSystemFamily",    pszDmiSystemFamily,    "Virtual Machine");

    /* DMI BIOS information */
    PDMIBIOSINF pBIOSInf         = (PDMIBIOSINF)pszStr;

    pszStr = (char *)&pBIOSInf->u8ReleaseMajor;
    pBIOSInf->header.u8Length    = RT_OFFSETOF(DMIBIOSINF, u8ReleaseMajor);

    /* don't set these fields by default for legacy compatibility */
    if (iDmiBIOSReleaseMajor != 0 || iDmiBIOSReleaseMinor != 0)
    {
        pszStr = (char *)&pBIOSInf->u8FirmwareMajor;
        pBIOSInf->header.u8Length = RT_OFFSETOF(DMIBIOSINF, u8FirmwareMajor);
        pBIOSInf->u8ReleaseMajor  = iDmiBIOSReleaseMajor;
        pBIOSInf->u8ReleaseMinor  = iDmiBIOSReleaseMinor;
        if (iDmiBIOSFirmwareMajor != 0 || iDmiBIOSFirmwareMinor != 0)
        {
            pszStr = (char *)(pBIOSInf + 1);
            pBIOSInf->header.u8Length = sizeof(DMIBIOSINF);
            pBIOSInf->u8FirmwareMajor = iDmiBIOSFirmwareMajor;
            pBIOSInf->u8FirmwareMinor = iDmiBIOSFirmwareMinor;
        }
    }

    iStrNr                       = 1;
    pBIOSInf->header.u8Type      = 0; /* BIOS Information */
    pBIOSInf->header.u16Handle   = 0x0000;
    SETSTRING(pBIOSInf->u8Vendor,  pszDmiBIOSVendor);
    SETSTRING(pBIOSInf->u8Version, pszDmiBIOSVersion);
    pBIOSInf->u16Start           = 0xE000;
    SETSTRING(pBIOSInf->u8Release, pszDmiBIOSReleaseDate);
    pBIOSInf->u8ROMSize          = 1; /* 128K */
    pBIOSInf->u64Characteristics = RT_BIT(4)   /* ISA is supported */
                                 | RT_BIT(7)   /* PCI is supported */
                                 | RT_BIT(15)  /* Boot from CD is supported */
                                 | RT_BIT(16)  /* Selectable Boot is supported */
                                 | RT_BIT(27)  /* Int 9h, 8042 Keyboard services supported */
                                 | RT_BIT(30)  /* Int 10h, CGA/Mono Video Services supported */
                                 /* any more?? */
                                 ;
    pBIOSInf->u8CharacteristicsByte1 = RT_BIT(0)   /* ACPI is supported */
                                     /* any more?? */
                                     ;
    pBIOSInf->u8CharacteristicsByte2 = 0
                                     /* any more?? */
                                     ;
    *pszStr++                    = '\0';

    /* DMI system information */
    PDMISYSTEMINF pSystemInf     = (PDMISYSTEMINF)pszStr;
    pszStr                       = (char *)(pSystemInf + 1);
    iStrNr                       = 1;
    pSystemInf->header.u8Type    = 1; /* System Information */
    pSystemInf->header.u8Length  = sizeof(*pSystemInf);
    pSystemInf->header.u16Handle = 0x0001;
    SETSTRING(pSystemInf->u8Manufacturer, pszDmiSystemVendor);
    SETSTRING(pSystemInf->u8ProductName,  pszDmiSystemProduct);
    SETSTRING(pSystemInf->u8Version,      pszDmiSystemVersion);
    SETSTRING(pSystemInf->u8SerialNumber, pszDmiSystemSerial);

    RTUUID uuid;
    if (pszDmiSystemUuid)
    {
        int rc = RTUuidFromStr(&uuid, pszDmiSystemUuid);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Invalid UUID for DMI tables specified"));
        uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
        uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
        uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
        pUuid = &uuid;
    }
    memcpy(pSystemInf->au8Uuid, pUuid, sizeof(RTUUID));

    pSystemInf->u8WakeupType     = 6; /* Power Switch */
    pSystemInf->u8SKUNumber      = 0;
    SETSTRING(pSystemInf->u8Family, pszDmiSystemFamily);
    *pszStr++                    = '\0';

    /* End-of-table marker - includes padding to account for fixed table size. */
    PDMIHDR pEndOfTable          = (PDMIHDR)pszStr;
    pEndOfTable->u8Type          = 0x7f;
    pEndOfTable->u8Length        = cbMax - ((char *)pszStr - (char *)pTable) - 2;
    pEndOfTable->u16Handle       = 0xFFFF;

    /* If more fields are added here, fix the size check in SETSTRING */

#undef SETSTRING
#undef READCFG

    MMR3HeapFree(pszDmiBIOSVendor);
    MMR3HeapFree(pszDmiBIOSVersion);
    MMR3HeapFree(pszDmiBIOSReleaseDate);
    MMR3HeapFree(pszDmiSystemVendor);
    MMR3HeapFree(pszDmiSystemProduct);
    MMR3HeapFree(pszDmiSystemVersion);
    MMR3HeapFree(pszDmiSystemSerial);
    MMR3HeapFree(pszDmiSystemUuid);
    MMR3HeapFree(pszDmiSystemFamily);

    return VINF_SUCCESS;
}
AssertCompile(VBOX_DMI_TABLE_ENTR == 3);


/**
 * Calculate a simple checksum for the MPS table.
 *
 * @param   data            data
 * @param   len             size of data
 */
static uint8_t pcbiosChecksum(const uint8_t * const au8Data, uint32_t u32Length)
{
    uint8_t u8Sum = 0;
    for (size_t i = 0; i < u32Length; ++i)
        u8Sum += au8Data[i];
    return -u8Sum;
}


/**
 * Construct the MPS table. Only applicable if IOAPIC is active!
 *
 * See ``MultiProcessor Specificatiton Version 1.4 (May 1997)'':
 *   ``1.3 Scope
 *     ...
 *     The hardware required to implement the MP specification is kept to a
 *     minimum, as follows:
 *     * One or more processors that are Intel architecture instruction set
 *       compatible, such as the CPUs in the Intel486 or Pentium processor
 *       family.
 *     * One or more APICs, such as the Intel 82489DX Advanced Programmable
 *       Interrupt Controller or the integrated APIC, such as that on the
 *       Intel Pentium 735\90 and 815\100 processors, together with a discrete
 *       I/O APIC unit.''
 * and later:
 *   ``4.3.3 I/O APIC Entries
 *     The configuration table contains one or more entries for I/O APICs.
 *     ...
 *     I/O APIC FLAGS: EN 3:0 1 If zero, this I/O APIC is unusable, and the
 *                              operating system should not attempt to access
 *                              this I/O APIC.
 *                              At least one I/O APIC must be enabled.''
 *
 * @param   pDevIns    The device instance data.
 * @param   addr       physical address in guest memory.
 */
static void pcbiosPlantMPStable(PPDMDEVINS pDevIns, uint8_t *pTable, uint16_t numCpus)
{
    /* configuration table */
    PMPSCFGTBLHEADER pCfgTab      = (MPSCFGTBLHEADER*)pTable;
    memcpy(pCfgTab->au8Signature, "PCMP", 4);
    pCfgTab->u8SpecRev             =  4;    /* 1.4 */
    memcpy(pCfgTab->au8OemId, "VBOXCPU ", 8);
    memcpy(pCfgTab->au8ProductId, "VirtualBox  ", 12);
    pCfgTab->u32OemTablePtr        =  0;
    pCfgTab->u16OemTableSize       =  0;
    pCfgTab->u16EntryCount         =  numCpus /* Processors */
                                   +  1 /* ISA Bus */
                                   +  1 /* I/O-APIC */
                                   + 16 /* Interrupts */;
    pCfgTab->u32AddrLocalApic      = 0xfee00000;
    pCfgTab->u16ExtTableLength     =  0;
    pCfgTab->u8ExtTableChecksxum   =  0;
    pCfgTab->u8Reserved            =  0;

    uint32_t u32Eax, u32Ebx, u32Ecx, u32Edx;
    uint32_t u32CPUSignature = 0x0520; /* default: Pentium 100 */
    uint32_t u32FeatureFlags = 0x0001; /* default: FPU */
    PDMDevHlpGetCpuId(pDevIns, 0, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
    if (u32Eax >= 1)
    {
        PDMDevHlpGetCpuId(pDevIns, 1, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
        u32CPUSignature = u32Eax & 0xfff;
        /* Local APIC will be enabled later so override it here. Since we provide
         * an MP table we have an IOAPIC and therefore a Local APIC. */
        u32FeatureFlags = u32Edx | X86_CPUID_FEATURE_EDX_APIC;
    }
#ifdef VBOX_WITH_SMP_GUESTS
    PMPSPROCENTRY pProcEntry       = (PMPSPROCENTRY)(pCfgTab+1);
    for (int i = 0; i<numCpus; i++)
    {
      pProcEntry->u8EntryType        = 0; /* processor entry */
      pProcEntry->u8LocalApicId      = i;
      pProcEntry->u8LocalApicVersion = 0x11;
      pProcEntry->u8CPUFlags         = (i == 0 ? 2 /* bootstrap processor */ : 0 /* application processor */) | 1 /* enabled */;
      pProcEntry->u32CPUSignature    = u32CPUSignature;
      pProcEntry->u32CPUFeatureFlags = u32FeatureFlags;
      pProcEntry->u32Reserved[0]     =
        pProcEntry->u32Reserved[1]     = 0;
      pProcEntry++;
    }
#else
    /* one processor so far */
    PMPSPROCENTRY pProcEntry       = (PMPSPROCENTRY)(pCfgTab+1);
    pProcEntry->u8EntryType        = 0; /* processor entry */
    pProcEntry->u8LocalApicId      = 0;
    pProcEntry->u8LocalApicVersion = 0x11;
    pProcEntry->u8CPUFlags         = 2 /* bootstrap processor */ | 1 /* enabled */;
    pProcEntry->u32CPUSignature    = u32CPUSignature;
    pProcEntry->u32CPUFeatureFlags = u32FeatureFlags;
    pProcEntry->u32Reserved[0]     =
    pProcEntry->u32Reserved[1]     = 0;
#endif

    /* ISA bus */
    PMPSBUSENTRY pBusEntry         = (PMPSBUSENTRY)(pProcEntry+1);
    pBusEntry->u8EntryType         = 1; /* bus entry */
    pBusEntry->u8BusId             = 0; /* this ID is referenced by the interrupt entries */
    memcpy(pBusEntry->au8BusTypeStr, "ISA   ", 6);

    /* PCI bus? */

    /* I/O-APIC.
     * MP spec: "The configuration table contains one or more entries for I/O APICs.
     *           ... At least one I/O APIC must be enabled." */
    PMPSIOAPICENTRY pIOAPICEntry   = (PMPSIOAPICENTRY)(pBusEntry+1);
    uint16_t apicId = numCpus;
    pIOAPICEntry->u8EntryType      = 2; /* I/O-APIC entry */
    pIOAPICEntry->u8Id             = apicId; /* this ID is referenced by the interrupt entries */
    pIOAPICEntry->u8Version        = 0x11;
    pIOAPICEntry->u8Flags          = 1 /* enable */;
    pIOAPICEntry->u32Addr          = 0xfec00000;

    PMPSIOIRQENTRY pIrqEntry       = (PMPSIOIRQENTRY)(pIOAPICEntry+1);
    for (int i = 0; i < 16; i++, pIrqEntry++)
    {
        pIrqEntry->u8EntryType     = 3; /* I/O interrupt entry */
        pIrqEntry->u8Type          = 0; /* INT, vectored interrupt */
        pIrqEntry->u16Flags        = 0; /* polarity of APIC I/O input signal = conforms to bus,
                                           trigger mode = conforms to bus */
        pIrqEntry->u8SrcBusId      = 0; /* ISA bus */
        pIrqEntry->u8SrcBusIrq     = i;
        pIrqEntry->u8DstIOAPICId   = apicId;
        pIrqEntry->u8DstIOAPICInt  = i;
    }

    pCfgTab->u16Length             = (uint8_t*)pIrqEntry - pTable;
    pCfgTab->u8Checksum            = pcbiosChecksum(pTable, pCfgTab->u16Length);

    AssertMsg(pCfgTab->u16Length < 0x1000 - 0x100,
              ("VBOX_MPS_TABLE_SIZE=%d, maximum allowed size is %d",
              pCfgTab->u16Length, 0x1000-0x100));

    MPSFLOATPTR floatPtr;
    floatPtr.au8Signature[0]       = '_';
    floatPtr.au8Signature[1]       = 'M';
    floatPtr.au8Signature[2]       = 'P';
    floatPtr.au8Signature[3]       = '_';
    floatPtr.u32MPSAddr            = VBOX_MPS_TABLE_BASE;
    floatPtr.u8Length              = 1; /* structure size in paragraphs */
    floatPtr.u8SpecRev             = 4; /* MPS revision 1.4 */
    floatPtr.u8Checksum            = 0;
    floatPtr.au8Feature[0]         = 0;
    floatPtr.au8Feature[1]         = 0;
    floatPtr.au8Feature[2]         = 0;
    floatPtr.au8Feature[3]         = 0;
    floatPtr.au8Feature[4]         = 0;
    floatPtr.u8Checksum            = pcbiosChecksum((uint8_t*)&floatPtr, 16);
    PDMDevHlpPhysWrite (pDevIns, 0x9fff0, &floatPtr, 16);
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) pcbiosReset(PPDMDEVINS pDevIns)
{
    PDEVPCBIOS  pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
    LogFlow(("pcbiosReset:\n"));

    if (pThis->u8IOAPIC)
        pcbiosPlantMPStable(pDevIns, pThis->au8DMIPage + VBOX_DMI_TABLE_SIZE, pThis->cCpus);
}


/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) pcbiosDestruct(PPDMDEVINS pDevIns)
{
    PDEVPCBIOS  pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
    LogFlow(("pcbiosDestruct:\n"));

    /*
     * Free MM heap pointers.
     */
    if (pThis->pu8PcBios)
    {
        MMR3HeapFree(pThis->pu8PcBios);
        pThis->pu8PcBios = NULL;
    }

    if (pThis->pszPcBiosFile)
    {
        MMR3HeapFree(pThis->pszPcBiosFile);
        pThis->pszPcBiosFile = NULL;
    }

    if (pThis->pu8LanBoot)
    {
        MMR3HeapFree(pThis->pu8LanBoot);
        pThis->pu8LanBoot = NULL;
    }

    if (pThis->pszLanBootFile)
    {
        MMR3HeapFree(pThis->pszLanBootFile);
        pThis->pszLanBootFile = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * Convert config value to DEVPCBIOSBOOT.
 *
 * @returns VBox status code.
 * @param   pCfgHandle      Configuration handle.
 * @param   pszParam        The name of the value to read.
 * @param   penmBoot        Where to store the boot method.
 */
static int pcbiosBootFromCfg(PPDMDEVINS pDevIns, PCFGMNODE pCfgHandle, const char *pszParam, DEVPCBIOSBOOT *penmBoot)
{
    char *psz;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, pszParam, &psz);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"%s\" as a string failed"),
                                   pszParam);
    if (!strcmp(psz, "DVD") || !strcmp(psz, "CDROM"))
        *penmBoot = DEVPCBIOSBOOT_DVD;
    else if (!strcmp(psz, "IDE"))
        *penmBoot = DEVPCBIOSBOOT_HD;
    else if (!strcmp(psz, "FLOPPY"))
        *penmBoot = DEVPCBIOSBOOT_FLOPPY;
    else if (!strcmp(psz, "LAN"))
        *penmBoot = DEVPCBIOSBOOT_LAN;
    else if (!strcmp(psz, "NONE"))
        *penmBoot = DEVPCBIOSBOOT_NONE;
    else
    {
        PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                            N_("Configuration error: The \"%s\" value \"%s\" is unknown"),
                            pszParam, psz);
        rc = VERR_INTERNAL_ERROR;
    }
    MMR3HeapFree(psz);
    return rc;
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
static DECLCALLBACK(int)  pcbiosConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    unsigned    i;
    PDEVPCBIOS  pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
    int         rc;
    int         cb;

    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "BootDevice0\0"
                              "BootDevice1\0"
                              "BootDevice2\0"
                              "BootDevice3\0"
                              "RamSize\0"
                              "RamHoleSize\0"
                              "HardDiskDevice\0"
                              "SataHardDiskDevice\0"
                              "SataPrimaryMasterLUN\0"
                              "SataPrimarySlaveLUN\0"
                              "SataSecondaryMasterLUN\0"
                              "SataSecondarySlaveLUN\0"
                              "FloppyDevice\0"
                              "DelayBoot\0"
                              "BiosRom\0"
                              "LanBootRom\0"
                              "PXEDebug\0"
                              "UUID\0"
                              "IOAPIC\0"
                              "NumCPUs\0"
                              "DmiBIOSVendor\0"
                              "DmiBIOSVersion\0"
                              "DmiBIOSReleaseDate\0"
                              "DmiBIOSReleaseMajor\0"
                              "DmiBIOSReleaseMinor\0"
                              "DmiBIOSFirmwareMajor\0"
                              "DmiBIOSFirmwareMinor\0"
                              "DmiSystemFamily\0"
                              "DmiSystemProduct\0"
                              "DmiSystemSerial\0"
                              "DmiSystemUuid\0"
                              "DmiSystemVendor\0"
                              "DmiSystemVersion\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuraton for  device pcbios device"));

    /*
     * Init the data.
     */
    rc = CFGMR3QueryU64(pCfgHandle, "RamSize", &pThis->cbRam);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"RamSize\" as integer failed"));

    rc = CFGMR3QueryU32Def(pCfgHandle, "RamHoleSize", &pThis->cbRamHole, MM_RAM_HOLE_SIZE_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"RamHoleSize\" as integer failed"));

    rc = CFGMR3QueryU16Def(pCfgHandle, "NumCPUs", &pThis->cCpus, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"NumCPUs\" as integer failed"));

#ifdef VBOX_WITH_SMP_GUESTS
    LogRel(("[SMP] BIOS with %u CPUs\n", pThis->cCpus));
#else
    /** @todo: move this check up in configuration chain */
    if (pThis->cCpus != 1)
    {
        LogRel(("WARNING: guest SMP not supported in this build, going UP\n"));
        pThis->cCpus = 1;
    }
#endif

    rc = CFGMR3QueryU8Def(pCfgHandle, "IOAPIC", &pThis->u8IOAPIC, 1);
    if (RT_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

    static const char * const s_apszBootDevices[] = { "BootDevice0", "BootDevice1", "BootDevice2", "BootDevice3" };
    Assert(RT_ELEMENTS(s_apszBootDevices) == RT_ELEMENTS(pThis->aenmBootDevice));
    for (i = 0; i < RT_ELEMENTS(pThis->aenmBootDevice); i++)
    {
        rc = pcbiosBootFromCfg(pDevIns, pCfgHandle, s_apszBootDevices[i], &pThis->aenmBootDevice[i]);
        if (RT_FAILURE(rc))
            return rc;
    }

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "HardDiskDevice", &pThis->pszHDDevice);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HardDiskDevice\" as a string failed"));

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "FloppyDevice", &pThis->pszFDDevice);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FloppyDevice\" as a string failed"));

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "SataHardDiskDevice", &pThis->pszSataDevice);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pThis->pszSataDevice = NULL;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"SataHardDiskDevice\" as a string failed"));

    if (pThis->pszSataDevice)
    {
        static const char * const s_apszSataDisks[] =
            { "SataPrimaryMasterLUN", "SataPrimarySlaveLUN", "SataSecondaryMasterLUN", "SataSecondarySlaveLUN" };
        Assert(RT_ELEMENTS(s_apszSataDisks) == RT_ELEMENTS(pThis->iSataHDLUN));
        for (i = 0; i < RT_ELEMENTS(pThis->iSataHDLUN); i++)
        {
            rc = CFGMR3QueryU32(pCfgHandle, s_apszSataDisks[i], &pThis->iSataHDLUN[i]);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                pThis->iSataHDLUN[i] = i;
            else if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("Configuration error: Querying \"%s\" as a string failed"), s_apszSataDisks);
        }
    }
    /*
     * Register I/O Ports and PC BIOS.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x400, 4, NULL, pcbiosIOPortWrite, pcbiosIOPortRead,
                                 NULL, NULL, "Bochs PC BIOS - Panic & Debug");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x8900, 1, NULL, pcbiosIOPortWrite, pcbiosIOPortRead,
                                 NULL, NULL, "Bochs PC BIOS - Shutdown");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Query the machine's UUID for SMBIOS/DMI use.
     */
    RTUUID  uuid;
    rc = CFGMR3QueryBytes(pCfgHandle, "UUID", &uuid, sizeof(uuid));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"UUID\" failed"));


    /* Convert the UUID to network byte order. Not entirely straightforward as parts are MSB already... */
    uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
    uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
    uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
    rc = pcbiosPlantDMITable(pDevIns, pThis->au8DMIPage, VBOX_DMI_TABLE_SIZE, &uuid, pCfgHandle);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->u8IOAPIC)
        pcbiosPlantMPStable(pDevIns, pThis->au8DMIPage + VBOX_DMI_TABLE_SIZE, pThis->cCpus);

    rc = PDMDevHlpROMRegister(pDevIns, VBOX_DMI_TABLE_BASE, _4K, pThis->au8DMIPage, false /* fShadow */, "DMI tables");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read the PXE debug logging option.
     */
    rc = CFGMR3QueryU8Def(pCfgHandle, "PXEDebug", &pThis->u8PXEDebug, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"PXEDebug\" as integer failed"));

    /*
     * Get the system BIOS ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "BiosRom", &pThis->pszPcBiosFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszPcBiosFile = NULL;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"BiosRom\" as a string failed"));
    else if (!*pThis->pszPcBiosFile)
    {
        MMR3HeapFree(pThis->pszPcBiosFile);
        pThis->pszPcBiosFile = NULL;
    }

    const uint8_t *pu8PcBiosBinary = NULL;
    uint64_t cbPcBiosBinary;
    /*
     * Determine the system BIOS ROM size, open specified ROM file in the process.
     */
    RTFILE FilePcBios = NIL_RTFILE;
    if (pThis->pszPcBiosFile)
    {
        rc = RTFileOpen(&FilePcBios, pThis->pszPcBiosFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileGetSize(FilePcBios, &pThis->cbPcBios);
            if (RT_SUCCESS(rc))
            {
                /* The following checks should be in sync the AssertReleaseMsg's below. */
                if (    RT_ALIGN(pThis->cbPcBios, _64K) != pThis->cbPcBios
                    ||  pThis->cbPcBios > 32 * _64K
                    ||  pThis->cbPcBios < _64K)
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (RT_FAILURE(rc))
        {
            /*
             * In case of failure simply fall back to the built-in BIOS ROM.
             */
            Log(("pcbiosConstruct: Failed to open system BIOS ROM file '%s', rc=%Rrc!\n", pThis->pszPcBiosFile, rc));
            RTFileClose(FilePcBios);
            FilePcBios = NIL_RTFILE;
            MMR3HeapFree(pThis->pszPcBiosFile);
            pThis->pszPcBiosFile = NULL;
        }
    }

    /*
     * Attempt to get the system BIOS ROM data from file.
     */
    if (pThis->pszPcBiosFile)
    {
        /*
         * Allocate buffer for the system BIOS ROM data.
         */
        pThis->pu8PcBios = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, pThis->cbPcBios);
        if (pThis->pu8PcBios)
        {
            rc = RTFileRead(FilePcBios, pThis->pu8PcBios, pThis->cbPcBios, NULL);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("RTFileRead(,,%d,NULL) -> %Rrc\n", pThis->cbPcBios, rc));
                MMR3HeapFree(pThis->pu8PcBios);
                pThis->pu8PcBios = NULL;
            }
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        pThis->pu8PcBios = NULL;

    /* cleanup */
    if (FilePcBios != NIL_RTFILE)
        RTFileClose(FilePcBios);

    /* If we were unable to get the data from file for whatever reason, fall
     * back to the built-in ROM image.
     */
    if (pThis->pu8PcBios == NULL)
    {
        pu8PcBiosBinary = g_abPcBiosBinary;
        cbPcBiosBinary  = g_cbPcBiosBinary;
    }
    else
    {
        pu8PcBiosBinary = pThis->pu8PcBios;
        cbPcBiosBinary  = pThis->cbPcBios;
    }

    /*
     * Map the BIOS into memory.
     * There are two mappings:
     *      1. 0x000e0000 to 0x000fffff contains the last 128 kb of the bios.
     *         The bios code might be 64 kb in size, and will then start at 0xf0000.
     *      2. 0xfffxxxxx to 0xffffffff contains the entire bios.
     */
    AssertReleaseMsg(cbPcBiosBinary >= _64K, ("cbPcBiosBinary=%#x\n", cbPcBiosBinary));
    AssertReleaseMsg(RT_ALIGN_Z(cbPcBiosBinary, _64K) == cbPcBiosBinary,
                     ("cbPcBiosBinary=%#x\n", cbPcBiosBinary));
    cb = RT_MIN(cbPcBiosBinary, 128 * _1K); /* Effectively either 64 or 128K. */
    rc = PDMDevHlpROMRegister(pDevIns, 0x00100000 - cb, cb, &pu8PcBiosBinary[cbPcBiosBinary - cb],
                              false /* fShadow */, "PC BIOS - 0xfffff");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pDevIns, (uint32_t)-(int32_t)cbPcBiosBinary, cbPcBiosBinary, pu8PcBiosBinary,
                              false /* fShadow */, "PC BIOS - 0xffffffff");
    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_VMI
    /*
     * Map the VMI BIOS into memory.
     */
    AssertReleaseMsg(g_cbVmiBiosBinary == _4K, ("cbVmiBiosBinary=%#x\n", g_cbVmiBiosBinary));
    rc = PDMDevHlpROMRegister(pDevIns, VBOX_VMI_BIOS_BASE, g_cbVmiBiosBinary, g_abVmiBiosBinary, false, "VMI BIOS");
    if (RT_FAILURE(rc))
        return rc;
#endif /* VBOX_WITH_VMI */

    /*
     * Call reset to set values and stuff.
     */
    pcbiosReset(pDevIns);

    /*
     * Get the LAN boot ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "LanBootRom", &pThis->pszLanBootFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszLanBootFile = NULL;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LanBootRom\" as a string failed"));
    else if (!*pThis->pszLanBootFile)
    {
        MMR3HeapFree(pThis->pszLanBootFile);
        pThis->pszLanBootFile = NULL;
    }

    uint64_t cbFileLanBoot;
    const uint8_t *pu8LanBootBinary = NULL;
    uint64_t cbLanBootBinary;

    /*
     * Determine the LAN boot ROM size, open specified ROM file in the process.
     */
    RTFILE FileLanBoot = NIL_RTFILE;
    if (pThis->pszLanBootFile)
    {
        rc = RTFileOpen(&FileLanBoot, pThis->pszLanBootFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileGetSize(FileLanBoot, &cbFileLanBoot);
            if (RT_SUCCESS(rc))
            {
                if (    RT_ALIGN(cbFileLanBoot, _4K) != cbFileLanBoot
                    ||  cbFileLanBoot > _64K)
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (RT_FAILURE(rc))
        {
            /*
             * Ignore failure and fall back to the built-in LAN boot ROM.
             */
            Log(("pcbiosConstruct: Failed to open LAN boot ROM file '%s', rc=%Rrc!\n", pThis->pszLanBootFile, rc));
            RTFileClose(FileLanBoot);
            FileLanBoot = NIL_RTFILE;
            MMR3HeapFree(pThis->pszLanBootFile);
            pThis->pszLanBootFile = NULL;
        }
    }

    /*
     * Get the LAN boot ROM data.
     */
    if (pThis->pszLanBootFile)
    {
        /*
         * Allocate buffer for the LAN boot ROM data.
         */
        pThis->pu8LanBoot = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, cbFileLanBoot);
        if (pThis->pu8LanBoot)
        {
            rc = RTFileRead(FileLanBoot, pThis->pu8LanBoot, cbFileLanBoot, NULL);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("RTFileRead(,,%d,NULL) -> %Rrc\n", cbFileLanBoot, rc));
                MMR3HeapFree(pThis->pu8LanBoot);
                pThis->pu8LanBoot = NULL;
            }
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        pThis->pu8LanBoot = NULL;

    /* cleanup */
    if (FileLanBoot != NIL_RTFILE)
        RTFileClose(FileLanBoot);

    /* If we were unable to get the data from file for whatever reason, fall
     * back to the built-in LAN boot ROM image.
     */
    if (pThis->pu8LanBoot == NULL)
    {
        pu8LanBootBinary = g_abNetBiosBinary;
        cbLanBootBinary  = g_cbNetBiosBinary;
    }
    else
    {
        pu8LanBootBinary = pThis->pu8LanBoot;
        cbLanBootBinary  = cbFileLanBoot;
    }

    /*
     * Map the Network Boot ROM into memory.
     * Currently there is a fixed mapping: 0x000c8000 to 0x000cffff contains
     * the (up to) 32 kb ROM image.
     */
    if (pu8LanBootBinary)
        rc = PDMDevHlpROMRegister(pDevIns, VBOX_LANBOOT_SEG << 4, cbLanBootBinary, pu8LanBootBinary,
                                  true /* fShadow */, "Net Boot ROM");

    rc = CFGMR3QueryU8Def(pCfgHandle, "DelayBoot", &pThis->uBootDelay, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                    N_("Configuration error: Querying \"DelayBoot\" as integer failed"));
    if (pThis->uBootDelay > 15)
        pThis->uBootDelay = 15;

    return rc;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePcBios =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "pcbios",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "PC BIOS Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64,
    /* fClass */
    PDM_DEVREG_CLASS_ARCH_BIOS,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVPCBIOS),
    /* pfnConstruct */
    pcbiosConstruct,
    /* pfnDestruct */
    pcbiosDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pcbiosReset,
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
    /* pfnInitComplete. */
    pcbiosInitComplete,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

