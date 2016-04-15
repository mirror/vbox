/* $Id$ */
/** @file
 * UsbTestServ - Remote USB test configuration and execution server, Platform
 *               specific helpers - Linux version.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/types.h>

#include <iprt/linux/sysfs.h>

#include "UsbTestServicePlatform.h"

/*********************************************************************************************************************************
*   Constants And Macros, Structures and Typedefs                                                                                *
*********************************************************************************************************************************/

/** Where the dummy_hcd.* and dummy_udc.* entries are stored. */
#define UTS_PLATFORM_LNX_DUMMY_HCD_PATH "/sys/devices/platform"

/**
 * A dummy UDC descriptor.
 */
typedef struct UTSPLATFORMLNXDUMMYHCD
{
    /* Index of the dummy hcd entry. */
    uint32_t                  idxDummyHcd;
    /** The bus ID on the host the dummy HCD is serving. */
    uint32_t                  uBusId;
    /** Flag whether this HCD is free for use. */
    bool                      fAvailable;
} UTSPLATFORMLNXDUMMYHCD;
/** Pointer to a dummy HCD entry. */
typedef UTSPLATFORMLNXDUMMYHCD *PUTSPLATFORMLNXDUMMYHCD;

/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/** Array of dummy HCD entries. */
static PUTSPLATFORMLNXDUMMYHCD g_paDummyHcd = NULL;
/** Number of Dummy hCD entries in the array. */
static unsigned                g_cDummyHcd = 0;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Queries the assigned bus ID for the given dummy HCD index.
 *
 * @returns IPRT status code.
 * @param   idxHcd            The HCD index to query the bus number for.
 * @param   puBusId           Where to store the bus number on success.
 */
static int utsPlatformLnxDummyHcdQueryBusId(uint32_t idxHcd, uint32_t *puBusId)
{
    int rc = VINF_SUCCESS;
    char aszPath[RTPATH_MAX + 1];

    size_t cchPath = RTStrPrintf(&aszPath[0], RT_ELEMENTS(aszPath), UTS_PLATFORM_LNX_DUMMY_HCD_PATH "/dummy_hcd.%u/usb*", idxHcd);
    if (cchPath == RT_ELEMENTS(aszPath))
        return VERR_BUFFER_OVERFLOW;

    PRTDIR pDir = NULL;
    rc = RTDirOpenFiltered(&pDir, aszPath, RTDIRFILTER_WINNT, 0);
    if (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirFolderContent;
        rc = RTDirRead(pDir, &DirFolderContent, NULL);
        if (RT_SUCCESS(rc))
        {
            /* Extract the bus number - it is after "usb", i.e. "usb9" indicates a bus ID of 9. */
            rc = RTStrToUInt32Ex(&DirFolderContent.szName[3], NULL, 10, puBusId);
            if (RT_SUCCESS(rc))
            {
                /* Make sure there is no other entry or something screwed us up. */
                rc = RTDirRead(pDir, &DirFolderContent, NULL);
                if (RT_SUCCESS(rc))
                    rc = VERR_INVALID_STATE;
                else if (rc == VERR_NO_MORE_FILES)
                    rc = VINF_SUCCESS;
            }
        }

        RTDirClose(pDir);
    }

    return rc;
}


DECLHIDDEN(int) utsPlatformInit(void)
{
    /* Load the modules required for setting up USB/IP testing. */
    int rc = utsPlatformModuleLoad("libcomposite", NULL, 0);
    if (RT_SUCCESS(rc))
    {
        const char *pszArg = "num=2"; /** @todo: Make configurable from config. */
        rc = utsPlatformModuleLoad("dummy_hcd", &pszArg, 1);
        if (RT_SUCCESS(rc))
        {
            /* Enumerate the available HCD and their bus numbers. */
            PRTDIR pDir = NULL;
            rc = RTDirOpenFiltered(&pDir, UTS_PLATFORM_LNX_DUMMY_HCD_PATH "/dummy_hcd.*", RTDIRFILTER_WINNT, 0);
            if (RT_SUCCESS(rc))
            {
                unsigned idxHcdCur = 0;
                unsigned idxHcdMax = 0;

                do
                {
                    RTDIRENTRY DirFolderContent;
                    rc = RTDirRead(pDir, &DirFolderContent, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Get the HCD index and assigned bus number form the sysfs entries,
                         * Any error here is silently ignored and results in the HCD not being
                         * added to the list of available controllers.
                         */
                        const char *pszIdx = RTStrStr(DirFolderContent.szName, ".");
                        if (pszIdx)
                        {
                            /* Skip the separator and convert number to index. */
                            pszIdx++;

                            uint32_t idxHcd = 0;
                            rc = RTStrToUInt32Ex(pszIdx, NULL, 10, &idxHcd);
                            if (RT_SUCCESS(rc))
                            {
                                uint32_t uBusId = 0;
                                rc = utsPlatformLnxDummyHcdQueryBusId(idxHcd, &uBusId);
                                if (RT_SUCCESS(rc))
                                {
                                    /* Add to array of available HCDs. */
                                    if (idxHcdCur == idxHcdMax)
                                    {
                                        size_t cbNew = (idxHcdMax + 10) * sizeof(UTSPLATFORMLNXDUMMYHCD);
                                        PUTSPLATFORMLNXDUMMYHCD pNew = (PUTSPLATFORMLNXDUMMYHCD)RTMemRealloc(g_paDummyHcd, cbNew);
                                        if (pNew)
                                        {
                                            idxHcdMax += 10;
                                            g_paDummyHcd = pNew;
                                        }
                                    }

                                    if (idxHcdCur < idxHcdMax)
                                    {
                                        g_paDummyHcd[idxHcdCur].idxDummyHcd = idxHcd;
                                        g_paDummyHcd[idxHcdCur].uBusId      = uBusId;
                                        g_paDummyHcd[idxHcdCur].fAvailable  = true;
                                        idxHcdCur++;
                                    }
                                }
                            }
                        }
                    }
                } while (RT_SUCCESS(rc));

                g_cDummyHcd = idxHcdCur;

                if (rc == VERR_NO_MORE_FILES)
                    rc = VINF_SUCCESS;

                RTDirClose(pDir);
            }
        }
    }

    return rc;
}


DECLHIDDEN(void) utsPlatformTerm(void)
{
    /* Unload dummy HCD. */
    utsPlatformModuleUnload("dummy_hcd");

    RTMemFree(g_paDummyHcd);
}


DECLHIDDEN(int) utsPlatformModuleLoad(const char *pszModule, const char **papszArgv,
                                      unsigned cArgv)
{
    RTPROCESS hProcModprobe = NIL_RTPROCESS;
    const char **papszArgs = (const char **)RTMemAllocZ((3 + cArgv) * sizeof(const char *));
    if (RT_UNLIKELY(!papszArgs))
        return VERR_NO_MEMORY;

    papszArgs[0] = "modprobe";
    papszArgs[1] = pszModule;

    unsigned idx;
    for (idx = 0; idx < cArgv; idx++)
        papszArgs[2+idx] = papszArgs[idx];
    papszArgs[2+idx] = NULL;

    int rc = RTProcCreate("modprobe", papszArgs, RTENV_DEFAULT, RTPROC_FLAGS_SEARCH_PATH, &hProcModprobe);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS ProcSts;
        rc = RTProcWait(hProcModprobe, RTPROCWAIT_FLAGS_BLOCK, &ProcSts);
        if (RT_SUCCESS(rc))
        {
            /* Evaluate the process status. */
            if (   ProcSts.enmReason != RTPROCEXITREASON_NORMAL
                || ProcSts.iStatus != 0)
                rc = VERR_UNRESOLVED_ERROR; /** @todo: Log and give finer grained status code. */
        }
    }

    RTMemFree(papszArgs);
    return rc;
}


DECLHIDDEN(int) utsPlatformModuleUnload(const char *pszModule)
{
    RTPROCESS hProcModprobe = NIL_RTPROCESS;
    const char *apszArgv[3];

    apszArgv[0] = "rmmod";
    apszArgv[1] = pszModule;
    apszArgv[2] = NULL;

    int rc = RTProcCreate("rmmod", apszArgv, RTENV_DEFAULT, RTPROC_FLAGS_SEARCH_PATH, &hProcModprobe);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS ProcSts;
        rc = RTProcWait(hProcModprobe, RTPROCWAIT_FLAGS_BLOCK, &ProcSts);
        if (RT_SUCCESS(rc))
        {
            /* Evaluate the process status. */
            if (   ProcSts.enmReason != RTPROCEXITREASON_NORMAL
                || ProcSts.iStatus != 0)
                rc = VERR_UNRESOLVED_ERROR; /** @todo: Log and give finer grained status code. */
        }
    }

    return rc;
}


DECLHIDDEN(int) utsPlatformLnxAcquireUDC(char **ppszUdc, uint32_t *puBusId)
{
    int rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < g_cDummyHcd; i++)
    {
        if (g_paDummyHcd[i].fAvailable)
        {
            rc = VINF_SUCCESS;
            int cbRet = RTStrAPrintf(ppszUdc, "dummy_udc.%u", g_paDummyHcd[i].idxDummyHcd);
            if (cbRet == -1)
                rc = VERR_NO_STR_MEMORY;
            *puBusId = g_paDummyHcd[i].uBusId;
            g_paDummyHcd[i].fAvailable = false;
            break;
        }
    }

    return rc;
}


DECLHIDDEN(int) utsPlatformLnxReleaseUDC(const char *pszUdc)
{
    int rc = VERR_INVALID_PARAMETER;
    const char *pszIdx = RTStrStr(pszUdc, ".");
    if (pszIdx)
    {
        pszIdx++;
        uint32_t idxHcd = 0;
        rc = RTStrToUInt32Ex(pszIdx, NULL, 10, &idxHcd);
        if (RT_SUCCESS(rc))
        {
            rc = VERR_NOT_FOUND;

            for (unsigned i = 0; i < g_cDummyHcd; i++)
            {
                if (g_paDummyHcd[i].idxDummyHcd == idxHcd)
                {
                    AssertReturn(!g_paDummyHcd[i].fAvailable, VERR_INVALID_PARAMETER);
                    g_paDummyHcd[i].fAvailable = true;
                    rc = VINF_SUCCESS;
                    break;
                }
            }
        }
    }

    return rc;
}

