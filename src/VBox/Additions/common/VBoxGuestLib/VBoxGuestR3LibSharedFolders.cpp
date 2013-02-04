/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, shared folders.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/stdarg.h>
#include <VBox/log.h>
#include <VBox/shflsvc.h> /** @todo File should be moved to VBox/HostServices/SharedFolderSvc.h */

#include "VBGLR3Internal.h"


/**
 * Connects to the shared folder service.
 *
 * @returns VBox status code
 * @param   pu32ClientId    Where to put the client id on success. The client id
 *                          must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3SharedFolderConnect(uint32_t *pu32ClientId)
{
    VBoxGuestHGCMConnectInfo Info;
    Info.result = VERR_WRONG_ORDER;
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    RT_ZERO(Info.Loc.u);
    strcpy(Info.Loc.u.host.achName, "VBoxSharedFolders");
    Info.u32ClientID = UINT32_MAX;  /* try make valgrind shut up. */

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
    {
        rc = Info.result;
        if (RT_SUCCESS(rc))
            *pu32ClientId = Info.u32ClientID;
    }
    return rc;
}


/**
 * Disconnect from the shared folder service.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 */
VBGLR3DECL(int) VbglR3SharedFolderDisconnect(uint32_t u32ClientId)
{
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result = VERR_WRONG_ORDER;
    Info.u32ClientID = u32ClientId;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;
    return rc;
}


/**
 * Checks whether a shared folder share exists or not.
 *
 * @returns True if shared folder exists, false if not.
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 * @param   pszShareName    Shared folder name to check.
 */
VBGLR3DECL(bool) VbglR3SharedFolderExists(uint32_t u32ClientId, const char *pszShareName)
{
    AssertPtr(pszShareName);

    uint32_t cMappings;
    VBGLR3SHAREDFOLDERMAPPING *paMappings;

    /** @todo Use some caching here? */
    bool fFound = false;
    int rc = VbglR3SharedFolderGetMappings(u32ClientId, true /* Only process auto-mounted folders */,
                                           &paMappings, &cMappings);
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < cMappings && !fFound; i++)
        {
            char *pszName = NULL;
            rc = VbglR3SharedFolderGetName(u32ClientId, paMappings[i].u32Root, &pszName);
            if (   RT_SUCCESS(rc)
                && *pszName)
            {
                if (RTStrICmp(pszName, pszShareName) == 0)
                    fFound = true;
                RTStrFree(pszName);
            }
        }
        VbglR3SharedFolderFreeMappings(paMappings);
    }
    return fFound;
}


/**
 * Get the list of available shared folders.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3SharedFolderConnect().
 * @param   fAutoMountOnly  Flag whether only auto-mounted shared folders
 *                          should be reported.
 * @param   ppaMappings     Allocated array which will retrieve the mapping info.  Needs
 *                          to be freed with VbglR3SharedFolderFreeMappings() later.
 * @param   pcMappings      The number of mappings returned in @a ppaMappings.
 */
VBGLR3DECL(int) VbglR3SharedFolderGetMappings(uint32_t u32ClientId, bool fAutoMountOnly,
                                              PVBGLR3SHAREDFOLDERMAPPING *ppaMappings, uint32_t *pcMappings)
{
    AssertPtrReturn(pcMappings, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppaMappings, VERR_INVALID_PARAMETER);

    *pcMappings = 0;
    *ppaMappings = NULL;

    VBoxSFQueryMappings Msg;

    Msg.callInfo.result = VERR_WRONG_ORDER;
    Msg.callInfo.u32ClientID = u32ClientId;
    Msg.callInfo.u32Function = SHFL_FN_QUERY_MAPPINGS;
    Msg.callInfo.cParms = 3;

    /* Set the mapping flags. */
    uint32_t u32Flags = 0; /** @todo SHFL_MF_UTF8 is not implemented yet. */
    if (fAutoMountOnly) /* We only want the mappings which get auto-mounted. */
        u32Flags |= SHFL_MF_AUTOMOUNT;
    VbglHGCMParmUInt32Set(&Msg.flags, u32Flags);

    /*
     * Prepare and get the actual mappings from the host service.
     */
    int rc = VINF_SUCCESS;
    uint32_t cMappings = 8; /* Should be a good default value. */
    uint32_t cbSize = cMappings * sizeof(VBGLR3SHAREDFOLDERMAPPING);
    VBGLR3SHAREDFOLDERMAPPING *ppaMappingsTemp = (PVBGLR3SHAREDFOLDERMAPPING)RTMemAllocZ(cbSize);
    if (!ppaMappingsTemp)
        return VERR_NO_MEMORY;

    do
    {
        VbglHGCMParmUInt32Set(&Msg.numberOfMappings, cMappings);
        VbglHGCMParmPtrSet(&Msg.mappings, ppaMappingsTemp, cbSize);

        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            rc = Msg.callInfo.result;
            if (RT_SUCCESS(rc))
            {
                VbglHGCMParmUInt32Get(&Msg.numberOfMappings, pcMappings);

                /* Do we have more mappings than we have allocated space for? */
                if (rc == VINF_BUFFER_OVERFLOW)
                {
                    cMappings = *pcMappings;
                    cbSize = cMappings * sizeof(VBGLR3SHAREDFOLDERMAPPING);
                    void *pvNew = RTMemRealloc(ppaMappingsTemp, cbSize);
                    AssertPtrBreakStmt(pvNew, rc = VERR_NO_MEMORY);
                    ppaMappingsTemp = (PVBGLR3SHAREDFOLDERMAPPING)pvNew;
                }
            }
        }
    } while (rc == VINF_BUFFER_OVERFLOW);

    if (   RT_FAILURE(rc)
        || !*pcMappings)
    {
        RTMemFree(ppaMappingsTemp);
        ppaMappingsTemp = NULL;
    }

    /* In this case, just return success with 0 mappings */
    if (   rc == VERR_INVALID_PARAMETER
        && fAutoMountOnly)
        rc = VINF_SUCCESS;

    *ppaMappings = ppaMappingsTemp;

    return rc;
}


/**
 * Frees the shared folder mappings allocated by
 * VbglR3SharedFolderGetMappings() before.
 *
 * @param   paMappings     What
 */
VBGLR3DECL(void) VbglR3SharedFolderFreeMappings(PVBGLR3SHAREDFOLDERMAPPING paMappings)
{
    if (paMappings)
        RTMemFree(paMappings);
}


/**
 * Get the real name of a shared folder.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InvsSvcConnect().
 * @param   u32Root         Root ID of shared folder to get the name for.
 * @param   ppszName        Where to return the name string.  This shall be
 *                          freed by calling RTStrFree.
 */
VBGLR3DECL(int) VbglR3SharedFolderGetName(uint32_t u32ClientId, uint32_t u32Root, char **ppszName)
{
    AssertPtr(ppszName);

    VBoxSFQueryMapName Msg;

    Msg.callInfo.result = VERR_WRONG_ORDER;
    Msg.callInfo.u32ClientID = u32ClientId;
    Msg.callInfo.u32Function = SHFL_FN_QUERY_MAP_NAME;
    Msg.callInfo.cParms = 2;

    int         rc;
    uint32_t    cbString = sizeof(SHFLSTRING) + SHFL_MAX_LEN;
    PSHFLSTRING pString = (PSHFLSTRING)RTMemAlloc(cbString);
    if (pString)
    {
        RT_ZERO(*pString);
        ShflStringInitBuffer(pString, SHFL_MAX_LEN);

        VbglHGCMParmUInt32Set(&Msg.root, u32Root);
        VbglHGCMParmPtrSet(&Msg.name, pString, cbString);

        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            rc = Msg.callInfo.result;
            if (RT_SUCCESS(rc))
            {
                *ppszName = NULL;
                rc = RTUtf16ToUtf8(&pString->String.ucs2[0], ppszName);
            }
        }
        RTMemFree(pString);
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}

/**
 * Retrieves the prefix for a shared folder mount point.  If no prefix
 * is set in the guest properties "sf_" is returned.
 *
 * @returns VBox status code.
 * @param   ppszPrefix      Where to return the prefix string.  This shall be
 *                          freed by calling RTStrFree.
 */
VBGLR3DECL(int) VbglR3SharedFolderGetMountPrefix(char **ppszPrefix)
{
    AssertPtrReturn(ppszPrefix, VERR_INVALID_POINTER);
    int rc;
#ifdef VBOX_WITH_GUEST_PROPS
    uint32_t u32ClientIdGuestProp;
    rc = VbglR3GuestPropConnect(&u32ClientIdGuestProp);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3GuestPropReadValueAlloc(u32ClientIdGuestProp, "/VirtualBox/GuestAdd/SharedFolders/MountPrefix", ppszPrefix);
        if (rc == VERR_NOT_FOUND) /* No prefix set? Then set the default. */
        {
#endif
            rc = RTStrDupEx(ppszPrefix, "sf_");
#ifdef VBOX_WITH_GUEST_PROPS
        }
        VbglR3GuestPropDisconnect(u32ClientIdGuestProp);
    }
#endif
    return rc;
}

/**
 * Retrieves the mount root directory for auto-mounted shared
 * folders. mount point. If no string is set (VERR_NOT_FOUND)
 * it's up on the caller (guest) to decide where to mount.
 *
 * @returns VBox status code.
 * @param   ppszDir         Where to return the directory
 *                          string. This shall be freed by
 *                          calling RTStrFree.
 */
VBGLR3DECL(int) VbglR3SharedFolderGetMountDir(char **ppszDir)
{
    AssertPtrReturn(ppszDir, VERR_INVALID_POINTER);
    int rc;
#ifdef VBOX_WITH_GUEST_PROPS
    uint32_t u32ClientIdGuestProp;
    rc = VbglR3GuestPropConnect(&u32ClientIdGuestProp);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3GuestPropReadValueAlloc(u32ClientIdGuestProp, "/VirtualBox/GuestAdd/SharedFolders/MountDir", ppszDir);
        VbglR3GuestPropDisconnect(u32ClientIdGuestProp);
    }
#endif
    return rc;
}

