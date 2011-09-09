/* $Id$ */
/** @file
 * PC BIOS Device.
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PC_BIOS
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include "VBoxDD.h"
#include "VBoxDD2.h"
#include "DevPcBios.h"
#include "DevFwCommon.h"

#define NET_BOOT_DEVS   4


/** @page pg_devbios_cmos_assign    CMOS Assignments (BIOS)
 *
 * The BIOS uses a CMOS to store configuration data.
 * It is currently used as follows:
 *
 * @verbatim
  First CMOS bank (offsets 0x00 to 0x7f):
    Floppy drive type:
         0x10
    Hard disk type (old):
         0x12
    Equipment byte:
         0x14
    Base memory:
         0x15
         0x16
    Extended memory:
         0x17
         0x18
         0x30
         0x31
    First IDE HDD:
         0x19
         0x1e - 0x25
    Second IDE HDD:
         0x1a
         0x26 - 0x2d
    Checksum of 0x10-0x2d:
         0x2e
         0x2f
    Amount of memory above 16M and below 4GB in 64KB units:
         0x34
         0x35
    Boot device (BOCHS BIOS specific):
         0x38
         0x3c
         0x3d
    PXE debug:
         0x3f
    First SATA HDD:
         0x40 - 0x47
    Second SATA HDD:
         0x48 - 0x4f
    Third SATA HDD:
         0x50 - 0x57
    Fourth SATA HDD:
         0x58 - 0x5f
    Number of CPUs:
         0x60
    RAM above 4G in 64KB units:
         0x61 - 0x65
    Third IDE HDD:
         0x67 - 0x6e
    Fourth IDE HDD:
         0x70 - 0x77

  Second CMOS bank (offsets 0x80 to 0xff):
    Reserved for internal use by PXE ROM:
         0x80 - 0x81
    First net boot device PCI bus/dev/fn:
         0x82 - 0x83
    Second to third net boot devices:
         0x84 - 0x89
@endverbatim
 *
 * @todo Mark which bits are compatible with which BIOSes and
 *       which are our own definitions.
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
    uint32_t        cbPcBios;
    /** The name of the BIOS ROM file. */
    char           *pszPcBiosFile;
    /** The LAN boot ROM data. */
    uint8_t        *pu8LanBoot;
    /** The name of the LAN boot ROM file. */
    char           *pszLanBootFile;
    /** The size of the LAN boot ROM. */
    uint64_t        cbLanBoot;
    /** The DMI tables. */
    uint8_t         au8DMIPage[0x1000];
    /** The boot countdown (in seconds). */
    uint8_t         uBootDelay;
    /** I/O-APIC enabled? */
    uint8_t         u8IOAPIC;
    /** PXE debug logging enabled? */
    uint8_t         u8PXEDebug;
    /** PXE boot PCI bus/dev/fn list. */
    uint16_t        au16NetBootDev[NET_BOOT_DEVS];
    /** Number of logical CPUs in guest */
    uint16_t        cCpus;
    uint32_t        u32McfgBase;
    uint32_t        cbMcfgLength;
} DEVPCBIOS, *PDEVPCBIOS;


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
    Assert(off < 256);
    Assert(u32Val < 256);

    int rc = PDMDevHlpCMOSWrite(pDevIns, off, u32Val);
    AssertRC(rc);
}

/**
 * Read from CMOS memory.
 * This is used by the init complete code.
 */
static uint8_t pcbiosCmosRead(PPDMDEVINS pDevIns, int off)
{
    uint8_t     u8val;

    Assert(off < 256);

    int rc = PDMDevHlpCMOSRead(pDevIns, off, &u8val);
    AssertRC(rc);

    return u8val;
}

/* -=-=-=-=-=-=- based on code from pc.c -=-=-=-=-=-=- */

/**
 * Initializes the CMOS data for one harddisk.
 */
static void pcbiosCmosInitHardDisk(PPDMDEVINS pDevIns, int offType, int offInfo, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    Log2(("%s: offInfo=%#x: LCHS=%d/%d/%d\n", __FUNCTION__, offInfo, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    if (offType)
        pcbiosCmosWrite(pDevIns, offType, 47);
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
        || LCHSGeometry.cHeads == 0
        || LCHSGeometry.cHeads > 255
        || LCHSGeometry.cSectors == 0
        || LCHSGeometry.cSectors > 63)
    {
        PPDMIBLOCK pBlock;
        pBlock = PDMIBASE_QUERY_INTERFACE(pBase, PDMIBLOCK);
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
     * Network boot device list.
     */
    for (i = 0; i < NET_BOOT_DEVS; ++i)
    {
        pcbiosCmosWrite(pDevIns, 0x82 + i * 2, pThis->au16NetBootDev[i] & 0xff);
        pcbiosCmosWrite(pDevIns, 0x83 + i * 2, pThis->au16NetBootDev[i] >> 8);
    }

    /*
     * Floppy drive type.
     */
    for (i = 0; i < RT_ELEMENTS(apFDs); i++)
    {
        PPDMIBASE pBase;
        int rc = PDMR3QueryLun(pVM, pThis->pszFDDevice, 0, i, &pBase);
        if (RT_SUCCESS(rc))
            apFDs[i] = PDMIBASE_QUERY_INTERFACE(pBase, PDMIBLOCKBIOS);
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
            apHDs[i] = PDMIBASE_QUERY_INTERFACE(pBase, PDMIBLOCKBIOS);
        if (   apHDs[i]
            && (   apHDs[i]->pfnGetType(apHDs[i]) != PDMBLOCKTYPE_HARD_DISK
                || !apHDs[i]->pfnIsVisible(apHDs[i])))
            apHDs[i] = NULL;
        if (apHDs[i])
        {
            PDMMEDIAGEOMETRY LCHSGeometry;
            int rc2 = setLogicalDiskGeometry(pBase, apHDs[i], &LCHSGeometry);
            AssertRC(rc2);

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
                apHDs[i] = PDMIBASE_QUERY_INTERFACE(pBase, PDMIBLOCKBIOS);
            if (   apHDs[i]
                && (   apHDs[i]->pfnGetType(apHDs[i]) != PDMBLOCKTYPE_HARD_DISK
                    || !apHDs[i]->pfnIsVisible(apHDs[i])))
                apHDs[i] = NULL;
            if (apHDs[i])
            {
                PDMMEDIAGEOMETRY LCHSGeometry;
                rc = setLogicalDiskGeometry(pBase, apHDs[i], &LCHSGeometry);
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

    /* Calculate and store AT-style CMOS checksum. */
    uint16_t    cksum = 0;
    for (i = 0x10; i < 0x2e; ++i)
        cksum += pcbiosCmosRead(pDevIns, i);
    pcbiosCmosWrite(pDevIns, 0x2e, cksum >> 8);
    pcbiosCmosWrite(pDevIns, 0x2f, cksum & 0xff);

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
        FwCommonPlantMpsFloatPtr(pDevIns);

    /*
     * Re-shadow the LAN ROM image and make it RAM/RAM.
     *
     * This is normally done by the BIOS code, but since we're currently lacking
     * the chipset support for this we do it here (and in the constructor).
     */
    uint32_t    cPages = RT_ALIGN_64(pThis->cbLanBoot, PAGE_SIZE) >> PAGE_SHIFT;
    RTGCPHYS    GCPhys = VBOX_LANBOOT_SEG << 4;
    while (cPages > 0)
    {
        uint8_t abPage[PAGE_SIZE];
        int     rc;

        /* Read the (original) ROM page and write it back to the RAM page. */
        rc = PDMDevHlpROMProtectShadow(pDevIns, GCPhys, PAGE_SIZE, PGMROMPROT_READ_ROM_WRITE_RAM);
        AssertLogRelRC(rc);

        rc = PDMDevHlpPhysRead(pDevIns, GCPhys, abPage, PAGE_SIZE);
        AssertLogRelRC(rc);
        if (RT_FAILURE(rc))
            memset(abPage, 0xcc, sizeof(abPage));

        rc = PDMDevHlpPhysWrite(pDevIns, GCPhys, abPage, PAGE_SIZE);
        AssertLogRelRC(rc);

        /* Switch to the RAM/RAM mode. */
        rc = PDMDevHlpROMProtectShadow(pDevIns, GCPhys, PAGE_SIZE, PGMROMPROT_READ_RAM_WRITE_RAM);
        AssertLogRelRC(rc);

        /* Advance */
        GCPhys += PAGE_SIZE;
        cPages--;
    }
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
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

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

    if (pThis->pszHDDevice)
    {
        MMR3HeapFree(pThis->pszHDDevice);
        pThis->pszHDDevice = NULL;
    }

    if (pThis->pszFDDevice)
    {
        MMR3HeapFree(pThis->pszFDDevice);
        pThis->pszFDDevice = NULL;
    }

    if (pThis->pszSataDevice)
    {
        MMR3HeapFree(pThis->pszSataDevice);
        pThis->pszSataDevice = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * Convert config value to DEVPCBIOSBOOT.
 *
 * @returns VBox status code.
 * @param   pCfg            Configuration handle.
 * @param   pszParam        The name of the value to read.
 * @param   penmBoot        Where to store the boot method.
 */
static int pcbiosBootFromCfg(PPDMDEVINS pDevIns, PCFGMNODE pCfg, const char *pszParam, DEVPCBIOSBOOT *penmBoot)
{
    char *psz;
    int rc = CFGMR3QueryStringAlloc(pCfg, pszParam, &psz);
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
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)  pcbiosConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    unsigned    i;
    PDEVPCBIOS  pThis = PDMINS_2_DATA(pDevIns, PDEVPCBIOS);
    int         rc;
    int         cb;

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
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
                              "McfgBase\0"
                              "McfgLength\0"
                              "DmiBIOSVendor\0"
                              "DmiBIOSVersion\0"
                              "DmiBIOSReleaseDate\0"
                              "DmiBIOSReleaseMajor\0"
                              "DmiBIOSReleaseMinor\0"
                              "DmiBIOSFirmwareMajor\0"
                              "DmiBIOSFirmwareMinor\0"
                              "DmiSystemSKU\0"
                              "DmiSystemFamily\0"
                              "DmiSystemProduct\0"
                              "DmiSystemSerial\0"
                              "DmiSystemUuid\0"
                              "DmiSystemVendor\0"
                              "DmiSystemVersion\0"
                              "DmiChassisVendor\0"
                              "DmiChassisVersion\0"
                              "DmiChassisSerial\0"
                              "DmiChassisAssetTag\0"
#ifdef VBOX_WITH_DMI_OEMSTRINGS
                              "DmiOEMVBoxVer\0"
                              "DmiOEMVBoxRev\0"
#endif
                              "DmiUseHostInfo\0"
                              "DmiExposeMemoryTable\0"
                              ))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for device pcbios device"));

    /*
     * Init the data.
     */
    rc = CFGMR3QueryU64(pCfg, "RamSize", &pThis->cbRam);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"RamSize\" as integer failed"));

    rc = CFGMR3QueryU32Def(pCfg, "RamHoleSize", &pThis->cbRamHole, MM_RAM_HOLE_SIZE_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"RamHoleSize\" as integer failed"));

    rc = CFGMR3QueryU16Def(pCfg, "NumCPUs", &pThis->cCpus, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"NumCPUs\" as integer failed"));

    rc = CFGMR3QueryU32Def(pCfg, "McfgBase", &pThis->u32McfgBase, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"\" as integer failed"));
    rc = CFGMR3QueryU32Def(pCfg, "McfgLength", &pThis->cbMcfgLength, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"McfgLength\" as integer failed"));


    LogRel(("[SMP] BIOS with %u CPUs\n", pThis->cCpus));

    rc = CFGMR3QueryU8Def(pCfg, "IOAPIC", &pThis->u8IOAPIC, 1);
    if (RT_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

    static const char * const s_apszBootDevices[] = { "BootDevice0", "BootDevice1", "BootDevice2", "BootDevice3" };
    Assert(RT_ELEMENTS(s_apszBootDevices) == RT_ELEMENTS(pThis->aenmBootDevice));
    for (i = 0; i < RT_ELEMENTS(pThis->aenmBootDevice); i++)
    {
        rc = pcbiosBootFromCfg(pDevIns, pCfg, s_apszBootDevices[i], &pThis->aenmBootDevice[i]);
        if (RT_FAILURE(rc))
            return rc;
    }

    rc = CFGMR3QueryStringAlloc(pCfg, "HardDiskDevice", &pThis->pszHDDevice);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HardDiskDevice\" as a string failed"));

    rc = CFGMR3QueryStringAlloc(pCfg, "FloppyDevice", &pThis->pszFDDevice);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FloppyDevice\" as a string failed"));

    rc = CFGMR3QueryStringAlloc(pCfg, "SataHardDiskDevice", &pThis->pszSataDevice);
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
            rc = CFGMR3QueryU32(pCfg, s_apszSataDisks[i], &pThis->iSataHDLUN[i]);
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
    rc = CFGMR3QueryBytes(pCfg, "UUID", &uuid, sizeof(uuid));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"UUID\" failed"));


    /* Convert the UUID to network byte order. Not entirely straightforward as parts are MSB already... */
    uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
    uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
    uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
    rc = FwCommonPlantDMITable(pDevIns, pThis->au8DMIPage,
                               VBOX_DMI_TABLE_SIZE, &uuid, pCfg);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->u8IOAPIC)
        FwCommonPlantMpsTable(pDevIns, pThis->au8DMIPage + VBOX_DMI_TABLE_SIZE,
                              _4K - VBOX_DMI_TABLE_SIZE, pThis->cCpus);

    rc = PDMDevHlpROMRegister(pDevIns, VBOX_DMI_TABLE_BASE, _4K, pThis->au8DMIPage, _4K,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "DMI tables");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read the PXE debug logging option.
     */
    rc = CFGMR3QueryU8Def(pCfg, "PXEDebug", &pThis->u8PXEDebug, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"PXEDebug\" as integer failed"));

    /* Clear the net boot device list. All bits set invokes old behavior,
     * as if no second CMOS bank was present.
     */
    memset(&pThis->au16NetBootDev, 0xff, sizeof(pThis->au16NetBootDev));

    /*
     * Determine the network boot order.
     */
    PCFGMNODE pCfgNetBoot = CFGMR3GetChild(pCfg, "NetBoot");
    if (pCfgNetBoot == NULL)
    {
        /* Do nothing. */
        rc = VINF_SUCCESS;
    }
    else
    {
        PCFGMNODE   pCfgNetBootDevice;
        uint8_t     u8PciBus;
        uint8_t     u8PciDev;
        uint8_t     u8PciFn;
        uint16_t    u16BusDevFn;
        char        szIndex[] = "?";

        Assert(pCfgNetBoot);
        for (i = 0; i < NET_BOOT_DEVS; ++i)
        {
            szIndex[0] = '0' + i;
            pCfgNetBootDevice = CFGMR3GetChild(pCfgNetBoot, szIndex);

            rc = CFGMR3QueryU8(pCfgNetBootDevice, "PCIBusNo", &u8PciBus);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
            {
                /* Do nothing and stop iterating. */
                rc = VINF_SUCCESS;
                break;
            }
            else if (RT_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc,
                                        N_("Configuration error: Querying \"Netboot/x/PCIBusNo\" as integer failed"));
            rc = CFGMR3QueryU8(pCfgNetBootDevice, "PCIDeviceNo", &u8PciDev);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
            {
                /* Do nothing and stop iterating. */
                rc = VINF_SUCCESS;
                break;
            }
            else if (RT_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc,
                                        N_("Configuration error: Querying \"Netboot/x/PCIDeviceNo\" as integer failed"));
            rc = CFGMR3QueryU8(pCfgNetBootDevice, "PCIFunctionNo", &u8PciFn);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
            {
                /* Do nothing and stop iterating. */
                rc = VINF_SUCCESS;
                break;
            }
            else if (RT_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc,
                                        N_("Configuration error: Querying \"Netboot/x/PCIFunctionNo\" as integer failed"));
            u16BusDevFn = (((uint16_t)u8PciBus) << 8) | ((u8PciDev & 0x1F) << 3) | (u8PciFn & 0x7);
            pThis->au16NetBootDev[i] = u16BusDevFn;
        }
    }

    /*
     * Get the system BIOS ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "BiosRom", &pThis->pszPcBiosFile);
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

    const uint8_t  *pu8PcBiosBinary;
    uint32_t        cbPcBiosBinary;
    if (pThis->pszPcBiosFile)
    {
        /*
         * Load the BIOS ROM.
         */
        RTFILE hFilePcBios;
        rc = RTFileOpen(&hFilePcBios, pThis->pszPcBiosFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            /* Figure the size and check restrictions. */
            uint64_t cbPcBios;
            rc = RTFileGetSize(hFilePcBios, &cbPcBios);
            if (RT_SUCCESS(rc))
            {
                pThis->cbPcBios = (uint32_t)cbPcBios;
                if (    RT_ALIGN(pThis->cbPcBios, _64K) == pThis->cbPcBios
                    &&  pThis->cbPcBios == cbPcBios
                    &&  pThis->cbPcBios <= 32 * _64K
                    &&  pThis->cbPcBios >= _64K)
                {
                    pThis->pu8PcBios = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, pThis->cbPcBios);
                    if (pThis->pu8PcBios)
                    {
                        rc = RTFileRead(hFilePcBios, pThis->pu8PcBios, pThis->cbPcBios, NULL);
                        if (RT_FAILURE(rc))
                            rc = PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                                     N_("Error reading the BIOS image ('%s)"), pThis->pszPcBiosFile);
                    }
                    else
                        rc = PDMDevHlpVMSetError(pDevIns, VERR_NO_MEMORY, RT_SRC_POS,
                                                 N_("Failed to allocate %#x bytes for loading the BIOS image"),
                                                 pThis->cbPcBios);
                }
                else
                    rc = PDMDevHlpVMSetError(pDevIns, VERR_OUT_OF_RANGE, RT_SRC_POS,
                                             N_("Invalid system BIOS file size ('%s'): %#llx (%llu)"),
                                             pThis->pszPcBiosFile, cbPcBios, cbPcBios);
            }
            else
                rc = PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, N_("Failed to query the system BIOS file size ('%s')"),
                                         pThis->pszPcBiosFile);
            RTFileClose(hFilePcBios);
        }
        else
            rc = PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, N_("Failed to open system BIOS file '%s'"), pThis->pszPcBiosFile);
        if (RT_FAILURE(rc))
            return rc;

        pu8PcBiosBinary = pThis->pu8PcBios;
        cbPcBiosBinary  = pThis->cbPcBios;
        LogRel(("Using BIOS ROM '%s' with a size of %#x bytes\n", pThis->pszPcBiosFile, pThis->cbPcBios));
    }
    else
    {
        /*
         * Use the embedded BIOS ROM image.
         */
        pu8PcBiosBinary = g_abPcBiosBinary;
        cbPcBiosBinary  = g_cbPcBiosBinary;
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
    rc = PDMDevHlpROMRegister(pDevIns, 0x00100000 - cb, cb, &pu8PcBiosBinary[cbPcBiosBinary - cb], cb,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "PC BIOS - 0xfffff");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pDevIns, (uint32_t)-(int32_t)cbPcBiosBinary, cbPcBiosBinary, pu8PcBiosBinary, cbPcBiosBinary,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "PC BIOS - 0xffffffff");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Get the LAN boot ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "LanBootRom", &pThis->pszLanBootFile);
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
                if (cbFileLanBoot > _64K - (VBOX_LANBOOT_SEG << 4 & 0xffff))
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
        pThis->pu8LanBoot = (uint8_t *)PDMDevHlpMMHeapAllocZ(pDevIns, cbFileLanBoot);
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
#ifdef VBOX_WITH_PXE_ROM
        pu8LanBootBinary = g_abNetBiosBinary;
        cbLanBootBinary  = g_cbNetBiosBinary;
#endif
    }
    else
    {
        pu8LanBootBinary = pThis->pu8LanBoot;
        cbLanBootBinary  = cbFileLanBoot;
    }

    /*
     * Map the Network Boot ROM into memory.
     * Currently there is a fixed mapping: 0x000d2000 to 0x000dffff contains
     * the (up to) 56 kb ROM image.  The mapping size is fixed to trouble with
     * the saved state (in PGM).
     */
    if (pu8LanBootBinary)
    {
        pThis->cbLanBoot = cbLanBootBinary;

        rc = PDMDevHlpROMRegister(pDevIns, VBOX_LANBOOT_SEG << 4,
                                  RT_MAX(cbLanBootBinary, _64K - (VBOX_LANBOOT_SEG << 4 & 0xffff)),
                                  pu8LanBootBinary, cbLanBootBinary,
                                  PGMPHYS_ROM_FLAGS_SHADOWED, "Net Boot ROM");
        AssertRCReturn(rc, rc);
    }

    rc = CFGMR3QueryU8Def(pCfg, "DelayBoot", &pThis->uBootDelay, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                    N_("Configuration error: Querying \"DelayBoot\" as integer failed"));
    if (pThis->uBootDelay > 15)
        pThis->uBootDelay = 15;

    /*
     * Call reset plant tables and shadow the PXE ROM.
     */
    pcbiosReset(pDevIns);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePcBios =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
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
