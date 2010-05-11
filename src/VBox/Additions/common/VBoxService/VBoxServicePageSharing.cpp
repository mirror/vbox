/* $Id$ */
/** @file
 * VBoxService - Guest page sharing.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/mem.h>
#include <iprt/stream.h>
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

typedef struct
{
    AVLPVNODECORE   Core;
    HMODULE         hModule;
    char            szFileVersion[16];
    MODULEENTRY32   Info;
} KNOWN_MODULE, *PKNOWN_MODULE;

static DECLCALLBACK(int) VBoxServicePageSharingEmptyTreeCallback(PAVLPVNODECORE pNode, void *);

static PAVLPVNODECORE   pKnownModuleTree = NULL;

/**
 * Registers a new module with the VMM
 * @param   dwProcessId     Process id
 */
void VBoxServicePageSharingRegisterModule(HANDLE hProcess, PKNOWN_MODULE pModule)
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
    do
    {
        MEMORY_BASIC_INFORMATION MemInfo[16];

        SIZE_T ret = VirtualQueryEx(hProcess, pBaseAddress, &MemInfo[0], sizeof(MemInfo));
        Assert(ret);
        if (!ret)
        {
            VBoxServiceVerbose(3, "VBoxServicePageSharingRegisterModule: VirtualQueryEx failed with %d\n", GetLastError());
            break;
        }

        unsigned cMemInfoBlocks = ret / sizeof(MemInfo[0]);

        for (unsigned i = 0; i < cMemInfoBlocks; i++)
        {
            if (    MemInfo[i].State == MEM_COMMIT
                &&  MemInfo[i].Type == MEM_IMAGE)
            {
                switch (MemInfo[i].Protect)
                {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_READONLY:
                    aRegions[idxRegion].GCRegionAddr = (RTGCPTR64)MemInfo[i].BaseAddress;
                    aRegions[idxRegion].cbRegion     = MemInfo[i].RegionSize;
                    idxRegion++;
                    break;

                default:
                    break; /* ignore */
                }
            }

            pBaseAddress = (BYTE *)MemInfo[i].BaseAddress + MemInfo[i].RegionSize;
            if (dwModuleSize > MemInfo[i].RegionSize)
            {
                dwModuleSize -= MemInfo[i].RegionSize;
            }
            else
            {
                dwModuleSize = 0;
                break;
            }

            if (idxRegion >= RT_ELEMENTS(aRegions))
                break;  /* out of room */
        }
        if (idxRegion >= RT_ELEMENTS(aRegions))
            break;  /* out of room */
    }
    while (dwModuleSize);

    VBoxServiceVerbose(3, "VbglR3RegisterSharedModule %s %s base=%p size=%x cregions=%d\n", pModule->Info.szModule, pModule->szFileVersion, pModule->Info.modBaseAddr, pModule->Info.modBaseSize, idxRegion);
    int rc = VbglR3RegisterSharedModule(pModule->Info.szModule, pModule->szFileVersion, (RTGCPTR64)pModule->Info.modBaseAddr,
                                        pModule->Info.modBaseSize, idxRegion, aRegions);
//    AssertRC(rc);
    if (RT_FAILURE(rc))
        VBoxServiceVerbose(3, "VbglR3RegisterSharedModule failed with %d\n", rc);
    

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
        /* Found it before? */
        PAVLPVNODECORE pRec = RTAvlPVGet(ppNewTree, ModuleInfo.modBaseAddr);
        if (!pRec)
        {
            pRec = RTAvlPVRemove(&pKnownModuleTree, ModuleInfo.modBaseAddr);
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
                Assert(pModule->hModule);

                VBoxServicePageSharingRegisterModule(hProcess, pModule);

                pRec = &pModule->Core;
            }
            bool ret = RTAvlPVInsert(ppNewTree, pRec);
            Assert(ret); NOREF(ret);

            VBoxServiceVerbose(3, "\n\n     MODULE NAME:     %s",           ModuleInfo.szModule );
            VBoxServiceVerbose(3, "\n     executable     = %s",             ModuleInfo.szExePath );
            VBoxServiceVerbose(3, "\n     process ID     = 0x%08X",         ModuleInfo.th32ProcessID );
            VBoxServiceVerbose(3, "\n     base address   = 0x%08X", (DWORD) ModuleInfo.modBaseAddr );
            VBoxServiceVerbose(3, "\n     base size      = %d",             ModuleInfo.modBaseSize );
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

    VBoxServiceVerbose(3, "VBoxServicePageSharingInspectGuest\n");

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        VBoxServiceVerbose(3, "CreateToolhelp32Snapshot failed with %d\n", GetLastError());
        return;
    }

    /* Check loaded modules for all running processes. */
    PROCESSENTRY32 ProcessInfo;

    ProcessInfo.dwSize = sizeof(ProcessInfo);
    Process32First(hSnapshot, &ProcessInfo);

    do
    {
        VBoxServicePageSharingInspectModules(ProcessInfo.th32ProcessID, &pNewTree);
    }
    while (Process32Next(hSnapshot, &ProcessInfo));

    CloseHandle(hSnapshot);

    /* Delete leftover modules in the old tree. */
    RTAvlPVDestroy(&pKnownModuleTree, VBoxServicePageSharingEmptyTreeCallback, NULL);

    /* Activate new module tree. */
    pKnownModuleTree = pNewTree;
}

/**
 * RTAvlPVDestroy callback.
 */
static DECLCALLBACK(int) VBoxServicePageSharingEmptyTreeCallback(PAVLPVNODECORE pNode, void *)
{
    PKNOWN_MODULE pModule = (PKNOWN_MODULE)pNode;

    VBoxServiceVerbose(3, "VBoxServicePageSharingEmptyTreeCallback %s %s\n", pModule->Info.szModule, pModule->szFileVersion);

    /* Defererence module in the hypervisor. */
    int rc = VbglR3UnregisterSharedModule(pModule->Info.szModule, pModule->szFileVersion, (RTGCPTR64)pModule->Info.modBaseAddr, pModule->Info.modBaseSize);
    AssertRC(rc);

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
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServicePageSharingInit(void)
{
    VBoxServiceVerbose(3, "VBoxServicePageSharingInit\n");

    int rc = RTSemEventMultiCreate(&g_PageSharingEvent);
    AssertRCReturn(rc, rc);

    /* @todo report system name and version */
    /* Never fail here. */
    return VINF_SUCCESS;
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
        VBoxServiceVerbose(3, "VBoxServicePageSharingWorker: enabled=%d\n", VbglR3PageSharingIsEnabled());

        if (VbglR3PageSharingIsEnabled())
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
            VBoxServiceError("RTSemEventMultiWait failed; rc=%Rrc\n", rc);
            break;
        }
    }

    RTSemEventMultiDestroy(g_PageSharingEvent);
    g_PageSharingEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServicePageSharingWorker: finished thread\n");
    return 0;
}

/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServicePageSharingTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServicePageSharingTerm\n");
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
    VBoxServicePageSharingWorker,
    VBoxServicePageSharingStop,
    VBoxServicePageSharingTerm
};
