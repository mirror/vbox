/* $Id$ */
/** @file
 * innotek Portable Runtime - Binary Image Loader, Portable Executable (PE).
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include "internal/ldrPE.h"
#include "internal/ldr.h"



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Converts rva to a type.
 * @param   pvBits  Pointer to base of image bits.
 * @param   rva     Relative virtual address.
 * @param   type    Type.
 */
#define PE_RVA2TYPE(pvBits, rva, type)  ((type) ((uintptr_t)pvBits + (uintptr_t)(rva)) )


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The PE loader structure.
 */
typedef struct RTLDRMODPE
{
    /** Core module structure. */
    RTLDRMODINTERNAL        Core;
    /** Pointer to the reader instance. */
    PRTLDRREADER            pReader;
    /** Pointer to internal copy of image bits.
     * @todo the reader should take care of this. */
    void                   *pvBits;
    /** The offset of the NT headers. */
    RTFOFF                  offNtHdrs;

    /** The machine type (IMAGE_FILE_HEADER::Machine). */
    uint16_t                u16Machine;
    /** The file flags (IMAGE_FILE_HEADER::Characteristics). */
    uint16_t                fFile;
    /** Number of sections (IMAGE_FILE_HEADER::NumberOfSections). */
    unsigned                cSections;
    /** Pointer to an array of the section headers related to the file. */
    PIMAGE_SECTION_HEADER   paSections;

    /** The RVA of the entry point (IMAGE_OPTIONAL_HEADER32::AddressOfEntryPoint). */
    RTUINTPTR               uEntryPointRVA;
    /** The base address of the image at link time (IMAGE_OPTIONAL_HEADER32::ImageBase). */
    RTUINTPTR               uImageBase;
    /** The size of the loaded image (IMAGE_OPTIONAL_HEADER32::SizeOfImage). */
    uint32_t                cbImage;
    /** Size of the header (IMAGE_OPTIONAL_HEADER32::SizeOfHeaders). */
    uint32_t                cbHeaders;
    /** The import data directory entry. */
    IMAGE_DATA_DIRECTORY    ImportDir;
    /** The base relocation data directory entry. */
    IMAGE_DATA_DIRECTORY    RelocDir;
    /** The export data directory entry. */
    IMAGE_DATA_DIRECTORY    ExportDir;
} RTLDRMODPE, *PRTLDRMODPE;

/**
 * PE Loader module operations.
 *
 * The PE loader has one operation which is a bit different between 32-bit and 64-bit PE images,
 * and for historical and performance reasons have been split into separate functions. Thus the
 * PE loader extends the RTLDROPS structure with this one entry.
 */
typedef struct RTLDROPSPE
{
    /** The usual ops. */
    RTLDROPS Core;

    /**
     * Resolves all imports.
     *
     * @returns iprt status code.
     * @param   pModPe          Pointer to the PE loader module structure.
     * @param   pvBitsR         Where to read raw image bits. (optional)
     * @param   pvBitsW         Where to store the imports. The size of this buffer is equal or
     *                          larger to the value returned by pfnGetImageSize().
     * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
     * @param   pvUser          User argument to pass to the callback.
     */
    DECLCALLBACKMEMBER(int, pfnResolveImports)(PRTLDRMODPE pModPe, const void *pvBitsR, void *pvBitsW, PFNRTLDRIMPORT pfnGetImport, void *pvUser);

    /** Dummy entry to make sure we've initialized it all. */
    RTUINT  uDummy;
} RTLDROPSPE, *PRTLDROPSPE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtldrPEConvert32BitOptionalHeaderTo64Bit(PIMAGE_OPTIONAL_HEADER64 pOptHdr);
static void rtldrPEConvert32BitLoadConfigTo64Bit(PIMAGE_LOAD_CONFIG_DIRECTORY64 pLoadCfg);
static int rtldrPEApplyFixups(PRTLDRMODPE pModPe, const void *pvBitsR, void *pvBitsW, RTUINTPTR BaseAddress, RTUINTPTR OldBaseAddress);


/** @copydoc RTLDROPS::pfnGetImageSize */
static DECLCALLBACK(size_t) rtldrPEGetImageSize(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;
    return pModPe->cbImage;
}


/**
 * Reads the image into memory.
 *
 * @returns iprt status code.
 * @param   pModPe      The PE module.
 * @param   pvBits      Where to store the bits, this buffer is at least pItem->Core.cbImage in size.
 */
static int rtldrPEGetBitsNoImportsNorFixups(PRTLDRMODPE pModPe, void *pvBits)
{
    /*
     * Both these checks are related to pfnDone().
     */
    PRTLDRREADER pReader = pModPe->pReader;
    if (!pReader)
    {
        AssertMsgFailed(("You've called done!\n"));
        return VERR_WRONG_ORDER;
    }
    if (!pvBits)
        return VERR_NO_MEMORY;

    /*
     * Zero everything (could be done per section).
     */
    memset(pvBits, 0, pModPe->cbImage);

#ifdef PE_FILE_OFFSET_EQUALS_RVA
    /*
     * Read the entire image / file.
     */
    const RTFOFF cbRawImage = pReader->pfnSize(pReader)
    rc = pReader->pfnRead(pReader, pvBits, RT_MIN(pModPe->cbImage, cbRawImage), 0);
    if (RT_FAILURE(rc))
        Log(("rtldrPE: %s: Reading %#x bytes at offset %#x failed, %Rrc!!! (the entire image)\n",
             pReader->pfnLogName(pReader), RT_MIN(pModPe->cbImage, cbRawImage), 0, rc));
#else

    /*
     * Read the headers.
     */
    int rc = pReader->pfnRead(pReader, pvBits, pModPe->cbHeaders, 0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Read the sections.
         */
        PIMAGE_SECTION_HEADER pSH = pModPe->paSections;
        for (unsigned cLeft = pModPe->cSections; cLeft > 0; cLeft--, pSH++)
            if (pSH->SizeOfRawData && pSH->Misc.VirtualSize)
            {
                rc = pReader->pfnRead(pReader, (uint8_t *)pvBits + pSH->VirtualAddress, pSH->SizeOfRawData, pSH->PointerToRawData);
                if (RT_FAILURE(rc))
                {
                    Log(("rtldrPE: %s: Reading %#x bytes at offset %#x failed, %Rrc - section #%d '%.*s'!!!\n",
                         pReader->pfnLogName(pReader), pSH->SizeOfRawData, pSH->PointerToRawData, rc,
                         pSH - pModPe->paSections, sizeof(pSH->Name), pSH->Name));
                    break;
                }
            }
    }
    else
        Log(("rtldrPE: %s: Reading %#x bytes at offset %#x failed, %Rrc!!!\n",
             pReader->pfnLogName(pReader), pModPe->cbHeaders, 0, rc));
#endif
    return rc;
}


/**
 * Reads the bits into the internal buffer pointed to by PRTLDRMODPE::pvBits.
 *
 * @returns iprt status code.
 * @param   pModPe      The PE module.
 */
static int rtldrPEReadBits(PRTLDRMODPE pModPe)
{
    Assert(!pModPe->pvBits);
    void *pvBitsW = RTMemAllocZ(pModPe->cbImage);
    if (!pvBitsW)
        return VERR_NO_MEMORY;
    int rc = rtldrPEGetBitsNoImportsNorFixups(pModPe, pvBitsW);
    if (RT_SUCCESS(rc))
        pModPe->pvBits = pvBitsW;
    else
        RTMemFree(pvBitsW);
    return rc;
}


/** @copydoc RTLDROPS::pfnGetBits */
static DECLCALLBACK(int) rtldrPEGetBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;

    /*
     * Read the image.
     */
    int rc = rtldrPEGetBitsNoImportsNorFixups(pModPe, pvBits);
    if (RT_SUCCESS(rc))
    {
        /*
         * Resolve imports.
         */
        rc = ((PRTLDROPSPE)pMod->pOps)->pfnResolveImports(pModPe, pvBits, pvBits, pfnGetImport, pvUser);
        if (RT_SUCCESS(rc))
        {
            /*
             * Apply relocations.
             */
            rc = rtldrPEApplyFixups(pModPe, pvBits, pvBits, BaseAddress, pModPe->uImageBase);
            if (RT_SUCCESS(rc))
                return rc;
            AssertMsgFailed(("Failed to apply fixups. rc=%Rrc\n", rc));
        }
        else
            AssertMsgFailed(("Failed to resolve imports. rc=%Rrc\n", rc));
    }
    return rc;
}


/** @copydoc RTLDROPSPE::pfnResolveImports */
static DECLCALLBACK(int) rtldrPEResolveImports32(PRTLDRMODPE pModPe, const void *pvBitsR, void *pvBitsW, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    /*
     * Check if there is actually anything to work on.
     */
    if (    !pModPe->ImportDir.VirtualAddress
        ||  !pModPe->ImportDir.Size)
        return 0;

    /*
     * Walk the IMAGE_IMPORT_DESCRIPTOR table.
     */
    int                         rc = VINF_SUCCESS;
    PIMAGE_IMPORT_DESCRIPTOR    pImps;
    for (pImps = PE_RVA2TYPE(pvBitsR, pModPe->ImportDir.VirtualAddress, PIMAGE_IMPORT_DESCRIPTOR);
         !rc && pImps->Name != 0 && pImps->FirstThunk != 0;
         pImps++)
    {
        const char *pszModName = PE_RVA2TYPE(pvBitsR, pImps->Name, const char *);
        PIMAGE_THUNK_DATA32 pFirstThunk;    /* update this. */
        PIMAGE_THUNK_DATA32 pThunk;         /* read from this. */
        Log3(("RTLdrPE: Import descriptor: %s\n", pszModName));
        Log4(("RTLdrPE:   OriginalFirstThunk = %#RX32\n"
              "RTLdrPE:   TimeDateStamp      = %#RX32\n"
              "RTLdrPE:   ForwarderChain     = %#RX32\n"
              "RTLdrPE:   Name               = %#RX32\n"
              "RTLdrPE:   FirstThunk         = %#RX32\n",
              pImps->u.OriginalFirstThunk, pImps->TimeDateStamp,
              pImps->ForwarderChain, pImps->Name, pImps->FirstThunk));

        /*
         * Walk the thunks table(s).
         */
        pFirstThunk = PE_RVA2TYPE(pvBitsW, pImps->FirstThunk, PIMAGE_THUNK_DATA32);
        pThunk = pImps->u.OriginalFirstThunk == 0
            ? PE_RVA2TYPE(pvBitsR, pImps->FirstThunk, PIMAGE_THUNK_DATA32)
            : PE_RVA2TYPE(pvBitsR, pImps->u.OriginalFirstThunk, PIMAGE_THUNK_DATA32);
        while (!rc && pThunk->u1.Ordinal != 0)
        {
            RTUINTPTR Value = 0;
            if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
            {
                rc = pfnGetImport(&pModPe->Core, pszModName, NULL, IMAGE_ORDINAL32(pThunk->u1.Ordinal), &Value, pvUser);
                Log4((RT_SUCCESS(rc) ? "RTLdrPE:  %RTptr #%u\n" : "RTLdrPE:  %08RX32 #%u rc=%Vrc\n",
                      (uint32_t)Value, IMAGE_ORDINAL32(pThunk->u1.Ordinal), rc));
            }
            else if (   pThunk->u1.Ordinal > 0
                     && pThunk->u1.Ordinal < pModPe->cbImage)
            {
                rc = pfnGetImport(&pModPe->Core, pszModName, PE_RVA2TYPE(pvBitsR, (char*)pThunk->u1.AddressOfData + 2, const char *),
                                  ~0, &Value, pvUser);
                Log4((RT_SUCCESS(rc) ? "RTLdrPE:  %RTptr %s\n" : "RTLdrPE:  %08RX32 %s rc=%Vrc\n",
                      (uint32_t)Value, PE_RVA2TYPE(pvBitsR, (char*)pThunk->u1.AddressOfData + 2, const char *), rc));
            }
            else
            {
                AssertMsgFailed(("bad import data thunk!\n"));
                rc = VERR_BAD_EXE_FORMAT;
            }
            pFirstThunk->u1.Function = Value;
            if (pFirstThunk->u1.Function != Value)
            {
                AssertMsgFailed(("external symbol address to big!\n"));
                rc = VERR_ADDRESS_CONFLICT; /** @todo get me a better error status code. */
            }
            pThunk++;
            pFirstThunk++;
        }
    }

    return rc;
}


/** @copydoc RTLDROPSPE::pfnResolveImports */
static DECLCALLBACK(int) rtldrPEResolveImports64(PRTLDRMODPE pModPe, const void *pvBitsR, void *pvBitsW, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    /*
     * Check if there is actually anything to work on.
     */
    if (    !pModPe->ImportDir.VirtualAddress
        ||  !pModPe->ImportDir.Size)
        return 0;

    /*
     * Walk the IMAGE_IMPORT_DESCRIPTOR table.
     */
    int                         rc = VINF_SUCCESS;
    PIMAGE_IMPORT_DESCRIPTOR    pImps;
    for (pImps = PE_RVA2TYPE(pvBitsR, pModPe->ImportDir.VirtualAddress, PIMAGE_IMPORT_DESCRIPTOR);
         !rc && pImps->Name != 0 && pImps->FirstThunk != 0;
         pImps++)
    {
        const char *        pszModName = PE_RVA2TYPE(pvBitsR, pImps->Name, const char *);
        PIMAGE_THUNK_DATA64 pFirstThunk;    /* update this. */
        PIMAGE_THUNK_DATA64 pThunk;         /* read from this. */
        Log3(("RTLdrPE: Import descriptor: %s\n", pszModName));
        Log4(("RTLdrPE:   OriginalFirstThunk = %#RX32\n"
              "RTLdrPE:   TimeDateStamp      = %#RX32\n"
              "RTLdrPE:   ForwarderChain     = %#RX32\n"
              "RTLdrPE:   Name               = %#RX32\n"
              "RTLdrPE:   FirstThunk         = %#RX32\n",
              pImps->u.OriginalFirstThunk, pImps->TimeDateStamp,
              pImps->ForwarderChain, pImps->Name, pImps->FirstThunk));

        /*
         * Walk the thunks table(s).
         */
        pFirstThunk = PE_RVA2TYPE(pvBitsW, pImps->FirstThunk, PIMAGE_THUNK_DATA64);
        pThunk = pImps->u.OriginalFirstThunk == 0
            ? PE_RVA2TYPE(pvBitsR, pImps->FirstThunk, PIMAGE_THUNK_DATA64)
            : PE_RVA2TYPE(pvBitsR, pImps->u.OriginalFirstThunk, PIMAGE_THUNK_DATA64);
        while (!rc && pThunk->u1.Ordinal != 0)
        {
            RTUINTPTR Value = 0;
            if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
            {
                rc = pfnGetImport(&pModPe->Core, pszModName, NULL, (unsigned)IMAGE_ORDINAL64(pThunk->u1.Ordinal), &Value, pvUser);
                Log4((RT_SUCCESS(rc) ? "RTLdrPE:  %016RX64 #%u\n" : "RTLdrPE:  %016RX64 #%u rc=%Vrc\n",
                      (uint64_t)Value, (unsigned)IMAGE_ORDINAL64(pThunk->u1.Ordinal), rc));
            }
            else if (   pThunk->u1.Ordinal > 0
                     && pThunk->u1.Ordinal < pModPe->cbImage)
            {
                /** @todo add validation of the string pointer! */
                rc = pfnGetImport(&pModPe->Core, pszModName, PE_RVA2TYPE(pvBitsR, (uintptr_t)pThunk->u1.AddressOfData + 2, const char *),
                                  ~0, &Value, pvUser);
                Log4((RT_SUCCESS(rc) ? "RTLdrPE:  %016RX64 %s\n" : "RTLdrPE:  %016RX64 %s rc=%Vrc\n",
                      (uint64_t)Value, PE_RVA2TYPE(pvBitsR, (uintptr_t)pThunk->u1.AddressOfData + 2, const char *), rc));
            }
            else
            {
                AssertMsgFailed(("bad import data thunk!\n"));
                rc = VERR_BAD_EXE_FORMAT;
            }
            pFirstThunk->u1.Function = Value;
            pThunk++;
            pFirstThunk++;
        }
    }

    return rc;
}


/**
 * Applies fixups.
 */
static int rtldrPEApplyFixups(PRTLDRMODPE pModPe, const void *pvBitsR, void *pvBitsW, RTUINTPTR BaseAddress, RTUINTPTR OldBaseAddress)
{
    if (    !pModPe->RelocDir.VirtualAddress
        ||  !pModPe->RelocDir.Size)
        return 0;

    /*
     * Apply delta fixups iterating fixup chunks.
     */
    PIMAGE_BASE_RELOCATION  pbr = PE_RVA2TYPE(pvBitsR, pModPe->RelocDir.VirtualAddress, PIMAGE_BASE_RELOCATION);
    PIMAGE_BASE_RELOCATION  pBaseRelocs = pbr;
    unsigned                cbBaseRelocs = pModPe->RelocDir.Size;
    RTUINTPTR               uDelta = BaseAddress - OldBaseAddress;
    Log2(("RTLdrPE: Fixups: uDelta=%#RTptr BaseAddress=%#RTptr OldBaseAddress=%#RTptr\n", uDelta, BaseAddress, OldBaseAddress));
    Log4(("RTLdrPE: BASERELOC: VirtualAddres=%RX32 Size=%RX32\n", pModPe->RelocDir.VirtualAddress, pModPe->RelocDir.Size));
    Assert(sizeof(*pbr) == sizeof(uint32_t) * 2);

    while (   (uintptr_t)pbr - (uintptr_t)pBaseRelocs + 8 < cbBaseRelocs /* 8= VirtualAddress and SizeOfBlock members */
           && pbr->SizeOfBlock >= 8)
    {
        uint16_t   *pwoffFixup   = (uint16_t *)(pbr + 1);
        uint32_t    cRelocations = (pbr->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        Log3(("RTLdrPE: base relocs for %#010x, size %#06x (%d relocs)\n", pbr->VirtualAddress, pbr->SizeOfBlock, cRelocations));

        /* Some bound checking just to be sure it works... */
        if ((uintptr_t)pbr - (uintptr_t)pBaseRelocs + pbr->SizeOfBlock > cbBaseRelocs)
            cRelocations = (((uintptr_t)pBaseRelocs + cbBaseRelocs) - (uintptr_t)pbr - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

        /*
         * Loop thru the fixups in this chunk.
         */
        while (cRelocations != 0)
        {
            /*
             * Common fixup
             */
            static const char * const s_apszReloc[16] =
            {
                "ABS", "HIGH", "LOW", "HIGHLOW", "HIGHADJ", "MIPS_JMPADDR", "RES6", "RES7",
                "RES8", "IA64_IMM64", "DIR64", "HIGH3ADJ", "RES12", "RES13", "RES14", "RES15"
            }; NOREF(s_apszReloc);
            union
            {
                uint16_t   *pu16;
                uint32_t   *pu32;
                uint64_t   *pu64;
            } u;
            const int offFixup  = *pwoffFixup & 0xfff;
            u.pu32 = PE_RVA2TYPE(pvBitsW, offFixup + pbr->VirtualAddress, uint32_t *);
            const int fType     = *pwoffFixup >> 12;
            Log4(("RTLdrPE: %08x %s\n", offFixup + pbr->VirtualAddress, s_apszReloc[fType]));
            switch (fType)
            {
                case IMAGE_REL_BASED_HIGHLOW:   /* 32-bit, add delta. */
                    *u.pu32 += uDelta;
                    break;
                case IMAGE_REL_BASED_DIR64:     /* 64-bit, add delta. */
                    *u.pu64 += (RTINTPTR)uDelta;
                    break;
                case IMAGE_REL_BASED_ABSOLUTE:  /* Alignment placeholder. */
                    break;
                /* odd ones */
                case IMAGE_REL_BASED_LOW:       /* 16-bit, add 1st 16-bit part of the delta. */
                    *u.pu16 += (uint16_t)uDelta;
                    break;
                case IMAGE_REL_BASED_HIGH:      /* 16-bit, add 2nd 16-bit part of the delta. */
                    *u.pu16 += (uint16_t)(uDelta >> 16);
                    break;
                /* never ever seen these next two, and I'm not 100% sure they are correctly implemented here. */
                case IMAGE_REL_BASED_HIGHADJ:
                {
                    if (cRelocations <= 1)
                    {
                        AssertMsgFailed(("HIGHADJ missing 2nd record!\n"));
                        return VERR_BAD_EXE_FORMAT;
                    }
                    cRelocations--;
                    pwoffFixup++;
                    int32_t i32 = (uint32_t)(*u.pu16 << 16) | *pwoffFixup;
                    i32 += uDelta;
                    i32 += 0x8000; //??
                    *u.pu16 = (uint16_t)(i32 >> 16);
                    break;
                }
                case IMAGE_REL_BASED_HIGH3ADJ:
                {
                    if (cRelocations <= 2)
                    {
                        AssertMsgFailed(("HIGHADJ3 missing 2nd record!\n"));
                        return VERR_BAD_EXE_FORMAT;
                    }
                    cRelocations -= 2;
                    pwoffFixup++;
                    int64_t i64 = ((uint64_t)*u.pu16 << 32) | *(uint32_t *)pwoffFixup++;
                    i64 += (int64_t)uDelta << 16; //??
                    i64 += 0x80000000;//??
                    *u.pu16 = (uint16_t)(i64 >> 32);
                    break;
                }
                default:
                    AssertMsgFailed(("Unknown fixup type %d offset=%#x\n", fType, offFixup));
                    break;
            }

            /*
             * Next offset/type
             */
            pwoffFixup++;
            cRelocations--;
        } /* while loop */

        /*
         * Next Fixup chunk. (i.e. next page)
         */
        pbr = (PIMAGE_BASE_RELOCATION)((uintptr_t)pbr + pbr->SizeOfBlock);
    } /* while loop */

    return 0;
}


/** @copydoc RTLDROPS::pfnRelocate. */
static int rtldrPERelocate(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress, RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;

    /*
     * Do we have to read the image bits?
     */
    if (!pModPe->pvBits)
    {
        int rc = rtldrPEReadBits(pModPe);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Process imports.
     */
    int rc = ((PRTLDROPSPE)pMod->pOps)->pfnResolveImports(pModPe, pModPe->pvBits, pvBits, pfnGetImport, pvUser);
    if (RT_SUCCESS(rc))
    {
        /*
         * Apply relocations.
         */
        rc = rtldrPEApplyFixups(pModPe, pModPe->pvBits, pvBits, NewBaseAddress, OldBaseAddress);
        AssertRC(rc);
    }
    return rc;
}


/** @copydoc RTLDROPS::pfnGetSymbolEx. */
static DECLCALLBACK(int) rtldrPEGetSymbolEx(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress, const char *pszSymbol, RTUINTPTR *pValue)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;

    /*
     * Check if there is actually anything to work on.
     */
    if (   !pModPe->ExportDir.VirtualAddress
        || !pModPe->ExportDir.Size)
        return VERR_SYMBOL_NOT_FOUND;

    /*
     * No bits supplied? Do we need to read the bits?
     */
    if (!pvBits)
    {
        if (!pModPe->pvBits)
        {
            int rc = rtldrPEReadBits(pModPe);
            if (RT_FAILURE(rc))
                return rc;
        }
        pvBits = pModPe->pvBits;
    }

    PIMAGE_EXPORT_DIRECTORY pExpDir = PE_RVA2TYPE(pvBits, pModPe->ExportDir.VirtualAddress, PIMAGE_EXPORT_DIRECTORY);
    int                     iExpOrdinal = 0;    /* index into address table. */
    if ((uintptr_t)pszSymbol <= 0xffff)
    {
        /*
         * Find ordinal export: Simple table lookup.
         */
        unsigned uOrdinal = (uintptr_t)pszSymbol & 0xffff;
        if (    uOrdinal >= pExpDir->Base + RT_MAX(pExpDir->NumberOfNames, pExpDir->NumberOfFunctions)
            ||  uOrdinal < pExpDir->Base)
            return VERR_SYMBOL_NOT_FOUND;
        iExpOrdinal = uOrdinal - pExpDir->Base;
    }
    else
    {
        /*
         * Find Named Export: Do binary search on the name table.
         */
        uint32_t   *paRVANames = PE_RVA2TYPE(pvBits, pExpDir->AddressOfNames, uint32_t *);
        uint16_t   *paOrdinals = PE_RVA2TYPE(pvBits, pExpDir->AddressOfNameOrdinals, uint16_t *);
        int         iStart = 1;
        int         iEnd = pExpDir->NumberOfNames;

        for (;;)
        {
            /* end of search? */
            if (iStart > iEnd)
            {
            #ifdef RT_STRICT
                /* do a linear search just to verify the correctness of the above algorithm */
                for (unsigned i = 0; i < pExpDir->NumberOfNames; i++)
                {
                    AssertMsg(i == 0 || strcmp(PE_RVA2TYPE(pvBits, paRVANames[i], const char *), PE_RVA2TYPE(pvBits, paRVANames[i - 1], const char *)) > 0,
                              ("bug in binary export search!!!\n"));
                    AssertMsg(strcmp(PE_RVA2TYPE(pvBits, paRVANames[i], const char *), pszSymbol) != 0,
                              ("bug in binary export search!!!\n"));
                }
            #endif
                return VERR_SYMBOL_NOT_FOUND;
            }

            int i  = (iEnd - iStart) / 2 + iStart;
            const char *pszExpName  = PE_RVA2TYPE(pvBits, paRVANames[i - 1], const char *);
            int         diff        = strcmp(pszExpName, pszSymbol);
            if (diff > 0)       /* pszExpName > pszSymbol: search chunck before i */
                iEnd = i - 1;
            else if (diff)      /* pszExpName < pszSymbol: search chunk after i */
                iStart = i + 1;
            else                /* pszExpName == pszSymbol */
            {
                iExpOrdinal = paOrdinals[i - 1];
                break;
            }
        } /* binary search thru name table */
    }

    /*
     * Found export (iExpOrdinal).
     */
    uint32_t *  paAddress = PE_RVA2TYPE(pvBits, pExpDir->AddressOfFunctions, uint32_t *);
    unsigned    uRVAExport = paAddress[iExpOrdinal];

    if (    uRVAExport > pModPe->ExportDir.VirtualAddress
        &&  uRVAExport < pModPe->ExportDir.VirtualAddress + pModPe->ExportDir.Size)
    {
        /* Resolve forwarder. */
        AssertMsgFailed(("Forwarders are not supported!\n"));
        return VERR_SYMBOL_NOT_FOUND;
    }

    /* Get plain export address */
    *pValue = PE_RVA2TYPE(BaseAddress, uRVAExport, RTUINTPTR);

    return VINF_SUCCESS;
}


/** @copydoc RTLDROPS::pfnEnumSymbols */
static DECLCALLBACK(int) rtldrPEEnumSymbols(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress,
                                            PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;

    /*
     * Check if there is actually anything to work on.
     */
    if (   !pModPe->ExportDir.VirtualAddress
        || !pModPe->ExportDir.Size)
        return VERR_SYMBOL_NOT_FOUND;

    /*
     * No bits supplied? Do we need to read the bits?
     */
    if (!pvBits)
    {
        if (!pModPe->pvBits)
        {
            int rc = rtldrPEReadBits(pModPe);
            if (RT_FAILURE(rc))
                return rc;
        }
        pvBits = pModPe->pvBits;
    }

    /*
     * We enumerates by ordinal, which means using a slow linear search for
     * getting any name
     */
    PIMAGE_EXPORT_DIRECTORY pExpDir = PE_RVA2TYPE(pvBits, pModPe->ExportDir.VirtualAddress, PIMAGE_EXPORT_DIRECTORY);
    uint32_t   *paAddress  = PE_RVA2TYPE(pvBits, pExpDir->AddressOfFunctions, uint32_t *);
    uint32_t   *paRVANames = PE_RVA2TYPE(pvBits, pExpDir->AddressOfNames, uint32_t *);
    uint16_t   *paOrdinals = PE_RVA2TYPE(pvBits, pExpDir->AddressOfNameOrdinals, uint16_t *);
    uintptr_t   uNamePrev = 0;
    unsigned    cOrdinals = RT_MAX(pExpDir->NumberOfNames, pExpDir->NumberOfFunctions);
    for (unsigned uOrdinal = 0; uOrdinal < cOrdinals; uOrdinal++)
    {
        if (paAddress[uOrdinal] /* needed? */)
        {
            /*
             * Look for name.
             */
            const char *pszName = NULL;
            /* Search from previous + 1 to the end.  */
            unsigned    uName = uNamePrev + 1;
            while (uName < pExpDir->NumberOfNames)
            {
                if (paOrdinals[uName] == uOrdinal)
                {
                    pszName = PE_RVA2TYPE(pvBits, paRVANames[uName], const char *);
                    uNamePrev = uName;
                    break;
                }
                uName++;
            }
            if (!pszName)
            {
                /* Search from start to the previous. */
                uName = 0;
                for (uName = 0 ; uName <= uNamePrev; uName++)
                {
                    if (paOrdinals[uName] == uOrdinal)
                    {
                        pszName = PE_RVA2TYPE(pvBits, paRVANames[uName], const char *);
                        uNamePrev = uName;
                        break;
                    }
                }
            }

            /*
             * Get address.
             */
            uintptr_t   uRVAExport = paAddress[uOrdinal];
            RTUINTPTR Value;
            if (    uRVAExport - (uintptr_t)pModPe->ExportDir.VirtualAddress
                <   pModPe->ExportDir.Size)
            {
                /* Resolve forwarder. */
                AssertMsgFailed(("Forwarders are not supported!\n"));
                continue;
            }
            else
                /* Get plain export address */
                Value = PE_RVA2TYPE(BaseAddress, uRVAExport, RTUINTPTR);

            /*
             * Call back.
             */
            int rc = pfnCallback(pMod, pszName, uOrdinal + pExpDir->Base, Value, pvUser);
            if (rc)
                return rc;
        }
    }

    return VINF_SUCCESS;
}


/** @copydoc RTLDROPS::pfnDone */
static DECLCALLBACK(int) rtldrPEDone(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;
    if (pModPe->pvBits)
    {
        RTMemFree(pModPe->pvBits);
        pModPe->pvBits = NULL;
    }
    if (pModPe->pReader)
    {
        int rc = pModPe->pReader->pfnDestroy(pModPe->pReader);
        AssertRC(rc);
        pModPe->pReader = NULL;
    }
    return VINF_SUCCESS;
}

/** @copydoc RTLDROPS::pfnClose */
static DECLCALLBACK(int) rtldrPEClose(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODPE pModPe = (PRTLDRMODPE)pMod;
    if (pModPe->paSections)
    {
        RTMemFree(pModPe->paSections);
        pModPe->paSections = NULL;
    }
    if (pModPe->pvBits)
    {
        RTMemFree(pModPe->pvBits);
        pModPe->pvBits = NULL;
    }
    if (pModPe->pReader)
    {
        int rc = pModPe->pReader->pfnDestroy(pModPe->pReader);
        AssertRC(rc);
        pModPe->pReader = NULL;
    }
    return VINF_SUCCESS;
}


/**
 * Operations for a 32-bit PE module.
 */
static const RTLDROPSPE s_rtldrPE32Ops =
{
    {
        "pe32",
        rtldrPEClose,
        NULL,
        rtldrPEDone,
        rtldrPEEnumSymbols,
        /* ext */
        rtldrPEGetImageSize,
        rtldrPEGetBits,
        rtldrPERelocate,
        rtldrPEGetSymbolEx,
        42
    },
    rtldrPEResolveImports32,
    42
};


/**
 * Operations for a 64-bit PE module.
 */
static const RTLDROPSPE s_rtldrPE64Ops =
{
    {
        "pe64",
        rtldrPEClose,
        NULL,
        rtldrPEDone,
        rtldrPEEnumSymbols,
        /* ext */
        rtldrPEGetImageSize,
        rtldrPEGetBits,
        rtldrPERelocate,
        rtldrPEGetSymbolEx,
        42
    },
    rtldrPEResolveImports64,
    42
};


/**
 * Converts the optional header from 32 bit to 64 bit.
 * This is a rather simple task, if you start from the right end.
 *
 * @param   pOptHdr     On input this is a PIMAGE_OPTIONAL_HEADER32.
 *                      On output this will be a PIMAGE_OPTIONAL_HEADER64.
 */
static void rtldrPEConvert32BitOptionalHeaderTo64Bit(PIMAGE_OPTIONAL_HEADER64 pOptHdr)
{
    /*
     * volatile everywhere! Trying to prevent the compiler being a smarta$$ and reorder stuff.
     */
    IMAGE_OPTIONAL_HEADER32 volatile *pOptHdr32 = (IMAGE_OPTIONAL_HEADER32 volatile *)pOptHdr;
    IMAGE_OPTIONAL_HEADER64 volatile *pOptHdr64 = pOptHdr;

    /* from LoaderFlags and out the difference is 4 * 32-bits. */
    Assert(RT_OFFSETOF(IMAGE_OPTIONAL_HEADER32, LoaderFlags) + 16 == RT_OFFSETOF(IMAGE_OPTIONAL_HEADER64, LoaderFlags));
    Assert(     RT_OFFSETOF(IMAGE_OPTIONAL_HEADER32, DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]) + 16
           ==   RT_OFFSETOF(IMAGE_OPTIONAL_HEADER64, DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]));
    uint32_t volatile       *pu32Dst     = (uint32_t *)&pOptHdr64->DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] - 1;
    const uint32_t volatile *pu32Src     = (uint32_t *)&pOptHdr32->DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] - 1;
    const uint32_t volatile *pu32SrcLast = (uint32_t *)&pOptHdr32->LoaderFlags;
    while (pu32Src >= pu32SrcLast)
        *pu32Dst-- = *pu32Src--;

    /* the previous 4 fields are 32/64 and needs special attention. */
    pOptHdr64->SizeOfHeapCommit   = pOptHdr32->SizeOfHeapCommit;
    pOptHdr64->SizeOfHeapReserve  = pOptHdr32->SizeOfHeapReserve;
    pOptHdr64->SizeOfStackCommit  = pOptHdr32->SizeOfStackCommit;
    uint32_t u32SizeOfStackReserve = pOptHdr32->SizeOfStackReserve;
    pOptHdr64->SizeOfStackReserve = u32SizeOfStackReserve;

    /* The rest matches except for BaseOfData which has been merged into ImageBase in the 64-bit version..
     * Thus, ImageBase needs some special treatement. It will probably work fine assigning one to the
     * other since this is all declared volatile, but taking now chances, we'll use a temp variable.
     */
    Assert(RT_OFFSETOF(IMAGE_OPTIONAL_HEADER32, SizeOfStackReserve) == RT_OFFSETOF(IMAGE_OPTIONAL_HEADER64, SizeOfStackReserve));
    Assert(RT_OFFSETOF(IMAGE_OPTIONAL_HEADER32, BaseOfData) == RT_OFFSETOF(IMAGE_OPTIONAL_HEADER64, ImageBase));
    Assert(RT_OFFSETOF(IMAGE_OPTIONAL_HEADER32, SectionAlignment) == RT_OFFSETOF(IMAGE_OPTIONAL_HEADER64, SectionAlignment));
    uint32_t u32ImageBase = pOptHdr32->ImageBase;
    pOptHdr64->ImageBase = u32ImageBase;
}


/**
 * Converts the load config directory from 32 bit to 64 bit.
 * This is a rather simple task, if you start from the right end.
 *
 * @param   pLoadCfg    On input this is a PIMAGE_LOAD_CONFIG_DIRECTORY32.
 *                      On output this will be a PIMAGE_LOAD_CONFIG_DIRECTORY64.
 */
static void rtldrPEConvert32BitLoadConfigTo64Bit(PIMAGE_LOAD_CONFIG_DIRECTORY64 pLoadCfg)
{
    /*
     * volatile everywhere! Trying to prevent the compiler being a smarta$$ and reorder stuff.
     */
    IMAGE_LOAD_CONFIG_DIRECTORY32 volatile *pLoadCfg32 = (IMAGE_LOAD_CONFIG_DIRECTORY32 volatile *)pLoadCfg;
    IMAGE_LOAD_CONFIG_DIRECTORY64 volatile *pLoadCfg64 = pLoadCfg;

    pLoadCfg64->SEHandlerCount             = pLoadCfg32->SEHandlerCount;
    pLoadCfg64->SEHandlerTable             = pLoadCfg32->SEHandlerTable;
    pLoadCfg64->SecurityCookie             = pLoadCfg32->SecurityCookie;
    pLoadCfg64->EditList                   = pLoadCfg32->EditList;
    pLoadCfg64->Reserved1                  = pLoadCfg32->Reserved1;
    pLoadCfg64->CSDVersion                 = pLoadCfg32->CSDVersion;
    pLoadCfg64->ProcessHeapFlags           = pLoadCfg32->ProcessHeapFlags; /* switched place with ProcessAffinityMask, but we're more than 16 byte off by now so it doesn't matter. */
    pLoadCfg64->ProcessAffinityMask        = pLoadCfg32->ProcessAffinityMask;
    pLoadCfg64->VirtualMemoryThreshold     = pLoadCfg32->VirtualMemoryThreshold;
    pLoadCfg64->MaximumAllocationSize      = pLoadCfg32->MaximumAllocationSize;
    pLoadCfg64->LockPrefixTable            = pLoadCfg32->LockPrefixTable;
    pLoadCfg64->DeCommitTotalFreeThreshold = pLoadCfg32->DeCommitTotalFreeThreshold;
    uint32_t u32DeCommitFreeBlockThreshold = pLoadCfg32->DeCommitFreeBlockThreshold;
    pLoadCfg64->DeCommitFreeBlockThreshold = u32DeCommitFreeBlockThreshold;
    /* the rest is equal. */
    Assert(     RT_OFFSETOF(IMAGE_LOAD_CONFIG_DIRECTORY32, DeCommitFreeBlockThreshold)
           ==   RT_OFFSETOF(IMAGE_LOAD_CONFIG_DIRECTORY64, DeCommitFreeBlockThreshold));
}


/**
 * Validates the file header.
 *
 * @returns iprt status code.
 * @param   pFileHdr    Pointer to the file header that needs validating.
 * @param   pszLogName  The log name to  prefix the errors with.
 */
int rtldrPEValidateFileHeader(PIMAGE_FILE_HEADER pFileHdr, const char *pszLogName)
{
    size_t cbOptionalHeader;
    switch (pFileHdr->Machine)
    {
        case IMAGE_FILE_MACHINE_I386:
            cbOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
            break;
        case IMAGE_FILE_MACHINE_AMD64:
            cbOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            break;

        default:
            Log(("rtldrPEOpen: %s: Unsupported Machine=%#x\n",
                 pszLogName, pFileHdr->Machine));
            return VERR_BAD_EXE_FORMAT;
    }
    if (pFileHdr->SizeOfOptionalHeader != cbOptionalHeader)
    {
        Log(("rtldrPEOpen: %s: SizeOfOptionalHeader=%#x expected %#x\n",
             pszLogName, pFileHdr->SizeOfOptionalHeader, cbOptionalHeader));
        return VERR_BAD_EXE_FORMAT;
    }
    /* This restriction needs to be implemented elsewhere. */
    if (pFileHdr->Characteristics & IMAGE_FILE_RELOCS_STRIPPED)
    {
        Log(("rtldrPEOpen: %s: IMAGE_FILE_RELOCS_STRIPPED\n", pszLogName));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pFileHdr->NumberOfSections > 42)
    {
        Log(("rtldrPEOpen: %s: NumberOfSections=%d - our limit is 42, please raise it if the binary makes sense.(!!!)\n",
             pszLogName, pFileHdr->NumberOfSections));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pFileHdr->NumberOfSections < 1)
    {
        Log(("rtldrPEOpen: %s: NumberOfSections=%d - we can't have an image without sections (!!!)\n",
             pszLogName, pFileHdr->NumberOfSections));
        return VERR_BAD_EXE_FORMAT;
    }
    return VINF_SUCCESS;
}


/**
 * Validates the optional header (64/32-bit)
 *
 * @returns iprt status code.
 * @param   pOptHdr     Pointer to the optional header which needs validation.
 * @param   pszLogName  The log name to  prefix the errors with.
 * @param   offNtHdrs   The offset of the NT headers from teh start of the file.
 * @param   pFileHdr    Pointer to the file header (valid).
 * @param   cbRawImage  The raw image size.
 */
static int rtldrPEValidateOptionalHeader(const IMAGE_OPTIONAL_HEADER64 *pOptHdr, const char *pszLogName, RTFOFF offNtHdrs,
                                         const IMAGE_FILE_HEADER *pFileHdr, RTFOFF cbRawImage)
{
    const uint16_t CorrectMagic = pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32)
             ? IMAGE_NT_OPTIONAL_HDR32_MAGIC : IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    if (pOptHdr->Magic != CorrectMagic)
    {
        Log(("rtldrPEOpen: %s: Magic=%#x - expected %#x!!!\n", pszLogName, pOptHdr->Magic, CorrectMagic));
        return VERR_BAD_EXE_FORMAT;
    }
    const uint32_t cbImage = pOptHdr->SizeOfImage;
    if (cbImage > _1G)
    {
        Log(("rtldrPEOpen: %s: SizeOfImage=%#x - Our limit is 1GB (%#x)!!!\n", pszLogName, cbImage, _1G));
        return VERR_BAD_EXE_FORMAT;
    }
    const uint32_t cbMinImageSize = pFileHdr->SizeOfOptionalHeader + sizeof(*pFileHdr) + 4 + (uint32_t)offNtHdrs;
    if (cbImage < cbMinImageSize)
    {
        Log(("rtldrPEOpen: %s: SizeOfImage=%#x to small, minimum %#x!!!\n", pszLogName, cbImage, cbMinImageSize));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pOptHdr->AddressOfEntryPoint >= cbImage)
    {
        Log(("rtldrPEOpen: %s: AddressOfEntryPoint=%#x - beyond image size (%#x)!!!\n",
             pszLogName, pOptHdr->AddressOfEntryPoint, cbImage));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pOptHdr->BaseOfCode >= cbImage)
    {
        Log(("rtldrPEOpen: %s: BaseOfCode=%#x - beyond image size (%#x)!!!\n",
             pszLogName, pOptHdr->BaseOfCode, cbImage));
        return VERR_BAD_EXE_FORMAT;
    }
#if 0/* only in 32-bit header */
    if (pOptHdr->BaseOfData >= cbImage)
    {
        Log(("rtldrPEOpen: %s: BaseOfData=%#x - beyond image size (%#x)!!!\n",
             pszLogName, pOptHdr->BaseOfData, cbImage));
        return VERR_BAD_EXE_FORMAT;
    }
#endif
    if (pOptHdr->SizeOfHeaders >= cbImage)
    {
        Log(("rtldrPEOpen: %s: SizeOfHeaders=%#x - beyond image size (%#x)!!!\n",
             pszLogName, pOptHdr->SizeOfHeaders, cbImage));
        return VERR_BAD_EXE_FORMAT;
    }
    /* don't know how to do the checksum, so ignore it. */
    if (pOptHdr->Subsystem == IMAGE_SUBSYSTEM_UNKNOWN)
    {
        Log(("rtldrPEOpen: %s: Subsystem=%#x (unknown)!!!\n", pszLogName, pOptHdr->Subsystem));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pOptHdr->SizeOfHeaders < cbMinImageSize + pFileHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER))
    {
        Log(("rtldrPEOpen: %s: SizeOfHeaders=%#x - cbMinImageSize %#x + sections %#x = %#llx!!!\n",
             pszLogName, pOptHdr->SizeOfHeaders,
             cbImage, cbMinImageSize, pFileHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER),
             cbMinImageSize + pFileHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER)));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pOptHdr->SizeOfStackReserve < pOptHdr->SizeOfStackCommit)
    {
        Log(("rtldrPEOpen: %s: SizeOfStackReserve %#x < SizeOfStackCommit %#x!!!\n",
             pszLogName, pOptHdr->SizeOfStackReserve, pOptHdr->SizeOfStackCommit));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pOptHdr->SizeOfHeapReserve < pOptHdr->SizeOfHeapCommit)
    {
        Log(("rtldrPEOpen: %s: SizeOfStackReserve %#x < SizeOfStackCommit %#x!!!\n",
             pszLogName, pOptHdr->SizeOfStackReserve, pOptHdr->SizeOfStackCommit));
        return VERR_BAD_EXE_FORMAT;
    }

    /* DataDirectory */
    if (pOptHdr->NumberOfRvaAndSizes != ELEMENTS(pOptHdr->DataDirectory))
    {
        Log(("rtldrPEOpen: %s: NumberOfRvaAndSizes=%d!!!\n", pszLogName, pOptHdr->NumberOfRvaAndSizes));
        return VERR_BAD_EXE_FORMAT;
    }
    for (unsigned i = 0; i < ELEMENTS(pOptHdr->DataDirectory); i++)
    {
        IMAGE_DATA_DIRECTORY const *pDir = &pOptHdr->DataDirectory[i];
        if (!pDir->Size)
            continue;
        size_t cb = cbImage;
        switch (i)
        {
            case IMAGE_DIRECTORY_ENTRY_EXPORT:        // 0
            case IMAGE_DIRECTORY_ENTRY_IMPORT:        // 1
            case IMAGE_DIRECTORY_ENTRY_RESOURCE:      // 2
            case IMAGE_DIRECTORY_ENTRY_EXCEPTION:     // 3
            case IMAGE_DIRECTORY_ENTRY_BASERELOC:     // 5
            case IMAGE_DIRECTORY_ENTRY_DEBUG:         // 6
            case IMAGE_DIRECTORY_ENTRY_COPYRIGHT:     // 7
            case IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT:  // 11
            case IMAGE_DIRECTORY_ENTRY_IAT:           // 12  /* Import Address Table */
                break;
            case IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG:   // 10 - need to check for lock prefixes.
                /* Delay inspection after section table is validated. */
                break;

            case IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT:  // 13
                Log(("rtldrPEOpen: %s: dir no. %d (DELAY_IMPORT) VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_LDRPE_DELAY_IMPORT;

            /* The security directory seems to be some kind of hack, and the rva is a fileoffset or something. */
            case IMAGE_DIRECTORY_ENTRY_SECURITY:      // 4
                cb = (size_t)cbRawImage; Assert((RTFOFF)cb == cbRawImage);
                Log(("rtldrPEOpen: %s: dir no. %d (SECURITY) VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_LDRPE_SECURITY;

            case IMAGE_DIRECTORY_ENTRY_GLOBALPTR:     // 8   /* (MIPS GP) */
                Log(("rtldrPEOpen: %s: dir no. %d (GLOBALPTR) VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_LDRPE_GLOBALPTR;

            case IMAGE_DIRECTORY_ENTRY_TLS:           // 9
                Log(("rtldrPEOpen: %s: dir no. %d (TLS) VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_LDRPE_TLS;

            case IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR: // 14
                Log(("rtldrPEOpen: %s: dir no. %d (COM_DESCRIPTOR) VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_LDRPE_COM_DESCRIPTOR;

            default:
                Log(("rtldrPEOpen: %s: dir no. %d VirtualAddress=%#x Size=%#x is not supported!!!\n",
                     pszLogName, i, pDir->VirtualAddress, pDir->Size));
                return VERR_BAD_EXE_FORMAT;
        }
        if (pDir->VirtualAddress >= cb)
        {
            Log(("rtldrPEOpen: %s: dir no. %d VirtualAddress=%#x is invalid (limit %#x)!!!\n",
                 pszLogName, i, pDir->VirtualAddress, cb));
            return VERR_BAD_EXE_FORMAT;
        }
        if (pDir->Size > cb - pDir->VirtualAddress)
        {
            Log(("rtldrPEOpen: %s: dir no. %d Size=%#x is invalid (rva=%#x, limit=%#x)!!!\n",
                 pszLogName, i, pDir->Size, pDir->VirtualAddress, cb));
            return VERR_BAD_EXE_FORMAT;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Validates the section headers.
 *
 * @returns iprt status code.
 * @param   paSections  Pointer to the array of sections that is to be validated.
 * @param   cSections   Number of sections in that array.
 * @param   pszLogName  The log name to  prefix the errors with.
 * @param   pOptHdr     Pointer to the optional header (valid).
 * @param   cbRawImage  The raw image size.
 */
int rtldrPEValidateSectionHeaders(const IMAGE_SECTION_HEADER *paSections, unsigned cSections, const char *pszLogName,
                                  const IMAGE_OPTIONAL_HEADER64 *pOptHdr, RTFOFF cbRawImage)
{
    const uint32_t              cbImage  = pOptHdr->SizeOfImage;
    const IMAGE_SECTION_HEADER *pSH      = &paSections[0];
    uint32_t                    uRvaPrev = pOptHdr->SizeOfHeaders;
    Log3(("RTLdrPE: Section Headers:\n"));
    for (unsigned cSHdrsLeft = cSections;  cSHdrsLeft > 0; cSHdrsLeft--, pSH++)
    {
        const unsigned iSH = pSH - &paSections[0]; NOREF(iSH);
        Log3(("RTLdrPE: #%d '%-8.8s'  Characteristics: %08RX32\n"
              "RTLdrPE: VirtAddr: %08RX32  VirtSize: %08RX32\n"
              "RTLdrPE:  FileOff: %08RX32  FileSize: %08RX32\n"
              "RTLdrPE: RelocOff: %08RX32   #Relocs: %08RX32\n"
              "RTLdrPE:  LineOff: %08RX32    #Lines: %08RX32\n",
              iSH, pSH->Name, pSH->Characteristics,
              pSH->VirtualAddress, pSH->Misc.VirtualSize,
              pSH->PointerToRawData, pSH->SizeOfRawData,
              pSH->PointerToRelocations, pSH->NumberOfRelocations,
              pSH->PointerToLinenumbers, pSH->NumberOfLinenumbers));
        if (pSH->Characteristics & (IMAGE_SCN_MEM_16BIT | IMAGE_SCN_MEM_FARDATA | IMAGE_SCN_MEM_PURGEABLE | IMAGE_SCN_MEM_PRELOAD))
        {
            Log(("rtldrPEOpen: %s: Unsupported section flag(s) %#x section #%d '%.*s'!!!\n",
                 pszLogName, pSH->Characteristics, iSH, sizeof(pSH->Name), pSH->Name));
            return VERR_BAD_EXE_FORMAT;
        }

        if (    pSH->Misc.VirtualSize
            &&  !(pSH->Characteristics & IMAGE_SCN_TYPE_NOLOAD)) /* binutils uses this for '.stab' even if it's reserved/obsoleted by MS. */
        {
            if (pSH->VirtualAddress < uRvaPrev)
            {
                Log(("rtldrPEOpen: %s: Overlaps previous section or sections aren't in ascending order, VirtualAddress=%#x uRvaPrev=%#x - section #%d '%.*s'!!!\n",
                     pszLogName, pSH->VirtualAddress, uRvaPrev, iSH, sizeof(pSH->Name), pSH->Name));
                return VERR_BAD_EXE_FORMAT;
            }
            if (pSH->VirtualAddress > cbImage)
            {
                Log(("rtldrPEOpen: %s: VirtualAddress=%#x - beyond image size (%#x) - section #%d '%.*s'!!!\n",
                     pszLogName, pSH->VirtualAddress, cbImage, iSH, sizeof(pSH->Name), pSH->Name));
                return VERR_BAD_EXE_FORMAT;
            }

            if (pSH->VirtualAddress & (pOptHdr->SectionAlignment - 1)) //ASSUMES power of 2 alignment.
            {
                Log(("rtldrPEOpen: %s: VirtualAddress=%#x missaligned (%#x) - section #%d '%.*s'!!!\n",
                     pszLogName, pSH->VirtualAddress, pOptHdr->SectionAlignment, iSH, sizeof(pSH->Name), pSH->Name));
                return VERR_BAD_EXE_FORMAT;
            }

#ifdef PE_FILE_OFFSET_EQUALS_RVA
            /* Our loader code assume rva matches the file offset. */
            if (    pSH->SizeOfRawData
                &&  pSH->PointerToRawData != pSH->VirtualAddress)
            {
                Log(("rtldrPEOpen: %s: ASSUMPTION FAILED: file offset %#x != RVA %#x - section #%d '%.*s'!!!\n",
                     pszLogName, pSH->PointerToRawData, pSH->VirtualAddress, iSH, sizeof(pSH->Name), pSH->Name));
                return VERR_BAD_EXE_FORMAT;
            }
#endif
        }

        ///@todo only if SizeOfRawData > 0 ?
        if (    pSH->PointerToRawData > cbRawImage /// @todo pSH->PointerToRawData >= cbRawImage ?
            ||  pSH->SizeOfRawData > cbRawImage
            ||  pSH->PointerToRawData + pSH->SizeOfRawData > cbRawImage)
        {
            Log(("rtldrPEOpen: %s: PointerToRawData=%#x SizeOfRawData=%#x - beyond end of file (%#x) - section #%d '%.*s'!!!\n",
                 pszLogName, pSH->PointerToRawData, pSH->SizeOfRawData, cbRawImage,
                 iSH, sizeof(pSH->Name), pSH->Name));
            return VERR_BAD_EXE_FORMAT;
        }

        if (pSH->PointerToRawData & (pOptHdr->FileAlignment - 1)) //ASSUMES power of 2 alignment.
        {
            Log(("rtldrPEOpen: %s: PointerToRawData=%#x missaligned (%#x) - section #%d '%.*s'!!!\n",
                 pszLogName, pSH->PointerToRawData, pOptHdr->FileAlignment, iSH, sizeof(pSH->Name), pSH->Name));
            return VERR_BAD_EXE_FORMAT;
        }

        /* ignore the relocations and linenumbers. */

        uRvaPrev = pSH->VirtualAddress + pSH->Misc.VirtualSize;
    }

    /** @todo r=bird: more sanity checks! */
    return VINF_SUCCESS;
}


/**
 * Reads image data by RVA using the section headers.
 *
 * @returns iprt status code.
 * @param   pModPe      The PE module instance.
 * @param   pvBuf       Where to store the bits.
 * @param   cb          Number of bytes to tread.
 * @param   RVA         Where to read from.
 */
static int rtldrPEReadRVA(PRTLDRMODPE pModPe, void *pvBuf, uint32_t cb, uint32_t RVA)
{
    const IMAGE_SECTION_HEADER *pSH = pModPe->paSections;
    PRTLDRREADER                pReader = pModPe->pReader;
    uint32_t                    cbRead;
    int                         rc;

    /*
     * Is it the headers, i.e. prior to the first section.
     */
    if (RVA < pModPe->cbHeaders)
    {
        cbRead = RT_MIN(pModPe->cbHeaders - RVA, cb);
        rc = pReader->pfnRead(pReader, pvBuf, cbRead, RVA);
        if (    cbRead == cb
            ||  RT_FAILURE(rc))
            return rc;
        cb -= cbRead;
        RVA += cbRead;
        pvBuf = (uint8_t *)pvBuf + cbRead;
    }

    /* In the zero space between headers and the first section? */
    if (RVA < pSH->VirtualAddress)
    {
        cbRead = RT_MIN(pSH->VirtualAddress - RVA, cb);
        memset(pvBuf, 0, cbRead);
        if (cbRead == cb)
            return VINF_SUCCESS;
        cb -= cbRead;
        RVA += cbRead;
        pvBuf = (uint8_t *)pvBuf + cbRead;
    }

    /*
     * Iterate the sections.
     */
    for (unsigned cLeft = pModPe->cSections;
         cLeft > 0;
         cLeft--, pSH++)
    {
        uint32_t off = RVA - pSH->VirtualAddress;
        if (off < pSH->Misc.VirtualSize)
        {
            cbRead = RT_MIN(pSH->Misc.VirtualSize - off, cb);
            rc = pReader->pfnRead(pReader, pvBuf, cbRead, pSH->PointerToRawData + off);
            if (    cbRead == cb
                ||  RT_FAILURE(rc))
                return rc;
            cb -= cbRead;
            RVA += cbRead;
            pvBuf = (uint8_t *)pvBuf + cbRead;
        }
        uint32_t RVANext = cLeft ? pSH[1].VirtualAddress : pModPe->cbImage;
        if (RVA < RVANext)
        {
            cbRead = RT_MIN(RVANext - RVA, cb);
            memset(pvBuf, 0, cbRead);
            if (cbRead == cb)
                return VINF_SUCCESS;
            cb -= cbRead;
            RVA += cbRead;
            pvBuf = (uint8_t *)pvBuf + cbRead;
        }
    }

    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Validates the data of some selected data directories entries.
 *
 * This requires a valid section table and thus has to wait
 * till after we've read and validated it.
 *
 * @returns iprt status code.
 * @param   pModPe      The PE module instance.
 * @param   pOptHdr     Pointer to the optional header (valid).
 */
int rtldrPEValidateDirectories(PRTLDRMODPE pModPe, const IMAGE_OPTIONAL_HEADER64 *pOptHdr)
{
    const char *pszLogName = pModPe->pReader->pfnLogName(pModPe->pReader); NOREF(pszLogName);
    union /* combine stuff we're reading to help reduce stack usage. */
    {
        IMAGE_LOAD_CONFIG_DIRECTORY64   Cfg64;
    } u;

    /*
     * The load config entry may include lock prefix tables and whatnot which we don't implement.
     * It does also include a lot of stuff which we can ignore, so we'll have to inspect the
     * actual data before we can make up our mind about it all.
     */
    IMAGE_DATA_DIRECTORY Dir = pOptHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
    if (Dir.Size)
    {
        const size_t cbExpect = pOptHdr->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC
            ? sizeof(IMAGE_LOAD_CONFIG_DIRECTORY32)
            : sizeof(IMAGE_LOAD_CONFIG_DIRECTORY64);
        if (    Dir.Size != cbExpect
            && (    cbExpect == sizeof(IMAGE_LOAD_CONFIG_DIRECTORY32)
                &&  Dir.Size != (uint32_t)RT_OFFSETOF(IMAGE_LOAD_CONFIG_DIRECTORY32, SEHandlerTable))
           )
        {
            Log(("rtldrPEOpen: %s: load cfg dir: unexpected dir size of %d bytes, expected %d.\n",
                 pszLogName, Dir.Size, cbExpect));
            return VERR_LDRPE_LOAD_CONFIG_SIZE;
        }

        /*
         * Read and convert to 64-bit.
         */
        memset(&u.Cfg64, 0, sizeof(u.Cfg64));
        int rc = rtldrPEReadRVA(pModPe, &u.Cfg64, Dir.Size, Dir.VirtualAddress);
        if (RT_FAILURE(rc))
            return rc;
        rtldrPEConvert32BitLoadConfigTo64Bit(&u.Cfg64);

        if (u.Cfg64.Size != cbExpect)
        {
            Log(("rtldrPEOpen: %s: load cfg dir: unexpected header size of %d bytes, expected %d.\n",
                 pszLogName, u.Cfg64.Size, cbExpect));
            return VERR_LDRPE_LOAD_CONFIG_SIZE;
        }
        if (u.Cfg64.LockPrefixTable)
        {
            Log(("rtldrPEOpen: %s: load cfg dir: lock prefix table at %RX64. We don't support lock prefix tables!\n",
                 pszLogName, u.Cfg64.LockPrefixTable));
            return VERR_LDRPE_LOCK_PREFIX_TABLE;
        }
#if 0/* this seems to be safe to ignore. */
        if (    u.Cfg64.SEHandlerTable
            ||  u.Cfg64.SEHandlerCount)
        {
            Log(("rtldrPEOpen: %s: load cfg dir: SEHandlerTable=%RX64 and SEHandlerCount=%RX64 are unsupported!\n",
                 pszLogName, u.Cfg64.SEHandlerTable, u.Cfg64.SEHandlerCount));
            return VERR_BAD_EXE_FORMAT;
        }
#endif
        if (u.Cfg64.EditList)
        {
            Log(("rtldrPEOpen: %s: load cfg dir: EditList=%RX64 is unsupported!\n",
                 pszLogName, u.Cfg64.EditList));
            return VERR_BAD_EXE_FORMAT;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Open a PE image.
 *
 * @returns iprt status code.
 * @param   pReader     The loader reader instance which will provide the raw image bits.
 * @param   offNtHdrs   The offset of the NT headers (where you find "PE\0\0").
 * @param   phLdrMod    Where to store the handle.
 */
int rtldrPEOpen(PRTLDRREADER pReader, RTFOFF offNtHdrs, PRTLDRMOD phLdrMod)
{
    /*
     * Read and validate the file header.
     */
    IMAGE_FILE_HEADER FileHdr;
    int rc = pReader->pfnRead(pReader, &FileHdr, sizeof(FileHdr), offNtHdrs + 4);
    if (RT_FAILURE(rc))
        return rc;
    const char *pszLogName = pReader->pfnLogName(pReader);
    rc = rtldrPEValidateFileHeader(&FileHdr, pszLogName);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read and validate the "optional" header. Convert 32->64 if necessary.
     */
    IMAGE_OPTIONAL_HEADER64 OptHdr;
    rc = pReader->pfnRead(pReader, &OptHdr, FileHdr.SizeOfOptionalHeader, offNtHdrs + 4 + sizeof(IMAGE_FILE_HEADER));
    if (RT_FAILURE(rc))
        return rc;
    if (FileHdr.SizeOfOptionalHeader != sizeof(OptHdr))
        rtldrPEConvert32BitOptionalHeaderTo64Bit(&OptHdr);
    rc = rtldrPEValidateOptionalHeader(&OptHdr, pszLogName, offNtHdrs, &FileHdr, pReader->pfnSize(pReader));
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read and validate section headers.
     */
    const size_t cbSections = sizeof(IMAGE_SECTION_HEADER) * FileHdr.NumberOfSections;
    PIMAGE_SECTION_HEADER paSections = (PIMAGE_SECTION_HEADER)RTMemAlloc(cbSections);
    if (!paSections)
        return VERR_NO_MEMORY;
    rc = pReader->pfnRead(pReader, paSections, cbSections, offNtHdrs + 4 + sizeof(IMAGE_FILE_HEADER) + FileHdr.SizeOfOptionalHeader);
    if (RT_SUCCESS(rc))
    {
        rc = rtldrPEValidateSectionHeaders(paSections, FileHdr.NumberOfSections, pszLogName,
                                           &OptHdr, pReader->pfnSize(pReader));
        if (RT_SUCCESS(rc))
        {
            /*
             * Allocate and initialize the PE module structure.
             */
            PRTLDRMODPE pModPe = (PRTLDRMODPE)RTMemAllocZ(sizeof(*pModPe));
            if (pModPe)
            {
                pModPe->Core.u32Magic = RTLDRMOD_MAGIC;
                pModPe->Core.eState   = LDR_STATE_OPENED;
                if (FileHdr.SizeOfOptionalHeader == sizeof(OptHdr))
                    pModPe->Core.pOps = &s_rtldrPE64Ops.Core;
                else
                    pModPe->Core.pOps = &s_rtldrPE32Ops.Core;
                pModPe->pReader       = pReader;
                pModPe->pvBits        = NULL;
                pModPe->offNtHdrs     = offNtHdrs;
                pModPe->u16Machine    = FileHdr.Machine;
                pModPe->fFile         = FileHdr.Characteristics;
                pModPe->cSections     = FileHdr.NumberOfSections;
                pModPe->paSections    = paSections;
                pModPe->uEntryPointRVA= OptHdr.AddressOfEntryPoint;
                pModPe->uImageBase    = (RTUINTPTR)OptHdr.ImageBase;
                pModPe->cbImage       = OptHdr.SizeOfImage;
                pModPe->cbHeaders     = OptHdr.SizeOfHeaders;
                pModPe->ImportDir     = OptHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
                pModPe->RelocDir      = OptHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
                pModPe->ExportDir     = OptHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

                /*
                 * Perform validation of some selected data directories which requires
                 * inspection of the actual data.
                 */
                rc = rtldrPEValidateDirectories(pModPe, &OptHdr);
                if (RT_SUCCESS(rc))
                {
                    *phLdrMod = &pModPe->Core;
                    return VINF_SUCCESS;
                }
                RTMemFree(pModPe);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    RTMemFree(paSections);
    return rc;
}



