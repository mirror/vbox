/* $Id$ */
/** @file
 * innotek Portable Runtime - File System, Win32.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include <windows.h>

#include <iprt/fs.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/assert.h>
#include "internal/fs.h"


/**
 * Checks quickly if this is an correct root specification.
 * Root specs ends with a slash of some kind.
 *
 * @returns indicator.
 * @param   pszFsPath       Path to check.
 */
static bool rtFsIsRoot(const char *pszFsPath)
{
    /*
     * UNC has exactly two slashes..
     *
     * Anything else starting with slashe(s) requires
     * expansion and will have to take the long road.
     */
    if (RTPATH_IS_SLASH(pszFsPath[0]))
    {
        if (    !RTPATH_IS_SLASH(pszFsPath[1])
            ||  RTPATH_IS_SLASH(pszFsPath[2]))
            return false;

        /* end of machine name */
        const char *pszSlash = strpbrk(pszFsPath + 2, "\\/");
        if (!pszSlash)
            return false;

        /* end of service name. */
        pszSlash = strpbrk(pszSlash + 1, "\\/");
        if (!pszSlash)
            return false;

        return pszSlash[1] == '\0';
    }

    /*
     * Ok the other alternative is driver letter.
     */
    return  pszFsPath[0] >= 'A' && pszFsPath[0] <= 'Z'
        &&  pszFsPath[1] == ':'
        &&  RTPATH_IS_SLASH(pszFsPath[2])
        && !pszFsPath[3];
}


#ifndef RT_DONT_CONVERT_FILENAMES
/**
 * Finds the root of the specified volume.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the filesystem. Verified as one byte or more.
 * @param   ppuszFsRoot     Where to store the returned string. Free with rtFsFreeRoot(),
 */
static int rtFsGetRoot(const char *pszFsPath, PRTUCS2 *ppuszFsRoot)
{
    /*
     * Do straight forward stuff first,
     */
    if (rtFsIsRoot(pszFsPath))
        return RTStrUtf8ToUcs2(ppuszFsRoot, pszFsPath);

    /*
     * Expand and add slash (if required).
     */
    char szFullPath[RTPATH_MAX];
    int rc = RTPathAbs(pszFsPath, szFullPath, sizeof(szFullPath));
    if (RT_FAILURE(rc))
        return rc;
    size_t cb = strlen(szFullPath);
    if (!RTPATH_IS_SLASH(szFullPath[cb - 1]))
    {
        AssertReturn(cb + 1 < RTPATH_MAX, VERR_FILENAME_TOO_LONG);
        szFullPath[cb] = '\\';
        szFullPath[++cb] = '\0';
    }

    /*
     * Convert the path.
     */
    rc = RTStrUtf8ToUcs2(ppuszFsRoot, szFullPath);
    if (RT_FAILURE(rc))
        return rc == VERR_BUFFER_OVERFLOW ? VERR_FILENAME_TOO_LONG : rc;

    /*
     * Walk the path until our proper API is happy or there is no more path left.
     */
    PRTUCS2 puszStart = *ppuszFsRoot;
    if (!GetVolumeInformationW(puszStart, NULL, 0, NULL, NULL, 0, NULL, 0))
    {
        PRTUCS2 puszEnd = puszStart + RTStrUcs2Len(puszStart);
        PRTUCS2 puszMin = puszStart + 2;
        do
        {
            /* Strip off the last path component. */
            while (puszEnd-- > puszMin)
                if (RTPATH_IS_SLASH(*puszEnd))
                    break;
            AssertReturn(puszEnd >= puszMin, VERR_INTERNAL_ERROR); /* leaks, but that's irrelevant for an internal error. */
            puszEnd[1] = '\0';
        } while (!GetVolumeInformationW(puszStart, NULL, 0, NULL, NULL, 0, NULL, 0));
    }

    return VINF_SUCCESS;
}

/**
 * Frees string returned by rtFsGetRoot().
 */
static void rtFsFreeRoot(PRTUCS2 puszFsRoot)
{
    RTStrUcs2Free(puszFsRoot);
}

#else

/**
 * Finds the root of the specified volume.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the filesystem. Verified as one byte or more.
 * @param   ppszFsRoot      Where to store the returned string. Free with rtFsFreeRoot(),
 */
static int rtFsGetRoot(const char *pszFsPath, char **ppszFsRoot)
{
    /*
     * Do straight forward stuff first,
     */
    if (rtFsIsRoot(pszFsPath))
        return RTStrDupEx(ppszFsRoot, pszFsPath);

    /*
     * Expand and add slash (if required).
     */
    char szFullPath[RTPATH_MAX];
    int rc = RTPathAbs(pszFsPath, szFullPath, sizeof(szFullPath));
    if (RT_FAILURE(rc))
        return rc;
    size_t cb = strlen(szFullPath);
    if (!RTPATH_IS_SLASH(szFullPath[cb - 1]))
    {
        AssertReturn(cb + 1 < RTPATH_MAX);
        szFullPath[cb] = '\\';
        szFullPath[++cb] = '\0';
    }

    /*
     * Walk the path until our proper API is happy or there is no more path left.
     */
    if (GetVolumeInformation(szFullPath, NULL, 0, NULL, NULL, 0, NULL, 0))
    {
        char *pszEnd = szFullPath + cb;
        char *pszMin = szFullPath + 2;
        do
        {
            /* Strip off the last path component. */
            while (pszEnd-- > pszMin)
                if (RTPATH_IS_SLASH(*pszEnd))
                    break;
            AssertReturn(pszEnd >= pszMin, VERR_INTERNAL_ERROR);
            pszEnd[1] = '\0';
        } while (GetVolumeInformationA(pszStart, NULL, 0, NULL, NULL, 0, NULL, 0));
    }

    return RTStrDupEx(ppszFsRoot, szFullPath);
}

/**
 * Frees string returned by rtFsGetRoot().
 */
static void rtFsFreeRoot(char *pszFsRoot)
{
    RTStrFree(pszFsRoot);
}
#endif


RTR3DECL(int) RTFsQuerySizes(const char *pszFsPath, RTFOFF *pcbTotal, RTFOFF *pcbFree,
                             uint32_t *pcbBlock, uint32_t *pcbSector)
{
    /*
     * Validate & get valid root path.
     */
    AssertMsgReturn(VALID_PTR(pszFsPath) && *pszFsPath, ("%p", pszFsPath), VERR_INVALID_PARAMETER);
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &puszFsRoot);
#else
    char pszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &pszFsRoot);
#endif
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Free and total.
     */
    if (pcbTotal || pcbFree)
    {
        ULARGE_INTEGER cbTotal;
        ULARGE_INTEGER cbFree;
#ifndef RT_DONT_CONVERT_FILENAMES
        if (GetDiskFreeSpaceExW(puszFsRoot, &cbFree, &cbTotal, NULL))
#else
        if (GetDiskFreeSpaceExA(pszFsRoot, &cbFree, &cbTotal, NULL))
#endif
        {
            if (pcbTotal)
                *pcbTotal = cbTotal.QuadPart;
            if (pcbFree)
                *pcbFree  = cbFree.QuadPart;
        }
        else
        {
            DWORD Err = GetLastError();
            rc = RTErrConvertFromWin32(Err);
            Log(("RTFsQuerySizes(%s,): GetDiskFreeSpaceEx failed with lasterr %d (%Vrc)\n",
                 pszFsPath, Err, rc));
        }
    }

    /*
     * Block and sector size.
     */
    if (    RT_SUCCESS(rc)
        &&  (pcbBlock || pcbSector))
    {
        DWORD dwDummy1, dwDummy2;
        DWORD cbSector;
        DWORD cSectorsPerCluster;
#ifndef RT_DONT_CONVERT_FILENAMES
        if (GetDiskFreeSpaceW(puszFsRoot, &cSectorsPerCluster, &cbSector, &dwDummy1, &dwDummy2))
#else
        if (GetDiskFreeSpaceA(pszFsRoot, &cSectorsPerCluster, &cbSector, &dwDummy1, &dwDummy2))
#endif
        {
            if (pcbBlock)
                *pcbBlock = cbSector * cSectorsPerCluster;
            if (pcbSector)
                *pcbSector = cbSector;
        }
        else
        {
            DWORD Err = GetLastError();
            rc = RTErrConvertFromWin32(Err);
            Log(("RTFsQuerySizes(%s,): GetDiskFreeSpace failed with lasterr %d (%Vrc)\n",
                 pszFsPath, Err, rc));
        }
    }

#ifndef RT_DONT_CONVERT_FILENAMES
    rtFsFreeRoot(puszFsRoot);
#else
    rtFsFreeRoot(pszFsRoot);
#endif
    return rc;
}


/**
 * Query the serial number of a filesystem.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pu32Serial      Where to store the serial number.
 */
RTR3DECL(int) RTFsQuerySerial(const char *pszFsPath, uint32_t *pu32Serial)
{
    /*
     * Validate & get valid root path.
     */
    AssertMsgReturn(VALID_PTR(pszFsPath) && *pszFsPath, ("%p", pszFsPath), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pu32Serial), ("%p", pu32Serial), VERR_INVALID_PARAMETER);
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &puszFsRoot);
#else
    char pszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &pszFsRoot);
#endif
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Do work.
     */
    DWORD   dwMaxName;
    DWORD   dwFlags;
    DWORD   dwSerial;
#ifndef RT_DONT_CONVERT_FILENAMES
    if (GetVolumeInformationW(puszFsRoot, NULL, 0, &dwSerial, &dwMaxName, &dwFlags, NULL, 0))
#else
    if (GetVolumeInformationA(pszFsRoot, NULL, 0, &dwSerial, &dwMaxName, &dwFlags, NULL, 0))
#endif
        *pu32Serial = dwSerial;
    else
    {
        DWORD Err = GetLastError();
        rc = RTErrConvertFromWin32(Err);
        Log(("RTFsQuerySizes(%s,): GetDiskFreeSpaceEx failed with lasterr %d (%Vrc)\n",
             pszFsPath, Err, rc));
    }
    return rc;
}


/**
 * Query the properties of a mounted filesystem.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pProperties     Where to store the properties.
 */
RTR3DECL(int) RTFsQueryProperties(const char *pszFsPath, PRTFSPROPERTIES pProperties)
{
    /*
     * Validate & get valid root path.
     */
    AssertMsgReturn(VALID_PTR(pszFsPath) && *pszFsPath, ("%p", pszFsPath), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pProperties), ("%p", pProperties), VERR_INVALID_PARAMETER);
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &puszFsRoot);
#else
    char pszFsRoot;
    int rc = rtFsGetRoot(pszFsPath, &pszFsRoot);
#endif
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Do work.
     */
    DWORD   dwMaxName;
    DWORD   dwFlags;
    DWORD   dwSerial;
#ifndef RT_DONT_CONVERT_FILENAMES
    if (GetVolumeInformationW(puszFsRoot, NULL, 0, &dwSerial, &dwMaxName, &dwFlags, NULL, 0))
#else
    if (GetVolumeInformationA(pszFsRoot, NULL, 0, &dwSerial, &dwMaxName, &dwFlags, NULL, 0))
#endif
    {
        memset(pProperties, 0, sizeof(*pProperties));
        pProperties->cbMaxComponent   = dwMaxName;
        pProperties->fFileCompression = !!(dwFlags & FILE_FILE_COMPRESSION);
        pProperties->fCompressed      = !!(dwFlags & FILE_VOLUME_IS_COMPRESSED);
        pProperties->fReadOnly        = !!(dwFlags & FILE_READ_ONLY_VOLUME);
        pProperties->fSupportsUnicode = !!(dwFlags & FILE_UNICODE_ON_DISK);
        pProperties->fCaseSensitive   = false;    /* win32 is case preserving only */
        pProperties->fRemote          = false;    /* no idea yet */
    }
    else
    {
        DWORD Err = GetLastError();
        rc = RTErrConvertFromWin32(Err);
        Log(("RTFsQuerySizes(%s,): GetVolumeInformation failed with lasterr %d (%Vrc)\n",
             pszFsPath, Err, rc));
    }
    return rc;
}

