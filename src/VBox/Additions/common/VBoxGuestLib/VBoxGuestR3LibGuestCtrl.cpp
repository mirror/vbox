/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, guest control.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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
#include <VBox/HostServices/GuestControlSvc.h>

#include "VBGLR3Internal.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

using namespace guestControl;

/**
 * Connects to the guest control service.
 *
 * @returns VBox status code
 * @param   puClientId    Where to put the client id on success. The client id
 *                        must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3GuestCtrlConnect(uint32_t *puClientId)
{
    VBoxGuestHGCMConnectInfo Info;
    Info.result = VERR_WRONG_ORDER;
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    RT_ZERO(Info.Loc.u);
    strcpy(Info.Loc.u.host.achName, "VBoxGuestControlSvc");
    Info.u32ClientID = UINT32_MAX;  /* try make valgrind shut up. */

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
    {
        rc = Info.result;
        if (RT_SUCCESS(rc))
            *puClientId = Info.u32ClientID;
    }
    return rc;
}


/**
 * Disconnect from the guest control service.
 *
 * @returns VBox status code.
 * @param   uClientId     The client id returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlDisconnect(uint32_t uClientId)
{
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result = VERR_WRONG_ORDER;
    Info.u32ClientID = uClientId;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;
    return rc;
}


/**
 * Waits until a new host message arrives.
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   uClientId       The client id returned by VbglR3GuestCtrlConnect().
 * @param   puMsg           Where to store the message id.
 * @param   puNumParms      Where to store the number  of parameters which will be received
 *                          in a second call to the host.
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgWaitFor(uint32_t uClientId, uint32_t *puMsg, uint32_t *puNumParms)
{
    AssertPtrReturn(puMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(puNumParms, VERR_INVALID_POINTER);

    HGCMMsgCmdWaitFor Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = GUEST_MSG_WAIT; /* Tell the host we want our next command. */
    Msg.hdr.cParms      = 2;              /* Just peek for the next message! */

    VbglHGCMParmUInt32Set(&Msg.msg, 0);
    VbglHGCMParmUInt32Set(&Msg.num_parms, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = VbglHGCMParmUInt32Get(&Msg.msg, puMsg);
        if (RT_SUCCESS(rc))
            rc = VbglHGCMParmUInt32Get(&Msg.num_parms, puNumParms);
            if (RT_SUCCESS(rc))
                rc = Msg.hdr.result;
                /* Ok, so now we know what message type and how much parameters there are. */
    }
    return rc;
}


/**
 * Asks the host guest control service to set a command filter to this
 * client so that it only will receive certain commands in the future.
 *
 * @return  IPRT status code.
 * @param   uClientId       The client id returned by VbglR3GuestCtrlConnect().
 * @param   uFilterAdd      Filter mask to add.
 * @param   uFilterRemove   Filter mask to remove.
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgSetFilter(uint32_t uClientId,
                                            uint32_t uFilterAdd, uint32_t uFilterRemove)
{
    HGCMMsgCmdSetFilter Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = GUEST_MSG_FILTER; /* Tell the host we want to set a filter. */
    Msg.hdr.cParms      = 2;

    VbglHGCMParmUInt32Set(&Msg.add, uFilterAdd);
    VbglHGCMParmUInt32Set(&Msg.remove, uFilterRemove);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}


/**
 * Asks the host to cancel (release) all pending waits which were deferred.
 *
 * @returns VBox status code.
 * @param   uClientId     The client id returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlCancelPendingWaits(uint32_t uClientId)
{
    HGCMMsgCancelPendingWaits Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = GUEST_CANCEL_PENDING_WAITS;
    Msg.hdr.cParms      = 0;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlSessionNotify(uint32_t uClientId, uint32_t uContext,
                                             uint32_t uType, uint32_t uResult)
{
    HGCMMsgSessionNotify Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = GUEST_SESSION_NOTIFY;
    Msg.hdr.cParms      = 3;

    VbglHGCMParmUInt32Set(&Msg.context, uContext);
    VbglHGCMParmUInt32Set(&Msg.type, uType);
    VbglHGCMParmUInt32Set(&Msg.result, uResult);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}


/**
 * Retrieves the request to create a new guest session.
 *
 * @return  IPRT status code.
 * @param   pHostCtx                    Host context.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionGetOpen(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                              uint32_t *puProtocol,
                                              char     *pszUser,     uint32_t  cbUser,
                                              char     *pszPassword, uint32_t  cbPassword,
                                              char     *pszDomain,   uint32_t  cbDomain,
                                              uint32_t *puFlags,     uint32_t *puSessionID)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertReturn(pHostCtx->uNumParms == 6, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puProtocol, VERR_INVALID_POINTER);
    AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDomain, VERR_INVALID_POINTER);
    AssertPtrReturn(puFlags, VERR_INVALID_POINTER);

    HGCMMsgSessionOpen Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.protocol, 0);
    VbglHGCMParmPtrSet(&Msg.username, pszUser, cbUser);
    VbglHGCMParmPtrSet(&Msg.password, pszPassword, cbPassword);
    VbglHGCMParmPtrSet(&Msg.domain, pszDomain, cbDomain);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.protocol.GetUInt32(puProtocol);
            Msg.flags.GetUInt32(puFlags);

            if (puSessionID)
                *puSessionID = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCtx->uContextID);
        }
    }

    return rc;
}


/**
 * Retrieves the request to terminate an existing guest session.
 *
 * @return  IPRT status code.
 * @param   pHostCtx                    Host context.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionGetClose(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puFlags, uint32_t *puSessionID)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertReturn(pHostCtx->uNumParms == 2, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puFlags, VERR_INVALID_POINTER);

    HGCMMsgSessionClose Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.flags.GetUInt32(puFlags);

            if (puSessionID)
                *puSessionID = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCtx->uContextID);
        }
    }

    return rc;
}


/**
 * Allocates and gets host data, based on the message id.
 *
 * This will block until data becomes available.
 *
 * @returns VBox status code.
 ** @todo Docs!
 ** @todo Move the parameters in an own struct!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetStart(PVBGLR3GUESTCTRLHOSTCTX   pHostCtx,
                                            char     *pszCmd,         uint32_t  cbCmd,
                                            uint32_t *puFlags,
                                            char     *pszArgs,        uint32_t  cbArgs,     uint32_t *pcArgs,
                                            char     *pszEnv,         uint32_t *pcbEnv,     uint32_t *pcEnvVars,
                                            char     *pszUser,        uint32_t  cbUser,
                                            char     *pszPassword,    uint32_t  cbPassword,
                                            uint32_t *puTimeoutMS,
                                            uint32_t *puPriority,
                                            uint64_t *puAffinity,     uint32_t  cbAffinity, uint32_t *pcAffinity)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertPtrReturn(pszCmd, VERR_INVALID_POINTER);
    AssertPtrReturn(puFlags, VERR_INVALID_POINTER);
    AssertPtrReturn(pszArgs, VERR_INVALID_POINTER);
    AssertPtrReturn(pcArgs, VERR_INVALID_POINTER);
    AssertPtrReturn(pszEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcEnvVars, VERR_INVALID_POINTER);
    AssertPtrReturn(puTimeoutMS, VERR_INVALID_POINTER);

    HGCMMsgProcExec Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmPtrSet(&Msg.cmd, pszCmd, cbCmd);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
    VbglHGCMParmUInt32Set(&Msg.num_args, 0);
    VbglHGCMParmPtrSet(&Msg.args, pszArgs, cbArgs);
    VbglHGCMParmUInt32Set(&Msg.num_env, 0);
    VbglHGCMParmUInt32Set(&Msg.cb_env, 0);
    VbglHGCMParmPtrSet(&Msg.env, pszEnv, *pcbEnv);
    if (pHostCtx->uProtocol < 2)
    {
        AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
        AssertReturn(cbUser, VERR_INVALID_PARAMETER);
        AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
        AssertReturn(pszPassword, VERR_INVALID_PARAMETER);

        VbglHGCMParmPtrSet(&Msg.u.v1.username, pszUser, cbUser);
        VbglHGCMParmPtrSet(&Msg.u.v1.password, pszPassword, cbPassword);
        VbglHGCMParmUInt32Set(&Msg.u.v1.timeout, 0);
    }
    else
    {
        AssertPtrReturn(puAffinity, VERR_INVALID_POINTER);
        AssertReturn(cbAffinity, VERR_INVALID_PARAMETER);

        VbglHGCMParmUInt32Set(&Msg.u.v2.timeout, 0);
        VbglHGCMParmUInt32Set(&Msg.u.v2.priority, 0);
        VbglHGCMParmUInt32Set(&Msg.u.v2.num_affinity, 0);
        VbglHGCMParmPtrSet(&Msg.u.v2.affinity, puAffinity, cbAffinity);
    }

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.flags.GetUInt32(puFlags);
            Msg.num_args.GetUInt32(pcArgs);
            Msg.num_env.GetUInt32(pcEnvVars);
            Msg.cb_env.GetUInt32(pcbEnv);
            if (pHostCtx->uProtocol < 2)
            {
                Msg.u.v1.timeout.GetUInt32(puTimeoutMS);
            }
            else
            {
                Msg.u.v2.timeout.GetUInt32(puTimeoutMS);
                Msg.u.v2.priority.GetUInt32(puPriority);
                Msg.u.v2.num_affinity.GetUInt32(pcAffinity);
            }
        }
    }
    return rc;
}


/**
 * Allocates and gets host data, based on the message id.
 *
 * This will block until data becomes available.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetOutput(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                             uint32_t *puPID, uint32_t *puHandle, uint32_t *puFlags)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertReturn(pHostCtx->uNumParms == 4, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puPID, VERR_INVALID_POINTER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puFlags, VERR_INVALID_POINTER);

    HGCMMsgProcOutput Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.pid, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.handle.GetUInt32(puHandle);
            Msg.flags.GetUInt32(puFlags);
        }
    }
    return rc;
}


/**
 * Retrieves the input data from host which then gets sent to the
 * started process.
 *
 * This will block until data becomes available.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetInput(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                            uint32_t  *puPID,       uint32_t *puFlags,
                                            void      *pvData,      uint32_t  cbData,
                                            uint32_t  *pcbSize)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertReturn(pHostCtx->uNumParms == 5, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puPID, VERR_INVALID_POINTER);
    AssertPtrReturn(puFlags, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    HGCMMsgProcInput Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.pid, 0);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.flags.GetUInt32(puFlags);
            Msg.size.GetUInt32(pcbSize);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetOpen(PVBGLR3GUESTCTRLHOSTCTX      pHostCtx,
                                           char     *pszFileName,       uint32_t cbFileName,
                                           char     *pszOpenMode,       uint32_t cbOpenMode,
                                           char     *pszDisposition,    uint32_t cbDisposition,
                                           uint32_t *puCreationMode,
                                           uint64_t *puOffset)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertReturn(pHostCtx->uNumParms == 6, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszFileName, VERR_INVALID_POINTER);
    AssertReturn(cbFileName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszOpenMode, VERR_INVALID_POINTER);
    AssertReturn(cbOpenMode, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDisposition, VERR_INVALID_POINTER);
    AssertReturn(cbDisposition, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puCreationMode, VERR_INVALID_POINTER);
    AssertPtrReturn(puOffset, VERR_INVALID_POINTER);

    HGCMMsgFileOpen Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmPtrSet(&Msg.filename, pszFileName, cbFileName);
    VbglHGCMParmPtrSet(&Msg.openmode, pszOpenMode, cbOpenMode);
    VbglHGCMParmPtrSet(&Msg.disposition, pszDisposition, cbDisposition);
    VbglHGCMParmUInt32Set(&Msg.creationmode, 0);
    VbglHGCMParmUInt64Set(&Msg.offset, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.creationmode.GetUInt32(puCreationMode);
            Msg.offset.GetUInt64(puOffset);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetClose(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puHandle)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);

    HGCMMsgFileClose Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetRead(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                           uint32_t *puHandle, uint32_t *puToRead)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puToRead, VERR_INVALID_POINTER);

    HGCMMsgFileRead Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(puToRead);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetReadAt(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                             uint32_t *puHandle, uint32_t *puToRead, uint64_t *puOffset)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 5, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puToRead, VERR_INVALID_POINTER);

    HGCMMsgFileRead Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(puToRead);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetWrite(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puHandle,
                                            void *pvData, uint32_t cbData, uint32_t *pcbSize)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    HGCMMsgFileWrite Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(pcbSize);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetWriteAt(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puHandle,
                                              void *pvData, uint32_t cbData, uint32_t *pcbSize, uint64_t *puOffset)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 5, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    HGCMMsgFileWriteAt Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
    VbglHGCMParmUInt32Set(&Msg.size, 0);
    VbglHGCMParmUInt32Set(&Msg.offset, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(pcbSize);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetSeek(PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                           uint32_t *puHandle, uint32_t *puSeekMethod, uint64_t *puOffset)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puSeekMethod, VERR_INVALID_POINTER);
    AssertPtrReturn(puOffset, VERR_INVALID_POINTER);

    HGCMMsgFileSeek Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);
    VbglHGCMParmUInt32Set(&Msg.method, 0);
    VbglHGCMParmUInt64Set(&Msg.offset, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.method.GetUInt32(puSeekMethod);
            Msg.offset.GetUInt64(puOffset);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileGetTell(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puHandle)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);

    HGCMMsgFileTell Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.handle, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlProcGetTerminate(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puPID)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_POINTER);

    HGCMMsgProcTerminate Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.pid, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlProcGetWaitFor(PVBGLR3GUESTCTRLHOSTCTX pHostCtx, uint32_t *puPID, uint32_t *puWaitFlags, uint32_t *puTimeoutMS)
{
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    AssertReturn(pHostCtx->uNumParms == 5, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_POINTER);

    HGCMMsgProcWaitFor Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = pHostCtx->uClientID;
    Msg.hdr.u32Function = GUEST_MSG_WAIT;
    Msg.hdr.cParms      = pHostCtx->uNumParms;

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmUInt32Set(&Msg.pid, 0);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
    VbglHGCMParmUInt32Set(&Msg.timeout, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
        {
            rc = rc2;
        }
        else
        {
            Msg.context.GetUInt32(&pHostCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.flags.GetUInt32(puWaitFlags);
            Msg.timeout.GetUInt32(puTimeoutMS);
        }
    }
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileNotify(uint32_t     uClientId,    uint32_t     uContext,
                                          uint32_t     uType,
                                          void        *pvPayload,    uint32_t     cbPayload)
{
    AssertPtrReturn(uContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pvPayload, VERR_INVALID_POINTER);
    AssertReturn(cbPayload, VERR_INVALID_PARAMETER);

    HGCMMsgFileNotify Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    //Msg.hdr.u32Function = GUEST_FILE_NOTIFY;
    Msg.hdr.cParms      = 3;

    VbglHGCMParmUInt32Set(&Msg.context, uContext);
    VbglHGCMParmUInt32Set(&Msg.type, uType);
    VbglHGCMParmPtrSet(&Msg.payload, pvPayload, cbPayload);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}


/**
 * Callback for reporting a guest process status (along with some other stuff) to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbStatus(uint32_t     u32ClientID,
                                            uint32_t     u32Context,
                                            uint32_t     u32PID,
                                            uint32_t     u32Status,
                                            uint32_t     u32Flags,
                                            void        *pvData,
                                            uint32_t     cbData)
{
    HGCMMsgProcStatus Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientID;
    Msg.hdr.u32Function = GUEST_EXEC_STATUS;
    Msg.hdr.cParms = 5;

    VbglHGCMParmUInt32Set(&Msg.context, u32Context);
    VbglHGCMParmUInt32Set(&Msg.pid, u32PID);
    VbglHGCMParmUInt32Set(&Msg.status, u32Status);
    VbglHGCMParmUInt32Set(&Msg.flags, u32Flags);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}


/**
 * Sends output (from stdout/stderr) from a running process.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbOutput(uint32_t     u32ClientID,
                                            uint32_t     u32Context,
                                            uint32_t     u32PID,
                                            uint32_t     u32Handle,
                                            uint32_t     u32Flags,
                                            void        *pvData,
                                            uint32_t     cbData)
{
    HGCMMsgProcOutput Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientID;
    Msg.hdr.u32Function = GUEST_EXEC_OUTPUT;
    Msg.hdr.cParms = 5;

    VbglHGCMParmUInt32Set(&Msg.context, u32Context);
    VbglHGCMParmUInt32Set(&Msg.pid, u32PID);
    VbglHGCMParmUInt32Set(&Msg.handle, u32Handle);
    VbglHGCMParmUInt32Set(&Msg.flags, u32Flags);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}


/**
 * Callback for reporting back the input status of a guest process to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbStatusInput(uint32_t     u32ClientID,
                                                 uint32_t     u32Context,
                                                 uint32_t     u32PID,
                                                 uint32_t     u32Status,
                                                 uint32_t     u32Flags,
                                                 uint32_t     cbWritten)
{
    HGCMMsgProcStatusInput Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientID;
    Msg.hdr.u32Function = GUEST_EXEC_INPUT_STATUS;
    Msg.hdr.cParms = 5;

    VbglHGCMParmUInt32Set(&Msg.context, u32Context);
    VbglHGCMParmUInt32Set(&Msg.pid, u32PID);
    VbglHGCMParmUInt32Set(&Msg.status, u32Status);
    VbglHGCMParmUInt32Set(&Msg.flags, u32Flags);
    VbglHGCMParmUInt32Set(&Msg.written, cbWritten);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = Msg.hdr.result;
        if (RT_FAILURE(rc2))
            rc = rc2;
    }
    return rc;
}

