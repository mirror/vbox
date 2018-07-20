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
#define VMCPU_INCL_CPUM_GST_CTX
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
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/dbgftrace.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>

#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>


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

/** The Windows build number. */
static uint32_t g_uBuildNo = 17134;



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
# define VidStartVirtualProcessor                   g_pfnVidStartVirtualProcessor
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
#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
# define NEM_WIN_TEMPLATE_MODE_OWN_RUN_API
#else
# undef NEM_WIN_TEMPLATE_MODE_OWN_RUN_API
#endif
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
 * Wrapper for different WHvGetCapability signatures.
 */
DECLINLINE(HRESULT) WHvGetCapabilityWrapper(WHV_CAPABILITY_CODE enmCap, WHV_CAPABILITY *pOutput, uint32_t cbOutput)
{
    return g_pfnWHvGetCapability(enmCap, pOutput, cbOutput, NULL);
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
    HRESULT hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeHypervisorPresent, &Caps, sizeof(Caps));
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
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeExtendedVmExits, &Caps, sizeof(Caps));
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
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeFeatures, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeFeatures failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    if (Caps.Features.AsUINT64 & ~(uint64_t)0)
        LogRel(("NEM: Warning! Unknown feature definitions: %#RX64\n", Caps.Features.AsUINT64));
    /** @todo RECHECK: WHV_CAPABILITY_FEATURES typedef. */

    /*
     * Check supported exception exit bitmap bits.
     * We don't currently require this, so we just log failure.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeExceptionExitBitmap, &Caps, sizeof(Caps));
    if (SUCCEEDED(hrc))
        LogRel(("NEM: Supported exception exit bitmap: %#RX64\n", Caps.ExceptionExitBitmap));
    else
        LogRel(("NEM: Warning! WHvGetCapability/WHvCapabilityCodeExceptionExitBitmap failed: %Rhrc (Last=%#x/%u)",
                hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));

    /*
     * Check that the CPU vendor is supported.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorVendor, &Caps, sizeof(Caps));
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
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorFeatures, &Caps, sizeof(Caps));
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
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorClFlushSize, &Caps, sizeof(Caps));
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
            { 0x0004, 0x000f },
            { 0x1003, 0x100f },
            { 0x2000, 0x200f },
            { 0x3000, 0x300f },
            { 0x4000, 0x400f },
        };
        for (uint32_t j = 0; j < RT_ELEMENTS(s_aUnknowns); j++)
            for (uint32_t i = s_aUnknowns[j].iMin; i <= s_aUnknowns[j].iMax; i++)
            {
                RT_ZERO(Caps);
                hrc = WHvGetCapabilityWrapper((WHV_CAPABILITY_CODE)i, &Caps, sizeof(Caps));
                if (SUCCEEDED(hrc))
                    LogRel(("NEM: Warning! Unknown capability %#x returning: %.*Rhxs\n", i, sizeof(Caps), &Caps));
            }
    }

    /*
     * For proper operation, we require CPUID exits.
     */
    if (!pVM->nem.s.fExtendedCpuIdExit)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Missing required extended CPUID exit support");
    if (!pVM->nem.s.fExtendedMsrExit)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Missing required extended MSR exit support");
    if (!pVM->nem.s.fExtendedXcptExit)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Missing required extended exception exit support");

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
    Property.ProcessorCount = pVM->cCpus;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorCount, &Property, sizeof(Property));
    if (SUCCEEDED(hrc))
    {
        RT_ZERO(Property);
        Property.ExtendedVmExits.X64CpuidExit  = pVM->nem.s.fExtendedCpuIdExit; /** @todo Register fixed results and restrict cpuid exits */
        Property.ExtendedVmExits.X64MsrExit    = pVM->nem.s.fExtendedMsrExit;
        Property.ExtendedVmExits.ExceptionExit = pVM->nem.s.fExtendedXcptExit;
        hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeExtendedVmExits, &Property, sizeof(Property));
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
 * Makes sure APIC and firmware will not allow X2APIC mode.
 *
 * This is rather ugly.
 *
 * @returns VBox status code
 * @param   pVM             The cross context VM structure.
 */
static int nemR3WinDisableX2Apic(PVM pVM)
{
    /*
     * First make sure the 'Mode' config value of the APIC isn't set to X2APIC.
     * This defaults to APIC, so no need to change unless it's X2APIC.
     */
    PCFGMNODE pCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/Devices/apic/0/Config");
    if (pCfg)
    {
        uint8_t bMode = 0;
        int rc = CFGMR3QueryU8(pCfg, "Mode", &bMode);
        AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_CFGM_VALUE_NOT_FOUND, ("%Rrc\n", rc), rc);
        if (RT_SUCCESS(rc) && bMode == PDMAPICMODE_X2APIC)
        {
            LogRel(("NEM: Adjusting APIC configuration from X2APIC to APIC max mode.  X2APIC is not supported by the WinHvPlatform API!\n"));
            LogRel(("NEM: Disable Hyper-V if you need X2APIC for your guests!\n"));
            rc = CFGMR3RemoveValue(pCfg, "Mode");
            rc = CFGMR3InsertInteger(pCfg, "Mode", PDMAPICMODE_APIC);
            AssertLogRelRCReturn(rc, rc);
        }
    }

    /*
     * Now the firmwares.
     * These also defaults to APIC and only needs adjusting if configured to X2APIC (2).
     */
    static const char * const s_apszFirmwareConfigs[] =
    {
        "/Devices/efi/0/Config",
        "/Devices/pcbios/0/Config",
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszFirmwareConfigs); i++)
    {
        pCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/Devices/APIC/0/Config");
        if (pCfg)
        {
            uint8_t bMode = 0;
            int rc = CFGMR3QueryU8(pCfg, "APIC", &bMode);
            AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_CFGM_VALUE_NOT_FOUND, ("%Rrc\n", rc), rc);
            if (RT_SUCCESS(rc) && bMode == 2)
            {
                LogRel(("NEM: Adjusting %s/Mode from 2 (X2APIC) to 1 (APIC).\n", s_apszFirmwareConfigs[i]));
                rc = CFGMR3RemoveValue(pCfg, "APIC");
                rc = CFGMR3InsertInteger(pCfg, "APIC", 1);
                AssertLogRelRCReturn(rc, rc);
            }
        }
    }

    return VINF_SUCCESS;
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
    g_uBuildNo = RTSystemGetNtBuildNo();

    /*
     * Some state init.
     */
    pVM->nem.s.fA20Enabled = true;
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PNEMCPU pNemCpu = &pVM->aCpus[iCpu].nem.s;
        pNemCpu->uPendingApicBase = UINT64_MAX;
    }

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
                        nemR3WinDisableX2Apic(pVM);

                        /* Register release statistics */
                        for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
                        {
                            PNEMCPU pNemCpu = &pVM->aCpus[iCpu].nem.s;
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitPortIo,          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of port I/O exits",               "/NEM/CPU%u/ExitPortIo", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitMemUnmapped,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unmapped memory exits",        "/NEM/CPU%u/ExitMemUnmapped", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitMemIntercept,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of intercepted memory exits",     "/NEM/CPU%u/ExitMemIntercept", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitHalt,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of HLT exits",                    "/NEM/CPU%u/ExitHalt", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitInterruptWindow, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of HLT exits",                    "/NEM/CPU%u/ExitInterruptWindow", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitCpuId,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of CPUID exits",                  "/NEM/CPU%u/ExitCpuId", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitMsr,             STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of MSR access exits",             "/NEM/CPU%u/ExitMsr", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitException,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of exception exits",              "/NEM/CPU%u/ExitException", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionBp,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #BP exits",                    "/NEM/CPU%u/ExitExceptionBp", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionDb,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #DB exits",                    "/NEM/CPU%u/ExitExceptionDb", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUd,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #UD exits",                    "/NEM/CPU%u/ExitExceptionUd", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUdHandled, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of handled #UD exits",         "/NEM/CPU%u/ExitExceptionUdHandled", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatExitUnrecoverable,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unrecoverable exits",          "/NEM/CPU%u/ExitUnrecoverable", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatGetMsgTimeout,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of get message timeouts/alerts",  "/NEM/CPU%u/GetMsgTimeout", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuSuccess,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of successful CPU stops",         "/NEM/CPU%u/StopCpuSuccess", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPending,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stops",            "/NEM/CPU%u/StopCpuPending", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingAlerts,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stop alerts",      "/NEM/CPU%u/StopCpuPendingAlerts", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingOdd,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of odd pending CPU stops (see code)", "/NEM/CPU%u/StopCpuPendingOdd", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatCancelChangedState,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel changed state",         "/NEM/CPU%u/CancelChangedState", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatCancelAlertedThread, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel alerted EMT",           "/NEM/CPU%u/CancelAlertedEMT", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPre,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pre execution FF breaks",      "/NEM/CPU%u/BreakOnFFPre", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPost,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of post execution FF breaks",     "/NEM/CPU%u/BreakOnFFPost", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnCancel,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel execution breaks",      "/NEM/CPU%u/BreakOnCancel", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnStatus,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of status code breaks",           "/NEM/CPU%u/BreakOnStatus", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnDemand,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of on-demand state imports",      "/NEM/CPU%u/ImportOnDemand", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturn,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of state imports on loop return", "/NEM/CPU%u/ImportOnReturn", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturnSkipped, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of skipped state imports on loop return", "/NEM/CPU%u/ImportOnReturnSkipped", iCpu);
                            STAMR3RegisterF(pVM, &pNemCpu->StatQueryCpuTick,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of TSC queries",                  "/NEM/CPU%u/QueryCpuTick", iCpu);
                        }

                        PUVM pUVM = pVM->pUVM;
                        STAMR3RegisterRefresh(pUVM, &pVM->nem.s.R0Stats.cPagesAvailable, STAMTYPE_U64, STAMVISIBILITY_ALWAYS,
                                              STAMUNIT_PAGES, STAM_REFRESH_GRP_NEM, "Free pages available to the hypervisor",
                                              "/NEM/R0Stats/cPagesAvailable");
                        STAMR3RegisterRefresh(pUVM, &pVM->nem.s.R0Stats.cPagesInUse,     STAMTYPE_U64, STAMVISIBILITY_ALWAYS,
                                              STAMUNIT_PAGES, STAM_REFRESH_GRP_NEM, "Pages in use by hypervisor",
                                              "/NEM/R0Stats/cPagesInUse");
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
    WHV_PARTITION_PROPERTY Property;
    HRESULT                hrc;

#if 0
    /* Not sure if we really need to set the vendor.
       Update: Apparently we don't. WHvPartitionPropertyCodeProcessorVendor was removed in 17110. */
    RT_ZERO(Property);
    Property.ProcessorVendor = pVM->nem.s.enmCpuVendor == CPUMCPUVENDOR_AMD ? WHvProcessorVendorAmd
                             : WHvProcessorVendorIntel;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorVendor, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorVendor to %u: %Rhrc (Last=%#x/%u)",
                          Property.ProcessorVendor, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
#endif

    /* Not sure if we really need to set the cache line flush size. */
    RT_ZERO(Property);
    Property.ProcessorClFlushSize = pVM->nem.s.cCacheLineFlushShift;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorClFlushSize, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorClFlushSize to %u: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.cCacheLineFlushShift, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    /* Intercept #DB, #BP and #UD exceptions. */
    RT_ZERO(Property);
    Property.ExceptionExitBitmap = RT_BIT_64(WHvX64ExceptionTypeDebugTrapOrFault)
                                 | RT_BIT_64(WHvX64ExceptionTypeBreakpointTrap)
                                 | RT_BIT_64(WHvX64ExceptionTypeInvalidOpcodeFault);
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeExceptionExitBitmap, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeExceptionExitBitmap to %#RX64: %Rhrc (Last=%#x/%u)",
                          Property.ExceptionExitBitmap, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());


    /*
     * Sync CPU features with CPUM.
     */
    /** @todo sync CPU features with CPUM. */

    /* Set the partition property. */
    RT_ZERO(Property);
    Property.ProcessorFeatures.AsUINT64 = pVM->nem.s.uCpuFeatures.u64;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorFeatures, &Property, sizeof(Property));
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

#ifndef NEM_WIN_USE_OUR_OWN_RUN_API
# ifdef NEM_WIN_WITH_RING0_RUNLOOP
        if (!pVM->nem.s.fUseRing0Runloop)
# endif
        {
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
        }
# ifdef NEM_WIN_WITH_RING0_RUNLOOP
        else
# endif
#endif /* !NEM_WIN_USE_OUR_OWN_RUN_API */
#if defined(NEM_WIN_WITH_RING0_RUNLOOP) || defined(NEM_WIN_USE_OUR_OWN_RUN_API)
        {
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
        }
#endif
    }
    pVM->nem.s.fCreatedEmts = true;

    /*
     * Do some more ring-0 initialization now that we've got the partition handle.
     */
    int rc = VMMR3CallR0Emt(pVM, &pVM->aCpus[0], VMMR0_DO_NEM_INIT_VM_PART_2, 0, NULL);
    if (RT_SUCCESS(rc))
    {
        LogRel(("NEM: Successfully set up partition (device handle %p, partition ID %#llx)\n", hPartitionDevice, idHvPartition));

#if 1
        VMMR3CallR0Emt(pVM, &pVM->aCpus[0], VMMR0_DO_NEM_UPDATE_STATISTICS, 0, NULL);
        LogRel(("NEM: Memory balance: %#RX64 out of %#RX64 pages in use\n",
                pVM->nem.s.R0Stats.cPagesInUse, pVM->nem.s.R0Stats.cPagesAvailable));
#endif

        /*
         * Register statistics on shared pages.
         */
        /** @todo HvCallMapStatsPage */

        /*
         * Adjust features.
         * Note! We've already disabled X2APIC via CFGM during the first init call.
         */

#if 0 && defined(DEBUG_bird)
        /*
         * Poke and probe a little.
         */
        PVMCPU              pVCpu = &pVM->aCpus[0];
        uint32_t            aRegNames[1024];
        HV_REGISTER_VALUE   aRegValues[1024];
        uint32_t            aPropCodes[128];
        uint64_t            aPropValues[128];
        for (int iOuter = 0; iOuter < 5; iOuter++)
        {
            LogRel(("\niOuter %d\n", iOuter));
# if 1
            /* registers */
            uint32_t iRegValue = 0;
            uint32_t cRegChanges = 0;
            for (uint32_t iReg = 0; iReg < 0x001101ff; iReg++)
            {
                if (iOuter != 0 && aRegNames[iRegValue] > iReg)
                    continue;
                RT_ZERO(pVCpu->nem.s.Hypercall.Experiment);
                pVCpu->nem.s.Hypercall.Experiment.uItem = iReg;
                int rc2 = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_EXPERIMENT, 0, NULL);
                AssertLogRelRCBreak(rc2);
                if (pVCpu->nem.s.Hypercall.Experiment.fSuccess)
                {
                    LogRel(("Register %#010x = %#18RX64, %#18RX64\n", iReg,
                            pVCpu->nem.s.Hypercall.Experiment.uLoValue, pVCpu->nem.s.Hypercall.Experiment.uHiValue));
                    if (iReg == HvX64RegisterTsc)
                    {
                        uint64_t uTsc = ASMReadTSC();
                        LogRel(("TSC = %#18RX64; Delta %#18RX64 or %#18RX64\n",
                                uTsc, pVCpu->nem.s.Hypercall.Experiment.uLoValue - uTsc, uTsc - pVCpu->nem.s.Hypercall.Experiment.uLoValue));
                    }

                    if (iOuter == 0)
                        aRegNames[iRegValue] = iReg;
                    else if(   aRegValues[iRegValue].Reg128.Low64  != pVCpu->nem.s.Hypercall.Experiment.uLoValue
                            || aRegValues[iRegValue].Reg128.High64 != pVCpu->nem.s.Hypercall.Experiment.uHiValue)
                    {
                        LogRel(("Changed from          %#18RX64, %#18RX64 !!\n",
                                aRegValues[iRegValue].Reg128.Low64, aRegValues[iRegValue].Reg128.High64));
                        LogRel(("Delta                 %#18RX64, %#18RX64 !!\n",
                                pVCpu->nem.s.Hypercall.Experiment.uLoValue - aRegValues[iRegValue].Reg128.Low64,
                                pVCpu->nem.s.Hypercall.Experiment.uHiValue - aRegValues[iRegValue].Reg128.High64));
                        cRegChanges++;
                    }
                    aRegValues[iRegValue].Reg128.Low64  = pVCpu->nem.s.Hypercall.Experiment.uLoValue;
                    aRegValues[iRegValue].Reg128.High64 = pVCpu->nem.s.Hypercall.Experiment.uHiValue;
                    iRegValue++;
                    AssertBreak(iRegValue < RT_ELEMENTS(aRegValues));
                }
            }
            LogRel(("Found %u registers, %u changed\n", iRegValue, cRegChanges));
# endif
# if 1
            /* partition properties */
            uint32_t iPropValue = 0;
            uint32_t cPropChanges = 0;
            for (uint32_t iProp = 0; iProp < 0xc11ff; iProp++)
            {
                if (iProp == HvPartitionPropertyDebugChannelId /* hangs host */)
                    continue;
                if (iOuter != 0 && aPropCodes[iPropValue] > iProp)
                    continue;
                RT_ZERO(pVCpu->nem.s.Hypercall.Experiment);
                pVCpu->nem.s.Hypercall.Experiment.uItem = iProp;
                int rc2 = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_EXPERIMENT, 1, NULL);
                AssertLogRelRCBreak(rc2);
                if (pVCpu->nem.s.Hypercall.Experiment.fSuccess)
                {
                    LogRel(("Property %#010x = %#18RX64\n", iProp, pVCpu->nem.s.Hypercall.Experiment.uLoValue));
                    if (iOuter == 0)
                        aPropCodes[iPropValue] = iProp;
                    else if (aPropValues[iPropValue] != pVCpu->nem.s.Hypercall.Experiment.uLoValue)
                    {
                        LogRel(("Changed from          %#18RX64, delta %#18RX64!!\n",
                                aPropValues[iPropValue], pVCpu->nem.s.Hypercall.Experiment.uLoValue - aPropValues[iPropValue]));
                        cRegChanges++;
                    }
                    aPropValues[iPropValue] = pVCpu->nem.s.Hypercall.Experiment.uLoValue;
                    iPropValue++;
                    AssertBreak(iPropValue < RT_ELEMENTS(aPropValues));
                }
            }
            LogRel(("Found %u properties, %u changed\n", iPropValue, cPropChanges));
# endif

            /* Modify the TSC register value and see what changes. */
            if (iOuter != 0)
            {
                RT_ZERO(pVCpu->nem.s.Hypercall.Experiment);
                pVCpu->nem.s.Hypercall.Experiment.uItem = HvX64RegisterTsc;
                pVCpu->nem.s.Hypercall.Experiment.uHiValue = UINT64_C(0x00000fffffffffff) >> iOuter;
                pVCpu->nem.s.Hypercall.Experiment.uLoValue = UINT64_C(0x0011100000000000) << iOuter;
                VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_EXPERIMENT, 2, NULL);
                LogRel(("Setting HvX64RegisterTsc -> %RTbool (%#RX64)\n", pVCpu->nem.s.Hypercall.Experiment.fSuccess, pVCpu->nem.s.Hypercall.Experiment.uStatus));
            }

            RT_ZERO(pVCpu->nem.s.Hypercall.Experiment);
            pVCpu->nem.s.Hypercall.Experiment.uItem = HvX64RegisterTsc;
            VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_EXPERIMENT, 0, NULL);
            LogRel(("HvX64RegisterTsc = %#RX64, %#RX64\n", pVCpu->nem.s.Hypercall.Experiment.uLoValue, pVCpu->nem.s.Hypercall.Experiment.uHiValue));
        }

#endif
        return VINF_SUCCESS;
    }
    return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS, "Call to NEMR0InitVMPart2 failed: %Rrc", rc);
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    //BOOL fRet = SetThreadPriority(GetCurrentThread(), 0);
    //AssertLogRel(fRet);

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
            pVM->aCpus[iCpu].nem.s.pvMsgSlotMapping = NULL;
#ifndef NEM_WIN_USE_OUR_OWN_RUN_API
# ifdef NEM_WIN_WITH_RING0_RUNLOOP
            if (!pVM->nem.s.fUseRing0Runloop)
# endif
            {
                HRESULT hrc = WHvDeleteVirtualProcessor(hPartition, iCpu);
                AssertLogRelMsg(SUCCEEDED(hrc), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                                 hPartition, iCpu, hrc, RTNtLastStatusValue(),
                                                 RTNtLastErrorValue()));
            }
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


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
#ifdef NEM_WIN_WITH_RING0_RUNLOOP
    if (pVM->nem.s.fUseRing0Runloop)
    {
        for (;;)
        {
            VBOXSTRICTRC rcStrict = VMMR3CallR0EmtFast(pVM, pVCpu, VMMR0_DO_NEM_RUN);
            if (RT_SUCCESS(rcStrict))
            {
                /*
                 * We deal with VINF_NEM_CHANGE_PGM_MODE, VINF_NEM_FLUSH_TLB and
                 * VINF_NEM_UPDATE_APIC_BASE  here, since we're running the risk of
                 * getting these while we already got another RC (I/O ports).
                 *
                 * The APIC base update and a PGM update can happen at the same time, so
                 * we don't depend on the status code for that and always checks it first.
                 */
                /* APIC base: */
                if (pVCpu->nem.s.uPendingApicBase != UINT64_MAX)
                {
                    LogFlow(("nemR3NativeRunGC: calling APICSetBaseMsr(,%RX64)...\n", pVCpu->nem.s.uPendingApicBase));
                    int rc2 = APICSetBaseMsr(pVCpu, pVCpu->nem.s.uPendingApicBase);
                    AssertLogRelMsg(rc2 == VINF_SUCCESS, ("rc2=%Rrc [%#RX64]\n", rc2, pVCpu->nem.s.uPendingApicBase));
                    pVCpu->nem.s.uPendingApicBase = UINT64_MAX;
                }

                /* Status codes: */
                VBOXSTRICTRC rcPending = pVCpu->nem.s.rcPending;
                pVCpu->nem.s.rcPending = VINF_SUCCESS;
                if (   rcStrict == VINF_NEM_CHANGE_PGM_MODE
                    || rcStrict == VINF_PGM_CHANGE_MODE
                    || rcPending == VINF_NEM_CHANGE_PGM_MODE )
                {
                    LogFlow(("nemR3NativeRunGC: calling PGMChangeMode...\n"));
                    int rc = PGMChangeMode(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR4(pVCpu), CPUMGetGuestEFER(pVCpu));
                    AssertRCReturn(rc, rc);
                    if (   rcStrict == VINF_NEM_CHANGE_PGM_MODE
                        || rcStrict == VINF_PGM_CHANGE_MODE
                        || rcStrict == VINF_NEM_FLUSH_TLB)
                    {
                        if (   !VM_FF_IS_PENDING(pVM, VM_FF_HIGH_PRIORITY_POST_MASK | VM_FF_HP_R0_PRE_HM_MASK)
                            && !VMCPU_FF_IS_PENDING(pVCpu,   (VMCPU_FF_HIGH_PRIORITY_POST_MASK | VMCPU_FF_HP_R0_PRE_HM_MASK)
                                                           & ~VMCPU_FF_RESUME_GUEST_MASK))
                        {
                            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_RESUME_GUEST_MASK);
                            continue;
                        }
                        rcStrict = VINF_SUCCESS;
                    }
                }
                else if (rcStrict == VINF_NEM_FLUSH_TLB || rcPending == VINF_NEM_FLUSH_TLB)
                {
                    LogFlow(("nemR3NativeRunGC: calling PGMFlushTLB...\n"));
                    int rc = PGMFlushTLB(pVCpu, CPUMGetGuestCR3(pVCpu), true);
                    AssertRCReturn(rc, rc);
                    if (rcStrict == VINF_NEM_FLUSH_TLB || rcStrict == VINF_NEM_CHANGE_PGM_MODE)
                    {
                        if (   !VM_FF_IS_PENDING(pVM, VM_FF_HIGH_PRIORITY_POST_MASK | VM_FF_HP_R0_PRE_HM_MASK)
                            && !VMCPU_FF_IS_PENDING(pVCpu,   (VMCPU_FF_HIGH_PRIORITY_POST_MASK | VMCPU_FF_HP_R0_PRE_HM_MASK)
                                                           & ~VMCPU_FF_RESUME_GUEST_MASK))
                        {
                            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_RESUME_GUEST_MASK);
                            continue;
                        }
                        rcStrict = VINF_SUCCESS;
                    }
                }
                else if (rcStrict == VINF_NEM_UPDATE_APIC_BASE || rcPending == VERR_NEM_UPDATE_APIC_BASE)
                    continue;
                else
                    AssertMsg(rcPending == VINF_SUCCESS, ("rcPending=%Rrc\n", VBOXSTRICTRC_VAL(rcPending) ));
            }
            LogFlow(("nemR3NativeRunGC: returns %Rrc\n", VBOXSTRICTRC_VAL(rcStrict) ));
            return rcStrict;
        }
    }
#endif
    return nemHCWinRunGC(pVM, pVCpu, NULL /*pGVM*/, NULL /*pGVCpu*/);
}


bool nemR3NativeCanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM); NOREF(pVCpu);
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
# ifdef NEM_WIN_WITH_RING0_RUNLOOP
    if (pVM->nem.s.fUseRing0Runloop)
        nemHCWinCancelRunVirtualProcessor(pVM, pVCpu);
    else
# endif
    {
        Log8(("nemR3NativeNotifyFF: canceling %u\n", pVCpu->idCpu));
        HRESULT hrc = WHvCancelRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, 0);
        AssertMsg(SUCCEEDED(hrc), ("WHvCancelRunVirtualProcessor -> hrc=%Rhrc\n", hrc));
        RT_NOREF_PV(hrc);
    }
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
 * around CreateFile and NtDeviceIoControlFile, it returns an actual HANDLE for
 * the partition WinHvPlatform.  We fish this HANDLE out of the WinHvPlatform
 * partition structures because we need to talk directly to VID for reasons
 * we'll get to in a bit.  (Btw. we could also intercept the CreateFileW or
 * NtDeviceIoControlFile calls from VID.DLL to get the HANDLE should fishing in
 * the partition structures become difficult.)
 *
 * The WinHvPlatform API requires us to both set the number of guest CPUs before
 * setting up the partition and call WHvCreateVirtualProcessor for each of them.
 * The CPU creation function boils down to a VidMessageSlotMap call that sets up
 * and maps a message buffer into ring-3 for async communication with hyper-V
 * and/or the VID.SYS thread actually running the CPU thru
 * WinHvRunVpDispatchLoop().  When for instance a VMEXIT is encountered, hyper-V
 * sends a message that the WHvRunVirtualProcessor API retrieves (and later
 * acknowledges) via VidMessageSlotHandleAndGetNext.  It should be noteded that
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
 * finto it's own WHV_RUN_VP_EXIT_CONTEXT format.
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
 * @subsection subsec_nem_win_whv_cons  Issues & Feedback
 *
 * Here are some observations (mostly against build 17101):
 *
 * - The VMEXIT performance is dismal (build 17134).
 *
 *   Our proof of concept implementation with a kernel runloop (i.e. not using
 *   WHvRunVirtualProcessor and friends, but calling VID.SYS fast I/O control
 *   entry point directly) delivers 9-10% of the port I/O performance and only
 *   6-7% of the MMIO performance that we have with our own hypervisor.
 *
 *   When using the offical WinHvPlatform API, the numbers are %3 for port I/O
 *   and 5% for MMIO.
 *
 *   While the tests we've done are using tight tight loops only doing port I/O
 *   and MMIO, the problem is clearly visible when running regular guest OSes.
 *   Anything that hammers the VGA device would be suffering, for example:
 *
 *       - Windows 2000 boot screen animation overloads us with MMIO exits
 *         and won't even boot because all the time is spent in interrupt
 *         handlers and redrawin the screen.
 *
 *       - DSL 4.4 and its bootmenu logo is slower than molasses in january.
 *
 *   We have not found a workaround for this yet.
 *
 *   Something that might improve the issue a little is to detect blocks with
 *   excessive MMIO and port I/O exits and emulate instructions to cover
 *   multiple exits before letting Hyper-V have a go at the guest execution
 *   again.  This will only improve the situation under some circumstances,
 *   since emulating instructions without recompilation can be expensive, so
 *   there will only be real gains if the exitting instructions are tightly
 *   packed.
 *
 *
 * - We need a way to directly modify the TSC offset (or bias if you like).
 *
 *   The current approach of setting the WHvX64RegisterTsc register one by one
 *   on each virtual CPU in sequence will introduce random inaccuracies,
 *   especially if the thread doing the job is reschduled at a bad time.
 *
 *
 * - Unable to access WHvX64RegisterMsrMtrrCap (build 17134).
 *
 *
 * - On AMD Ryzen grub/debian 9.0 ends up with a unrecoverable exception
 *   when IA32_MTRR_PHYSMASK0 is written.
 *
 *
 * - The IA32_APIC_BASE register does not work right:
 *
 *      - Attempts by the guest to clear bit 11 (EN) are ignored, both the
 *        guest and the VMM reads back the old value.
 *
 *      - Attempts to modify the base address (bits NN:12) seems to be ignored
 *        in the same way.
 *
 *      - The VMM can modify both the base address as well as the the EN and
 *        BSP bits, however this is useless if we cannot intercept the WRMSR.
 *
 *      - Attempts by the guest to set the EXTD bit (X2APIC) result in \#GP(0),
 *        while the VMM ends up with with ERROR_HV_INVALID_PARAMETER.  Seems
 *        there is no way to support X2APIC.
 *
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
 * - Implementing A20 gate behavior is tedious, where as correctly emulating the
 *   A20M# pin (present on 486 and later) is near impossible for SMP setups
 *   (e.g. possiblity of two CPUs with different A20 status).
 *
 *   Workaround: Only do A20 on CPU 0, restricting the emulation to HMA.  We
 *   unmap all pages related to HMA (0x100000..0x10ffff) when the A20 state
 *   changes, lazily syncing the right pages back when accessed.
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
 * - Why does VID.SYS only query/set 32 registers at the time thru the
 *   HvCallGetVpRegisters and HvCallSetVpRegisters hypercalls?
 *
 *   We've not trouble getting/setting all the registers defined by
 *   WHV_REGISTER_NAME in one hypercall (around 80).  Some kind of stack
 *   buffering or similar?
 *
 *
 * - To handle the VMMCALL / VMCALL instructions, it seems we need to intercept
 *   \#UD exceptions and inspect the opcodes.  A dedicated exit for hypercalls
 *   would be more efficient, esp. for guests using \#UD for other purposes..
 *
 *
 * - Wrong instruction length in the VpContext with unmapped GPA memory exit
 *   contexts on 17115/AMD.
 *
 *   One byte "PUSH CS" was reported as 2 bytes, while a two byte
 *   "MOV [EBX],EAX" was reported with a 1 byte instruction length.  Problem
 *   naturally present in untranslated hyper-v messages.
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
 * - Querying WHvCapabilityCodeExceptionExitBitmap returns zero even when
 *   intercepts demonstrably works (17134).
 *
 *
 * - Querying HvPartitionPropertyDebugChannelId via HvCallGetPartitionProperty
 *   (hypercall) hangs the host (17134).
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
 *   Update: All concerns have been addressed in build 17110.
 *
 *
 * - The WHvGetPartitionProperty function uses the same weird design as
 *   WHvGetCapability, see above.
 *
 *   Update: All concerns have been addressed in build 17110.
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
 *   Update: All concerns have been addressed in build 17110.
 *
 *
 *
 * @section sec_nem_win_impl    Our implementation.
 *
 * We set out with the goal of wanting to run as much as possible in ring-0,
 * reasoning that this would give use the best performance.
 *
 * This goal was approached gradually, starting out with a pure WinHvPlatform
 * implementation, gradually replacing parts: register access, guest memory
 * handling, running virtual processors.  Then finally moving it all into
 * ring-0, while keeping most of it configurable so that we could make
 * comparisons (see NEMInternal.h and nemR3NativeRunGC()).
 *
 *
 * @subsection subsect_nem_win_impl_ioctl       VID.SYS I/O control calls
 *
 * To run things in ring-0 we need to talk directly to VID.SYS thru its I/O
 * control interface.  Looking at changes between like build 17083 and 17101 (if
 * memory serves) a set of the VID I/O control numbers shifted a little, which
 * means we need to determin them dynamically.  We currently do this by hooking
 * the NtDeviceIoControlFile API call from VID.DLL and snooping up the
 * parameters when making dummy calls to relevant APIs.  (We could also
 * disassemble the relevant APIs and try fish out the information from that, but
 * this is way simpler.)
 *
 * Issuing I/O control calls from ring-0 is facing a small challenge with
 * respect to direct buffering.  When using direct buffering the device will
 * typically check that the buffer is actually in the user address space range
 * and reject kernel addresses.  Fortunately, we've got the cross context VM
 * structure that is mapped into both kernel and user space, it's also locked
 * and safe to access from kernel space.  So, we place the I/O control buffers
 * in the per-CPU part of it (NEMCPU::uIoCtlBuf) and give the driver the user
 * address if direct access buffering or kernel address if not.
 *
 * The I/O control calls are 'abstracted' in the support driver, see
 * SUPR0IoCtlSetupForHandle(), SUPR0IoCtlPerform() and SUPR0IoCtlCleanup().
 *
 *
 * @subsection subsect_nem_win_impl_cpumctx     CPUMCTX
 *
 * Since the CPU state needs to live in Hyper-V when executing, we probably
 * should not transfer more than necessary when handling VMEXITs.  To help us
 * manage this CPUMCTX got a new field CPUMCTX::fExtrn that to indicate which
 * part of the state is currently externalized (== in Hyper-V).
 *
 *
 * @subsection sec_nem_win_benchmarks           Benchmarks.
 *
 * @subsubsection subsect_nem_win_benchmarks_bs2t1 Bootsector2-test1
 *
 * This is ValidationKit/bootsectors/bootsector2-test1.asm as of 2018-06-22
 * (internal r123172) running a the release build of VirtualBox from the same
 * source, though with exit optimizations disabled.  Host is AMD Threadripper 1950X
 * running out an up to date 64-bit Windows 10 build 17134.
 *
 * The base line column is using the official WinHv API for everything but physical
 * memory mapping.  The 2nd column is the default NEM/win configuration where we
 * put the main execution loop in ring-0, using hypercalls when we can and VID for
 * managing execution.  The 3rd column is regular VirtualBox using AMD-V directly,
 * hyper-V is disabled, main execution loop in ring-0.
 *
 * @verbatim
TESTING...                                                           WinHv API           Hypercalls + VID    VirtualBox AMD-V
  32-bit paged protected mode, CPUID                        :          108 874 ins/sec   113% / 123 602      1198% / 1 305 113
  32-bit pae protected mode, CPUID                          :          106 722 ins/sec   115% / 122 740      1232% / 1 315 201
  64-bit long mode, CPUID                                   :          106 798 ins/sec   114% / 122 111      1198% / 1 280 404
  16-bit unpaged protected mode, CPUID                      :          106 835 ins/sec   114% / 121 994      1216% / 1 299 665
  32-bit unpaged protected mode, CPUID                      :          105 257 ins/sec   115% / 121 772      1235% / 1 300 860
  real mode, CPUID                                          :          104 507 ins/sec   116% / 121 800      1228% / 1 283 848
CPUID EAX=1                                                 : PASSED
  32-bit paged protected mode, RDTSC                        :       99 581 834 ins/sec   100% / 100 323 307    93% / 93 473 299
  32-bit pae protected mode, RDTSC                          :       99 620 585 ins/sec   100% / 99 960 952     84% / 83 968 839
  64-bit long mode, RDTSC                                   :      100 540 009 ins/sec   100% / 100 946 372    93% / 93 652 826
  16-bit unpaged protected mode, RDTSC                      :       99 688 473 ins/sec   100% / 100 097 751    76% / 76 281 287
  32-bit unpaged protected mode, RDTSC                      :       98 385 857 ins/sec   102% / 100 510 404    94% / 93 379 536
  real mode, RDTSC                                          :      100 087 967 ins/sec   101% / 101 386 138    93% / 93 234 999
RDTSC                                                       : PASSED
  32-bit paged protected mode, Read CR4                     :        2 156 102 ins/sec    98% / 2 121 967   17114% / 369 009 009
  32-bit pae protected mode, Read CR4                       :        2 163 820 ins/sec    98% / 2 133 804   17469% / 377 999 261
  64-bit long mode, Read CR4                                :        2 164 822 ins/sec    98% / 2 128 698   18875% / 408 619 313
  16-bit unpaged protected mode, Read CR4                   :        2 162 367 ins/sec   100% / 2 168 508   17132% / 370 477 568
  32-bit unpaged protected mode, Read CR4                   :        2 163 189 ins/sec   100% / 2 169 808   16768% / 362 734 679
  real mode, Read CR4                                       :        2 162 436 ins/sec   100% / 2 164 914   15551% / 336 288 998
Read CR4                                                    : PASSED
  real mode, 32-bit IN                                      :          104 649 ins/sec   118% / 123 513      1028% / 1 075 831
  real mode, 32-bit OUT                                     :          107 102 ins/sec   115% / 123 660       982% / 1 052 259
  real mode, 32-bit IN-to-ring-3                            :          105 697 ins/sec    98% / 104 471       201% / 213 216
  real mode, 32-bit OUT-to-ring-3                           :          105 830 ins/sec    98% / 104 598       198% / 210 495
  16-bit unpaged protected mode, 32-bit IN                  :          104 855 ins/sec   117% / 123 174      1029% / 1 079 591
  16-bit unpaged protected mode, 32-bit OUT                 :          107 529 ins/sec   115% / 124 250       992% / 1 067 053
  16-bit unpaged protected mode, 32-bit IN-to-ring-3        :          106 337 ins/sec   103% / 109 565       196% / 209 367
  16-bit unpaged protected mode, 32-bit OUT-to-ring-3       :          107 558 ins/sec   100% / 108 237       191% / 206 387
  32-bit unpaged protected mode, 32-bit IN                  :          106 351 ins/sec   116% / 123 584      1016% / 1 081 325
  32-bit unpaged protected mode, 32-bit OUT                 :          106 424 ins/sec   116% / 124 252       995% / 1 059 408
  32-bit unpaged protected mode, 32-bit IN-to-ring-3        :          104 035 ins/sec   101% / 105 305       202% / 210 750
  32-bit unpaged protected mode, 32-bit OUT-to-ring-3       :          103 831 ins/sec   102% / 106 919       205% / 213 198
  32-bit paged protected mode, 32-bit IN                    :          103 356 ins/sec   119% / 123 870      1041% / 1 076 463
  32-bit paged protected mode, 32-bit OUT                   :          107 177 ins/sec   115% / 124 302       998% / 1 069 655
  32-bit paged protected mode, 32-bit IN-to-ring-3          :          104 491 ins/sec   100% / 104 744       200% / 209 264
  32-bit paged protected mode, 32-bit OUT-to-ring-3         :          106 603 ins/sec    97% / 103 849       197% / 210 219
  32-bit pae protected mode, 32-bit IN                      :          105 923 ins/sec   115% / 122 759      1041% / 1 103 261
  32-bit pae protected mode, 32-bit OUT                     :          107 083 ins/sec   117% / 126 057      1024% / 1 096 667
  32-bit pae protected mode, 32-bit IN-to-ring-3            :          106 114 ins/sec    97% / 103 496       199% / 211 312
  32-bit pae protected mode, 32-bit OUT-to-ring-3           :          105 675 ins/sec    96% / 102 096       198% / 209 890
  64-bit long mode, 32-bit IN                               :          105 800 ins/sec   113% / 120 006      1013% / 1 072 116
  64-bit long mode, 32-bit OUT                              :          105 635 ins/sec   113% / 120 375       997% / 1 053 655
  64-bit long mode, 32-bit IN-to-ring-3                     :          105 274 ins/sec    95% / 100 763       197% / 208 026
  64-bit long mode, 32-bit OUT-to-ring-3                    :          106 262 ins/sec    94% / 100 749       196% / 209 288
NOP I/O Port Access                                         : PASSED
  32-bit paged protected mode, 32-bit read                  :           57 687 ins/sec   119% / 69 136       1197% / 690 548
  32-bit paged protected mode, 32-bit write                 :           57 957 ins/sec   118% / 68 935       1183% / 685 930
  32-bit paged protected mode, 32-bit read-to-ring-3        :           57 958 ins/sec    95% / 55 432        276% / 160 505
  32-bit paged protected mode, 32-bit write-to-ring-3       :           57 922 ins/sec   100% / 58 340        304% / 176 464
  32-bit pae protected mode, 32-bit read                    :           57 478 ins/sec   119% / 68 453       1141% / 656 159
  32-bit pae protected mode, 32-bit write                   :           57 226 ins/sec   118% / 68 097       1157% / 662 504
  32-bit pae protected mode, 32-bit read-to-ring-3          :           57 582 ins/sec    94% / 54 651        268% / 154 867
  32-bit pae protected mode, 32-bit write-to-ring-3         :           57 697 ins/sec   100% / 57 750        299% / 173 030
  64-bit long mode, 32-bit read                             :           57 128 ins/sec   118% / 67 779       1071% / 611 949
  64-bit long mode, 32-bit write                            :           57 127 ins/sec   118% / 67 632       1084% / 619 395
  64-bit long mode, 32-bit read-to-ring-3                   :           57 181 ins/sec    94% / 54 123        265% / 151 937
  64-bit long mode, 32-bit write-to-ring-3                  :           57 297 ins/sec    99% / 57 286        294% / 168 694
  16-bit unpaged protected mode, 32-bit read                :           58 827 ins/sec   118% / 69 545       1185% / 697 602
  16-bit unpaged protected mode, 32-bit write               :           58 678 ins/sec   118% / 69 442       1183% / 694 387
  16-bit unpaged protected mode, 32-bit read-to-ring-3      :           57 841 ins/sec    96% / 55 730        275% / 159 163
  16-bit unpaged protected mode, 32-bit write-to-ring-3     :           57 855 ins/sec   101% / 58 834        304% / 176 169
  32-bit unpaged protected mode, 32-bit read                :           58 063 ins/sec   120% / 69 690       1233% / 716 444
  32-bit unpaged protected mode, 32-bit write               :           57 936 ins/sec   120% / 69 633       1199% / 694 753
  32-bit unpaged protected mode, 32-bit read-to-ring-3      :           58 451 ins/sec    96% / 56 183        273% / 159 972
  32-bit unpaged protected mode, 32-bit write-to-ring-3     :           58 962 ins/sec    99% / 58 955        298% / 175 936
  real mode, 32-bit read                                    :           58 571 ins/sec   118% / 69 478       1160% / 679 917
  real mode, 32-bit write                                   :           58 418 ins/sec   118% / 69 320       1185% / 692 513
  real mode, 32-bit read-to-ring-3                          :           58 072 ins/sec    96% / 55 751        274% / 159 145
  real mode, 32-bit write-to-ring-3                         :           57 870 ins/sec   101% / 58 755        307% / 178 042
NOP MMIO Access                                             : PASSED
SUCCESS
 * @endverbatim
 *
 * What we see here is:
 *
 *  - The WinHv API approach is 10 to 12 times slower for exits we can
 *    handle directly in ring-0 in the VBox AMD-V code.
 *
 *  - The WinHv API approach is 2 to 3 times slower for exits we have to
 *    go to ring-3 to handle with the VBox AMD-V code.
 *
 *  - By using hypercalls and VID.SYS from ring-0 we gain between
 *    13% and 20% over the WinHv API on exits handled in ring-0.
 *
 *  - For exits requiring ring-3 handling are between 6% slower and 3% faster
 *    than the WinHv API.
 *
 *
 * As a side note, it looks like Hyper-V doesn't let the guest read CR4 but
 * triggers exits all the time.  This isn't all that important these days since
 * OSes like Linux cache the CR4 value specifically to avoid these kinds of exits.
 *
 *
 * @subsubsection subsect_nem_win_benchmarks_w2k    Windows 2000 Boot & Shutdown
 *
 * Timing the startup and automatic shutdown of a Windows 2000 SP4 guest serves
 * as a real world benchmark and example of why exit performance is import.  When
 * Windows 2000 boots up is doing a lot of VGA redrawing of the boot animation,
 * which is very costly.  Not having installed guest additions leaves it in a VGA
 * mode after the bootup sequence is done, keep up the screen access expenses,
 * though the graphics driver more economical than the bootvid code.
 *
 * The VM was configured to automatically logon.  A startup script was installed
 * to perform the automatic shuting down and powering off the VM (thru
 * vts_shutdown.exe -f -p).  An offline snapshot of the VM was taken an restored
 * before each test run.  The test time run time is calculated from the monotonic
 * VBox.log timestamps, starting with the state change to 'RUNNING' and stopping
 * at 'POWERING_OFF'.
 *
 * The host OS and VirtualBox build is the same as for the bootsector2-test1
 * scenario.
 *
 * Results:
 *
 *  - WinHv API for all but physical page mappings:
 *          32 min 12.19 seconds
 *
 *  - The default NEM/win configuration where we put the main execution loop
 *    in ring-0, using hypercalls when we can and VID for managing execution:
 *          3 min 23.18 seconds
 *
 *  - Regular VirtualBox using AMD-V directly, hyper-V is disabled, main
 *    execution loop in ring-0:
 *          58.09 seconds
 *
 *  - WinHv API with exit history based optimizations:
 *          58.66 seconds
 *
 *  - Hypercall + VID.SYS with exit history base optimizations:
 *          58.94 seconds
 *
 * With a well above average machine needing over half an hour for booting a
 * nearly 20 year old guest kind of says it all.  The 13%-20% exit performance
 * increase we get by using hypercalls and VID.SYS directly pays off a lot here.
 * The 3m23s is almost acceptable in comparison to the half an hour.
 *
 * The similarity between the last three results strongly hits at windows 2000
 * doing a lot of waiting during boot and shutdown and isn't the best testcase
 * once a basic performance level is reached.
 *
 *
 * @subsubsection subsection_iem_win_benchmarks_deb9_nat    Debian 9 NAT performance
 *
 * This benchmark is about network performance over NAT from a 64-bit Debian 9
 * VM with a single CPU.  For network performance measurements, we use our own
 * NetPerf tool (ValidationKit/utils/network/NetPerf.cpp) to measure latency
 * and throughput.
 *
 * The setups, builds and configurations are as in the previous benchmarks
 * (release r123172 on 1950X running 64-bit W10/17134).  Please note that the
 * exit optimizations hasn't yet been in tuned with NetPerf in mind.
 *
 * The NAT network setup was selected here since it's the default one and the
 * slowest one.  There is quite a bit of IPC with worker threads and packet
 * processing involved.
 *
 * Latency test is first up.  This is a classic back and forth between the two
 * NetPerf instances, where the key measurement is the roundrip latency.  The
 * values here are the lowest result over 3-6 runs.
 *
 * Against host system:
 *   - 152 258 ns/roundtrip - 100% - regular VirtualBox SVM
 *   - 271 059 ns/roundtrip - 178% - Hypercalls + VID.SYS in ring-0 with exit optimizations.
 *   - 280 149 ns/roundtrip - 184% - Hypercalls + VID.SYS in ring-0
 *   - 317 735 ns/roundtrip - 209% - Win HV API with exit optimizations.
 *   - 342 440 ns/roundtrip - 225% - Win HV API
 *
 * Against a remote Windows 10 system over a 10Gbps link:
 *   - 243 969 ns/roundtrip - 100% - regular VirtualBox SVM
 *   - 384 427 ns/roundtrip - 158% - Win HV API with exit optimizations.
 *   - 402 411 ns/roundtrip - 165% - Hypercalls + VID.SYS in ring-0
 *   - 406 313 ns/roundtrip - 167% - Win HV API
 *   - 413 160 ns/roundtrip - 169% - Hypercalls + VID.SYS in ring-0 with exit optimizations.
 *
 * What we see here is:
 *
 *   - Consistent and signficant latency increase using Hyper-V compared
 *     to directly harnessing AMD-V ourselves.
 *
 *   - When talking to the host, it's clear that the hypercalls + VID.SYS
 *     in ring-0 method pays off.
 *
 *   - When talking to a different host, the numbers are closer and it
 *     is not longer clear which Hyper-V execution method is better.
 *
 *
 * Throughput benchmarks are performed by one side pushing data full throttle
 * for 10 seconds (minus a 1 second at each end of the test), then reversing
 * the roles and measuring it in the other direction.  The tests ran 3-5 times
 * and below are the highest and lowest results in each direction.
 *
 * Receiving from host system:
 *   - Regular VirtualBox SVM:
 *      Max: 96 907 549 bytes/s - 100%
 *      Min: 86 912 095 bytes/s - 100%
 *   - Hypercalls + VID.SYS in ring-0:
 *      Max: 84 036 544 bytes/s - 87%
 *      Min: 64 978 112 bytes/s - 75%
 *   - Hypercalls + VID.SYS in ring-0 with exit optimizations:
 *      Max: 77 760 699 bytes/s - 80%
 *      Min: 72 677 171 bytes/s - 84%
 *   - Win HV API with exit optimizations:
 *      Max: 64 465 905 bytes/s - 67%
 *      Min: 62 286 369 bytes/s - 72%
 *   - Win HV API:
 *      Max: 62 466 631 bytes/s - 64%
 *      Min: 61 362 782 bytes/s - 70%
 *
 * Sending to the host system:
 *   - Regular VirtualBox SVM:
 *      Max: 87 728 652 bytes/s - 100%
 *      Min: 86 923 198 bytes/s - 100%
 *   - Hypercalls + VID.SYS in ring-0:
 *      Max: 84 280 749 bytes/s - 96%
 *      Min: 78 369 842 bytes/s - 90%
 *   - Hypercalls + VID.SYS in ring-0 with exit optimizations:
 *      Max: 84 119 932 bytes/s - 96%
 *      Min: 77 396 811 bytes/s - 89%
 *   - Win HV API:
 *      Max: 81 714 377 bytes/s - 93%
 *      Min: 78 697 419 bytes/s - 91%
 *   - Win HV API with exit optimizations:
 *      Max: 80 502 488 bytes/s - 91%
 *      Min: 71 164 978 bytes/s - 82%
 *
 * Receiving from a remote Windows 10 system over a 10Gbps link:
 *   - Hypercalls + VID.SYS in ring-0:
 *      Max: 115 346 922 bytes/s - 136%
 *      Min: 112 912 035 bytes/s - 137%
 *   - Regular VirtualBox SVM:
 *      Max:  84 517 504 bytes/s - 100%
 *      Min:  82 597 049 bytes/s - 100%
 *   - Hypercalls + VID.SYS in ring-0 with exit optimizations:
 *      Max:  77 736 251 bytes/s - 92%
 *      Min:  73 813 784 bytes/s - 89%
 *   - Win HV API with exit optimizations:
 *      Max:  63 035 587 bytes/s - 75%
 *      Min:  57 538 380 bytes/s - 70%
 *   - Win HV API:
 *      Max:  62 279 185 bytes/s - 74%
 *      Min:  56 813 866 bytes/s - 69%
 *
 * Sending to a remote Windows 10 system over a 10Gbps link:
 *   - Win HV API with exit optimizations:
 *      Max: 116 502 357 bytes/s - 103%
 *      Min:  49 046 550 bytes/s - 59%
 *   - Regular VirtualBox SVM:
 *      Max: 113 030 991 bytes/s - 100%
 *      Min:  83 059 511 bytes/s - 100%
 *   - Hypercalls + VID.SYS in ring-0:
 *      Max: 106 435 031 bytes/s - 94%
 *      Min:  47 253 510 bytes/s - 57%
 *   - Hypercalls + VID.SYS in ring-0 with exit optimizations:
 *      Max:  94 842 287 bytes/s - 84%
 *      Min:  68 362 172 bytes/s - 82%
 *   - Win HV API:
 *      Max:  65 165 225 bytes/s - 58%
 *      Min:  47 246 573 bytes/s - 57%
 *
 * What we see here is:
 *
 *   - Again consistent numbers when talking to the host.  Showing that the
 *     ring-0 approach is preferable to the ring-3 one.
 *
 *   - Again when talking to a remote host, things get more difficult to
 *     make sense of.  The spread is larger and direct AMD-V gets beaten by
 *     a different the Hyper-V approaches in each direction.
 *
 *   - However, if we treat the first entry (remote host) as weird spikes, the
 *     other entries are consistently worse compared to direct AMD-V.  For the
 *     send case we get really bad results for WinHV.
 *
 */

