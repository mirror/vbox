/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Host Channel.
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

 
#include <iprt/mem.h>

#include <VBox/HostServices/VBoxHostChannel.h>

#include "VBGLR3Internal.h"


VBGLR3DECL(int) VbglR3HostChannelInit(uint32_t *pu32HGCMClientId)
{
    VBoxGuestHGCMConnectInfo connectInfo;
    RT_ZERO(connectInfo);

    connectInfo.result = VERR_WRONG_ORDER;
    connectInfo.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy(connectInfo.Loc.u.host.achName, "VBoxHostChannel");
    connectInfo.u32ClientID = 0;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CONNECT, &connectInfo, sizeof(connectInfo));

    if (RT_SUCCESS(rc))
    {
        rc = connectInfo.result;

        if (RT_SUCCESS(rc))
        {
            *pu32HGCMClientId = connectInfo.u32ClientID;
        }
    }

    return rc;
}

VBGLR3DECL(void) VbglR3HostChannelTerm(uint32_t u32HGCMClientId)
{
    VBoxGuestHGCMDisconnectInfo disconnectInfo;
    disconnectInfo.result = VERR_WRONG_ORDER;
    disconnectInfo.u32ClientID = u32HGCMClientId;

    vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &disconnectInfo, sizeof(disconnectInfo));
}

VBGLR3DECL(int) VbglR3HostChannelAttach(uint32_t *pu32ChannelHandle,
                                        uint32_t u32HGCMClientId,
                                        const char *pszName,
                                        uint32_t u32Flags)
{
    /* Make a heap copy of the name, because HGCM can not use some of other memory types. */
    size_t cbName = strlen(pszName) + 1;
    char *pszCopy = (char *)RTMemAlloc(cbName);
    if (pszCopy == NULL)
    {
        return VERR_NO_MEMORY;
    }

    memcpy(pszCopy, pszName, cbName);

    VBoxHostChannelAttach parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_ATTACH;
    parms.hdr.cParms = 3;

    VbglHGCMParmPtrSet(&parms.name, pszCopy, (uint32_t)cbName);
    VbglHGCMParmUInt32Set(&parms.flags, u32Flags);
    VbglHGCMParmUInt32Set(&parms.handle, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;
        if (RT_SUCCESS(rc))
        {
            *pu32ChannelHandle = parms.handle.u.value32;
        }
    }

    RTMemFree(pszCopy);

    return rc;
}

VBGLR3DECL(void) VbglR3HostChannelDetach(uint32_t u32ChannelHandle,
                                         uint32_t u32HGCMClientId)
{
    VBoxHostChannelDetach parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_DETACH;
    parms.hdr.cParms = 1;

    VbglHGCMParmUInt32Set(&parms.handle, u32ChannelHandle);

    vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));
}

VBGLR3DECL(int) VbglR3HostChannelSend(uint32_t u32ChannelHandle,
                                      uint32_t u32HGCMClientId,
                                      void *pvData,
                                      uint32_t cbData)
{
    VBoxHostChannelSend parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_SEND;
    parms.hdr.cParms = 2;

    VbglHGCMParmUInt32Set(&parms.handle, u32ChannelHandle);
    VbglHGCMParmPtrSet(&parms.data, pvData, cbData);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;
    }

    return rc;
}

VBGLR3DECL(int) VbglR3HostChannelRecv(uint32_t u32ChannelHandle,
                                      uint32_t u32HGCMClientId,
                                      void *pvData,
                                      uint32_t cbData,
                                      uint32_t *pu32SizeReceived,
                                      uint32_t *pu32SizeRemaining)
{
    VBoxHostChannelRecv parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_RECV;
    parms.hdr.cParms = 4;

    VbglHGCMParmUInt32Set(&parms.handle, u32ChannelHandle);
    VbglHGCMParmPtrSet(&parms.data, pvData, cbData);
    VbglHGCMParmUInt32Set(&parms.sizeReceived, 0);
    VbglHGCMParmUInt32Set(&parms.sizeRemaining, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;

        if (RT_SUCCESS(rc))
        {
            *pu32SizeReceived = parms.sizeReceived.u.value32;
            *pu32SizeRemaining = parms.sizeRemaining.u.value32;
        }
    }

    return rc;
}

VBGLR3DECL(int) VbglR3HostChannelControl(uint32_t u32ChannelHandle,
                                         uint32_t u32HGCMClientId,
                                         uint32_t u32Code,
                                         void *pvParm,
                                         uint32_t cbParm,
                                         void *pvData,
                                         uint32_t cbData,
                                         uint32_t *pu32SizeDataReturned)
{
    VBoxHostChannelControl parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_CONTROL;
    parms.hdr.cParms = 5;

    VbglHGCMParmUInt32Set(&parms.handle, u32ChannelHandle);
    VbglHGCMParmUInt32Set(&parms.code, u32Code);
    VbglHGCMParmPtrSet(&parms.parm, pvParm, cbParm);
    VbglHGCMParmPtrSet(&parms.data, pvData, cbData);
    VbglHGCMParmUInt32Set(&parms.sizeDataReturned, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;

        if (RT_SUCCESS(rc))
        {
            *pu32SizeDataReturned = parms.sizeDataReturned.u.value32;
        }
    }

    return rc;
}

VBGLR3DECL(int) VbglR3HostChannelEventWait(uint32_t *pu32ChannelHandle,
                                           uint32_t u32HGCMClientId,
                                           uint32_t *pu32EventId,
                                           void *pvParm,
                                           uint32_t cbParm,
                                           uint32_t *pu32SizeReturned)
{
    VBoxHostChannelEventWait parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_EVENT_WAIT;
    parms.hdr.cParms = 4;

    VbglHGCMParmUInt32Set(&parms.handle, 0);
    VbglHGCMParmUInt32Set(&parms.id, 0);
    VbglHGCMParmPtrSet(&parms.parm, pvParm, cbParm);
    VbglHGCMParmUInt32Set(&parms.sizeReturned, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;

        if (RT_SUCCESS(rc))
        {
            *pu32ChannelHandle = parms.handle.u.value32;
            *pu32EventId = parms.id.u.value32;
            *pu32SizeReturned = parms.sizeReturned.u.value32;
        }
    }

    return rc;
}

VBGLR3DECL(int) VbglR3HostChannelEventCancel(uint32_t u32ChannelHandle,
                                             uint32_t u32HGCMClientId)
{
    VBoxHostChannelEventCancel parms;

    parms.hdr.result = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32HGCMClientId;
    parms.hdr.u32Function = VBOX_HOST_CHANNEL_FN_EVENT_CANCEL;
    parms.hdr.cParms = 0;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(parms)), &parms, sizeof(parms));

    if (RT_SUCCESS(rc))
    {
        rc = parms.hdr.result;
    }

    return rc;
}
