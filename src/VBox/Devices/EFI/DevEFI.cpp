/* $Id$ */
/** @file
 * DevEFI - EFI <-> VirtualBox Integration Framework.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_DEV_EFI

#include <VBox/pdmdev.h>
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/dbgf.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/string.h>
#ifdef DEBUG
# include <iprt/stream.h>
# define DEVEFI_WITH_VBOXDBG_SCRIPT
#endif

#include "Firmware2/VBoxPkg/Include/DevEFI.h"
#include "../Builtins.h"
#include "../Builtins2.h"
#include "../PC/DevFwCommon.h"

/* EFI includes */
#include <ProcessorBind.h>
#include <Common/UefiBaseTypes.h>
#include <Common/PiFirmwareVolume.h>
#include <Common/PiFirmwareFile.h>
#include <IndustryStandard/PeImage.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct DEVEFI
{
    /** Pointer back to the device instance. */
    PPDMDEVINS      pDevIns;
    /** EFI message buffer. */
    char            szMsg[VBOX_EFI_DEBUG_BUFFER];
    /** EFI message buffer index. */
    uint32_t        iMsg;
    /** EFI panic message buffer. */
    char            szPanicMsg[2048];
    /** EFI panic message buffer index. */
    uint32_t        iPanicMsg;
    /** The system EFI ROM data. */
    uint8_t        *pu8EfiRom;
    /** The size of the system EFI ROM. */
    uint64_t        cbEfiRom;
    /** The name of the EFI ROM file. */
    char           *pszEfiRomFile;
    /** Thunk page pointer. */
    uint8_t        *pu8EfiThunk;
    /** First entry point of the EFI firmware */
    RTGCPHYS        GCEntryPoint0;
    /* Second Entry Point (PeiCore)*/
    RTGCPHYS        GCEntryPoint1;
    /** EFI firmware physical load address */
    RTGCPHYS        GCLoadAddress;
    /** Current info selector */
    uint32_t        iInfoSelector;
    /** Current info position */
    int32_t         iInfoPosition;

    /** Number of virtual CPUs. (Config) */
    uint32_t        cCpus;
    /** RAM below 4GB (in bytes). (Config) */
    uint32_t        cbBelow4GB;
    /** RAM above 4GB (in bytes). (Config) */
    uint64_t        cbAbove4GB;

    uint64_t cbRam;

    uint64_t cbRamHole;

    /** The DMI tables. */
    uint8_t         au8DMIPage[0x1000];

    /** I/O-APIC enabled? */
    uint8_t         u8IOAPIC;
} DEVEFI;
typedef DEVEFI *PDEVEFI;

/**
 * Write to CMOS memory.
 * This is used by the init complete code.
 */
static void cmosWrite(PPDMDEVINS pDevIns, int off, uint32_t u32Val)
{
    Assert(off < 128);
    Assert(u32Val < 256);

    int rc = PDMDevHlpCMOSWrite(pDevIns, off, u32Val);
    AssertRC(rc);
}


static uint32_t efiInfoSize(PDEVEFI pThis)
{
    /* So far, everything is 4 bytes, as we only support 32-bit EFI */
    switch (pThis->iInfoSelector)
    {
        case EFI_INFO_INDEX_VOLUME_BASE:
        case EFI_INFO_INDEX_VOLUME_SIZE:
        case EFI_INFO_INDEX_TEMPMEM_BASE:
        case EFI_INFO_INDEX_TEMPMEM_SIZE:
        case EFI_INFO_INDEX_STACK_BASE:
        case EFI_INFO_INDEX_STACK_SIZE:
            return 4;
    }
    Assert(false);
    return 0;
}

static uint8_t efiInfoNextByte(PDEVEFI pThis)
{
    uint32_t iValue;
    switch (pThis->iInfoSelector)
    {
        case EFI_INFO_INDEX_VOLUME_BASE:
            iValue = pThis->GCLoadAddress;
            break;
        case EFI_INFO_INDEX_VOLUME_SIZE:
            iValue = pThis->cbEfiRom;
            break;
        case EFI_INFO_INDEX_TEMPMEM_BASE:
            iValue = VBOX_EFI_TOP_OF_STACK; /* just after stack */
            break;
        case EFI_INFO_INDEX_TEMPMEM_SIZE:
            iValue = 512 * 1024; /* 512 K */
            break;
        case EFI_INFO_INDEX_STACK_BASE:
            /* Keep in sync with value in EfiThunk.asm */
            iValue = VBOX_EFI_TOP_OF_STACK - 128*1024; /* 2M - 128 K */
            break;
        case EFI_INFO_INDEX_STACK_SIZE:
            iValue = 128*1024; /* 128 K */
            break;
        default:
            Assert(false);
            iValue = 0;
            break;
    }
    /* somewhat ugly, but works atm */
    return *((uint8_t*)&iValue+pThis->iInfoPosition);
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
static DECLCALLBACK(int) efiIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    Log4(("EFI in: %x %x\n", Port, cb));

    switch (Port)
    {
        case EFI_INFO_PORT:
            if (pThis->iInfoPosition == -1 && cb == 4)
            {
                *pu32 = efiInfoSize(pThis);
                pThis->iInfoPosition = 0;
            }
            else
            {
                /* So far */
                if (cb != 1)
                    return VERR_IOM_IOPORT_UNUSED;
                *pu32 = efiInfoNextByte(pThis);
                pThis->iInfoPosition++;
            }
            return VINF_SUCCESS;
    }

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
static DECLCALLBACK(int) efiIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    Log4(("efi: out %x %x %d\n", Port, u32, cb));

    switch (Port)
    {
        case EFI_INFO_PORT:
            pThis->iInfoSelector = u32;
            pThis->iInfoPosition = -1;
            break;
        case EFI_DEBUG_PORT:
        {
            /* The raw version. */
            switch (u32)
            {
                case '\r': Log3(("efi: <return>\n")); break;
                case '\n': Log3(("efi: <newline>\n")); break;
                case '\t': Log3(("efi: <tab>\n")); break;
                default:   Log3(("efi: %c (%02x)\n", u32, u32)); break;
            }
            /* The readable, buffered version. */
            if (u32 == '\n' || u32 == '\r')
            {
                pThis->szMsg[pThis->iMsg] = '\0';
                if (pThis->iMsg)
                {
                    Log(("efi: %s\n", pThis->szMsg));
#ifdef DEVEFI_WITH_VBOXDBG_SCRIPT
                    const char *pszVBoxDbg = strstr(pThis->szMsg, "VBoxDbg> ");
                    if (pszVBoxDbg)
                    {
                        pszVBoxDbg += sizeof("VBoxDbg> ") - 1;

                        PRTSTREAM pStrm;
                        int rc = RTStrmOpen("./DevEFI.VBoxDbg", "a", &pStrm);
                        if (RT_SUCCESS(rc))
                        {
                            RTStrmPutStr(pStrm, pszVBoxDbg);
                            RTStrmPutCh(pStrm, '\n');
                            RTStrmClose(pStrm);
                        }
                    }
#endif
                }
                pThis->iMsg = 0;
            }
            else
            {
                if (pThis->iMsg >= sizeof(pThis->szMsg)-1)
                {
                    pThis->szMsg[pThis->iMsg] = '\0';
                    Log(("efi: %s\n", pThis->szMsg));
                    pThis->iMsg = 0;
                }
                pThis->szMsg[pThis->iMsg] = (char )u32;
                pThis->szMsg[++pThis->iMsg] = '\0';
            }
            break;
        }

        case EFI_PANIC_PORT:
        {
            switch (u32)
            {
                case EFI_PANIC_CMD_BAD_ORG:
                    LogRel(("EFI Panic: You have to fix ORG offset in EfiThunk.asm! Must be 0x%x\n",
                            g_cbEfiThunkBinary));
                    AssertMsg2("Fix ORG offset in EfiThunk.asm: must be 0x%x\n",
                               g_cbEfiThunkBinary);
                    break;

                case EFI_PANIC_CMD_THUNK_TRAP:
                    LogRel(("EFI Panic: Unexpected trap!!\n"));
#ifdef VBOX_STRICT
                    return PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "EFI Panic: Unexpected trap during early bootstrap!\n");
#else
                    AssertReleaseMsgFailed(("Unexpected trap during early EFI bootstrap!!\n"));
#endif
                    break;

                case EFI_PANIC_CMD_START_MSG:
                    pThis->iPanicMsg = 0;
                    pThis->szPanicMsg[0] = '\0';
                    break;

                case EFI_PANIC_CMD_END_MSG:
                    LogRel(("EFI Panic: %s\n", pThis->szPanicMsg));
#ifdef VBOX_STRICT
                    return PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "EFI Panic: %s\n", pThis->szPanicMsg);
#else
                    return VERR_INTERNAL_ERROR;
#endif

                default:
                    if (    u32 >= EFI_PANIC_CMD_MSG_FIRST
                        &&  u32 <= EFI_PANIC_CMD_MSG_LAST)
                    {
                        /* Add the message char to the buffer. */
                        uint32_t i = pThis->iPanicMsg;
                        if (i + 1 < sizeof(pThis->szPanicMsg))
                        {
                            char ch = EFI_PANIC_CMD_MSG_GET_CHAR(u32);
                            if (    ch == '\n'
                                &&  i > 0
                                &&  pThis->szPanicMsg[i - 1] == '\r')
                                i--;
                            pThis->szPanicMsg[i] = ch;
                            pThis->szPanicMsg[i + 1] = '\0';
                            pThis->iPanicMsg = i + 1;
                        }
                    }
                    else
                        Log(("EFI: Unknown panic command: %#x (cb=%d)\n", u32, cb));
                    break;
            }
            break;
        }

        default:
            Log(("EFI: Write to reserved port %RTiop: %#x (cb=%d)\n", Port, u32, cb));
            break;
    }
    return VINF_SUCCESS;
}

/**
 * Init complete notification.
 *
 * @returns VBOX status code.
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(int) efiInitComplete(PPDMDEVINS pDevIns)
{
    /* PC Bios */
    PDEVEFI pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    uint32_t u32;

    /*
     * Memory sizes.
     */
    uint64_t const offRamHole = _4G - pThis->cbRamHole;
    if (pThis->cbRam > 16 * _1M)
        u32 = (uint32_t)( (RT_MIN(RT_MIN(pThis->cbRam, offRamHole), UINT32_C(0xffe00000)) - 16U * _1M) / _64K );
    else
        u32 = 0;
    cmosWrite(pDevIns, 0x34, u32 & 0xff);
    cmosWrite(pDevIns, 0x35, u32 >> 8);

    /*
     * Number of CPUs.
     */
    cmosWrite(pDevIns, 0x60, pThis->cCpus & 0xff);

    return VINF_SUCCESS;
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) efiReset(PPDMDEVINS pDevIns)
{
    PDEVEFI  pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);

    LogFlow(("efiReset\n"));

    pThis->iInfoSelector = 0;
    pThis->iInfoPosition = -1;

    pThis->iMsg = 0;
    pThis->szMsg[0] = '\0';
    pThis->iPanicMsg = 0;
    pThis->szPanicMsg[0] = '\0';

    /*
     * Re-shadow the Firmware Volume and make it RAM/RAM.
     */
    uint32_t    cPages = RT_ALIGN_64(pThis->cbEfiRom, PAGE_SIZE) >> PAGE_SHIFT;
    RTGCPHYS    GCPhys = pThis->GCLoadAddress;
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
static DECLCALLBACK(int) efiDestruct(PPDMDEVINS pDevIns)
{
    PDEVEFI  pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);

    /*
     * Free MM heap pointers.
     */
    if (pThis->pu8EfiRom)
    {
        RTFileReadAllFree(pThis->pu8EfiRom, (size_t)pThis->cbEfiRom);
        pThis->pu8EfiRom = NULL;
    }

    if (pThis->pszEfiRomFile)
    {
        MMR3HeapFree(pThis->pszEfiRomFile);
        pThis->pszEfiRomFile = NULL;
    }

    if (pThis->pu8EfiThunk)
    {
        MMR3HeapFree(pThis->pu8EfiThunk);
        pThis->pu8EfiThunk = NULL;
    }

    return VINF_SUCCESS;
}

/**
 * Helper that searches for a FFS file of a given type.
 *
 * @returns Pointer to the FFS file header if found, NULL if not.
 *
 * @param   pFfsFile    Pointer to the FFS file header to start searching at.
 * @param   pbEnd       The end of the firmware volume.
 * @param   FileType    The file type to look for.
 * @param   pcbFfsFile  Where to store the FFS file size (includes header).
 */
DECLINLINE(EFI_FFS_FILE_HEADER const *)
efiFwVolFindFileByType(EFI_FFS_FILE_HEADER const *pFfsFile, uint8_t const *pbEnd, EFI_FV_FILETYPE FileType, uint32_t *pcbFile)
{
#define FFS_SIZE(hdr)   RT_MAKE_U32_FROM_U8((hdr)->Size[0], (hdr)->Size[1], (hdr)->Size[2], 0)
    while ((uintptr_t)pFfsFile < (uintptr_t)pbEnd)
    {
        if (pFfsFile->Type == FileType)
        {
            *pcbFile = FFS_SIZE(pFfsFile);
            return pFfsFile;
        }
        pFfsFile = (EFI_FFS_FILE_HEADER *)((uintptr_t)pFfsFile + RT_ALIGN(FFS_SIZE(pFfsFile), 8));
    }
    return NULL;
}

static int efiFindEntryPoint(EFI_FFS_FILE_HEADER const *pFfsFile, uint32_t cbFfsFile, RTGCPHYS *pImageBase, uint8_t **ppbImage)
{
    /*
     * Sections headers are lays at the begining of block it describes,
     * the first section header is located immidiately after FFS header.
     */
    EFI_FILE_SECTION_POINTER uSecHdrPtr;
    uint8_t const * const    pbFfsFileEnd = (uint8_t *)pFfsFile + cbFfsFile;
    uint8_t const           *pbImage      = NULL;
    uint8_t const           *pbSecHdr     = (uint8_t const *)&pFfsFile[1]; /* FFS header has fixed size */
    for (; (uintptr_t)pbSecHdr < (uintptr_t)pbFfsFileEnd;
         pbSecHdr += SECTION_SIZE(uSecHdrPtr.CommonHeader))
    {
        uSecHdrPtr.CommonHeader = (EFI_COMMON_SECTION_HEADER *)pbSecHdr;
        if (    uSecHdrPtr.CommonHeader->Type == EFI_SECTION_PE32
            ||  uSecHdrPtr.CommonHeader->Type == EFI_SECTION_TE)
        {
            /*Here should be other code containing sections*/
            pbImage = (uint8_t const *)&uSecHdrPtr.Pe32Section[1]; /* the PE/PE+/TE headers begins just after the Section Header */
            break;
        }
        Log2(("EFI: Section of type:%d has been detected\n", uSecHdrPtr.CommonHeader->Type));
    }
    AssertLogRelMsgReturn(pbImage, ("Failed to find PE32 or TE section for the SECURITY_CORE FFS\n"), VERR_INVALID_PARAMETER);

    /*
     * Parse the image extracting the ImageBase and the EntryPoint.
     */
    int rc = VINF_SUCCESS;
    union EfiHdrUnion
    {
        EFI_IMAGE_DOS_HEADER    Dos;
        EFI_IMAGE_NT_HEADERS32  Nt32;
        EFI_IMAGE_NT_HEADERS64  Nt64;
        EFI_TE_IMAGE_HEADER     Te;
    } const *pHdr = (EfiHdrUnion const *)pbImage;

    /* Skip MZ if found. */
    if (pHdr->Dos.e_magic == RT_MAKE_U16('M',  'Z'))
    {
        uint8_t const *pbNewHdr = (uint8_t const *)pHdr + pHdr->Dos.e_lfanew;
        AssertLogRelMsgReturn(   (uintptr_t)pbNewHdr <  (uintptr_t)pbFfsFileEnd
                              && (uintptr_t)pbNewHdr >= (uintptr_t)&pHdr->Dos.e_lfanew + sizeof(pHdr->Dos.e_lfanew),
                              ("%x\n", pHdr->Dos.e_lfanew),
                              VERR_BAD_EXE_FORMAT);
        pHdr = (EfiHdrUnion const *)pbNewHdr;
    }

    RTGCPHYS ImageBase;
    RTGCPHYS EpRVA;
    if (pHdr->Nt32.Signature == RT_MAKE_U32_FROM_U8('P', 'E', 0, 0))
    {
        AssertLogRelMsgReturn(   (   pHdr->Nt32.FileHeader.Machine == EFI_IMAGE_FILE_MACHINE_I386
                                  && pHdr->Nt32.FileHeader.SizeOfOptionalHeader == sizeof(pHdr->Nt32.OptionalHeader))
                              || (   pHdr->Nt32.FileHeader.Machine == EFI_IMAGE_MACHINE_X64
                                  && pHdr->Nt32.FileHeader.SizeOfOptionalHeader == sizeof(pHdr->Nt64.OptionalHeader)),
                              ("%x / %x\n", pHdr->Nt32.FileHeader.Machine, pHdr->Nt32.FileHeader.SizeOfOptionalHeader),
                              VERR_LDR_ARCH_MISMATCH);
        if (pHdr->Nt32.FileHeader.Machine == EFI_IMAGE_FILE_MACHINE_I386)
        {
            Log2(("EFI: PE32/i386\n"));
            AssertLogRelMsgReturn(pHdr->Nt32.OptionalHeader.SizeOfImage < cbFfsFile,
                                  ("%#x / %#x\n", pHdr->Nt32.OptionalHeader.SizeOfImage, cbFfsFile),
                                  VERR_BAD_EXE_FORMAT);
            ImageBase = pHdr->Nt32.OptionalHeader.ImageBase;
            EpRVA     = pHdr->Nt32.OptionalHeader.AddressOfEntryPoint;
            AssertLogRelMsgReturn(EpRVA < pHdr->Nt32.OptionalHeader.SizeOfImage,
                                  ("%#RGp / %#x\n", EpRVA, pHdr->Nt32.OptionalHeader.SizeOfImage),
                                  VERR_BAD_EXE_FORMAT);
        }
        else
        {
            Log2(("EFI: PE+/AMD64\n"));
            AssertLogRelMsgReturn(pHdr->Nt64.OptionalHeader.SizeOfImage < cbFfsFile,
                                  ("%#x / %#x\n", pHdr->Nt64.OptionalHeader.SizeOfImage, cbFfsFile),
                                  VERR_BAD_EXE_FORMAT);
            ImageBase = pHdr->Nt64.OptionalHeader.ImageBase;
            EpRVA     = pHdr->Nt64.OptionalHeader.AddressOfEntryPoint;
            AssertLogRelMsgReturn(EpRVA < pHdr->Nt64.OptionalHeader.SizeOfImage,
                                  ("%#RGp / %#x\n", EpRVA, pHdr->Nt64.OptionalHeader.SizeOfImage),
                                  VERR_BAD_EXE_FORMAT);
        }
    }
    else if (pHdr->Te.Signature == RT_MAKE_U16('V', 'Z'))
    {
        /* TE header */
        Log2(("EFI: TE header\n"));
        AssertLogRelMsgReturn(   pHdr->Te.Machine == EFI_IMAGE_FILE_MACHINE_I386
                              || pHdr->Te.Machine == EFI_IMAGE_MACHINE_X64,
                              ("%x\n", pHdr->Te.Machine),
                              VERR_LDR_ARCH_MISMATCH);
        ImageBase = pHdr->Te.ImageBase;
        EpRVA     = pHdr->Te.AddressOfEntryPoint;
        AssertLogRelMsgReturn(EpRVA < cbFfsFile,
                              ("%#RGp / %#x\n", EpRVA, cbFfsFile),
                              VERR_BAD_EXE_FORMAT);
    }
    else
        AssertLogRelMsgFailedReturn(("%#x\n", pHdr->Nt32.Signature), VERR_INVALID_EXE_SIGNATURE);

    Log2(("EFI: EpRVA=%RGp ImageBase=%RGp EntryPoint=%RGp\n", EpRVA, ImageBase, EpRVA + ImageBase));
    if (pImageBase != NULL)
        *pImageBase = ImageBase;
    if (ppbImage != NULL)
        *ppbImage = (uint8_t *)pbImage;
    return ImageBase + EpRVA;
}

/**
 * Parse EFI ROM headers and find entry points.
 *
 * @returns VBox status.
 * @param   pThis    The device instance data.
 */
static int efiParseFirmware(PDEVEFI pThis)
{
    EFI_FIRMWARE_VOLUME_HEADER const *pFwVolHdr = (EFI_FIRMWARE_VOLUME_HEADER const *)pThis->pu8EfiRom;

    /*
     * Validate firmware volume header.
     */
    AssertLogRelMsgReturn(pFwVolHdr->Signature == RT_MAKE_U32_FROM_U8('_', 'F', 'V', 'H'),
                          ("%#x, expected %#x\n", pFwVolHdr->Signature, RT_MAKE_U32_FROM_U8('_', 'F', 'V', 'H')),
                          VERR_INVALID_MAGIC);
    AssertLogRelMsgReturn(pFwVolHdr->Revision == EFI_FVH_REVISION,
                          ("%#x, expected %#x\n", pFwVolHdr->Signature, EFI_FVH_REVISION),
                          VERR_VERSION_MISMATCH);
    /** @todo check checksum, see PE spec vol. 3 */
    AssertLogRelMsgReturn(pFwVolHdr->FvLength <= pThis->cbEfiRom,
                          ("%#llx, expected %#llx\n", pFwVolHdr->FvLength, pThis->cbEfiRom),
                          VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(      pFwVolHdr->BlockMap[0].Length > 0
                          &&    pFwVolHdr->BlockMap[0].NumBlocks > 0,
                          ("%#x, %x\n", pFwVolHdr->BlockMap[0].Length, pFwVolHdr->BlockMap[0].NumBlocks),
                          VERR_INVALID_PARAMETER);

    AssertLogRelMsgReturn(!(pThis->cbEfiRom & PAGE_OFFSET_MASK), ("%RX64\n", pThis->cbEfiRom), VERR_INVALID_PARAMETER);

    uint8_t const * const pbFwVolEnd = pThis->pu8EfiRom + pFwVolHdr->FvLength;

    /*
     * Ffs files are stored one by one, so to find SECURITY_CORE we've to
     * search thru every one on the way.
     */
    uint32_t                    cbFfsFile = 0;  /* shut up gcc */
    EFI_FFS_FILE_HEADER const  *pFfsFile  = (EFI_FFS_FILE_HEADER const *)(pThis->pu8EfiRom + pFwVolHdr->HeaderLength);
    pFfsFile = efiFwVolFindFileByType(pFfsFile, pbFwVolEnd, EFI_FV_FILETYPE_SECURITY_CORE, &cbFfsFile);
    AssertLogRelMsgReturn(pFfsFile, ("No SECURITY_CORE found in the firmware volume\n"), VERR_FILE_NOT_FOUND);

    RTGCPHYS ImageBase;
    uint8_t *pbImage;
    pThis->GCEntryPoint0 = efiFindEntryPoint(pFfsFile, cbFfsFile, &ImageBase, &pbImage);

    /*
     * Calc the firmware load address from the image base and validate it.
     */
    pThis->GCLoadAddress = ImageBase - (pbImage - pThis->pu8EfiRom);
    AssertLogRelMsgReturn(~(pThis->GCLoadAddress & PAGE_OFFSET_MASK),
                          ("%RGp\n", pThis->GCLoadAddress),
                          VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pThis->GCLoadAddress > UINT32_C(0xf0000000),
                          ("%RGp\n", pThis->GCLoadAddress),
                          VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(   pThis->GCLoadAddress + (pThis->cbEfiRom - 1) > UINT32_C(0xf0000000)
                          && pThis->GCLoadAddress + (pThis->cbEfiRom - 1) < UINT32_C(0xffffe000),
                          ("%RGp + %RX64\n", pThis->GCLoadAddress, pThis->cbEfiRom),
                          VERR_OUT_OF_RANGE);

    LogRel(("EFI: Firmware volume loading at %RGp, SEC CORE at %RGp with EP at %RGp\n",
            pThis->GCLoadAddress, ImageBase, pThis->GCEntryPoint0));

    pFfsFile = efiFwVolFindFileByType(pFfsFile, pbFwVolEnd, EFI_FV_FILETYPE_PEI_CORE, &cbFfsFile);
    pThis->GCEntryPoint1 = efiFindEntryPoint(pFfsFile, cbFfsFile, NULL, NULL);
    LogRel(("EFI: Firmware volume loading at %RGp, PEI CORE at with EP at %RGp\n",
            pThis->GCLoadAddress, pThis->GCEntryPoint1));
    return VINF_SUCCESS;
}

/**
 * Load EFI ROM file into the memory.
 *
 * @returns VBox status.
 * @param   pThis       The device instance data.
 * @param   pCfgHandle  Configuration node handle for the device.
 */
static int efiLoadRom(PDEVEFI pThis, PCFGMNODE pCfgHandle)
{
    /*
     * Read the entire firmware volume into memory.
     */
    void   *pvFile;
    size_t  cbFile;
    int rc = RTFileReadAllEx(pThis->pszEfiRomFile,
                             0 /*off*/,
                             RTFOFF_MAX /*cbMax*/,
                             RTFILE_RDALL_O_DENY_WRITE,
                             &pvFile,
                             &cbFile);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pThis->pDevIns, rc, RT_SRC_POS,
                                   N_("Loading the EFI firmware volume '%s' failed with rc=%Rrc"),
                                   pThis->pszEfiRomFile, rc);
    pThis->pu8EfiRom = (uint8_t *)pvFile;
    pThis->cbEfiRom  = cbFile;

    /*
     * Validate firmware volume and figure out the load address as well as the SEC entry point.
     */
    rc = efiParseFirmware(pThis);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pThis->pDevIns, rc, RT_SRC_POS,
                                   N_("Parsing the EFI firmware volume '%s' failed with rc=%Rrc"),
                                   pThis->pszEfiRomFile, rc);

    /*
     * Map the firmware volume into memory as shadowed ROM.
     */
    /** @todo fix PGMR3PhysRomRegister so it doesn't mess up in SUPLib when mapping a big ROM image. */
    RTGCPHYS cbQuart = RT_ALIGN_64(pThis->cbEfiRom / 4, PAGE_SIZE);
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress,
                              cbQuart,
                              pThis->pu8EfiRom,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart,
                              cbQuart,
                              pThis->pu8EfiRom + cbQuart,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 2)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart * 2,
                              cbQuart,
                              pThis->pu8EfiRom + cbQuart * 2,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 3)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpROMRegister(pThis->pDevIns,
                              pThis->GCLoadAddress + cbQuart * 3,
                              pThis->cbEfiRom - cbQuart * 3,
                              pThis->pu8EfiRom + cbQuart * 3,
                              PGMPHYS_ROM_FLAGS_SHADOWED | PGMPHYS_ROM_FLAGS_PERMANENT_BINARY,
                              "EFI Firmware Volume (Part 4)");
    if (RT_FAILURE(rc))
        return rc;
    return VINF_SUCCESS;
}

/**
 * Patches and loads the EfiThunk ROM image.
 *
 * The thunk image is where the CPU starts and will switch it into
 * 32-bit protected or long mode and invoke the SEC CORE image in the
 * firmware volume. It also contains some static VM configuration data
 * at the very beginning of the page, see DEVEFIINFO.
 *
 * @returns VBox status code.
 * @param   pThis       The device instance data.
 * @param   pCfgHandle  Configuration node handle for the device.
 */
static int efiLoadThunk(PDEVEFI pThis, PCFGMNODE pCfgHandle)
{
    /*
     * Make a copy of the page and set the values of the DEVEFIINFO structure
     * found at the beginning of it.
     */

    /* Duplicate the page so we can change it. */
    AssertRelease(g_cbEfiThunkBinary == PAGE_SIZE);
    pThis->pu8EfiThunk = (uint8_t *)PDMDevHlpMMHeapAlloc(pThis->pDevIns, PAGE_SIZE);
    if (pThis->pu8EfiThunk == NULL)
        return VERR_NO_MEMORY;
    memcpy(pThis->pu8EfiThunk, &g_abEfiThunkBinary[0], PAGE_SIZE);

    /* Fill in the info. */
    PDEVEFIINFO pEfiInfo = (PDEVEFIINFO)pThis->pu8EfiThunk;
    pEfiInfo->pfnFirmwareEP = (uint32_t)pThis->GCEntryPoint0;
    //AssertRelease(pEfiInfo->pfnFirmwareEP == pThis->GCEntryPoint0);
    pEfiInfo->HighEPAddress = 0;
    pEfiInfo->PhysFwVol     = pThis->GCLoadAddress;
    pEfiInfo->cbFwVol       = (uint32_t)pThis->cbEfiRom;
    AssertRelease(pEfiInfo->cbFwVol == (uint32_t)pThis->cbEfiRom);
    pEfiInfo->cbBelow4GB    = pThis->cbBelow4GB;
    pEfiInfo->cbAbove4GB    = pThis->cbAbove4GB;
    pEfiInfo->fFlags        = 0;
    pEfiInfo->cCpus         = pThis->cCpus;
    pEfiInfo->pfnPeiEP      = (uint32_t)pThis->GCEntryPoint1;
    pEfiInfo->u32Reserved2  = 0;

    /* Register the page as a ROM (data will be copied). */
    int rc = PDMDevHlpROMRegister(pThis->pDevIns, UINT32_C(0xfffff000), PAGE_SIZE,
                                  pThis->pu8EfiThunk,
                                  PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "EFI Thunk");
    if (RT_FAILURE(rc))
        return rc;

#if 1    /** @todo this is probably not necessary. */
    /*
     * Map thunk page also at low address, so that real->protected mode jump code can
     * store GDT/IDT in code segment in low memory and load them during switch to the
     * protected mode, while being in 16-bits mode.
     *
     * @todo: maybe need to unregister later or place somewhere else (although could
     *        be needed during reset)
     */
    rc = PDMDevHlpROMRegister(pThis->pDevIns, 0xff000, PAGE_SIZE,
                              pThis->pu8EfiThunk,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "EFI Thunk (2)");
    if (RT_FAILURE(rc))
        return rc;
#endif

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
static DECLCALLBACK(int)  efiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PDEVEFI     pThis = PDMINS_2_DATA(pDevIns, PDEVEFI);
    int         rc;

    Assert(iInstance == 0);

    pThis->pDevIns = pDevIns;

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "EfiRom\0"
                              "RamSize\0"
                              "RamHoleSize\0"
                              "NumCPUs\0"
                              "UUID\0"
                              "IOAPIC\0"
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
                              "DmiSystemVersion\0"
                              "DmiChassisVendor\0"
                              "DmiChassisVersion\0"
                              "DmiChassisSerial\0"
                              "DmiChassisAssetTag\0"
#ifdef VBOX_WITH_DMI_OEMSTRINGS
                              "DmiOEMVBoxVer\0"
                              "DmiOEMVBoxRev\0"
#endif
                              ))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: Invalid config value(s) for the EFI device"));

    rc = CFGMR3QueryU8Def(pCfgHandle, "IOAPIC", &pThis->u8IOAPIC, 1);
    if (RT_FAILURE (rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to read \"IOAPIC\""));

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
    rc = sharedfwPlantDMITable(pDevIns, pThis->au8DMIPage, VBOX_DMI_TABLE_SIZE, &uuid, pCfgHandle);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->u8IOAPIC)
        sharedfwPlantMpsTable(pDevIns, pThis->au8DMIPage + VBOX_DMI_TABLE_SIZE, pThis->cCpus);

    rc = PDMDevHlpROMRegister(pDevIns, VBOX_DMI_TABLE_BASE, _4K, pThis->au8DMIPage,
                              PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "DMI tables");
    if (RT_FAILURE(rc))
        return rc;

    /* RAM sizes */
    rc = CFGMR3QueryU64(pCfgHandle, "RamSize", &pThis->cbRam);
    AssertLogRelRCReturn(rc, rc);
    rc = CFGMR3QueryU64(pCfgHandle, "RamHoleSize", &pThis->cbRamHole);
    AssertLogRelRCReturn(rc, rc);
    pThis->cbBelow4GB = RT_MIN(pThis->cbRam, _4G - pThis->cbRamHole);
    pThis->cbAbove4GB = pThis->cbRam - pThis->cbBelow4GB;

    /* CPU count (optional). */
    rc = CFGMR3QueryU32Def(pCfgHandle, "NumCPUs", &pThis->cCpus, 1);
    AssertLogRelRCReturn(rc, rc);
    /*
     * Get the system EFI ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "EfiRom", &pThis->pszEfiRomFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszEfiRomFile = (char*)PDMDevHlpMMHeapAlloc(pDevIns, RTPATH_MAX);
        if (!pThis->pszEfiRomFile)
            return VERR_NO_MEMORY;

        int rc = RTPathAppPrivateArch(pThis->pszEfiRomFile, RTPATH_MAX - 32);
        AssertRCReturn(rc, rc);

        size_t offFilename = strlen(pThis->pszEfiRomFile);
        strcpy(pThis->pszEfiRomFile+offFilename, "/vboxefi.fv");
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"EfiRom\" as a string failed"));
    else if (!*pThis->pszEfiRomFile)
    {
        MMR3HeapFree(pThis->pszEfiRomFile);
        pThis->pszEfiRomFile = NULL;
    }

#ifdef DEVEFI_WITH_VBOXDBG_SCRIPT
    /*
     * Zap the debugger script
     */
    RTFileDelete("./DevEFI.VBoxDbg");
#endif

    /*
     * Load firmware volume and thunk ROM.
     */
    rc = efiLoadRom(pThis, pCfgHandle);
    if (RT_FAILURE(rc))
        return rc;

    rc = efiLoadThunk(pThis, pCfgHandle);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register our communication ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, EFI_PORT_BASE, EFI_PORT_COUNT, NULL,
                                 efiIOPortWrite, efiIOPortRead,
                                 NULL, NULL, "EFI communication ports");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Call reset to set things up.
     */
    efiReset(pDevIns);

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceEFI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "efi",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Extensible Firmware Interface Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64,
    /* fClass */
    PDM_DEVREG_CLASS_ARCH_BIOS,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVEFI),
    /* pfnConstruct */
    efiConstruct,
    /* pfnDestruct */
    efiDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    efiReset,
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
    efiInitComplete,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
