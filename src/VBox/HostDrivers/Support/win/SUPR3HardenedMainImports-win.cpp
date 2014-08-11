/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened Main, Windows Import Trickery.
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
#include <iprt/nt/nt-and-windows.h>

#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>

#include "SUPLibInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define SUPHARNT_COMMENT(a_Blah) /* nothing */

#define VBOX_HARDENED_STUB_WITHOUT_IMPORTS
#ifdef VBOX_HARDENED_STUB_WITHOUT_IMPORTS
# define SUPHNTIMP_ERROR(a_id, a_szWhere, a_enmOp, a_rc, ...) \
    do { static const char s_szWhere[] = a_szWhere; *(char *)(uintptr_t)(a_id) += 1; __debugbreak(); } while (0)
#else
# define SUPHNTIMP_ERROR(a_id, a_szWhere, a_enmOp, a_rc, ...) \
    supR3HardenedFatalMsg(a_szWhere, a_enmOp, a_rc, __VA_ARGS__)

#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
typedef struct SUPHNTIMPFUNC
{
    const char         *pszName;
    PFNRT              *ppfnImport;
} SUPHNTIMPFUNC;
typedef SUPHNTIMPFUNC const *PCSUPHNTIMPFUNC;

typedef struct SUPHNTIMPSYSCALL
{
    PFNRT              pfnType1;
    PFNRT              pfnType2;
} SUPHNTIMPSYSCALL;
typedef SUPHNTIMPSYSCALL const *PCSUPHNTIMPSYSCALL;

typedef struct SUPHNTIMPDLL
{
    /** @name Static data.
     * @{  */
    const wchar_t          *pwszName;
    const char             *pszName;
    size_t                  cImports;
    PCSUPHNTIMPFUNC         paImports;
    /** Array running parallel to paImports if present. */
    PCSUPHNTIMPSYSCALL      paSyscalls;
    /** @} */


    /** The image base. */
    uint8_t const          *pbImageBase;
    /** The NT headers. */
    PIMAGE_NT_HEADERS       pNtHdrs;
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

} SUPHNTIMPDLL;
typedef SUPHNTIMPDLL *PSUPHNTIMPDLL;



/*
 * Declare assembly symbols.
 */
#define SUPHARNT_IMPORT_STDCALL(a_Name, a_cbParamsX86) \
    extern PFNRT    RT_CONCAT(g_pfn, a_Name);
#define SUPHARNT_IMPORT_SYSCALL(a_Name, a_cbParamsX86) \
    SUPHARNT_IMPORT_STDCALL(a_Name, a_cbParamsX86) \
    extern uint32_t RT_CONCAT(g_uApiNo, a_Name); \
    extern FNRT     RT_CONCAT(a_Name, _SyscallType1); \
    extern FNRT     RT_CONCAT(a_Name, _SyscallType2);

RT_C_DECLS_BEGIN
#include "import-template-ntdll.h"
#include "import-template-kernel32.h"
RT_C_DECLS_END

/*
 * Import functions.
 */
#undef SUPHARNT_IMPORT_SYSCALL
#undef SUPHARNT_IMPORT_STDCALL
#define SUPHARNT_IMPORT_SYSCALL(a_Name, a_cbParamsX86) { #a_Name, &RT_CONCAT(g_pfn, a_Name) },
#define SUPHARNT_IMPORT_STDCALL(a_Name, a_cbParamsX86) { #a_Name, &RT_CONCAT(g_pfn, a_Name) },
static const SUPHNTIMPFUNC g_aSupNtImpNtDllFunctions[] =
{
#include "import-template-ntdll.h"
};

static const SUPHNTIMPFUNC g_aSupNtImpKernel32Functions[] =
{
#include "import-template-kernel32.h"
};



/*
 * Syscalls in ntdll.
 */
static const SUPHNTIMPSYSCALL g_aSupNtImpNtDllSyscalls[] =
{
#undef SUPHARNT_IMPORT_SYSCALL
#undef SUPHARNT_IMPORT_STDCALL
#ifdef RT_ARCH_AMD64
# define SUPHARNT_IMPORT_SYSCALL(a_Name, a_cbParamsX86) { &RT_CONCAT(a_Name, _SyscallType1), NULL },
#else
# define SUPHARNT_IMPORT_SYSCALL(a_Name, a_cbParamsX86) { &RT_CONCAT(a_Name,_SyscallType1), &RT_CONCAT(a_Name, _SyscallType2) },
#endif
#define SUPHARNT_IMPORT_STDCALL(a_Name, a_cbParamsX86)  { NULL, NULL },
#include "import-template-ntdll.h"
};


/**
 * All the DLLs we import from.
 */
static SUPHNTIMPDLL g_aSupNtImpDlls[] =
{
    { L"ntdll.dll",      "ntdll.dll",      RT_ELEMENTS(g_aSupNtImpNtDllFunctions), g_aSupNtImpNtDllFunctions, g_aSupNtImpNtDllSyscalls },
    { L"kernelbase.dll", "kernelbase.dll", 0 /* optional module, forwarders only */, NULL, NULL },
    { L"kernel32.dll",   "kernel32.dll",   RT_ELEMENTS(g_aSupNtImpKernel32Functions), g_aSupNtImpKernel32Functions, NULL },
};


static void supR3HardenedFindOrLoadModule(PSUPHNTIMPDLL pDll)
{
#ifdef VBOX_HARDENED_STUB_WITHOUT_IMPORTS
    uint32_t const  cbName     = (uint32_t)RTUtf16Len(pDll->pwszName) * sizeof(WCHAR);
    PPEB_LDR_DATA   pLdrData   = NtCurrentPeb()->Ldr;
    LIST_ENTRY     *pList      = &pLdrData->InMemoryOrderModuleList;
    LIST_ENTRY     *pListEntry = pList->Flink;
    uint32_t        cLoops     = 0;
    while (pListEntry != pList && cLoops < 1024)
    {
        PLDR_DATA_TABLE_ENTRY pLdrEntry = RT_FROM_MEMBER(pListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        if (   pLdrEntry->FullDllName.Length > cbName + sizeof(WCHAR)
            && (   pLdrEntry->FullDllName.Buffer[(pLdrEntry->FullDllName.Length - cbName) / sizeof(WCHAR) - 1] == '\\'
                || pLdrEntry->FullDllName.Buffer[(pLdrEntry->FullDllName.Length - cbName) / sizeof(WCHAR) - 1] == '/')
            && RTUtf16ICmpAscii(&pLdrEntry->FullDllName.Buffer[(pLdrEntry->FullDllName.Length - cbName) / sizeof(WCHAR)],
                                pDll->pszName) == 0)
        {
            pDll->pbImageBase = (uint8_t *)pLdrEntry->DllBase;
            return;
        }

        pListEntry = pListEntry->Flink;
        cLoops++;
    }

    if (!pDll->cImports)
        pDll->pbImageBase = NULL; /* optional */
    else
        SUPHNTIMP_ERROR(1, "supR3HardenedFindOrLoadModule", kSupInitOp_Misc, VERR_MODULE_NOT_FOUND,
                        "Failed to locate %ls", pDll->pwszName);
#else
    HMODULE hmod = GetModuleHandleW(pDll->pwszName);
    if (RT_UNLIKELY(!hmod && pDll->cImports))
        SUPHNTIMP_ERROR(1, "supR3HardenedWinInitImports", kSupInitOp_Misc, VERR_MODULE_NOT_FOUND,
                        "Failed to locate %ls", pDll->pwszName);
    pDll->pbImageBase = (uint8_t *)hmod;
#endif
}


static void supR3HardenedParseModule(PSUPHNTIMPDLL pDll)
{
    /*
     * Locate the PE header, do some basic validations.
     */
    IMAGE_DOS_HEADER const *pMzHdr = (IMAGE_DOS_HEADER const *)pDll->pbImageBase;
    uint32_t           offNtHdrs = 0;
    PIMAGE_NT_HEADERS  pNtHdrs;
    if (pMzHdr->e_magic == IMAGE_DOS_SIGNATURE)
    {
        offNtHdrs = pMzHdr->e_lfanew;
        if (offNtHdrs > _2K)
            SUPHNTIMP_ERROR(2, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_MODULE_NOT_FOUND,
                            "%ls: e_lfanew=%#x, expected a lower value", pDll->pwszName, offNtHdrs);
    }
    pDll->pNtHdrs = pNtHdrs = (PIMAGE_NT_HEADERS)&pDll->pbImageBase[offNtHdrs];

    if (pNtHdrs->Signature != IMAGE_NT_SIGNATURE)
        SUPHNTIMP_ERROR(3, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: Invalid PE signature: %#x", pDll->pwszName, pNtHdrs->Signature);
    if (pNtHdrs->FileHeader.SizeOfOptionalHeader != sizeof(pNtHdrs->OptionalHeader))
        SUPHNTIMP_ERROR(4, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: Unexpected optional header size: %#x", pDll->pwszName, pNtHdrs->FileHeader.SizeOfOptionalHeader);
    if (pNtHdrs->OptionalHeader.Magic != RT_CONCAT3(IMAGE_NT_OPTIONAL_HDR,ARCH_BITS,_MAGIC))
        SUPHNTIMP_ERROR(5, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: Unexpected optional header magic: %#x", pDll->pwszName, pNtHdrs->OptionalHeader.Magic);
    if (pNtHdrs->OptionalHeader.NumberOfRvaAndSizes != IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
        SUPHNTIMP_ERROR(6, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: Unexpected number of RVA and sizes: %#x", pDll->pwszName, pNtHdrs->OptionalHeader.NumberOfRvaAndSizes);

    pDll->offNtHdrs      = offNtHdrs;
    pDll->offEndSectHdrs = offNtHdrs
                         + sizeof(*pNtHdrs)
                         + pNtHdrs->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    pDll->cbImage        = pNtHdrs->OptionalHeader.SizeOfImage;

    /*
     * Find the export directory.
     */
    IMAGE_DATA_DIRECTORY ExpDir = pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (   ExpDir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)
        || ExpDir.VirtualAddress < pDll->offEndSectHdrs
        || ExpDir.VirtualAddress >= pNtHdrs->OptionalHeader.SizeOfImage
        || ExpDir.VirtualAddress + ExpDir.Size > pNtHdrs->OptionalHeader.SizeOfImage)
        SUPHNTIMP_ERROR(7, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: Missing or invalid export directory: %#lx LB %#x", pDll->pwszName, ExpDir.VirtualAddress, ExpDir.Size);
    pDll->offExportDir = ExpDir.VirtualAddress;
    pDll->cbExportDir  = ExpDir.Size;

    IMAGE_EXPORT_DIRECTORY const *pExpDir = (IMAGE_EXPORT_DIRECTORY const *)&pDll->pbImageBase[ExpDir.VirtualAddress];

    if (   pExpDir->NumberOfFunctions >= _1M
        || pExpDir->NumberOfFunctions <  1
        || pExpDir->NumberOfNames     >= _1M
        || pExpDir->NumberOfNames     <  1)
        SUPHNTIMP_ERROR(8, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                        "%ls: NumberOfNames or/and NumberOfFunctions are outside the expected range: nof=%#x non=%#x\n",
                        pDll->pwszName, pExpDir->NumberOfFunctions, pExpDir->NumberOfNames);
    pDll->cNamedExports = pExpDir->NumberOfNames;
    pDll->cExports      = RT_MAX(pExpDir->NumberOfNames,  pExpDir->NumberOfFunctions);

    if (   pExpDir->AddressOfFunctions < pDll->offEndSectHdrs
        || pExpDir->AddressOfFunctions >= pNtHdrs->OptionalHeader.SizeOfImage
        || pExpDir->AddressOfFunctions + pDll->cExports * sizeof(uint32_t) > pNtHdrs->OptionalHeader.SizeOfImage)
           SUPHNTIMP_ERROR(9, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                           "%ls: Bad AddressOfFunctions: %#x\n", pDll->pwszName, pExpDir->AddressOfFunctions);
    pDll->paoffExports = (uint32_t const *)&pDll->pbImageBase[pExpDir->AddressOfFunctions];

    if (   pExpDir->AddressOfNames < pDll->offEndSectHdrs
        || pExpDir->AddressOfNames >= pNtHdrs->OptionalHeader.SizeOfImage
        || pExpDir->AddressOfNames + pExpDir->NumberOfNames * sizeof(uint32_t) > pNtHdrs->OptionalHeader.SizeOfImage)
           SUPHNTIMP_ERROR(10, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                           "%ls: Bad AddressOfNames: %#x\n", pDll->pwszName, pExpDir->AddressOfNames);
    pDll->paoffNamedExports = (uint32_t const *)&pDll->pbImageBase[pExpDir->AddressOfNames];

    if (   pExpDir->AddressOfNameOrdinals < pDll->offEndSectHdrs
        || pExpDir->AddressOfNameOrdinals >= pNtHdrs->OptionalHeader.SizeOfImage
        || pExpDir->AddressOfNameOrdinals + pExpDir->NumberOfNames * sizeof(uint32_t) > pNtHdrs->OptionalHeader.SizeOfImage)
           SUPHNTIMP_ERROR(11, "supR3HardenedParseModule", kSupInitOp_Misc, VERR_INVALID_EXE_SIGNATURE,
                           "%ls: Bad AddressOfNameOrdinals: %#x\n", pDll->pwszName, pExpDir->AddressOfNameOrdinals);
    pDll->pau16NameOrdinals = (uint16_t const *)&pDll->pbImageBase[pExpDir->AddressOfNameOrdinals];
}


static const char *supR3HardenedResolveImport(PSUPHNTIMPDLL pDll, PCSUPHNTIMPFUNC pImport)
{
    /*
     * Binary search.
     */
    uint32_t iStart = 0;
    uint32_t iEnd   = pDll->cNamedExports;
    while (iStart < iEnd)
    {
        uint32_t iCur        = iStart + (iEnd - iStart) / 2;
        uint32_t offExpName  = pDll->paoffNamedExports[iCur];
        if (RT_UNLIKELY(offExpName < pDll->offEndSectHdrs || offExpName >= pDll->cbImage))
            SUPHNTIMP_ERROR(12, "supR3HardenedResolveImport", kSupInitOp_Misc, VERR_SYMBOL_NOT_FOUND,
                            "%ls: Bad export name entry: %#x (iCur=%#x)", pDll->pwszName, offExpName, iCur);

        const char *pszExpName = (const char *)&pDll->pbImageBase[offExpName];
        int iDiff = strcmp(pszExpName, pImport->pszName);
        if (iDiff > 0)      /* pszExpName > pszSymbol: search chunck before i */
            iEnd = iCur;
        else if (iDiff < 0) /* pszExpName < pszSymbol: search chunk after i */
            iStart = iCur + 1;
        else                /* pszExpName == pszSymbol */
        {
            uint16_t iExpOrdinal = pDll->pau16NameOrdinals[iCur];
            if (iExpOrdinal < pDll->cExports)
            {
                uint32_t offExport = pDll->paoffExports[iExpOrdinal];
                if (offExport < pDll->cbImage)
                {
                    if (offExport - pDll->offExportDir >= pDll->cbExportDir)
                    {
                        *pImport->ppfnImport = (PFNRT)&pDll->pbImageBase[offExport];
                        return NULL;
                    }

                    /* Forwarder. */
                    return (const char *)&pDll->pbImageBase[offExport];
                }
                SUPHNTIMP_ERROR(13, "supR3HardenedResolveImport", kSupInitOp_Misc, VERR_BAD_EXE_FORMAT,
                                "%ls: The export RVA for '%s' is out of bounds: %#x (SizeOfImage %#x)",
                                 pDll->pwszName, offExport, pDll->cbImage);
            }
            SUPHNTIMP_ERROR(14, "supR3HardenedResolveImport", kSupInitOp_Misc, VERR_BAD_EXE_FORMAT,
                            "%ls: Name ordinal for '%s' is out of bounds: %#x (max %#x)",
                            pDll->pwszName, iExpOrdinal, pDll->cExports);
            return NULL;
        }
    }

    SUPHNTIMP_ERROR(15, "supR3HardenedResolveImport", kSupInitOp_Misc, VERR_SYMBOL_NOT_FOUND,
                    "%ls: Failed to resolve '%s'.", pDll->pwszName, pImport->pszName);
    return NULL;
}



/**
 * Resolves imported functions, esp. system calls from NTDLL.
 *
 * This crap is necessary because there are sandboxing products out there that
 * will mess with system calls we make, just like any other wannabe userland
 * rootkit.  Kudos to microsoft for not providing a generic system call hook API
 * in the kernel mode, which I guess is what forcing these kind of products to
 * do ugly userland hacks that doesn't really hold water.
 */
DECLHIDDEN(void) supR3HardenedWinInitImports(void)
{
    /*
     * Find the DLLs we will be needing first (forwarders).
     */
    for (uint32_t iDll = 0; iDll < RT_ELEMENTS(g_aSupNtImpDlls); iDll++)
    {
        supR3HardenedFindOrLoadModule(&g_aSupNtImpDlls[iDll]);
        if (g_aSupNtImpDlls[iDll].pbImageBase)
            supR3HardenedParseModule(&g_aSupNtImpDlls[iDll]);
    }

    /*
     * Resolve the functions.
     */
    for (uint32_t iDll = 0; iDll < RT_ELEMENTS(g_aSupNtImpDlls); iDll++)
        for (uint32_t i = 0; i < g_aSupNtImpDlls[iDll].cImports; i++)
        {
            const char *pszForwarder = supR3HardenedResolveImport(&g_aSupNtImpDlls[iDll], &g_aSupNtImpDlls[iDll].paImports[i]);
            if (pszForwarder)
            {
                const char *pszDot = strchr(pszForwarder, '.');
                size_t  cchDllName = pszDot - pszForwarder;
                SUPHNTIMPFUNC  Tmp = g_aSupNtImpDlls[iDll].paImports[i];
                Tmp.pszName = pszDot + 1;
                if (cchDllName == sizeof("ntdll") - 1 && RTStrNICmp(pszForwarder, RT_STR_TUPLE("ntdll")) == 0)
                    supR3HardenedResolveImport(&g_aSupNtImpDlls[0], &Tmp);
                else if (cchDllName == sizeof("kernelbase") - 1 && RTStrNICmp(pszForwarder, RT_STR_TUPLE("kernelbase")) == 0)
                    supR3HardenedResolveImport(&g_aSupNtImpDlls[1], &Tmp);
                else
                    SUPHNTIMP_ERROR(16, "supR3HardenedWinInitImports", kSupInitOp_Misc, VERR_MODULE_NOT_FOUND,
                                    "%ls: Failed to resolve forwarder '%s'.", g_aSupNtImpDlls[iDll].pwszName, pszForwarder);
            }
        }
}


