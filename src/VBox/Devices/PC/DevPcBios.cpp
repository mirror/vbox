/** @file
 *
 * VBox basic PC devices:
 * PC BIOS device
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PC_BIOS
#include <VBox/pdm.h>
#include <VBox/mm.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <VBox/err.h>

#include "vl_vbox.h"
#include "Builtins.h"
#include "Builtins2.h"
#include "DevPcBios.h"


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
    /** Ram Size (in bytes). */
    uint64_t        cbRam;
    /** Bochs shutdown index. */
    uint32_t        iShutdown;
    /** Floppy device. */
    char           *pszFDDevice;
    /** Harddisk device. */
    char           *pszHDDevice;
    /** Bios message buffer. */
    char            szMsg[256];
    /** Bios message buffer index. */
    uint32_t        iMsg;
    /** Current logo data memory bank. */
    uint8_t         u8LogoBank;
    /** The size of the BIOS logo data. */
    uint32_t        cbLogo;
    /** The BIOS logo data. */
    uint8_t        *pu8Logo;
    /** The name of the logo file. */
    char           *pszLogoFile;
    /** The LAN boot ROM data. */
    uint8_t        *pu8LanBoot;
    /** The name of the LAN boot ROM file. */
    char           *pszLanBootFile;
    /** The DMI tables. */
    uint8_t        au8DMIPage[0x1000];
    /** The boot countdown (in seconds). */
    uint8_t        uBootDelay;
} DEVPCBIOS, *PDEVPCBIOS;


/** @todo The logo stuff shared with the BIOS goes into a header of course. */

/**
 * PC Bios logo data structure.
 */
#pragma pack(2) /* pack(2) is important! (seems that bios compiled with pack(2)...) */
typedef struct LOGOHDR
{
    /** Signature (0x66BB). */
    uint16_t        u16Signature;
    /** Fade in - boolean. */
    uint8_t         u8FadeIn;
    /** Fade out - boolean. */
    uint8_t         u8FadeOut;
    /** Logo time (msec). */
    uint16_t        u16LogoMillies;
    /** Show setup - boolean. */
    uint8_t         u8ShowBootMenu;
    /** Logo file size. */
    uint32_t        cbLogo;
} LOGOHDR, *PLOGOHDR;
#pragma pack()

/** The value of the LOGOHDR::u16Signature field. */
#define LOGOHDR_MAGIC   0x66BB

/** Size of a logo bank. */
#define LOGO_BANK_SIZE      0xffff
/** Logo offset mask. */
#define LOGO_BANK_OFFSET    0xffff
/** The last bank for custom logo data. */
#define LOGO_BANK_LAST      254
/** The bank which will give you the default logo. */
#define LOGO_BANK_DEFAULT_LOGO 255

#pragma pack(1)

typedef struct DMIHDR
{
    uint8_t         u8Type;
    uint8_t         u8Length;
    uint16_t        u16Handle;
} *PDMIHDR;

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
} *PDMIBIOSINF;

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

#pragma pack()


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

static DECLCALLBACK(int) logoMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
static DECLCALLBACK(int) logoMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);

__END_DECLS


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
    PVM pVM = pDevIns->pDevHlp->pfnGetVM(pDevIns);
    IOMIOPortWrite(pVM, 0x70, off, 1);
    IOMIOPortWrite(pVM, 0x71, u32Val, 1);
    IOMIOPortWrite(pVM, 0x70, 0, 1);
#endif
}

/* -=-=-=-=-=-=- based on code from pc.c -=-=-=-=-=-=- */

/**
 * Initializes the CMOS data for one harddisk.
 */
static void pcbiosCmosInitHardDisk(PPDMDEVINS pDevIns, int offType, int offInfo, PPDMIBLOCKBIOS pBlockBios)
{
    if (    pBlockBios->pfnGetType(pBlockBios) == PDMBLOCKTYPE_HARD_DISK
        &&  pBlockBios->pfnIsVisible(pBlockBios))
    {
        uint32_t    cCylinders;
        uint32_t    cHeads;
        uint32_t    cSectors;
        int rc = pBlockBios->pfnGetGeometry(pBlockBios, &cCylinders, &cHeads, &cSectors);
        if (VBOX_SUCCESS(rc))
        {
            Log2(("pcbiosCmosInitHardDisk: offInfo=%#x: CHS=%d/%d/%d\n", offInfo, cCylinders, cHeads, cSectors));
            pcbiosCmosWrite(pDevIns, offType, 47);                              /* 19h - First Extended Hard Disk Drive Type */
            pcbiosCmosWrite(pDevIns, offInfo + 0, cCylinders & 0xff);           /* 1Bh - (AMI) First Hard Disk (type 47) user defined: # of Cylinders, LSB */
            pcbiosCmosWrite(pDevIns, offInfo + 1, cCylinders >> 8);             /* 1Ch - (AMI) First Hard Disk user defined: # of Cylinders, High Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 2, cHeads);                      /* 1Dh - (AMI) First Hard Disk user defined: Number of Heads */
            pcbiosCmosWrite(pDevIns, offInfo + 3, 0xff);                        /* 1Eh - (AMI) First Hard Disk user defined: Write Precompensation Cylinder, Low Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 4, 0xff);                        /* 1Fh - (AMI) First Hard Disk user defined: Write Precompensation Cylinder, High Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 5, 0xc0 | ((cHeads > 8) << 3));  /* 20h - (AMI) First Hard Disk user defined: Control Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 6, cCylinders & 0xff);           /* 21h - (AMI) First Hard Disk user defined: Landing Zone, Low Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 7, cCylinders >> 8);             /* 22h - (AMI) First Hard Disk user defined: Landing Zone, High Byte */
            pcbiosCmosWrite(pDevIns, offInfo + 8, cSectors);                    /* 23h - (AMI) First Hard Disk user defined: # of Sectors per track */
            return;
        }
    }
    pcbiosCmosWrite(pDevIns, offType, 0);
}


/**
 * Get BIOS boot code from enmBootDevice in order
 *
 * @todo r=bird: This is a rather silly function since the conversion is 1:1.
 */
static uint8_t getBiosBootCode(PDEVPCBIOS pData, unsigned iOrder)
{
    switch (pData->aenmBootDevice[iOrder])
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
            AssertMsgFailed(("aenmBootDevice[%d]=%d\n", iOrder, pData->aenmBootDevice[iOrder]));
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
    PDEVPCBIOS      pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    uint32_t        u32;
    unsigned        i;
    PVM             pVM = pDevIns->pDevHlp->pfnGetVM(pDevIns);
    PPDMIBLOCKBIOS  apHDs[4] = {0};
    PPDMIBLOCKBIOS  apFDs[2] = {0};
    AssertRelease(pVM);
    LogFlow(("pcbiosInitComplete:\n"));

    /*
     * Memory sizes.
     */
    /* base memory. */
    u32 = pData->cbRam > 640 ? 640 : (uint32_t)pData->cbRam / _1K;
    pcbiosCmosWrite(pDevIns, 0x15, u32 & 0xff);                                 /* 15h - Base Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x16, u32 >> 8);                                   /* 16h - Base Memory in K, High Byte */

    /* Extended memory, up to 65MB */
    u32 = pData->cbRam >= 65 * _1M ? 0xffff : ((uint32_t)pData->cbRam - _1M) / _1K;
    pcbiosCmosWrite(pDevIns, 0x17, u32 & 0xff);                                 /* 17h - Extended Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x18, u32 >> 8);                                   /* 18h - Extended Memory in K, High Byte */
    pcbiosCmosWrite(pDevIns, 0x30, u32 & 0xff);                                 /* 30h - Extended Memory in K, Low Byte */
    pcbiosCmosWrite(pDevIns, 0x31, u32 >> 8);                                   /* 31h - Extended Memory in K, High Byte */

    /* Bochs BIOS specific? Anyway, it's the amount of memory above 16MB */
    if (pData->cbRam > 16 * _1M)
    {
        u32 = (uint32_t)( (pData->cbRam - 16 * _1M) / _64K );
        u32 = RT_MIN(u32, 0xffff);
    }
    else
        u32 = 0;
    pcbiosCmosWrite(pDevIns, 0x34, u32 & 0xff);
    pcbiosCmosWrite(pDevIns, 0x35, u32 >> 8);

    /*
     * Bochs BIOS specifics - boot device.
     * We do both new and old (ami-style) settings.
     * See rombios.c line ~7215 (int19_function).
     */

    uint8_t reg3d = getBiosBootCode(pData, 0) | (getBiosBootCode(pData, 1) << 4);
    uint8_t reg38 = /* pcbiosCmosRead(pDevIns, 0x38) | */ getBiosBootCode(pData, 2) << 4;
    /* This is an extension. Bochs BIOS normally supports only 3 boot devices. */
    uint8_t reg3c = getBiosBootCode(pData, 3) | (pData->uBootDelay << 4);
    pcbiosCmosWrite(pDevIns, 0x3d, reg3d);
    pcbiosCmosWrite(pDevIns, 0x38, reg38);
    pcbiosCmosWrite(pDevIns, 0x3c, reg3c);

    /*
     * Floppy drive type.
     */
    for (i = 0; i < ELEMENTS(apFDs); i++)
    {
        PPDMIBASE pBase;
        int rc = PDMR3QueryLun(pVM, pData->pszFDDevice, 0, i, &pBase);
        if (VBOX_SUCCESS(rc))
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
        case 1: u32 = 0x01; break;      /* floppy installed, 1 drive. */
        case 2: u32 = 0x41; break;      /* floppy installed, 2 drives. */
        default:u32 = 0;    break;      /* floppy not installed. */
    }
    u32 |= BIT(1);                      /* math coprocessor installed  */
    u32 |= BIT(2);                      /* keyboard enabled (or mouse?) */
    u32 |= BIT(3);                      /* display enabled (monitory type is 0, i.e. vga) */
    pcbiosCmosWrite(pDevIns, 0x14, u32);                                        /* 14h - Equipment Byte */

    /*
     * Harddisks.
     */
    for (i = 0; i < ELEMENTS(apHDs); i++)
    {
        PPDMIBASE pBase;
        int rc = PDMR3QueryLun(pVM, pData->pszHDDevice, 0, i, &pBase);
        if (VBOX_SUCCESS(rc))
            apHDs[i] = (PPDMIBLOCKBIOS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_BLOCK_BIOS);
    }

    u32 = (apHDs[0] ? 0xf0 : 0) | (apHDs[1] ? 0x0f : 0);                        /* 0Fh means extended and points to 1Ah, 1Bh */
    pcbiosCmosWrite(pDevIns, 0x12, u32);                                        /* 12h - Hard Disk Data (type) */
    if (apHDs[0])
        pcbiosCmosInitHardDisk(pDevIns, 0x19, 0x1b, apHDs[0]);                  /* 19h - First Extended Hard Disk Drive Type */
    if (apHDs[1])
        pcbiosCmosInitHardDisk(pDevIns, 0x1a, 0x24, apHDs[1]);                  /* 1Ah - Second Extended Hard Disk Drive Type */

    /*
     * Translation type - Bochs BIOS specific.
     */
    u32 = 0;
    for (i = 0; i < 4; i++)
    {
        if (apHDs[i])
        {
            PDMBIOSTRANSLATION enmTranslation;
            int rc = apHDs[i]->pfnGetTranslation(apHDs[i], &enmTranslation);
            if (VBOX_FAILURE(rc) || enmTranslation == PDMBIOSTRANSLATION_AUTO)
            {
                uint32_t cCylinders, cHeads, cSectors;
                rc = apHDs[i]->pfnGetGeometry(apHDs[i], &cCylinders, &cHeads, &cSectors);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsg(rc == VERR_PDM_MEDIA_NOT_MOUNTED, ("This shouldn't happen! rc=%Vrc\n", rc));
                    enmTranslation = PDMBIOSTRANSLATION_NONE;
                }
                else if (cCylinders <= 1024 && cHeads <= 16 && cSectors <= 63)
                    enmTranslation = PDMBIOSTRANSLATION_NONE;
                else
                    enmTranslation = PDMBIOSTRANSLATION_LBA;
            }
            switch (enmTranslation)
            {
                case PDMBIOSTRANSLATION_AUTO: /* makes gcc happy */
                case PDMBIOSTRANSLATION_NONE:
                    /* u32 |= 0 << (i * 2) */
                    break;
                default:
                    AssertMsgFailed(("bad enmTranslation=%d\n", enmTranslation));
                case PDMBIOSTRANSLATION_LBA:
                    u32 |= 1 << (i * 2);
                    break;
            }
        }
    }
    Log2(("pcbiosInitComplete: translation byte: %#02x\n", u32));
    pcbiosCmosWrite(pDevIns, 0x39, u32);

    LogFlow(("pcbiosInitComplete: returns VINF_SUCCESS\n"));
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
        PDEVPCBIOS pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
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
            pData->szMsg[pData->iMsg] = '\0';
            if (pData->iMsg)
                Log(("pcbios: %s\n", pData->szMsg));
            pData->iMsg = 0;
        }
        else
        {
            if (pData->iMsg >= sizeof(pData->szMsg))
            {
                pData->szMsg[pData->iMsg] = '\0';
                Log(("pcbios: %s\n", pData->szMsg));
                pData->iMsg = 0;
            }
            pData->szMsg[pData->iMsg] = (char )u32;
            pData->szMsg[++pData->iMsg] = '\0';
        }
        return VINF_SUCCESS;
    }

    /*
     * Bochs BIOS shutdown request.
     */
    if (cb == 1 && Port == 0x8900)
    {
        static const unsigned char szShutdown[] = "Shutdown";
        PDEVPCBIOS pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
        if (u32 == szShutdown[pData->iShutdown])
        {
            pData->iShutdown++;
            if (pData->iShutdown == 8)
            {
#if 0
                PVM pVM = pDevIns->pDevHlp->pfnGetVM(pDevIns);
                AssertRelease(pVM);
                VM_FF_SET(pVM, VM_FF_TERMINATE);
#endif
                pData->iShutdown = 0;
                return VINF_EM_TERMINATE;
            }
        }
        else
            pData->iShutdown = 0;
        return VINF_SUCCESS;
    }

    /* not in use. */
    return VINF_SUCCESS;
}


/**
 * Legacy LOGO memory (0xd0000 - 0xdffff) read hook, to be called from IOM.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to read.
 * @param   pv          Where to store readed data.
 * @param   cb          Bytes to read.
 */
static DECLCALLBACK(int) logoMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PDEVPCBIOS  pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    Log(("logoMMIORead call GCPhysAddr:%x pv:%x cb:%d (%d)\n", GCPhysAddr, pv, cb, pData->u8LogoBank));

    uint32_t offLogo = GCPhysAddr & LOGO_BANK_OFFSET;
    if (pData->u8LogoBank != LOGO_BANK_DEFAULT_LOGO)
    {
        /*
         * Banked logo.
         */
        offLogo += pData->u8LogoBank * LOGO_BANK_SIZE;
        if (    offLogo > pData->cbLogo
            ||  offLogo + cb > pData->cbLogo)
        {
            Log(("logoMMIORead: Requested address is out of Logo data!!! offLogo=%#x(%d) cbLogo=%#x(%d)\n",
                 offLogo, offLogo, pData->cbLogo, pData->cbLogo));
            return VINF_SUCCESS;
        }

        memcpy(pv, &pData->pu8Logo[offLogo], cb);
        Log(("logoMMIORead: offLogo=%#x(%d) cb=%#x %.*Vhxs\n", offLogo, offLogo, cb, cb, &pData->pu8Logo[offLogo]));
    }
    else
    {
        /*
         * Default bios logo.
         */
        if (offLogo > g_cbPcDefBiosLogo || offLogo + cb > g_cbPcDefBiosLogo)
        {
            Log(("logoMMIORead: Requested address is out of Logo data!!! offLogo=%#x(%d) max:%#x(%d)\n",
                 offLogo, offLogo, g_cbPcDefBiosLogo, g_cbPcDefBiosLogo));
            return VINF_SUCCESS;
        }

        memcpy(pv, &g_abPcDefBiosLogo[offLogo], cb);
        Log(("logoMMIORead: offLogo=%#x(%d) cb=%#x %.*Vhxs\n", offLogo, offLogo, cb, cb, &g_abPcDefBiosLogo[offLogo]));
    }

    return VINF_SUCCESS;
}


/**
 * Legacy LOGO memory (0xd0000 - 0xdffff) write hook, to be called from IOM.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to write.
 * @param   pv          Pointer to data.
 * @param   cb          Bytes to write.
 */
static DECLCALLBACK(int) logoMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PDEVPCBIOS  pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    Log(("logoMMIOWrite: GCPhysAddr=%x cb=%d pv[0]=%#04x (byte)\n", GCPhysAddr, pv, cb, *(uint8_t *)pv));

    /*
     * Byte write to off 0: Switch the logo bank.
     */
    if ((GCPhysAddr & LOGO_BANK_OFFSET) == 0 && cb == 1)
    {
        uint8_t u8Bank = *(uint8_t *)pv;
        uint32_t off = u8Bank * LOGO_BANK_SIZE;

        if (    u8Bank != LOGO_BANK_DEFAULT_LOGO
            &&  off > pData->cbLogo)
        {
            Log(("logoMMIOWrite: The requested bank is outside the logo image! (cbLogo=%d off=%d)\n", pData->cbLogo, off));
            return VINF_SUCCESS;
        }

        pData->u8LogoBank = u8Bank;
    }

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
    PDEVPCBIOS  pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    LogFlow(("pcbiosReset:\n"));

    pData->u8LogoBank = 0;
    /** @todo Should we perhaps do pcbiosInitComplete() on reset? */

#if 1
    /*
     * Paranoia: Check that the BIOS ROM hasn't changed.
     */
    PVM pVM = pDevIns->pDevHlp->pfnGetVM(pDevIns);
    /* the low ROM mapping. */
    unsigned cb = RT_MIN(g_cbPcBiosBinary, 128 * _1K);
    const uint8_t *pb1 = (uint8_t *)MMPhysGCPhys2HCVirt(pVM, 0x00100000 - cb);
    AssertRelease(pb1);
    const uint8_t *pb2 = &g_abPcBiosBinary[g_cbPcBiosBinary - cb];
    if (memcmp(pb1, pb2, cb))
    {
        AssertMsg2("low ROM mismatch! cb=%#x\n", cb);
        for (unsigned off = 0; off < cb; off++)
            if (pb1[off] != pb2[off])
                AssertMsg2("%05x: %02x expected %02x\n", off, pb1[off], pb2[off]);
        AssertReleaseFailed();
    }

    /* the high ROM mapping. */
    pb1 = (uint8_t *)MMPhysGCPhys2HCVirt(pVM, (uint32_t)-g_cbPcBiosBinary);
    AssertRelease(pb1);
    pb2 = &g_abPcBiosBinary[0];
    if (memcmp(pb1, pb2, g_cbPcBiosBinary))
    {
        AssertMsg2("high ROM mismatch! g_cbPcBiosBinary=%#x\n", g_cbPcBiosBinary);
        for (unsigned off = 0; off < g_cbPcBiosBinary; off++)
            if (pb1[off] != pb2[off])
                AssertMsg2("%05x: %02x expected %02x\n", off, pb1[off], pb2[off]);
        AssertReleaseFailed();
    }
#endif
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
    PDEVPCBIOS  pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    LogFlow(("pcbiosDestruct:\n"));

    /*
     * Free MM heap pointers.
     */
    if (pData->pu8LanBoot)
    {
        MMR3HeapFree(pData->pu8LanBoot);
        pData->pu8LanBoot = NULL;
    }

    if (pData->pszLanBootFile)
    {
        MMR3HeapFree(pData->pszLanBootFile);
        pData->pszLanBootFile = NULL;
    }

    if (pData->pu8Logo)
    {
        MMR3HeapFree(pData->pu8Logo);
        pData->pu8Logo = NULL;
    }

    if (pData->pszLogoFile)
    {
        MMR3HeapFree(pData->pszLogoFile);
        pData->pszLogoFile = NULL;
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
    if (VBOX_FAILURE(rc))
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
                            N_("Configuration error: The \"%s\" value \"%s\" is unknown.\n"),
                            pszParam, psz);
        rc = VERR_INTERNAL_ERROR;
    }
    MMR3HeapFree(psz);
    return rc;
}

#define STRCPY(p, s) do { memcpy (p, s, sizeof(s)); p += sizeof(s); } while (0)
static void pcbiosPlantDMITable(uint8_t *table)
{
    char *pszStr = (char*)table;
    int iStrNr;

    PDMIBIOSINF pBIOSInf         = (PDMIBIOSINF)pszStr;
    pszStr                       = (char*)(pBIOSInf+1);
    iStrNr                       = 1;
    pBIOSInf->header.u8Type      = 0; /* BIOS Information */
    pBIOSInf->header.u8Length    = sizeof(*pBIOSInf);
    pBIOSInf->header.u16Handle   = 0x0000;
    pBIOSInf->u8Vendor           = iStrNr++;
    STRCPY(pszStr, "InnoTek Systemberatung GmbH");
    pBIOSInf->u8Version          = iStrNr++;
    STRCPY(pszStr, "VirtualBox");
    pBIOSInf->u16Start           = 0xE000;
    pBIOSInf->u8Release          = iStrNr++;
    STRCPY(pszStr, "12/01/2006");
    pBIOSInf->u8ROMSize          = 1; /* 128K */
    pBIOSInf->u64Characteristics = BIT(4)   /* ISA is supported */
                                 | BIT(7)   /* PCI is supported */
                                 | BIT(15)  /* Boot from CD is supported */
                                 | BIT(16)  /* Selectable Boot is supported */
                                 | BIT(27)  /* Int 9h, 8042 Keyboard services supported */
                                 | BIT(30)  /* Int 10h, CGA/Mono Video Services supported */
                                 /* any more?? */
                                 ;
    pBIOSInf->u8CharacteristicsByte1 = BIT(0)   /* ACPI is supported */
                                     /* any more?? */
                                     ;
    pBIOSInf->u8CharacteristicsByte2 = 0
                                     /* any more?? */
                                     ;
    *pszStr++                    = '\0';


    PDMISYSTEMINF pSystemInf     = (PDMISYSTEMINF)pszStr;
    pszStr                       = (char*)(pSystemInf+1);
    iStrNr                       = 1;
    pSystemInf->header.u8Type    = 1; /* System Information */
    pSystemInf->header.u8Length  = sizeof(*pSystemInf);
    pSystemInf->header.u16Handle = 0x0001;
    pSystemInf->u8Manufacturer   = iStrNr++;
    STRCPY(pszStr, "InnoTek Systemberatung GmbH");
    pSystemInf->u8ProductName    = iStrNr++;
    STRCPY(pszStr, "VirtualBox");
    pSystemInf->u8Version        = iStrNr++;
    STRCPY(pszStr, "1.2");
    pSystemInf->u8SerialNumber   = iStrNr++;
    STRCPY(pszStr, "0");
    pSystemInf->u8WakeupType     = 6; /* Power Switch */
    pSystemInf->u8SKUNumber      = 0;
    pSystemInf->u8Family         = iStrNr++;
    STRCPY(pszStr, "Virtual Machine");
    *pszStr++                    = '\0';

    AssertMsg(pszStr - (char*)table == VBOX_DMI_TABLE_SIZE,
              ("VBOX_DMI_TABLE_SIZE=%d, actual DMI table size is %d",
              VBOX_DMI_TABLE_SIZE, pszStr - (char*)table));
}

AssertCompile(VBOX_DMI_TABLE_ENTR == 2);


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
    PDEVPCBIOS  pData = PDMINS2DATA(pDevIns, PDEVPCBIOS);
    int         rc;
    int         cb;
    // char       *psz;

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
                              "HardDiskDevice\0"
                              "FloppyDevice\0"
                              "FadeIn\0"
                              "FadeOut\0"
                              "LogoTime\0"
                              "LogoFile\0"
                              "ShowBootMenu\0"
                              "DelayBoot\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuraton for  device pcbios device"));

    /*
     * Init the data.
     */
    rc = CFGMR3QueryU64(pCfgHandle, "RamSize", &pData->cbRam);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"RamSize\" as integer failed"));

    static const char * const s_apszBootDevices[] = { "BootDevice0", "BootDevice1", "BootDevice2", "BootDevice3" };
    Assert(ELEMENTS(s_apszBootDevices) == ELEMENTS(pData->aenmBootDevice));
    for (i = 0; i < ELEMENTS(pData->aenmBootDevice); i++)
    {
        rc = pcbiosBootFromCfg(pDevIns, pCfgHandle, s_apszBootDevices[i], &pData->aenmBootDevice[i]);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "HardDiskDevice", &pData->pszHDDevice);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HardDiskDevice\" as a string failed"));

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "FloppyDevice", &pData->pszFDDevice);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FloppyDevice\" as a string failed"));

    /*
     * Register I/O Ports and PC BIOS.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x400, 4, NULL, pcbiosIOPortWrite, pcbiosIOPortRead,
                                 NULL, NULL, "Bochs PC BIOS - Panic & Debug");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x8900, 1, NULL, pcbiosIOPortWrite, pcbiosIOPortRead,
                                 NULL, NULL, "Bochs PC BIOS - Shutdown");
    if (VBOX_FAILURE(rc))
        return rc;

    pcbiosPlantDMITable(pData->au8DMIPage);

    rc = PDMDevHlpROMRegister(pDevIns, VBOX_DMI_TABLE_BASE, 0x1000, pData->au8DMIPage, "DMI tables");
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Map the BIOS into memory.
     * There are two mappings:
     *      1. 0x000e0000 to 0x000fffff contains the last 128 kb of the bios.
     *         The bios code might be 64 kb in size, and will then start at 0xf0000.
     *      2. 0xfffxxxxx to 0xffffffff contains the entire bios.
     */
    AssertReleaseMsg(g_cbPcBiosBinary >= _64K, ("g_cbPcBiosBinary=%#x\n", g_cbPcBiosBinary));
    AssertReleaseMsg(RT_ALIGN_Z(g_cbPcBiosBinary, _64K) == g_cbPcBiosBinary,
                     ("g_cbPcBiosBinary=%#x\n", g_cbPcBiosBinary));
    cb = RT_MIN(g_cbPcBiosBinary, 128 * _1K);
    rc = PDMDevHlpROMRegister(pDevIns, 0x00100000 - cb, cb,
                              &g_abPcBiosBinary[g_cbPcBiosBinary - cb], "PC BIOS - 0xfffff");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pDevIns, (uint32_t)-g_cbPcBiosBinary, g_cbPcBiosBinary, 
                              &g_abPcBiosBinary[0], "PC BIOS - 0xffffffff");
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Register the MMIO region for the BIOS Logo: 0x000d0000 to 0x000dffff (64k)
     */
    rc = PDMDevHlpMMIORegister(pDevIns, 0x000d0000, 0x00010000, 0, 
                               logoMMIOWrite, logoMMIORead, NULL, "PC BIOS - Logo Buffer");
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Construct the logo header.
     */
    LOGOHDR LogoHdr = { LOGOHDR_MAGIC, 0, 0, 0, 0, 0 };

    rc = CFGMR3QueryU8(pCfgHandle, "FadeIn", &LogoHdr.u8FadeIn);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.u8FadeIn = 1;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FadeIn\" as integer failed"));

    rc = CFGMR3QueryU8(pCfgHandle, "FadeOut", &LogoHdr.u8FadeOut);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.u8FadeOut = 1;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FadeOut\" as integer failed"));

    rc = CFGMR3QueryU16(pCfgHandle, "LogoTime", &LogoHdr.u16LogoMillies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.u16LogoMillies = 1;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LogoTime\" as integer failed"));

    rc = CFGMR3QueryU8(pCfgHandle, "ShowBootMenu", &LogoHdr.u8ShowBootMenu);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.u8ShowBootMenu = 0;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"ShowBootMenu\" as integer failed"));

    /*
     * Get the Logo file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "LogoFile", &pData->pszLogoFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->pszLogoFile = NULL;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LogoFile\" as a string failed"));
    else if (!*pData->pszLogoFile)
    {
        MMR3HeapFree(pData->pszLogoFile);
        pData->pszLogoFile = NULL;
    }

    /*
     * Determine the logo size, open any specified logo file in the process.
     */
    LogoHdr.cbLogo = g_cbPcDefBiosLogo;
    RTFILE FileLogo = NIL_RTFILE;
    if (pData->pszLogoFile)
    {
        rc = RTFileOpen(&FileLogo, pData->pszLogoFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (VBOX_SUCCESS(rc))
        {
            uint64_t cbFile;
            rc = RTFileGetSize(FileLogo, &cbFile);
            if (VBOX_SUCCESS(rc))
            {
                if (    cbFile > 0
                    &&  cbFile < ((LOGO_BANK_LAST + 1) * LOGO_BANK_SIZE) - sizeof(LogoHdr))
                    LogoHdr.cbLogo = (uint32_t)cbFile;
                else
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (VBOX_FAILURE(rc))
        {
            /*
             * Ignore failure and fall back to the default logo.
             */
            LogRel(("pcbiosConstruct: Failed to open logo file '%s', rc=%Vrc!\n", pData->pszLogoFile, rc));
            RTFileClose(FileLogo);
            FileLogo = NIL_RTFILE;
            MMR3HeapFree(pData->pszLogoFile);
            pData->pszLogoFile = NULL;
        }
    }

    /*
     * Allocate buffer for the logo data.
     * RT_MAX() is applied to let us fall back to default logo on read failure.
     */
    pData->cbLogo = sizeof(LogoHdr) + LogoHdr.cbLogo;
    pData->pu8Logo = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, RT_MAX(pData->cbLogo, g_cbPcDefBiosLogo + sizeof(LogoHdr)));
    if (pData->pu8Logo)
    {
        /*
         * Write the logo header.
         */
        PLOGOHDR pLogoHdr = (PLOGOHDR)pData->pu8Logo;
        *pLogoHdr = LogoHdr;

        /*
         * Write the logo bitmap.
         */
        if (pData->pszLogoFile)
        {
            rc = RTFileRead(FileLogo, pLogoHdr + 1, LogoHdr.cbLogo, NULL);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("RTFileRead(,,%d,NULL) -> %Vrc\n", LogoHdr.cbLogo, rc));
                pLogoHdr->cbLogo = LogoHdr.cbLogo = g_cbPcDefBiosLogo;
                memcpy(pLogoHdr + 1, g_abPcDefBiosLogo, LogoHdr.cbLogo);
            }
        }
        else
            memcpy(pLogoHdr + 1, g_abPcDefBiosLogo, LogoHdr.cbLogo);

        /*
         * Call reset to set values and stuff.
         */
        pcbiosReset(pDevIns);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    /* cleanup */
    if (FileLogo != NIL_RTFILE)
        RTFileClose(FileLogo);

    /*
     * Get the LAN boot ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "LanBootRom", &pData->pszLanBootFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pData->pszLanBootFile = NULL;
        rc = VINF_SUCCESS;
    }
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LanBootRom\" as a string failed"));
    else if (!*pData->pszLanBootFile)
    {
        MMR3HeapFree(pData->pszLanBootFile);
        pData->pszLanBootFile = NULL;
    }

    const uint8_t *pu8LanBoot = NULL;
    uint64_t cbFileLanBoot;
#ifdef VBOX_DO_NOT_LINK_LANBOOT
    /*
     * Determine the LAN boot ROM size, open specified ROM file in the process.
     */
    RTFILE FileLanBoot = NIL_RTFILE;
    if (pData->pszLanBootFile)
    {
        rc = RTFileOpen(&FileLanBoot, pData->pszLanBootFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (VBOX_SUCCESS(rc))
        {
            rc = RTFileGetSize(FileLanBoot, &cbFileLanBoot);
            if (VBOX_SUCCESS(rc))
            {
                if (    RT_ALIGN(cbFileLanBoot, _4K) != cbFileLanBoot
                    ||  cbFileLanBoot > _32K)
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (VBOX_FAILURE(rc))
        {
            /*
             * Ignore failure and fall back to no LAN boot ROM.
             */
            Log(("pcbiosConstruct: Failed to open LAN boot ROM file '%s', rc=%Vrc!\n", pData->pszLanBootFile, rc));
            RTFileClose(FileLanBoot);
            FileLanBoot = NIL_RTFILE;
            MMR3HeapFree(pData->pszLanBootFile);
            pData->pszLanBootFile = NULL;
        }
    }

    /*
     * Get the LAN boot ROM data.
     */
    if (pData->pszLanBootFile)
    {
        /*
         * Allocate buffer for the LAN boot ROM data.
         */
        pu8LanBoot = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, cbFileLanBoot);
        pData->pu8LanBoot = pu8LanBoot;
        if (pu8LanBoot)
        {
            rc = RTFileRead(FileLanBoot, pData->pu8LanBoot, cbFileLanBoot, NULL);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("RTFileRead(,,%d,NULL) -> %Vrc\n", cbFileLanBoot, rc));
                MMR3HeapFree(pu8LanBoot);
                pu8LanBoot = NULL;
                pData->pu8LanBoot = NULL;
            }
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        pData->pu8LanBoot = NULL;

    /* cleanup */
    if (FileLanBoot != NIL_RTFILE)
        RTFileClose(FileLanBoot);

#else /* !VBOX_DO_NOT_LINK_LANBOOT */
    pData->pu8LanBoot = NULL;
    pu8LanBoot = g_abNetBiosBinary;
    cbFileLanBoot = g_cbNetBiosBinary;
#endif /* !VBOX_DO_NOT_LINK_LANBOOT */

    /*
     * Map the Network Boot ROM into memory.
     * Currently there is a fixed mapping: 0x000c8000 to 0x000cffff contains
     * the (up to) 32 kb ROM image.
     */
    if (pu8LanBoot)
        rc = PDMDevHlpROMRegister(pDevIns, VBOX_LANBOOT_SEG << 4, cbFileLanBoot, pu8LanBoot, "Net Boot ROM");

    rc = CFGMR3QueryU8(pCfgHandle, "DelayBoot", &pData->uBootDelay);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pData->uBootDelay = 0;
        rc = VINF_SUCCESS;
    }
    else
    {
        if (VBOX_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc,
                                    N_("Configuration error: Querying \"DelayBoot\" as integer failed"));
        if (pData->uBootDelay > 15)
            pData->uBootDelay = 15;
    }

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
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Bochs PC BIOS",
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
    pcbiosInitComplete
};

