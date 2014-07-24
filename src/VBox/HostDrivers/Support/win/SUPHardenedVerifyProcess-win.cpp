/* $Id$ */
/** @file
 * VirtualBox Support Library/Driver - Hardened Process Verification, Windows.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef IN_RING0
# define IPRT_NT_MAP_TO_ZW
# include <iprt/nt/nt.h>
# include <ntimage.h>
#else
# include <iprt/nt/nt-and-windows.h>
#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/param.h>

#ifdef IN_RING0
# include "SUPDrvInternal.h"
#else
# include "SUPLibInternal.h"
#endif
#include "win/SUPHardenedVerify-win.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Virtual address space region.
 */
typedef struct SUPHNTVPREGION
{
    /** The RVA of the region. */
    uint32_t    uRva;
    /** The size of the region. */
    uint32_t    cb;
    /** The protection of the region. */
    uint32_t    fProt;
} SUPHNTVPREGION;
/** Pointer to a virtual address space region. */
typedef SUPHNTVPREGION *PSUPHNTVPREGION;

/**
 * Virtual address space image information.
 */
typedef struct SUPHNTVPIMAGE
{
    /** The base address of the image. */
    uintptr_t       uImageBase;
    /** The size of the image mapping. */
    uintptr_t       cbImage;

    /** The name from the allowed lists. */
    const char     *pszName;
    /** Name structure for NtQueryVirtualMemory/MemorySectionName. */
    struct
    {
        /** The full unicode name. */
        UNICODE_STRING  UniStr;
        /** Buffer space. */
        WCHAR           awcBuffer[260];
    } Name;

    /** The number of mapping regions. */
    uint32_t        cRegions;
    /** Mapping regions. */
    SUPHNTVPREGION  aRegions[16];

    /** The image characteristics from the FileHeader. */
    uint16_t        fImageCharecteristics;
    /** The DLL characteristics from the OptionalHeader. */
    uint16_t        fDllCharecteristics;

    /** Set if this is the DLL. */
    bool            fDll;
    /** Set if the image is NTDLL an the verficiation code needs to watch out for
     *  the NtCreateSection patch. */
    bool            fNtCreateSectionPatch;
    /** Whether the API set schema hack needs to be applied when verifying memory
     * content.  The hack means that we only check if the 1st section is mapped. */
    bool            fApiSetSchemaOnlySection1;
    /** This may be a 32-bit resource DLL. */
    bool            f32bitResourceDll;
} SUPHNTVPIMAGE;
/** Pointer to image info from the virtual address space scan. */
typedef SUPHNTVPIMAGE *PSUPHNTVPIMAGE;

/**
 * Virtual address space scanning state.
 */
typedef struct SUPHNTVPSTATE
{
    /** The result. */
    int                     rcResult;
    /** Number of images in aImages. */
    uint32_t                cImages;
    /** Images found in the process.
     * The array is large enough to hold the executable, all allowed DLLs, and one
     * more so we can get the image name of the first unwanted DLL. */
    SUPHNTVPIMAGE           aImages[1 + 6 + 1
#ifdef VBOX_PERMIT_MORE
                                    + 5
#endif
#ifdef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
                                    + 16
#endif
                                   ];
    /** Memory compare scratch buffer.*/
    uint8_t                 abMemory[_4K];
    /** File compare scratch buffer.*/
    uint8_t                 abFile[_4K];
    /** Section headers for use when comparing file and loaded image. */
    IMAGE_SECTION_HEADER    aSecHdrs[16];
    /** Pointer to the error info. */
    PRTERRINFO              pErrInfo;
} SUPHNTVPSTATE;
/** Pointer to stat information of a virtual address space scan. */
typedef SUPHNTVPSTATE *PSUPHNTVPSTATE;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * System DLLs allowed to be loaded into the process.
 * @remarks supHardNtVpCheckDlls assumes these are lower case.
 */
static const char *g_apszSupNtVpAllowedDlls[] =
{
    "ntdll.dll",
    "kernel32.dll",
    "kernelbase.dll",
    "apphelp.dll",
    "apisetschema.dll",
#ifdef VBOX_PERMIT_MORE
# define VBOX_PERMIT_MORE_FIRST_IDX 5
    "sfc.dll",
    "sfc_os.dll",
    "user32.dll",
    "acres.dll",
    "acgenral.dll",
#endif
#ifdef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
    "psapi.dll",
    "msvcrt.dll",
    "advapi32.dll",
    "sechost.dll",
    "rpcrt4.dll",
    "SamplingRuntime.dll",
#endif
};

/**
 * VBox executables allowed to start VMs.
 * @remarks Remember to keep in sync with SUPR3HardenedVerify.cpp.
 */
static const char *g_apszSupNtVpAllowedVmExes[] =
{
    "VBoxHeadless.exe",
    "VirtualBox.exe",
    "VBoxSDL.exe",
    "VBoxNetDHCP.exe",
    "VBoxNetNAT.exe",

    "tstMicro.exe",
    "tstPDMAsyncCompletion.exe",
    "tstPDMAsyncCompletionStress.exe",
    "tstVMM.exe",
    "tstVMREQ.exe",
    "tstCFGM.exe",
    "tstIntNet-1.exe",
    "tstMMHyperHeap.exe",
    "tstR0ThreadPreemptionDriver.exe",
    "tstRTR0MemUserKernelDriver.exe",
    "tstRTR0SemMutexDriver.exe",
    "tstRTR0TimerDriver.exe",
    "tstSSM.exe",
};

/** Pointer to NtQueryVirtualMemory.  Initialized by SUPDrv-win.cpp in
 *  ring-0, in ring-3 it's just a slightly confusing define. */
#ifdef IN_RING0
PFNNTQUERYVIRTUALMEMORY g_pfnNtQueryVirtualMemory = NULL;
#else
# define g_pfnNtQueryVirtualMemory NtQueryVirtualMemory
#endif


/**
 * Fills in error information.
 *
 * @returns @a rc.
 * @param   pErrInfo            Pointer to the extended error info structure.
 *                              Can be NULL.
 * @param   pszErr              Where to return error details.
 * @param   cbErr               Size of the buffer @a pszErr points to.
 * @param   rc                  The status to return.
 * @param   pszMsg              The format string for the message.
 * @param   ...                 The arguments for the format string.
 */
static int supHardNtVpSetInfo1(PRTERRINFO pErrInfo, int rc, const char *pszMsg, ...)
{
    va_list va;
#ifdef IN_RING3
    va_start(va, pszMsg);
    supR3HardenedError(rc, false /*fFatal*/, "%N\n", pszMsg, &va);
    va_end(va);
#endif

    va_start(va, pszMsg);
    RTErrInfoSetV(pErrInfo, rc, pszMsg, va);
    va_end(va);

    return rc;
}


/**
 * Fills in error information.
 *
 * @returns @a rc.
 * @param   pThis               The process validator instance.
 * @param   pszErr              Where to return error details.
 * @param   cbErr               Size of the buffer @a pszErr points to.
 * @param   rc                  The status to return.
 * @param   pszMsg              The format string for the message.
 * @param   ...                 The arguments for the format string.
 */
static int supHardNtVpSetInfo2(PSUPHNTVPSTATE pThis, int rc, const char *pszMsg, ...)
{
    va_list va;
#ifdef IN_RING3
    va_start(va, pszMsg);
    supR3HardenedError(rc, false /*fFatal*/, "%N\n", pszMsg, &va);
    va_end(va);
#endif

    va_start(va, pszMsg);
#ifdef IN_RING0
    RTErrInfoSetV(pThis->pErrInfo, rc, pszMsg, va);
    pThis->rcResult = rc;
#else
    if (RT_SUCCESS(pThis->rcResult))
    {
        RTErrInfoSetV(pThis->pErrInfo, rc, pszMsg, va);
        pThis->rcResult = rc;
    }
    else
    {
        RTErrInfoAddF(pThis->pErrInfo, rc, " \n[rc=%d] ", rc);
        RTErrInfoAddV(pThis->pErrInfo, rc, pszMsg, va);
    }
#endif
    va_end(va);

    return pThis->rcResult;
}


static NTSTATUS supHardNtVpReadFile(HANDLE hFile, uint64_t off, void *pvBuf, size_t cbRead)
{
    if ((ULONG)cbRead != cbRead)
        return STATUS_INTEGER_OVERFLOW;

    IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    NTSTATUS rcNt = NtReadFile(hFile,
                               NULL /*hEvent*/,
                               NULL /*ApcRoutine*/,
                               NULL /*ApcContext*/,
                               &Ios,
                               pvBuf,
                               (ULONG)cbRead,
                               (PLARGE_INTEGER)&off,
                               NULL);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    return rcNt;
}


static NTSTATUS supHardNtVpReadMem(HANDLE hProcess, uintptr_t uPtr, void *pvBuf, size_t cbRead)
{
#ifdef IN_RING0
    /* ASSUMES hProcess is the current process. */
    /** @todo use MmCopyVirtualMemory where available! */
    int rc = RTR0MemUserCopyFrom(pvBuf, uPtr, cbRead);
    if (RT_SUCCESS(rc))
        return STATUS_SUCCESS;
    return STATUS_ACCESS_DENIED;
#else
    SIZE_T cbIgn;
    NTSTATUS rcNt = NtReadVirtualMemory(hProcess, (PVOID)uPtr, pvBuf, cbRead, &cbIgn);
    if (NT_SUCCESS(rcNt) && cbIgn != cbRead)
        rcNt = STATUS_IO_DEVICE_ERROR;
    return rcNt;
#endif
}


static int supHardNtVpFileMemCompare(PSUPHNTVPSTATE pThis, const void *pvFile, const void *pvMemory, size_t cbToCompare,
                                     PSUPHNTVPIMAGE pImage, uint32_t uRva)
{
    if (suplibHardenedMemComp(pvFile, pvMemory, cbToCompare) == 0)
        return VINF_SUCCESS;

    /* Find the exact location. */
    const uint8_t *pbFile   = (const uint8_t *)pvFile;
    const uint8_t *pbMemory = (const uint8_t *)pvMemory;
    while (cbToCompare > 0 && *pbFile == *pbMemory)
    {
        cbToCompare--;
        pbFile++;
        pbMemory++;
        uRva++;
    }

    return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_MEMORY_VS_FILE_MISMATCH,
                               "%s: memory compare at %#x failed: %#x != %#x\n", pImage->pszName, uRva, *pbFile, *pbMemory);
}


static int supHardNtVpCheckSectionProtection(PSUPHNTVPSTATE pThis, PSUPHNTVPIMAGE pImage,
                                             uint32_t uRva, uint32_t cb, uint32_t fProt)
{
    uint32_t const cbOrg = cb;
    if (!cb)
        return VINF_SUCCESS;

    for (uint32_t i = 0; i < pImage->cRegions; i++)
    {
        uint32_t offRegion = uRva - pImage->aRegions[i].uRva;
        if (offRegion < pImage->aRegions[i].cb)
        {
            uint32_t cbLeft = pImage->aRegions[i].cb - offRegion;
            if (   pImage->aRegions[i].fProt != fProt
                && (   fProt != PAGE_READWRITE
                    || pImage->aRegions[i].fProt != PAGE_WRITECOPY))
                return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_SECTION_PROTECTION_MISMATCH,
                                           "%s: RVA range %#x-%#x protection is %#x, expected %#x. (cb=%#x)",
                                           pImage->pszName, uRva, uRva + cbLeft - 1, pImage->aRegions[i].fProt, fProt, cb);
            if (cbLeft >= cb)
                return VINF_SUCCESS;
            cb   -= cbLeft;
            uRva += cbLeft;

#if 0 /* This shouldn't ever be necessary. */
            if (   i + 1 < pImage->cRegions
                && uRva < pImage->aRegions[i + 1].uRva)
            {
                cbLeft = pImage->aRegions[i + 1].uRva - uRva;
                if (cbLeft >= cb)
                    return VINF_SUCCESS;
                cb   -= cbLeft;
                uRva += cbLeft;
            }
#endif
        }
    }

    return supHardNtVpSetInfo2(pThis, cbOrg == cb ? VERR_SUP_VP_SECTION_NOT_MAPPED : VERR_SUP_VP_SECTION_NOT_FULLY_MAPPED,
                               "%s: RVA range %#x-%#x is not mapped?", pImage->pszName, uRva, uRva + cb - 1);
}


/**
 * Compares process memory with the disk content.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure (for the
 *                              two scratch buffers).
 * @param   pImage              The image data collected during the address
 *                              space scan.
 * @param   hProcess            Handle to the process.
 * @param   hFile               Handle to the image file.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
static int supHardNtVpVerifyImageCompareMemory(PSUPHNTVPSTATE pThis, PSUPHNTVPIMAGE pImage, HANDLE hProcess, HANDLE hFile,
                                               PRTERRINFO pErrInfo)
{
    /*
     * Read and find the file headers.
     */
    NTSTATUS rcNt = supHardNtVpReadFile(hFile, 0, pThis->abFile, sizeof(pThis->abFile));
    if (!NT_SUCCESS(rcNt))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_IMAGE_HDR_READ_ERROR,
                                   "%s: Error reading image header: %#x", pImage->pszName, rcNt);

    uint32_t offNtHdrs = 0;
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)&pThis->abFile[0];
    if (pDosHdr->e_magic == IMAGE_DOS_SIGNATURE)
    {
        offNtHdrs = pDosHdr->e_lfanew;
        if (offNtHdrs > 512 || offNtHdrs < sizeof(*pDosHdr))
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_MZ_OFFSET,
                                       "%s: Unexpected e_lfanew value: %#x", pImage->pszName, offNtHdrs);
    }
    PIMAGE_NT_HEADERS   pNtHdrs   = (PIMAGE_NT_HEADERS)&pThis->abFile[offNtHdrs];
    PIMAGE_NT_HEADERS32 pNtHdrs32 = (PIMAGE_NT_HEADERS32)pNtHdrs;
    if (pNtHdrs->Signature != IMAGE_NT_SIGNATURE)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_IMAGE_SIGNATURE,
                                   "%s: No PE signature at %#x: %#x", pImage->pszName, offNtHdrs, pNtHdrs->Signature);

    /*
     * Do basic header validation.
     */
#ifdef RT_ARCH_AMD64
    if (pNtHdrs->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 && !pImage->f32bitResourceDll)
#else
    if (pNtHdrs->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
#endif
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_UNEXPECTED_IMAGE_MACHINE,
                                   "%s: Unexpected machine: %#x", pImage->pszName, pNtHdrs->FileHeader.Machine);
    bool const fIs32Bit = pNtHdrs->FileHeader.Machine == IMAGE_FILE_MACHINE_I386;

    if (pNtHdrs->FileHeader.SizeOfOptionalHeader != (fIs32Bit ? sizeof(IMAGE_OPTIONAL_HEADER32) : sizeof(IMAGE_OPTIONAL_HEADER64)))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_OPTIONAL_HEADER,
                                   "%s: Unexpected optional header size: %#x",
                                   pImage->pszName, pNtHdrs->FileHeader.SizeOfOptionalHeader);

    if (pNtHdrs->OptionalHeader.Magic != (fIs32Bit ? IMAGE_NT_OPTIONAL_HDR32_MAGIC : IMAGE_NT_OPTIONAL_HDR64_MAGIC))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_OPTIONAL_HEADER,
                                   "%s: Unexpected optional header magic: %#x", pImage->pszName, pNtHdrs->OptionalHeader.Magic);

    uint32_t cDirs = (fIs32Bit ? pNtHdrs32->OptionalHeader.NumberOfRvaAndSizes : pNtHdrs->OptionalHeader.NumberOfRvaAndSizes);
    if (cDirs != IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_OPTIONAL_HEADER,
                                   "%s: Unexpected data dirs: %#x", pImage->pszName, cDirs);

    /*
     * Before we start comparing things, store what we need to know from the headers.
     */
    uint32_t  const cSections  = pNtHdrs->FileHeader.NumberOfSections;
    if (cSections > RT_ELEMENTS(pThis->aSecHdrs))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_MANY_SECTIONS,
                                   "%s: Too many section headers: %#x", pImage->pszName, cSections);
    suplibHardenedMemCopy(pThis->aSecHdrs, (fIs32Bit ? (void *)(pNtHdrs32 + 1) : (void *)(pNtHdrs + 1)),
                          cSections * sizeof(IMAGE_SECTION_HEADER));

    uintptr_t const uImageBase = fIs32Bit ? pNtHdrs32->OptionalHeader.ImageBase : pNtHdrs->OptionalHeader.ImageBase;
    if (uImageBase & PAGE_OFFSET_MASK)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_IMAGE_BASE,
                                   "%s: Invalid image base: %p", pImage->pszName, uImageBase);

    uint32_t  const cbImage    = fIs32Bit ? pNtHdrs32->OptionalHeader.SizeOfImage : pNtHdrs->OptionalHeader.SizeOfImage;
    if (RT_ALIGN_32(pImage->cbImage, PAGE_SIZE) != RT_ALIGN_32(cbImage, PAGE_SIZE) && !pImage->fApiSetSchemaOnlySection1)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_IMAGE_SIZE,
                                   "%s: SizeOfImage (%#x) isn't close enough to the mapping size (%#x)",
                                   pImage->pszName, cbImage, pImage->cbImage);

    uint32_t const cbSectAlign = fIs32Bit ? pNtHdrs32->OptionalHeader.SectionAlignment : pNtHdrs->OptionalHeader.SectionAlignment;
    if (   !RT_IS_POWER_OF_TWO(cbSectAlign)
        || cbSectAlign < PAGE_SIZE
        || cbSectAlign > (pImage->fApiSetSchemaOnlySection1 ? _64K : (uint32_t)PAGE_SIZE) )
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SECTION_ALIGNMENT_VALUE,
                                   "%s: Unexpected SectionAlignment value: %#x", pImage->pszName, cbSectAlign);

    uint32_t const cbFileAlign = fIs32Bit ? pNtHdrs32->OptionalHeader.FileAlignment : pNtHdrs->OptionalHeader.FileAlignment;
    if (!RT_IS_POWER_OF_TWO(cbFileAlign) || cbFileAlign < 512 || cbFileAlign > PAGE_SIZE || cbFileAlign > cbSectAlign)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_FILE_ALIGNMENT_VALUE,
                                   "%s: Unexpected FileAlignment value: %#x (cbSectAlign=%#x)",
                                   pImage->pszName, cbFileAlign, cbSectAlign);

    uint32_t  const cbHeaders  = fIs32Bit ? pNtHdrs32->OptionalHeader.SizeOfHeaders : pNtHdrs->OptionalHeader.SizeOfHeaders;
    uint32_t  const cbMinHdrs  = offNtHdrs + (fIs32Bit ? sizeof(*pNtHdrs32) : sizeof(*pNtHdrs) )
                               + sizeof(IMAGE_SECTION_HEADER) * cSections;
    if (cbHeaders < cbMinHdrs)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SIZE_OF_HEADERS,
                                   "%s: Headers are too small: %#x < %#x (cSections=%#x)",
                                   pImage->pszName, cbHeaders, cbMinHdrs, cSections);
    uint32_t  const cbHdrsFile = RT_ALIGN_32(cbHeaders, cbFileAlign);
    if (cbHdrsFile > sizeof(pThis->abFile))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SIZE_OF_HEADERS,
                                   "%s: Headers are larger than expected: %#x/%#x (expected max %zx)",
                                   pImage->pszName, cbHeaders, cbHdrsFile, sizeof(pThis->abFile));

    /*
     * Compare the file header with the loaded bits.  The loader will fiddle
     * with image base, changing it to the actual load address.
     */
    int rc;
    if (!pImage->fApiSetSchemaOnlySection1)
    {
        rcNt = supHardNtVpReadMem(hProcess, pImage->uImageBase, pThis->abMemory, cbHdrsFile);
        if (!NT_SUCCESS(rcNt))
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_MEMORY_READ_ERROR,
                                       "%s: Error reading image header from memory: %#x", pImage->pszName, rcNt);
        if (uImageBase != pImage->uImageBase)
        {
            if (fIs32Bit)
                pNtHdrs32->OptionalHeader.ImageBase = (uint32_t)pImage->uImageBase;
            else
                pNtHdrs->OptionalHeader.ImageBase = pImage->uImageBase;
        }

        rc = supHardNtVpFileMemCompare(pThis, pThis->abFile, pThis->abMemory, cbHeaders, pImage, 0 /*uRva*/);
        if (RT_FAILURE(rc))
            return rc;
        rc = supHardNtVpCheckSectionProtection(pThis, pImage, 0 /*uRva*/, cbHdrsFile, PAGE_READONLY);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Save some header fields we might be using later on.
     */
    pImage->fImageCharecteristics = pNtHdrs->FileHeader.Characteristics;
    pImage->fDllCharecteristics   = fIs32Bit ? pNtHdrs32->OptionalHeader.DllCharacteristics : pNtHdrs->OptionalHeader.DllCharacteristics;

    /*
     * Validate sections and check them against the mapping regions.
     */
    uint32_t uRva = cbHdrsFile;
    for (uint32_t i = 0; i < cSections; i++)
    {
        /* Validate the section. */
        uint32_t uSectRva = pThis->aSecHdrs[i].VirtualAddress;
        if (uSectRva < uRva || uSectRva > cbImage || RT_ALIGN_32(uSectRva, cbSectAlign) != uSectRva)
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SECTION_RVA,
                                       "%s: Section %u: Invalid virtual address: %#x (uRva=%#x, cbImage=%#x, cbSectAlign=%#x)",
                                       pImage->pszName, i, uSectRva, uRva, cbImage, cbSectAlign);
        uint32_t cbMap  = pThis->aSecHdrs[i].Misc.VirtualSize;
        if (cbMap > cbImage || uRva + cbMap > cbImage)
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SECTION_VIRTUAL_SIZE,
                                       "%s: Section %u: Invalid virtual size: %#x (uSectRva=%#x, uRva=%#x, cbImage=%#x)",
                                       pImage->pszName, i, cbMap, uSectRva, uRva, cbImage);
        uint32_t cbFile = pThis->aSecHdrs[i].SizeOfRawData;
        if (cbFile != RT_ALIGN_32(cbFile, cbFileAlign) || cbFile > RT_ALIGN_32(cbMap, cbSectAlign))
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_BAD_SECTION_FILE_SIZE,
                                       "%s: Section %u: Invalid file size: %#x (cbMap=%#x, uSectRva=%#x)",
                                       pImage->pszName, i, cbFile, cbMap, uSectRva);

        /* Validate the protection. */
        if (!pImage->fApiSetSchemaOnlySection1 || i == 0)
        {
            if (pImage->fApiSetSchemaOnlySection1)
            {
                pImage->uImageBase -= uSectRva;
                pImage->cbImage    += uSectRva;
                pImage->aRegions[i].uRva = uSectRva;
            }

            uint32_t fProt;
            switch (pThis->aSecHdrs[i].Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE))
            {
                case IMAGE_SCN_MEM_READ:
                    fProt = PAGE_READONLY;
                    break;
                case IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE:
                    fProt = PAGE_READWRITE;
                    if (!suplibHardenedMemComp(pThis->aSecHdrs[i].Name, ".mrdata", 8)) /* w8.1, ntdll */
                        fProt = PAGE_READONLY;
                    break;
                case IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE:
                    fProt = PAGE_EXECUTE_READ;
                    break;
                case IMAGE_SCN_MEM_EXECUTE:
                    fProt = PAGE_EXECUTE;
                    break;
                default:
                    return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_UNEXPECTED_SECTION_FLAGS,
                                               "%s: Section %u: Unexpected characteristics: %#x (uSectRva=%#x, cbMap=%#x)",
                                               pImage->pszName, i, pThis->aSecHdrs[i].Characteristics, uSectRva, cbMap);

            }
            rc = supHardNtVpCheckSectionProtection(pThis, pImage, uSectRva, RT_ALIGN_32(cbMap, PAGE_SIZE), fProt);
            if (RT_FAILURE(rc))
                return rc;
        }

        /* Advance the RVA. */
        uRva = uSectRva + RT_ALIGN_32(cbMap, cbSectAlign);
    }

    /*
     * Check the mapping regions with the image to make sure someone didn't
     * fill executable code into some gap in the image.
     */
    /** @todo not vital. */


    /*
     * Compare executable code.  If we're not loaded at the link address, we
     * need to load base relocations and apply them while making the compare.
     * A special case
     */
    /** @todo not vital. */


    return VINF_SUCCESS;
}


/**
 * Verifies the signature of the given image on disk, then checks if the memory
 * mapping matches what we verified.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure (for the
 *                              two scratch buffers).
 * @param   pImage              The image data collected during the address
 *                              space scan.
 * @param   hProcess            Handle to the process.
 * @param   hFile               Handle to the image file.
 */
static int supHardNtVpVerifyImage(PSUPHNTVPSTATE pThis, PSUPHNTVPIMAGE pImage, HANDLE hProcess)
{
    /*
     * Open the image.
     */
    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

    OBJECT_ATTRIBUTES   ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &pImage->Name.UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
#ifdef IN_RING0
    ObjAttr.Attributes |= OBJ_KERNEL_HANDLE;
#endif

    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 GENERIC_READ,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /* Allocation Size*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ,
                                 FILE_OPEN,
                                 FILE_NON_DIRECTORY_FILE,
                                 NULL /*EaBuffer*/,
                                 0 /*EaLength*/);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    if (!NT_SUCCESS(rcNt))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_IMAGE_FILE_OPEN_ERROR,
                                  "Error opening image for scanning: %#x (name %ls)", rcNt, pImage->Name.UniStr.Buffer);

    /*
     * Validate the signature, then make an attempt at comparing memory and
     * disk content.
     */
    uint32_t fFlags = pImage->fDll ? 0 : SUPHNTVI_F_REQUIRE_BUILD_CERT;
    if (pImage->f32bitResourceDll)
        fFlags |= SUPHNTVI_F_RESOURCE_IMAGE;
    int rc = supHardenedWinVerifyImageByHandle(hFile, pImage->Name.UniStr.Buffer, fFlags, NULL /*pfCacheable*/, pThis->pErrInfo);
    if (RT_SUCCESS(rc))
        rc = supHardNtVpVerifyImageCompareMemory(pThis, pImage, hProcess, hFile, pThis->pErrInfo);

    /*
     * Clean up and return.
     */
    rcNt = NtClose(hFile);
    if (!NT_SUCCESS(rcNt) && RT_SUCCESS(rc))
        rc = supHardNtVpSetInfo2(pThis, VERR_SUP_VP_IMAGE_FILE_CLOSE_ERROR,
                                "Error closing image after scanning: %#x (name %ls)", rcNt, pImage->Name.UniStr.Buffer);
    return rc;
}


/**
 * Verifies that there is only one thread in the process.
 *
 * @returns VBox status code.
 * @param   hProcess            The process.
 * @param   hThread             The thread.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
static int supHardNtVpThread(HANDLE hProcess, HANDLE hThread, PRTERRINFO pErrInfo)
{
    /*
     * Use the ThreadAmILastThread request to check that there is only one
     * thread in the process.
     */
    ULONG cbIgn = 0;
    ULONG fAmI  = 0;
    NTSTATUS rcNt = NtQueryInformationThread(hThread, ThreadAmILastThread, &fAmI, sizeof(fAmI), &cbIgn);
    if (!NT_SUCCESS(rcNt))
        return supHardNtVpSetInfo1(pErrInfo, VERR_SUP_VP_NT_QI_THREAD_ERROR,
                                   "NtQueryInformationThread/ThreadAmILastThread -> %#x", rcNt);
    if (!fAmI)
        return supHardNtVpSetInfo1(pErrInfo, VERR_SUP_VP_THREAD_NOT_ALONE,
                                   "More than one thread in process");

    /** @todo Would be nice to verify the relation ship between hProcess and hThread
     *        as well... */
    return VINF_SUCCESS;
}


#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
/**
 * Verifies that there isn't a debugger attached to the process.
 *
 * @returns VBox status code.
 * @param   hProcess            The process.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
static int supHardNtVpDebugger(HANDLE hProcess, PRTERRINFO pErrInfo)
{
    /*
     * Use the ProcessDebugPort request to check there is no debugger
     * currently attached to the process.
     */
    ULONG     cbIgn = 0;
    uintptr_t uPtr  = ~(uintptr_t)0;
    NTSTATUS rcNt = NtQueryInformationProcess(hProcess,
                                              ProcessDebugPort,
                                              &uPtr, sizeof(uPtr), &cbIgn);
    if (!NT_SUCCESS(rcNt))
        return supHardNtVpSetInfo1(pErrInfo, VERR_SUP_VP_NT_QI_PROCESS_DBG_PORT_ERROR,
                                   "NtQueryInformationProcess/ProcessDebugPort -> %#x", rcNt);
    if (uPtr != 0)
        return supHardNtVpSetInfo1(pErrInfo, VERR_SUP_VP_DEBUGGED,
                                   "Debugger attached (%#zx)", uPtr);
    return VINF_SUCCESS;
}
#endif /* !VBOX_WITHOUT_DEBUGGER_CHECKS */


/**
 * Allocates and initalizes a process stat structure for process virtual memory
 * scanning.
 *
 * @returns Pointer to the state structure on success, NULL on failure.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
static PSUPHNTVPSTATE supHardNtVpCreateState(PRTERRINFO pErrInfo)
{
    /*
     * Allocate the memory.
     */
    PSUPHNTVPSTATE pThis = (PSUPHNTVPSTATE)suplibHardenedAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->rcResult = VINF_SUCCESS;
        pThis->pErrInfo = pErrInfo;
        return pThis;
    }
    supHardNtVpSetInfo1(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %zu bytes for state structures.", sizeof(*pThis));
    return NULL;
}


/**
 * Matches two UNICODE_STRING structures in a case sensitive fashion.
 *
 * @returns true if equal, false if not.
 * @param   pUniStr1            The first unicode string.
 * @param   pUniStr2            The first unicode string.
 */
static bool supHardNtVpAreUniStringsEqual(PCUNICODE_STRING pUniStr1, PCUNICODE_STRING pUniStr2)
{
    if (pUniStr1->Length != pUniStr2->Length)
        return false;
    return suplibHardenedMemComp(pUniStr1->Buffer, pUniStr2->Buffer, pUniStr1->Length) == 0;
}


/**
 * Performs a case insensitive comparison of an ASCII and an UTF-16 file name.
 *
 * @returns true / false
 * @param   pszName1            The ASCII name.
 * @param   pwszName2           The UTF-16 name.
 */
static bool supHardNtVpAreNamesEqual(const char *pszName1, PCRTUTF16 pwszName2)
{
    for (;;)
    {
        char    ch1 = *pszName1++;
        RTUTF16 wc2 = *pwszName2++;
        if (ch1 != wc2)
        {
            ch1 = RT_C_TO_LOWER(ch1);
            wc2 = wc2 < 0x80 ? RT_C_TO_LOWER(wc2) : wc2;
            if (ch1 != wc2)
                return false;
        }
        if (!ch1)
            return true;
    }
}


/**
 * Records an additional memory region for an image.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure.
 * @param   pImage              The new image structure.  Only the unicode name
 *                              buffer is valid.
 * @param   pMemInfo            The memory information for the image.
 */
static int supHardNtVpNewImage(PSUPHNTVPSTATE pThis, PSUPHNTVPIMAGE pImage, PMEMORY_BASIC_INFORMATION pMemInfo)
{
    /*
     * Extract the final component.
     */
    unsigned  cwcDirName   = pImage->Name.UniStr.Length / sizeof(WCHAR);
    PCRTUTF16 pwszFilename = &pImage->Name.UniStr.Buffer[cwcDirName];
    while (   cwcDirName > 0
           && pwszFilename[-1] != '\\'
           && pwszFilename[-1] != '/'
           && pwszFilename[-1] != ':')
    {
        pwszFilename--;
        cwcDirName--;
    }
    if (!*pwszFilename)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NO_IMAGE_MAPPING_NAME,
                                   "Empty filename (len=%u) for image at %p.", pImage->Name.UniStr.Length, pMemInfo->BaseAddress);

    /*
     * Drop trailing slashes from the directory name.
     */
    while (   cwcDirName > 0
           && (   pImage->Name.UniStr.Buffer[cwcDirName - 1] == '\\'
               || pImage->Name.UniStr.Buffer[cwcDirName - 1] == '/'))
        cwcDirName--;

    /*
     * Match it against known DLLs.
     */
    pImage->pszName = NULL;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_apszSupNtVpAllowedDlls); i++)
        if (supHardNtVpAreNamesEqual(g_apszSupNtVpAllowedDlls[i], pwszFilename))
        {
            pImage->pszName = g_apszSupNtVpAllowedDlls[i];
            pImage->fDll    = true;

#ifndef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
            /* The directory name must match the one we've got for System32. */
            if (   (   cwcDirName * sizeof(WCHAR) != g_System32NtPath.UniStr.Length
                    || suplibHardenedMemComp(pImage->Name.UniStr.Buffer,
                                            g_System32NtPath.UniStr.Buffer,
                                            cwcDirName * sizeof(WCHAR)) )
# ifdef VBOX_PERMIT_MORE
                && (   pImage->pszName[0] != 'a'
                    || pImage->pszName[1] != 'c'
                    || !supHardViIsAppPatchDir(pImage->Name.UniStr.Buffer, pImage->Name.UniStr.Length / sizeof(WCHAR)) )
# endif
                )
                return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NON_SYSTEM32_DLL,
                                           "Expected %ls to be loaded from %ls.",
                                           pImage->Name.UniStr.Buffer, g_System32NtPath.UniStr.Buffer);
# ifdef VBOX_PERMIT_MORE
            if (g_uNtVerCombined < SUP_NT_VER_W70 && i >= VBOX_PERMIT_MORE_FIRST_IDX)
                pImage->pszName = NULL; /* hard limit: user32.dll is unwanted prior to w7. */
# endif

#endif /* VBOX_PERMIT_VISUAL_STUDIO_PROFILING */
            break;
        }
    if (!pImage->pszName)
    {
        /*
         * Not a known DLL, executable?
         */
        for (uint32_t i = 0; i < RT_ELEMENTS(g_apszSupNtVpAllowedVmExes); i++)
            if (supHardNtVpAreNamesEqual(g_apszSupNtVpAllowedVmExes[i], pwszFilename))
            {
                pImage->pszName = g_apszSupNtVpAllowedVmExes[i];
                pImage->fDll    = false;
                break;
            }
    }
    if (!pImage->pszName)
    {
        if (   pMemInfo->AllocationBase == pMemInfo->BaseAddress
            && (   supHardNtVpAreNamesEqual("sysfer.dll", pwszFilename)
                || supHardNtVpAreNamesEqual("sysfer32.dll", pwszFilename)
                || supHardNtVpAreNamesEqual("sysfer64.dll", pwszFilename)) )
        {
            supHardNtVpSetInfo2(pThis, VERR_SUP_VP_SYSFER_DLL,
                                "Found %ls at %p - This is probably part of Symantec Endpoint Protection. \n"
                                "You or your admin need to add and exception to the Application and Device Control (ADC) "
                                "component (or disable it) to prevent ADC from injecting itself into the VirtualBox VM processes. "
                                "See http://www.symantec.com/connect/articles/creating-application-control-exclusions-symantec-endpoint-protection-121"
                                , pImage->Name.UniStr.Buffer, pMemInfo->BaseAddress);
            return pThis->rcResult = VERR_SUP_VP_SYSFER_DLL; /* Try make sure this is what the user sees first! */
        }
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NOT_KNOWN_DLL_OR_EXE,
                                   "Unknown image file %ls at %p.", pImage->Name.UniStr.Buffer, pMemInfo->BaseAddress);
    }

    /*
     * Since it's a new image, we expect to be at the start of the mapping now.
     */
    if (pMemInfo->AllocationBase != pMemInfo->BaseAddress)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_IMAGE_MAPPING_BASE_ERROR,
                                   "Invalid AllocationBase/BaseAddress for %s: %p vs %p.",
                                   pImage->pszName, pMemInfo->AllocationBase, pMemInfo->BaseAddress);

    /*
     * Check for size/rva overflow.
     */
    if (pMemInfo->RegionSize >= _2G)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_LARGE_REGION,
                                   "Region 0 of image %s is too large: %p.", pImage->pszName, pMemInfo->RegionSize);

    /*
     * Fill in details from the memory info.
     */
    pImage->uImageBase = (uintptr_t)pMemInfo->AllocationBase;
    pImage->cbImage    = pMemInfo->RegionSize;
    pImage->cRegions   = 1;
    pImage->aRegions[0].uRva    = 0;
    pImage->aRegions[0].cb      = (uint32_t)pMemInfo->RegionSize;
    pImage->aRegions[0].fProt   = pMemInfo->Protect;

    return VINF_SUCCESS;
}


/**
 * Records an additional memory region for an image.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure.
 * @param   pImage              The image.
 * @param   pMemInfo            The memory information for the region.
 */
static int supHardNtVpAddRegion(PSUPHNTVPSTATE pThis, PSUPHNTVPIMAGE pImage, PMEMORY_BASIC_INFORMATION pMemInfo)
{
    /*
     * Make sure the base address matches.
     */
    if (pImage->uImageBase != (uintptr_t)pMemInfo->AllocationBase)
        return supHardNtVpSetInfo2(pThis, VERR_SUPLIB_NT_PROCESS_UNTRUSTED_3,
                                   "Base address mismatch for %s: have %p, found %p for region %p LB %#zx.",
                                   pImage->pszName, pImage->uImageBase, pMemInfo->AllocationBase,
                                   pMemInfo->BaseAddress, pMemInfo->RegionSize);

    /*
     * Check for size and rva overflows.
     */
    uintptr_t uRva = (uintptr_t)pMemInfo->BaseAddress - pImage->uImageBase;
    if (pMemInfo->RegionSize >= _2G)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_LARGE_REGION,
                                   "Region %u of image %s is too large: %p/%p.", pImage->pszName, pMemInfo->RegionSize, uRva);
    if (uRva >= _2G)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_HIGH_REGION_RVA,
                                   "Region %u of image %s is too high: %p/%p.", pImage->pszName, pMemInfo->RegionSize, uRva);


    /*
     * Record the region.
     */
    uint32_t iRegion = pImage->cRegions;
    if (iRegion + 1 >= RT_ELEMENTS(pImage->aRegions))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_MANY_IMAGE_REGIONS,
                                   "Too many regions for %s.", pImage->pszName);
    pImage->aRegions[iRegion].uRva  = (uint32_t)uRva;
    pImage->aRegions[iRegion].cb    = (uint32_t)pMemInfo->RegionSize;
    pImage->aRegions[iRegion].fProt = pMemInfo->Protect;
    pImage->cbImage = pImage->aRegions[iRegion].uRva + pImage->aRegions[iRegion].cb;
    pImage->cRegions++;

    return VINF_SUCCESS;
}


/**
 * Scans the virtual memory of the process.
 *
 * This collects the locations of DLLs and the EXE, and verifies that executable
 * memory is only associated with these.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure. Details
 *                              about images are added to this.
 * @param   hProcess            The process to verify.
 */
static int supHardNtVpScanVirtualMemory(PSUPHNTVPSTATE pThis, HANDLE hProcess)
{
    SUP_DPRINTF(("supHardNtVpScanVirtualMemory:\n"));

    uint32_t    cXpExceptions = 0;
    uintptr_t   cbAdvance = 0;
    uintptr_t   uPtrWhere = 0;
    for (uint32_t i = 0; i < 1024; i++)
    {
        SIZE_T                      cbActual = 0;
        MEMORY_BASIC_INFORMATION    MemInfo  = { 0, 0, 0, 0, 0, 0, 0 };
        NTSTATUS rcNt = g_pfnNtQueryVirtualMemory(hProcess,
                                                  (void const *)uPtrWhere,
                                                  MemoryBasicInformation,
                                                  &MemInfo,
                                                  sizeof(MemInfo),
                                                  &cbActual);
        if (!NT_SUCCESS(rcNt))
        {
            if (rcNt == STATUS_INVALID_PARAMETER)
                return pThis->rcResult;
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NT_QI_VIRTUAL_MEMORY_ERROR,
                                       "NtQueryVirtualMemory failed for %p: %#x", uPtrWhere, rcNt);
        }

        /*
         * Record images.
         */
        if (   MemInfo.Type == SEC_IMAGE
            || MemInfo.Type == SEC_PROTECTED_IMAGE
            || MemInfo.Type == (SEC_IMAGE | SEC_PROTECTED_IMAGE))
        {
            uint32_t iImg = pThis->cImages;
            rcNt = g_pfnNtQueryVirtualMemory(hProcess,
                                             (void const *)uPtrWhere,
                                             MemorySectionName,
                                             &pThis->aImages[iImg].Name,
                                             sizeof(pThis->aImages[iImg].Name) - sizeof(WCHAR),
                                             &cbActual);
            if (!NT_SUCCESS(rcNt))
                return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NT_QI_VIRTUAL_MEMORY_NM_ERROR,
                                           "NtQueryVirtualMemory/MemorySectionName failed for %p: %#x", uPtrWhere, rcNt);
            pThis->aImages[iImg].Name.UniStr.Buffer[pThis->aImages[iImg].Name.UniStr.Length / sizeof(WCHAR)] = '\0';
            SUP_DPRINTF((MemInfo.AllocationBase == MemInfo.BaseAddress
                         ? " *%p-%p %#06x/%#06x %#09x  %ls\n"
                         : "  %p-%p %#06x/%#06x %#09x  %ls\n",
                         MemInfo.BaseAddress, (uintptr_t)MemInfo.BaseAddress - MemInfo.RegionSize - 1, MemInfo.Protect,
                         MemInfo.AllocationProtect, MemInfo.Type, pThis->aImages[iImg].Name.UniStr.Buffer));

            /* New or existing image? */
            bool fNew = true;
            uint32_t iSearch = iImg;
            while (iSearch-- > 0)
                if (supHardNtVpAreUniStringsEqual(&pThis->aImages[iSearch].Name.UniStr, &pThis->aImages[iImg].Name.UniStr))
                {
                    int rc = supHardNtVpAddRegion(pThis, &pThis->aImages[iSearch], &MemInfo);
                    if (RT_FAILURE(rc))
                        return rc;
                    fNew = false;
                    break;
                }
                else if (pThis->aImages[iSearch].uImageBase == (uintptr_t)MemInfo.AllocationBase)
                    return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NT_MAPPING_NAME_CHANGED,
                                               "Unexpected base address match");

            if (fNew)
            {
                int rc = supHardNtVpNewImage(pThis, &pThis->aImages[iImg], &MemInfo);
                if (RT_SUCCESS(rc))
                {
                    pThis->cImages++;
                    if (pThis->cImages >= RT_ELEMENTS(pThis->aImages))
                        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_MANY_DLLS_LOADED,
                                                   "Internal error: aImages is full.\n");
                }
#ifdef IN_RING3 /* Continue and add more information if unknown DLLs are found. */
                else if (rc != VERR_SUP_VP_NOT_KNOWN_DLL_OR_EXE && rc != VERR_SUP_VP_NON_SYSTEM32_DLL)
                    return rc;
#else
                else
                    return rc;
#endif
            }
        }
        /*
         * XP, W2K3: Ignore the CSRSS read-only region as best we can.
         */
        else if (      (MemInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
                    == PAGE_EXECUTE_READ
                 && cXpExceptions == 0
                 && (uintptr_t)MemInfo.BaseAddress >= UINT32_C(0x78000000)
                 /* && MemInfo.BaseAddress == pPeb->ReadOnlySharedMemoryBase */
                 && g_uNtVerCombined < SUP_MAKE_NT_VER_SIMPLE(6, 0) )
        {
            cXpExceptions++;
            SUP_DPRINTF(("  %p-%p %#06x/%#06x %#09x  XP CSRSS read-only region\n", MemInfo.BaseAddress,
                         (uintptr_t)MemInfo.BaseAddress - MemInfo.RegionSize - 1, MemInfo.Protect,
                         MemInfo.AllocationProtect, MemInfo.Type));
        }
        /*
         * Executable memory?
         */
#ifndef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
        else if (MemInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
        {
            supHardNtVpSetInfo2(pThis, VERR_SUP_VP_FOUND_EXEC_MEMORY,
                                "Found executable memory at %p (%p LB %#zx): type=%#x prot=%#x state=%#x aprot=%#x abase=%p",
                                uPtrWhere,
                                MemInfo.BaseAddress,
                                MemInfo.RegionSize,
                                MemInfo.Type,
                                MemInfo.Protect,
                                MemInfo.State,
                                MemInfo.AllocationBase,
                                MemInfo.AllocationProtect);
# ifdef IN_RING3
            /* Continue add more information about the problematic process. */
# else
            return pThis->rcResult;
# endif
        }
#endif
        else
            SUP_DPRINTF((MemInfo.AllocationBase == MemInfo.BaseAddress
                         ? " *%p-%p %#06x/%#06x %#09x\n"
                         : "  %p-%p %#06x/%#06x %#09x\n",
                         MemInfo.BaseAddress, (uintptr_t)MemInfo.BaseAddress - MemInfo.RegionSize - 1,
                         MemInfo.Protect, MemInfo.AllocationProtect, MemInfo.Type));

        /*
         * Advance.
         */
        cbAdvance = MemInfo.RegionSize;
        if (uPtrWhere + cbAdvance <= uPtrWhere)
            return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_EMPTY_REGION_TOO_LARGE,
                                       "Empty region at %p.", uPtrWhere);
        uPtrWhere += MemInfo.RegionSize;
    }

    return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_TOO_MANY_MEMORY_REGIONS,
                               "Too many virtual memory regions.\n");
}


/**
 * Check the integrity of the executable of the process.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure. Details
 *                              about images are added to this.
 * @param   hProcess            The process to verify.
 */
static int supHardNtVpCheckExe(PSUPHNTVPSTATE pThis, HANDLE hProcess)
{
    /*
     * Make sure there is exactly one executable image.
     */
    unsigned cExecs = 0;
    unsigned iExe   = ~0U;
    unsigned i = pThis->cImages;
    while (i-- > 0)
    {
        if (!pThis->aImages[i].fDll)
        {
            cExecs++;
            iExe = i;
        }
    }
    if (cExecs == 0)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NO_FOUND_NO_EXE_MAPPING,
                                   "No executable mapping found in the virtual address space.");
    if (cExecs != 1)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_FOUND_MORE_THAN_ONE_EXE_MAPPING,
                                   "Found more than one executable mapping in the virtual address space.");
    PSUPHNTVPIMAGE pImage = &pThis->aImages[iExe];

    /*
     * Check that it matches the executable image of the process.
     */
    int             rc;
    ULONG           cbUniStr = sizeof(UNICODE_STRING) + RTPATH_MAX * sizeof(RTUTF16);
    PUNICODE_STRING pUniStr  = (PUNICODE_STRING)suplibHardenedAllocZ(cbUniStr);
    if (!pUniStr)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NO_MEMORY,
                                  "Error allocating %zu bytes for process name.", cbUniStr);
    ULONG    cbIgn = 0;
    NTSTATUS rcNt = NtQueryInformationProcess(hProcess, ProcessImageFileName, pUniStr, cbUniStr - sizeof(WCHAR), &cbIgn);
    if (NT_SUCCESS(rcNt))
    {
        if (supHardNtVpAreUniStringsEqual(pUniStr, &pImage->Name.UniStr))
            rc = VINF_SUCCESS;
        else
        {
            pUniStr->Buffer[pUniStr->Length / sizeof(WCHAR)] = '\0';
            rc = supHardNtVpSetInfo2(pThis, VERR_SUP_VP_EXE_VS_PROC_NAME_MISMATCH,
                                     "Process image name does not match the exectuable we found: %ls vs %ls.",
                                     pUniStr->Buffer, pImage->Name.UniStr.Buffer);
        }
    }
    else
        rc = supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NT_QI_PROCESS_NM_ERROR,
                                 "NtQueryInformationProcess/ProcessImageFileName failed: %#x", rcNt);
    suplibHardenedFree(pUniStr);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Validate the signing of the executable image.
     * This will load the fDllCharecteristics and fImageCharecteristics members we use below.
     */
    rc = supHardNtVpVerifyImage(pThis, pImage, hProcess);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Check linking requirements.
     */
    SECTION_IMAGE_INFORMATION ImageInfo;
    rcNt = NtQueryInformationProcess(hProcess, ProcessImageInformation, &ImageInfo, sizeof(ImageInfo), NULL);
    if (!NT_SUCCESS(rcNt))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NT_QI_PROCESS_IMG_INFO_ERROR,
                                   "NtQueryInformationProcess/ProcessImageInformation failed: %#x", rcNt);
    if ( !(ImageInfo.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_EXE_MISSING_FORCE_INTEGRITY,
                                   "EXE DllCharacteristics=%#x, expected IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY to be set.",
                                   ImageInfo.DllCharacteristics);
    if (!(ImageInfo.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_EXE_MISSING_DYNAMIC_BASE,
                                   "EXE DllCharacteristics=%#x, expected IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE to be set.",
                                   ImageInfo.DllCharacteristics);
    if (!(ImageInfo.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT))
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_EXE_MISSING_NX_COMPAT,
                                   "EXE DllCharacteristics=%#x, expected IMAGE_DLLCHARACTERISTICS_NX_COMPAT to be set.",
                                   ImageInfo.DllCharacteristics);

    if (pImage->fDllCharecteristics != ImageInfo.DllCharacteristics)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_DLL_CHARECTERISTICS_MISMATCH,
                                   "EXE Info.DllCharacteristics=%#x fDllCharecteristics=%#x.",
                                   ImageInfo.DllCharacteristics, pImage->fDllCharecteristics);

    if (pImage->fImageCharecteristics != ImageInfo.ImageCharacteristics)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_DLL_CHARECTERISTICS_MISMATCH,
                                   "EXE Info.ImageCharacteristics=%#x fImageCharecteristics=%#x.",
                                   ImageInfo.ImageCharacteristics, pImage->fImageCharecteristics);

    return VINF_SUCCESS;
}


/**
 * Check the integrity of the DLLs found in the process.
 *
 * @returns VBox status code.
 * @param   pThis               The process scanning state structure. Details
 *                              about images are added to this.
 * @param   hProcess            The process to verify.
 */
static int supHardNtVpCheckDlls(PSUPHNTVPSTATE pThis, HANDLE hProcess)
{
    /*
     * Check for duplicate entries.
     */
    uint32_t i = pThis->cImages;
    while (i-- > 1)
    {
        const char *pszName = pThis->aImages[i].pszName;
        uint32_t j = i;
        while (j-- > 0)
            if (pThis->aImages[j].pszName == pszName)
                return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_DUPLICATE_DLL_MAPPING,
                                           "Duplicate image entries for %s: %ls and %ls",
                                           pszName, pThis->aImages[i].Name.UniStr.Buffer, pThis->aImages[j].Name.UniStr.Buffer);
    }

    /*
     * Check that both ntdll and kernel32 are present.
     * ASSUMES the entries in g_apszSupNtVpAllowedDlls are all lower case.
     */
    uint32_t iNtDll    = UINT32_MAX;
    uint32_t iKernel32 = UINT32_MAX;
    uint32_t iApiSetSchema = UINT32_MAX;
    i = pThis->cImages;
    while (i-- > 0)
        if (suplibHardenedStrCmp(pThis->aImages[i].pszName, "ntdll.dll") == 0)
            iNtDll = i;
        else if (suplibHardenedStrCmp(pThis->aImages[i].pszName, "kernel32.dll") == 0)
            iKernel32 = i;
        else if (suplibHardenedStrCmp(pThis->aImages[i].pszName, "apisetschema.dll") == 0)
            iApiSetSchema = i;
#ifdef VBOX_PERMIT_MORE
        else if (suplibHardenedStrCmp(pThis->aImages[i].pszName, "acres.dll") == 0)
            pThis->aImages[i].f32bitResourceDll = true;
#endif
    if (iNtDll == UINT32_MAX)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NO_NTDLL_MAPPING,
                                   "The process has no NTDLL.DLL.");
    if (iKernel32 == UINT32_MAX)
        return supHardNtVpSetInfo2(pThis, VERR_SUP_VP_NO_KERNEL32_MAPPING,
                                   "The process has no KERNEL32.DLL.");


    /*
     * Verify that the DLLs are correctly signed (by MS).
     */
    i = pThis->cImages;
    while (i-- > 0)
    {
        pThis->aImages[i].fNtCreateSectionPatch     = i == iNtDll;
        pThis->aImages[i].fApiSetSchemaOnlySection1 = i == iApiSetSchema && pThis->aImages[i].cRegions == 1;

        int rc = supHardNtVpVerifyImage(pThis, &pThis->aImages[i], hProcess);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Verifies the given process.
 *
 * The following requirements are checked:
 *  - The process only has one thread, the calling thread.
 *  - The process has no debugger attached.
 *  - The executable image of the process is verified to be signed with
 *    certificate known to this code at build time.
 *  - The executable image is one of a predefined set.
 *  - The process has only a very limited set of system DLLs loaded.
 *  - The system DLLs signatures check out fine.
 *  - The only executable memory in the process belongs to the system DLLs and
 *    the executable image.
 *
 * @returns VBox status code.
 * @param   hProcess            The process to verify.
 * @param   hThread             A thread in the process (the caller).
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
DECLHIDDEN(int) supHardenedWinVerifyProcess(HANDLE hProcess, HANDLE hThread, PRTERRINFO pErrInfo)
{
    int rc = supHardNtVpThread(hProcess, hThread, pErrInfo);
#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
    if (RT_SUCCESS(rc))
        rc = supHardNtVpDebugger(hProcess, pErrInfo);
#endif
    if (RT_SUCCESS(rc))
    {
        PSUPHNTVPSTATE pThis = supHardNtVpCreateState(pErrInfo);
        if (pThis)
        {
            rc = supHardNtVpScanVirtualMemory(pThis, hProcess);
            if (RT_SUCCESS(rc))
                rc = supHardNtVpCheckExe(pThis, hProcess);
            if (RT_SUCCESS(rc))
                rc = supHardNtVpCheckDlls(pThis, hProcess);

            suplibHardenedFree(pThis);
        }
        else
            rc = VERR_SUP_VP_NO_MEMORY_STATE;
    }
    return rc;
}

