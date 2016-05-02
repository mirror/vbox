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
 * A USB bus provided by the dummy HCD.
 */
typedef struct UTSPLATFORMLNXDUMMYHCDBUS
{
    /** The bus ID on the host the dummy HCD is serving. */
    uint32_t                  uBusId;
    /** Flag whether this is a super speed bus. */
    bool                      fSuperSpeed;
} UTSPLATFORMLNXDUMMYHCDBUS;
/** Pointer to a Dummy HCD bus. */
typedef UTSPLATFORMLNXDUMMYHCDBUS *PUTSPLATFORMLNXDUMMYHCDBUS;

/**
 * A dummy UDC descriptor.
 */
typedef struct UTSPLATFORMLNXDUMMYHCD
{
    /** Index of the dummy hcd entry. */
    uint32_t                   idxDummyHcd;
    /** Flag whether this HCD is free for use. */
    bool                       fAvailable;
    /** Number of busses this HCD instance serves. */
    unsigned                   cBusses;
    /** Bus structures the HCD serves.*/
    PUTSPLATFORMLNXDUMMYHCDBUS paBusses;
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
 * Queries the assigned busses for the given dummy HCD instance.
 *
 * @returns IPRT status code.
 * @param   pHcd              The dummy HCD bus instance.
 */
static int utsPlatformLnxDummyHcdQueryBusses(PUTSPLATFORMLNXDUMMYHCD pHcd)
{
    int rc = VINF_SUCCESS;
    char aszPath[RTPATH_MAX + 1];
    unsigned idxBusCur = 0;
    unsigned idxBusMax = 0;

    size_t cchPath = RTStrPrintf(&aszPath[0], RT_ELEMENTS(aszPath), UTS_PLATFORM_LNX_DUMMY_HCD_PATH "/dummy_hcd.%u/usb*", pHcd->idxDummyHcd);
    if (cchPath == RT_ELEMENTS(aszPath))
        return VERR_BUFFER_OVERFLOW;

    PRTDIR pDir = NULL;
    rc = RTDirOpenFiltered(&pDir, aszPath, RTDIRFILTER_WINNT, 0);
    if (RT_SUCCESS(rc))
    {
        do
        {
            RTDIRENTRY DirFolderContent;
            rc = RTDirRead(pDir, &DirFolderContent, NULL);
            if (RT_SUCCESS(rc))
            {
                uint32_t uBusId = 0;

                /* Extract the bus number - it is after "usb", i.e. "usb9" indicates a bus ID of 9. */
                rc = RTStrToUInt32Ex(&DirFolderContent.szName[3], NULL, 10, &uBusId);
                if (RT_SUCCESS(rc))
                {
                    /* Check whether this is a super speed bus. */
                    int64_t iSpeed = 0;
                    bool fSuperSpeed = false;
                    rc = RTLinuxSysFsReadIntFile(10, &iSpeed, UTS_PLATFORM_LNX_DUMMY_HCD_PATH "/dummy_hcd.%u/%s/speed",
                                                 pHcd->idxDummyHcd, DirFolderContent.szName);
                    if (   RT_SUCCESS(rc)
                        && (iSpeed == 5000 || iSpeed == 10000))
                        fSuperSpeed = true;

                    /* Add to array of available busses for this HCD. */
                    if (idxBusCur == idxBusMax)
                    {
                        size_t cbNew = (idxBusMax + 10) * sizeof(UTSPLATFORMLNXDUMMYHCDBUS);
                        PUTSPLATFORMLNXDUMMYHCDBUS pNew = (PUTSPLATFORMLNXDUMMYHCDBUS)RTMemRealloc(pHcd->paBusses, cbNew);
                        if (pNew)
                        {
                            idxBusMax += 10;
                            pHcd->paBusses = pNew;
                        }
                    }

                    if (idxBusCur < idxBusMax)
                    {
                        pHcd->paBusses[idxBusCur].uBusId      = uBusId;
                        pHcd->paBusses[idxBusCur].fSuperSpeed = fSuperSpeed;
                        idxBusCur++;
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
            }
        } while (RT_SUCCESS(rc));

        pHcd->cBusses = idxBusCur;

        if (rc == VERR_NO_MORE_FILES)
            rc = VINF_SUCCESS;

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
        const char *apszArg[] = { "num=2", "is_super_speed=1" }; /** @todo: Make configurable from config. */
        rc = utsPlatformModuleLoad("dummy_hcd", &apszArg[0], RT_ELEMENTS(apszArg));
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
                                    g_paDummyHcd[idxHcdCur].fAvailable  = true;
                                    g_paDummyHcd[idxHcdCur].cBusses     = 0;
                                    g_paDummyHcd[idxHcdCur].paBusses    = NULL;
                                    rc = utsPlatformLnxDummyHcdQueryBusses(&g_paDummyHcd[idxHcdCur]);
                                    if (RT_SUCCESS(rc))
                                        idxHcdCur++;
                                }
                                else
                                    rc = VERR_NO_MEMORY;
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
        papszArgs[2+idx] = papszArgv[idx];
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


DECLHIDDEN(int) utsPlatformLnxAcquireUDC(bool fSuperSpeed, char **ppszUdc, uint32_t *puBusId)
{
    int rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < g_cDummyHcd; i++)
    {
        PUTSPLATFORMLNXDUMMYHCD pHcd = &g_paDummyHcd[i];

        if (pHcd->fAvailable)
        {
            /* Check all assigned busses for a speed match. */
            for (unsigned idxBus = 0; idxBus < pHcd->cBusses; idxBus++)
            {
                if (pHcd->paBusses[idxBus].fSuperSpeed == fSuperSpeed)
                {
                    rc = VINF_SUCCESS;
                    int cbRet = RTStrAPrintf(ppszUdc, "dummy_udc.%u", pHcd->idxDummyHcd);
                    if (cbRet == -1)
                        rc = VERR_NO_STR_MEMORY;
                    *puBusId = pHcd->paBusses[idxBus].uBusId;
                    pHcd->fAvailable = false;
                    break;
                }
            }

            if (rc != VERR_NOT_FOUND)
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

