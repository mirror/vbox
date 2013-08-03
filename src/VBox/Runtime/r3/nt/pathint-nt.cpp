/* $Id$ */
/** @file
 * IPRT - Native NT, Internal Path stuff.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include "internal-r3-nt.h"

#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/assert.h>



/**
 * Handles the pass thru case.
 *
 * @returns IPRT status code.
 * @param   pNtName             Where to return the NT name.
 * @param   phRootDir           Where to return the root handle, if applicable.
 * @param   pszPath             The UTF-8 path.
 */
static int rtNtPathToNativePassThru(struct _UNICODE_STRING *pNtName, PHANDLE phRootDir, const char *pszPath)
{
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath + 1, &pwszPath);
    if (RT_SUCCESS(rc))
    {
        pwszPath[0] = '\\';
        pwszPath[1] = '.';
        pwszPath[2] = '\\';

        size_t cwcLen = RTUtf16Len(pwszPath);
        if (cwcLen < _32K - 1)
        {
            pNtName->Buffer = pwszPath;
            pNtName->MaximumLength = pNtName->Length = (uint16_t)((cwcLen + 0) * 2);
            *phRootDir = NULL;
            return VINF_SUCCESS;
        }

        RTUtf16Free(pwszPath);
        rc = VERR_FILENAME_TOO_LONG;
    }
    return rc;
}


/**
 * Converts a path to NT format and encoding.
 *
 * @returns IPRT status code.
 * @param   pNtName             Where to return the NT name.
 * @param   phRootDir           Where to return the root handle, if applicable.
 * @param   pszPath             The UTF-8 path.
 */
static int rtNtPathToNative(struct _UNICODE_STRING *pNtName, PHANDLE phRootDir, const char *pszPath)
{
    static char const s_szPrefixUnc[] = "\\??\\UNC\\";
    static char const s_szPrefix[]    = "\\??\\";

    /*
     * Very simple conversion of a win32-like path into an NT path.
     */
    const char *pszPrefix = s_szPrefix;
    size_t      cchPrefix = sizeof(s_szPrefix) - 1;
    size_t      cchSkip   = 0;

    if (   RTPATH_IS_SLASH(pszPath[0])
        && RTPATH_IS_SLASH(pszPath[1])
        && !RTPATH_IS_SLASH(pszPath[2])
        && pszPath[2])
    {
        if (   pszPath[2] == '?'
            && RTPATH_IS_SLASH(pszPath[3]))
            return rtNtPathToNativePassThru(pNtName, phRootDir, pszPath);

        if (   pszPath[2] == '.'
            && RTPATH_IS_SLASH(pszPath[3]))
        {
            /*
             * Device path.
             * Note! I suspect \\.\stuff\..\otherstuff may be handled differently by windows.
             */
            cchSkip   = 4;
        }
        else
        {
            /* UNC */
            pszPrefix = s_szPrefixUnc;
            cchPrefix = sizeof(s_szPrefixUnc) - 1;
            cchSkip   = 2;
        }
    }

    /*
     * Straighten out all .. and uncessary . references and convert slashes.
     */
    char szPath[RTPATH_MAX];
    int rc = RTPathAbs(pszPath, &szPath[cchPrefix - cchSkip], sizeof(szPath) - (cchPrefix - cchSkip));
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Add prefix and convert it to UTF16.
     */
    memcpy(szPath, pszPrefix, cchPrefix);
    PRTUTF16 pwszPath;
    rc = RTStrToUtf16(szPath, &pwszPath);
    if (RT_FAILURE(rc))
        return rc;

    size_t cwcLen = RTUtf16Len(pwszPath);
    if (cwcLen >= _32K - 1)
    {
        RTUtf16Free(pwszPath);
        return VERR_FILENAME_TOO_LONG;
    }

    /*
     * Success.
     */
    pNtName->Buffer = pwszPath;
    pNtName->MaximumLength = pNtName->Length = (uint16_t)((cwcLen + 0) * 2);
    *phRootDir = NULL;
    return VINF_SUCCESS;
}


/**
 * Frees the native path and root handle.
 *
 * @param   pNtName             The NT path after a successful rtNtPathToNative
 *                              call.
 * @param   phRootDir           The root handle variable after a
 *                              rtNtPathToNative.
 */
void rtNtPathFreeNative(struct _UNICODE_STRING *pNtName, PHANDLE phRootDir)
{
    RTUtf16Free(pNtName->Buffer);
    pNtName->Buffer = NULL;
}


/**
 * Wrapper around NtCreateFile.
 *
 * @returns IPRT status code.
 * @param   pszPath             The UTF-8 path.
 * @param   fDesiredAccess      See NtCreateFile.
 * @param   fFileAttribs        See NtCreateFile.
 * @param   fShareAccess        See NtCreateFile.
 * @param   fCreateDisposition  See NtCreateFile.
 * @param   fCreateOptions      See NtCreateFile.
 * @param   fObjAttribs         The OBJECT_ATTRIBUTES::Attributes value, see
 *                              NtCreateFile and InitializeObjectAttributes.
 * @param   phHandle            Where to return the handle.
 * @param   puAction            Where to return the action taken. Optional.
 */
int  rtNtPathOpen(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fFileAttribs, ULONG fShareAccess,
                  ULONG fCreateDisposition, ULONG fCreateOptions, ULONG fObjAttribs,
                  PHANDLE phHandle, PULONG_PTR puAction)
{
    *phHandle = MY_INVALID_HANDLE_VALUE;

    HANDLE         hRootDir;
    UNICODE_STRING NtName;
    int rc = rtNtPathToNative(&NtName, &hRootDir, pszPath);
    if (RT_SUCCESS(rc))
    {
        HANDLE              hFile = MY_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios   = MY_IO_STATUS_BLOCK_INITIALIZER;
        OBJECT_ATTRIBUTES   ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtName, fObjAttribs, hRootDir, NULL);

        NTSTATUS rcNt = NtCreateFile(&hFile,
                                     fDesiredAccess,
                                     &ObjAttr,
                                     &Ios,
                                     NULL /* AllocationSize*/,
                                     fFileAttribs,
                                     fShareAccess,
                                     fCreateDisposition,
                                     fCreateOptions,
                                     NULL /*EaBuffer*/,
                                     0 /*EaLength*/);
        if (NT_SUCCESS(rcNt))
        {
            if (puAction)
                *puAction = Ios.Information;
            *phHandle = hFile;
            rc = VINF_SUCCESS;
        }
        else
            rc = RTErrConvertFromNtStatus(rcNt);
        rtNtPathFreeNative(&NtName, &hRootDir);
    }
    return rc;
}


/**
 * Closes an handled open by rtNtPathOpen.
 *
 * @returns IPRT status code
 * @param   hHandle             The handle value.
 */
int rtNtPathClose(HANDLE hHandle)
{
    NTSTATUS rcNt = NtClose(hHandle);
    if (NT_SUCCESS(rcNt))
        return VINF_SUCCESS;
    return RTErrConvertFromNtStatus(rcNt);
}

