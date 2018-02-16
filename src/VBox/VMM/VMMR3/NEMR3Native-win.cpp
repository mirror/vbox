/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Windows backend.
 *
 * Log group 2: Exit logging.
 * Log group 3: Log context on exit.
 * Log group 12: API intercepts.
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
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
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

/** @name Our two-bit physical page state for PGMPAGE
 * @{ */
#define NEM_WIN_PAGE_STATE_NOT_SET      0
#define NEM_WIN_PAGE_STATE_UNMAPPED     1
#define NEM_WIN_PAGE_STATE_READABLE     2
#define NEM_WIN_PAGE_STATE_WRITABLE     3
/** @} */

/** Checks if a_GCPhys is subject to the limited A20 gate emulation.  */
#define NEM_WIN_IS_SUBJECT_TO_A20(a_GCPhys)     ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K)

/** Checks if a_GCPhys is relevant to the limited A20 gate emulation.  */
#define NEM_WIN_IS_RELEVANT_TO_A20(a_GCPhys)    \
    ( ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K) || ((RTGCPHYS)(a_GCPhys) < (RTGCPHYS)_64K) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** WHvRegisterInterruptState layout, reconstructed from the v7.1 DDK. */
typedef union MISSINGINTERUPTSTATEREG
{
    /** 64-bit view. */
    uint64_t au64[2];
    struct /* unamed */
    {
        uint64_t fInterruptShadow : 1;
        uint64_t fNmiMasked : 2;
        uint64_t uReserved0 : 61;
        uint64_t uReserved1;
    };
} MISSINGINTERUPTSTATEREG;
AssertCompileSize(MISSINGINTERUPTSTATEREG, 16);

/** Used by MISSINGPENDINGINTERRUPTIONREG. */
typedef enum MISSINGPENDINGINTERRUPTIONTYPE
{
    kPendingIntType_Interrupt = 0,
    kPendingIntType_Nmi,
    kPendingIntType_Xcpt,
    kPendingIntType_Dunno,
    kPendingIntType_SoftwareInterrupt
} MISSINGPENDINGINTERRUPTIONTYPE;

/** WHvRegisterPendingInterruption layout, reconstructed from the v7.1 DDK. */
typedef union MISSINGPENDINGINTERRUPTIONREG
{
    /** 64-bit view. */
    uint64_t au64[2];
    struct /* unamed */
    {
        uint32_t fInterruptionPending : 1;
        uint32_t enmInterruptionType : 3; /**< MISSINGPENDINGINTERRUPTIONTYPE */
        uint32_t fDeliverErrCd : 1;
        uint32_t fUnknown0 : 1;
        uint32_t fUnknown1 : 1; /**< Observed set when software interrupt was issued. */
        uint32_t uReserved0 : 9;
        uint32_t InterruptionVector : 16;
        uint32_t uErrCd;
        uint64_t uReserved1;
    };
} MISSINGPENDINGINTERRUPTIONREG;
AssertCompileSize(MISSINGPENDINGINTERRUPTIONREG, 16);


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

/** NEM_WIN_PAGE_STATE_XXX names. */
static const char * const g_apszPageStates[4] = { "not-set", "unmapped", "readable", "writable" };
/** WHV_MEMORY_ACCESS_TYPE names */
static const char * const g_apszWHvMemAccesstypes[4] = { "read", "write", "exec", "!undefined!" };


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int nemR3NativeSetPhysPage(PVM pVM, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst, uint32_t fPageProt,
                                  uint8_t *pu2State, bool fBackingChanged);


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
        Log12(("VID!NtDeviceIoControlFile: hFile=%#zx pIos=%p->{s:%#x, i:%#zx} uFunction=%#x Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
               hFile, pIos, pIos->Status, pIos->Information, uFunction, pvInput, cbInput, pvOutput, cbOutput, rcNt, ASMReturnAddress()));
    else
        Log12(("VID!NtDeviceIoControlFile: hFile=%#zx hEvt=%#zx Apc=%p/%p pIos=%p->{s:%#x, i:%#zx} uFunction=%#x Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
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
     * @todo Someone at Microsoft please explain another weird API:
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


/**
 * VM reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
void nemR3NativeReset(PVM pVM)
{
    /* Unfix the A20 gate. */
    pVM->nem.s.fA20Fixed = false;
}


/**
 * Reset CPU due to INIT IPI or hot (un)plugging.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the CPU being
 *                      reset.
 * @param   fInitIpi    Whether this is the INIT IPI or hot (un)plugging case.
 */
void nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi)
{
    /* Lock the A20 gate if INIT IPI, make sure it's enabled.  */
    if (fInitIpi && pVCpu->idCpu > 0)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (!pVM->nem.s.fA20Enabled)
            nemR3NativeNotifySetA20(pVCpu, true);
        pVM->nem.s.fA20Enabled = true;
        pVM->nem.s.fA20Fixed   = true;
    }
}


static int nemR3WinCopyStateToHyperV(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    WHV_REGISTER_NAME  aenmNames[128];
    WHV_REGISTER_VALUE aValues[128];

    /* GPRs */
    aenmNames[0]      = WHvX64RegisterRax;
    aValues[0].Reg64  = pCtx->rax;
    aenmNames[1]      = WHvX64RegisterRcx;
    aValues[1].Reg64  = pCtx->rcx;
    aenmNames[2]      = WHvX64RegisterRdx;
    aValues[2].Reg64  = pCtx->rdx;
    aenmNames[3]      = WHvX64RegisterRbx;
    aValues[3].Reg64  = pCtx->rbx;
    aenmNames[4]      = WHvX64RegisterRsp;
    aValues[4].Reg64  = pCtx->rsp;
    aenmNames[5]      = WHvX64RegisterRbp;
    aValues[5].Reg64  = pCtx->rbp;
    aenmNames[6]      = WHvX64RegisterRsi;
    aValues[6].Reg64  = pCtx->rsi;
    aenmNames[7]      = WHvX64RegisterRdi;
    aValues[7].Reg64  = pCtx->rdi;
    aenmNames[8]      = WHvX64RegisterR8;
    aValues[8].Reg64  = pCtx->r8;
    aenmNames[9]      = WHvX64RegisterR9;
    aValues[9].Reg64  = pCtx->r9;
    aenmNames[10]     = WHvX64RegisterR10;
    aValues[10].Reg64 = pCtx->r10;
    aenmNames[11]     = WHvX64RegisterR11;
    aValues[11].Reg64 = pCtx->r11;
    aenmNames[12]     = WHvX64RegisterR12;
    aValues[12].Reg64 = pCtx->r12;
    aenmNames[13]     = WHvX64RegisterR13;
    aValues[13].Reg64 = pCtx->r13;
    aenmNames[14]     = WHvX64RegisterR14;
    aValues[14].Reg64 = pCtx->r14;
    aenmNames[15]     = WHvX64RegisterR15;
    aValues[15].Reg64 = pCtx->r15;

    /* RIP & Flags */
    aenmNames[16]     = WHvX64RegisterRip;
    aValues[16].Reg64 = pCtx->rip;
    aenmNames[17]     = WHvX64RegisterRflags;
    aValues[17].Reg64 = pCtx->rflags.u;

    /* Segments */
#define COPY_OUT_SEG(a_idx, a_enmName, a_SReg) \
        do { \
            aenmNames[a_idx]                  = a_enmName; \
            aValues[a_idx].Segment.Base       = (a_SReg).u64Base; \
            aValues[a_idx].Segment.Limit      = (a_SReg).u32Limit; \
            aValues[a_idx].Segment.Selector   = (a_SReg).Sel; \
            aValues[a_idx].Segment.Attributes = (a_SReg).Attr.u; \
        } while (0)
    COPY_OUT_SEG(18, WHvX64RegisterEs,   pCtx->es);
    COPY_OUT_SEG(19, WHvX64RegisterCs,   pCtx->cs);
    COPY_OUT_SEG(20, WHvX64RegisterSs,   pCtx->ss);
    COPY_OUT_SEG(21, WHvX64RegisterDs,   pCtx->ds);
    COPY_OUT_SEG(22, WHvX64RegisterFs,   pCtx->fs);
    COPY_OUT_SEG(23, WHvX64RegisterGs,   pCtx->gs);
    COPY_OUT_SEG(24, WHvX64RegisterLdtr, pCtx->ldtr);
    COPY_OUT_SEG(25, WHvX64RegisterTr,   pCtx->tr);

    uintptr_t iReg = 26;
    /* Descriptor tables. */
    aenmNames[iReg] = WHvX64RegisterIdtr;
    aValues[iReg].Table.Limit = pCtx->idtr.cbIdt;
    aValues[iReg].Table.Base  = pCtx->idtr.pIdt;
    iReg++;
    aenmNames[iReg] = WHvX64RegisterGdtr;
    aValues[iReg].Table.Limit = pCtx->gdtr.cbGdt;
    aValues[iReg].Table.Base  = pCtx->gdtr.pGdt;
    iReg++;

    /* Control registers. */
    aenmNames[iReg]     = WHvX64RegisterCr0;
    aValues[iReg].Reg64 = pCtx->cr0;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr2;
    aValues[iReg].Reg64 = pCtx->cr2;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr3;
    aValues[iReg].Reg64 = pCtx->cr3;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr4;
    aValues[iReg].Reg64 = pCtx->cr4;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr8;
    aValues[iReg].Reg64 = CPUMGetGuestCR8(pVCpu);
    iReg++;

    /* Debug registers. */
/** @todo fixme. Figure out what the hyper-v version of KVM_SET_GUEST_DEBUG would be. */
    aenmNames[iReg]     = WHvX64RegisterDr0;
    //aValues[iReg].Reg64 = CPUMGetHyperDR0(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[0];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr1;
    //aValues[iReg].Reg64 = CPUMGetHyperDR1(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr2;
    //aValues[iReg].Reg64 = CPUMGetHyperDR2(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[2];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr3;
    //aValues[iReg].Reg64 = CPUMGetHyperDR3(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[3];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr6;
    //aValues[iReg].Reg64 = CPUMGetHyperDR6(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[6];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr7;
    //aValues[iReg].Reg64 = CPUMGetHyperDR7(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[7];
    iReg++;

    /* Vector state. */
    aenmNames[iReg]     = WHvX64RegisterXmm0;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm1;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm2;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm3;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm4;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm5;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm6;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm7;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm8;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm9;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm10;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm11;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm12;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm13;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm14;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm15;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi;
    iReg++;

    /* Floating point state. */
    aenmNames[iReg]     = WHvX64RegisterFpMmx0;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[0].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[0].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx1;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[1].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[1].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx2;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[2].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[2].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx3;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[3].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[3].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx4;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[4].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[4].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx5;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[5].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[5].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx6;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[6].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[6].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx7;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[7].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[7].au64[1];
    iReg++;

    aenmNames[iReg]     = WHvX64RegisterFpControlStatus;
    aValues[iReg].FpControlStatus.FpControl = pCtx->pXStateR3->x87.FCW;
    aValues[iReg].FpControlStatus.FpStatus  = pCtx->pXStateR3->x87.FSW;
    aValues[iReg].FpControlStatus.FpTag     = pCtx->pXStateR3->x87.FTW;
    aValues[iReg].FpControlStatus.Reserved  = pCtx->pXStateR3->x87.FTW >> 8;
    aValues[iReg].FpControlStatus.LastFpOp  = pCtx->pXStateR3->x87.FOP;
    aValues[iReg].FpControlStatus.LastFpRip = (pCtx->pXStateR3->x87.FPUIP)
                                            | ((uint64_t)pCtx->pXStateR3->x87.CS << 32)
                                            | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd1 << 48);
    iReg++;

    aenmNames[iReg]     = WHvX64RegisterXmmControlStatus;
    aValues[iReg].XmmControlStatus.LastFpRdp            = (pCtx->pXStateR3->x87.FPUDP)
                                                        | ((uint64_t)pCtx->pXStateR3->x87.DS << 32)
                                                        | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd2 << 48);
    aValues[iReg].XmmControlStatus.XmmStatusControl     = pCtx->pXStateR3->x87.MXCSR;
    aValues[iReg].XmmControlStatus.XmmStatusControlMask = pCtx->pXStateR3->x87.MXCSR_MASK; /** @todo ??? (Isn't this an output field?) */
    iReg++;

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    aenmNames[iReg]     = WHvX64RegisterEfer;
    aValues[iReg].Reg64 = pCtx->msrEFER;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterKernelGsBase;
    aValues[iReg].Reg64 = pCtx->msrKERNELGSBASE;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterApicBase;
    aValues[iReg].Reg64 = APICGetBaseMsrNoCheck(pVCpu);
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterPat;
    aValues[iReg].Reg64 = pCtx->msrPAT;
    iReg++;
    /// @todo WHvX64RegisterSysenterCs
    /// @todo WHvX64RegisterSysenterEip
    /// @todo WHvX64RegisterSysenterEsp
    aenmNames[iReg]     = WHvX64RegisterStar;
    aValues[iReg].Reg64 = pCtx->msrSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterLstar;
    aValues[iReg].Reg64 = pCtx->msrLSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCstar;
    aValues[iReg].Reg64 = pCtx->msrCSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterSfmask;
    aValues[iReg].Reg64 = pCtx->msrSFMASK;
    iReg++;

    /* event injection (always clear it). */
    /** @todo Someone at microsoft please explain why HV_X64_PENDING_INTERRUPTION_REGISTER
     * and HV_X64_INTERRUPT_STATE_REGISTER are missing from the headers.  Ditto for
     * wathever structures WHvRegisterPendingEvent0/1 uses.   */
    aenmNames[iReg]     = WHvRegisterPendingInterruption;
    aValues[iReg].Reg64 = 0;
    iReg++;
    /// @todo WHvRegisterInterruptState
    /// @todo WHvRegisterPendingEvent0
    /// @todo WHvRegisterPendingEvent1

    /*
     * Set the registers.
     */
    Assert(iReg < RT_ELEMENTS(aValues));
    Assert(iReg < RT_ELEMENTS(aenmNames));
#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
    Log12(("Calling WHvSetVirtualProcessorRegisters(%p, %u, %p, %u, %p)\n",
           pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, iReg, aValues));
#endif
    HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, iReg, aValues);
    if (SUCCEEDED(hrc))
        return VINF_SUCCESS;
    AssertLogRelMsgFailed(("WHvSetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, iReg,
                           hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
    return VERR_INTERNAL_ERROR;
}

static int nemR3WinCopyStateFromHyperV(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    WHV_REGISTER_NAME  aenmNames[128];

    /* GPRs */
    aenmNames[0]  = WHvX64RegisterRax;
    aenmNames[1]  = WHvX64RegisterRcx;
    aenmNames[2]  = WHvX64RegisterRdx;
    aenmNames[3]  = WHvX64RegisterRbx;
    aenmNames[4]  = WHvX64RegisterRsp;
    aenmNames[5]  = WHvX64RegisterRbp;
    aenmNames[6]  = WHvX64RegisterRsi;
    aenmNames[7]  = WHvX64RegisterRdi;
    aenmNames[8]  = WHvX64RegisterR8;
    aenmNames[9]  = WHvX64RegisterR9;
    aenmNames[10] = WHvX64RegisterR10;
    aenmNames[11] = WHvX64RegisterR11;
    aenmNames[12] = WHvX64RegisterR12;
    aenmNames[13] = WHvX64RegisterR13;
    aenmNames[14] = WHvX64RegisterR14;
    aenmNames[15] = WHvX64RegisterR15;

    /* RIP & Flags */
    aenmNames[16] = WHvX64RegisterRip;
    aenmNames[17] = WHvX64RegisterRflags;

    /* Segments */
    aenmNames[18] = WHvX64RegisterEs;
    aenmNames[19] = WHvX64RegisterCs;
    aenmNames[20] = WHvX64RegisterSs;
    aenmNames[21] = WHvX64RegisterDs;
    aenmNames[22] = WHvX64RegisterFs;
    aenmNames[23] = WHvX64RegisterGs;
    aenmNames[24] = WHvX64RegisterLdtr;
    aenmNames[25] = WHvX64RegisterTr;

    /* Descriptor tables. */
    aenmNames[26] = WHvX64RegisterIdtr;
    aenmNames[27] = WHvX64RegisterGdtr;

    /* Control registers. */
    aenmNames[28] = WHvX64RegisterCr0;
    aenmNames[29] = WHvX64RegisterCr2;
    aenmNames[30] = WHvX64RegisterCr3;
    aenmNames[31] = WHvX64RegisterCr4;
    aenmNames[32] = WHvX64RegisterCr8;

    /* Debug registers. */
    aenmNames[33] = WHvX64RegisterDr0;
    aenmNames[34] = WHvX64RegisterDr1;
    aenmNames[35] = WHvX64RegisterDr2;
    aenmNames[36] = WHvX64RegisterDr3;
    aenmNames[37] = WHvX64RegisterDr6;
    aenmNames[38] = WHvX64RegisterDr7;

    /* Vector state. */
    aenmNames[39] = WHvX64RegisterXmm0;
    aenmNames[40] = WHvX64RegisterXmm1;
    aenmNames[41] = WHvX64RegisterXmm2;
    aenmNames[42] = WHvX64RegisterXmm3;
    aenmNames[43] = WHvX64RegisterXmm4;
    aenmNames[44] = WHvX64RegisterXmm5;
    aenmNames[45] = WHvX64RegisterXmm6;
    aenmNames[46] = WHvX64RegisterXmm7;
    aenmNames[47] = WHvX64RegisterXmm8;
    aenmNames[48] = WHvX64RegisterXmm9;
    aenmNames[49] = WHvX64RegisterXmm10;
    aenmNames[50] = WHvX64RegisterXmm11;
    aenmNames[51] = WHvX64RegisterXmm12;
    aenmNames[52] = WHvX64RegisterXmm13;
    aenmNames[53] = WHvX64RegisterXmm14;
    aenmNames[54] = WHvX64RegisterXmm15;

    /* Floating point state. */
    aenmNames[55] = WHvX64RegisterFpMmx0;
    aenmNames[56] = WHvX64RegisterFpMmx1;
    aenmNames[57] = WHvX64RegisterFpMmx2;
    aenmNames[58] = WHvX64RegisterFpMmx3;
    aenmNames[59] = WHvX64RegisterFpMmx4;
    aenmNames[60] = WHvX64RegisterFpMmx5;
    aenmNames[61] = WHvX64RegisterFpMmx6;
    aenmNames[62] = WHvX64RegisterFpMmx7;
    aenmNames[63] = WHvX64RegisterFpControlStatus;
    aenmNames[64] = WHvX64RegisterXmmControlStatus;

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    aenmNames[65] = WHvX64RegisterEfer;
    aenmNames[66] = WHvX64RegisterKernelGsBase;
    aenmNames[67] = WHvX64RegisterApicBase;
    aenmNames[68] = WHvX64RegisterPat;
    aenmNames[69] = WHvX64RegisterSysenterCs;
    aenmNames[70] = WHvX64RegisterSysenterEip;
    aenmNames[71] = WHvX64RegisterSysenterEsp;
    aenmNames[72] = WHvX64RegisterStar;
    aenmNames[73] = WHvX64RegisterLstar;
    aenmNames[74] = WHvX64RegisterCstar;
    aenmNames[75] = WHvX64RegisterSfmask;

    /* event injection */
    aenmNames[76] = WHvRegisterPendingInterruption;
    aenmNames[77] = WHvRegisterInterruptState;
    aenmNames[78] = WHvRegisterInterruptState;
    aenmNames[79] = WHvRegisterPendingEvent0;
    aenmNames[80] = WHvRegisterPendingEvent1;
    unsigned const cRegs = 81;

    /*
     * Get the registers.
     */
    WHV_REGISTER_VALUE aValues[cRegs];
    RT_ZERO(aValues);
    Assert(RT_ELEMENTS(aValues) >= cRegs);
    Assert(RT_ELEMENTS(aenmNames) >= cRegs);
#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
    Log12(("Calling WHvGetVirtualProcessorRegisters(%p, %u, %p, %u, %p)\n",
          pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, cRegs, aValues));
#endif
    HRESULT hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, cRegs, aValues);
    if (SUCCEEDED(hrc))
    {
        /* GPRs */
        Assert(aenmNames[0]  == WHvX64RegisterRax);
        Assert(aenmNames[15] == WHvX64RegisterR15);
        pCtx->rax = aValues[0].Reg64;
        pCtx->rcx = aValues[1].Reg64;
        pCtx->rdx = aValues[2].Reg64;
        pCtx->rbx = aValues[3].Reg64;
        pCtx->rsp = aValues[4].Reg64;
        pCtx->rbp = aValues[5].Reg64;
        pCtx->rsi = aValues[6].Reg64;
        pCtx->rdi = aValues[7].Reg64;
        pCtx->r8  = aValues[8].Reg64;
        pCtx->r9  = aValues[9].Reg64;
        pCtx->r10 = aValues[10].Reg64;
        pCtx->r11 = aValues[11].Reg64;
        pCtx->r12 = aValues[12].Reg64;
        pCtx->r13 = aValues[13].Reg64;
        pCtx->r14 = aValues[14].Reg64;
        pCtx->r15 = aValues[15].Reg64;

        /* RIP & Flags */
        Assert(aenmNames[16] == WHvX64RegisterRip);
        pCtx->rip      = aValues[16].Reg64;
        pCtx->rflags.u = aValues[17].Reg64;

        /* Segments */
#define COPY_BACK_SEG(a_idx, a_enmName, a_SReg) \
            do { \
                Assert(aenmNames[a_idx] == a_enmName); \
                (a_SReg).u64Base  = aValues[a_idx].Segment.Base; \
                (a_SReg).u32Limit = aValues[a_idx].Segment.Limit; \
                (a_SReg).ValidSel = (a_SReg).Sel = aValues[a_idx].Segment.Selector; \
                (a_SReg).Attr.u   = aValues[a_idx].Segment.Attributes; \
                (a_SReg).fFlags   = CPUMSELREG_FLAGS_VALID; \
            } while (0)
        COPY_BACK_SEG(18, WHvX64RegisterEs,   pCtx->es);
        COPY_BACK_SEG(19, WHvX64RegisterCs,   pCtx->cs);
        COPY_BACK_SEG(20, WHvX64RegisterSs,   pCtx->ss);
        COPY_BACK_SEG(21, WHvX64RegisterDs,   pCtx->ds);
        COPY_BACK_SEG(22, WHvX64RegisterFs,   pCtx->fs);
        COPY_BACK_SEG(23, WHvX64RegisterGs,   pCtx->gs);
        COPY_BACK_SEG(24, WHvX64RegisterLdtr, pCtx->ldtr);
        COPY_BACK_SEG(25, WHvX64RegisterTr,   pCtx->tr);

        /* Descriptor tables. */
        Assert(aenmNames[26] == WHvX64RegisterIdtr);
        pCtx->idtr.cbIdt = aValues[26].Table.Limit;
        pCtx->idtr.pIdt  = aValues[26].Table.Base;
        Assert(aenmNames[27] == WHvX64RegisterGdtr);
        pCtx->gdtr.cbGdt = aValues[27].Table.Limit;
        pCtx->gdtr.pGdt  = aValues[27].Table.Base;

        /* Control registers. */
        Assert(aenmNames[28] == WHvX64RegisterCr0);
        bool fMaybeChangedMode = false;
        bool fFlushTlb         = false;
        bool fFlushGlobalTlb   = false;
        if (pCtx->cr0 != aValues[28].Reg64)
        {
            CPUMSetGuestCR0(pVCpu, aValues[28].Reg64);
            fMaybeChangedMode = true;
            fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
        }
        Assert(aenmNames[29] == WHvX64RegisterCr2);
        pCtx->cr2 = aValues[29].Reg64;
        if (pCtx->cr3 != aValues[30].Reg64)
        {
            CPUMSetGuestCR3(pVCpu, aValues[30].Reg64);
            fFlushTlb = true;
        }
        if (pCtx->cr4 != aValues[31].Reg64)
        {
            CPUMSetGuestCR4(pVCpu, aValues[31].Reg64);
            fMaybeChangedMode = true;
            fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
        }
        APICSetTpr(pVCpu, (uint8_t)aValues[32].Reg64 << 4);

        /* Debug registers. */
        Assert(aenmNames[33] == WHvX64RegisterDr0);
    /** @todo fixme */
        if (pCtx->dr[0] != aValues[33].Reg64)
            CPUMSetGuestDR0(pVCpu, aValues[33].Reg64);
        if (pCtx->dr[1] != aValues[34].Reg64)
            CPUMSetGuestDR1(pVCpu, aValues[34].Reg64);
        if (pCtx->dr[2] != aValues[35].Reg64)
            CPUMSetGuestDR2(pVCpu, aValues[35].Reg64);
        if (pCtx->dr[3] != aValues[36].Reg64)
            CPUMSetGuestDR3(pVCpu, aValues[36].Reg64);
        Assert(aenmNames[37] == WHvX64RegisterDr6);
        Assert(aenmNames[38] == WHvX64RegisterDr7);
        if (pCtx->dr[6] != aValues[37].Reg64)
            CPUMSetGuestDR6(pVCpu, aValues[37].Reg64);
        if (pCtx->dr[7] != aValues[38].Reg64)
            CPUMSetGuestDR6(pVCpu, aValues[38].Reg64);

        /* Vector state. */
        Assert(aenmNames[39] == WHvX64RegisterXmm0);
        Assert(aenmNames[54] == WHvX64RegisterXmm15);
        pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Lo  = aValues[39].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Hi  = aValues[39].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Lo  = aValues[40].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Hi  = aValues[40].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Lo  = aValues[41].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Hi  = aValues[41].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Lo  = aValues[42].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Hi  = aValues[42].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Lo  = aValues[43].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Hi  = aValues[43].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Lo  = aValues[44].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Hi  = aValues[44].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Lo  = aValues[45].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Hi  = aValues[45].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Lo  = aValues[46].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Hi  = aValues[46].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Lo  = aValues[47].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Hi  = aValues[47].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Lo  = aValues[48].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Hi  = aValues[48].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo = aValues[49].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi = aValues[49].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo = aValues[50].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi = aValues[50].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo = aValues[51].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi = aValues[51].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo = aValues[52].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi = aValues[52].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo = aValues[53].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi = aValues[53].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo = aValues[54].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi = aValues[54].Reg128.High64;

        /* Floating point state. */
        Assert(aenmNames[55] == WHvX64RegisterFpMmx0);
        Assert(aenmNames[62] == WHvX64RegisterFpMmx7);
        pCtx->pXStateR3->x87.aRegs[0].au64[0] = aValues[55].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[0].au64[1] = aValues[55].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[1].au64[0] = aValues[56].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[1].au64[1] = aValues[56].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[2].au64[0] = aValues[57].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[2].au64[1] = aValues[57].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[3].au64[0] = aValues[58].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[3].au64[1] = aValues[58].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[4].au64[0] = aValues[59].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[4].au64[1] = aValues[59].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[5].au64[0] = aValues[60].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[5].au64[1] = aValues[60].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[6].au64[0] = aValues[61].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[6].au64[1] = aValues[61].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[7].au64[0] = aValues[62].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[7].au64[1] = aValues[62].Fp.AsUINT128.High64;

        Assert(aenmNames[63] == WHvX64RegisterFpControlStatus);
        pCtx->pXStateR3->x87.FCW        = aValues[63].FpControlStatus.FpControl;
        pCtx->pXStateR3->x87.FSW        = aValues[63].FpControlStatus.FpStatus;
        pCtx->pXStateR3->x87.FTW        = aValues[63].FpControlStatus.FpTag
                                        /*| (aValues[63].FpControlStatus.Reserved << 8)*/;
        pCtx->pXStateR3->x87.FOP        = aValues[63].FpControlStatus.LastFpOp;
        pCtx->pXStateR3->x87.FPUIP      = (uint32_t)aValues[63].FpControlStatus.LastFpRip;
        pCtx->pXStateR3->x87.CS         = (uint16_t)(aValues[63].FpControlStatus.LastFpRip >> 32);
        pCtx->pXStateR3->x87.Rsrvd1     = (uint16_t)(aValues[63].FpControlStatus.LastFpRip >> 48);

        Assert(aenmNames[64] == WHvX64RegisterXmmControlStatus);
        pCtx->pXStateR3->x87.FPUDP      = (uint32_t)aValues[64].XmmControlStatus.LastFpRdp;
        pCtx->pXStateR3->x87.DS         = (uint16_t)(aValues[64].XmmControlStatus.LastFpRdp >> 32);
        pCtx->pXStateR3->x87.Rsrvd2     = (uint16_t)(aValues[64].XmmControlStatus.LastFpRdp >> 48);
        pCtx->pXStateR3->x87.MXCSR      = aValues[64].XmmControlStatus.XmmStatusControl;
        pCtx->pXStateR3->x87.MXCSR_MASK = aValues[64].XmmControlStatus.XmmStatusControlMask; /** @todo ??? (Isn't this an output field?) */

        /* MSRs */
        // WHvX64RegisterTsc - don't touch
        Assert(aenmNames[65] == WHvX64RegisterEfer);
        if (aValues[65].Reg64 != pCtx->msrEFER)
        {
            pCtx->msrEFER = aValues[65].Reg64;
            fMaybeChangedMode = true;
        }

        Assert(aenmNames[66] == WHvX64RegisterKernelGsBase);
        pCtx->msrKERNELGSBASE = aValues[66].Reg64;

        Assert(aenmNames[67] == WHvX64RegisterApicBase);
        if (aValues[67].Reg64 != APICGetBaseMsrNoCheck(pVCpu))
        {
            VBOXSTRICTRC rc2 = APICSetBaseMsr(pVCpu, aValues[67].Reg64);
            Assert(rc2 == VINF_SUCCESS); NOREF(rc2);
        }

        Assert(aenmNames[68] == WHvX64RegisterPat);
        pCtx->msrPAT    = aValues[68].Reg64;
        /// @todo WHvX64RegisterSysenterCs
        /// @todo WHvX64RegisterSysenterEip
        /// @todo WHvX64RegisterSysenterEsp
        Assert(aenmNames[72] == WHvX64RegisterStar);
        pCtx->msrSTAR   = aValues[72].Reg64;
        Assert(aenmNames[73] == WHvX64RegisterLstar);
        pCtx->msrLSTAR  = aValues[73].Reg64;
        Assert(aenmNames[74] == WHvX64RegisterCstar);
        pCtx->msrCSTAR  = aValues[74].Reg64;
        Assert(aenmNames[75] == WHvX64RegisterSfmask);
        pCtx->msrSFMASK = aValues[75].Reg64;

        /// @todo WHvRegisterPendingInterruption
        Assert(aenmNames[76] == WHvRegisterPendingInterruption);
        /** @todo Someone at microsoft please explain why HV_X64_PENDING_INTERRUPTION_REGISTER
         * and HV_X64_INTERRUPT_STATE_REGISTER are missing from the headers.  Ditto for
         * wathever structures WHvRegisterPendingEvent0/1 uses.   */
        MISSINGPENDINGINTERRUPTIONREG const * pPendingInt = (MISSINGPENDINGINTERRUPTIONREG const *)&aValues[76];
        if (pPendingInt->fInterruptionPending)
        {
            Log6(("PendingInterruption: type=%u vector=%#x errcd=%RTbool/%#x unk0=%u unk1=%u\n",
                  pPendingInt->enmInterruptionType, pPendingInt->InterruptionVector, pPendingInt->fDeliverErrCd,
                  pPendingInt->uErrCd, pPendingInt->fUnknown0, pPendingInt->fUnknown1));
            AssertMsg(pPendingInt->uReserved0 == 0 && pPendingInt->uReserved1 == 0,
                      ("%#RX64 %#RX64\n", pPendingInt->au64[0], pPendingInt->au64[1]));
        }

        /// @todo WHvRegisterInterruptState
        /// @todo WHvRegisterPendingEvent0
        /// @todo WHvRegisterPendingEvent1


        if (fMaybeChangedMode)
        {
            int rc = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
            AssertRC(rc);
        }
        if (fFlushTlb)
        {
            int rc = PGMFlushTLB(pVCpu, pCtx->cr3, fFlushGlobalTlb);
            AssertRC(rc);
        }

        return VINF_SUCCESS;
    }

    AssertLogRelMsgFailed(("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, cRegs,
                           hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
    return VERR_INTERNAL_ERROR;
}


#ifdef LOG_ENABLED
/**
 * Log the full details of an exit reason.
 *
 * @param   pExitReason     The exit reason to log.
 */
static void nemR3WinLogExitReason(WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    bool fExitCtx = false;
    bool fExitInstr = false;
    switch (pExitReason->ExitReason)
    {
        case WHvRunVpExitReasonMemoryAccess:
            Log2(("Exit: Memory access: GCPhys=%RGp GCVirt=%RGv %s %s %s\n",
                  pExitReason->MemoryAccess.Gpa, pExitReason->MemoryAccess.Gva,
                  g_apszWHvMemAccesstypes[pExitReason->MemoryAccess.AccessInfo.AccessType],
                  pExitReason->MemoryAccess.AccessInfo.GpaUnmapped ? "unmapped" : "mapped",
                  pExitReason->MemoryAccess.AccessInfo.GvaValid    ? "" : "invalid-gc-virt"));
            AssertMsg(!(pExitReason->MemoryAccess.AccessInfo.AsUINT32 & ~UINT32_C(0xf)),
                      ("MemoryAccess.AccessInfo=%#x\n", pExitReason->MemoryAccess.AccessInfo.AsUINT32));
            fExitCtx = fExitInstr = true;
            break;

        case WHvRunVpExitReasonX64IoPortAccess:
            Log2(("Exit: I/O port access: IoPort=%#x LB %u %s%s%s rax=%#RX64 rcx=%#RX64 rsi=%#RX64 rdi=%#RX64\n",
                  pExitReason->IoPortAccess.PortNumber,
                  pExitReason->IoPortAccess.AccessInfo.AccessSize,
                  pExitReason->IoPortAccess.AccessInfo.IsWrite ? "out" : "in",
                  pExitReason->IoPortAccess.AccessInfo.StringOp ? " string" : "",
                  pExitReason->IoPortAccess.AccessInfo.RepPrefix ? " rep" : "",
                  pExitReason->IoPortAccess.Rax,
                  pExitReason->IoPortAccess.Rcx,
                  pExitReason->IoPortAccess.Rsi,
                  pExitReason->IoPortAccess.Rdi));
            Log2(("Exit: + ds=%#x:{%#RX64 LB %#RX32, %#x}  es=%#x:{%#RX64 LB %#RX32, %#x}\n",
                  pExitReason->IoPortAccess.Ds.Selector,
                  pExitReason->IoPortAccess.Ds.Base,
                  pExitReason->IoPortAccess.Ds.Limit,
                  pExitReason->IoPortAccess.Ds.Attributes,
                  pExitReason->IoPortAccess.Es.Selector,
                  pExitReason->IoPortAccess.Es.Base,
                  pExitReason->IoPortAccess.Es.Limit,
                  pExitReason->IoPortAccess.Es.Attributes ));

            AssertMsg(   pExitReason->IoPortAccess.AccessInfo.AccessSize == 1
                      || pExitReason->IoPortAccess.AccessInfo.AccessSize == 2
                      || pExitReason->IoPortAccess.AccessInfo.AccessSize == 4,
                      ("IoPortAccess.AccessInfo.AccessSize=%d\n", pExitReason->IoPortAccess.AccessInfo.AccessSize));
            AssertMsg(!(pExitReason->IoPortAccess.AccessInfo.AsUINT32 & ~UINT32_C(0x3f)),
                      ("IoPortAccess.AccessInfo=%#x\n", pExitReason->IoPortAccess.AccessInfo.AsUINT32));
            fExitCtx = fExitInstr = true;
            break;

# if 0
        case WHvRunVpExitReasonUnrecoverableException:
        case WHvRunVpExitReasonInvalidVpRegisterValue:
        case WHvRunVpExitReasonUnsupportedFeature:
        case WHvRunVpExitReasonX64InterruptWindow:
        case WHvRunVpExitReasonX64Halt:
        case WHvRunVpExitReasonX64MsrAccess:
        case WHvRunVpExitReasonX64Cpuid:
        case WHvRunVpExitReasonException:
        case WHvRunVpExitReasonCanceled:
        case WHvRunVpExitReasonAlerted:
            WHV_X64_MSR_ACCESS_CONTEXT MsrAccess;
            WHV_X64_CPUID_ACCESS_CONTEXT CpuidAccess;
            WHV_VP_EXCEPTION_CONTEXT VpException;
            WHV_X64_INTERRUPTION_DELIVERABLE_CONTEXT InterruptWindow;
            WHV_UNRECOVERABLE_EXCEPTION_CONTEXT UnrecoverableException;
            WHV_X64_UNSUPPORTED_FEATURE_CONTEXT UnsupportedFeature;
            WHV_RUN_VP_CANCELED_CONTEXT CancelReason;
#endif

        case WHvRunVpExitReasonNone:
            Log2(("Exit: No reason\n"));
            AssertFailed();
            break;

        default:
            Log(("Exit: %#x\n", pExitReason->ExitReason));
            break;
    }

    /*
     * Context and maybe instruction details.
     */
    if (fExitCtx)
    {
        const WHV_VP_EXIT_CONTEXT *pVpCtx = &pExitReason->IoPortAccess.VpContext;
        Log2(("Exit: + CS:RIP=%04x:%08RX64 RFLAGS=%06RX64 cbInstr=%u CS={%RX64 L %#RX32, %#x}\n",
              pVpCtx->Cs.Selector,
              pVpCtx->Rip,
              pVpCtx->Rflags,
              pVpCtx->InstructionLength,
              pVpCtx->Cs.Base, pVpCtx->Cs.Limit, pVpCtx->Cs.Attributes));
        Log2(("Exit: + cpl=%d CR0.PE=%d CR0.AM=%d EFER.LMA=%d DebugActive=%d InterruptionPending=%d InterruptShadow=%d\n",
              pVpCtx->ExecutionState.Cpl,
              pVpCtx->ExecutionState.Cr0Pe,
              pVpCtx->ExecutionState.Cr0Am,
              pVpCtx->ExecutionState.EferLma,
              pVpCtx->ExecutionState.DebugActive,
              pVpCtx->ExecutionState.InterruptionPending,
              pVpCtx->ExecutionState.InterruptShadow));
        AssertMsg(!(pVpCtx->ExecutionState.AsUINT16 & ~UINT16_C(0x107f)),
                  ("ExecutionState.AsUINT16=%#x\n", pVpCtx->ExecutionState.AsUINT16));

        /** @todo Someone at Microsoft please explain why the InstructionBytes fields
         * are 16 bytes long, when 15 would've been sufficent and saved 3-7 bytes of
         * alignment padding?  Intel max length is 15, so is this sSome ARM stuff?
         * Aren't ARM
         * instructions max 32-bit wide?  Confused. */
        if (fExitInstr && pExitReason->IoPortAccess.InstructionByteCount > 0)
            Log2(("Exit: + Instruction %.*Rhxs\n",
                  pExitReason->IoPortAccess.InstructionByteCount, pExitReason->IoPortAccess.InstructionBytes));
    }
}


/**
 * Logs the current CPU state.
 */
static void nemR3WinLogState(PVM pVM, PVMCPU pVCpu)
{
    if (LogIs3Enabled())
    {
        char szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                        "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                        "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                        "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                        "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                        "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                        "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                        "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                        "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                        "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                        "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                        "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                        "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                        "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                        "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                        "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                        "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                        "        efer=%016VR{efer}\n"
                        "         pat=%016VR{pat}\n"
                        "     sf_mask=%016VR{sf_mask}\n"
                        "krnl_gs_base=%016VR{krnl_gs_base}\n"
                        "       lstar=%016VR{lstar}\n"
                        "        star=%016VR{star} cstar=%016VR{cstar}\n"
                        "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                        );

        char szInstr[256];
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
        Log3(("%s%s\n", szRegs, szInstr));
    }
}

#endif /* LOG_ENABLED */


/**
 * Advances the guest RIP and clear EFLAGS.RF.
 *
 * This may clear VMCPU_FF_INHIBIT_INTERRUPTS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The CPU context to update.
 * @param   pExitCtx        The exit context.
 */
DECLINLINE(void) nemR3WinAdvanceGuestRipAndClearRF(PVMCPU pVCpu, PCPUMCTX pCtx, WHV_VP_EXIT_CONTEXT const *pExitCtx)
{
    /* Advance the RIP. */
    Assert(pExitCtx->InstructionLength > 0 && pExitCtx->InstructionLength < 16);
    pCtx->rip += pExitCtx->InstructionLength;
    pCtx->rflags.Bits.u1RF = 0;

    /* Update interrupt inhibition. */
    if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    { /* likely */ }
    else if (pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}


static VBOXSTRICTRC nemR3WinHandleHalt(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    LogFlow(("nemR3WinHandleHalt\n"));
    return VINF_EM_HALT;
}

static DECLCALLBACK(int) nemR3WinUnmapOnePageCallback(PVM pVM, RTGCPHYS GCPhys, uint8_t *pu2NemState, void *pvUser)
{
    RT_NOREF_PV(pvUser);
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
        *pu2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
    else
    {
        LogRel(("nemR3WinUnmapOnePageCallback: GCPhys=%RGp %s hrc=%Rhrc (%#x) Last=%#x/%u (cMappedPages=%u)\n",
                GCPhys, g_apszPageStates[*pu2NemState], hrc, hrc, RTNtCurrentTeb()->LastStatusValue,
                RTNtCurrentTeb()->LastErrorValue, pVM->nem.s.cMappedPages));
        *pu2NemState = NEM_WIN_PAGE_STATE_NOT_SET;
    }
    if (pVM->nem.s.cMappedPages > 0)
        ASMAtomicDecU32(&pVM->nem.s.cMappedPages);
    return VINF_SUCCESS;
}


/**
 * State to pass between  nemR3WinHandleMemoryAccess and
 * nemR3WinHandleMemoryAccessPageCheckerCallback.
 */
typedef struct NEMR3WINHMACPCCSTATE
{
    /** Input: Write access. */
    bool    fWriteAccess;
    /** Output: Set if we did something. */
    bool    fDidSomething;
    /** Output: Set it we should resume. */
    bool    fCanResume;
} NEMR3WINHMACPCCSTATE;

/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE,
 *      Worker for nemR3WinHandleMemoryAccess, pvUser points to a
 *      NEMR3WINHMACPCCSTATE structure. }
 */
static DECLCALLBACK(int) nemR3WinHandleMemoryAccessPageCheckerCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys,
                                                                       PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    NEMR3WINHMACPCCSTATE *pState = (NEMR3WINHMACPCCSTATE *)pvUser;
    pState->fDidSomething = false;
    pState->fCanResume    = false;

    /* If A20 is disabled, we may need to make another query on the masked
       page to get the correct protection information. */
    uint8_t  u2State = pInfo->u2NemState;
    RTGCPHYS GCPhysSrc;
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        GCPhysSrc = GCPhys;
    else
    {
        GCPhysSrc = GCPhys & ~(RTGCPHYS)RT_BIT_32(20);
        PGMPHYSNEMPAGEINFO Info2;
        int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, GCPhysSrc, pState->fWriteAccess, &Info2, NULL, NULL);
        AssertRCReturn(rc, rc);

        *pInfo = Info2;
        pInfo->u2NemState = u2State;
    }

    /*
     * I'm not sure WTF was going on, but I ended up in a loop if I remapped a
     * readonly page as writable.  Specifically, this was an issue with the big
     * VRAM mapping at 0xe0000000 when booing DSL 4.4.1.  So, in a hope to work
     * around that we no longer pre-map anything, just unmap stuff and do it
     * lazily here.  And here we will first unmap, restart, and then remap with
     * new protection or backing.
     */
    switch (u2State)
    {
        case NEM_WIN_PAGE_STATE_UNMAPPED:
        case NEM_WIN_PAGE_STATE_NOT_SET:
        {
            if (pInfo->fNemProt == NEM_PAGE_PROT_NONE)
            {
                Log4(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp - #1\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Don't bother remapping it if it's a write request to a non-writable page. */
            if (   pState->fWriteAccess
                && !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE))
            {
                Log4(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp - #1w\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Map the page. */
            int rc = nemR3NativeSetPhysPage(pVM,
                                            GCPhysSrc & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                            GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                            pInfo->fNemProt,
                                            &u2State,
                                            true /*fBackingState*/);
            pInfo->u2NemState = u2State;
            Log4(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp - synced => %s + %Rrc\n",
                  GCPhys, g_apszPageStates[u2State], rc));
            RT_NOREF(pVCpu);
            pState->fDidSomething = true;
            pState->fCanResume    = true;
            return rc;
        }

        case NEM_WIN_PAGE_STATE_READABLE:
            if (   !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
                && (pInfo->fNemProt & (NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE)))
            {
                Log4(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp - #2\n", GCPhys));
                return VINF_SUCCESS;
            }
            break;

        case NEM_WIN_PAGE_STATE_WRITABLE:
            if (pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
            {
                Log4(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp - #3\n", GCPhys));
                return VINF_SUCCESS;
            }
            break;

        default:
            AssertLogRelMsgFailedReturn(("u2State=%#x\n", u2State), VERR_INTERNAL_ERROR_3);
    }

    /*
     * Unmap and restart the instruction.
     * If this fails, which it does every so often, just unmap everything for now.
     */
    /** @todo figure out whether we mess up the state or if it's WHv.   */
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
    {
        pState->fDidSomething = true;
        pState->fCanResume    = true;
        pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
        Log5(("nemR3WinHandleMemoryAccessPageCheckerCallback: %RGp => unmapped[%s]\n", GCPhys, g_apszPageStates[u2State]));
        return VINF_SUCCESS;
    }
    LogRel(("nemR3WinHandleMemoryAccessPageCheckerCallback/unmap: GCPhysDst=%RGp %s hrc=%Rhrc (%#x) Last=%#x/%u (cMappedPages=%u)\n",
            GCPhys, g_apszPageStates[u2State], hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue,
            pVM->nem.s.cMappedPages));

    PGMPhysNemEnumPagesByState(pVM, NEM_WIN_PAGE_STATE_READABLE, nemR3WinUnmapOnePageCallback, NULL);
    Log(("nemR3WinHandleMemoryAccessPageCheckerCallback: Unmapped all (cMappedPages=%u)\n", pVM->nem.s.cMappedPages));

    pState->fDidSomething = true;
    pState->fCanResume    = true;
    pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
}


/**
 * Handles an memory access VMEXIT.
 *
 * This can be triggered by a number of things.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The CPU context to update.
 * @param   pMemCtx         The exit reason information.
 */
static VBOXSTRICTRC nemR3WinHandleMemoryAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_MEMORY_ACCESS_CONTEXT const *pMemCtx)
{
    /*
     * Ask PGM for information about the given GCPhys.  We need to check if we're
     * out of sync first.
     */
    NEMR3WINHMACPCCSTATE State = { pMemCtx->AccessInfo.AccessType == WHvMemoryAccessWrite, false, false };
    PGMPHYSNEMPAGEINFO   Info;
    int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, pMemCtx->Gpa, State.fWriteAccess, &Info,
                                       nemR3WinHandleMemoryAccessPageCheckerCallback, &State);
    if (RT_SUCCESS(rc))
    {
        if (Info.fNemProt & (pMemCtx->AccessInfo.AccessType == WHvMemoryAccessWrite ? NEM_PAGE_PROT_WRITE : NEM_PAGE_PROT_READ))
        {
            if (State.fCanResume)
            {
                Log4(("MemExit: %RGp (=>%RHp) %s fProt=%u%s%s%s; restarting (%s)\n",
                      pMemCtx->Gpa, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
                      Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
                      State.fDidSomething ? "" : " no-change", g_apszWHvMemAccesstypes[pMemCtx->AccessInfo.AccessType]));
                return VINF_SUCCESS;
            }
        }
        Log4(("MemExit: %RGp (=>%RHp) %s fProt=%u%s%s%s; emulating (%s)\n",
              pMemCtx->Gpa, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
              Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
              State.fDidSomething ? "" : " no-change", g_apszWHvMemAccesstypes[pMemCtx->AccessInfo.AccessType]));
    }
    else
        Log4(("MemExit: %RGp rc=%Rrc%s; emulating (%s)\n", pMemCtx->Gpa, rc,
              State.fDidSomething ? " modified-backing" : "", g_apszWHvMemAccesstypes[pMemCtx->AccessInfo.AccessType]));

    /*
     * Emulate the memory access, either access handler or special memory.
     */
    VBOXSTRICTRC rcStrict;
    if (pMemCtx->InstructionByteCount > 0)
        rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pMemCtx->VpContext.Rip,
                                                pMemCtx->InstructionBytes, pMemCtx->InstructionByteCount);
    else
        rcStrict = IEMExecOne(pVCpu);
    /** @todo do we need to do anything wrt debugging here?   */
    return rcStrict;
}


/**
 * Handles an I/O port access VMEXIT.
 *
 * We ASSUME that the hypervisor has don't I/O port access control.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The CPU context to update.
 * @param   pIoPortCtx      The exit reason information.
 */
static VBOXSTRICTRC nemR3WinHandleIoPortAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx,
                                               WHV_X64_IO_PORT_ACCESS_CONTEXT const *pIoPortCtx)
{
    Assert(   pIoPortCtx->AccessInfo.AccessSize == 1
           || pIoPortCtx->AccessInfo.AccessSize == 2
           || pIoPortCtx->AccessInfo.AccessSize == 4);

    VBOXSTRICTRC rcStrict;
    if (!pIoPortCtx->AccessInfo.StringOp)
    {
        /*
         * Simple port I/O.
         */
        Assert(pCtx->rax == pIoPortCtx->Rax);

        static uint32_t const s_fAndMask[8] =
        { UINT32_MAX, UINT32_C(0xff), UINT32_C(0xffff), UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
        uint32_t const fAndMask = s_fAndMask[pIoPortCtx->AccessInfo.AccessSize];
        if (pIoPortCtx->AccessInfo.IsWrite)
        {
            rcStrict = IOMIOPortWrite(pVM, pVCpu, pIoPortCtx->PortNumber, (uint32_t)pIoPortCtx->Rax & fAndMask,
                                      pIoPortCtx->AccessInfo.AccessSize);
            if (IOM_SUCCESS(rcStrict))
                nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pIoPortCtx->VpContext);
        }
        else
        {
            uint32_t uValue = 0;
            rcStrict = IOMIOPortRead(pVM, pVCpu, pIoPortCtx->PortNumber, &uValue,
                                     pIoPortCtx->AccessInfo.AccessSize);
            if (IOM_SUCCESS(rcStrict))
            {
                pCtx->eax = (pCtx->eax & ~fAndMask) | (uValue & fAndMask);
                nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pIoPortCtx->VpContext);
            }
        }
    }
    else
    {
        /*
         * String port I/O.
         */
        /** @todo Someone at Microsoft please explain how we can get the address mode
         * from the IoPortAccess.VpContext.  CS.Attributes is only sufficient for
         * getting the default mode, it can always be overridden by a prefix.   This
         * forces us to interpret the instruction from opcodes, which is suboptimal.
         * Both AMD-V and VT-x includes the address size in the exit info, at least on
         * CPUs that are reasonably new. */
        Assert(   pIoPortCtx->Ds.Base     == pCtx->ds.u64Base
               && pIoPortCtx->Ds.Limit    == pCtx->ds.u32Limit
               && pIoPortCtx->Ds.Selector == pCtx->ds.Sel);
        Assert(   pIoPortCtx->Es.Base     == pCtx->es.u64Base
               && pIoPortCtx->Es.Limit    == pCtx->es.u32Limit
               && pIoPortCtx->Es.Selector == pCtx->es.Sel);
        Assert(pIoPortCtx->Rdi == pCtx->rdi);
        Assert(pIoPortCtx->Rsi == pCtx->rsi);
        Assert(pIoPortCtx->Rcx == pCtx->rcx);
        Assert(pIoPortCtx->Rcx == pCtx->rcx);

        rcStrict = IEMExecOne(pVCpu);
    }
    if (IOM_SUCCESS(rcStrict))
    {
        /*
         * Do debug checks.
         */
        if (   pIoPortCtx->VpContext.ExecutionState.DebugActive /** @todo Microsoft: Does DebugActive this only reflext DR7? */
            || (pIoPortCtx->VpContext.Rflags & X86_EFL_TF)
            || DBGFBpIsHwIoArmed(pVM) )
        {
            /** @todo Debugging. */
        }
    }
    return rcStrict;
}


static VBOXSTRICTRC nemR3WinHandleInterruptWindow(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleMsrAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleCpuId(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleException(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleUD(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleTripleFault(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinHandleInvalidState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
    {
        Log3(("nemR3NativeRunGC: Entering #%u\n", pVCpu->idCpu));
        nemR3WinLogState(pVM, pVCpu);
    }
#endif

    /*
     * The run loop.
     *
     * Current approach to state updating to use the sledgehammer and sync
     * everything every time.  This will be optimized later.
     */
    const bool   fSingleStepping = false; /** @todo get this from somewhere. */
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Copy the state.
         */
        PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
        int rc2 = nemR3WinCopyStateToHyperV(pVM, pVCpu, pCtx);
        AssertRCBreakStmt(rc2, rcStrict = rc2);

        /*
         * Run a bit.
         */
        WHV_RUN_VP_EXIT_CONTEXT ExitReason;
        RT_ZERO(ExitReason);
        if (   !VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED);
            HRESULT hrc = WHvRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, &ExitReason, sizeof(ExitReason));
            VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM);
            AssertLogRelMsgBreakStmt(SUCCEEDED(hrc),
                                     ("WHvRunVirtualProcessor(%p, %u,,) -> %Rhrc (Last=%#x/%u)\n", pVM->nem.s.hPartition, pVCpu->idCpu,
                                      hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue),
                                     rcStrict = VERR_INTERNAL_ERROR);
            Log2(("WHvRunVirtualProcessor -> %#x; exit code %#x (%d)\n", hrc, ExitReason.ExitReason, ExitReason.ExitReason));
        }
        else
        {
            LogFlow(("nemR3NativeRunGC: returning: pending FF (pre exec)\n"));
            break;
        }

        /*
         * Copy back the state.
         */
        rc2 = nemR3WinCopyStateFromHyperV(pVM, pVCpu, pCtx);
        AssertRCBreakStmt(rc2, rcStrict = rc2);

#ifdef LOG_ENABLED
        /*
         * Do some logging.
         */
        if (LogIs2Enabled())
            nemR3WinLogExitReason(&ExitReason);
        if (LogIs3Enabled())
            nemR3WinLogState(pVM, pVCpu);
#endif

#ifdef VBOX_STRICT
        /* Assert that the VpContext field makes sense. */
        switch (ExitReason.ExitReason)
        {
            case WHvRunVpExitReasonMemoryAccess:
            case WHvRunVpExitReasonX64IoPortAccess:
            case WHvRunVpExitReasonX64MsrAccess:
            case WHvRunVpExitReasonX64Cpuid:
            case WHvRunVpExitReasonException:
            case WHvRunVpExitReasonUnrecoverableException:
                Assert(   ExitReason.IoPortAccess.VpContext.InstructionLength > 0
                       || (   ExitReason.ExitReason == WHvRunVpExitReasonMemoryAccess
                           && ExitReason.MemoryAccess.AccessInfo.AccessType == WHvMemoryAccessExecute));
                Assert(ExitReason.IoPortAccess.VpContext.InstructionLength < 16);
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.Cpl == CPUMGetGuestCPL(pVCpu));
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.Cr0Pe == RT_BOOL(pCtx->cr0 & X86_CR0_PE));
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.Cr0Am == RT_BOOL(pCtx->cr0 & X86_CR0_AM));
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.EferLma == RT_BOOL(pCtx->msrEFER & MSR_K6_EFER_LMA));
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.DebugActive == RT_BOOL(pCtx->dr[7] & X86_DR7_ENABLED_MASK));
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.Reserved0 == 0);
                Assert(ExitReason.IoPortAccess.VpContext.ExecutionState.Reserved1 == 0);
                Assert(ExitReason.IoPortAccess.VpContext.Rip == pCtx->rip);
                Assert(ExitReason.IoPortAccess.VpContext.Rflags == pCtx->rflags.u);
                Assert(   ExitReason.IoPortAccess.VpContext.Cs.Base     == pCtx->cs.u64Base
                       && ExitReason.IoPortAccess.VpContext.Cs.Limit    == pCtx->cs.u32Limit
                       && ExitReason.IoPortAccess.VpContext.Cs.Selector == pCtx->cs.Sel);
                break;
            default: break; /* shut up compiler. */
        }
#endif

        /*
         * Deal with the exit.
         */
        switch (ExitReason.ExitReason)
        {
            /* Frequent exits: */
            case WHvRunVpExitReasonCanceled:
            case WHvRunVpExitReasonAlerted:
                rcStrict = VINF_SUCCESS;
                break;

            case WHvRunVpExitReasonX64Halt:
                rcStrict = nemR3WinHandleHalt(pVM, pVCpu, pCtx);
                break;

            case WHvRunVpExitReasonMemoryAccess:
                rcStrict = nemR3WinHandleMemoryAccess(pVM, pVCpu, pCtx, &ExitReason.MemoryAccess);
                break;

            case WHvRunVpExitReasonX64IoPortAccess:
                rcStrict = nemR3WinHandleIoPortAccess(pVM, pVCpu, pCtx, &ExitReason.IoPortAccess);
                break;

            case WHvRunVpExitReasonX64InterruptWindow:
                rcStrict = nemR3WinHandleInterruptWindow(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonX64MsrAccess: /* needs configuring */
                rcStrict = nemR3WinHandleMsrAccess(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonX64Cpuid: /* needs configuring */
                rcStrict = nemR3WinHandleCpuId(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonException: /* needs configuring */
                rcStrict = nemR3WinHandleException(pVM, pVCpu, pCtx, &ExitReason);
                break;

            /* Unlikely exits: */
            case WHvRunVpExitReasonUnsupportedFeature:
                rcStrict = nemR3WinHandleUD(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonUnrecoverableException:
                rcStrict = nemR3WinHandleTripleFault(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonInvalidVpRegisterValue:
                rcStrict = nemR3WinHandleInvalidState(pVM, pVCpu, pCtx, &ExitReason);
                break;

            /* Undesired exits: */
            case WHvRunVpExitReasonNone:
            default:
                AssertLogRelMsgFailed(("Unknown ExitReason: %#x\n", ExitReason.ExitReason));
                rcStrict = VERR_INTERNAL_ERROR_3;
                break;
        }
        if (rcStrict != VINF_SUCCESS)
        {
            LogFlow(("nemR3NativeRunGC: returning: %Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
            break;
        }

        /* Hack alert! */
        uint32_t const cMappedPages = pVM->nem.s.cMappedPages;
        if (cMappedPages < 4000)
        { /* likely */ }
        else
        {
            PGMPhysNemEnumPagesByState(pVM, NEM_WIN_PAGE_STATE_READABLE, nemR3WinUnmapOnePageCallback, NULL);
            Log(("nemR3NativeRunGC: Unmapped all; cMappedPages=%u -> %u\n", cMappedPages, pVM->nem.s.cMappedPages));
        }

        /* If any FF is pending, return to the EM loops.  That's okay for the
           current sledgehammer approach. */
        if (   VM_FF_IS_PENDING(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
            || VMCPU_FF_IS_PENDING(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
        {
            LogFlow(("nemR3NativeRunGC: returning: pending FF (%#x / %#x)\n", pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions));
            break;
        }
    }

    return rcStrict;
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


/**
 * Forced flag notification call from VMEmt.h.
 *
 * This is only called when pVCpu is in the VMCPUSTATE_STARTED_EXEC_NEM state.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the CPU
 *                          to be notified.
 * @param   fFlags          Notification flags, VMNOTIFYFF_FLAGS_XXX.
 */
void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    HRESULT hrc = WHvCancelRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, 0);
    AssertMsg(SUCCEEDED(hrc), ("WHvCancelRunVirtualProcessor -> hrc=%Rhrc\n", hrc));

    RT_NOREF_PV(hrc);
    RT_NOREF_PV(fFlags);
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


/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE}
 */
static DECLCALLBACK(int) nemR3WinUnsetForA20CheckerCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys,
                                                            PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    Assert(pVCpu == NULL);

    /* We'll just unmap the memory. */
    if (pInfo->u2NemState > NEM_WIN_PAGE_STATE_UNMAPPED)
    {
        HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
        if (SUCCEEDED(hrc))
            pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
        else
        {
            LogRel(("nemR3WinUnsetForA20CheckerCallback/unmap: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
            return VERR_INTERNAL_ERROR_2;
        }
    }
    RT_NOREF(pVCpu, pvUser);
    return VINF_SUCCESS;
}


/**
 * Unmaps a page from Hyper-V for the purpose of emulating A20 gate behavior.
 *
 * @returns The PGMPhysNemQueryPageInfo result.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.  Optional.
 * @param   GCPhys          The page to unmap.
 */
static int nemR3WinUnmapPageForA20Gate(PVM pVM, RTGCPHYS GCPhys)
{
    PGMPHYSNEMPAGEINFO Info;
    return PGMPhysNemPageInfoChecker(pVM, NULL /*pVCpu*/, GCPhys, false /*fMakeWritable*/, &Info,
                                     nemR3WinUnsetForA20CheckerCallback, NULL);
}


/**
 * Called when the A20 state changes.
 *
 * Hyper-V doesn't seem to offer a simple way of implementing the A20 line
 * features of PCs.  So, we do a very minimal emulation of the HMA to make DOS
 * happy.
 *
 * @param   pVCpu           The CPU the A20 state changed on.
 * @param   fEnabled        Whether it was enabled (true) or disabled.
 */
void nemR3NativeNotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("nemR3NativeNotifySetA20: fEnabled=%RTbool\n", fEnabled));
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!pVM->nem.s.fA20Fixed)
    {
        pVM->nem.s.fA20Enabled = fEnabled;
        for (RTGCPHYS GCPhys = _1M; GCPhys < _1M + _64K; GCPhys += X86_PAGE_SIZE)
            nemR3WinUnmapPageForA20Gate(pVM, GCPhys);
    }
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


/**
 * Worker that maps pages into Hyper-V.
 *
 * This is used by the PGM physical page notifications as well as the memory
 * access VMEXIT handlers.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   GCPhysSrc       The source page address.
 * @param   GCPhysDst       The hyper-V destination page.  This may differ from
 *                          GCPhysSrc when A20 is disabled.
 * @param   fPageProt       NEM_PAGE_PROT_XXX.
 * @param   pu2State        Our page state (input/output).
 * @param   fBackingChanged Set if the page backing is being changed.
 */
static int nemR3NativeSetPhysPage(PVM pVM, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst, uint32_t fPageProt,
                                  uint8_t *pu2State, bool fBackingChanged)
{
    /*
     * Looks like we need to unmap a page before we can change the backing
     * or even modify the protection.  This is going to be *REALLY* efficient.
     * PGM lends us two bits to keep track of the state here.
     */
    uint8_t const u2OldState = *pu2State;
    uint8_t const u2NewState = fPageProt & NEM_PAGE_PROT_WRITE ? NEM_WIN_PAGE_STATE_WRITABLE
                             : fPageProt & NEM_PAGE_PROT_READ  ? NEM_WIN_PAGE_STATE_READABLE : NEM_WIN_PAGE_STATE_UNMAPPED;
    if (   fBackingChanged
        || u2NewState != u2OldState)
    {
        if (u2OldState > NEM_WIN_PAGE_STATE_UNMAPPED)
        {
            HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhysDst, X86_PAGE_SIZE);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
                uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                if (u2NewState == NEM_WIN_PAGE_STATE_UNMAPPED)
                {
                    Log5(("nemR3NativeSetPhysPage: %RGp => unmapped (total %u)\n", GCPhysDst, cMappedPages));
                    return VINF_SUCCESS;
                }
            }
            else
            {
                LogRel(("nemR3NativeSetPhysPage/unmap: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                        GCPhysDst, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
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
        int rc = nemR3NativeGCPhys2R3PtrWriteable(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvPage, GCPhysDst, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute | WHvMapGpaRangeFlagWrite);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("nemR3NativeSetPhysPage: %RGp => writable (total %u)\n", GCPhysDst, cMappedPages));
                return VINF_SUCCESS;
            }
            LogRel(("nemR3NativeSetPhysPage/writable: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhysDst, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemR3NativeSetPhysPage/writable: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
    }

    if (fPageProt & NEM_PAGE_PROT_READ)
    {
        const void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrReadOnly(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, (void *)pvPage, GCPhysDst, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_READABLE;
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("nemR3NativeSetPhysPage: %RGp => read+exec (total %u)\n", GCPhysDst, cMappedPages));
                return VINF_SUCCESS;
            }
            LogRel(("nemR3NativeSetPhysPage/readonly: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhysDst, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemR3NativeSetPhysPage/readonly: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
    }

    /* We already unmapped it above. */
    *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
}


static int nemR3JustUnmapPageFromHyperV(PVM pVM, RTGCPHYS GCPhysDst, uint8_t *pu2State)
{
    if (*pu2State <= NEM_WIN_PAGE_STATE_UNMAPPED)
    {
        Log5(("nemR3JustUnmapPageFromHyperV: %RGp == unmapped\n", GCPhysDst));
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
        return VINF_SUCCESS;
    }

    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhysDst, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
    {
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
        Log5(("nemR3JustUnmapPageFromHyperV: %RGp => unmapped (total %u)\n", GCPhysDst, cMappedPages));
        return VINF_SUCCESS;
    }
    LogRel(("nemR3JustUnmapPageFromHyperV(%RGp): failed! hrc=%Rhrc (%#x) Last=%#x/%u\n",
            GCPhysDst, hrc, hrc, RTNtCurrentTeb()->LastStatusValue, RTNtCurrentTeb()->LastErrorValue));
    return VERR_INTERNAL_ERROR_3;
}


int nemR3NativeNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

    int rc;
#if 0
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        rc = nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        rc = nemR3WinUnmapPageForA20Gate(pVM, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys) && RT_SUCCESS(rc))
            rc = nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);

    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        rc = nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        rc = nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else
        rc = VINF_SUCCESS; /* ignore since we've got the alias page at this address. */
#endif
    return rc;
}


void nemR3NativeNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                          PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

#if 0
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, false /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        nemR3WinUnmapPageForA20Gate(pVM, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
            nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, false /*fBackingChanged*/);
    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    /* else: ignore since we've got the alias page at this address. */
#endif
}


void nemR3NativeNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                      uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    LogRel(("nemR3NativeNotifyPhysPageProtChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
            GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhysPrev); RT_NOREF_PV(HCPhysNew); RT_NOREF_PV(enmType);

#if 0
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        nemR3WinUnmapPageForA20Gate(pVM, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
            nemR3NativeSetPhysPage(pVM, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    /* else: ignore since we've got the alias page at this address. */
#endif
}

