/* $Id$ */
/** @file
 * IPRT - Symbolic Links, Windows.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_SYMLINK

#include <iprt/symlink.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/path.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include "internal/path.h"

#include <Windows.h>


RTDECL(bool) RTSymlinkExists(const char *pszSymlink)
{
    bool        fRc = false;
    RTFSOBJINFO ObjInfo;
    int rc = RTPathQueryInfoEx(pszSymlink, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
    if (RT_SUCCESS(rc))
        fRc = RTFS_IS_SYMLINK(ObjInfo.Attr.fMode);

    LogFlow(("RTSymlinkExists(%p={%s}): returns %RTbool\n", pszSymlink, pszSymlink, fRc));
    return fRc;
}


RTDECL(bool) RTSymlinkIsDangling(const char *pszSymlink)
{
    bool        fRc = false;
    RTFSOBJINFO ObjInfo;
    int rc = RTPathQueryInfoEx(pszSymlink, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
    if (RT_SUCCESS(rc))
    {
        fRc = RTFS_IS_SYMLINK(ObjInfo.Attr.fMode);
        if (fRc)
        {
            rc = RTPathQueryInfoEx(pszSymlink, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_FOLLOW_LINK);
            fRc = RT_SUCCESS(rc);
        }
    }

    LogFlow(("RTSymlinkIsDangling(%p={%s}): returns %RTbool\n", pszSymlink, pszSymlink, fRc));
    return fRc;
}


RTDECL(int) RTSymlinkCreate(const char *pszSymlink, const char *pszTarget, RTSYMLINKTYPE enmType)
{
    /*
     * Validate the input.
     */
    AssertReturn(enmType > RTSYMLINKTYPE_INVALID && enmType < RTSYMLINKTYPE_END, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszSymlink, VERR_INVALID_POINTER);
    AssertPtrReturn(pszTarget, VERR_INVALID_POINTER);

    /*
     * Resolve the API.
     */
    typedef BOOLEAN (WINAPI *PFNCREATESYMBOLICLINKW)(LPCWSTR, LPCWSTR, DWORD);
    static PFNCREATESYMBOLICLINKW   s_pfnCreateSymbolicLinkW = NULL;
    static bool                     s_fTried = FALSE;
    if (!s_fTried)
    {
        HMODULE hmod = LoadLibrary("KERNEL32.DLL");
        if (hmod)
        {
            PFNCREATESYMBOLICLINKW pfn = (PFNCREATESYMBOLICLINKW)GetProcAddress(hmod, "CreateSymbolicLinkW");
            if (pfn)
                s_pfnCreateSymbolicLinkW = pfn;
        }
        s_fTried = true;
    }
    if (!s_pfnCreateSymbolicLinkW)
    {
        LogFlow(("RTSymlinkCreate(%p={%s}, %p={%s}, %d): returns VERR_NOT_SUPPORTED - Windows API not found\n",
                 pszSymlink, pszSymlink, pszTarget, pszTarget, enmType));
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Convert the paths.
     */
    PRTUTF16 pwszNativeSymlink;
    int rc = RTStrToUtf16(pszSymlink, &pwszNativeSymlink);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszNativeTarget;
        rc = RTStrToUtf16(pszTarget, &pwszNativeTarget);
        if (RT_SUCCESS(rc))
        {
            /*
             * Massage the target path, determin the link type.
             */
            size_t cchTarget        = strlen(pszTarget);
            size_t cchVolSpecTarget = rtPathVolumeSpecLen(pszTarget);
            if (   cchTarget > RT_MIN(cchVolSpecTarget, 1)
                && RTPATH_IS_SLASH(pszTarget[cchTarget - 1]))
            {
                size_t cwcNativeTarget = RTUtf16Len(pwszNativeTarget);
                size_t offFromEnd = 1;
                while (   offFromEnd < cchTarget
                       && cchTarget - offFromEnd >= cchVolSpecTarget
                       && RTPATH_IS_SLASH(pszTarget[cchTarget - offFromEnd]))
                {
                    Assert(offFromEnd < cwcNativeTarget);
                    pwszNativeTarget[cwcNativeTarget - offFromEnd] = 0;
                    offFromEnd++;
                }
            }

            if (enmType == RTSYMLINKTYPE_UNKNOWN)
            {
                if (   cchTarget > cchVolSpecTarget
                    && RTPATH_IS_SLASH(pszTarget[cchTarget - 1]))
                    enmType = RTSYMLINKTYPE_DIR;
                else if (cchVolSpecTarget)
                {
                    DWORD dwAttr = GetFileAttributesW(pwszNativeTarget);
                    if (   dwAttr != INVALID_FILE_ATTRIBUTES
                        && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
                        enmType = RTSYMLINKTYPE_DIR;
                }
                else
                {
                    /** @todo Join the symlink directory with the target and
                     *        look up the attributes on that. -lazy bird. */
                }
            }

            /*
             * Create the link.
             */
            if (s_pfnCreateSymbolicLinkW(pwszNativeSymlink, pwszNativeTarget, enmType == RTSYMLINKTYPE_DIR))
                rc = VINF_SUCCESS;
            else
                rc = RTErrConvertFromWin32(GetLastError());

            RTUtf16Free(pwszNativeTarget);
        }
        RTUtf16Free(pwszNativeSymlink);
    }

    LogFlow(("RTSymlinkCreate(%p={%s}, %p={%s}, %d): returns %Rrc\n", pszSymlink, pszSymlink, pszTarget, pszTarget, enmType, rc));
    return rc;
}


RTDECL(int) RTSymlinkDelete(const char *pszSymlink)
{
    /*
     * Convert the path.
     */
    PRTUTF16 pwszNativeSymlink;
    int rc = RTStrToUtf16(pszSymlink, &pwszNativeSymlink);
    if (RT_SUCCESS(rc))
    {
        /*
         * We have to use different APIs depending on whether this is a
         * directory or file link.  This means we're subject to one more race
         * than on posix at the moment.  We could probably avoid this though,
         * if we wanted to go talk with the native API layer below Win32...
         */
        DWORD dwAttr = GetFileAttributesW(pwszNativeSymlink);
        if (dwAttr != INVALID_FILE_ATTRIBUTES)
        {
            if (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                BOOL fRc;
                if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
                    fRc = RemoveDirectoryW(pwszNativeSymlink);
                else
                    fRc = DeleteFileW(pwszNativeSymlink);
                if (fRc)
                    rc = VINF_SUCCESS;
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
            else
                rc = VERR_NOT_SYMLINK;
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
        RTUtf16Free(pwszNativeSymlink);
    }

    LogFlow(("RTSymlinkDelete(%p={%s}): returns %Rrc\n", pszSymlink, pszSymlink, rc));
    return rc;
}


RTDECL(int) RTSymlinkRead(const char *pszSymlink, char *pszTarget, size_t cbTarget)
{
    char *pszMyTarget;
    int rc = RTSymlinkReadA(pszSymlink, &pszMyTarget);
    if (RT_SUCCESS(rc))
    {
        rc = RTStrCopy(pszTarget, cbTarget, pszMyTarget);
        RTStrFree(pszMyTarget);
    }
    LogFlow(("RTSymlinkRead(%p={%s}): returns %Rrc\n", pszSymlink, pszSymlink, rc));
    return rc;
}


RTDECL(int) RTSymlinkReadA(const char *pszSymlink, char **ppszTarget)
{
    AssertPtr(ppszTarget);
    PRTUTF16 pwszNativeSymlink;
    int rc = RTStrToUtf16(pszSymlink, &pwszNativeSymlink);
    if (RT_SUCCESS(rc))
    {
        HANDLE hSymlink = CreateFileW(pwszNativeSymlink,
                                      GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_FLAG_OPEN_REPARSE_POINT,
                                      NULL);
        if (hSymlink != INVALID_HANDLE_VALUE)
        {
            /** @todo what do we do next? FSCTL_GET_REPARSE_POINT? */
            rc = VERR_NOT_IMPLEMENTED;

            CloseHandle(hSymlink);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
        RTUtf16Free(pwszNativeSymlink);
    }

    if (RT_SUCCESS(rc))
        LogFlow(("RTSymlinkReadA(%p={%s},%p): returns %Rrc *ppszTarget=%p:{%s}\n", pszSymlink, pszSymlink, ppszTarget, rc, *ppszTarget, *ppszTarget));
    else
        LogFlow(("RTSymlinkReadA(%p={%s},%p): returns %Rrc\n", pszSymlink, pszSymlink, ppszTarget, rc));
    return rc;
}

