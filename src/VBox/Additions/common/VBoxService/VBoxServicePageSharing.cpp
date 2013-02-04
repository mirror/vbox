/* $Id$ */
/** @file
 * VBoxService - Guest page sharing.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/env.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_PageSharingEvent = NIL_RTSEMEVENTMULTI;

#if defined(RT_OS_WINDOWS) && !defined(TARGET_NT4)
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>

typedef struct
{
    AVLPVNODECORE   Core;
    HMODULE         hModule;
    char            szFileVersion[16];
    MODULEENTRY32   Info;
} KNOWN_MODULE, *PKNOWN_MODULE;

#define SystemModuleInformation     11

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
    ULONG Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    CHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef NTSTATUS (WINAPI *PFNZWQUERYSYSTEMINFORMATION)(ULONG, PVOID, ULONG, PULONG);
static PFNZWQUERYSYSTEMINFORMATION ZwQuerySystemInformation = NULL;
static HMODULE hNtdll = 0;


static DECLCALLBACK(int) VBoxServicePageSharingEmptyTreeCallback(PAVLPVNODECORE pNode, void *pvUser);

static PAVLPVNODECORE   g_pKnownModuleTree = NULL;
static uint64_t         g_idSession = 0;

/**
 * Registers a new module with the VMM
 * @param   pModule         Module ptr
 * @param   fValidateMemory Validate/touch memory pages or not
 */
void VBoxServicePageSharingRegisterModule(PKNOWN_MODULE pModule, bool fValidateMemory)
{
    VMMDEVSHAREDREGIONDESC   aRegions[VMMDEVSHAREDREGIONDESC_MAX];
    DWORD                    dwModuleSize = pModule->Info.modBaseSize;
    BYTE                    *pBaseAddress = pModule->Info.modBaseAddr;
    DWORD                    cbVersionSize, dummy;
    BYTE                    *pVersionInfo;

    VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule\n");

    cbVersionSize = GetFileVersionInfoSize(pModule->Info.szExePath, &dummy);
    if (!cbVersionSize)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: GetFileVersionInfoSize failed with %d\n", GetLastError());
        return;
    }
    pVersionInfo = (BYTE *)RTMemAlloc(cbVersionSize);
    if (!pVersionInfo)
        return;

    if (!GetFileVersionInfo(pModule->Info.szExePath, 0, cbVersionSize, pVersionInfo))
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: GetFileVersionInfo failed with %d\n", GetLastError());
        goto end;
    }

    /* Fetch default code page. */
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    UINT   cbTranslate;
    BOOL ret = VerQueryValue(pVersionInfo, TEXT("\\VarFileInfo\\Translation"), (LPVOID *)&lpTranslate, &cbTranslate);
    if (    !ret
        ||  cbTranslate < 4)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: VerQueryValue failed with %d (cb=%d)\n", GetLastError(), cbTranslate);
        goto end;
    }

    unsigned i;
    UINT     cbFileVersion;
    char    *lpszFileVersion;
    unsigned cTranslationBlocks = cbTranslate/sizeof(struct LANGANDCODEPAGE);

    for(i = 0; i < cTranslationBlocks; i++)
    {
        /* Fetch file version string. */
        char   szFileVersionLocation[256];

        sprintf(szFileVersionLocation, TEXT("\\StringFileInfo\\%04x%04x\\FileVersion"), lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);
        ret = VerQueryValue(pVersionInfo, szFileVersionLocation, (LPVOID *)&lpszFileVersion, &cbFileVersion);
        if (ret)
            break;
    }
    if (i == cTranslationBlocks)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: no file version found!\n");
        goto end;
    }

    _snprintf(pModule->szFileVersion, sizeof(pModule->szFileVersion), "%s", lpszFileVersion);
    pModule->szFileVersion[RT_ELEMENTS(pModule->szFileVersion) - 1] = 0;

    unsigned idxRegion = 0;

    if (fValidateMemory)
    {
        do
        {
            MEMORY_BASIC_INFORMATION MemInfo;

            SIZE_T ret = VirtualQuery(pBaseAddress, &MemInfo, sizeof(MemInfo));
            Assert(ret);
            if (!ret)
            {
                VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: VirtualQueryEx failed with %d\n", GetLastError());
                break;
            }

            if (    MemInfo.State == MEM_COMMIT
                &&  MemInfo.Type == MEM_IMAGE)
            {
                switch (MemInfo.Protect)
                {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_READONLY:
                {
                    char *pRegion = (char *)MemInfo.BaseAddress;

                    /* Skip the first region as it only contains the image file header. */
                    if (pRegion != (char *)pModule->Info.modBaseAddr)
                    {
                        /* Touch all pages. */
                        while (pRegion < (char *)MemInfo.BaseAddress + MemInfo.RegionSize)
                        {
                            /* Try to trick the optimizer to leave the page touching code in place. */
                            ASMProbeReadByte(pRegion);
                            pRegion += PAGE_SIZE;
                        }
                    }
#ifdef RT_ARCH_X86
                    aRegions[idxRegion].GCRegionAddr = (RTGCPTR32)MemInfo.BaseAddress;
#else
                    aRegions[idxRegion].GCRegionAddr = (RTGCPTR64)MemInfo.BaseAddress;
#endif
                    aRegions[idxRegion].cbRegion     = MemInfo.RegionSize;
                    idxRegion++;

                    break;
                }

                default:
                    break; /* ignore */
                }
            }

            pBaseAddress = (BYTE *)MemInfo.BaseAddress + MemInfo.RegionSize;
            if (dwModuleSize > MemInfo.RegionSize)
            {
                dwModuleSize -= MemInfo.RegionSize;
            }
            else
            {
                dwModuleSize = 0;
                break;
            }

            if (idxRegion >= RT_ELEMENTS(aRegions))
                break;  /* out of room */
        }
        while (dwModuleSize);
    }
    else
    {
        /* We can't probe kernel memory ranges, so pretend it's one big region. */
#ifdef RT_ARCH_X86
        aRegions[idxRegion].GCRegionAddr = (RTGCPTR32)pBaseAddress;
#else
        aRegions[idxRegion].GCRegionAddr = (RTGCPTR64)pBaseAddress;
#endif
        aRegions[idxRegion].cbRegion     = dwModuleSize;
        idxRegion++;
    }
    VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: VbglR3RegisterSharedModule %s %s base=%p size=%x cregions=%d\n", pModule->Info.szModule, pModule->szFileVersion, pModule->Info.modBaseAddr, pModule->Info.modBaseSize, idxRegion);
    int rc = VbglR3RegisterSharedModule(pModule->Info.szModule, pModule->szFileVersion, (uintptr_t)pModule->Info.modBaseAddr,
                                        pModule->Info.modBaseSize, idxRegion, aRegions);
    if (RT_FAILURE(rc))
        VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: VbglR3RegisterSharedModule failed with %Rrc\n", rc);

end:
    RTMemFree(pVersionInfo);
    return;
}

/**
 * Inspect all loaded modules for the specified process
 * @param   dwProcessId     Process id
 */
void VBoxServicePageSharingInspectModules(DWORD dwProcessId, PAVLPVNODECORE *ppNewTree)
{
    HANDLE hProcess, hSnapshot;

    /* Get a list of all the modules in this process. */
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION,
                           FALSE /* no child process handle inheritance */, dwProcessId);
    if (hProcess == NULL)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingInspectModules: OpenProcess %x failed with %d\n", dwProcessId, GetLastError());
        return;
    }

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingInspectModules: CreateToolhelp32Snapshot failed with %d\n", GetLastError());
        CloseHandle(hProcess);
        return;
    }

    VBoxServiceVerbose(3, "VBoxServicePageSharingInspectModules\n");

    MODULEENTRY32 ModuleInfo;
    BOOL          bRet;

    ModuleInfo.dwSize = sizeof(ModuleInfo);
    bRet = Module32First(hSnapshot, &ModuleInfo);
    do
    {
        /** @todo when changing this make sure VBoxService.exe is excluded! */
        char *pszDot = strrchr(ModuleInfo.szModule, '.');
        if (    pszDot
            &&  (pszDot[1] == 'e' || pszDot[1] == 'E'))
            continue;   /* ignore executables for now. */

        /* Found it before? */
        PAVLPVNODECORE pRec = RTAvlPVGet(ppNewTree, ModuleInfo.modBaseAddr);
        if (!pRec)
        {
            pRec = RTAvlPVRemove(&g_pKnownModuleTree, ModuleInfo.modBaseAddr);
            if (!pRec)
            {
                /* New module; register it. */
                PKNOWN_MODULE pModule = (PKNOWN_MODULE)RTMemAllocZ(sizeof(*pModule));
                Assert(pModule);
                if (!pModule)
                    break;

                pModule->Info     = ModuleInfo;
                pModule->Core.Key = ModuleInfo.modBaseAddr;
                pModule->hModule  = LoadLibraryEx(ModuleInfo.szExePath, 0, DONT_RESOLVE_DLL_REFERENCES);
                if (pModule->hModule)
                    VBoxServicePageSharingRegisterModule(pModule, true /* validate pages */);

                VBoxServiceVerbose(3, "\n\n     MODULE NAME:     %s",           ModuleInfo.szModule );
                VBoxServiceVerbose(3, "\n     executable     = %s",             ModuleInfo.szExePath );
                VBoxServiceVerbose(3, "\n     process ID     = 0x%08X",         ModuleInfo.th32ProcessID );
                VBoxServiceVerbose(3, "\n     base address   = 0x%08X", (DWORD) ModuleInfo.modBaseAddr );
                VBoxServiceVerbose(3, "\n     base size      = %d",             ModuleInfo.modBaseSize );

                pRec = &pModule->Core;
            }
            bool ret = RTAvlPVInsert(ppNewTree, pRec);
            Assert(ret); NOREF(ret);
        }
    }
    while (Module32Next(hSnapshot, &ModuleInfo));

    CloseHandle(hSnapshot);
    CloseHandle(hProcess);
}

/**
 * Inspect all running processes for executables and dlls that might be worth sharing
 * with other VMs.
 *
 */
void VBoxServicePageSharingInspectGuest()
{
    HANDLE hSnapshot;
    PAVLPVNODECORE pNewTree = NULL;
    DWORD dwProcessId = GetCurrentProcessId();

    VBoxServiceVerbose(3, "VBoxServicePageSharingInspectGuest\n");

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        VBoxServiceVerbose(3, "VBoxServicePageSharingInspectGuest: CreateToolhelp32Snapshot failed with %d\n", GetLastError());
        return;
    }

    /* Check loaded modules for all running processes. */
    PROCESSENTRY32 ProcessInfo;

    ProcessInfo.dwSize = sizeof(ProcessInfo);
    Process32First(hSnapshot, &ProcessInfo);

    do
    {
        /* Skip our own process. */
        if (ProcessInfo.th32ProcessID != dwProcessId)
            VBoxServicePageSharingInspectModules(ProcessInfo.th32ProcessID, &pNewTree);
    }
    while (Process32Next(hSnapshot, &ProcessInfo));

    CloseHandle(hSnapshot);

    /* Check all loaded kernel modules. */
    if (ZwQuerySystemInformation)
    {
        ULONG                cbBuffer = 0;
        PVOID                pBuffer = NULL;
        PRTL_PROCESS_MODULES pSystemModules;

        NTSTATUS ret = ZwQuerySystemInformation(SystemModuleInformation, (PVOID)&cbBuffer, 0, &cbBuffer);
        if (!cbBuffer)
        {
            VBoxServiceVerbose(1, "ZwQuerySystemInformation returned length 0\n");
            goto skipkernelmodules;
        }

        pBuffer = RTMemAllocZ(cbBuffer);
        if (!pBuffer)
            goto skipkernelmodules;

        ret = ZwQuerySystemInformation(SystemModuleInformation, pBuffer, cbBuffer, &cbBuffer);
        if (ret != STATUS_SUCCESS)
        {
            VBoxServiceVerbose(1, "ZwQuerySystemInformation returned %x (1)\n", ret);
            goto skipkernelmodules;
        }

        pSystemModules = (PRTL_PROCESS_MODULES)pBuffer;
        for (unsigned i = 0; i < pSystemModules->NumberOfModules; i++)
        {
            VBoxServiceVerbose(4, "\n\n   KERNEL  MODULE NAME:     %s",     pSystemModules->Modules[i].FullPathName[pSystemModules->Modules[i].OffsetToFileName] );
            VBoxServiceVerbose(4, "\n     executable     = %s",             pSystemModules->Modules[i].FullPathName );
            VBoxServiceVerbose(4, "\n     flags          = 0x%08X\n",       pSystemModules->Modules[i].Flags);

            /* User-mode modules seem to have no flags set; skip them as we detected them above. */
            if (pSystemModules->Modules[i].Flags == 0)
                continue;

            /* Found it before? */
            PAVLPVNODECORE pRec = RTAvlPVGet(&pNewTree, pSystemModules->Modules[i].ImageBase);
            if (!pRec)
            {
                pRec = RTAvlPVRemove(&g_pKnownModuleTree, pSystemModules->Modules[i].ImageBase);
                if (!pRec)
                {
                    /* New module; register it. */
                    char          szFullFilePath[512];
                    PKNOWN_MODULE pModule = (PKNOWN_MODULE)RTMemAllocZ(sizeof(*pModule));
                    Assert(pModule);
                    if (!pModule)
                        break;

                    strcpy(pModule->Info.szModule, &pSystemModules->Modules[i].FullPathName[pSystemModules->Modules[i].OffsetToFileName]);
                    GetSystemDirectoryA(szFullFilePath, sizeof(szFullFilePath));

                    /* skip \Systemroot\system32 */
                    char *lpPath = strchr(&pSystemModules->Modules[i].FullPathName[1], '\\');
                    if (!lpPath)
                    {
                        /* Seen just file names in XP; try to locate the file in the system32 and system32\drivers directories. */
                        strcat(szFullFilePath, "\\");
                        strcat(szFullFilePath, pSystemModules->Modules[i].FullPathName);
                        VBoxServiceVerbose(3, "Unexpected kernel module name try %s\n", szFullFilePath);
                        if (RTFileExists(szFullFilePath) == false)
                        {
                            GetSystemDirectoryA(szFullFilePath, sizeof(szFullFilePath));
                            strcat(szFullFilePath, "\\drivers\\");
                            strcat(szFullFilePath, pSystemModules->Modules[i].FullPathName);
                            VBoxServiceVerbose(3, "Unexpected kernel module name try %s\n", szFullFilePath);
                            if (RTFileExists(szFullFilePath) == false)
                            {
                                VBoxServiceVerbose(1, "Unexpected kernel module name %s\n", pSystemModules->Modules[i].FullPathName);
                                RTMemFree(pModule);
                                continue;
                            }
                        }
                    }
                    else
                    {
                        lpPath = strchr(lpPath+1, '\\');
                        if (!lpPath)
                        {
                            VBoxServiceVerbose(1, "Unexpected kernel module name %s (2)\n", pSystemModules->Modules[i].FullPathName);
                            RTMemFree(pModule);
                            continue;
                        }

                        strcat(szFullFilePath, lpPath);
                    }

                    strcpy(pModule->Info.szExePath, szFullFilePath);
                    pModule->Info.modBaseAddr = (BYTE *)pSystemModules->Modules[i].ImageBase;
                    pModule->Info.modBaseSize = pSystemModules->Modules[i].ImageSize;

                    pModule->Core.Key = pSystemModules->Modules[i].ImageBase;
                    VBoxServicePageSharingRegisterModule(pModule, false /* don't check memory pages */);

                    VBoxServiceVerbose(3, "\n\n   KERNEL  MODULE NAME:     %s",     pModule->Info.szModule );
                    VBoxServiceVerbose(3, "\n     executable     = %s",             pModule->Info.szExePath );
                    VBoxServiceVerbose(3, "\n     base address   = 0x%08X", (DWORD) pModule->Info.modBaseAddr );
                    VBoxServiceVerbose(3, "\n     flags          = 0x%08X",         pSystemModules->Modules[i].Flags);
                    VBoxServiceVerbose(3, "\n     base size      = %d",             pModule->Info.modBaseSize );

                    pRec = &pModule->Core;
                }
                bool ret = RTAvlPVInsert(&pNewTree, pRec);
                Assert(ret); NOREF(ret);
            }
        }
skipkernelmodules:
        if (pBuffer)
            RTMemFree(pBuffer);
    }

    /* Delete leftover modules in the old tree. */
    RTAvlPVDestroy(&g_pKnownModuleTree, VBoxServicePageSharingEmptyTreeCallback, NULL);

    /* Check all registered modules. */
    VbglR3CheckSharedModules();

    /* Activate new module tree. */
    g_pKnownModuleTree = pNewTree;
}

/**
 * RTAvlPVDestroy callback.
 */
static DECLCALLBACK(int) VBoxServicePageSharingEmptyTreeCallback(PAVLPVNODECORE pNode, void *pvUser)
{
    PKNOWN_MODULE pModule = (PKNOWN_MODULE)pNode;
    bool *pfUnregister = (bool *)pvUser;

    VBoxServiceVerbose(3, "VBoxServicePageSharingEmptyTreeCallback %s %s\n", pModule->Info.szModule, pModule->szFileVersion);

    /* Dereference module in the hypervisor. */
    if (   !pfUnregister
        || *pfUnregister)
    {
        int rc = VbglR3UnregisterSharedModule(pModule->Info.szModule, pModule->szFileVersion,
                                              (uintptr_t)pModule->Info.modBaseAddr, pModule->Info.modBaseSize);
        AssertRC(rc);
    }

    if (pModule->hModule)
        FreeLibrary(pModule->hModule);
    RTMemFree(pNode);
    return 0;
}


#elif TARGET_NT4
void VBoxServicePageSharingInspectGuest()
{
    /* not implemented */
}
#else
void VBoxServicePageSharingInspectGuest()
{
    /* @todo other platforms */
}
#endif

/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServicePageSharingPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServicePageSharingOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    NOREF(ppszShort);
    NOREF(argc);
    NOREF(argv);
    NOREF(pi);

    return -1;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServicePageSharingInit(void)
{
    VBoxServiceVerbose(3, "VBoxServicePageSharingInit\n");

    int rc = RTSemEventMultiCreate(&g_PageSharingEvent);
    AssertRCReturn(rc, rc);

#if defined(RT_OS_WINDOWS) && !defined(TARGET_NT4)
    hNtdll = LoadLibrary("ntdll.dll");

    if (hNtdll)
        ZwQuerySystemInformation = (PFNZWQUERYSYSTEMINFORMATION)GetProcAddress(hNtdll, "ZwQuerySystemInformation");

    rc =  VbglR3GetSessionId(&g_idSession);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_IO_GEN_FAILURE)
            VBoxServiceVerbose(0, "PageSharing: Page sharing support is not available by the host\n");
        else
            VBoxServiceError("VBoxServicePageSharingInit: Failed with rc=%Rrc\n", rc);

        rc = VERR_SERVICE_DISABLED;

        RTSemEventMultiDestroy(g_PageSharingEvent);
        g_PageSharingEvent = NIL_RTSEMEVENTMULTI;

    }
#endif

    return rc;
}

/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServicePageSharingWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        BOOL fEnabled = VbglR3PageSharingIsEnabled();

        VBoxServiceVerbose(3, "VBoxServicePageSharingWorker: enabled=%d\n", fEnabled);

        if (fEnabled)
            VBoxServicePageSharingInspectGuest();

        /*
         * Block for a minute.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc = RTSemEventMultiWait(g_PageSharingEvent, 60000);
        if (*pfShutdown)
            break;
        if (rc != VERR_TIMEOUT && RT_FAILURE(rc))
        {
            VBoxServiceError("VBoxServicePageSharingWorker: RTSemEventMultiWait failed; rc=%Rrc\n", rc);
            break;
        }
#if defined(RT_OS_WINDOWS) && !defined(TARGET_NT4)
        uint64_t idNewSession = g_idSession;
        rc =  VbglR3GetSessionId(&idNewSession);
        AssertRC(rc);

        if (idNewSession != g_idSession)
        {
            bool fUnregister = false;

            VBoxServiceVerbose(3, "VBoxServicePageSharingWorker: VM was restored!!\n");
            /* The VM was restored, so reregister all modules the next time. */
            RTAvlPVDestroy(&g_pKnownModuleTree, VBoxServicePageSharingEmptyTreeCallback, &fUnregister);
            g_pKnownModuleTree = NULL;

            g_idSession = idNewSession;
        }
#endif
    }

    RTSemEventMultiDestroy(g_PageSharingEvent);
    g_PageSharingEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServicePageSharingWorker: finished thread\n");
    return 0;
}

#ifdef RT_OS_WINDOWS

/**
 * This gets control when VBoxService is launched with -pagefusionfork by
 * VBoxServicePageSharingWorkerProcess().
 *
 * @returns RTEXITCODE_SUCCESS.
 *
 * @remarks It won't normally return since the parent drops the shutdown hint
 *          via RTProcTerminate().
 */
RTEXITCODE VBoxServicePageSharingInitFork(void)
{
    VBoxServiceVerbose(3, "VBoxServicePageSharingInitFork\n");

    bool fShutdown = false;
    VBoxServicePageSharingInit();
    VBoxServicePageSharingWorker(&fShutdown);

    return RTEXITCODE_SUCCESS;
}

/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServicePageSharingWorkerProcess(bool volatile *pfShutdown)
{
    RTPROCESS hProcess = NIL_RTPROCESS;
    int rc;

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        BOOL fEnabled = VbglR3PageSharingIsEnabled();
        VBoxServiceVerbose(3, "VBoxServicePageSharingWorkerProcess: enabled=%d\n", fEnabled);

        /*
         * Start a 2nd VBoxService process to deal with page fusion as we do
         * not wish to dummy load dlls into this process.  (First load with
         * DONT_RESOLVE_DLL_REFERENCES, 2nd normal -> dll init routines not called!)
         */
        if (    fEnabled
            &&  hProcess == NIL_RTPROCESS)
        {
            char szExeName[256];
            char *pszExeName = RTProcGetExecutablePath(szExeName, sizeof(szExeName));
            if (pszExeName)
            {
                char const *papszArgs[3];
                papszArgs[0] = pszExeName;
                papszArgs[1] = "--pagefusionfork";
                papszArgs[2] = NULL;
                rc = RTProcCreate(pszExeName, papszArgs, RTENV_DEFAULT, 0 /* normal child */, &hProcess);
                if (RT_FAILURE(rc))
                    VBoxServiceError("VBoxServicePageSharingWorkerProcess: RTProcCreate %s failed; rc=%Rrc\n", pszExeName, rc);
            }
        }

        /*
         * Block for a minute.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        rc = RTSemEventMultiWait(g_PageSharingEvent, 60000);
        if (*pfShutdown)
            break;
        if (rc != VERR_TIMEOUT && RT_FAILURE(rc))
        {
            VBoxServiceError("VBoxServicePageSharingWorkerProcess: RTSemEventMultiWait failed; rc=%Rrc\n", rc);
            break;
        }
    }

    if (hProcess != NIL_RTPROCESS)
        RTProcTerminate(hProcess);

    RTSemEventMultiDestroy(g_PageSharingEvent);
    g_PageSharingEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServicePageSharingWorkerProcess: finished thread\n");
    return 0;
}

#endif /* RT_OS_WINDOWS */

/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServicePageSharingTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServicePageSharingTerm\n");

#if defined(RT_OS_WINDOWS) && !defined(TARGET_NT4)
    if (hNtdll)
        FreeLibrary(hNtdll);
#endif
    return;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServicePageSharingStop(void)
{
    RTSemEventMultiSignal(g_PageSharingEvent);
}


/**
 * The 'pagesharing' service description.
 */
VBOXSERVICE g_PageSharing =
{
    /* pszName. */
    "pagesharing",
    /* pszDescription. */
    "Page Sharing",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServicePageSharingPreInit,
    VBoxServicePageSharingOption,
    VBoxServicePageSharingInit,
#ifdef RT_OS_WINDOWS
    VBoxServicePageSharingWorkerProcess,
#else
    VBoxServicePageSharingWorker,
#endif
    VBoxServicePageSharingStop,
    VBoxServicePageSharingTerm
};
