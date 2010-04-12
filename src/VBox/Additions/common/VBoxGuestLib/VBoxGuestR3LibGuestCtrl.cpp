/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, guest control.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
    Info.u32ClientID = UINT32_MAX;  /* try make valgrid shut up. */

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
 * Gets a host message.
 *
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   puMsg           Where to store the message id.
 * @param   puNumParms      Where to store the number  of parameters which will be received
 *                          in a second call to the host.
 */
VBGLR3DECL(int) VbglR3GuestCtrlGetHostMsg(uint32_t u32ClientId, uint32_t *puMsg, uint32_t *puNumParms)
{
    AssertPtr(puMsg);
    AssertPtr(puNumParms);

    VBoxGuestCtrlHGCMMsgType Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG; /* Tell the host we want our next command. */
    Msg.hdr.cParms = 2;                       /* Just peek for the next message! */

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
 * Allocates and gets host data, based on the message id.
 *
 * This will block until data becomes available.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3GuestCtrlConnect().
 * @param   ppvData
 * @param   uNumParms
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlExecGetHostCmd(uint32_t u32ClientId, uint32_t uNumParms,
                                              char    *pszCmd,      uint32_t cbCmd,
                                              uint32_t *puFlags,
                                              char *pszArgs,        uint32_t cbArgs,  uint32_t *puNumArgs,
                                              char *pszEnv,         uint32_t *pcbEnv, uint32_t *puNumEnvVars,
                                              char *pszStdIn,       uint32_t cbStdIn,
                                              char *pszStdOut,      uint32_t cbStdOut,
                                              char *pszStdErr,      uint32_t cbStdErr,
                                              char *pszUser,        uint32_t cbUser,
                                              char *pszPassword,    uint32_t cbPassword,
                                              uint32_t *puTimeLimit)
{
    AssertPtr(pszCmd);
    AssertPtr(puFlags);
    AssertPtr(pszArgs);
    AssertPtr(puNumArgs);
    AssertPtr(pszEnv);
    AssertPtr(pcbEnv);
    AssertPtr(puNumEnvVars);
    AssertPtr(pszStdIn);
    AssertPtr(pszStdOut);
    AssertPtr(pszStdOut);
    AssertPtr(pszStdErr);
    AssertPtr(pszUser);
    AssertPtr(pszPassword);
    AssertPtr(puTimeLimit);

    VBoxGuestCtrlHGCMMsgExecCmd Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_GET_HOST_MSG;
    Msg.hdr.cParms = uNumParms;

    VbglHGCMParmPtrSet(&Msg.cmd, pszCmd, cbCmd);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
    VbglHGCMParmUInt32Set(&Msg.num_args, 0);
    VbglHGCMParmPtrSet(&Msg.args, pszArgs, cbArgs);
    VbglHGCMParmUInt32Set(&Msg.num_env, 0);
    VbglHGCMParmUInt32Set(&Msg.cb_env, 0);
    VbglHGCMParmPtrSet(&Msg.env, pszEnv, *pcbEnv);
    VbglHGCMParmPtrSet(&Msg.std_in, pszStdIn, cbStdIn);
    VbglHGCMParmPtrSet(&Msg.std_out, pszStdOut, cbStdOut);
    VbglHGCMParmPtrSet(&Msg.std_err, pszStdErr, cbStdErr);
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
            Msg.flags.GetUInt32(puFlags);
            Msg.num_args.GetUInt32(puNumArgs);
            Msg.num_env.GetUInt32(puNumEnvVars);
            Msg.cb_env.GetUInt32(pcbEnv);
            Msg.timeout.GetUInt32(puTimeLimit);
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
VBGLR3DECL(int) VbglR3GuestCtrlExecReportStatus(uint32_t  u32ClientId, 
                                                uint32_t  u32PID,
                                                uint32_t  u32Status,
                                                uint32_t  u32Flags,
                                                void     *pvData,
                                                uint32_t  cbData)
{
    VBoxGuestCtrlHGCMMsgExecStatus Msg;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GUEST_EXEC_SEND_STATUS;
    Msg.hdr.cParms = 4;

    VbglHGCMParmUInt32Set(&Msg.pid, 0);
    VbglHGCMParmUInt32Set(&Msg.status, 0);
    VbglHGCMParmUInt32Set(&Msg.flags, 0);
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

