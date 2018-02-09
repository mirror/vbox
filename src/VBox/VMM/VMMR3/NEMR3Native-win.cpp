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
#include <WinHvPlatform.h>

#ifndef _WIN32_WINNT_WIN10
# error "Missing _WIN32_WINNT_WIN10"
#endif
#ifndef _WIN32_WINNT_WIN10_RS1 /* Missing define, causing trouble for us. */
# define _WIN32_WINNT_WIN10_RS1 (_WIN32_WINNT_WIN10 + 1)
#endif
#include <sysinfoapi.h>
#include <fileapi.h>
#include <errhandlingapi.h>
#include <winerror.h> /* no api header for this. */

#include <VBox/vmm/nem.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>

#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>


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



int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    /*
     * Error state.
     *
     * The error message will be non-empty on failure, 'rc' may or  may not
     * be set.  Early API detection failures will not set 'rc', so we'll sort
     * that out at the other end of the function.
     */
    RTERRINFOSTATIC ErrInfo;
    int             rc = VINF_SUCCESS;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);

    /*
     * Check that the DLL files we need are present, but without loading them.
     * We'd like to avoid loading them unnecessarily.
     */
    WCHAR wszPath[MAX_PATH + 64];
    UINT  cwcPath = GetSystemDirectoryW(wszPath, MAX_PATH);
    if (cwcPath >= MAX_PATH || cwcPath < 2)
        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "GetSystemDirectoryW failed (%#x / %u)", cwcPath, GetLastError());
    else
    {
        if (wszPath[cwcPath - 1] != '\\' || wszPath[cwcPath - 1] != '/')
            wszPath[cwcPath++] = '\\';
        RTUtf16CopyAscii(&wszPath[cwcPath], RT_ELEMENTS(wszPath) - cwcPath, "WinHvPlatform.dll");
        if (GetFileAttributesW(wszPath) == INVALID_FILE_ATTRIBUTES)
            rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "The native API dll was not found (%ls)", wszPath);
        else
        {
            /*
             * Check that we're in a VM and that the hypervisor identifies itself as Hyper-V.
             */
            if (!ASMHasCpuId())
                rc = RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "No CPUID support");
            else if (!ASMIsValidStdRange(ASMCpuId_EAX(0)))
                rc = RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "No CPUID leaf #1");
            else if (!(ASMCpuId_ECX(1) & X86_CPUID_FEATURE_ECX_HVP))
                rc = RTErrInfoSet(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Not in a hypervisor partition (HVP=0)");
            else
            {
                uint32_t cMaxHyperLeaf = 0;
                uint32_t uEbx = 0;
                uint32_t uEcx = 0;
                uint32_t uEdx = 0;
                ASMCpuIdExSlow(0x40000000, 0, 0, 0, &cMaxHyperLeaf, &uEbx, &uEcx, &uEdx);
                if (!ASMIsValidHypervisorRange(cMaxHyperLeaf))
                    rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Invalid hypervisor CPUID range (%#x %#x %#x %#x)",
                                       cMaxHyperLeaf, uEbx, uEcx, uEdx);
                else if (   uEbx != UINT32_C(0x7263694d) /* Micr */
                         || uEcx != UINT32_C(0x666f736f) /* osof */
                         || uEdx != UINT32_C(0x76482074) /* t Hv */)
                    rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                                       "Not Hyper-V CPUID signature: %#x %#x %#x (expected %#x %#x %#x)",
                                       uEbx, uEcx, uEdx, UINT32_C(0x7263694d), UINT32_C(0x666f736f), UINT32_C(0x76482074));
                else if (cMaxHyperLeaf < UINT32_C(0x40000005))
                    rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "Too narrow hypervisor CPUID range (%#x)", cMaxHyperLeaf);
                else
                {
                    /** @todo would be great if we could recognize a root partition from the
                     *        CPUID info, but I currently don't dare do that. */

                    /*
                     * Now try load the DLLs and resolve the APIs.
                     */
                    static const char * const s_pszDllPrefixes[] = { "WinHvPlatform.dll!",  "vid.dll!" };
                    RTLDRMOD                  ahMods[2]          = { NIL_RTLDRMOD,          NIL_RTLDRMOD };
                    rc = RTLdrLoadSystem("vid.dll", true /*fNoUnload*/, &ahMods[1]);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTLdrLoadSystem("WinHvPlatform.dll", true /*fNoUnload*/, &ahMods[0]);
                        if (RT_SUCCESS(rc))
                        {
                            for (unsigned i = 0; i < RT_ELEMENTS(g_aImports); i++)
                            {
                                int rc2 = RTLdrGetSymbol(ahMods[g_aImports[i].idxDll], g_aImports[i].pszName,
                                                         (void **)g_aImports[i].ppfn);
                                if (RT_FAILURE(rc2))
                                {
                                    *g_aImports[i].ppfn = NULL;

                                    LogRel(("NEM:  %s: Failed to import %s%s: %Rrc",
                                            g_aImports[i].fOptional ? "info" : fForced ? "fatal" : "error",
                                            s_pszDllPrefixes[g_aImports[i].idxDll], g_aImports[i].pszName, rc2));
                                    if (!g_aImports[i].fOptional)
                                    {
                                        if (RTErrInfoIsSet(pErrInfo))
                                            RTErrInfoAddF(pErrInfo, rc2, ", %s%s",
                                                          s_pszDllPrefixes[g_aImports[i].idxDll], g_aImports[i].pszName);
                                        else
                                            rc = RTErrInfoSetF(pErrInfo, rc2, "Failed to import: %s%s",
                                                               s_pszDllPrefixes[g_aImports[i].idxDll], g_aImports[i].pszName);
                                        Assert(RT_FAILURE(rc));
                                    }
                                }
                            }
                            if (RT_SUCCESS(rc))
                            {
                                Assert(!RTErrInfoIsSet(pErrInfo));

                                /*
                                 * Check if the hypervisor API is present.
                                 */
                                /** @todo Someone (MS) please explain weird API design:
                                 *   1. Caps.CapabilityCode duplication,
                                 *   2. No output size.
                                 */
                                WHV_CAPABILITY Caps;
                                RT_ZERO(Caps);
                                SetLastError(0);
                                HRESULT hrc = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &Caps, sizeof(Caps));
                                DWORD   rcWin = GetLastError();
                                if (FAILED(hrc))
                                    rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                                                       "WHvGetCapability/WHvCapabilityCodeHypervisorPresent failed: %Rhrc", hrc);
                                else if (!Caps.HypervisorPresent)
                                {
                                    if (!RTPathExists(RTPATH_NT_PASSTHRU_PREFIX "Device\\VidExo"))
                                        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                                                           "WHvCapabilityCodeHypervisorPresent is FALSE! Make sure you have enabled the 'Windows Hypervisor Platform' feature.");
                                    else
                                        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                                                           "WHvCapabilityCodeHypervisorPresent is FALSE! (%u)", rcWin);
                                }
                                else
                                {
                                    /*
                                     * .
                                     */
                                    LogRel(("NEM: WHvCapabilityCodeHypervisorPresent is TRUE, so this might work...\n"));

                                    rc = RTErrInfoAddF(pErrInfo, VERR_NOT_IMPLEMENTED, "lazy bugger isn't done yet");
                                }
                            }
                            RTLdrClose(ahMods[0]);
                        }
                        else
                            rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                                               "Failed to load API DLL 'WinHvPlatform.dll': %Rrc", rc);
                        RTLdrClose(ahMods[1]);
                    }
                    else
                        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Failed to load API DLL 'vid.dll': %Rrc", rc);
                }
            }
        }
    }

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(pVM->fNEMActive || RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && !pVM->fNEMActive)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s\n", pErrInfo->pszMsg);

    if (RTErrInfoIsSet(pErrInfo))
        LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    NOREF(pVM); NOREF(enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    NOREF(pVM);
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

