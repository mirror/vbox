/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, information service.
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
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <VBox/HostServices/VBoxInfoSvc.h>  /* For Save and RetrieveVideoMode */

#include "VBGLR3Internal.h"

using namespace svcInfo;

/**
 * Connects to the information service.
 *
 * @returns VBox status code
 * @param   pu32ClientId    Where to put the client id on success. The client id
 *                          must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3InfoSvcConnect(uint32_t *pu32ClientId)
{
    VBoxGuestHGCMConnectInfo Info;
    Info.result = (uint32_t)VERR_WRONG_ORDER; /** @todo drop the cast when the result type has been fixed! */
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    memset(&Info.Loc.u, 0, sizeof(Info.Loc.u));
    strcpy(Info.Loc.u.host.achName, "VBoxSharedInfoSvc");
    Info.u32ClientID = UINT32_MAX;  /* try make valgrid shut up. */

    int rc = vbglR3DoIOCtl(IOCTL_VBOXGUEST_HGCM_CONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
    {
        rc = Info.result;
        if (RT_SUCCESS(rc))
            *pu32ClientId = Info.u32ClientID;
    }
    return rc;
}


/**
 * Disconnect from the information service.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 */
VBGLR3DECL(int) VbglR3InfoSvcDisconnect(uint32_t u32ClientId)
{
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Info.u32ClientID = u32ClientId;

    int rc = vbglR3DoIOCtl(IOCTL_VBOXGUEST_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;
    return rc;
}


/**
 * Write a registry value.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InvsSvcConnect().
 * @param   pszKey          The registry key to save to.
 * @param   pszValue        The value to save.
 */
VBGLR3DECL(int) VbglR3InfoSvcWriteKey(uint32_t u32ClientId, char *pszKey, char *pszValue)
{
    SetConfigKey Msg;

    Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = SET_CONFIG_KEY;
    Msg.hdr.cParms = 2;
    VbglHGCMParmPtrSet(&Msg.key, pszKey, strlen(pszKey) + 1);
    VbglHGCMParmPtrSet(&Msg.value, pszValue, strlen(pszValue) + 1);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    return rc;
}


/**
 * Retrieve a registry value.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3ClipboardConnect().
 * @param   pszKey          The registry key to save to.
 * @param   pcValue         Where to store the value retrieved.
 * @param   cbValue         The size of the array pcValue
 */
VBGLR3DECL(int) VbglR3InfoSvcReadKey(uint32_t u32ClientId, char *pszKey,
                                     char *pcValue, uint32_t cbValue, uint32_t *pcbActual)
{
    GetConfigKey Msg;

    Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GET_CONFIG_KEY;
    Msg.hdr.cParms = 3;
    VbglHGCMParmPtrSet(&Msg.key, pszKey, strlen(pszKey) + 1);
    VbglHGCMParmPtrSet(&Msg.value, pcValue, cbValue);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    uint32_t cbActual;
    if (RT_SUCCESS(rc))
        rc = VbglHGCMParmUInt32Get(&Msg.size, &cbActual);
    if (RT_SUCCESS(rc))
    {
        if (pcbActual != NULL)
            *pcbActual = cbActual;
        if (cbActual > cbValue)
            rc = VINF_BUFFER_OVERFLOW;
        else
            rc = Msg.hdr.result;
    }
    return rc;
}
