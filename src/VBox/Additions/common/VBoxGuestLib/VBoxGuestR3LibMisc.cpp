/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Misc.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include "VBGLR3Internal.h"


/**
 * Wait for the host to signal one or more events and return which.
 *
 * The events will only be delivered by the host if they have been enabled
 * previously using @a VbglR3CtlFilterMask.  If one or several of the events
 * have already been signalled but not yet waited for, this function will return
 * immediately and return those events.
 *
 * @returns IPRT status code
 *
 * @param   fMask       The events we want to wait for, or-ed together.
 * @param   cMillies    How long to wait before giving up and returning
 *                      (VERR_TIMEOUT). Use RT_INDEFINITE_WAIT to wait until we
 *                      are interrupted or one of the events is signalled.
 * @param   pfEvents    Where to store the events signalled. Optional.
 */
VBGLR3DECL(int) VbglR3WaitEvent(uint32_t fMask, uint32_t cMillies, uint32_t *pfEvents)
{
    LogFlow(("VbglR3WaitEvent: fMask=0x%x, cMillies=%u, pfEvents=%p\n",
             fMask, cMillies, pfEvents));
    AssertReturn((fMask & ~VMMDEV_EVENT_VALID_EVENT_MASK) == 0, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfEvents, VERR_INVALID_POINTER);

    VBoxGuestWaitEventInfo waitEvent;
    waitEvent.u32TimeoutIn = cMillies;
    waitEvent.u32EventMaskIn = fMask;
    waitEvent.u32Result = VBOXGUEST_WAITEVENT_ERROR;
    waitEvent.u32EventFlagsOut = 0;
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent));
    if (RT_SUCCESS(rc))
    {
        AssertMsg(waitEvent.u32Result == VBOXGUEST_WAITEVENT_OK, ("%d\n", waitEvent.u32Result));
        if (pfEvents)
            *pfEvents = waitEvent.u32EventFlagsOut;
    }

    LogFlow(("VbglR3WaitEvent: rc=%Rrc, u32EventFlagsOut=0x%x. u32Result=%d\n",
             rc, waitEvent.u32EventFlagsOut, waitEvent.u32Result));
    return rc;
}


/**
 * Cause any pending WaitEvent calls (VBOXGUEST_IOCTL_WAITEVENT) to return
 * with a VERR_INTERRUPTED status.
 *
 * Can be used in combination with a termination flag variable for interrupting
 * event loops. Avoiding race conditions is the responsibility of the caller.
 *
 * @returns IPRT status code
 */
VBGLR3DECL(int) VbglR3InterruptEventWaits(void)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS, 0, 0);
}


/**
 * Write to the backdoor logger from ring 3 guest code.
 *
 * @returns IPRT status code
 *
 * @remarks This currently does not accept more than 255 bytes of data at
 *          one time. It should probably be rewritten to use pass a pointer
 *          in the IOCtl.
 */
VBGLR3DECL(int) VbglR3WriteLog(const char *pch, size_t cb)
{
    /*
     * Quietly skip NULL strings.
     * (Happens in the RTLogBackdoorPrintf case.)
     */
    if (!cb)
        return VINF_SUCCESS;
    if (!VALID_PTR(pch))
        return VERR_INVALID_POINTER;

#ifdef RT_OS_WINDOWS
    /*
     * Duplicate the string as it may be read only (a C string).
     */
    void *pvTmp = RTMemDup(pch, cb);
    if (!pvTmp)
        return VERR_NO_MEMORY;
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cb), pvTmp, cb);
    RTMemFree(pvTmp);
    return rc;

#elif 0 /** @todo Several OSes could take this route (solaris and freebsd for instance). */
    /*
     * Handle the entire request in one go.
     */
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cb), pvTmp, cb);

#else
    /*
     * *BSD does not accept more than 4KB per ioctl request, while
     * Linux can't express sizes above 8KB, so, split it up into 2KB chunks.
     */
# define STEP 2048
    int rc = VINF_SUCCESS;
    for (size_t off = 0; off < cb && RT_SUCCESS(rc); off += STEP)
    {
        size_t cbStep = RT_MIN(cb - off, STEP);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cbStep), (char *)pch + off, cbStep);
    }
# undef STEP
    return rc;
#endif
}


/**
 * Change the IRQ filter mask.
 *
 * @returns IPRT status code
 * @param   fOr     The OR mask.
 * @param   fNo     The NOT mask.
 */
VBGLR3DECL(int) VbglR3CtlFilterMask(uint32_t fOr, uint32_t fNot)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Not yet implemented. */
    return VERR_NOT_SUPPORTED;

#else

    VBoxGuestFilterMaskInfo Info;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CTL_FILTER_MASK, &Info, sizeof(Info));
#endif
}


/**
 * Report a change in the capabilities that we support to the host.
 *
 * @returns IPRT status value
 * @param   fOr     Capabilities which have been added.
 * @param   fNot    Capabilities which have been removed.
 *
 * @todo    Move to a different file.
 */
VBGLR3DECL(int) VbglR3SetGuestCaps(uint32_t fOr, uint32_t fNot)
{
    VMMDevReqGuestCapabilities2 Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_SetGuestCapabilities);
    Req.u32OrMask = fOr;
    Req.u32NotMask = fNot;
    int rc = vbglR3GRPerform(&Req.header);
#ifdef DEBUG
    if (RT_SUCCESS(rc))
        LogRel(("Successfully changed guest capabilities: or mask 0x%x, not mask 0x%x.\n", fOr, fNot));
    else
        LogRel(("Failed to change guest capabilities: or mask 0x%x, not mask 0x%x.  rc=%Rrc.\n", fOr, fNot, rc));
#endif
    return rc;
}


/**
 * Fallback for vbglR3GetAdditionsVersion.
 */
static int vbglR3GetAdditionsCompileTimeVersion(char **ppszVer, char **ppszRev)
{
    if (ppszVer)
    {
        *ppszVer = RTStrDup(VBOX_VERSION_STRING);
        if (!*ppszVer)
            return VERR_NO_STR_MEMORY;
    }

    if (ppszRev)
    {
        char szRev[64];
        RTStrPrintf(szRev, sizeof(szRev), "%d", VBOX_SVN_REV);
        *ppszRev = RTStrDup(szRev);
        if (!*ppszRev)
        {
            if (ppszVer)
            {
                RTStrFree(*ppszVer);
                *ppszVer = NULL;
            }
            return VERR_NO_STR_MEMORY;
        }
    }

    return VINF_SUCCESS;
}


#ifdef RT_OS_WINDOWS
/**
 * Looks up the storage path handle (registry).
 *
 * @returns IPRT status value
 * @param   hKey        Receives pointer of allocated version string. NULL is
 *                      accepted. The returned pointer must be closed by
 *                      vbglR3CloseAdditionsWinStoragePath().
 */
static int vbglR3GetAdditionsWinStoragePath(PHKEY phKey)
{
    /*
     * Try get the *installed* version first.
     */
    LONG r;

    /* Check the new path first. */
    r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
    if (r != ERROR_SUCCESS)
    {
        /* Check Wow6432Node (for new entries). */
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
    }
# endif

    /* Still no luck? Then try the old xVM paths ... */
    if (r != ERROR_SUCCESS)
    {
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
        if (r != ERROR_SUCCESS)
        {
            /* Check Wow6432Node (for new entries). */
            r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, phKey);
        }
# endif
    }
    return RTErrConvertFromNtStatus(r);
}


/**
 * Closes the storage path handle (registry).
 *
 * @returns IPRT status value
 * @param   hKey        Handle to close, retrieved by
 *                      vbglR3GetAdditionsWinStoragePath().
 */
static int vbglR3CloseAdditionsWinStoragePath(HKEY hKey)
{
    return RTErrConvertFromNtStatus(RegCloseKey(hKey));
}
#endif /* RT_OS_WINDOWS */ 


/**
 * Retrieves the installed Guest Additions version and/or revision.
 *
 * @returns IPRT status value
 * @param   ppszVer     Receives pointer of allocated version string. NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 * @param   ppszRev     Receives pointer of allocated revision string. NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 */
VBGLR3DECL(int) VbglR3GetAdditionsVersion(char **ppszVer, char **ppszRev)
{
#ifdef RT_OS_WINDOWS
    HKEY hKey;
    int rc = vbglR3GetAdditionsWinStoragePath(&hKey);
    if (RT_SUCCESS(rc))
    {
        /* Version. */
        LONG l;
        DWORD dwType;
        DWORD dwSize = 32;
        char *pszTmp;
        if (ppszVer)
        {
            pszTmp = (char*)RTMemAlloc(dwSize);
            if (pszTmp)
            {
                l = RegQueryValueEx(hKey, "Version", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
                if (l == ERROR_SUCCESS)
                {
                    if (dwType == REG_SZ)
                        rc = RTStrDupEx(ppszVer, pszTmp);
                    else
                        rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = RTErrConvertFromNtStatus(l);
                }
                RTMemFree(pszTmp);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        /* Revision. */
        if (ppszRev)
        {
            dwSize = 32; /* Reset */
            pszTmp = (char*)RTMemAlloc(dwSize);
            if (pszTmp)
            {
                l = RegQueryValueEx(hKey, "Revision", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
                if (l == ERROR_SUCCESS)
                {
                    if (dwType == REG_SZ)
                        rc = RTStrDupEx(ppszRev, pszTmp);
                    else
                        rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = RTErrConvertFromNtStatus(l);
                }
                RTMemFree(pszTmp);
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_FAILURE(rc) && ppszVer)
            {
                RTStrFree(*ppszVer);
                *ppszVer = NULL;
            }
        }
        rc = vbglR3CloseAdditionsWinStoragePath(hKey);
    }
    else
    {
        /*
         * No registry entries found, return the version string compiled
         * into this binary.
         */
        rc = vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszRev);
    }
    return rc;

#else  /* !RT_OS_WINDOWS */
    /*
     * On non-Windows platforms just return the compile-time version string.
     */
    return vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszRev);
#endif /* !RT_OS_WINDOWS */
}


/**
 * Retrieves the installation path of Guest Additions.
 *
 * @returns IPRT status value
 * @param   ppszPath    Receives pointer of allocated installation path string.
 *                      The returned pointer must be freed using
 *                      RTStrFree().
 */
VBGLR3DECL(int) VbglR3GetAdditionsInstallationPath(char **ppszPath)
{
    int rc;
#ifdef RT_OS_WINDOWS
    HKEY hKey;
    rc = vbglR3GetAdditionsWinStoragePath(&hKey);
    if (RT_SUCCESS(rc))
    {
        /* Installation directory. */
        DWORD dwType;
        DWORD dwSize = _MAX_PATH * sizeof(char);
        char *pszTmp = (char*)RTMemAlloc(dwSize + 1);
        if (pszTmp)
        {
            LONG l = RegQueryValueEx(hKey, "InstallDir", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
            if ((l != ERROR_SUCCESS) && (l != ERROR_FILE_NOT_FOUND))
            {
                rc = RTErrConvertFromNtStatus(l);
            }
            else
            {
                if (dwType == REG_SZ)
                    rc = RTStrDupEx(ppszPath, pszTmp);
                else
                    rc = VERR_INVALID_PARAMETER;
                if (RT_SUCCESS(rc))
                {
                    /* Flip slashes. */
                    for (char *pszTmp2 = ppszPath[0]; *pszTmp2; ++pszTmp2)
                        if (*pszTmp2 == '\\')
                            *pszTmp2 = '/';
                }
            }
            RTMemFree(pszTmp);
        }
        else
            rc = VERR_NO_MEMORY;
        rc = vbglR3CloseAdditionsWinStoragePath(hKey);
    }
#else
    /** @todo implement me */
    rc = VERR_NOT_IMPLEMENTED;
#endif
    return rc;
}
