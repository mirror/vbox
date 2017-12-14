/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, R0 Driver, NT.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define IMAGE_NT_HEADERS   NT_IMAGE_NT_HEADERS
#define IMAGE_NT_HEADERS32 NT_IMAGE_NT_HEADERS32
#define IMAGE_NT_HEADERS64 NT_IMAGE_NT_HEADERS64
#define PIMAGE_NT_HEADERS   NT_PIMAGE_NT_HEADERS
#define PIMAGE_NT_HEADERS32 NT_PIMAGE_NT_HEADERS32
#define PIMAGE_NT_HEADERS64 NT_PIMAGE_NT_HEADERS64
#define IPRT_NT_MAP_TO_ZW
#include "the-nt-kernel.h"
#include <iprt/dbg.h>

#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/utf16.h>
#include "internal-r0drv-nt.h"
#include "internal/magics.h"

#undef IMAGE_NT_HEADERS
#undef IMAGE_NT_HEADERS32
#undef IMAGE_NT_HEADERS64
#undef PIMAGE_NT_HEADERS
#undef PIMAGE_NT_HEADERS32
#undef PIMAGE_NT_HEADERS64
#include <iprt/formats/pecoff.h>
#include <iprt/formats/mz.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Private logging macro, will use DbgPrint! */
#ifdef IN_GUEST
# define RTR0DBG_NT_ERROR_LOG(a) do { RTLogBackdoorPrintf a; DbgPrint a; } while (0)
# define RTR0DBG_NT_DEBUG_LOG(a) do { RTLogBackdoorPrintf a; DbgPrint a; } while (0)
#else
# define RTR0DBG_NT_ERROR_LOG(a) do { DbgPrint a; } while (0)
# define RTR0DBG_NT_DEBUG_LOG(a) do { DbgPrint a; } while (0)
#endif
#ifndef LOG_ENABLED
# undef RTR0DBG_NT_DEBUG_LOG
# define RTR0DBG_NT_DEBUG_LOG(a) do { } while (0)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#define PIMAGE_NT_HEADERS RT_CONCAT(PIMAGE_NT_HEADERS, ARCH_BITS)

/**
 * Information we cache for a kernel module.
 */
typedef struct RTDBGNTKRNLMODINFO
{
    /** The module name.   */
    char                    szName[32];

    /** The image base. */
    uint8_t const          *pbImageBase;
    /** The NT headers. */
    PIMAGE_NT_HEADERS       pNtHdrs;
    /** Set if this module parsed okay and all fields are valid. */
    bool                    fOkay;
    /** The NT header offset/RVA. */
    uint32_t                offNtHdrs;
    /** The end of the section headers. */
    uint32_t                offEndSectHdrs;
    /** The end of the image. */
    uint32_t                cbImage;
    /** Offset of the export directory. */
    uint32_t                offExportDir;
    /** Size of the export directory. */
    uint32_t                cbExportDir;

    /** Exported functions and data by ordinal (RVAs). */
    uint32_t const         *paoffExports;
    /** The number of exports. */
    uint32_t                cExports;
    /** The number of exported names. */
    uint32_t                cNamedExports;
    /** Pointer to the array of exported names (RVAs to strings). */
    uint32_t const         *paoffNamedExports;
    /** Array parallel to paoffNamedExports with the corresponding ordinals
     *  (indexes into paoffExports). */
    uint16_t const         *pau16NameOrdinals;
} RTDBGNTKRNLMODINFO;
/** Pointer to kernel module info. */
typedef RTDBGNTKRNLMODINFO *PRTDBGNTKRNLMODINFO;
/** Pointer to const kernel module info. */
typedef RTDBGNTKRNLMODINFO const *PCRTDBGNTKRNLMODINFO;


/**
 * NT kernel info instance.
 */
typedef struct RTDBGKRNLINFOINT
{
    /** Magic value (RTDBGKRNLINFO_MAGIC). */
    uint32_t            u32Magic;
    /** Reference counter.  */
    uint32_t volatile   cRefs;
    /** Number of additional modules in the cache. */
    uint32_t            cModules;
    /** Additional modules. */
    RTDBGNTKRNLMODINFO  aModules[3];
} RTDBGKRNLINFOINT;



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Pointer to MmGetSystemRoutineAddress. */
#ifdef RT_ARCH_X86
static decltype(MmGetSystemRoutineAddress) *g_pfnMmGetSystemRoutineAddress = NULL;
#else
static decltype(MmGetSystemRoutineAddress) *g_pfnMmGetSystemRoutineAddress = MmGetSystemRoutineAddress;
#endif
/** Info about the ntoskrnl.exe mapping. */
static RTDBGNTKRNLMODINFO   g_NtOsKrnlInfo = { "ntoskrnl.exe", NULL, NULL, false, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL };
/** Info about the hal.dll mapping. */
static RTDBGNTKRNLMODINFO   g_HalInfo      = { "hal.dll",      NULL, NULL, false, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL };



/**
 * Looks up an symbol int the export table.
 *
 * @returns VINF_SUCCESS or VERR_SYMBOL_NOT_FOUND.
 * @param   pModInfo            The module info.
 * @param   pszSymbol           The symbol to find.
 * @param   ppvSymbol           Where to put the symbol address.
 *
 * @note    Support library has similar code for in the importless area.
 */
static int rtR0DbgKrnlInfoLookupSymbol(PCRTDBGNTKRNLMODINFO pModInfo, const char *pszSymbol, void **ppvSymbol)
{
    if (pModInfo->fOkay)
    {
        /*
         * Binary search.
         */
        __try
        {
            uint32_t iStart = 0;
            uint32_t iEnd   = pModInfo->cNamedExports;
            while (iStart < iEnd)
            {
                uint32_t iCur        = iStart + (iEnd - iStart) / 2;
                uint32_t offExpName  = pModInfo->paoffNamedExports[iCur];
                if (offExpName >= pModInfo->offEndSectHdrs && offExpName < pModInfo->cbImage)
                { /* likely */ }
                else
                {
                    RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlInfoLookupSymbol: %s: Bad export name entry: %#x (iCur=%#x)\n",
                                          pModInfo->szName, offExpName, iCur));
                    break;
                }

                const char *pszExpName = (const char *)&pModInfo->pbImageBase[offExpName];
                int iDiff = strcmp(pszExpName, pszSymbol);
                if (iDiff > 0)      /* pszExpName > pszSymbol: search chunck before i */
                    iEnd = iCur;
                else if (iDiff < 0) /* pszExpName < pszSymbol: search chunk after i */
                    iStart = iCur + 1;
                else                /* pszExpName == pszSymbol */
                {
                    uint16_t iExpOrdinal = pModInfo->pau16NameOrdinals[iCur];
                    if (iExpOrdinal < pModInfo->cExports)
                    {
                        uint32_t offExport = pModInfo->paoffExports[iExpOrdinal];
                        if (offExport - pModInfo->offExportDir >= pModInfo->cbExportDir)
                        {
                            *ppvSymbol = (void *)&pModInfo->pbImageBase[offExport];
                            return VINF_SUCCESS;
                        }

                        RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlInfoLookupSymbol: %s: Forwarded symbol '%s': offExport=%#x (dir %#x LB %#x)\n",
                                              pModInfo->szName, pszSymbol, offExport, pModInfo->offExportDir, pModInfo->cbExportDir));
                    }
                    else
                        RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlInfoLookupSymbol: %s: Name ordinal for '%s' is out of bounds: %#x (max %#x)\n",
                                              pModInfo->szName, iExpOrdinal, pModInfo->cExports));
                    break;
                }
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlInfoLookupSymbol: Exception searching '%s' for '%s'...\n",
                                  pModInfo->szName, pszSymbol));
        }
    }

    *ppvSymbol = NULL;
    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Parses (PE) module headers and fills in the coresponding module info struct.
 *
 * @returns true on if success, false if not.
 * @param   pModInfo            The module info structure to fill in with parsed
 *                              data.  The szName and fOkay are set by the
 *                              caller, this function does the rest.
 * @param   pbMapping           The image mapping address
 * @param   cbMapping           The image mapping size.
 *
 * @note    Support library has similar code for in the importless area.
 */
static bool rtR0DbgKrnlNtParseModule(PRTDBGNTKRNLMODINFO pModInfo, uint8_t const *pbMapping, size_t cbMapping)
{
#define MODERR_RETURN(a_LogMsg, ...) \
        do { RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtParseModule: " a_LogMsg, __VA_ARGS__)); return false; } while (0)

    pModInfo->pbImageBase = pbMapping;

    /*
     * Locate the PE header, do some basic validations.
     */
    IMAGE_DOS_HEADER const *pMzHdr = (IMAGE_DOS_HEADER const *)pbMapping;
    uint32_t                offNtHdrs = 0;
    PIMAGE_NT_HEADERS       pNtHdrs;
    if (pMzHdr->e_magic == IMAGE_DOS_SIGNATURE)
    {
        offNtHdrs = pMzHdr->e_lfanew;
        if (offNtHdrs > _2K)
            MODERR_RETURN("%s: e_lfanew=%#x, expected a lower value\n", pModInfo->szName, offNtHdrs);
    }
    pModInfo->pNtHdrs = pNtHdrs = (PIMAGE_NT_HEADERS)&pbMapping[offNtHdrs];

    if (pNtHdrs->Signature != IMAGE_NT_SIGNATURE)
        MODERR_RETURN("%s: Invalid PE signature: %#x", pModInfo->szName, pNtHdrs->Signature);
    if (pNtHdrs->FileHeader.SizeOfOptionalHeader != sizeof(pNtHdrs->OptionalHeader))
        MODERR_RETURN("%s: Unexpected optional header size: %#x\n", pModInfo->szName, pNtHdrs->FileHeader.SizeOfOptionalHeader);
    if (pNtHdrs->OptionalHeader.Magic != RT_CONCAT3(IMAGE_NT_OPTIONAL_HDR,ARCH_BITS,_MAGIC))
        MODERR_RETURN("%s: Unexpected optional header magic: %#x\n", pModInfo->szName, pNtHdrs->OptionalHeader.Magic);
    if (pNtHdrs->OptionalHeader.NumberOfRvaAndSizes != IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
        MODERR_RETURN("%s: Unexpected number of RVA and sizes: %#x\n", pModInfo->szName, pNtHdrs->OptionalHeader.NumberOfRvaAndSizes);

    pModInfo->offNtHdrs      = offNtHdrs;
    pModInfo->offEndSectHdrs = offNtHdrs
                             + sizeof(*pNtHdrs)
                             + pNtHdrs->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    pModInfo->cbImage        = pNtHdrs->OptionalHeader.SizeOfImage;
    if (pModInfo->cbImage > cbMapping)
        MODERR_RETURN("%s: The image size %#x is larger than the mapping: %#x\n",
                      pModInfo->szName, pModInfo->cbImage, cbMapping);

    /*
     * Find the export directory.
     */
    IMAGE_DATA_DIRECTORY ExpDir = pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (   ExpDir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)
        || ExpDir.VirtualAddress < pModInfo->offEndSectHdrs
        || ExpDir.VirtualAddress >= pModInfo->cbImage
        || ExpDir.VirtualAddress + ExpDir.Size > pModInfo->cbImage)
        MODERR_RETURN("%s: Missing or invalid export directory: %#lx LB %#x\n", pModInfo->szName, ExpDir.VirtualAddress, ExpDir.Size);
    pModInfo->offExportDir = ExpDir.VirtualAddress;
    pModInfo->cbExportDir  = ExpDir.Size;

    IMAGE_EXPORT_DIRECTORY const *pExpDir = (IMAGE_EXPORT_DIRECTORY const *)&pbMapping[ExpDir.VirtualAddress];

    if (   pExpDir->NumberOfFunctions >= _1M
        || pExpDir->NumberOfFunctions <  1
        || pExpDir->NumberOfNames     >= _1M
        || pExpDir->NumberOfNames     <  1)
        MODERR_RETURN("%s: NumberOfNames or/and NumberOfFunctions are outside the expected range: nof=%#x non=%#x\n",
                      pModInfo->szName, pExpDir->NumberOfFunctions, pExpDir->NumberOfNames);
    pModInfo->cNamedExports = pExpDir->NumberOfNames;
    pModInfo->cExports      = RT_MAX(pExpDir->NumberOfNames,  pExpDir->NumberOfFunctions);

    if (   pExpDir->AddressOfFunctions < pModInfo->offEndSectHdrs
        || pExpDir->AddressOfFunctions >= pModInfo->cbImage
        || pExpDir->AddressOfFunctions + pModInfo->cExports * sizeof(uint32_t) > pModInfo->cbImage)
           MODERR_RETURN("%s: Bad AddressOfFunctions: %#x\n", pModInfo->szName, pExpDir->AddressOfFunctions);
    pModInfo->paoffExports = (uint32_t const *)&pbMapping[pExpDir->AddressOfFunctions];

    if (   pExpDir->AddressOfNames < pModInfo->offEndSectHdrs
        || pExpDir->AddressOfNames >= pModInfo->cbImage
        || pExpDir->AddressOfNames + pExpDir->NumberOfNames * sizeof(uint32_t) > pModInfo->cbImage)
           MODERR_RETURN("%s: Bad AddressOfNames: %#x\n", pModInfo->szName, pExpDir->AddressOfNames);
    pModInfo->paoffNamedExports = (uint32_t const *)&pbMapping[pExpDir->AddressOfNames];

    if (   pExpDir->AddressOfNameOrdinals < pModInfo->offEndSectHdrs
        || pExpDir->AddressOfNameOrdinals >= pModInfo->cbImage
        || pExpDir->AddressOfNameOrdinals + pExpDir->NumberOfNames * sizeof(uint32_t) > pModInfo->cbImage)
           MODERR_RETURN("%s: Bad AddressOfNameOrdinals: %#x\n", pModInfo->szName, pExpDir->AddressOfNameOrdinals);
    pModInfo->pau16NameOrdinals = (uint16_t const *)&pbMapping[pExpDir->AddressOfNameOrdinals];

    /*
     * Success.
     */
    return true;
#undef MODERR_RETURN
}


/**
 * Searches the module information from the kernel for the NT kernel module, the
 * HAL module, and optionally one more module.
 *
 * If the NT kernel or HAL modules have already been found, they'll be skipped.
 *
 * @returns IPRT status code.
 * @retval  VERR_LDR_GENERAL_FAILURE if we failed to parse the NT kernel or HAL.
 * @retval  VERR_BAD_EXE_FORMAT if we failed to parse @a pModInfo.
 * @retval  VERR_MODULE_NOT_FOUND if @a pModInfo wasn't found.
 * @retval  VERR_BUFFER_UNDERFLOW if less that two modules was returned by the
 *          system.
 *
 * @param   pModInfo            Custom module to search for.  Optional.
 */
static int rtR0DbgKrnlNtInit(PRTDBGNTKRNLMODINFO pModInfo)
{
    RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: pModInfo=%p\n", pModInfo));

    /*
     * Allocate a reasonably large buffer and get the information we need.  We don't
     * need everything since the result starts off with the kernel bits in load order.
     *
     * Note! ZwQuerySystemInformation requires NT4.  For 3.51 we could possibly emit
     *       the syscall ourselves, if we cared.
     */
    uint32_t                cModules = pModInfo ? 110 /*32KB*/ : 27 /*8KB*/;
    ULONG                   cbInfo   = RT_OFFSETOF(RTL_PROCESS_MODULES, Modules[cModules]);
    PRTL_PROCESS_MODULES    pInfo    = (PRTL_PROCESS_MODULES)RTMemAllocZ(cbInfo);
    if (!pInfo)
    {
        cModules = cModules / 4;
        cbInfo   = RT_OFFSETOF(RTL_PROCESS_MODULES, Modules[cModules]);
        pInfo    = (PRTL_PROCESS_MODULES)RTMemAllocZ(cbInfo);
        if (!pInfo)
        {
            RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: Out of memory!\n"));
            return VERR_NO_MEMORY;
        }
    }

    int      rc;
    ULONG    cbActual = 0;
    NTSTATUS rcNt = ZwQuerySystemInformation(SystemModuleInformation, pInfo, cbInfo, &cbActual);
    RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: ZwQuerySystemInformation returned %#x and NumberOfModules=%#x\n",
                          rcNt, pInfo->NumberOfModules));
    if (   NT_SUCCESS(rcNt)
        || rcNt == STATUS_INFO_LENGTH_MISMATCH)
        rc = VINF_SUCCESS;
    else
    {
        RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: ZwQuerySystemInformation failed: %#x\n", rcNt));
        rc = RTErrConvertFromNtStatus(rcNt);
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Search the info.  The information is ordered with the kernel bits first,
         * we expect aleast two modules to be returned to us (kernel + hal)!
         */
#if ARCH_BITS == 32
        uintptr_t const uMinKernelAddr = _2G; /** @todo resolve MmSystemRangeStart */
#else
        uintptr_t const uMinKernelAddr = (uintptr_t)MM_SYSTEM_RANGE_START;
#endif
        if (pInfo->NumberOfModules < cModules)
            cModules = pInfo->NumberOfModules;
        if (cModules < 2)
        {
            RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: Error! Only %u module(s) returned!\n", cModules));
            rc = VERR_BUFFER_UNDERFLOW;
        }
        for (uint32_t iModule = 0; iModule < cModules; iModule++)
            RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: [%u]= %p LB %#x %s\n", iModule, pInfo->Modules[iModule].ImageBase,
                                  pInfo->Modules[iModule].ImageSize, pInfo->Modules[iModule].FullPathName));

        /*
         * First time around we serch for the NT kernel and HAL.  We'll look for NT
         * kerneland HAL in the first 16 entries, and if not found, use the first
         * and second entry respectively.
         */
        if (   RT_SUCCESS(rc)
            && !g_NtOsKrnlInfo.pbImageBase
            && !g_HalInfo.pbImageBase)
        {
            /* Find them. */
            RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: Looking for kernel and hal...\n"));
            uint32_t const cMaxModules = RT_MIN(cModules, 16);
            uint32_t       idxNtOsKrnl = UINT32_MAX;
            uint32_t       idxHal      = UINT32_MAX;
            for (uint32_t iModule = 0; iModule < cMaxModules; iModule++)
            {
                RTL_PROCESS_MODULE_INFORMATION const * const pModule = &pInfo->Modules[iModule];
                if (   (uintptr_t)pModule->ImageBase >= uMinKernelAddr
                    && (uintptr_t)pModule->ImageSize >= _4K)
                {
                    const char *pszName = (const char *)&pModule->FullPathName[pModule->OffsetToFileName];
                    if (   idxNtOsKrnl == UINT32_MAX
                        && RTStrICmpAscii(pszName, g_NtOsKrnlInfo.szName) == 0)
                    {
                        idxNtOsKrnl = iModule;
                        if (idxHal != UINT32_MAX)
                            break;
                    }
                    else if (   idxHal == UINT32_MAX
                             && RTStrICmpAscii(pszName, g_HalInfo.szName) == 0)
                    {
                        idxHal = iModule;
                        if (idxHal != UINT32_MAX)
                            break;
                    }
                }
            }
            RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: idxNtOsKrnl=%#x idxHal=%#x\n", idxNtOsKrnl, idxHal));
            if (idxNtOsKrnl == UINT32_MAX)
            {
                idxNtOsKrnl = 0;
                RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: 'ntoskrnl.exe' not found, picking '%s' instead\n",
                                      pInfo->Modules[idxNtOsKrnl].FullPathName));
            }
            if (idxHal == UINT32_MAX)
            {
                idxHal = 1;
                RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: 'hal.dll' not found, picking '%s' instead\n",
                                      pInfo->Modules[idxHal].FullPathName));
            }

            /* Parse them. */
            //RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: Parsing NT kernel...\n"));
            __try
            {
                g_NtOsKrnlInfo.fOkay = rtR0DbgKrnlNtParseModule(&g_NtOsKrnlInfo,
                                                                (uint8_t const *)pInfo->Modules[idxNtOsKrnl].ImageBase,
                                                                pInfo->Modules[idxNtOsKrnl].ImageSize);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                g_NtOsKrnlInfo.fOkay = false;
                RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: Exception in rtR0DbgKrnlNtParseModule parsing ntoskrnl.exe...\n"));
            }

            //RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: Parsing HAL...\n"));
            __try
            {
                g_HalInfo.fOkay = rtR0DbgKrnlNtParseModule(&g_HalInfo, (uint8_t const *)pInfo->Modules[idxHal].ImageBase,
                                                           pInfo->Modules[idxHal].ImageSize);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                g_HalInfo.fOkay = false;
                RTR0DBG_NT_ERROR_LOG(("rtR0DbgKrnlNtInit: Exception in rtR0DbgKrnlNtParseModule parsing hal.dll...\n"));
            }
            if (!g_NtOsKrnlInfo.fOkay || !g_HalInfo.fOkay)
                rc = VERR_LDR_GENERAL_FAILURE;

            /*
             * Resolve symbols we may need in the NT kernel (provided it parsed successfully)
             */
            if (g_NtOsKrnlInfo.fOkay)
            {
                if (!g_pfnMmGetSystemRoutineAddress)
                {
                    //RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: Looking up 'MmGetSystemRoutineAddress'...\n"));
                    rtR0DbgKrnlInfoLookupSymbol(&g_NtOsKrnlInfo, "MmGetSystemRoutineAddress", (void **)&g_pfnMmGetSystemRoutineAddress);
                }
            }
        }

        /*
         * If we're still good, search for the given module (optional).
         */
        if (RT_SUCCESS(rc) && pModInfo)
        {
            RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: Locating module '%s'...\n", pModInfo->szName));
            rc = VERR_MODULE_NOT_FOUND;
            for (uint32_t iModule = 0; iModule < cModules; iModule++)
            {
                RTL_PROCESS_MODULE_INFORMATION const * const pModule = &pInfo->Modules[iModule];
                if (   (uintptr_t)pModule->ImageBase >= uMinKernelAddr
                    && (uintptr_t)pModule->ImageSize >= _4K)
                {
                    const char *pszName = (const char *)&pModule->FullPathName[pModule->OffsetToFileName];
                    if (   pModInfo->pbImageBase == NULL
                        && RTStrICmpAscii(pszName, pModInfo->szName) == 0)
                    {
                        /*
                         * Found the module, try parse it.
                         */
                        __try
                        {
                            pModInfo->fOkay = rtR0DbgKrnlNtParseModule(pModInfo, (uint8_t const *)pModule->ImageBase,
                                                                       pModule->ImageSize);
                            rc = VINF_SUCCESS;
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER)
                        {
                            pModInfo->fOkay = false;
                            rc = VERR_BAD_EXE_FORMAT;
                        }
                        break;
                    }
                }
            }
        }
    }

    RTR0DBG_NT_DEBUG_LOG(("rtR0DbgKrnlNtInit: returns %d\n", rc));
    RTMemFree(pInfo);
    return rc;
}



RTR0DECL(int) RTR0DbgKrnlInfoOpen(PRTDBGKRNLINFO phKrnlInfo, uint32_t fFlags)
{
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    RTDBGKRNLINFOINT *pThis = (RTDBGKRNLINFOINT *)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTDBGKRNLINFO_MAGIC;
        pThis->cRefs    = 1;
        *phKrnlInfo = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTR0DECL(uint32_t) RTR0DbgKrnlInfoRetain(RTDBGKRNLINFO hKrnlInfo)
{
    RTDBGKRNLINFOINT *pThis = hKrnlInfo;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs && cRefs < 100000);
    return cRefs;
}


static void rtR0DbgKrnlNtDtor(RTDBGKRNLINFOINT *pThis)
{
    pThis->u32Magic = ~RTDBGKRNLINFO_MAGIC;
    RTMemFree(pThis);
}


RTR0DECL(uint32_t) RTR0DbgKrnlInfoRelease(RTDBGKRNLINFO hKrnlInfo)
{
    RTDBGKRNLINFOINT *pThis = hKrnlInfo;
    if (pThis == NIL_RTDBGKRNLINFO)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (cRefs == 0)
        rtR0DbgKrnlNtDtor(pThis);
    return cRefs;
}


RTR0DECL(int) RTR0DbgKrnlInfoQueryMember(RTDBGKRNLINFO hKrnlInfo, const char *pszModule, const char *pszStructure,
                                         const char *pszMember, size_t *poffMember)
{
    RTDBGKRNLINFOINT *pThis = hKrnlInfo;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertPtrReturn(pszMember, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszModule, VERR_INVALID_POINTER);
    AssertPtrReturn(pszStructure, VERR_INVALID_POINTER);
    AssertPtrReturn(poffMember, VERR_INVALID_POINTER);
    return VERR_NOT_FOUND;
}


RTR0DECL(int) RTR0DbgKrnlInfoQuerySymbol(RTDBGKRNLINFO hKrnlInfo, const char *pszModule, const char *pszSymbol, void **ppvSymbol)
{
    RTDBGKRNLINFOINT *pThis = hKrnlInfo;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertPtrReturn(pszSymbol, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppvSymbol, VERR_INVALID_PARAMETER);

    RTR0DBG_NT_DEBUG_LOG(("RTR0DbgKrnlInfoQuerySymbol: pszModule=%s pszSymbol=%s\n", pszModule ? pszModule : "<null>", pszSymbol));

    int rc;
    uintptr_t uValue = 0;
    if (!pszModule)
    {
        /*
         * Search both ntoskrnl and hal, may use MmGetSystemRoutineAddress as fallback.
         */
        if (g_NtOsKrnlInfo.pbImageBase)
            rc = VINF_SUCCESS;
        else
            rc = rtR0DbgKrnlNtInit(NULL);
        if (RT_SUCCESS(rc))
        {
            Assert(g_NtOsKrnlInfo.fOkay);
            Assert(g_HalInfo.fOkay);
            RTR0DBG_NT_DEBUG_LOG(("RTR0DbgKrnlInfoQuerySymbol: Calling RTR0DbgKrnlInfoQuerySymbol on NT kernel...\n"));
            rc = rtR0DbgKrnlInfoLookupSymbol(&g_NtOsKrnlInfo, pszSymbol, ppvSymbol);
            if (RT_FAILURE(rc))
            {
                RTR0DBG_NT_DEBUG_LOG(("RTR0DbgKrnlInfoQuerySymbol: Calling RTR0DbgKrnlInfoQuerySymbol on HAL kernel...\n"));
                rc = rtR0DbgKrnlInfoLookupSymbol(&g_HalInfo, pszSymbol, ppvSymbol);
            }
            RTR0DBG_NT_DEBUG_LOG(("RTR0DbgKrnlInfoQuerySymbol: returns %d\n", rc));
        }
        else
        {
            /* Init failed. Try resolve symbol, but preserve the status code up to a point. */
            int rc2 = VERR_SYMBOL_NOT_FOUND;
            if (g_NtOsKrnlInfo.fOkay)
                rc2 = rtR0DbgKrnlInfoLookupSymbol(&g_NtOsKrnlInfo, pszSymbol, ppvSymbol);
            if (g_HalInfo.fOkay && rc2 == VERR_SYMBOL_NOT_FOUND)
                rc2 = rtR0DbgKrnlInfoLookupSymbol(&g_HalInfo, pszSymbol, ppvSymbol);
            if (   rc2 == VERR_SYMBOL_NOT_FOUND
                && g_pfnMmGetSystemRoutineAddress)
            {
                /* We'll overwrite init failure status code here since
                   MmGetSystemRoutineAddress will do the job for us.  */
                PRTUTF16 pwszSymbol;
                rc = RTStrToUtf16(pszSymbol, &pwszSymbol);
                if (RT_SUCCESS(rc))
                {
                    UNICODE_STRING UniStr;
                    UniStr.Buffer = pwszSymbol;
                    UniStr.Length = (uint16_t)RTUtf16Len(pwszSymbol);
                    UniStr.MaximumLength = UniStr.Length + 1;
                    *ppvSymbol = g_pfnMmGetSystemRoutineAddress(&UniStr);
                    if (*ppvSymbol)
                        rc = VINF_SUCCESS;
                    else
                        rc = VERR_SYMBOL_NOT_FOUND;
                    RTUtf16Free(pwszSymbol);
                }
            }
        }
    }
    else
    {
        /*
         * Search specified module.
         */
        rc = VERR_MODULE_NOT_FOUND;
        PRTDBGNTKRNLMODINFO pModInfo;
        if (RTStrICmpAscii(pszModule, g_NtOsKrnlInfo.szName) == 0)
            pModInfo = &g_NtOsKrnlInfo;
        else if (RTStrICmpAscii(pszModule, g_HalInfo.szName) == 0)
            pModInfo = &g_NtOsKrnlInfo;
        else
        {
            pModInfo = NULL;
            for (unsigned i = 0; i < pThis->cModules; i++)
                if (RTStrICmpAscii(pszModule, pThis->aModules[i].szName) == 0)
                {
                    pModInfo = &pThis->aModules[i];
                    break;
                }
            if (!pModInfo)
            {
                /*
                 * Not found, try load it.  If module table is full, drop the first
                 * entry and shuffle the other up to make space.
                 */
                size_t const        cchModule = strlen(pszModule);
                RTDBGNTKRNLMODINFO  NewModInfo;
                if (cchModule < sizeof(NewModInfo.szName))
                {
                    RT_ZERO(NewModInfo);
                    memcpy(NewModInfo.szName, pszModule, cchModule);
                    NewModInfo.szName[cchModule] = '\0';

                    rc = rtR0DbgKrnlNtInit(&NewModInfo);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(NewModInfo.fOkay);
                        uint32_t iModule = pThis->cModules;
                        if (iModule >= RT_ELEMENTS(pThis->aModules))
                        {
                            iModule = RT_ELEMENTS(pThis->aModules) - 1;
                            memmove(&pThis->aModules[0], &pThis->aModules[1], iModule * sizeof(pThis->aModules[0]));
                        }
                        pThis->aModules[iModule] = NewModInfo;
                        pThis->cModules          = iModule + 1;
                        pModInfo = &pThis->aModules[iModule];
                        rc = VINF_SUCCESS;
                    }
                }
                else
                {
                    AssertMsgFailed(("cchModule=%zu pszModule=%s\n", cchModule, pszModule));
                    rc = VERR_FILENAME_TOO_LONG;
                }
            }
        }
        if (pModInfo)
            rc = rtR0DbgKrnlInfoLookupSymbol(pModInfo, pszSymbol, ppvSymbol);
    }
    if (ppvSymbol)
        *ppvSymbol = (void *)uValue;
    return rc;
}


RTR0DECL(int) RTR0DbgKrnlInfoQuerySize(RTDBGKRNLINFO hKrnlInfo, const char *pszModule, const char *pszType, size_t *pcbType)
{
    RTDBGKRNLINFOINT *pThis = hKrnlInfo;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTDBGKRNLINFO_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertPtrNullReturn(pszModule, VERR_INVALID_POINTER);
    AssertPtrReturn(pszType, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbType, VERR_INVALID_POINTER);
    return VERR_NOT_FOUND;
}

