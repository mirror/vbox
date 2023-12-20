/* $Id$ */
/** @file
 * IPRT - File System, RTFsMountpointsEnum, Windows.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/utf16.h>

#include <iprt/win/windows.h>


/**
 * Handles a mountpoint callback with WCHAR (as UTF-16) strings.
 *
 * @returns IPRT status code. Failure terminates the enumeration.
 * @param   pfnCallback         Callback function to invoke.
 * @param   pvUser              User-supplied pointer.
 * @param   pwszStr             WCHAR string to pass to the callback function.
 */
DECLINLINE(int) rtFsWinHandleCallback(PFNRTFSMOUNTPOINTENUM pfnCallback, void *pvUser, PWCHAR pwszStr)
{
    char *psz;
    int rc = RTUtf16ToUtf8((PCRTUTF16)pwszStr, &psz);
    AssertRCReturn(rc, rc);
    rc = pfnCallback(psz, pvUser);
    RTStrFree(psz);
    return rc;
}

/**
 * Mountpoint enumeration worker.
 *
 * This always enumerates all available/connected (disk-based) mountpoints and
 * can be used for other functions to perform (custom) filtering.
 *
 * @returns IPRT status code.
 * @param   fRemote             Also enumerates (currently connected) remote network resources.
 * @param   pfnCallback         Callback function to invoke.
 * @param   pvUser              User-supplied pointer.  Optional and can be NULL.
 */
static int rtFsWinMountpointsEnumWorker(bool fRemote, PFNRTFSMOUNTPOINTENUM pfnCallback, void *pvUser)
{
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);
    /* pvUser is optional. */

    int rc = VINF_SUCCESS;

    SetLastError(0);

    WCHAR wszVol[RTPATH_MAX];
    HANDLE hVol = FindFirstVolumeW(wszVol, RT_ELEMENTS(wszVol));
    if (   hVol
        && hVol != INVALID_HANDLE_VALUE)
    {
        do
        {
            WCHAR wszMp[RTPATH_MAX];
            HANDLE hMp = FindFirstVolumeMountPointW(wszVol, wszMp, RT_ELEMENTS(wszMp));
            if (   hMp
                && hMp != INVALID_HANDLE_VALUE)
            {
                do
                {
                    rc = rtFsWinHandleCallback(pfnCallback, pvUser, wszMp);
                    if (RT_FAILURE(rc))
                        break;
                }
                while (FindNextVolumeMountPointW(hMp, wszMp, RT_ELEMENTS(wszMp)));
                FindVolumeMountPointClose(hMp);
            }
            else if (   GetLastError() != ERROR_NO_MORE_FILES
                     && GetLastError() != ERROR_PATH_NOT_FOUND
                     && GetLastError() != ERROR_UNRECOGNIZED_VOLUME)
                rc = RTErrConvertFromWin32(GetLastError());

            if (RT_SUCCESS(rc))
            {
                DWORD cwch = 0;
                if (GetVolumePathNamesForVolumeNameW(wszVol, wszMp, RT_ELEMENTS(wszMp), &cwch))
                {
                    size_t off = 0;
                    while (off < cwch)
                    {
                        size_t cwc = wcslen(&wszMp[off]);
                        if (cwc)
                        {
                            rc = rtFsWinHandleCallback(pfnCallback, pvUser, &wszMp[off]);
                            if (RT_FAILURE(rc))
                                break;
                        }
                        off += cwc + 1;
                    }
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }

        } while (RT_SUCCESS(rc) && FindNextVolumeW(hVol, wszVol, RT_ELEMENTS(wszVol)));
        FindVolumeClose(hVol);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    if (   RT_SUCCESS(rc)
        && fRemote)
    {
        HANDLE hEnum;
        DWORD dwErr = WNetOpenEnumA(RESOURCE_CONNECTED, RESOURCETYPE_DISK, RESOURCEUSAGE_ALL, NULL, &hEnum);
        if (dwErr == NO_ERROR)
        {
            for (;;)
            {
                DWORD cResources = 1;
                DWORD cbRet = sizeof(wszVol);
                dwErr = WNetEnumResourceW(hEnum, &cResources, wszVol, &cbRet);
                if (dwErr == NO_ERROR)
                {
                    NETRESOURCEW const *pRsrc = (NETRESOURCEW const *)wszVol;
                    if (   pRsrc
                        && pRsrc->lpLocalName)
                    {
                        rc = rtFsWinHandleCallback(pfnCallback, pvUser, (PWCHAR)pRsrc->lpLocalName);
                        if (RT_FAILURE(rc))
                            break;
                    }
                }
                else
                {
                    if (dwErr != ERROR_NO_MORE_ITEMS)
                        rc = RTErrConvertFromWin32(dwErr);
                    break;
                }
            }
        }
        else
            rc = RTErrConvertFromWin32(dwErr);
    }

    return rc;
}


RTR3DECL(int) RTFsMountpointsEnum(PFNRTFSMOUNTPOINTENUM pfnCallback, void *pvUser)
{
    return rtFsWinMountpointsEnumWorker(true /* fRemote */, pfnCallback, pvUser);
}

