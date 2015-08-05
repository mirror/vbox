/* $Id$ */
/** @file
 * VBox storage devices: Host block device management API.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOG_GROUP LOG_GROUP_DRV_VD
#include <VBox/cdefs.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/list.h>

#include <iprt/nt/nt-and-windows.h>
#include <Windows.h>

#include "HBDMgmt.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Claimed block device state.
 */
typedef struct HBDMGRDEV
{
    /** List node. */
    RTLISTNODE         ListNode;
    /** The block device name. */
    char              *pszDevice;
    /** Number of mountpoint handles in the array below. */
    unsigned           cMountpointHandles;
    /** Array of mountpoint handles for a particular volume or disk, variable size. */
    HANDLE             aMountpointHandles[1];
} HBDMGRDEV;
/** Pointer to a claimed block device. */
typedef HBDMGRDEV *PHBDMGRDEV;

/**
 * Internal Host block device manager state.
 */
typedef struct HBDMGRINT
{
    /** List of claimed block devices. */
    RTLISTANCHOR       ListClaimed;
    /** Fast mutex protecting the list. */
    RTSEMFASTMUTEX     hMtxList;
} HBDMGRINT;
/** Pointer to an interal block device manager state. */
typedef HBDMGRINT *PHBDMGRINT;

#define HBDMGR_NT_HARDDISK_START "\\Device\\Harddisk"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * Unclaims the given block device and frees its state removing it from the list.
 *
 * @returns nothing.
 * @param   pDev           The block device to unclaim.
 */
static void hbdMgrDevUnclaim(PHBDMGRDEV pDev)
{
    for (unsigned i = 0; i < pDev->cMountpointHandles; i++)
    {
        DWORD dwReturned = 0;
        BOOL bRet = DeviceIoControl(pDev->aMountpointHandles[i], IOCTL_VOLUME_ONLINE, NULL, 0, NULL, 0, &dwReturned, NULL);
        if (!bRet)
            LogRel(("HBDMgmt: Failed to take claimed volume online during cleanup: %s{%Rrc}\n",
                    pDev->pszDevice, RTErrConvertFromWin32(GetLastError())));
    }

    RTListNodeRemove(&pDev->ListNode);
    RTStrFree(pDev->pszDevice);
    RTMemFree(pDev);
}

/**
 * Returns the block device given by the filename if claimed or NULL.
 *
 * @returns Pointer to the claimed block device or NULL if not claimed.
 * @param   pThis          The block device manager.
 * @param   pszFilename    The name to look for. 
 */
static PHBDMGRDEV hbdMgrDevFindByName(PHBDMGRINT pThis, const char *pszFilename)
{
    bool fFound = false;

    PHBDMGRDEV pIt;
    RTListForEach(&pThis->ListClaimed, pIt, HBDMGRDEV, ListNode)
    {
        if (!RTStrCmp(pszFilename, pIt->pszDevice))
        {
            fFound = true;
            break;
        }
    }

    return fFound ? pIt : NULL;
}

/**
 * Queries the target in the NT namespace of the given symbolic link.
 *
 * @returns VBox status code.
 * @param   pwszLinkNt      The symbolic link to query the target for.
 * @param   ppwszLinkTarget Where to store the link target in the NT namespace on success.
 *                          Must be freed with RTUtf16Free().
 */
static int hbdMgrQueryNtLinkTarget(PRTUTF16 pwszLinkNt, PRTUTF16 *ppwszLinkTarget)
{
    int                 rc    = VINF_SUCCESS;
    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    UNICODE_STRING      NtName;

    NtName.Buffer        = (PWSTR)pwszLinkNt;
    NtName.Length        = (USHORT)(RTUtf16Len(pwszLinkNt) * sizeof(RTUTF16));
    NtName.MaximumLength = NtName.Length + sizeof(RTUTF16);

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    NTSTATUS rcNt = NtOpenSymbolicLinkObject(&hFile, SYMBOLIC_LINK_QUERY, &ObjAttr);
    if (NT_SUCCESS(rcNt))
    {
        UNICODE_STRING UniStr;
        RTUTF16 awszBuf[1024];
        RT_ZERO(awszBuf);
        UniStr.Buffer = awszBuf;
        UniStr.MaximumLength = sizeof(awszBuf);
        rcNt = NtQuerySymbolicLinkObject(hFile, &UniStr, NULL);
        if (NT_SUCCESS(rcNt))
        {
            *ppwszLinkTarget = RTUtf16Dup((PRTUTF16)UniStr.Buffer);
            if (!*ppwszLinkTarget)
                rc = VERR_NO_STR_MEMORY;
        }
        else
            rc = RTErrConvertFromNtStatus(rcNt);

        CloseHandle(hFile);
    }
    else
        rc = RTErrConvertFromNtStatus(rcNt);

    return rc;
}

/**
 * Queries the harddisk volume device in the NT namespace for the given Win32
 * block device path.
 *
 * @returns VBox status code.
 * @param   pwszDriveWin32  The Win32 path to the block device (e.g. \\.\PhysicalDrive0 for example)
 * @param   ppwszDriveNt    Where to store the NT path to the volume on success.
 *                          Must be freed with RTUtf16Free().
 */
static int hbdMgrQueryNtName(PRTUTF16 pwszDriveWin32, PRTUTF16 *ppwszDriveNt)
{
    int rc = VINF_SUCCESS;
    RTUTF16 awszFileNt[1024];

    /*
     * Make sure the path is at least 5 characters long so we can safely access
     * the part following \\.\ below.
     */
    AssertReturn(RTUtf16Len(pwszDriveWin32) >= 5, VERR_INVALID_STATE);

    RT_ZERO(awszFileNt);
    RTUtf16CatAscii(awszFileNt, RT_ELEMENTS(awszFileNt), "\\??\\");
    RTUtf16Cat(awszFileNt, RT_ELEMENTS(awszFileNt), &pwszDriveWin32[4]);

    /* Make sure there is no trailing \ at the end or we will fail. */
    size_t cwcPath = RTUtf16Len(awszFileNt);
    if (awszFileNt[cwcPath - 1] == L'\\')
        awszFileNt[cwcPath - 1] = L'\0';

    return hbdMgrQueryNtLinkTarget(awszFileNt, ppwszDriveNt);
}

/**
 * Splits the given mount point buffer into individual entries.
 *
 * @returns VBox status code.
 * @param   pwszMountpoints    The buffer holding the list of mountpoints separated by a null terminator,
 *                             the end of the list is marked with an extra null terminator.
 * @param   ppapwszMountpoints Where to store the array of mount points on success.
 * @param   pcMountpoints      Where to store the returned number of mount points on success.
 */
static int hbdMgrSplitMountpointBuffer(PRTUTF16 pwszMountpoints, PRTUTF16 **ppapwszMountpoints, unsigned *pcMountpoints)
{
    int rc = VINF_SUCCESS;
    unsigned cMountpoints = 0;

    *pcMountpoints = 0;
    *ppapwszMountpoints = NULL;

    /* First round, count the number of mountpoints. */
    PRTUTF16 pwszMountpointCur = pwszMountpoints;
    while (*pwszMountpointCur != L'\0')
    {
        pwszMountpointCur = (PRTUTF16)RTUtf16End(pwszMountpointCur, RTSTR_MAX);
        pwszMountpointCur++;
        cMountpoints++;
    }

    PRTUTF16 *papwszMountpoints = (PRTUTF16 *)RTMemAllocZ(cMountpoints * sizeof(PRTUTF16));
    if (RT_LIKELY(papwszMountpoints))
    {
        unsigned idxMountpoint = 0;

        pwszMountpointCur = pwszMountpoints;

        /* Split the buffer now. */
        while (   *pwszMountpointCur != L'\0'
               && RT_SUCCESS(rc)
               && idxMountpoint < cMountpoints)
        {
            papwszMountpoints[idxMountpoint] = RTUtf16Dup(pwszMountpointCur);
            if (!papwszMountpoints[idxMountpoint])
            {
                rc = VERR_NO_STR_MEMORY;
                break;
            }

            pwszMountpointCur = (PRTUTF16)RTUtf16End(pwszMountpointCur, RTSTR_MAX);
            pwszMountpointCur++;
            idxMountpoint++;
        }

        if (RT_SUCCESS(rc))
        {
            *pcMountpoints = cMountpoints;
            *ppapwszMountpoints = papwszMountpoints;
        }
        else
        {
            /* Undo everything. */
            while (idxMountpoint-- > 0)
                RTUtf16Free(papwszMountpoints[idxMountpoint]);
            RTMemFree(papwszMountpoints);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Queries all available mount points for a given volume.
 *
 * @returns VBox status code.
 * @param   pwszVolNt          The volume to query the mount points for (NT namespace).
 * @param   ppapwszMountpoints Where to store the array of mount points on success.
 * @param   pcMountpoints      Where to store the returned number of mount points on success.
 */
static int hbdMgrQueryMountpointsForVolume(PRTUTF16 pwszVolNt, PRTUTF16 **ppapwszMountpoints, unsigned *pcMountpoints)
{
    int rc = VINF_SUCCESS;
    RTUTF16 awszVolume[64];

    /*
     * Volume names returned by FindFirstVolume/FindNextVolume have a very strict format looking
     * like \\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}, so the buffer for the name can be static.
     */
    RT_ZERO(awszVolume);

    HANDLE hVol = FindFirstVolumeW(&awszVolume[0], RT_ELEMENTS(awszVolume));
    if (hVol != INVALID_HANDLE_VALUE)
    {
        do
        {
            PRTUTF16 pwszVolNtCur = NULL;
            rc = hbdMgrQueryNtName(awszVolume, &pwszVolNtCur);
            if (RT_SUCCESS(rc))
            {
                /* If both volume names match we found the right one. */
                if (!RTUtf16Cmp(pwszVolNtCur, pwszVolNt))
                {
                    DWORD cchBufferLength = 100;
                    DWORD cchBufRequired = 0;
                    PRTUTF16 pwszVolumeMountpoints = RTUtf16Alloc(cchBufferLength * sizeof(RTUTF16));
                    if (pwszVolumeMountpoints)
                    {
                        BOOL bRet = GetVolumePathNamesForVolumeNameW(awszVolume, pwszVolumeMountpoints,
                                                                     cchBufferLength, &cchBufRequired);
                        if (   !bRet
                            && GetLastError() == ERROR_MORE_DATA)
                        {
                            /* Realloc and try again. */
                            RTUtf16Free(pwszVolumeMountpoints);
                            pwszVolumeMountpoints = RTUtf16Alloc(cchBufRequired * sizeof(RTUTF16));
                            if (pwszVolumeMountpoints)
                            {
                                cchBufferLength = cchBufRequired;
                                bRet = GetVolumePathNamesForVolumeNameW(awszVolume, pwszVolumeMountpoints,
                                                                        cchBufferLength, &cchBufRequired);
                            }
                            else
                                rc = VERR_NO_STR_MEMORY;
                        }

                        if (   RT_SUCCESS(rc)
                            && bRet)
                            rc = hbdMgrSplitMountpointBuffer(pwszVolumeMountpoints, ppapwszMountpoints,
                                                             pcMountpoints);
                        else if (RT_SUCCESS(rc))
                            rc = RTErrConvertFromWin32(GetLastError());

                        RTUtf16Free(pwszVolumeMountpoints);
                    }
                    else
                        rc = VERR_NO_STR_MEMORY;

                    break;
                }
                else
                {
                    RTUtf16Free(pwszVolNtCur);
                    RT_ZERO(awszVolume);
                    BOOL bRet = FindNextVolumeW(hVol, &awszVolume[0], RT_ELEMENTS(awszVolume));
                    if (!bRet)
                    {
                        DWORD dwRet = GetLastError();
                        if (dwRet == ERROR_NO_MORE_FILES)
                            rc = VERR_NOT_FOUND;
                        else
                            rc = RTErrConvertFromWin32(dwRet);
                        break;
                    }
                }
            }
        } while (RT_SUCCESS(rc));

        FindVolumeClose(hVol);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}

static int hbdMgrQueryAllMountpointsForDisk(PRTUTF16 pwszDiskNt, PRTUTF16 **ppapwszMountpoints,
                                            unsigned *pcMountpoints)
{
    /*
     * Try to get the symlink target for every partition, we will take the easy approach
     * and just try to open every partition starting with \Device\Harddisk<N>\Partition1
     * (0 is a special reserved partition linking to the complete disk).
     *
     * For every partition we get the target \Device\HarddiskVolume<N> and query all mountpoints
     * with that.
     */
    int rc = VINF_SUCCESS;
    char *pszDiskNt = NULL;
    unsigned cMountpoints = 0;
    PRTUTF16 *papwszMountpoints = NULL;

    rc = RTUtf16ToUtf8(pwszDiskNt, &pszDiskNt);
    if (RT_SUCCESS(rc))
    {
        /* Check that the path matches our expectation \Device\Harddisk<N>\DR<N>. */
        if (!RTStrNCmp(pszDiskNt, HBDMGR_NT_HARDDISK_START, sizeof(HBDMGR_NT_HARDDISK_START) - 1))
        {
            uint32_t iDisk = 0;

            rc = RTStrToUInt32Ex(pszDiskNt + sizeof(HBDMGR_NT_HARDDISK_START) - 1, NULL, 10, &iDisk);
            if (RT_SUCCESS(rc) || rc == VWRN_TRAILING_CHARS)
            {
                uint32_t iPart = 1;

                /* Try to query all mount points for all partitions, the simple way. */
                do
                {
                    char aszDisk[1024];
                    RT_ZERO(aszDisk);

                    size_t cchWritten = RTStrPrintf(&aszDisk[0], sizeof(aszDisk), "\\Device\\Harddisk%u\\Partition%u", iDisk, iPart);
                    if (cchWritten < sizeof(aszDisk))
                    {
                        PRTUTF16 pwszDisk = NULL;
                        rc = RTStrToUtf16(&aszDisk[0], &pwszDisk);
                        if (RT_SUCCESS(rc))
                        {
                            PRTUTF16 pwszTargetNt = NULL;

                            rc = hbdMgrQueryNtLinkTarget(pwszDisk, &pwszTargetNt);
                            if (RT_SUCCESS(rc))
                            {
                                unsigned cMountpointsCur = 0;
                                PRTUTF16 *papwszMountpointsCur = NULL;

                                rc = hbdMgrQueryMountpointsForVolume(pwszTargetNt, &papwszMountpointsCur,
                                                                     &cMountpointsCur);
                                if (RT_SUCCESS(rc))
                                {
                                    if (!papwszMountpoints)
                                    {
                                        papwszMountpoints = papwszMountpointsCur;
                                        cMountpoints = cMountpointsCur;
                                    }
                                    else
                                    {
                                        PRTUTF16 *papwszMountpointsNew = (PRTUTF16 *)RTMemRealloc(papwszMountpoints,
                                                                                                  (cMountpoints + cMountpointsCur) * sizeof(PRTUTF16));
                                        if (papwszMountpointsNew)
                                        {
                                            for (unsigned i = cMountpoints; i < cMountpoints + cMountpointsCur; i++)
                                                papwszMountpointsNew[i] = papwszMountpointsCur[i - cMountpoints];

                                            papwszMountpoints = papwszMountpointsNew;
                                        }
                                        else
                                        {
                                            for (unsigned i = 0; i < cMountpointsCur; i++)
                                                RTUtf16Free(papwszMountpointsCur[i]);

                                            rc = VERR_NO_MEMORY;
                                        }
                                    }

                                    RTMemFree(papwszMountpointsCur);
                                }

                                iPart++;
                                RTUtf16Free(pwszTargetNt);
                            }
                            else if (rc == VERR_FILE_NOT_FOUND)
                            {
                                /* The partition does not exist, so stop trying. */
                                rc = VINF_SUCCESS;
                                break;
                            }

                            RTUtf16Free(pwszDisk);
                        }
                    }
                    else
                        rc = VERR_BUFFER_OVERFLOW;

                } while (RT_SUCCESS(rc));
            }
        }
        else
            rc = VERR_INVALID_STATE;

        RTStrFree(pszDiskNt);
    }

    if (RT_SUCCESS(rc))
    {
        *pcMountpoints = cMountpoints;
        *ppapwszMountpoints = papwszMountpoints;
    }
    else
    {
        for (unsigned i = 0; i < cMountpoints; i++)
            RTUtf16Free(papwszMountpoints[i]);

        RTMemFree(papwszMountpoints);
    }

    return rc;
}

DECLHIDDEN(int) HBDMgrCreate(PHBDMGR phHbdMgr)
{
    AssertPtrReturn(phHbdMgr, VERR_INVALID_POINTER);

    PHBDMGRINT pThis = (PHBDMGRINT)RTMemAllocZ(sizeof(HBDMGRINT));
    if (RT_UNLIKELY(!pThis))
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    RTListInit(&pThis->ListClaimed);

    rc = RTSemFastMutexCreate(&pThis->hMtxList);
    if (RT_SUCCESS(rc))
    {
        *phHbdMgr = pThis;
        return VINF_SUCCESS;
    }

    RTMemFree(pThis);
    return rc;
}

DECLHIDDEN(void) HBDMgrDestroy(HBDMGR hHbdMgr)
{
    PHBDMGRINT pThis = hHbdMgr;
    AssertPtrReturnVoid(pThis);

    /* Go through all claimed block devices and release them. */
    RTSemFastMutexRequest(pThis->hMtxList);
    PHBDMGRDEV pIt, pItNext;
    RTListForEachSafe(&pThis->ListClaimed, pIt, pItNext, HBDMGRDEV, ListNode)
    {
        hbdMgrDevUnclaim(pIt);
    }
    RTSemFastMutexRelease(pThis->hMtxList);

    RTSemFastMutexDestroy(pThis->hMtxList);
    RTMemFree(pThis);
}

DECLHIDDEN(bool) HBDMgrIsBlockDevice(const char *pszFilename)
{
    bool fIsBlockDevice = RTStrNICmp(pszFilename, "\\\\.\\PhysicalDrive", sizeof("\\\\.\\PhysicalDrive") - 1) == 0 ? true : false;
    if (!fIsBlockDevice)
        fIsBlockDevice = RTStrNICmp(pszFilename, "\\\\.\\Harddisk", sizeof("\\\\.\\Harddisk") - 1) == 0 ? true : false;
    return fIsBlockDevice;
}

DECLHIDDEN(int) HBDMgrClaimBlockDevice(HBDMGR hHbdMgr, const char *pszFilename)
{
    PHBDMGRINT pThis = hHbdMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(HBDMgrIsBlockDevice(pszFilename), VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PHBDMGRDEV pDev = hbdMgrDevFindByName(pThis, pszFilename);
    if (!pDev)
    {
        PRTUTF16 pwszVolume = NULL;

        rc = RTStrToUtf16(pszFilename, &pwszVolume);
        if (RT_SUCCESS(rc))
        {
            PRTUTF16 pwszVolNt = NULL;

            rc = hbdMgrQueryNtName(pwszVolume, &pwszVolNt);
            if (RT_SUCCESS(rc))
            {
                PRTUTF16 *papwszMountpoints = NULL;
                unsigned cMountpoints = 0;

                /* Complete disks need to be handled differently. */
                if (!RTStrNCmp(pszFilename, "\\\\.\\PhysicalDrive", sizeof("\\\\.\\PhysicalDrive") - 1))
                    rc = hbdMgrQueryAllMountpointsForDisk(pwszVolNt, &papwszMountpoints, &cMountpoints);
                else
                    rc = hbdMgrQueryMountpointsForVolume(pwszVolNt, &papwszMountpoints, &cMountpoints);

                if (RT_SUCCESS(rc))
                {
                    /** @todo: Unmount everything and take volume offline. */

                    for (unsigned i = 0; i < cMountpoints; i++)
                        RTUtf16Free(papwszMountpoints[i]);

                    RTMemFree(papwszMountpoints);
                }

                RTUtf16Free(pwszVolNt);
            }

            RTUtf16Free(pwszVolume);
        }
    }
    else
        rc = VERR_ALREADY_EXISTS;

    return rc;
}

DECLHIDDEN(int) HBDMgrUnclaimBlockDevice(HBDMGR hHbdMgr, const char *pszFilename)
{
    PHBDMGRINT pThis = hHbdMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    RTSemFastMutexRequest(pThis->hMtxList);
    int rc = VINF_SUCCESS;
    PHBDMGRDEV pDev = hbdMgrDevFindByName(pThis, pszFilename);
    if (pDev)
        hbdMgrDevUnclaim(pDev);
    else
        rc = VERR_NOT_FOUND;
    RTSemFastMutexRelease(pThis->hMtxList);

    return rc;
}

DECLHIDDEN(bool) HBDMgrIsBlockDeviceClaimed(HBDMGR hHbdMgr, const char *pszFilename)
{
    PHBDMGRINT pThis = hHbdMgr;
    AssertPtrReturn(pThis, false);

    RTSemFastMutexRequest(pThis->hMtxList);
    PHBDMGRDEV pIt = hbdMgrDevFindByName(pThis, pszFilename);
    RTSemFastMutexRelease(pThis->hMtxList);

    return pIt ? true : false;
}

