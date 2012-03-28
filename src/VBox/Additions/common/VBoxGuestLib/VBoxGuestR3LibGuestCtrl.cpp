/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, guest control.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
 * @param   pu32ClientId    Where to put the client id on success. The client id
 *                          must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3GuestCtrlConnect(uint32_t *pu32ClientId)
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
            *pu32ClientId = Info.u32ClientID;
    }
    return rc;
}


/**
 * Disconnect from the guest control service.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlDisconnect(uint32_t u32ClientId)
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
 * Waits until a new host message arrives.
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   puMsg           Where to store the message id.
 * @param   puNumParms      Where to store the number  of parameters which will be received
 *                          in a second call to the host.
 */
VBGLR3DECL(int) VbglR3GuestCtrlWaitForHostMsg(uint32_t u32ClientId, uint32_t *puMsg, uint32_t *puNumParms)
{
    AssertPtrReturn(puMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(puNumParms, VERR_INVALID_POINTER);

    VBoxGuestCtrlHGCMMsgType Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG; /* Tell the host we want our next command. */
    Msg.hdr.cParms      = 2;                  /* Just peek for the next message! */

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
 * Asks the host to cancel (release) all pending waits which were deferred.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlCancelPendingWaits(uint32_t u32ClientId)
{
    VBoxGuestCtrlHGCMMsgCancelPendingWaits Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
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


/**
 * Allocates and gets host data, based on the message id.
 *
 * This will block until data becomes available.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   uNumParms
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecGetHostCmdExec(uint32_t  u32ClientId,    uint32_t  cParms,
                                                  uint32_t *puContext,
                                                  char     *pszCmd,         uint32_t  cbCmd,
                                                  uint32_t *puFlags,
                                                  char     *pszArgs,        uint32_t  cbArgs,   uint32_t *pcArgs,
                                                  char     *pszEnv,         uint32_t *pcbEnv,   uint32_t *pcEnvVars,
                                                  char     *pszUser,        uint32_t  cbUser,
                                                  char     *pszPassword,    uint32_t  cbPassword,
                                                  uint32_t *pcMsTimeLimit)
{
    AssertPtrReturn(puContext, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszCmd, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puFlags, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszArgs, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcArgs, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszEnv, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbEnv, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcEnvVars, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszUser, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcMsTimeLimit, VERR_INVALID_PARAMETER);

    VBoxGuestCtrlHGCMMsgExecCmd Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG;
    Msg.hdr.cParms      = cParms; /** @todo r=bird: This isn't safe/right. The parameter count of this HGCM call
                                   * is fixed from our point of view. */

    VbglHGCMParmUInt32Set(&Msg.context, 0);
    VbglHGCMParmPtrSet(&Msg.cmd, pszCmd, cbCmd);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
    VbglHGCMParmUInt32Set(&Msg.num_args, 0);
    VbglHGCMParmPtrSet(&Msg.args, pszArgs, cbArgs);
    VbglHGCMParmUInt32Set(&Msg.num_env, 0);
    VbglHGCMParmUInt32Set(&Msg.cb_env, 0);
    VbglHGCMParmPtrSet(&Msg.env, pszEnv, *pcbEnv);
    VbglHGCMParmPtrSet(&Msg.username, pszUser, cbUser);
    VbglHGCMParmPtrSet(&Msg.password, pszPassword, cbPassword);
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
            Msg.context.GetUInt32(puContext);
            Msg.flags.GetUInt32(puFlags);
            Msg.num_args.GetUInt32(pcArgs);
            Msg.num_env.GetUInt32(pcEnvVars);
            Msg.cb_env.GetUInt32(pcbEnv);
            Msg.timeout.GetUInt32(pcMsTimeLimit);
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
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   cParms
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecGetHostCmdOutput(uint32_t  u32ClientId,    uint32_t  cParms,
                                                    uint32_t *puContext,      uint32_t *puPID,
                                                    uint32_t *puHandle,       uint32_t *puFlags)
{
    AssertPtrReturn(puContext, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puFlags, VERR_INVALID_PARAMETER);

    VBoxGuestCtrlHGCMMsgExecOut Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG;
    Msg.hdr.cParms = cParms;

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
            Msg.context.GetUInt32(puContext);
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
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   cParms
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecGetHostCmdInput(uint32_t  u32ClientId,    uint32_t   cParms,
                                                   uint32_t *puContext,      uint32_t  *puPID,
                                                   uint32_t *puFlags,
                                                   void     *pvData,         uint32_t  cbData,
                                                   uint32_t *pcbSize)
{
    AssertPtrReturn(puContext, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puFlags, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbSize, VERR_INVALID_PARAMETER);

    VBoxGuestCtrlHGCMMsgExecIn Msg;

    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG;
    Msg.hdr.cParms      = cParms;

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
            Msg.context.GetUInt32(puContext);
            Msg.pid.GetUInt32(puPID);
            Msg.flags.GetUInt32(puFlags);
            Msg.size.GetUInt32(pcbSize);
        }
    }
    return rc;
}


/**
 * Reports the process status (along with some other stuff) to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecReportStatus(uint32_t     u32ClientId,
                                                uint32_t     u32Context,
                                                uint32_t     u32PID,
                                                uint32_t     u32Status,
                                                uint32_t     u32Flags,
                                                void        *pvData,
                                                uint32_t     cbData)
{
    VBoxGuestCtrlHGCMMsgExecStatus Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_EXEC_SEND_STATUS;
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
VBGLR3DECL(int) VbglR3GuestCtrlExecSendOut(uint32_t     u32ClientId,
                                           uint32_t     u32Context,
                                           uint32_t     u32PID,
                                           uint32_t     u32Handle,
                                           uint32_t     u32Flags,
                                           void        *pvData,
                                           uint32_t     cbData)
{
    VBoxGuestCtrlHGCMMsgExecOut Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_EXEC_SEND_OUTPUT;
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
 * Reports back the input status to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecReportStatusIn(uint32_t     u32ClientId,
                                                  uint32_t     u32Context,
                                                  uint32_t     u32PID,
                                                  uint32_t     u32Status,
                                                  uint32_t     u32Flags,
                                                  uint32_t     cbWritten)
{
    VBoxGuestCtrlHGCMMsgExecStatusIn Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_EXEC_SEND_INPUT_STATUS;
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
