/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Windows backend.
 *
 * Log group 2: Exit logging.
 * Log group 3: Log context on exit.
 * Log group 5: Ring-3 memory management
 * Log group 6: Ring-0 memory management
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
#include <iprt/nt/hyperv.h>
#include <iprt/nt/vid.h>
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

/** VID I/O control detection: Fake partition handle input. */
#define NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE          ((HANDLE)(uintptr_t)38479125)
/** VID I/O control detection: Fake partition ID return. */
#define NEM_WIN_IOCTL_DETECTOR_FAKE_PARTITION_ID    UINT64_C(0xfa1e000042424242)
/** VID I/O control detection: Fake CPU index input. */
#define NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX        UINT32_C(42)
/** VID I/O control detection: Fake timeout input. */
#define NEM_WIN_IOCTL_DETECTOR_FAKE_TIMEOUT         UINT32_C(0x00080286)


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
#ifndef NEM_WIN_USE_OUR_OWN_RUN_API
static decltype(WHvCreateVirtualProcessor) *        g_pfnWHvCreateVirtualProcessor;
static decltype(WHvDeleteVirtualProcessor) *        g_pfnWHvDeleteVirtualProcessor;
static decltype(WHvRunVirtualProcessor) *           g_pfnWHvRunVirtualProcessor;
static decltype(WHvGetRunExitContextSize) *         g_pfnWHvGetRunExitContextSize;
static decltype(WHvCancelRunVirtualProcessor) *     g_pfnWHvCancelRunVirtualProcessor;
static decltype(WHvGetVirtualProcessorRegisters) *  g_pfnWHvGetVirtualProcessorRegisters;
static decltype(WHvSetVirtualProcessorRegisters) *  g_pfnWHvSetVirtualProcessorRegisters;
#endif
/** @} */

/** @name APIs imported from Vid.dll
 * @{ */
static decltype(VidGetHvPartitionId)               *g_pfnVidGetHvPartitionId;
static decltype(VidStartVirtualProcessor)          *g_pfnVidStartVirtualProcessor;
static decltype(VidStopVirtualProcessor)           *g_pfnVidStopVirtualProcessor;
static decltype(VidMessageSlotMap)                 *g_pfnVidMessageSlotMap;
static decltype(VidMessageSlotHandleAndGetNext)    *g_pfnVidMessageSlotHandleAndGetNext;
#ifdef LOG_ENABLED
static decltype(VidGetVirtualProcessorState)       *g_pfnVidGetVirtualProcessorState;
static decltype(VidSetVirtualProcessorState)       *g_pfnVidSetVirtualProcessorState;
static decltype(VidGetVirtualProcessorRunningStatus) *g_pfnVidGetVirtualProcessorRunningStatus;
#endif
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
#ifndef NEM_WIN_USE_OUR_OWN_RUN_API
    NEM_WIN_IMPORT(0, false, WHvCreateVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvDeleteVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvCancelRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvGetRunExitContextSize),
    NEM_WIN_IMPORT(0, false, WHvGetVirtualProcessorRegisters),
    NEM_WIN_IMPORT(0, false, WHvSetVirtualProcessorRegisters),
#endif
    NEM_WIN_IMPORT(1, false, VidGetHvPartitionId),
    NEM_WIN_IMPORT(1, false, VidMessageSlotMap),
    NEM_WIN_IMPORT(1, false, VidMessageSlotHandleAndGetNext),
    NEM_WIN_IMPORT(1, false, VidStartVirtualProcessor),
    NEM_WIN_IMPORT(1, false, VidStopVirtualProcessor),
#ifdef LOG_ENABLED
    NEM_WIN_IMPORT(1, false, VidGetVirtualProcessorState),
    NEM_WIN_IMPORT(1, false, VidSetVirtualProcessorState),
    NEM_WIN_IMPORT(1, false, VidGetVirtualProcessorRunningStatus),
#endif
#undef NEM_WIN_IMPORT
};


/** The real NtDeviceIoControlFile API in NTDLL.   */
static decltype(NtDeviceIoControlFile) *g_pfnNtDeviceIoControlFile;
/** Pointer to the NtDeviceIoControlFile import table entry. */
static decltype(NtDeviceIoControlFile) **g_ppfnVidNtDeviceIoControlFile;
/** Info about the VidGetHvPartitionId I/O control interface. */
static NEMWINIOCTL g_IoCtlGetHvPartitionId;
/** Info about the VidStartVirtualProcessor I/O control interface. */
static NEMWINIOCTL g_IoCtlStartVirtualProcessor;
/** Info about the VidStopVirtualProcessor I/O control interface. */
static NEMWINIOCTL g_IoCtlStopVirtualProcessor;
/** Info about the VidMessageSlotHandleAndGetNext I/O control interface. */
static NEMWINIOCTL g_IoCtlMessageSlotHandleAndGetNext;
#ifdef LOG_ENABLED
/** Info about the VidMessageSlotMap I/O control interface - for logging. */
static NEMWINIOCTL g_IoCtlMessageSlotMap;
/* Info about the VidGetVirtualProcessorState I/O control interface - for logging. */
static NEMWINIOCTL g_IoCtlGetVirtualProcessorState;
/* Info about the VidSetVirtualProcessorState I/O control interface - for logging. */
static NEMWINIOCTL g_IoCtlSetVirtualProcessorState;
/** Pointer to what nemR3WinIoctlDetector_ForLogging should fill in. */
static NEMWINIOCTL *g_pIoCtlDetectForLogging;
#endif

#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
/** Mapping slot for CPU #0.
 * @{  */
static VID_MESSAGE_MAPPING_HEADER            *g_pMsgSlotMapping = NULL;
static const HV_MESSAGE_HEADER               *g_pHvMsgHdr;
static const HV_X64_INTERCEPT_MESSAGE_HEADER *g_pX64MsgHdr;
/** @} */
#endif


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

# define VidMessageSlotHandleAndGetNext             g_pfnVidMessageSlotHandleAndGetNext
# define VidStartVirtualProcessor                    g_pfnVidStartVirtualProcessor
# define VidStopVirtualProcessor                    g_pfnVidStopVirtualProcessor

#endif

/** WHV_MEMORY_ACCESS_TYPE names */
static const char * const g_apszWHvMemAccesstypes[4] = { "read", "write", "exec", "!undefined!" };


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/*
 * Instantate the code we share with ring-0.
 */
#include "../VMMAll/NEMAllNativeTemplate-win.cpp.h"



#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
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

    char szFunction[32];
    const char *pszFunction;
    if (uFunction == g_IoCtlMessageSlotHandleAndGetNext.uFunction)
        pszFunction = "VidMessageSlotHandleAndGetNext";
    else if (uFunction == g_IoCtlStartVirtualProcessor.uFunction)
        pszFunction = "VidStartVirtualProcessor";
    else if (uFunction == g_IoCtlStopVirtualProcessor.uFunction)
        pszFunction = "VidStopVirtualProcessor";
    else if (uFunction == g_IoCtlMessageSlotMap.uFunction)
        pszFunction = "VidMessageSlotMap";
    else if (uFunction == g_IoCtlGetVirtualProcessorState.uFunction)
        pszFunction = "VidGetVirtualProcessorState";
    else if (uFunction == g_IoCtlSetVirtualProcessorState.uFunction)
        pszFunction = "VidSetVirtualProcessorState";
    else
    {
        RTStrPrintf(szFunction, sizeof(szFunction), "%#x", uFunction);
        pszFunction = szFunction;
    }

    if (cbInput > 0 && pvInput)
        Log12(("VID!NtDeviceIoControlFile: %s/input: %.*Rhxs\n", pszFunction, RT_MIN(cbInput, 32), pvInput));
    NTSTATUS rcNt = g_pfnNtDeviceIoControlFile(hFile, hEvt, pfnApcCallback, pvApcCtx, pIos, uFunction,
                                               pvInput, cbInput, pvOutput, cbOutput);
    if (!hEvt && !pfnApcCallback && !pvApcCtx)
        Log12(("VID!NtDeviceIoControlFile: hFile=%#zx pIos=%p->{s:%#x, i:%#zx} uFunction=%s Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
               hFile, pIos, pIos->Status, pIos->Information, pszFunction, pvInput, cbInput, pvOutput, cbOutput, rcNt, ASMReturnAddress()));
    else
        Log12(("VID!NtDeviceIoControlFile: hFile=%#zx hEvt=%#zx Apc=%p/%p pIos=%p->{s:%#x, i:%#zx} uFunction=%s Input=%p LB %#x Output=%p LB %#x) -> %#x; Caller=%p\n",
               hFile, hEvt, pfnApcCallback, pvApcCtx, pIos, pIos->Status, pIos->Information, pszFunction,
               pvInput, cbInput, pvOutput, cbOutput, rcNt, ASMReturnAddress()));
    if (cbOutput > 0 && pvOutput)
    {
        Log12(("VID!NtDeviceIoControlFile: %s/output: %.*Rhxs\n", pszFunction, RT_MIN(cbOutput, 32), pvOutput));
        if (uFunction == 0x2210cc && g_pMsgSlotMapping == NULL && cbOutput >= sizeof(void *))
        {
            g_pMsgSlotMapping = *(VID_MESSAGE_MAPPING_HEADER **)pvOutput;
            g_pHvMsgHdr       = (const HV_MESSAGE_HEADER               *)(g_pMsgSlotMapping + 1);
            g_pX64MsgHdr      = (const HV_X64_INTERCEPT_MESSAGE_HEADER *)(g_pHvMsgHdr + 1);
            Log12(("VID!NtDeviceIoControlFile: Message slot mapping: %p\n", g_pMsgSlotMapping));
        }
    }
    if (   g_pMsgSlotMapping
        && (   uFunction == g_IoCtlMessageSlotHandleAndGetNext.uFunction
            || uFunction == g_IoCtlStopVirtualProcessor.uFunction
            || uFunction == g_IoCtlMessageSlotMap.uFunction
               ))
        Log12(("VID!NtDeviceIoControlFile: enmVidMsgType=%#x cb=%#x msg=%#x payload=%u cs:rip=%04x:%08RX64 (%s)\n",
               g_pMsgSlotMapping->enmVidMsgType, g_pMsgSlotMapping->cbMessage,
               g_pHvMsgHdr->MessageType, g_pHvMsgHdr->PayloadSize,
               g_pX64MsgHdr->CsSegment.Selector, g_pX64MsgHdr->Rip, pszFunction));

    return rcNt;
}
#endif /* NEM_WIN_INTERCEPT_NT_IO_CTLS */


/**
 * Patches the call table of VID.DLL so we can intercept NtDeviceIoControlFile.
 *
 * This is for used to figure out the I/O control codes and in logging builds
 * for logging API calls that WinHvPlatform.dll does.
 *
 * @returns VBox status code.
 * @param   hLdrModVid      The VID module handle.
 * @param   pErrInfo        Where to return additional error information.
 */
static int nemR3WinInitVidIntercepts(RTLDRMOD hLdrModVid, PRTERRINFO pErrInfo)
{
    /*
     * Locate the real API.
     */
    g_pfnNtDeviceIoControlFile = (decltype(NtDeviceIoControlFile) *)RTLdrGetSystemSymbol("NTDLL.DLL", "NtDeviceIoControlFile");
    AssertReturn(g_pfnNtDeviceIoControlFile != NULL,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Failed to resolve NtDeviceIoControlFile from NTDLL.DLL"));

    /*
     * Locate the PE header and get what we need from it.
     */
    uint8_t const *pbImage = (uint8_t const *)RTLdrGetNativeHandle(hLdrModVid);
    IMAGE_DOS_HEADER const *pMzHdr  = (IMAGE_DOS_HEADER const *)pbImage;
    AssertReturn(pMzHdr->e_magic == IMAGE_DOS_SIGNATURE,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL mapping doesn't start with MZ signature: %#x", pMzHdr->e_magic));
    IMAGE_NT_HEADERS const *pNtHdrs = (IMAGE_NT_HEADERS const *)&pbImage[pMzHdr->e_lfanew];
    AssertReturn(pNtHdrs->Signature == IMAGE_NT_SIGNATURE,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL has invalid PE signaturre: %#x @%#x",
                               pNtHdrs->Signature, pMzHdr->e_lfanew));

    uint32_t const             cbImage   = pNtHdrs->OptionalHeader.SizeOfImage;
    IMAGE_DATA_DIRECTORY const ImportDir = pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    /*
     * Walk the import descriptor table looking for NTDLL.DLL.
     */
    AssertReturn(   ImportDir.Size > 0
                 && ImportDir.Size < cbImage,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad import directory size: %#x", ImportDir.Size));
    AssertReturn(   ImportDir.VirtualAddress > 0
                 && ImportDir.VirtualAddress <= cbImage - ImportDir.Size,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad import directory RVA: %#x", ImportDir.VirtualAddress));

    for (PIMAGE_IMPORT_DESCRIPTOR pImps = (PIMAGE_IMPORT_DESCRIPTOR)&pbImage[ImportDir.VirtualAddress];
         pImps->Name != 0 && pImps->FirstThunk != 0;
         pImps++)
    {
        AssertReturn(pImps->Name < cbImage,
                     RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad import directory entry name: %#x", pImps->Name));
        const char *pszModName = (const char *)&pbImage[pImps->Name];
        if (RTStrICmpAscii(pszModName, "ntdll.dll"))
            continue;
        AssertReturn(pImps->FirstThunk < cbImage,
                     RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad FirstThunk: %#x", pImps->FirstThunk));
        AssertReturn(pImps->OriginalFirstThunk < cbImage,
                     RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad FirstThunk: %#x", pImps->FirstThunk));

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
                AssertReturn(pThunk->u1.Ordinal > 0 && pThunk->u1.Ordinal < cbImage,
                             RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "VID.DLL bad FirstThunk: %#x", pImps->FirstThunk));

                const char *pszSymbol = (const char *)&pbImage[(uintptr_t)pThunk->u1.AddressOfData + 2];
                if (strcmp(pszSymbol, "NtDeviceIoControlFile") == 0)
                {
                    DWORD fOldProt = PAGE_READONLY;
                    VirtualProtect(&pFirstThunk->u1.Function, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &fOldProt);
                    g_ppfnVidNtDeviceIoControlFile = (decltype(NtDeviceIoControlFile) **)&pFirstThunk->u1.Function;
                    /* Don't restore the protection here, so we modify the NtDeviceIoControlFile pointer later. */
                }
            }

            pThunk++;
            pFirstThunk++;
        }
    }

    if (*g_ppfnVidNtDeviceIoControlFile)
    {
#ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
        *g_ppfnVidNtDeviceIoControlFile = nemR3WinLogWrapper_NtDeviceIoControlFile;
#endif
        return VINF_SUCCESS;
    }
    return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Failed to patch NtDeviceIoControlFile import in VID.DLL!");
}


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
        rc = nemR3WinInitVidIntercepts(ahMods[1], pErrInfo);
    if (RT_SUCCESS(rc))
    {
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
        {
            Assert(!RTErrInfoIsSet(pErrInfo));
        }
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
 * Used to fill in g_IoCtlGetHvPartitionId.
 */
static NTSTATUS WINAPI
nemR3WinIoctlDetector_GetHvPartitionId(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                       PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                       PVOID pvOutput, ULONG cbOutput)
{
    AssertLogRelMsgReturn(hFile == NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, ("hFile=%p\n", hFile), STATUS_INVALID_PARAMETER_1);
    RT_NOREF(hEvt); RT_NOREF(pfnApcCallback); RT_NOREF(pvApcCtx);
    AssertLogRelMsgReturn(RT_VALID_PTR(pIos), ("pIos=%p\n", pIos), STATUS_INVALID_PARAMETER_5);
    AssertLogRelMsgReturn(cbInput == 0, ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_8);
    RT_NOREF(pvInput);

    AssertLogRelMsgReturn(RT_VALID_PTR(pvOutput), ("pvOutput=%p\n", pvOutput), STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(cbOutput == sizeof(HV_PARTITION_ID), ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_10);
    *(HV_PARTITION_ID *)pvOutput = NEM_WIN_IOCTL_DETECTOR_FAKE_PARTITION_ID;

    g_IoCtlGetHvPartitionId.cbInput   = cbInput;
    g_IoCtlGetHvPartitionId.cbOutput  = cbOutput;
    g_IoCtlGetHvPartitionId.uFunction = uFunction;

    return STATUS_SUCCESS;
}


/**
 * Used to fill in g_IoCtlStartVirtualProcessor.
 */
static NTSTATUS WINAPI
nemR3WinIoctlDetector_StartVirtualProcessor(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                            PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                            PVOID pvOutput, ULONG cbOutput)
{
    AssertLogRelMsgReturn(hFile == NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, ("hFile=%p\n", hFile), STATUS_INVALID_PARAMETER_1);
    RT_NOREF(hEvt); RT_NOREF(pfnApcCallback); RT_NOREF(pvApcCtx);
    AssertLogRelMsgReturn(RT_VALID_PTR(pIos), ("pIos=%p\n", pIos), STATUS_INVALID_PARAMETER_5);
    AssertLogRelMsgReturn(cbInput == sizeof(HV_VP_INDEX), ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_8);
    AssertLogRelMsgReturn(RT_VALID_PTR(pvInput), ("pvInput=%p\n", pvInput), STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(*(HV_VP_INDEX *)pvInput == NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX,
                          ("*piCpu=%u\n", *(HV_VP_INDEX *)pvInput), STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(cbOutput == 0, ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_10);
    RT_NOREF(pvOutput);

    g_IoCtlStartVirtualProcessor.cbInput   = cbInput;
    g_IoCtlStartVirtualProcessor.cbOutput  = cbOutput;
    g_IoCtlStartVirtualProcessor.uFunction = uFunction;

    return STATUS_SUCCESS;
}


/**
 * Used to fill in g_IoCtlStartVirtualProcessor.
 */
static NTSTATUS WINAPI
nemR3WinIoctlDetector_StopVirtualProcessor(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                           PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                           PVOID pvOutput, ULONG cbOutput)
{
    AssertLogRelMsgReturn(hFile == NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, ("hFile=%p\n", hFile), STATUS_INVALID_PARAMETER_1);
    RT_NOREF(hEvt); RT_NOREF(pfnApcCallback); RT_NOREF(pvApcCtx);
    AssertLogRelMsgReturn(RT_VALID_PTR(pIos), ("pIos=%p\n", pIos), STATUS_INVALID_PARAMETER_5);
    AssertLogRelMsgReturn(cbInput == sizeof(HV_VP_INDEX), ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_8);
    AssertLogRelMsgReturn(RT_VALID_PTR(pvInput), ("pvInput=%p\n", pvInput), STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(*(HV_VP_INDEX *)pvInput == NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX,
                          ("*piCpu=%u\n", *(HV_VP_INDEX *)pvInput), STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(cbOutput == 0, ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_10);
    RT_NOREF(pvOutput);

    g_IoCtlStopVirtualProcessor.cbInput   = cbInput;
    g_IoCtlStopVirtualProcessor.cbOutput  = cbOutput;
    g_IoCtlStopVirtualProcessor.uFunction = uFunction;

    return STATUS_SUCCESS;
}


/**
 * Used to fill in g_IoCtlMessageSlotHandleAndGetNext
 */
static NTSTATUS WINAPI
nemR3WinIoctlDetector_MessageSlotHandleAndGetNext(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                                  PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                                  PVOID pvOutput, ULONG cbOutput)
{
    AssertLogRelMsgReturn(hFile == NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, ("hFile=%p\n", hFile), STATUS_INVALID_PARAMETER_1);
    RT_NOREF(hEvt); RT_NOREF(pfnApcCallback); RT_NOREF(pvApcCtx);
    AssertLogRelMsgReturn(RT_VALID_PTR(pIos), ("pIos=%p\n", pIos), STATUS_INVALID_PARAMETER_5);

    AssertLogRelMsgReturn(cbInput == sizeof(VID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT), ("cbInput=%#x\n", cbInput),
                          STATUS_INVALID_PARAMETER_8);
    AssertLogRelMsgReturn(RT_VALID_PTR(pvInput), ("pvInput=%p\n", pvInput), STATUS_INVALID_PARAMETER_9);
    PCVID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT pVidIn = (PCVID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT)pvInput;
    AssertLogRelMsgReturn(   pVidIn->iCpu == NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX
                          && pVidIn->fFlags == VID_MSHAGN_F_HANDLE_MESSAGE
                          && pVidIn->cMillies == NEM_WIN_IOCTL_DETECTOR_FAKE_TIMEOUT,
                          ("iCpu=%u fFlags=%#x cMillies=%#x\n", pVidIn->iCpu, pVidIn->fFlags, pVidIn->cMillies),
                          STATUS_INVALID_PARAMETER_9);
    AssertLogRelMsgReturn(cbOutput == 0, ("cbInput=%#x\n", cbInput), STATUS_INVALID_PARAMETER_10);
    RT_NOREF(pvOutput);

    g_IoCtlMessageSlotHandleAndGetNext.cbInput   = cbInput;
    g_IoCtlMessageSlotHandleAndGetNext.cbOutput  = cbOutput;
    g_IoCtlMessageSlotHandleAndGetNext.uFunction = uFunction;

    return STATUS_SUCCESS;
}


#ifdef LOG_ENABLED
/**
 * Used to fill in what g_pIoCtlDetectForLogging points to.
 */
static NTSTATUS WINAPI nemR3WinIoctlDetector_ForLogging(HANDLE hFile, HANDLE hEvt, PIO_APC_ROUTINE pfnApcCallback, PVOID pvApcCtx,
                                                        PIO_STATUS_BLOCK pIos, ULONG uFunction, PVOID pvInput, ULONG cbInput,
                                                        PVOID pvOutput, ULONG cbOutput)
{
    RT_NOREF(hFile, hEvt, pfnApcCallback, pvApcCtx, pIos, pvInput, pvOutput);

    g_pIoCtlDetectForLogging->cbInput   = cbInput;
    g_pIoCtlDetectForLogging->cbOutput  = cbOutput;
    g_pIoCtlDetectForLogging->uFunction = uFunction;

    return STATUS_SUCCESS;
}
#endif


/**
 * Worker for nemR3NativeInit that detect I/O control function numbers for VID.
 *
 * We use the function numbers directly in ring-0 and to name functions when
 * logging NtDeviceIoControlFile calls.
 *
 * @note    We could alternatively do this by disassembling the respective
 *          functions, but hooking NtDeviceIoControlFile and making fake calls
 *          more easily provides the desired information.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.  Will set I/O
 *                              control info members.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitDiscoverIoControlProperties(PVM pVM, PRTERRINFO pErrInfo)
{
    /*
     * Probe the I/O control information for select VID APIs so we can use
     * them directly from ring-0 and better log them.
     *
     */
    decltype(NtDeviceIoControlFile) * const pfnOrg = *g_ppfnVidNtDeviceIoControlFile;

    /* VidGetHvPartitionId */
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_GetHvPartitionId;
    HV_PARTITION_ID idHvPartition = HV_PARTITION_ID_INVALID;
    BOOL fRet = g_pfnVidGetHvPartitionId(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, &idHvPartition);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    AssertReturn(fRet && idHvPartition == NEM_WIN_IOCTL_DETECTOR_FAKE_PARTITION_ID && g_IoCtlGetHvPartitionId.uFunction != 0,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                               "Problem figuring out VidGetHvPartitionId: fRet=%u idHvPartition=%#x dwErr=%u",
                               fRet, idHvPartition, GetLastError()) );
    LogRel(("NEM: VidGetHvPartitionId            -> fun:%#x in:%#x out:%#x\n",
            g_IoCtlGetHvPartitionId.uFunction, g_IoCtlGetHvPartitionId.cbInput, g_IoCtlGetHvPartitionId.cbOutput));

    /* VidStartVirtualProcessor */
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_StartVirtualProcessor;
    fRet = g_pfnVidStartVirtualProcessor(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    AssertReturn(fRet && g_IoCtlStartVirtualProcessor.uFunction != 0,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                               "Problem figuring out VidStartVirtualProcessor: fRet=%u dwErr=%u",
                               fRet, GetLastError()) );
    LogRel(("NEM: VidStartVirtualProcessor       -> fun:%#x in:%#x out:%#x\n", g_IoCtlStartVirtualProcessor.uFunction,
            g_IoCtlStartVirtualProcessor.cbInput, g_IoCtlStartVirtualProcessor.cbOutput));

    /* VidStopVirtualProcessor */
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_StopVirtualProcessor;
    fRet = g_pfnVidStopVirtualProcessor(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    AssertReturn(fRet && g_IoCtlStopVirtualProcessor.uFunction != 0,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                               "Problem figuring out VidStopVirtualProcessor: fRet=%u dwErr=%u",
                               fRet, GetLastError()) );
    LogRel(("NEM: VidStopVirtualProcessor        -> fun:%#x in:%#x out:%#x\n", g_IoCtlStopVirtualProcessor.uFunction,
            g_IoCtlStopVirtualProcessor.cbInput, g_IoCtlStopVirtualProcessor.cbOutput));

    /* VidMessageSlotHandleAndGetNext */
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_MessageSlotHandleAndGetNext;
    fRet = g_pfnVidMessageSlotHandleAndGetNext(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE,
                                               NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX, VID_MSHAGN_F_HANDLE_MESSAGE,
                                               NEM_WIN_IOCTL_DETECTOR_FAKE_TIMEOUT);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    AssertReturn(fRet && g_IoCtlMessageSlotHandleAndGetNext.uFunction != 0,
                 RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                               "Problem figuring out VidMessageSlotHandleAndGetNext: fRet=%u dwErr=%u",
                               fRet, GetLastError()) );
    LogRel(("NEM: VidMessageSlotHandleAndGetNext -> fun:%#x in:%#x out:%#x\n",
            g_IoCtlMessageSlotHandleAndGetNext.uFunction, g_IoCtlMessageSlotHandleAndGetNext.cbInput,
            g_IoCtlMessageSlotHandleAndGetNext.cbOutput));

#ifdef LOG_ENABLED
    /* The following are only for logging: */
    union
    {
        VID_MAPPED_MESSAGE_SLOT MapSlot;
        HV_REGISTER_NAME        Name;
        HV_REGISTER_VALUE       Value;
    } uBuf;

    /* VidMessageSlotMap */
    g_pIoCtlDetectForLogging = &g_IoCtlMessageSlotMap;
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_ForLogging;
    fRet = g_pfnVidMessageSlotMap(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, &uBuf.MapSlot, NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    Assert(fRet);
    LogRel(("NEM: VidMessageSlotMap              -> fun:%#x in:%#x out:%#x\n", g_pIoCtlDetectForLogging->uFunction,
            g_pIoCtlDetectForLogging->cbInput, g_pIoCtlDetectForLogging->cbOutput));

    /* VidGetVirtualProcessorState */
    uBuf.Name = HvRegisterExplicitSuspend;
    g_pIoCtlDetectForLogging = &g_IoCtlGetVirtualProcessorState;
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_ForLogging;
    fRet = g_pfnVidGetVirtualProcessorState(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX,
                                            &uBuf.Name, 1, &uBuf.Value);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    Assert(fRet);
    LogRel(("NEM: VidGetVirtualProcessorState    -> fun:%#x in:%#x out:%#x\n", g_pIoCtlDetectForLogging->uFunction,
            g_pIoCtlDetectForLogging->cbInput, g_pIoCtlDetectForLogging->cbOutput));

    /* VidSetVirtualProcessorState */
    uBuf.Name = HvRegisterExplicitSuspend;
    g_pIoCtlDetectForLogging = &g_IoCtlSetVirtualProcessorState;
    *g_ppfnVidNtDeviceIoControlFile = nemR3WinIoctlDetector_ForLogging;
    fRet = g_pfnVidSetVirtualProcessorState(NEM_WIN_IOCTL_DETECTOR_FAKE_HANDLE, NEM_WIN_IOCTL_DETECTOR_FAKE_VP_INDEX,
                                            &uBuf.Name, 1, &uBuf.Value);
    *g_ppfnVidNtDeviceIoControlFile = pfnOrg;
    Assert(fRet);
    LogRel(("NEM: VidSetVirtualProcessorState    -> fun:%#x in:%#x out:%#x\n", g_pIoCtlDetectForLogging->uFunction,
            g_pIoCtlDetectForLogging->cbInput, g_pIoCtlDetectForLogging->cbOutput));

    g_pIoCtlDetectForLogging = NULL;
#endif

    /* Done. */
    pVM->nem.s.IoCtlGetHvPartitionId            = g_IoCtlGetHvPartitionId;
    pVM->nem.s.IoCtlStartVirtualProcessor       = g_IoCtlStartVirtualProcessor;
    pVM->nem.s.IoCtlStopVirtualProcessor        = g_IoCtlStopVirtualProcessor;
    pVM->nem.s.IoCtlMessageSlotHandleAndGetNext = g_IoCtlMessageSlotHandleAndGetNext;
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
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

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
                           pVM->cCpus, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
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
             * Discover the VID I/O control function numbers we need.
             */
            rc = nemR3WinInitDiscoverIoControlProperties(pVM, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Check out our ring-0 capabilities.
                 */
                rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_NEM_INIT_VM, 0, NULL);
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
                          Property.ProcessorVendor, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    /* Not sure if we really need to set the cache line flush size. */
    RT_ZERO(Property);
    Property.PropertyCode         = WHvPartitionPropertyCodeProcessorClFlushSize;
    Property.ProcessorClFlushSize = pVM->nem.s.cCacheLineFlushShift;
    hrc = WHvSetPartitionProperty(hPartition, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorClFlushSize to %u: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.cCacheLineFlushShift, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

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
                          pVM->nem.s.uCpuFeatures.u64, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

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
                          hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

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

    HV_PARTITION_ID idHvPartition = HV_PARTITION_ID_INVALID;
    if (!g_pfnVidGetHvPartitionId(hPartitionDevice, &idHvPartition))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to get device handle and/or partition ID for %p (hPartitionDevice=%p, Last=%#x/%u)",
                          hPartition, hPartitionDevice, RTNtLastStatusValue(), RTNtLastErrorValue());
    pVM->nem.s.hPartitionDevice = hPartitionDevice;
    pVM->nem.s.idHvPartition    = idHvPartition;

    /*
     * Setup the EMTs.
     */
    VMCPUID iCpu;
    for (iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[iCpu];

        pVCpu->nem.s.hNativeThreadHandle = (RTR3PTR)RTThreadGetNativeHandle(VMR3GetThreadHandle(pVCpu->pUVCpu));
        Assert((HANDLE)pVCpu->nem.s.hNativeThreadHandle != INVALID_HANDLE_VALUE);

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
        VID_MAPPED_MESSAGE_SLOT MappedMsgSlot = { NULL, UINT32_MAX, UINT32_MAX };
        if (g_pfnVidMessageSlotMap(hPartitionDevice, &MappedMsgSlot, iCpu))
        {
            AssertLogRelMsg(MappedMsgSlot.iCpu == iCpu && MappedMsgSlot.uParentAdvisory == UINT32_MAX,
                            ("%#x %#x (iCpu=%#x)\n", MappedMsgSlot.iCpu, MappedMsgSlot.uParentAdvisory, iCpu));
            pVCpu->nem.s.pvMsgSlotMapping = MappedMsgSlot.pMsgBlock;
        }
        else
        {
            NTSTATUS const rcNtLast  = RTNtLastStatusValue();
            DWORD const    dwErrLast = RTNtLastErrorValue();
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                              "Call to WHvSetupPartition failed: %Rhrc (Last=%#x/%u)", hrc, rcNtLast, dwErrLast);
        }
#else
        hrc = WHvCreateVirtualProcessor(hPartition, iCpu, 0 /*fFlags*/);
        if (FAILED(hrc))
        {
            NTSTATUS const rcNtLast  = RTNtLastStatusValue();
            DWORD const    dwErrLast = RTNtLastErrorValue();
            while (iCpu-- > 0)
            {
                HRESULT hrc2 = WHvDeleteVirtualProcessor(hPartition, iCpu);
                AssertLogRelMsg(SUCCEEDED(hrc2), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                                  hPartition, iCpu, hrc2, RTNtLastStatusValue(),
                                                  RTNtLastErrorValue()));
            }
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                              "Call to WHvSetupPartition failed: %Rhrc (Last=%#x/%u)", hrc, rcNtLast, dwErrLast);
        }
#endif /* !NEM_WIN_USE_OUR_OWN_RUN_API */
    }
    pVM->nem.s.fCreatedEmts = true;

    /*
     * Do some more ring-0 initialization now that we've got the partition handle.
     */
    int rc = VMMR3CallR0Emt(pVM, &pVM->aCpus[0], VMMR0_DO_NEM_INIT_VM_PART_2, 0, NULL);
    if (RT_SUCCESS(rc))
    {
        LogRel(("NEM: Successfully set up partition (device handle %p, partition ID %#llx)\n", hPartitionDevice, idHvPartition));
        return VINF_SUCCESS;
    }
    return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS, "Call to NEMR0InitVMPart2 failed: %Rrc", rc);
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
#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
            pVM->aCpus[iCpu].nem.s.pvMsgSlotMapping = NULL;
#else
            HRESULT hrc = WHvDeleteVirtualProcessor(hPartition, iCpu);
            AssertLogRelMsg(SUCCEEDED(hrc), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                             hPartition, iCpu, hrc, RTNtLastStatusValue(),
                                             RTNtLastErrorValue()));
#endif
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

#ifndef NEM_WIN_USE_OUR_OWN_RUN_API

# ifdef LOG_ENABLED
/**
 * Log the full details of an exit reason.
 *
 * @param   pExitReason     The exit reason to log.
 */
static void nemR3WinLogWHvExitReason(WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
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

#  if 0
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
#  endif

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
# endif /* LOG_ENABLED */


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


static VBOXSTRICTRC nemR3WinWHvHandleHalt(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    LogFlow(("nemR3WinWHvHandleHalt\n"));
    return VINF_EM_HALT;
}


# ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
/**
 * @callback_method_impl{FNPGMPHYSNEMENUMCALLBACK,
 *      Hack to unmap all pages when/before we run into quota (WHv only).}
 */
static DECLCALLBACK(int) nemR3WinWHvUnmapOnePageCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, uint8_t *pu2NemState, void *pvUser)
{
    RT_NOREF_PV(pvUser);
    RT_NOREF_PV(pVCpu);
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
    {
        Log5(("NEM GPA unmap all: %RGp (cMappedPages=%u)\n", GCPhys, pVM->nem.s.cMappedPages - 1));
        *pu2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
    }
    else
    {
        LogRel(("nemR3WinWHvUnmapOnePageCallback: GCPhys=%RGp %s hrc=%Rhrc (%#x) Last=%#x/%u (cMappedPages=%u)\n",
                GCPhys, g_apszPageStates[*pu2NemState], hrc, hrc, RTNtLastStatusValue(),
                RTNtLastErrorValue(), pVM->nem.s.cMappedPages));
        *pu2NemState = NEM_WIN_PAGE_STATE_NOT_SET;
    }
    if (pVM->nem.s.cMappedPages > 0)
        ASMAtomicDecU32(&pVM->nem.s.cMappedPages);
    return VINF_SUCCESS;
}
# endif /* !NEM_WIN_USE_HYPERCALLS_FOR_PAGES */


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
static VBOXSTRICTRC nemR3WinWHvHandleMemoryAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_MEMORY_ACCESS_CONTEXT const *pMemCtx)
{
    /*
     * Ask PGM for information about the given GCPhys.  We need to check if we're
     * out of sync first.
     */
    NEMHCWINHMACPCCSTATE State = { pMemCtx->AccessInfo.AccessType == WHvMemoryAccessWrite, false, false };
    PGMPHYSNEMPAGEINFO   Info;
    int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, pMemCtx->Gpa, State.fWriteAccess, &Info,
                                       nemHCWinHandleMemoryAccessPageCheckerCallback, &State);
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
    rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
    AssertRCReturn(rc, rc);

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
static VBOXSTRICTRC nemR3WinWHvHandleIoPortAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx,
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
# if 0 // requires sledgehammer
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
# endif

        int rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
        AssertRCReturn(rc, rc);

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


static VBOXSTRICTRC nemR3WinWHvHandleInterruptWindow(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleMsrAccess(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleCpuId(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleException(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleUD(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleTripleFault(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


static VBOXSTRICTRC nemR3WinWHvHandleInvalidState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, WHV_RUN_VP_EXIT_CONTEXT const *pExitReason)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx); NOREF(pExitReason);
    AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
}


VBOXSTRICTRC nemR3WinWHvRunGC(PVM pVM, PVMCPU pVCpu)
{
# ifdef LOG_ENABLED
    if (LogIs3Enabled())
    {
        Log3(("nemR3NativeRunGC: Entering #%u\n", pVCpu->idCpu));
        nemHCWinLogState(pVM, pVCpu);
    }
# endif

    /*
     * The run loop.
     */
    PCPUMCTX     pCtx            = CPUMQueryGuestCtxPtr(pVCpu);
    const bool   fSingleStepping = false; /** @todo get this from somewhere. */
    VBOXSTRICTRC rcStrict        = VINF_SUCCESS;
    for (unsigned iLoop = 0;;iLoop++)
    {
        /*
         * Copy the state.
         */
        int rc2 = nemHCWinCopyStateToHyperV(pVM, pVCpu, pCtx);
        AssertRCBreakStmt(rc2, rcStrict = rc2);

        /*
         * Run a bit.
         */
        WHV_RUN_VP_EXIT_CONTEXT ExitReason;
        RT_ZERO(ExitReason);
        if (   !VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            Log8(("Calling WHvRunVirtualProcessor\n"));
            VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED);
            HRESULT hrc = WHvRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, &ExitReason, sizeof(ExitReason));
            VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM);
            AssertLogRelMsgBreakStmt(SUCCEEDED(hrc),
                                     ("WHvRunVirtualProcessor(%p, %u,,) -> %Rhrc (Last=%#x/%u)\n", pVM->nem.s.hPartition, pVCpu->idCpu,
                                      hrc, RTNtLastStatusValue(), RTNtLastErrorValue()),
                                     rcStrict = VERR_INTERNAL_ERROR);
            Log2(("WHvRunVirtualProcessor -> %#x; exit code %#x (%d) (cpu status %u)\n",
                  hrc, ExitReason.ExitReason, ExitReason.ExitReason, nemHCWinCpuGetRunningStatus(pVCpu) ));
        }
        else
        {
            LogFlow(("nemR3NativeRunGC: returning: pending FF (pre exec)\n"));
            break;
        }

# if 0 /* sledgehammer approach */
        /*
         * Copy back the state.
         */
        rc2 = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, UINT64_MAX);
        AssertRCBreakStmt(rc2, rcStrict = rc2);
# endif

# ifdef LOG_ENABLED
        /*
         * Do some logging.
         */
        if (LogIs2Enabled())
            nemR3WinLogWHvExitReason(&ExitReason);
        if (LogIs3Enabled())
            nemHCWinLogState(pVM, pVCpu);
# endif

# if 0 //def VBOX_STRICT - requires sledgehammer
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
# endif

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
                rcStrict = nemR3WinWHvHandleHalt(pVM, pVCpu, pCtx);
                break;

            case WHvRunVpExitReasonMemoryAccess:
                rcStrict = nemR3WinWHvHandleMemoryAccess(pVM, pVCpu, pCtx, &ExitReason.MemoryAccess);
                break;

            case WHvRunVpExitReasonX64IoPortAccess:
                rcStrict = nemR3WinWHvHandleIoPortAccess(pVM, pVCpu, pCtx, &ExitReason.IoPortAccess);
                break;

            case WHvRunVpExitReasonX64InterruptWindow:
                rcStrict = nemR3WinWHvHandleInterruptWindow(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonX64MsrAccess: /* needs configuring */
                rcStrict = nemR3WinWHvHandleMsrAccess(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonX64Cpuid: /* needs configuring */
                rcStrict = nemR3WinWHvHandleCpuId(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonException: /* needs configuring */
                rcStrict = nemR3WinWHvHandleException(pVM, pVCpu, pCtx, &ExitReason);
                break;

            /* Unlikely exits: */
            case WHvRunVpExitReasonUnsupportedFeature:
                rcStrict = nemR3WinWHvHandleUD(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonUnrecoverableException:
                rcStrict = nemR3WinWHvHandleTripleFault(pVM, pVCpu, pCtx, &ExitReason);
                break;

            case WHvRunVpExitReasonInvalidVpRegisterValue:
                rcStrict = nemR3WinWHvHandleInvalidState(pVM, pVCpu, pCtx, &ExitReason);
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

# ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        /* Hack alert! */
        uint32_t const cMappedPages = pVM->nem.s.cMappedPages;
        if (cMappedPages < 4000)
        { /* likely */ }
        else
        {
            PGMPhysNemEnumPagesByState(pVM, pVCpu, NEM_WIN_PAGE_STATE_READABLE, nemR3WinWHvUnmapOnePageCallback, NULL);
            Log(("nemR3NativeRunGC: Unmapped all; cMappedPages=%u -> %u\n", cMappedPages, pVM->nem.s.cMappedPages));
        }
# endif

        /* If any FF is pending, return to the EM loops.  That's okay for the
           current sledgehammer approach. */
        if (   VM_FF_IS_PENDING(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
            || VMCPU_FF_IS_PENDING(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
        {
            LogFlow(("nemR3NativeRunGC: returning: pending FF (%#x / %#x)\n", pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions));
            break;
        }
    }


    /*
     * Copy back the state before returning.
     */
    if (pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT)))
    {
        int rc2 = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
        if (RT_SUCCESS(rc2))
            pCtx->fExtrn = 0;
        else if (RT_SUCCESS(rcStrict))
            rcStrict = rc2;
    }
    else
        pCtx->fExtrn = 0;

    return rcStrict;
}

#endif /* !NEM_WIN_USE_OUR_OWN_RUN_API */


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
#ifndef NEM_WIN_USE_OUR_OWN_RUN_API
    return nemR3WinWHvRunGC(pVM, pVCpu);
#elif 0
    return nemHCWinRunGC(pVM, pVCpu, NULL /*pGVM*/, NULL /*pGVCpu*/);
#else
    VBOXSTRICTRC rcStrict = VMMR3CallR0EmtFast(pVM, pVCpu, VMMR0_DO_NEM_RUN);
    if (RT_SUCCESS(rcStrict))
    {
        /* We deal wtih VINF_NEM_CHANGE_PGM_MODE and VINF_NEM_FLUSH_TLB here, since we're running
           the risk of getting these while we already got another RC (I/O ports). */
        VBOXSTRICTRC rcPgmPending = pVCpu->nem.s.rcPgmPending;
        pVCpu->nem.s.rcPgmPending = VINF_SUCCESS;
        if (   rcStrict == VINF_NEM_CHANGE_PGM_MODE
            || rcStrict == VINF_PGM_CHANGE_MODE
            || rcPgmPending == VINF_NEM_CHANGE_PGM_MODE )
        {
            LogFlow(("nemR3NativeRunGC: calling PGMChangeMode...\n"));
            int rc = PGMChangeMode(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR4(pVCpu), CPUMGetGuestEFER(pVCpu));
            AssertRCReturn(rc, rc);
            if (rcStrict == VINF_NEM_CHANGE_PGM_MODE || rcStrict == VINF_NEM_FLUSH_TLB)
                rcStrict = VINF_SUCCESS;
        }
        else if (rcStrict == VINF_NEM_FLUSH_TLB || rcPgmPending == VINF_NEM_FLUSH_TLB)
        {
            LogFlow(("nemR3NativeRunGC: calling PGMFlushTLB...\n"));
            int rc = PGMFlushTLB(pVCpu, CPUMGetGuestCR3(pVCpu), true);
            AssertRCReturn(rc, rc);
            if (rcStrict == VINF_NEM_FLUSH_TLB || rcStrict == VINF_NEM_CHANGE_PGM_MODE)
                rcStrict = VINF_SUCCESS;
        }
        else
            AssertMsg(rcPgmPending == VINF_SUCCESS, ("rcPgmPending=%Rrc\n", VBOXSTRICTRC_VAL(rcPgmPending) ));
    }
    return rcStrict;
#endif
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
#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
    nemHCWinCancelRunVirtualProcessor(pVM, pVCpu);
#else
    Log8(("nemR3NativeNotifyFF: canceling %u\n", pVCpu->idCpu));
    HRESULT hrc = WHvCancelRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, 0);
    AssertMsg(SUCCEEDED(hrc), ("WHvCancelRunVirtualProcessor -> hrc=%Rhrc\n", hrc));
    RT_NOREF_PV(hrc);
#endif
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
    Log5(("nemR3NativeNotifyPhysRamRegister: %RGp LB %RGp\n", GCPhys, cb));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb);
    return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2)
{
    Log5(("nemR3NativeNotifyPhysMmioExMap: %RGp LB %RGp fFlags=%#x pvMmio2=%p\n", GCPhys, cb, fFlags, pvMmio2));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags); NOREF(pvMmio2);
    return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    Log5(("nemR3NativeNotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
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
    Log5(("nemR3NativeNotifyPhysRomRegisterEarly: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
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
                        GCPhys, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
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
    Log5(("nemR3NativeNotifyPhysRomRegisterLate: %RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags));
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE}
 */
static DECLCALLBACK(int) nemR3WinUnsetForA20CheckerCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys,
                                                            PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    /* We'll just unmap the memory. */
    if (pInfo->u2NemState > NEM_WIN_PAGE_STATE_UNMAPPED)
    {
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        int rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhys);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
#else
        HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
        if (SUCCEEDED(hrc))
#endif
        {
            uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
            Log5(("NEM GPA unmapped/A20: %RGp (was %s, cMappedPages=%u)\n", GCPhys, g_apszPageStates[pInfo->u2NemState], cMappedPages));
            pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
        }
        else
        {
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
            LogRel(("nemR3WinUnsetForA20CheckerCallback/unmap: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
            return rc;
#else
            LogRel(("nemR3WinUnsetForA20CheckerCallback/unmap: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            return VERR_INTERNAL_ERROR_2;
#endif
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
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   GCPhys          The page to unmap.
 */
static int nemR3WinUnmapPageForA20Gate(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    PGMPHYSNEMPAGEINFO Info;
    return PGMPhysNemPageInfoChecker(pVM, pVCpu, GCPhys, false /*fMakeWritable*/, &Info,
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
            nemR3WinUnmapPageForA20Gate(pVM, pVCpu, GCPhys);
    }
}


/** @page pg_nem_win NEM/win - Native Execution Manager, Windows.
 *
 * On Windows the Hyper-V root partition (dom0 in zen terminology) does not have
 * nested VT-x or AMD-V capabilities.  For a while raw-mode worked inside it,
 * but for a while now we've been getting \#GP when trying to modify CR4 in the
 * world switcher.  So, when Hyper-V is active on Windows we have little choice
 * but to use Hyper-V to run our VMs.
 *
 *
 * @section sub_nem_win_whv   The WinHvPlatform API
 *
 * Since Windows 10 build 17083 there is a documented API for managing Hyper-V
 * VMs, header file WinHvPlatform.h and implementation in WinHvPlatform.dll.
 * This interface is a wrapper around the undocumented Virtualization
 * Infrastructure Driver (VID) API - VID.DLL and VID.SYS.  The wrapper is
 * written in C++, namespaced, early versions (at least) was using standard C++
 * container templates in several places.
 *
 * When creating a VM using WHvCreatePartition, it will only create the
 * WinHvPlatform structures for it, to which you get an abstract pointer.  The
 * VID API that actually creates the partition is first engaged when you call
 * WHvSetupPartition after first setting a lot of properties using
 * WHvSetPartitionProperty.  Since the VID API is just a very thin wrapper
 * around CreateFile and NtDeviceIoControl, it returns an actual HANDLE for the
 * partition WinHvPlatform.  We fish this HANDLE out of the WinHvPlatform
 * partition structures because we need to talk directly to VID for reasons
 * we'll get to in a bit.  (Btw. we could also intercept the CreateFileW or
 * NtDeviceIoControl calls from VID.DLL to get the HANDLE should fishing in the
 * partition structures become difficult.)
 *
 * The WinHvPlatform API requires us to both set the number of guest CPUs before
 * setting up the partition and call WHvCreateVirtualProcessor for each of them.
 * The CPU creation function boils down to a VidMessageSlotMap call that sets up
 * and maps a message buffer into ring-3 for async communication with hyper-V
 * and/or the VID.SYS thread actually running the CPU.  When for instance a
 * VMEXIT is encountered, hyper-V sends a message that the
 * WHvRunVirtualProcessor API retrieves (and later acknowledges) via
 * VidMessageSlotHandleAndGetNext.  It should be noteded that
 * WHvDeleteVirtualProcessor doesn't do much as there seems to be no partner
 * function VidMessagesSlotMap that reverses what it did.
 *
 * Memory is managed thru calls to WHvMapGpaRange and WHvUnmapGpaRange (GPA does
 * not mean grade point average here, but rather guest physical addressspace),
 * which corresponds to VidCreateVaGpaRangeSpecifyUserVa and VidDestroyGpaRange
 * respectively.  As 'UserVa' indicates, the functions works on user process
 * memory.  The mappings are also subject to quota restrictions, so the number
 * of ranges are limited and probably their total size as well.  Obviously
 * VID.SYS keeps track of the ranges, but so does WinHvPlatform, which means
 * there is a bit of overhead involved and quota restrctions makes sense.  For
 * some reason though, regions are lazily mapped on VMEXIT/memory by
 * WHvRunVirtualProcessor.
 *
 * Running guest code is done thru the WHvRunVirtualProcessor function.  It
 * asynchronously starts or resumes hyper-V CPU execution and then waits for an
 * VMEXIT message.  Hyper-V / VID.SYS will return information about the message
 * in the message buffer mapping, and WHvRunVirtualProcessor will convert that
 * into it's own WHV_RUN_VP_EXIT_CONTEXT format.
 *
 * Other threads can interrupt the execution by using WHvCancelVirtualProcessor,
 * which which case the thread in WHvRunVirtualProcessor is woken up via a dummy
 * QueueUserAPC and will call VidStopVirtualProcessor to asynchronously end
 * execution.  The stop CPU call not immediately succeed if the CPU encountered
 * a VMEXIT before the stop was processed, in which case the VMEXIT needs to be
 * processed first, and the pending stop will be processed in a subsequent call
 * to WHvRunVirtualProcessor.
 *
 * Registers are retrieved and set via WHvGetVirtualProcessorRegisters and
 * WHvSetVirtualProcessorRegisters.  In addition, several VMEXITs include
 * essential register state in the exit context information, potentially making
 * it possible to emulate the instruction causing the exit without involving
 * WHvGetVirtualProcessorRegisters.
 *
 *
 * @subsection subsec_nem_win_whv_cons  Issues / Disadvantages
 *
 * Here are some observations (mostly against build 17101):
 *
 * - The WHvCancelVirtualProcessor API schedules a dummy usermode APC callback
 *   in order to cancel any current or future alertable wait in VID.SYS during
 *   the VidMessageSlotHandleAndGetNext call.
 *
 *   IIRC this will make the kernel schedule the specified callback thru
 *   NTDLL!KiUserApcDispatcher by modifying the thread context and quite
 *   possibly the userland thread stack.  When the APC callback returns to
 *   KiUserApcDispatcher, it will call NtContinue to restore the old thread
 *   context and resume execution from there.  This naturally adds up to some
 *   CPU cycles, ring transitions aren't for free, especially after Spectre &
 *   Meltdown mitigations.
 *
 *   Using NtAltertThread call could do the same without the thread context
 *   modifications and the extra kernel call.
 *
 *
 * - Not sure if this is a thing, but WHvCancelVirtualProcessor seems to cause
 *   cause a lot more spurious WHvRunVirtualProcessor returns that what we get
 *   with the replacement code.  By spurious returns we mean that the
 *   subsequent call to WHvRunVirtualProcessor would return immediately.
 *
 *
 * - When WHvRunVirtualProcessor returns without a message, or on a terse
 *   VID message like HLT, it will make a kernel call to get some registers.
 *   This is potentially inefficient if the caller decides he needs more
 *   register state.
 *
 *   It would be better to just return what's available and let the caller fetch
 *   what is missing from his point of view in a single kernel call.
 *
 *
 * - The WHvRunVirtualProcessor implementation does lazy GPA range mappings when
 *   a unmapped GPA message is received from hyper-V.
 *
 *   Since MMIO is currently realized as unmapped GPA, this will slow down all
 *   MMIO accesses a tiny little bit as WHvRunVirtualProcessor looks up the
 *   guest physical address to check if it is a pending lazy mapping.
 *
 *   The lazy mapping feature makes no sense to us.  We as API user have all the
 *   information and can do lazy mapping ourselves if we want/have to (see next
 *   point).
 *
 *
 * - There is no API for modifying protection of a page within a GPA range.
 *
 *   From what we can tell, the only way to modify the protection (like readonly
 *   -> writable, or vice versa) is to first unmap the range and then remap it
 *   with the new protection.
 *
 *   We are for instance doing this quite a bit in order to track dirty VRAM
 *   pages.  VRAM pages starts out as readonly, when the guest writes to a page
 *   we take an exit, notes down which page it is, makes it writable and restart
 *   the instruction.  After refreshing the display, we reset all the writable
 *   pages to readonly again, bulk fashion.
 *
 *   Now to work around this issue, we do page sized GPA ranges.  In addition to
 *   add a lot of tracking overhead to WinHvPlatform and VID.SYS, this also
 *   causes us to exceed our quota before we've even mapped a default sized
 *   (128MB) VRAM page-by-page.  So, to work around this quota issue we have to
 *   lazily map pages and actively restrict the number of mappings.
 *
 *   Our best workaround thus far is bypassing WinHvPlatform and VID entirely
 *   when in comes to guest memory management and instead use the underlying
 *   hypercalls (HvCallMapGpaPages, HvCallUnmapGpaPages) to do it ourselves.
 *   (This also maps a whole lot better into our own guest page management
 *   infrastructure.)
 *
 *
 * - Observed problems doing WHvUnmapGpaRange immediately followed by
 *   WHvMapGpaRange.
 *
 *   As mentioned above, we've been forced to use this sequence when modifying
 *   page protection.   However, when transitioning from readonly to writable,
 *   we've ended up looping forever with the same write to readonly memory
 *   VMEXIT.  We're wondering if this issue might be related to the lazy mapping
 *   logic in WinHvPlatform.
 *
 *   Workaround: Insert a WHvRunVirtualProcessor call and make sure to get a GPA
 *   unmapped exit between the two calls.  Not entirely great performance wise
 *   (or the santity of our code).
 *
 *
 * - WHVRunVirtualProcessor wastes time converting VID/Hyper-V messages to its
 *   own format (WHV_RUN_VP_EXIT_CONTEXT).
 *
 *   We understand this might be because Microsoft wishes to remain free to
 *   modify the VID/Hyper-V messages, but it's still rather silly and does slow
 *   things down a little.  We'd much rather just process the messages directly.
 *
 *
 * - WHVRunVirtualProcessor would've benefited from using a callback interface:
 *
 *      - The potential size changes of the exit context structure wouldn't be
 *        an issue, since the function could manage that itself.
 *
 *      - State handling could probably be simplified (like cancelation).
 *
 *
 * - WHvGetVirtualProcessorRegisters and WHvSetVirtualProcessorRegisters
 *   internally converts register names, probably using temporary heap buffers.
 *
 *   From the looks of things, they are converting from WHV_REGISTER_NAME to
 *   HV_REGISTER_NAME from in the "Virtual Processor Register Names" section in
 *   the "Hypervisor Top-Level Functional Specification" document.  This feels
 *   like an awful waste of time.
 *
 *   We simply cannot understand why HV_REGISTER_NAME isn't used directly here,
 *   or at least the same values, making any conversion reduntant.  Restricting
 *   access to certain registers could easily be implement by scanning the
 *   inputs.
 *
 *   To avoid the heap + conversion overhead, we're currently using the
 *   HvCallGetVpRegisters and HvCallSetVpRegisters calls directly.
 *
 *
 * - The YMM and XCR0 registers are not yet named (17083).  This probably
 *   wouldn't be a problem if HV_REGISTER_NAME was used, see previous point.
 *
 *
 * - Why does WINHVR.SYS (or VID.SYS) only query/set 32 registers at the time
 *   thru the HvCallGetVpRegisters and HvCallSetVpRegisters hypercalls?
 *
 *   We've not trouble getting/setting all the registers defined by
 *   WHV_REGISTER_NAME in one hypercall (around 80)...
 *
 *
 * - The I/O port exit context information seems to be missing the address size
 *   information needed for correct string I/O emulation.
 *
 *   VT-x provides this information in bits 7:9 in the instruction information
 *   field on newer CPUs.  AMD-V in bits 7:9 in the EXITINFO1 field in the VMCB.
 *
 *   We can probably work around this by scanning the instruction bytes for
 *   address size prefixes.  Haven't investigated it any further yet.
 *
 *
 * - The WHvGetCapability function has a weird design:
 *      - The CapabilityCode parameter is pointlessly duplicated in the output
 *        structure (WHV_CAPABILITY).
 *
 *      - API takes void pointer, but everyone will probably be using
 *        WHV_CAPABILITY due to WHV_CAPABILITY::CapabilityCode making it
 *        impractical to use anything else.
 *
 *      - No output size.
 *
 *      - See GetFileAttributesEx, GetFileInformationByHandleEx,
 *        FindFirstFileEx, and others for typical pattern for generic
 *        information getters.
 *
 *
 * - The WHvGetPartitionProperty function uses the same weird design as
 *   WHvGetCapability, see above.
 *
 *
 * - The WHvSetPartitionProperty function has a totally weird design too:
 *      - In contrast to its partner WHvGetPartitionProperty, the property code
 *        is not a separate input parameter here but part of the input
 *        structure.
 *
 *      - The input structure is a void pointer rather than a pointer to
 *        WHV_PARTITION_PROPERTY which everyone probably will be using because
 *        of the WHV_PARTITION_PROPERTY::PropertyCode field.
 *
 *      - Really, why use PVOID for the input when the function isn't accepting
 *        minimal sizes.  E.g. WHVPartitionPropertyCodeProcessorClFlushSize only
 *        requires a 9 byte input, but the function insists on 16 bytes (17083).
 *
 *      - See GetFileAttributesEx, SetFileInformationByHandle, FindFirstFileEx,
 *        and others for typical pattern for generic information setters and
 *        getters.
 *
 *
 * @section sec_nem_win_impl    Our implementation.
 *
 * Tomorrow...
 *
 *
 */

