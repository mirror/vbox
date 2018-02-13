/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Windows backend.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NEM
#include <iprt/nt/nt-and-windows.h>
#include <WinHvPlatform.h>

#ifndef _WIN32_WINNT_WIN10
# error "Missing _WIN32_WINNT_WIN10"
#endif
#ifndef _WIN32_WINNT_WIN10_RS1 /* Missing define, causing trouble for us. */
# define _WIN32_WINNT_WIN10_RS1 (_WIN32_WINNT_WIN10 + 1)
#endif
#include <sysinfoapi.h>
#include <debugapi.h>
#include <errhandlingapi.h>
#include <fileapi.h>
#include <winerror.h> /* no api header for this. */

#include <VBox/vmm/nem.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>

#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef LOG_ENABLED
# define NEM_WIN_INTERCEPT_NT_IO_CTLS
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** @name Our two-bit physical page state for PGMPAGE
 * @{ */
#define NEM_WIN_PAGE_STATE_NOT_SET      0
#define NEM_WIN_PAGE_STATE_UNMAPPED     1
#define NEM_WIN_PAGE_STATE_READABLE     2
#define NEM_WIN_PAGE_STATE_WRITABLE     3
/** @} */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name APIs imported from WinHvPlatform.dll
 * @{ */
static decltype(WHvGetCapability) *                 g_pfnWHvGetCapability;
static decltype(WHvCreatePartition) *               g_pfnWHvCreatePartition;
static decltype(WHvSetupPartition) *                g_pfnWHvSetupPartition;
static decltype(WHvDeletePartition) *               g_pfnWHvDeletePartition;
static decltype(WHvGetPartitionProperty) *          g_pfnWHvGetPartitionProperty;
static decltype(WHvSetPartitionProperty) *          g_pfnWHvSetPartitionProperty;
static decltype(WHvMapGpaRange) *                   g_pfnWHvMapGpaRange;
static decltype(WHvUnmapGpaRange) *                 g_pfnWHvUnmapGpaRange;
static decltype(WHvTranslateGva) *                  g_pfnWHvTranslateGva;
static decltype(WHvCreateVirtualProcessor) *        g_pfnWHvCreateVirtualProcessor;
static decltype(WHvDeleteVirtualProcessor) *        g_pfnWHvDeleteVirtualProcessor;
static decltype(WHvRunVirtualProcessor) *           g_pfnWHvRunVirtualProcessor;
static decltype(WHvGetRunExitContextSize) *         g_pfnWHvGetRunExitContextSize;
static decltype(WHvCancelRunVirtualProcessor) *     g_pfnWHvCancelRunVirtualProcessor;
static decltype(WHvGetVirtualProcessorRegisters) *  g_pfnWHvGetVirtualProcessorRegisters;
static decltype(WHvSetVirtualProcessorRegisters) *  g_pfnWHvSetVirtualProcessorRegisters;
/** @} */


/**
 * Import instructions.
 */
static const struct
{
    uint8_t     idxDll;     /**< 0 for WinHvPlatform.dll, 1 for vid.dll. */
    bool        fOptional;  /**< Set if import is optional. */
    PFNRT      *ppfn;       /**< The function pointer variable. */
    const char *pszName;    /**< The function name. */
} g_aImports[] =
{
#define NEM_WIN_IMPORT(a_idxDll, a_fOptional, a_Name) { (a_idxDll), (a_fOptional), (PFNRT *)&RT_CONCAT(g_pfn,a_Name), #a_Name }
    NEM_WIN_IMPORT(0, false, WHvGetCapability),
    NEM_WIN_IMPORT(0, false, WHvCreatePartition),
    NEM_WIN_IMPORT(0, false, WHvSetupPartition),
    NEM_WIN_IMPORT(0, false, WHvDeletePartition),
    NEM_WIN_IMPORT(0, false, WHvGetPartitionProperty),
    NEM_WIN_IMPORT(0, false, WHvSetPartitionProperty),
    NEM_WIN_IMPORT(0, false, WHvMapGpaRange),
    NEM_WIN_IMPORT(0, false, WHvUnmapGpaRange),
    NEM_WIN_IMPORT(0, false, WHvTranslateGva),
    NEM_WIN_IMPORT(0, false, WHvCreateVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvDeleteVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvGetRunExitContextSize),
    NEM_WIN_IMPORT(0, false, WHvCancelRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvGetVirtualProcessorRegisters),
    NEM_WIN_IMPORT(0, false, WHvSetVirtualProcessorRegisters),
#undef NEM_WIN_IMPORT
};


/*
 * Let the preprocessor alias the APIs to import variables for better autocompletion.
 */
#ifndef IN_SLICKEDIT
# define WHvGetCapability                           g_pfnWHvGetCapability
# define WHvCreatePartition                         g_pfnWHvCreatePartition
# define WHvSetupPartition                          g_pfnWHvSetupPartition
# define WHvDeletePartition                         g_pfnWHvDeletePartition
# define WHvGetPartitionProperty                    g_pfnWHvGetPartitionProperty
# define WHvSetPartitionProperty                    g_pfnWHvSetPartitionProperty
# define WHvMapGpaRange                             g_pfnWHvMapGpaRange
# define WHvUnmapGpaRange                           g_pfnWHvUnmapGpaRange
# define WHvTranslateGva                            g_pfnWHvTranslateGva
# define WHvCreateVirtualProcessor                  g_pfnWHvCreateVirtualProcessor
# define WHvDeleteVirtualProcessor                  g_pfnWHvDeleteVirtualProcessor
# define WHvRunVirtualProcessor                     g_pfnWHvRunVirtualProcessor
# define WHvGetRunExitContextSize                   g_pfnWHvGetRunExitContextSize
# define WHvCancelRunVirtualProcessor               g_pfnWHvCancelRunVirtualProcessor
# define WHvGetVirtualProcessorRegisters            g_pfnWHvGetVirtualProcessorRegisters
# define WHvSetVirtualProcessorRegisters            g_pfnWHvSetVirtualProcessorRegisters
#endif


#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS

/** The real NtDeviceIoControlFile API in NTDLL.   */
static decltype(NtDeviceIoControlFile) *g_pfnNtDeviceIoControlFile;

/**
 * Wrapper that logs the call from VID.DLL.
 *
 * This is very handy for figuring out why an API call fails.
 */
static NTSTATUS WINAPI
nemR3WinLogWrapper_NtDeviceIoControlFile(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                         PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                         PVOID pvOutput, ULONG cbOutput)
{
    NTSTATUS rcNt = g_pfnNtDeviceIoControlFile(hFile, hEvt, pfnApcCallback, pvApcCtx, pIos, uFunction,
                                               pvInput, cbInput, pvOutput, cbOutput);
    if (!hEvt && !pfnApcCallback && !pvApcCtx)
        LogRel(("VID!NtDeviceIoControlFile: hFile=%#zx pIos=%p->{s:%#x, i:%#zx} uFunction=%#x Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
                hFile, pIos, pIos->Status, pIos->Information, uFunction, pvInput, cbInput, pvOutput, cbOutput, rcNt, ASMReturnAddress()));
    else
        LogRel(("VID!NtDeviceIoControlFile: hFile=%#zx hEvt=%#zx Apc=%p/%p pIos=%p->{s:%#x, i:%#zx} uFunction=%#x Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
                hFile, hEvt, pfnApcCallback, pvApcCtx, pIos, pIos->Status, pIos->Information, uFunction,
                pvInput, cbInput, pvOutput, cbOutput, rcNt, ASMReturnAddress()));
    return rcNt;
}


/**
 * Patches the call table of VID.DLL so we can intercept and log
 * NtDeviceIoControlFile.
 *
 * This is for DEBUGGING only.
 *
 * @param   hLdrModVid      The VID module handle.
 */
static void nemR3WinInitVidIntercepts(RTLDRMOD hLdrModVid)
{
    /*
     * Locate the real API.
     */
    g_pfnNtDeviceIoControlFile = (decltype(NtDeviceIoControlFile) *)RTLdrGetSystemSymbol("NTDLL.DLL", "NtDeviceIoControlFile");
    AssertReturnVoid(g_pfnNtDeviceIoControlFile > 0);

    /*
     * Locate the PE header and get what we need from it.
     */
    uint8_t const *pbImage = (uint8_t const *)RTLdrGetNativeHandle(hLdrModVid);
    IMAGE_DOS_HEADER const *pMzHdr  = (IMAGE_DOS_HEADER const *)pbImage;
    AssertReturnVoid(pMzHdr->e_magic == IMAGE_DOS_SIGNATURE);
    IMAGE_NT_HEADERS const *pNtHdrs = (IMAGE_NT_HEADERS const *)&pbImage[pMzHdr->e_lfanew];
    AssertReturnVoid(pNtHdrs->Signature == IMAGE_NT_SIGNATURE);

    uint32_t const             cbImage   = pNtHdrs->OptionalHeader.SizeOfImage;
    IMAGE_DATA_DIRECTORY const ImportDir = pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    /*
     * Walk the import descriptor table looking for NTDLL.DLL.
     */
    bool fSuccess = true;
    AssertReturnVoid(ImportDir.Size > 0);
    AssertReturnVoid(ImportDir.Size < cbImage);
    AssertReturnVoid(ImportDir.VirtualAddress > 0);
    AssertReturnVoid(ImportDir.VirtualAddress < cbImage);
    for (PIMAGE_IMPORT_DESCRIPTOR pImps = (PIMAGE_IMPORT_DESCRIPTOR)&pbImage[ImportDir.VirtualAddress];
         pImps->Name != 0 && pImps->FirstThunk != 0;
         pImps++)
    {
        AssertReturnVoid(pImps->Name < cbImage);
        const char *pszModName = (const char *)&pbImage[pImps->Name];
        if (RTStrICmpAscii(pszModName, "ntdll.dll"))
            continue;
        AssertReturnVoid(pImps->FirstThunk < cbImage);
        AssertReturnVoid(pImps->OriginalFirstThunk < cbImage);

        /*
         * Walk the thunks table(s) looking for NtDeviceIoControlFile.
         */
        PIMAGE_THUNK_DATA pFirstThunk = (PIMAGE_THUNK_DATA)&pbImage[pImps->FirstThunk]; /* update this. */
        PIMAGE_THUNK_DATA pThunk      = pImps->OriginalFirstThunk == 0                  /* read from this. */
                                      ? (PIMAGE_THUNK_DATA)&pbImage[pImps->FirstThunk]
                                      : (PIMAGE_THUNK_DATA)&pbImage[pImps->OriginalFirstThunk];
        while (pThunk->u1.Ordinal != 0)
        {
            if (!(pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32))
            {
                AssertReturnVoid(pThunk->u1.Ordinal > 0 && pThunk->u1.Ordinal < cbImage);

                const char *pszSymbol = (const char *)&pbImage[(uintptr_t)pThunk->u1.AddressOfData + 2];
                if (strcmp(pszSymbol, "NtDeviceIoControlFile") == 0)
                {
                    DWORD fOldProt = PAGE_EXECUTE;
                    VirtualProtect(&pFirstThunk->u1.Function, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &fOldProt);
                    pFirstThunk->u1.Function = (uintptr_t)nemR3WinLogWrapper_NtDeviceIoControlFile;
                    VirtualProtect(&pFirstThunk->u1.Function, sizeof(uintptr_t), fOldProt, &fOldProt);
                    fSuccess = true;
                }
            }

            pThunk++;
            pFirstThunk++;
        }
    }
    Assert(fSuccess);
}
#endif



/**
 * Worker for nemR3NativeInit that probes and load the native API.
 *
 * @returns VBox status code.
 * @param   fForced             Whether the HMForced flag is set and we should
 *                              fail if we cannot initialize.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitProbeAndLoad(bool fForced, PRTERRINFO pErrInfo)
{
    /*
     * Check that the DLL files we need are present, but without loading them.
     * We'd like to avoid loading them unnecessarily.
     */
    WCHAR wszPath[MAX_PATH + 64];
    UINT  cwcPath = GetSystemDirectoryW(wszPath, MAX_PATH);
    if (cwcPath >= MAX_PATH || cwcPath < 2)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "GetSystemDirectoryW failed (%#x / %u)", cwcPath, GetLastError());

    if (wszPath[cwcPath - 1] != '\\' || wszPath[cwcPath - 1] != '/')
        wszPath[cwcPath++] = '\\';
    RTUtf16CopyAscii(&wszPath[cwcPath], RT_ELEMENTS(wszPath) - cwcPath, "WinHvPlatform.dll");
    if (GetFileAttributesW(wszPath) == INVALID_FILE_ATTRIBUTES)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "The native API dll was not found (%ls)", wszPath);

    /*
     * Check that we're in a VM and that the hypervisor identifies itself as Hyper-V.
     */
    if (!ASMHasCpuId())
        return RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "No CPUID support");
    if (!ASMIsValidStdRange(ASMCpuId_EAX(0)))
        return RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "No CPUID leaf #1");
    if (!(ASMCpuId_ECX(1) & X86_CPUID_FEATURE_ECX_HVP))
        return RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Not in a hypervisor partition (HVP=0)");

    uint32_t cMaxHyperLeaf = 0;
    uint32_t uEbx = 0;
    uint32_t uEcx = 0;
    uint32_t uEdx = 0;
    ASMCpuIdExSlow(0x40000000, 0, 0, 0, &cMaxHyperLeaf, &uEbx, &uEcx, &uEdx);
    if (!ASMIsValidHypervisorRange(cMaxHyperLeaf))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Invalid hypervisor CPUID range (%#x %#x %#x %#x)",
                             cMaxHyperLeaf, uEbx, uEcx, uEdx);
    if (   uEbx != UINT32_C(0x7263694d) /* Micr */
        || uEcx != UINT32_C(0x666f736f) /* osof */
        || uEdx != UINT32_C(0x76482074) /* t Hv */)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                             "Not Hyper-V CPUID signature: %#x %#x %#x (expected %#x %#x %#x)",
                             uEbx, uEcx, uEdx, UINT32_C(0x7263694d), UINT32_C(0x666f736f), UINT32_C(0x76482074));
    if (cMaxHyperLeaf < UINT32_C(0x40000005))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Too narrow hypervisor CPUID range (%#x)", cMaxHyperLeaf);

    /** @todo would be great if we could recognize a root partition from the
     *        CPUID info, but I currently don't dare do that. */

    /*
     * Now try load the DLLs and resolve the APIs.
     */
    static const char * const s_apszDllNames[2] = { "WinHvPlatform.dll",  "vid.dll" };
    RTLDRMOD                  ahMods[2]         = { NIL_RTLDRMOD,          NIL_RTLDRMOD };
    int                       rc = VINF_SUCCESS;
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszDllNames); i++)
    {
        int rc2 = RTLdrLoadSystem(s_apszDllNames[i], true /*fNoUnload*/, &ahMods[i]);
        if (RT_FAILURE(rc2))
        {
            if (!RTErrInfoIsSet(pErrInfo))
                RTErrInfoSetF(pErrInfo, rc2, "Failed to load API DLL: %s: %Rrc", s_apszDllNames[i], rc2);
            else
                RTErrInfoAddF(pErrInfo, rc2, "; %s: %Rrc", s_apszDllNames[i], rc2);
            ahMods[i] = NIL_RTLDRMOD;
            rc = VERR_NEM_INIT_FAILED;
        }
    }
    if (RT_SUCCESS(rc))
    {
#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
        nemR3WinInitVidIntercepts(ahMods[1]);
#endif
        for (unsigned i = 0; i < RT_ELEMENTS(g_aImports); i++)
        {
            int rc2 = RTLdrGetSymbol(ahMods[g_aImports[i].idxDll], g_aImports[i].pszName, (void **)g_aImports[i].ppfn);
            if (RT_FAILURE(rc2))
            {
                *g_aImports[i].ppfn = NULL;

                LogRel(("NEM:  %s: Failed to import %s!%s: %Rrc",
                        g_aImports[i].fOptional ? "info" : fForced ? "fatal" : "error",
                        s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName, rc2));
                if (!g_aImports[i].fOptional)
                {
                    if (RTErrInfoIsSet(pErrInfo))
                        RTErrInfoAddF(pErrInfo, rc2, ", %s!%s",
                                      s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName);
                    else
                        rc = RTErrInfoSetF(pErrInfo, rc2, "Failed to import: %s!%s",
                                           s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName);
                    Assert(RT_FAILURE(rc));
                }
            }
        }
        if (RT_SUCCESS(rc))
            Assert(!RTErrInfoIsSet(pErrInfo));
    }

    for (unsigned i = 0; i < RT_ELEMENTS(ahMods); i++)
        RTLdrClose(ahMods[i]);
    return rc;
}


/**
 * Worker for nemR3NativeInit that gets the hypervisor capabilities.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitCheckCapabilities(PVM pVM, PRTERRINFO pErrInfo)
{
#define NEM_LOG_REL_CAP_EX(a_szField, a_szFmt, a_Value)     LogRel(("NEM: %-38s= " a_szFmt "\n", a_szField, a_Value))
#define NEM_LOG_REL_CAP_SUB_EX(a_szField, a_szFmt, a_Value) LogRel(("NEM:   %36s: " a_szFmt "\n", a_szField, a_Value))
#define NEM_LOG_REL_CAP_SUB(a_szField, a_Value)             NEM_LOG_REL_CAP_SUB_EX(a_szField, "%d", a_Value)

    /*
     * Is the hypervisor present with the desired capability?
     *
     * In build 17083 this translates into:
     *      - CPUID[0x00000001].HVP is set
     *      - CPUID[0x40000000] == "Microsoft Hv"
     *      - CPUID[0x40000001].eax == "Hv#1"
     *      - CPUID[0x40000003].ebx[12] is set.
     *      - VidGetExoPartitionProperty(INVALID_HANDLE_VALUE, 0x60000, &Ignored) returns
     *        a non-zero value.
     */
    /**
     * @todo Someone at Microsoft please explain weird API design:
     *   1. Pointless CapabilityCode duplication int the output;
     *   2. No output size.
     */
    WHV_CAPABILITY Caps;
    RT_ZERO(Caps);
    SetLastError(0);
    HRESULT hrc = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &Caps, sizeof(Caps));
    DWORD   rcWin = GetLastError();
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeHypervisorPresent failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    if (!Caps.HypervisorPresent)
    {
        if (!RTPathExists(RTPATH_NT_PASSTHRU_PREFIX "Device\\VidExo"))
            return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                                 "WHvCapabilityCodeHypervisorPresent is FALSE! Make sure you have enabled the 'Windows Hypervisor Platform' feature.");
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "WHvCapabilityCodeHypervisorPresent is FALSE! (%u)", rcWin);
    }
    LogRel(("NEM: WHvCapabilityCodeHypervisorPresent is TRUE, so this might work...\n"));


    /*
     * Check what extended VM exits are supported.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapability(WHvCapabilityCodeExtendedVmExits, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeExtendedVmExits failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeExtendedVmExits", "%'#018RX64", Caps.ExtendedVmExits.AsUINT64);
    pVM->nem.s.fExtendedMsrExit   = RT_BOOL(Caps.ExtendedVmExits.X64MsrExit);
    pVM->nem.s.fExtendedCpuIdExit = RT_BOOL(Caps.ExtendedVmExits.X64CpuidExit);
    pVM->nem.s.fExtendedXcptExit  = RT_BOOL(Caps.ExtendedVmExits.ExceptionExit);
    NEM_LOG_REL_CAP_SUB("fExtendedMsrExit",   pVM->nem.s.fExtendedMsrExit);
    NEM_LOG_REL_CAP_SUB("fExtendedCpuIdExit", pVM->nem.s.fExtendedCpuIdExit);
    NEM_LOG_REL_CAP_SUB("fExtendedXcptExit",  pVM->nem.s.fExtendedXcptExit);
    if (Caps.ExtendedVmExits.AsUINT64 & ~(uint64_t)7)
        LogRel(("NEM: Warning! Unknown VM exit definitions: %#RX64\n", Caps.ExtendedVmExits.AsUINT64));
    /** @todo RECHECK: WHV_EXTENDED_VM_EXITS typedef. */

    /*
     * Check features in case they end up defining any.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapability(WHvCapabilityCodeFeatures, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeFeatures failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    if (Caps.Features.AsUINT64 & ~(uint64_t)0)
        LogRel(("NEM: Warning! Unknown feature definitions: %#RX64\n", Caps.Features.AsUINT64));
    /** @todo RECHECK: WHV_CAPABILITY_FEATURES typedef. */

    /*
     * Check that the CPU vendor is supported.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapability(WHvCapabilityCodeProcessorVendor, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorVendor failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    switch (Caps.ProcessorVendor)
    {
        /** @todo RECHECK: WHV_PROCESSOR_VENDOR typedef. */
        case WHvProcessorVendorIntel:
            NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorVendor", "%d - Intel", Caps.ProcessorVendor);
            pVM->nem.s.enmCpuVendor = CPUMCPUVENDOR_INTEL;
            break;
        case WHvProcessorVendorAmd:
            NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorVendor", "%d - AMD", Caps.ProcessorVendor);
            pVM->nem.s.enmCpuVendor = CPUMCPUVENDOR_AMD;
            break;
        default:
            NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorVendor", "%d", Caps.ProcessorVendor);
            return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Unknown processor vendor: %d", Caps.ProcessorVendor);
    }

    /*
     * CPU features, guessing these are virtual CPU features?
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapability(WHvCapabilityCodeProcessorFeatures, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorFeatures failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorFeatures", "%'#018RX64", Caps.ProcessorFeatures.AsUINT64);
#define NEM_LOG_REL_CPU_FEATURE(a_Field)    NEM_LOG_REL_CAP_SUB(#a_Field, Caps.ProcessorFeatures.a_Field)
    NEM_LOG_REL_CPU_FEATURE(Sse3Support);
    NEM_LOG_REL_CPU_FEATURE(LahfSahfSupport);
    NEM_LOG_REL_CPU_FEATURE(Ssse3Support);
    NEM_LOG_REL_CPU_FEATURE(Sse4_1Support);
    NEM_LOG_REL_CPU_FEATURE(Sse4_2Support);
    NEM_LOG_REL_CPU_FEATURE(Sse4aSupport);
    NEM_LOG_REL_CPU_FEATURE(XopSupport);
    NEM_LOG_REL_CPU_FEATURE(PopCntSupport);
    NEM_LOG_REL_CPU_FEATURE(Cmpxchg16bSupport);
    NEM_LOG_REL_CPU_FEATURE(Altmovcr8Support);
    NEM_LOG_REL_CPU_FEATURE(LzcntSupport);
    NEM_LOG_REL_CPU_FEATURE(MisAlignSseSupport);
    NEM_LOG_REL_CPU_FEATURE(MmxExtSupport);
    NEM_LOG_REL_CPU_FEATURE(Amd3DNowSupport);
    NEM_LOG_REL_CPU_FEATURE(ExtendedAmd3DNowSupport);
    NEM_LOG_REL_CPU_FEATURE(Page1GbSupport);
    NEM_LOG_REL_CPU_FEATURE(AesSupport);
    NEM_LOG_REL_CPU_FEATURE(PclmulqdqSupport);
    NEM_LOG_REL_CPU_FEATURE(PcidSupport);
    NEM_LOG_REL_CPU_FEATURE(Fma4Support);
    NEM_LOG_REL_CPU_FEATURE(F16CSupport);
    NEM_LOG_REL_CPU_FEATURE(RdRandSupport);
    NEM_LOG_REL_CPU_FEATURE(RdWrFsGsSupport);
    NEM_LOG_REL_CPU_FEATURE(SmepSupport);
    NEM_LOG_REL_CPU_FEATURE(EnhancedFastStringSupport);
    NEM_LOG_REL_CPU_FEATURE(Bmi1Support);
    NEM_LOG_REL_CPU_FEATURE(Bmi2Support);
    /* two reserved bits here, see below */
    NEM_LOG_REL_CPU_FEATURE(MovbeSupport);
    NEM_LOG_REL_CPU_FEATURE(Npiep1Support);
    NEM_LOG_REL_CPU_FEATURE(DepX87FPUSaveSupport);
    NEM_LOG_REL_CPU_FEATURE(RdSeedSupport);
    NEM_LOG_REL_CPU_FEATURE(AdxSupport);
    NEM_LOG_REL_CPU_FEATURE(IntelPrefetchSupport);
    NEM_LOG_REL_CPU_FEATURE(SmapSupport);
    NEM_LOG_REL_CPU_FEATURE(HleSupport);
    NEM_LOG_REL_CPU_FEATURE(RtmSupport);
    NEM_LOG_REL_CPU_FEATURE(RdtscpSupport);
    NEM_LOG_REL_CPU_FEATURE(ClflushoptSupport);
    NEM_LOG_REL_CPU_FEATURE(ClwbSupport);
    NEM_LOG_REL_CPU_FEATURE(ShaSupport);
    NEM_LOG_REL_CPU_FEATURE(X87PointersSavedSupport);
#undef NEM_LOG_REL_CPU_FEATURE
    if (Caps.ProcessorFeatures.AsUINT64 & (~(RT_BIT_64(43) - 1) | RT_BIT_64(27) | RT_BIT_64(28)))
        LogRel(("NEM: Warning! Unknown CPU features: %#RX64\n", Caps.ProcessorFeatures.AsUINT64));
    pVM->nem.s.uCpuFeatures.u64 = Caps.ProcessorFeatures.AsUINT64;
    /** @todo RECHECK: WHV_PROCESSOR_FEATURES typedef. */

    /*
     * The cache line flush size.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapability(WHvCapabilityCodeProcessorClFlushSize, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorClFlushSize failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorClFlushSize", "2^%u", Caps.ProcessorClFlushSize);
    if (Caps.ProcessorClFlushSize < 8 && Caps.ProcessorClFlushSize > 9)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Unsupported cache line flush size: %u", Caps.ProcessorClFlushSize);
    pVM->nem.s.cCacheLineFlushShift = Caps.ProcessorClFlushSize;

    /*
     * See if they've added more properties that we're not aware of.
     */
    /** @todo RECHECK: WHV_CAPABILITY_CODE typedef. */
    if (!IsDebuggerPresent()) /* Too noisy when in debugger, so skip. */
    {
        static const struct
        {
            uint32_t iMin, iMax; } s_aUnknowns[] =
        {
            { 0x0003, 0x000f },
            { 0x1003, 0x100f },
            { 0x2000, 0x200f },
            { 0x3000, 0x300f },
            { 0x4000, 0x400f },
        };
        for (uint32_t j = 0; j < RT_ELEMENTS(s_aUnknowns); j++)
            for (uint32_t i = s_aUnknowns[j].iMin; i <= s_aUnknowns[j].iMax; i++)
            {
                RT_ZERO(Caps);
                hrc = WHvGetCapability((WHV_CAPABILITY_CODE)i, &Caps, sizeof(Caps));
                if (SUCCEEDED(hrc))
                    LogRel(("NEM: Warning! Unknown capability %#x returning: %.*Rhxs\n", i, sizeof(Caps), &Caps));
            }
    }

#undef NEM_LOG_REL_CAP_EX
#undef NEM_LOG_REL_CAP_SUB_EX
#undef NEM_LOG_REL_CAP_SUB
    return VINF_SUCCESS;
}


/**
 * Creates and sets up a Hyper-V (exo) partition.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitCreatePartition(PVM pVM, PRTERRINFO pErrInfo)
{
    AssertReturn(!pVM->nem.s.hPartition,       RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));
    AssertReturn(!pVM->nem.s.hPartitionDevice, RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));

    /*
     * Create the partition.
     */
    WHV_PARTITION_HANDLE hPartition;
    HRESULT hrc = WHvCreatePartition(&hPartition);
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "WHvCreatePartition failed with %Rhrc (Last=%#x/%u)",
                             hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);

    int rc;

    /*
     * Set partition properties, most importantly the CPU count.
     */
    /**
     * @todo Someone at microsoft please explain another weird API:
     *  - Why this API doesn't take the WHV_PARTITION_PROPERTY_CODE value as an
     *    argument rather than as part of the struct.  That is so weird if you've
     *    used any other NT or windows API,  including WHvGetCapability().
     *  - Why use PVOID when WHV_PARTITION_PROPERTY is what's expected.  We
     *    technically only need 9 bytes for setting/getting
     *    WHVPartitionPropertyCodeProcessorClFlushSize, but the API insists on 16. */
    WHV_PARTITION_PROPERTY Property;
    RT_ZERO(Property);
    Property.PropertyCode   = WHvPartitionPropertyCodeProcessorCount;
    Property.ProcessorCount = pVM->cCpus;
    hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
    if (SUCCEEDED(hrc))
    {
        RT_ZERO(Property);
        Property.PropertyCode                  = WHvPartitionPropertyCodeExtendedVmExits;
        Property.ExtendedVmExits.X64CpuidExit  = pVM->nem.s.fExtendedCpuIdExit;
        Property.ExtendedVmExits.X64MsrExit    = pVM->nem.s.fExtendedMsrExit;
        Property.ExtendedVmExits.ExceptionExit = pVM->nem.s.fExtendedXcptExit;
        hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
        if (SUCCEEDED(hrc))
        {
            /*
             * We'll continue setup in nemR3NativeInitAfterCPUM.
             */
            pVM->nem.s.fCreatedEmts     = false;
            pVM->nem.s.hPartition       = hPartition;
            LogRel(("NEM: Created partition %p.\n", hPartition));
            return VINF_SUCCESS;
        }

        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED,
                           "Failed setting WHvPartitionPropertyCodeExtendedVmExits to %'#RX64: %Rhrc",
                           Property.ExtendedVmExits.AsUINT64, hrc);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED,
                           "Failed setting WHvPartitionPropertyCodeProcessorCount to %u: %Rhrc (Last=%#x/%u)",
                           pVM->cCpus, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);
    WHvDeletePartition(hPartition);

    Assert(!pVM->nem.s.hPartitionDevice);
    Assert(!pVM->nem.s.hPartition);
    return rc;
}


/**
 * Try initialize the native API.
 *
 * This may only do part of the job, more can be done in
 * nemR3NativeInitAfterCPUM() and nemR3NativeInitCompleted().
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   fFallback       Whether we're in fallback mode or use-NEM mode. In
 *                          the latter we'll fail if we cannot initialize.
 * @param   fForced         Whether the HMForced flag is set and we should
 *                          fail if we cannot initialize.
 */
int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    /*
     * Error state.
     * The error message will be non-empty on failure and 'rc' will be set too.
     */
    RTERRINFOSTATIC ErrInfo;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);
    int rc = nemR3WinInitProbeAndLoad(fForced, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * Check the capabilties of the hypervisor, starting with whether it's present.
         */
        rc = nemR3WinInitCheckCapabilities(pVM, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create and initialize a partition.
             */
            rc = nemR3WinInitCreatePartition(pVM, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);
                Log(("NEM: Marked active!\n"));
            }
        }
    }

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API || RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s", pErrInfo->pszMsg);

    if (RTErrInfoIsSet(pErrInfo))
        LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
    return VINF_SUCCESS;
}


/**
 * This is called after CPUMR3Init is done.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle..
 */
int nemR3NativeInitAfterCPUM(PVM pVM)
{
    /*
     * Validate sanity.
     */
    WHV_PARTITION_HANDLE hPartition = pVM->nem.s.hPartition;
    AssertReturn(hPartition != NULL, VERR_WRONG_ORDER);
    AssertReturn(!pVM->nem.s.hPartitionDevice, VERR_WRONG_ORDER);
    AssertReturn(!pVM->nem.s.fCreatedEmts, VERR_WRONG_ORDER);
    AssertReturn(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API, VERR_WRONG_ORDER);

    /*
     * Continue setting up the partition now that we've got most of the CPUID feature stuff.
     */

    /* Not sure if we really need to set the vendor. */
    WHV_PARTITION_PROPERTY Property;
    RT_ZERO(Property);
    Property.PropertyCode    = WHvPartitionPropertyCodeProcessorVendor;
    Property.ProcessorVendor = pVM->nem.s.enmCpuVendor == CPUMCPUVENDOR_AMD ? WHvProcessorVendorAmd
                             : WHvProcessorVendorIntel;
    HRESULT hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorVendor to %u: %Rhrc (Last=%#x/%u)",
                          Property.ProcessorVendor, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);

    /* Not sure if we really need to set the cache line flush size. */
    RT_ZERO(Property);
    Property.PropertyCode         = WHvPartitionPropertyCodeProcessorClFlushSize;
    Property.ProcessorClFlushSize = pVM->nem.s.cCacheLineFlushShift;
    hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorClFlushSize to %u: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.cCacheLineFlushShift, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);

    /*
     * Sync CPU features with CPUM.
     */
    /** @todo sync CPU features with CPUM. */

    /* Set the partition property. */
    RT_ZERO(Property);
    Property.PropertyCode               = WHvPartitionPropertyCodeProcessorFeatures;
    Property.ProcessorFeatures.AsUINT64 = pVM->nem.s.uCpuFeatures.u64;
    hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorFeatures to %'#RX64: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.uCpuFeatures.u64, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);

    /*
     * Set up the partition and create EMTs.
     *
     * Seems like this is where the partition is actually instantiated and we get
     * a handle to it.
     */
    hrc = WHvSetupPartition(hPartition);
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Call to WHvSetupPartition failed: %Rhrc (Last=%#x/%u)",
                          hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue);

    /* Get the handle. */
    HANDLE hPartitionDevice;
    __try
    {
        hPartitionDevice = ((HANDLE *)hPartition)[1];
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        hrc = GetExceptionCode();
        hPartitionDevice = NULL;
    }
    if (   hPartitionDevice == NULL
        || hPartitionDevice == (HANDLE)(intptr_t)-1)
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to get device handle for partition %p: %Rhrc", hPartition, hrc);
    /** @todo Do a Vid query that uses the handle to check that we've got a
     *  working value.  */
    pVM->nem.s.hPartitionDevice = hPartitionDevice;

    /*
     * Create EMTs.
     */
    VMCPUID iCpu;
    for (iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        hrc = WHvCreateVirtualProcessor(hPartition, iCpu, 0 /*fFlags*/);
        if (FAILED(hrc))
        {
            NTSTATUS const rcNtLast  = RTNtCurrentTeb()->LastStatusValue;
            DWORD const    dwErrLast = RTNtCurrentTeb()->LastErrorValue;
            while (iCpu-- > 0)
            {
                HRESULT hrc2 = WHvDeleteVirtualProcessor(hPartition, iCpu);
                AssertLogRelMsg(SUCCEEDED(hrc2), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                                  hPartition, iCpu, hrc2, RTNtCurrentTeb()->LastStatusValue,
                                                  RTNtCurrentTeb()->LastErrorValue));
            }
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                              "Call to WHvSetupPartition failed: %Rhrc (Last=%#x/%u)", hrc, rcNtLast, dwErrLast);
        }
    }
    pVM->nem.s.fCreatedEmts = true;

    LogRel(("NEM: Successfully set up partition (device handle %p)\n", hPartitionDevice));
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    NOREF(pVM); NOREF(enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    /*
     * Delete the partition.
     */
    WHV_PARTITION_HANDLE hPartition = pVM->nem.s.hPartition;
    pVM->nem.s.hPartition       = NULL;
    pVM->nem.s.hPartitionDevice = NULL;
    if (hPartition != NULL)
    {
        VMCPUID iCpu = pVM->nem.s.fCreatedEmts ? pVM->cCpus : 0;
        LogRel(("NEM: Destroying partition %p with its %u VCpus...\n", hPartition, iCpu));
        while (iCpu-- > 0)
        {
            HRESULT hrc = WHvDeleteVirtualProcessor(hPartition, iCpu);
            AssertLogRelMsg(SUCCEEDED(hrc), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                             hPartition, iCpu, hrc, RTNtCurrentTeb()->LastStatusValue,
                                             RTNtCurrentTeb()->LastErrorValue));
        }
        WHvDeletePartition(hPartition);
    }
    pVM->nem.s.fCreatedEmts = false;
    return VINF_SUCCESS;
}


void nemR3NativeReset(PVM pVM)
{
    NOREF(pVM);
}


void nemR3NativeResetCpu(PVMCPU pVCpu)
{
    NOREF(pVCpu);
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM); NOREF(pVCpu);
    return VERR_INTERNAL_ERROR_2;
}


bool nemR3NativeCanExecuteGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    return true;
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
}


DECLINLINE(int) nemR3NativeGCPhys2R3PtrReadOnly(PVM pVM, RTGCPHYS GCPhys, const void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}

DECLINLINE(int) nemR3NativeGCPhys2R3PtrWriteable(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}


int nemR3NativeNotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    LogRel(("nemR3NativeNotifyPhysRamRegister: %RGp LB %RGp\n", GCPhys, cb));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb);
    return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2)
{
    LogRel(("nemR3NativeNotifyPhysMmioExMap: %RGp LB %RGp fFlags=%#x pvMmio2=%p\n", GCPhys, cb, fFlags, pvMmio2));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags); NOREF(pvMmio2);
    return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    LogRel(("nemR3NativeNotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
    return VINF_SUCCESS;
}


/**
 * Called early during ROM registration, right after the pages have been
 * allocated and the RAM range updated.
 *
 * This will be succeeded by a number of NEMHCNotifyPhysPageProtChanged() calls
 * and finally a NEMR3NotifyPhysRomRegisterEarly().
 *
 * @returns VBox status code
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          The ROM address (page aligned).
 * @param   cb              The size (page aligned).
 * @param   fFlags          NEM_NOTIFY_PHYS_ROM_F_XXX.
 */
int nemR3NativeNotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    LogRel(("nemR3NativeNotifyPhysRomRegisterEarly: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
#if 0 /* Let's not do this after all.  We'll protection change notifications for each page and if not we'll map them lazily. */
    RTGCPHYS const cPages = cb >> X86_PAGE_SHIFT;
    for (RTGCPHYS iPage = 0; iPage < cPages; iPage++, GCPhys += X86_PAGE_SIZE)
    {
        const void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrReadOnly(pVM, GCPhys, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, (void *)pvPage, GCPhys, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute);
            if (SUCCEEDED(hrc))
            { /* likely */ }
            else
            {
                LogRel(("nemR3NativeNotifyPhysRomRegisterEarly: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                        GCPhys, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
                return VERR_NEM_INIT_FAILED;
            }
        }
        else
        {
            LogRel(("nemR3NativeNotifyPhysRomRegisterEarly: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
            return rc;
        }
    }
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb);
#endif
    RT_NOREF_PV(fFlags);
    return VINF_SUCCESS;
}


/**
 * Called after the ROM range has been fully completed.
 *
 * This will be preceeded by a NEMR3NotifyPhysRomRegisterEarly() call as well a
 * number of NEMHCNotifyPhysPageProtChanged calls.
 *
 * @returns VBox status code
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          The ROM address (page aligned).
 * @param   cb              The size (page aligned).
 * @param   fFlags          NEM_NOTIFY_PHYS_ROM_F_XXX.
 */
int nemR3NativeNotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    LogRel(("nemR3NativeNotifyPhysRomRegisterLate: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
    return VINF_SUCCESS;
}


void nemR3NativeNotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    LogRel(("nemR3NativeNotifySetA20: fEnabled=%RTbool\n", fEnabled));
    NOREF(pVCpu); NOREF(fEnabled);
}


void nemR3NativeNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    LogRel(("nemR3NativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb);
}


void nemR3NativeNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                int fRestoreAsRAM, bool fRestoreAsRAM2)
{
    LogRel(("nemR3NativeNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d fRestoreAsRAM=%d fRestoreAsRAM2=%d\n",
            GCPhys, cb, enmKind, fRestoreAsRAM, fRestoreAsRAM2));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb); NOREF(fRestoreAsRAM); NOREF(fRestoreAsRAM2);
}


void nemR3NativeNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    LogRel(("nemR3NativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
            GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhysOld); NOREF(GCPhysNew); NOREF(cb); NOREF(fRestoreAsRAM);
}


static int nemR3NativeSetPhysPage(PVM pVM, RTGCPHYS GCPhys, uint32_t fPageProt, uint8_t *pu2State, bool fBackingChanged)
{
    /*
     * Looks like we need to unmap a page before we can change the backing
     * or even modify the protection.  This is going to be *REALLY* efficient.
     * PGM lends us two bits to keep track of the state here.
     */
    uint8_t const u2OldState = *pu2State;
    uint8_t const u2NewState = fPageProt & NEM_PAGE_PROT_WRITE ? NEM_WIN_PAGE_STATE_WRITABLE
                             : fPageProt & NEM_PAGE_PROT_READ  ? NEM_WIN_PAGE_STATE_READABLE : NEM_WIN_PAGE_STATE_NOT_SET;
    if (   fBackingChanged
        || u2NewState != u2OldState)
    {
        if (u2OldState > NEM_WIN_PAGE_STATE_NOT_SET)
        {
            HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_NOT_SET;
                if (u2NewState == NEM_WIN_PAGE_STATE_NOT_SET)
                    return VINF_SUCCESS;
            }
            else
            {
                LogRel(("nemR3NativeSetPhysPage/unmap: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                        GCPhys, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
                return VERR_NEM_INIT_FAILED;
            }
        }
    }

    /*
     * Writeable mapping?
     */
    if (fPageProt & NEM_PAGE_PROT_WRITE)
    {
        void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrWriteable(pVM, GCPhys, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvPage, GCPhys, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute | WHvMapGpaRangeFlagWrite);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
                return VINF_SUCCESS;
            }
            LogRel(("nemR3NativeSetPhysPage/writable: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemR3NativeSetPhysPage/writable: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
        return rc;
    }

    if (fPageProt & NEM_PAGE_PROT_READ)
    {
        const void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrReadOnly(pVM, GCPhys, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, (void *)pvPage, GCPhys, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_READABLE;
                return VINF_SUCCESS;
            }
            LogRel(("nemR3NativeSetPhysPage/readonly: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemR3NativeSetPhysPage/readonly: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
        return rc;
    }

    /* We already unmapped it above. */
    *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

    return nemR3NativeSetPhysPage(pVM, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
}


void nemR3NativeNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                          PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

    nemR3NativeSetPhysPage(pVM, GCPhys, fPageProt, pu2State, false /*fBackingChanged*/);
}


void nemR3NativeNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                      uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageProtChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhysPrev); RT_NOREF_PV(HCPhysNew); RT_NOREF_PV(enmType);

    nemR3NativeSetPhysPage(pVM, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
}

