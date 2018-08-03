/* $Id$ */
/** @file
 * DBGFR3ModInMemPe - In memory PE module 'loader'.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>

#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/mz.h>
#include <iprt/formats/elf.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The WinNT digger's loader reader instance data.
 */
typedef struct DBGFMODPERDR
{
    /** The VM handle (referenced). */
    PUVM                pUVM;
    /** The image base. */
    DBGFADDRESS         ImageAddr;
    /** The image size. */
    uint32_t            cbImage;
    /** The file offset of the SizeOfImage field in the optional header if it
     *  needs patching, otherwise set to UINT32_MAX. */
    uint32_t            offSizeOfImage;
    /** The correct image size. */
    uint32_t            cbCorrectImageSize;
    /** Number of entries in the aMappings table. */
    uint32_t            cMappings;
    /** Mapping hint. */
    uint32_t            iHint;
    /** Mapping file offset to memory offsets, ordered by file offset. */
    struct
    {
        /** The file offset. */
        uint32_t        offFile;
        /** The size of this mapping. */
        uint32_t        cbMem;
        /** The offset to the memory from the start of the image.  */
        uint32_t        offMem;
    } aMappings[1];
} DBGFMODPERDR;
/** Pointer a WinNT loader reader instance data. */
typedef DBGFMODPERDR *PDBGFMODPERDR;

/**
 * Stack buffer.
 */
typedef union DBGFMODINMEMBUF
{
    uint8_t             ab[0x2000];
    IMAGE_DOS_HEADER    DosHdr;
    IMAGE_NT_HEADERS32  Nt32;
    IMAGE_NT_HEADERS64  Nt64;
} DBGFMODINMEMBUF;
/** Pointer to stack buffer. */
typedef DBGFMODINMEMBUF *PDBGFMODINMEMBUF;



/**
 * Normalizes a debug module name.
 *
 * @returns Normalized debug module name.
 * @param   pszName         The name.
 * @param   pszBuf          Buffer to use if work is needed.
 * @param   cbBuf           Size of buffer.
 */
const char *dbgfR3ModNormalizeName(const char *pszName, char *pszBuf, size_t cbBuf)
{
    /*
     * Skip to the filename in case someone gave us a full filename path.
     */
    pszName = RTPathFilenameEx(pszName, RTPATH_STR_F_STYLE_DOS);

    /*
     * Is it okay?
     */
    size_t cchName = strlen(pszName);
    size_t off = 0;
    for (;; off++)
    {
        char ch = pszName[off];
        if (ch == '\0')
            return pszName;
        if (!RT_C_IS_ALNUM(ch) && ch != '_')
            break;
    }

    /*
     * It's no okay, so morph it.
     */
    if (cchName >= cbBuf)
        cchName = cbBuf - 1;
    for (off = 0; off < cchName; off++)
    {
        char ch = pszName[off];
        if (!RT_C_IS_ALNUM(ch))
            ch = '_';
        pszBuf[off] = ch;
    }
    pszBuf[off] = '\0';

    return pszBuf;
}


/**
 * Handles in-memory ELF images.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   pImageAddr      The image address.
 * @param   fFlags          Flags, DBGFMODINMEM_F_XXX.
 * @param   pszName         The module name, optional.
 * @param   pszFilename     The image filename, optional.
 * @param   enmArch         The image arch if we force it, pass
 *                          RTLDRARCH_WHATEVER if you don't care.
 * @param   cbImage         Image size.  Pass 0 if not known.
 * @param   puBuf           The header buffer.
 * @param   phDbgMod        Where to return the resulting debug module on success.
 * @param   pErrInfo        Where to return extended error info on failure.
 */
static int dbgfR3ModInMemElf(PUVM pUVM, PCDBGFADDRESS pImageAddr, uint32_t fFlags, const char *pszName, const char *pszFilename,
                             RTLDRARCH enmArch, uint32_t cbImage, PDBGFMODINMEMBUF puBuf,
                             PRTDBGMOD phDbgMod, PRTERRINFO pErrInfo)
{
    RT_NOREF(pUVM, fFlags, pszName, pszFilename, enmArch, cbImage, puBuf, phDbgMod);
    return RTERRINFO_LOG_SET_F(pErrInfo, VERR_INVALID_EXE_SIGNATURE, "Found ELF magic at %RGv", pImageAddr->FlatPtr);
}


/**
 * @callback_method_impl{PFNRTLDRRDRMEMREAD}
 */
static DECLCALLBACK(int) dbgfModInMemPeRdr_Read(void *pvBuf, size_t cb, size_t off, void *pvUser)
{
    PDBGFMODPERDR pThis   = (PDBGFMODPERDR)pvUser;
    uint32_t      offFile = (uint32_t)off;
    AssertReturn(offFile == off, VERR_INVALID_PARAMETER);

    uint32_t i = pThis->iHint;
    if (pThis->aMappings[i].offFile > offFile)
    {
        i = pThis->cMappings;
        while (i-- > 0)
            if (offFile >= pThis->aMappings[i].offFile)
                break;
        pThis->iHint = i;
    }

    while (cb > 0)
    {
        uint32_t offNextMap =  i + 1 < pThis->cMappings ? pThis->aMappings[i + 1].offFile : pThis->cbImage;
        uint32_t offMap     = offFile - pThis->aMappings[i].offFile;

        /* Read file bits backed by memory. */
        if (offMap < pThis->aMappings[i].cbMem)
        {
            uint32_t cbToRead = pThis->aMappings[i].cbMem - offMap;
            if (cbToRead > cb)
                cbToRead = (uint32_t)cb;

            DBGFADDRESS Addr = pThis->ImageAddr;
            DBGFR3AddrAdd(&Addr, pThis->aMappings[i].offMem + offMap);

            int rc = DBGFR3MemRead(pThis->pUVM, 0 /*idCpu*/, &Addr, pvBuf, cbToRead);
            if (RT_FAILURE(rc))
                return rc;

            /* Apply SizeOfImage patch? */
            if (   pThis->offSizeOfImage != UINT32_MAX
                && offFile            < pThis->offSizeOfImage + 4
                && offFile + cbToRead > pThis->offSizeOfImage)
            {
                uint32_t SizeOfImage = pThis->cbCorrectImageSize;
                uint32_t cbPatch     = sizeof(SizeOfImage);
                int32_t  offPatch    = pThis->offSizeOfImage - offFile;
                uint8_t *pbPatch     = (uint8_t *)pvBuf + offPatch;
                if (offFile + cbToRead < pThis->offSizeOfImage + cbPatch)
                    cbPatch = offFile + cbToRead - pThis->offSizeOfImage;
                while (cbPatch-- > 0)
                {
                    if (offPatch >= 0)
                        *pbPatch = (uint8_t)SizeOfImage;
                    offPatch++;
                    pbPatch++;
                    SizeOfImage >>= 8;
                }
            }

            /* Done? */
            if (cbToRead == cb)
                break;

            offFile += cbToRead;
            cb      -= cbToRead;
            pvBuf    = (char *)pvBuf + cbToRead;
        }

        /* Mind the gap. */
        if (offNextMap > offFile)
        {
            uint32_t cbZero = offNextMap - offFile;
            if (cbZero > cb)
            {
                RT_BZERO(pvBuf, cb);
                break;
            }

            RT_BZERO(pvBuf, cbZero);
            offFile += cbZero;
            cb      -= cbZero;
            pvBuf   = (char *)pvBuf + cbZero;
        }

        pThis->iHint = ++i;
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{PFNRTLDRRDRMEMDTOR}
 */
static DECLCALLBACK(void) dbgfModInMemPeRdr_Dtor(void *pvUser)
{
    PDBGFMODPERDR pThis = (PDBGFMODPERDR)pvUser;

    VMR3ReleaseUVM(pThis->pUVM);
    pThis->pUVM = NULL;
    RTMemFree(pvUser);
}


/**
 * Checks if the section headers look okay.
 *
 * @returns VBox status code.
 * @param   paShdrs             Pointer to the section headers.
 * @param   cShdrs              Number of headers.
 * @param   cbImage             The image size reported by NT.
 * @param   cbImageFromHdr      The image size by the linker in the header.
 * @param   uRvaRsrc            The RVA of the resource directory. UINT32_MAX if
 *                              no resource directory.
 * @param   cbSectAlign         The section alignment specified in the header.
 * @param   fNt31               Set if NT 3.1.  Needed for chopped off HAL.
 * @param   pcbImageCorrect     The corrected image size.  This is derived from
 *                              cbImage and virtual range of the section tables.
 *
 *                              The problem is that NT may choose to drop the
 *                              last pages in images it loads early, starting at
 *                              the resource directory.  These images will have
 *                              a page aligned cbImage.
 *
 * @param   pErrInfo            Where to return more error details.
 */
static int dbgfR3ModPeCheckSectHdrsAndImgSize(PCIMAGE_SECTION_HEADER paShdrs, uint32_t cShdrs, uint32_t cbImage,
                                              uint32_t cbImageFromHdr, uint32_t uRvaRsrc, uint32_t cbSectAlign,
                                              bool fNt31, uint32_t *pcbImageCorrect, PRTERRINFO pErrInfo)
{
    *pcbImageCorrect = cbImage;

    for (uint32_t i = 0; i < cShdrs; i++)
    {
        if (!paShdrs[i].Name[0])
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "Section header #%u has no name", i);

        if (paShdrs[i].Characteristics & IMAGE_SCN_TYPE_NOLOAD)
            continue;

        /* Tweak to determine the virtual size if the linker didn't set it (NT 3.1). */
        /** @todo this isn't really perfect. cbImage is kind of wrong...   */
        uint32_t cbVirtual = paShdrs[i].Misc.VirtualSize;
        if (cbVirtual == 0)
        {
            for (uint32_t j = i + 1; j < cShdrs; j++)
                if (   !(paShdrs[j].Characteristics & IMAGE_SCN_TYPE_NOLOAD)
                    && paShdrs[j].VirtualAddress > paShdrs[i].VirtualAddress)
                {
                    cbVirtual = paShdrs[j].VirtualAddress - paShdrs[i].VirtualAddress;
                    break;
                }
            if (!cbVirtual)
            {
                if (paShdrs[i].VirtualAddress < cbImageFromHdr)
                    cbVirtual = cbImageFromHdr - paShdrs[i].VirtualAddress;
                else if (paShdrs[i].SizeOfRawData > 0)
                    cbVirtual = RT_ALIGN(paShdrs[i].SizeOfRawData, _4K);
            }
        }

        /* Check that sizes are within the same range and that both sizes and
           addresses are within reasonable limits. */
        if (   RT_ALIGN(cbVirtual, _64K) < RT_ALIGN(paShdrs[i].SizeOfRawData, _64K)
            || cbVirtual                >= _1G
            || paShdrs[i].SizeOfRawData >= _1G)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT,
                                       "Section header #%u (%.8s) has a VirtualSize=%#x (%#x) and SizeOfRawData=%#x, that's too much data!",
                                       i, paShdrs[i].Name, cbVirtual, paShdrs[i].Misc.VirtualSize, paShdrs[i].SizeOfRawData);
        uint32_t uRvaEnd = paShdrs[i].VirtualAddress + cbVirtual;
        if (uRvaEnd >= _1G || uRvaEnd < paShdrs[i].VirtualAddress)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT,
                                       "Section header #%u (%.8s) has a VirtualSize=%#x (%#x) and VirtualAddr=%#x, %#x in total, that's too much!",
                                       i, paShdrs[i].Name, cbVirtual, paShdrs[i].Misc.VirtualSize, paShdrs[i].VirtualAddress, uRvaEnd);

        /* Check for images chopped off around '.rsrc'. */
        if (    cbImage < uRvaEnd
            &&  uRvaEnd >= uRvaRsrc)
            cbImage = RT_ALIGN(uRvaEnd, cbSectAlign);

        /* Check that the section is within the image. */
        if (uRvaEnd > cbImage && fNt31)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT,
                                       "Section header #%u has a virtual address range beyond the image: %#x TO %#x cbImage=%#x",
                                       i, paShdrs[i].VirtualAddress, uRvaEnd, cbImage);
    }

    Assert(*pcbImageCorrect == cbImage || !(*pcbImageCorrect & 0xfff));
    *pcbImageCorrect = cbImage;
    return VINF_SUCCESS;
}


/**
 * Create a loader module for the in-guest-memory PE module.
 */
static int dbgfR3ModInMemPeCreateLdrMod(PUVM pUVM, uint32_t fFlags, const char *pszName, PCDBGFADDRESS pImageAddr,
                                        uint32_t cbImage, uint32_t cbImageFromHdr, bool f32Bit,
                                        uint32_t cShdrs, PCIMAGE_SECTION_HEADER paShdrs, uint32_t cbSectAlign,
                                        uint32_t cDataDir, PCIMAGE_DATA_DIRECTORY paDataDir, uint32_t offHdrs,
                                        PRTLDRMOD phLdrMod, PRTERRINFO pErrInfo)
{
    /*
     * Allocate and create a reader instance.
     */
    PDBGFMODPERDR pRdr = (PDBGFMODPERDR)RTMemAlloc(RT_UOFFSETOF_DYN(DBGFMODPERDR, aMappings[cShdrs + 2]));
    if (!pRdr)
        return VERR_NO_MEMORY;

    VMR3RetainUVM(pUVM);
    pRdr->pUVM               = pUVM;
    pRdr->ImageAddr          = *pImageAddr;
    pRdr->cbImage            = cbImage;
    pRdr->cbCorrectImageSize = cbImage;
    pRdr->offSizeOfImage     = UINT32_MAX;
    pRdr->iHint              = 0;

    /*
     * Use the section table to construct a more accurate view of the file/image.
     */
    uint32_t uRvaRsrc = UINT32_MAX;
    if (   cDataDir > IMAGE_DIRECTORY_ENTRY_RESOURCE
        && paDataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size > 0)
        uRvaRsrc = paDataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;

    int rc = dbgfR3ModPeCheckSectHdrsAndImgSize(paShdrs, cShdrs, cbImage, cbImageFromHdr, uRvaRsrc, cbSectAlign,
                                                RT_BOOL(fFlags & DBGFMODINMEM_F_PE_NT31), &pRdr->cbCorrectImageSize, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        pRdr->cMappings = 0;

        for (uint32_t i = 0; i < cShdrs; i++)
            if (   paShdrs[i].SizeOfRawData    > 0
                && paShdrs[i].PointerToRawData > 0)
            {
                uint32_t j = 1;
                if (!pRdr->cMappings)
                    pRdr->cMappings++;
                else
                {
                    while (j < pRdr->cMappings && pRdr->aMappings[j].offFile < paShdrs[i].PointerToRawData)
                        j++;
                    if (j < pRdr->cMappings)
                        memmove(&pRdr->aMappings[j + 1], &pRdr->aMappings[j], (pRdr->cMappings - j) * sizeof(pRdr->aMappings));
                }
                pRdr->aMappings[j].offFile = paShdrs[i].PointerToRawData;
                pRdr->aMappings[j].offMem  = paShdrs[i].VirtualAddress;
                pRdr->aMappings[j].cbMem   = i + 1 < cShdrs
                                           ? paShdrs[i + 1].VirtualAddress - paShdrs[i].VirtualAddress
                                           : paShdrs[i].Misc.VirtualSize;
                if (j == pRdr->cMappings)
                    pRdr->cbImage = paShdrs[i].PointerToRawData + paShdrs[i].SizeOfRawData;
                pRdr->cMappings++;
            }

        /* Insert the mapping of the headers that isn't covered by the section table. */
        pRdr->aMappings[0].offFile = 0;
        pRdr->aMappings[0].offMem  = 0;
        pRdr->aMappings[0].cbMem   = pRdr->cMappings ? pRdr->aMappings[1].offFile : pRdr->cbImage;

        int j = pRdr->cMappings - 1;
        while (j-- > 0)
        {
            uint32_t cbFile = pRdr->aMappings[j + 1].offFile - pRdr->aMappings[j].offFile;
            if (pRdr->aMappings[j].cbMem > cbFile)
                pRdr->aMappings[j].cbMem = cbFile;
        }
    }
    else if (fFlags & DBGFMODINMEM_F_NO_READER_FALLBACK)
        return rc;
    else
    {
        /*
         * Fallback, fake identity mapped file data.
         */
        pRdr->cMappings = 1;
        pRdr->aMappings[0].offFile = 0;
        pRdr->aMappings[0].offMem  = 0;
        pRdr->aMappings[0].cbMem   = pRdr->cbImage;
    }

    /* Enable the SizeOfImage patching if necessary. */
    if (pRdr->cbCorrectImageSize != cbImage)
    {
        Log(("dbgfR3ModInMemPeCreateLdrMod: The image is really %#x bytes long, not %#x as mapped by NT!\n",
             pRdr->cbCorrectImageSize, cbImage));
        pRdr->offSizeOfImage = f32Bit
                             ? offHdrs + RT_OFFSETOF(IMAGE_NT_HEADERS32, OptionalHeader.SizeOfImage)
                             : offHdrs + RT_OFFSETOF(IMAGE_NT_HEADERS64, OptionalHeader.SizeOfImage);
    }

    /*
     * Call the loader to open the PE image for debugging.
     * Note! It always calls pfnDtor.
     */
    RTLDRMOD hLdrMod;
    rc = RTLdrOpenInMemory(pszName, RTLDR_O_FOR_DEBUG, RTLDRARCH_WHATEVER, pRdr->cbImage,
                           dbgfModInMemPeRdr_Read, dbgfModInMemPeRdr_Dtor, pRdr,
                           &hLdrMod, pErrInfo);
    if (RT_SUCCESS(rc))
        *phLdrMod = hLdrMod;
    else
        *phLdrMod = NIL_RTLDRMOD;
    return rc;
}


/**
 * Handles in-memory PE images.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   pImageAddr      The image address.
 * @param   fFlags          Flags, DBGFMODINMEM_F_XXX.
 * @param   pszName         The module name, optional.
 * @param   pszFilename     The image filename, optional.
 * @param   enmArch         The image arch if we force it, pass
 *                          RTLDRARCH_WHATEVER if you don't care.
 * @param   cbImage         Image size.  Pass 0 if not known.
 * @param   offPeHdrs       Offset of the PE header.
 * @param   cbPeHdrsPart1   How read into uBuf at @a offPeHdrs.
 * @param   puBuf           The header buffer.
 * @param   phDbgMod        Where to return the resulting debug module on success.
 * @param   pErrInfo        Where to return extended error info on failure.
 */
static int dbgfR3ModInMemPe(PUVM pUVM, PCDBGFADDRESS pImageAddr, uint32_t fFlags, const char *pszName, const char *pszFilename,
                            RTLDRARCH enmArch, uint32_t cbImage, uint32_t offPeHdrs, uint32_t cbPeHdrsPart1,
                            PDBGFMODINMEMBUF puBuf, PRTDBGMOD phDbgMod, PRTERRINFO pErrInfo)
{
    /*
     * Read the optional header and the section table after validating the
     * info we need from the file header.
     */
    /* Check the opt hdr size and number of sections as these are used to determine how much to read next. */
    if (   puBuf->Nt32.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)
        || puBuf->Nt32.FileHeader.SizeOfOptionalHeader > sizeof(IMAGE_OPTIONAL_HEADER64) + 128)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "Invalid SizeOfOptionalHeader value: %#RX32",
                                   puBuf->Nt32.FileHeader.SizeOfOptionalHeader);

    if (   puBuf->Nt32.FileHeader.NumberOfSections < 1
        || puBuf->Nt32.FileHeader.NumberOfSections > 190 /* what fits in our 8K buffer */)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "NumberOfSections is out of range: %#RX32 (1..190)",
                                   puBuf->Nt32.FileHeader.NumberOfSections);

    /* Read the optional header and section table. */
    uint32_t const cbHdrs = RT_UOFFSETOF(IMAGE_NT_HEADERS32, OptionalHeader)
                          + puBuf->Nt32.FileHeader.SizeOfOptionalHeader
                          + puBuf->Nt32.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    AssertReturn(cbHdrs <= sizeof(*puBuf), RTERRINFO_LOG_SET_F(pErrInfo, VERR_INTERNAL_ERROR_2, "cbHdrs=%#x", cbHdrs));

    DBGFADDRESS PeHdrPart2Addr = *pImageAddr;
    DBGFR3AddrAdd(&PeHdrPart2Addr, offPeHdrs + cbPeHdrsPart1);
    int rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &PeHdrPart2Addr, &puBuf->ab[cbPeHdrsPart1], cbHdrs - cbPeHdrsPart1);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET_F(pErrInfo, rc,
                                   "Failed to read the second part of the PE headers at %RGv (off=%#RX32 + %#RX32): %Rrc",
                                   PeHdrPart2Addr.FlatPtr, offPeHdrs, cbPeHdrsPart1, rc);

    /*
     * Check the image architecture and determine the bitness.
     */
    RTLDRARCH enmArchActual;
    bool f32Bit;
    switch (puBuf->Nt32.FileHeader.Machine)
    {
        case IMAGE_FILE_MACHINE_I386:
            enmArchActual = RTLDRARCH_X86_32;
            f32Bit = true;
            break;
        case IMAGE_FILE_MACHINE_AMD64:
            enmArchActual = RTLDRARCH_AMD64;
            f32Bit = false;
            break;
        case IMAGE_FILE_MACHINE_ARM:
        case IMAGE_FILE_MACHINE_THUMB:
        case IMAGE_FILE_MACHINE_ARMNT:
            enmArchActual = RTLDRARCH_ARM32;
            f32Bit = true;
            break;
        case IMAGE_FILE_MACHINE_ARM64:
            enmArchActual = RTLDRARCH_ARM64;
            f32Bit = false;
            break;
        default:
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_LDR_ARCH_MISMATCH, "Unknown machine: %#x", puBuf->Nt32.FileHeader.Machine);
    }
    if (   enmArch != RTLDRARCH_WHATEVER
        && enmArch != enmArchActual)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_LDR_ARCH_MISMATCH, "Found %s expected %s",
                                   RTLdrArchName(enmArchActual), RTLdrArchName(enmArch));

    /*
     * Check optional header magic and size.
     */
    uint16_t const uOptMagic = f32Bit ? IMAGE_NT_OPTIONAL_HDR32_MAGIC : IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    if (puBuf->Nt32.OptionalHeader.Magic != uOptMagic)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "Unexpected optional header magic: %#x (expected %#x)",
                                   puBuf->Nt32.OptionalHeader.Magic, uOptMagic);

    uint32_t const cDataDir = f32Bit ? puBuf->Nt32.OptionalHeader.NumberOfRvaAndSizes : puBuf->Nt64.OptionalHeader.NumberOfRvaAndSizes;
    if (   cDataDir <= IMAGE_DIRECTORY_ENTRY_BASERELOC /* a bit random */
        || cDataDir > 32 /* also random */)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "Unexpected data directory size: %#x", cDataDir);

    uint32_t cbOptHdr = f32Bit ? sizeof(IMAGE_OPTIONAL_HEADER32) : sizeof(IMAGE_OPTIONAL_HEADER64);
    cbOptHdr -= sizeof(IMAGE_DATA_DIRECTORY) * IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    cbOptHdr += sizeof(IMAGE_DATA_DIRECTORY) * cDataDir;
    if (puBuf->Nt32.FileHeader.SizeOfOptionalHeader != cbOptHdr)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_BAD_EXE_FORMAT, "Unexpected optional header size: %#x (expected %#x)",
                                   puBuf->Nt32.FileHeader.SizeOfOptionalHeader, cbOptHdr);

    uint32_t const cbSectAlign = f32Bit ? puBuf->Nt32.OptionalHeader.SectionAlignment : puBuf->Nt64.OptionalHeader.SectionAlignment;
    PCIMAGE_SECTION_HEADER pSHdrs    = (PCIMAGE_SECTION_HEADER)((uintptr_t)&puBuf->Nt32.OptionalHeader + cbOptHdr);
    PCIMAGE_DATA_DIRECTORY paDataDir = (PCIMAGE_DATA_DIRECTORY)((uintptr_t)pSHdrs - cDataDir * sizeof(IMAGE_DATA_DIRECTORY));

    /*
     * Establish the image size.
     */
    uint32_t cbImageFromHdr = f32Bit ? puBuf->Nt32.OptionalHeader.SizeOfImage : puBuf->Nt64.OptionalHeader.SizeOfImage;
    if (   !cbImage
        || (fFlags & DBGFMODINMEM_F_PE_NT31))
        cbImage = RT_ALIGN(cbImageFromHdr, _4K);
    else if (RT_ALIGN(cbImageFromHdr, _4K) != RT_ALIGN(cbImage, _4K))
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_MISMATCH, "Image size mismatch: input=%#x header=%#x", cbImage, cbImageFromHdr);

    /*
     * Guess the module name if not specified and make sure it conforms to DBGC expectations.
     */
    if (!pszName)
    {
        if (pszFilename)
            pszName = RTPathFilenameEx(pszFilename, RTPATH_STR_F_STYLE_DOS);
        /** @todo */
    }

    char szNormalized[128];
    pszName = dbgfR3ModNormalizeName(pszName, szNormalized, sizeof(szNormalized));

    /*
     * Create the module using the in memory image first, falling back on cached image.
     */
    RTLDRMOD hLdrMod;
    rc = dbgfR3ModInMemPeCreateLdrMod(pUVM, fFlags, pszName, pImageAddr, cbImage, cbImageFromHdr, f32Bit,
                                      puBuf->Nt32.FileHeader.NumberOfSections, pSHdrs, cbSectAlign, cDataDir, paDataDir,
                                      offPeHdrs, &hLdrMod, pErrInfo);
    if (RT_FAILURE(rc))
        hLdrMod = NIL_RTLDRMOD;

    RTDBGMOD hMod;
    rc = RTDbgModCreateFromPeImage(&hMod, pszFilename, pszName, &hLdrMod, cbImageFromHdr,
                                   puBuf->Nt32.FileHeader.TimeDateStamp, DBGFR3AsGetConfig(pUVM));
    if (RT_SUCCESS(rc))
        *phDbgMod = hMod;
    else if (!(fFlags & DBGFMODINMEM_F_NO_CONTAINER_FALLBACK))
    {
        /*
         * Fallback is a container module.
         */
        rc = RTDbgModCreate(&hMod, pszName, cbImage, 0);
        if (RT_SUCCESS(rc))
        {
            rc = RTDbgModSymbolAdd(hMod, "Headers", 0 /*iSeg*/, 0, cbImage, 0 /*fFlags*/, NULL);
            AssertRC(rc);
        }
    }
    return rc;
}



/**
 * Process a PE image found in guest memory.
 *
 * @param   pUVM            The user mode VM handle.
 * @param   pImageAddr      The image address.
 * @param   fFlags          Flags, DBGFMODINMEM_F_XXX.
 * @param   pszName         The module name, optional.
 * @param   pszFilename     The image filename, optional.
 * @param   enmArch         The image arch if we force it, pass
 *                          RTLDRARCH_WHATEVER if you don't care.
 * @param   cbImage         Image size.  Pass 0 if not known.
 * @param   phDbgMod        Where to return the resulting debug module on success.
 * @param   pErrInfo        Where to return extended error info on failure.
 */
VMMR3DECL(int) DBGFR3ModInMem(PUVM pUVM, PCDBGFADDRESS pImageAddr, uint32_t fFlags, const char *pszName, const char *pszFilename,
                              RTLDRARCH enmArch, uint32_t cbImage, PRTDBGMOD phDbgMod, PRTERRINFO pErrInfo)
{
    /*
     * Validate and adjust.
     */
    AssertPtrReturn(phDbgMod, VERR_INVALID_POINTER);
    *phDbgMod = NIL_RTDBGMOD;
    AssertPtrReturn(pImageAddr, VERR_INVALID_POINTER);
    AssertMsgReturn(cbImage == 0 || cbImage >= sizeof(IMAGE_NT_HEADERS32) + sizeof(IMAGE_DOS_HEADER),
                    ("cbImage=%#x\n", cbImage), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fFlags & ~DBGFMODINMEM_F_VALID_MASK), ("%#x\n", fFlags), VERR_INVALID_FLAGS);
    if (enmArch == RTLDRARCH_HOST)
        enmArch = RTLdrGetHostArch();

    /*
     * Look for an image header we can work with.
     */
    DBGFMODINMEMBUF uBuf;
    RT_ZERO(uBuf);

    int rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, pImageAddr, &uBuf, sizeof(uBuf.DosHdr));
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET_F(pErrInfo, rc, "Failed to read DOS header at %RGv: %Rrc", pImageAddr->FlatPtr, rc);

    if (uBuf.ab[0] == ELFMAG0 && uBuf.ab[1] == ELFMAG1 && uBuf.ab[2] == ELFMAG2 && uBuf.ab[3] == ELFMAG3)
        return dbgfR3ModInMemElf(pUVM, pImageAddr, fFlags, pszName, pszFilename, enmArch, cbImage, &uBuf, phDbgMod, pErrInfo);

    uint32_t offNewHdrs;
    if (uBuf.DosHdr.e_magic == IMAGE_DOS_SIGNATURE)
    {
        offNewHdrs = uBuf.DosHdr.e_lfanew;
        if (   offNewHdrs < 16
            || offNewHdrs > (cbImage ? _2M : cbImage - sizeof(IMAGE_NT_HEADERS32)))
            return RTERRINFO_LOG_SET_F(pErrInfo, rc, "e_lfanew value is out of range: %RX32 (16..%u)",
                                       offNewHdrs, (cbImage ? _2M : cbImage - sizeof(IMAGE_NT_HEADERS32)));
    }
    else if (uBuf.Nt32.Signature == IMAGE_NT_SIGNATURE)
        offNewHdrs = 0;
    else
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_INVALID_EXE_SIGNATURE, "Unknown image magic at %RGv: %.8Rhxs",
                                   pImageAddr->FlatPtr, uBuf.ab);

    /*
     * Read the next bit of header, assuming PE so stop at the end of
     * the COFF file header.
     */
    DBGFADDRESS PeHdrAddr = *pImageAddr;
    DBGFR3AddrAdd(&PeHdrAddr, offNewHdrs);
    uint32_t const cbPeHdrsPart1 = RT_UOFFSETOF(IMAGE_NT_HEADERS32, OptionalHeader);
    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &PeHdrAddr, &uBuf, cbPeHdrsPart1);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET_F(pErrInfo, rc, "Failed to read PE/LX/NE headers at %RGv (off=%#RX32): %Rrc",
                                   PeHdrAddr.FlatPtr, offNewHdrs, rc);

    if (uBuf.Nt32.Signature == IMAGE_NT_SIGNATURE)
        return dbgfR3ModInMemPe(pUVM, pImageAddr, fFlags, pszName, pszFilename, enmArch, cbImage, offNewHdrs, cbPeHdrsPart1,
                                &uBuf, phDbgMod, pErrInfo);

    return RTERRINFO_LOG_SET_F(pErrInfo, VERR_INVALID_EXE_SIGNATURE, "No PE/LX/NE header at %RGv (off=%#RX32): %.8Rhxs",
                               PeHdrAddr.FlatPtr, offNewHdrs, uBuf.ab);
}

