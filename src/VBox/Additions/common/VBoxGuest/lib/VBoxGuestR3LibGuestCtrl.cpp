/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, guest control.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/stdarg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/HostServices/GuestControlSvc.h>

#ifndef RT_OS_WINDOWS
# include <signal.h>
#endif

#include "VBoxGuestR3LibInternal.h"

using namespace guestControl;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Set if GUEST_MSG_PEEK_WAIT and friends are supported. */
static int g_fVbglR3GuestCtrlHavePeekGetCancel = -1;


/**
 * Connects to the guest control service.
 *
 * @returns VBox status code
 * @param   pidClient     Where to put The client ID on success. The client ID
 *                        must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3GuestCtrlConnect(uint32_t *pidClient)
{
    return VbglR3HGCMConnect("VBoxGuestControlSvc", pidClient);
}


/**
 * Disconnect from the guest control service.
 *
 * @returns VBox status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlDisconnect(uint32_t idClient)
{
    return VbglR3HGCMDisconnect(idClient);
}


/**
 * Waits until a new host message arrives.
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 * @param   pidMsg          Where to store the message id.
 * @param   pcParameters    Where to store the number  of parameters which will
 *                          be received in a second call to the host.
 */
static int vbglR3GuestCtrlMsgWaitFor(uint32_t idClient, uint32_t *pidMsg, uint32_t *pcParameters)
{
    AssertPtrReturn(pidMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(pcParameters, VERR_INVALID_POINTER);

    HGCMMsgCmdWaitFor Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       GUEST_MSG_WAIT,      /* Tell the host we want our next command. */
                       2);                  /* Just peek for the next message! */
    VbglHGCMParmUInt32Set(&Msg.msg, 0);
    VbglHGCMParmUInt32Set(&Msg.num_parms, 0);

    /*
     * We should always get a VERR_TOO_MUCH_DATA response here, see
     * guestControl::HostCommand::Peek() and its caller ClientState::SendReply().
     * We accept success too here, in case someone decide to make the protocol
     * slightly more sane.
     *
     * Note! A really sane protocol design would have a separate call for getting
     *       info about a pending message (returning VINF_SUCCESS), and a separate
     *       one for retriving the actual message parameters.  Not this weird
     *       stuff, to put it rather bluntly.
     *
     * Note! As a result of this weird design, we are not able to correctly
     *       retrieve message if we're interrupted by a signal, like SIGCHLD.
     *       Because IPRT wants to use waitpid(), we're forced to have a handler
     *       installed for SIGCHLD, so when working with child processes there
     *       will be signals in the air and we will get VERR_INTERRUPTED returns.
     *       The way HGCM handles interrupted calls is to silently (?) drop them
     *       as they complete (see VMMDev), so the server knows little about it
     *       and just goes on to the next message inline.
     *
     *       So, as a "temporary" mesasure, we block SIGCHLD here while waiting,
     *       because it will otherwise be impossible do simple stuff like 'mkdir'
     *       on a mac os x guest, and probably most other unix guests.
     */
#ifdef RT_OS_WINDOWS
    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
#else
    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGCHLD);
    sigprocmask(SIG_BLOCK, &SigSet, NULL);
    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    sigprocmask(SIG_UNBLOCK, &SigSet, NULL);
#endif
    if (   rc == VERR_TOO_MUCH_DATA
        || RT_SUCCESS(rc))
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.msg, pidMsg);
        if (RT_SUCCESS(rc2))
        {
            rc2 = VbglHGCMParmUInt32Get(&Msg.num_parms, pcParameters);
            if (RT_SUCCESS(rc2))
            {
                /* Ok, so now we know what message type and how much parameters there are. */
                return rc;
            }
        }
        rc = rc2;
    }
    *pidMsg       = UINT32_MAX - 1;
    *pcParameters = UINT32_MAX - 2;
    return rc;
}


/**
 * Determins the value of g_fVbglR3GuestCtrlHavePeekGetCancel.
 *
 * @returns true if supported, false if not.
 * @param   idClient         The client ID to use for the testing.
 */
DECL_NO_INLINE(static, bool) vbglR3GuestCtrlDetectPeekGetCancelSupport(uint32_t idClient)
{
    /*
     * Seems we get VINF_SUCCESS back from the host if we try unsupported
     * guest control functions, so we need to supply some random message
     * parameters and check that they change.
     */
    uint32_t const idDummyMsg      = UINT32_C(0x8350bdca);
    uint32_t const cDummyParmeters = UINT32_C(0x7439604f);
    uint32_t const cbDummyMask     = UINT32_C(0xc0ffe000);
    Assert(cDummyParmeters > VMMDEV_MAX_HGCM_PARMS);

    int rc;
    struct
    {
        VBGLIOCHGCMCALL         Hdr;
        HGCMFunctionParameter   idMsg;
        HGCMFunctionParameter   cParams;
        HGCMFunctionParameter   acbParams[14];
    } PeekCall;
    Assert(RT_ELEMENTS(PeekCall.acbParams) + 2 < VMMDEV_MAX_HGCM_PARMS);

    do
    {
        memset(&PeekCall, 0xf6, sizeof(PeekCall));
        VBGL_HGCM_HDR_INIT(&PeekCall.Hdr, idClient, GUEST_MSG_PEEK_NOWAIT, 16);
        VbglHGCMParmUInt32Set(&PeekCall.idMsg, idDummyMsg);
        VbglHGCMParmUInt32Set(&PeekCall.cParams, cDummyParmeters);
        for (uint32_t i = 0; i < RT_ELEMENTS(PeekCall.acbParams); i++)
            VbglHGCMParmUInt32Set(&PeekCall.acbParams[i], i | cbDummyMask);

        rc = VbglR3HGCMCall(&PeekCall.Hdr, sizeof(PeekCall));
    } while (rc == VERR_INTERRUPTED);

    LogRel2(("vbglR3GuestCtrlDetectPeekGetCancelSupport: rc=%Rrc %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x\n",
             rc, PeekCall.idMsg.u.value32,     PeekCall.cParams.u.value32,
             PeekCall.acbParams[ 0].u.value32, PeekCall.acbParams[ 1].u.value32,
             PeekCall.acbParams[ 2].u.value32, PeekCall.acbParams[ 3].u.value32,
             PeekCall.acbParams[ 4].u.value32, PeekCall.acbParams[ 5].u.value32,
             PeekCall.acbParams[ 6].u.value32, PeekCall.acbParams[ 7].u.value32,
             PeekCall.acbParams[ 8].u.value32, PeekCall.acbParams[ 9].u.value32,
             PeekCall.acbParams[10].u.value32, PeekCall.acbParams[11].u.value32,
             PeekCall.acbParams[12].u.value32, PeekCall.acbParams[13].u.value32));

    /*
     * VERR_TRY_AGAIN is likely and easy.
     */
    if (   rc == VERR_TRY_AGAIN
        && PeekCall.idMsg.u.value32 == 0
        && PeekCall.cParams.u.value32 == 0
        && PeekCall.acbParams[0].u.value32 == 0
        && PeekCall.acbParams[1].u.value32 == 0
        && PeekCall.acbParams[2].u.value32 == 0
        && PeekCall.acbParams[3].u.value32 == 0)
    {
        g_fVbglR3GuestCtrlHavePeekGetCancel = 1;
        LogRel(("vbglR3GuestCtrlDetectPeekGetCancelSupport: Supported (#1)\n"));
        return true;
    }

    /*
     * VINF_SUCCESS is annoying but with 16 parameters we've got plenty to check.
     */
    if (   rc == VINF_SUCCESS
        && PeekCall.idMsg.u.value32 != idDummyMsg
        && PeekCall.idMsg.u.value32 != 0
        && PeekCall.cParams.u.value32 <= VMMDEV_MAX_HGCM_PARMS)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(PeekCall.acbParams); i++)
            if (PeekCall.acbParams[i].u.value32 != (i | cbDummyMask))
            {
                g_fVbglR3GuestCtrlHavePeekGetCancel = 0;
                LogRel(("vbglR3GuestCtrlDetectPeekGetCancelSupport: Not supported (#1)\n"));
                return false;
            }
        g_fVbglR3GuestCtrlHavePeekGetCancel = 1;
        LogRel(("vbglR3GuestCtrlDetectPeekGetCancelSupport: Supported (#2)\n"));
        return true;
    }

    /*
     * Okay, pretty sure it's not supported then.
     */
    LogRel(("vbglR3GuestCtrlDetectPeekGetCancelSupport: Not supported (#3)\n"));
    g_fVbglR3GuestCtrlHavePeekGetCancel = 0;
    return false;
}


/**
 * Reads g_fVbglR3GuestCtrlHavePeekGetCancel and resolved -1.
 *
 * @returns true if supported, false if not.
 * @param   idClient         The client ID to use for the testing.
 */
DECLINLINE(bool) vbglR3GuestCtrlSupportsPeekGetCancel(uint32_t idClient)
{
    int fState = g_fVbglR3GuestCtrlHavePeekGetCancel;
    if (RT_LIKELY(fState != -1))
        return fState != 0;
    return vbglR3GuestCtrlDetectPeekGetCancelSupport(idClient);
}


/**
 * Figures which getter function to use to retrieve the message.
 */
DECLINLINE(uint32_t) vbglR3GuestCtrlGetMsgFunctionNo(uint32_t idClient)
{
    return vbglR3GuestCtrlSupportsPeekGetCancel(idClient) ? GUEST_MSG_GET : GUEST_MSG_WAIT;
}


/**
 * Checks if the host supports the optimizes message and session functions.
 *
 * @returns true / false.
 * @param   idClient    The client ID returned by VbglR3GuestCtrlConnect().
 *                      We may need to use this for checking.
 * @since   6.0
 */
VBGLR3DECL(bool) VbglR3GuestCtrlSupportsOptimizations(uint32_t idClient)
{
    return vbglR3GuestCtrlSupportsPeekGetCancel(idClient);
}


/**
 * Make us the guest control master client.
 *
 * @returns VBox status code.
 * @param   idClient    The client ID returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlMakeMeMaster(uint32_t idClient)
{
    int rc;
    do
    {
        VBGLIOCHGCMCALL Hdr;
        VBGL_HGCM_HDR_INIT(&Hdr, idClient, GUEST_MAKE_ME_MASTER, 0);
        rc = VbglR3HGCMCall(&Hdr, sizeof(Hdr));
    } while (rc == VERR_INTERRUPTED);
    return rc;
}


/**
 * Peeks at the next host message, waiting for one to turn up.
 *
 * @returns VBox status code.
 * @retval  VERR_INTERRUPTED if interrupted.  Does the necessary cleanup, so
 *          caller just have to repeat this call.
 * @retval  VERR_VM_RESTORED if the VM has been restored (idRestoreCheck).
 *
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 * @param   pidMsg          Where to store the message id.
 * @param   pcParameters    Where to store the number  of parameters which will
 *                          be received in a second call to the host.
 * @param   pidRestoreCheck Pointer to the VbglR3GetSessionId() variable to use
 *                          for the VM restore check.  Optional.
 *
 * @note    Restore check is only performed optimally with a 6.0 host.
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgPeekWait(uint32_t idClient, uint32_t *pidMsg, uint32_t *pcParameters, uint64_t *pidRestoreCheck)
{
    AssertPtrReturn(pidMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(pcParameters, VERR_INVALID_POINTER);

    int rc;
    if (vbglR3GuestCtrlSupportsPeekGetCancel(idClient))
    {
        struct
        {
            VBGLIOCHGCMCALL Hdr;
            HGCMFunctionParameter idMsg;       /* Doubles as restore check on input. */
            HGCMFunctionParameter cParameters;
        } Msg;
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_MSG_PEEK_WAIT, 2);
        VbglHGCMParmUInt64Set(&Msg.idMsg, pidRestoreCheck ? *pidRestoreCheck : 0);
        VbglHGCMParmUInt32Set(&Msg.cParameters, 0);
        rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
        LogRel2(("VbglR3GuestCtrlMsgPeekWait -> %Rrc\n", rc));
        if (RT_SUCCESS(rc))
        {
            AssertMsgReturn(   Msg.idMsg.type       == VMMDevHGCMParmType_64bit
                            && Msg.cParameters.type == VMMDevHGCMParmType_32bit,
                            ("msg.type=%d num_parms.type=%d\n", Msg.idMsg.type, Msg.cParameters.type),
                            VERR_INTERNAL_ERROR_3);

            *pidMsg       = (uint32_t)Msg.idMsg.u.value64;
            *pcParameters = Msg.cParameters.u.value32;
            return rc;
        }

        /*
         * If interrupted we must cancel the call so it doesn't prevent us from making another one.
         */
        if (rc == VERR_INTERRUPTED)
        {
            VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_MSG_CANCEL, 0);
            int rc2 = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg.Hdr));
            AssertRC(rc2);
        }

        /*
         * If restored, update pidRestoreCheck.
         */
        if (rc == VERR_VM_RESTORED && pidRestoreCheck)
            *pidRestoreCheck = Msg.idMsg.u.value64;

        *pidMsg       = UINT32_MAX - 1;
        *pcParameters = UINT32_MAX - 2;
        return rc;
    }

    /*
     * Fallback if host < v6.0.
     *
     * Note! The restore check isn't perfect. Would require checking afterwards
     *       and stash the result if we were restored during the call.  Too much
     *       hazzle for a downgrade scenario.
     */
    if (pidRestoreCheck)
    {
        uint64_t idRestoreCur = *pidRestoreCheck;
        rc = VbglR3GetSessionId(&idRestoreCur);
        if (RT_SUCCESS(rc) && idRestoreCur != *pidRestoreCheck)
        {
            *pidRestoreCheck = idRestoreCur;
            return VERR_VM_RESTORED;
        }
    }

    rc = vbglR3GuestCtrlMsgWaitFor(idClient, pidMsg, pcParameters);
    if (rc == VERR_TOO_MUCH_DATA)
        rc = VINF_SUCCESS;
    return rc;
}


/**
 * Asks the host guest control service to set a command filter to this
 * client so that it only will receive certain commands in the future.
 * The filter(s) are a bitmask for the context IDs, served from the host.
 *
 * @return  IPRT status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 * @param   uValue          The value to filter messages for.
 * @param   uMaskAdd        Filter mask to add.
 * @param   uMaskRemove     Filter mask to remove.
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgFilterSet(uint32_t idClient, uint32_t uValue, uint32_t uMaskAdd, uint32_t uMaskRemove)
{
    HGCMMsgCmdFilterSet Msg;

    /* Tell the host we want to set a filter. */
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, GUEST_MSG_FILTER_SET, 4);
    VbglHGCMParmUInt32Set(&Msg.value, uValue);
    VbglHGCMParmUInt32Set(&Msg.mask_add, uMaskAdd);
    VbglHGCMParmUInt32Set(&Msg.mask_remove, uMaskRemove);
    VbglHGCMParmUInt32Set(&Msg.flags, 0 /* Flags, unused */);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


VBGLR3DECL(int) VbglR3GuestCtrlMsgReply(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                        int rc)
{
    return VbglR3GuestCtrlMsgReplyEx(pCtx, rc, 0 /* uType */,
                                     NULL /* pvPayload */, 0 /* cbPayload */);
}


VBGLR3DECL(int) VbglR3GuestCtrlMsgReplyEx(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                          int rc, uint32_t uType,
                                          void *pvPayload, uint32_t cbPayload)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    /* Everything else is optional. */

    HGCMMsgCmdReply Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_MSG_REPLY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, uType);
    VbglHGCMParmUInt32Set(&Msg.rc, (uint32_t)rc); /* int vs. uint32_t */
    VbglHGCMParmPtrSet(&Msg.payload, pvPayload, cbPayload);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Tell the host to skip the current message replying VERR_NOT_SUPPORTED
 *
 * @return  IPRT status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 * @param   rcSkip          The status code to pass back to Main when skipping.
 * @param   idMsg           The message ID to skip, pass UINT32_MAX to pass any.
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgSkip(uint32_t idClient, int rcSkip, uint32_t idMsg)
{
    if (vbglR3GuestCtrlSupportsPeekGetCancel(idClient))
    {
        struct
        {
            VBGLIOCHGCMCALL         Hdr;
            HGCMFunctionParameter   rcSkip;
            HGCMFunctionParameter   idMsg;
        } Msg;
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_MSG_SKIP, 2);
        VbglHGCMParmUInt32Set(&Msg.rcSkip, (uint32_t)rcSkip);
        VbglHGCMParmUInt32Set(&Msg.idMsg, idMsg);
        return VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    }

    /* This is generally better than nothing... */
    return VbglR3GuestCtrlMsgSkipOld(idClient);
}


/**
 * Tells the host service to skip the current message returned by
 * VbglR3GuestCtrlMsgWaitFor().
 *
 * @return  IPRT status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlMsgSkipOld(uint32_t idClient)
{
    HGCMMsgCmdSkip Msg;

    /* Tell the host we want to skip the current assigned command. */
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, GUEST_MSG_SKIP_OLD, 1);
    VbglHGCMParmUInt32Set(&Msg.flags, 0 /* Flags, unused */);
    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


/**
 * Asks the host to cancel (release) all pending waits which were deferred.
 *
 * @returns VBox status code.
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 */
VBGLR3DECL(int) VbglR3GuestCtrlCancelPendingWaits(uint32_t idClient)
{
    HGCMMsgCancelPendingWaits Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, GUEST_MSG_CANCEL, 0);
    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


/**
 * Prepares a session.
 * @since   6.0
 * @sa      GUEST_SESSION_PREPARE
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionPrepare(uint32_t idClient, uint32_t idSession, void const *pvKey, uint32_t cbKey)
{
    int rc;
    do
    {
        struct
        {
            VBGLIOCHGCMCALL         Hdr;
            HGCMFunctionParameter   idSession;
            HGCMFunctionParameter   pKey;
        } Msg;
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_SESSION_PREPARE, 2);
        VbglHGCMParmUInt32Set(&Msg.idSession, idSession);
        VbglHGCMParmPtrSet(&Msg.pKey, (void *)pvKey, cbKey);
        rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    } while (rc == VERR_INTERRUPTED);
    return rc;
}


/**
 * Accepts a session.
 * @since   6.0
 * @sa      GUEST_SESSION_ACCEPT
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionAccept(uint32_t idClient, uint32_t idSession, void const *pvKey, uint32_t cbKey)
{
    int rc;
    do
    {
        struct
        {
            VBGLIOCHGCMCALL         Hdr;
            HGCMFunctionParameter   idSession;
            HGCMFunctionParameter   pKey;
        } Msg;
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_SESSION_ACCEPT, 2);
        VbglHGCMParmUInt32Set(&Msg.idSession, idSession);
        VbglHGCMParmPtrSet(&Msg.pKey, (void *)pvKey, cbKey);
        rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    } while (rc == VERR_INTERRUPTED);
    return rc;
}


/**
 * Cancels a prepared session.
 * @since   6.0
 * @sa      GUEST_SESSION_CANCEL_PREPARED
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionCancelPrepared(uint32_t idClient, uint32_t idSession)
{
    int rc;
    do
    {
        struct
        {
            VBGLIOCHGCMCALL         Hdr;
            HGCMFunctionParameter   idSession;
        } Msg;
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, GUEST_SESSION_CANCEL_PREPARED, 1);
        VbglHGCMParmUInt32Set(&Msg.idSession, idSession);
        rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    } while (rc == VERR_INTERRUPTED);
    return rc;
}


/**
 * Asks a specific guest session to close.
 *
 * @return  IPRT status code.
 * @param   pCtx                    Host context.
 * @param   fFlags                  Some kind of flag. Figure it out yourself.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionClose(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t fFlags)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 2, VERR_INVALID_PARAMETER);

    HGCMMsgSessionClose Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_SESSION_CLOSE, pCtx->uNumParms);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.flags, fFlags);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


VBGLR3DECL(int) VbglR3GuestCtrlSessionNotify(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t uType, int32_t iResult)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMMsgSessionNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_SESSION_NOTIFY, 3);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, uType);
    VbglHGCMParmUInt32Set(&Msg.result, (uint32_t)iResult);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


/**
 * Retrieves a HOST_SESSION_CREATE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionGetOpen(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                              uint32_t *puProtocol,
                                              char     *pszUser,     uint32_t  cbUser,
                                              char     *pszPassword, uint32_t  cbPassword,
                                              char     *pszDomain,   uint32_t  cbDomain,
                                              uint32_t *pfFlags,     uint32_t *pidSession)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 6, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puProtocol, VERR_INVALID_POINTER);
    AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDomain, VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgSessionOpen Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_SESSION_CREATE);
        VbglHGCMParmUInt32Set(&Msg.protocol, 0);
        VbglHGCMParmPtrSet(&Msg.username, pszUser, cbUser);
        VbglHGCMParmPtrSet(&Msg.password, pszPassword, cbPassword);
        VbglHGCMParmPtrSet(&Msg.domain, pszDomain, cbDomain);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.protocol.GetUInt32(puProtocol);
            Msg.flags.GetUInt32(pfFlags);

            if (pidSession)
                *pidSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pCtx->uContextID);
        }

    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_SESSION_CLOSE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlSessionGetClose(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *pfFlags, uint32_t *pidSession)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 2, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgSessionClose Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_SESSION_CLOSE);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.flags.GetUInt32(pfFlags);

            if (pidSession)
                *pidSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pCtx->uContextID);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_PATH_RENAME message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlPathGetRename(PVBGLR3GUESTCTRLCMDCTX     pCtx,
                                             char     *pszSource,       uint32_t cbSource,
                                             char     *pszDest,         uint32_t cbDest,
                                             uint32_t *pfFlags)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 4, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    AssertReturn(cbSource, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    AssertReturn(cbDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgPathRename Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_PATH_RENAME);
        VbglHGCMParmPtrSet(&Msg.source, pszSource, cbSource);
        VbglHGCMParmPtrSet(&Msg.dest, pszDest, cbDest);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.flags.GetUInt32(pfFlags);
        }

    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_PATH_USER_DOCUMENTS message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlPathGetUserDocuments(PVBGLR3GUESTCTRLCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 1, VERR_INVALID_PARAMETER);

    int rc;
    do
    {
        HGCMMsgPathUserDocuments Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_PATH_USER_DOCUMENTS);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
            Msg.context.GetUInt32(&pCtx->uContextID);
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_PATH_USER_HOME message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlPathGetUserHome(PVBGLR3GUESTCTRLCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 1, VERR_INVALID_PARAMETER);

    int rc;
    do
    {
        HGCMMsgPathUserHome Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_PATH_USER_HOME);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
            Msg.context.GetUInt32(&pCtx->uContextID);
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_EXEC_CMD message.
 *
 * @todo Move the parameters in an own struct!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetStart(PVBGLR3GUESTCTRLCMDCTX    pCtx,
                                            char     *pszCmd,         uint32_t  cbCmd,
                                            uint32_t *pfFlags,
                                            char     *pszArgs,        uint32_t  cbArgs,     uint32_t *pcArgs,
                                            char     *pszEnv,         uint32_t *pcbEnv,     uint32_t *pcEnvVars,
                                            char     *pszUser,        uint32_t  cbUser,
                                            char     *pszPassword,    uint32_t  cbPassword,
                                            uint32_t *puTimeoutMS,
                                            uint32_t *puPriority,
                                            uint64_t *puAffinity,     uint32_t  cbAffinity, uint32_t *pcAffinity)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertPtrReturn(pszCmd, VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);
    AssertPtrReturn(pszArgs, VERR_INVALID_POINTER);
    AssertPtrReturn(pcArgs, VERR_INVALID_POINTER);
    AssertPtrReturn(pszEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcEnvVars, VERR_INVALID_POINTER);
    AssertPtrReturn(puTimeoutMS, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgProcExec Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_EXEC_CMD);
        VbglHGCMParmPtrSet(&Msg.cmd, pszCmd, cbCmd);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);
        VbglHGCMParmUInt32Set(&Msg.num_args, 0);
        VbglHGCMParmPtrSet(&Msg.args, pszArgs, cbArgs);
        VbglHGCMParmUInt32Set(&Msg.num_env, 0);
        VbglHGCMParmUInt32Set(&Msg.cb_env, 0);
        VbglHGCMParmPtrSet(&Msg.env, pszEnv, *pcbEnv);
        if (pCtx->uProtocol < 2)
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

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.flags.GetUInt32(pfFlags);
            Msg.num_args.GetUInt32(pcArgs);
            Msg.num_env.GetUInt32(pcEnvVars);
            Msg.cb_env.GetUInt32(pcbEnv);
            if (pCtx->uProtocol < 2)
                Msg.u.v1.timeout.GetUInt32(puTimeoutMS);
            else
            {
                Msg.u.v2.timeout.GetUInt32(puTimeoutMS);
                Msg.u.v2.priority.GetUInt32(puPriority);
                Msg.u.v2.num_affinity.GetUInt32(pcAffinity);
            }
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
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
VBGLR3DECL(int) VbglR3GuestCtrlProcGetOutput(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                             uint32_t *puPID, uint32_t *puHandle, uint32_t *pfFlags)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 4, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puPID, VERR_INVALID_POINTER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgProcOutput Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_EXEC_GET_OUTPUT);
        VbglHGCMParmUInt32Set(&Msg.pid, 0);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, RT_UOFFSETOF(HGCMMsgProcOutput, data));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.handle.GetUInt32(puHandle);
            Msg.flags.GetUInt32(pfFlags);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves the input data from host which then gets sent to the started
 * process (HOST_EXEC_SET_INPUT).
 *
 * This will block until data becomes available.
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetInput(PVBGLR3GUESTCTRLCMDCTX  pCtx,
                                            uint32_t  *puPID,       uint32_t *pfFlags,
                                            void      *pvData,      uint32_t  cbData,
                                            uint32_t  *pcbSize)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 5, VERR_INVALID_PARAMETER);

    AssertPtrReturn(puPID, VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgProcInput Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_EXEC_SET_INPUT);
        VbglHGCMParmUInt32Set(&Msg.pid, 0);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);
        VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
        VbglHGCMParmUInt32Set(&Msg.size, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.flags.GetUInt32(pfFlags);
            Msg.size.GetUInt32(pcbSize);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);

    if (   rc != VERR_TOO_MUCH_DATA
        || g_fVbglR3GuestCtrlHavePeekGetCancel)
        return rc;
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Retrieves a HOST_DIR_REMOVE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlDirGetRemove(PVBGLR3GUESTCTRLCMDCTX     pCtx,
                                            char     *pszPath,         uint32_t cbPath,
                                            uint32_t *pfFlags)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 3, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgDirRemove Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_DIR_REMOVE);
        VbglHGCMParmPtrSet(&Msg.path, pszPath, cbPath);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.flags.GetUInt32(pfFlags);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_OPEN message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetOpen(PVBGLR3GUESTCTRLCMDCTX      pCtx,
                                           char     *pszFileName,      uint32_t cbFileName,
                                           char     *pszAccess,        uint32_t cbAccess,
                                           char     *pszDisposition,   uint32_t cbDisposition,
                                           char     *pszSharing,       uint32_t cbSharing,
                                           uint32_t *puCreationMode,
                                           uint64_t *poffAt)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(pCtx->uNumParms == 7, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszFileName, VERR_INVALID_POINTER);
    AssertReturn(cbFileName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszAccess, VERR_INVALID_POINTER);
    AssertReturn(cbAccess, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDisposition, VERR_INVALID_POINTER);
    AssertReturn(cbDisposition, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszSharing, VERR_INVALID_POINTER);
    AssertReturn(cbSharing, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puCreationMode, VERR_INVALID_POINTER);
    AssertPtrReturn(poffAt, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileOpen Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_OPEN);
        VbglHGCMParmPtrSet(&Msg.filename, pszFileName, cbFileName);
        VbglHGCMParmPtrSet(&Msg.openmode, pszAccess, cbAccess);
        VbglHGCMParmPtrSet(&Msg.disposition, pszDisposition, cbDisposition);
        VbglHGCMParmPtrSet(&Msg.sharing, pszSharing, cbSharing);
        VbglHGCMParmUInt32Set(&Msg.creationmode, 0);
        VbglHGCMParmUInt64Set(&Msg.offset, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.creationmode.GetUInt32(puCreationMode);
            Msg.offset.GetUInt64(poffAt);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_CLOSE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetClose(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puHandle)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileClose Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_CLOSE);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_READ message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetRead(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puHandle, uint32_t *puToRead)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 3, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puToRead, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileRead Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_READ);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmUInt32Set(&Msg.size, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(puToRead);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_READ_AT message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetReadAt(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                             uint32_t *puHandle, uint32_t *puToRead, uint64_t *poffAt)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puToRead, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileReadAt Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_READ_AT);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmUInt32Set(&Msg.offset, 0);
        VbglHGCMParmUInt32Set(&Msg.size, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.offset.GetUInt64(poffAt);
            Msg.size.GetUInt32(puToRead);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_WRITE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetWrite(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puHandle,
                                            void *pvData, uint32_t cbData, uint32_t *pcbSize)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileWrite Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_WRITE);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
        VbglHGCMParmUInt32Set(&Msg.size, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(pcbSize);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);

    if (   rc != VERR_TOO_MUCH_DATA
        || g_fVbglR3GuestCtrlHavePeekGetCancel)
        return rc;
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Retrieves a HOST_FILE_WRITE_AT message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetWriteAt(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puHandle,
                                              void *pvData, uint32_t cbData, uint32_t *pcbSize, uint64_t *poffAt)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 5, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileWriteAt Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_WRITE_AT);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);
        VbglHGCMParmUInt32Set(&Msg.size, 0);
        VbglHGCMParmUInt32Set(&Msg.offset, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.size.GetUInt32(pcbSize);
            Msg.offset.GetUInt64(poffAt);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);

    if (   rc != VERR_TOO_MUCH_DATA
        || g_fVbglR3GuestCtrlHavePeekGetCancel)
        return rc;
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Retrieves a HOST_FILE_SEEK message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetSeek(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                           uint32_t *puHandle, uint32_t *puSeekMethod, uint64_t *poffAt)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 4, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(puSeekMethod, VERR_INVALID_POINTER);
    AssertPtrReturn(poffAt, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileSeek Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_SEEK);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);
        VbglHGCMParmUInt32Set(&Msg.method, 0);
        VbglHGCMParmUInt64Set(&Msg.offset, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
            Msg.method.GetUInt32(puSeekMethod);
            Msg.offset.GetUInt64(poffAt);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_FILE_TELL message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlFileGetTell(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puHandle)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgFileTell Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_FILE_TELL);
        VbglHGCMParmUInt32Set(&Msg.handle, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.handle.GetUInt32(puHandle);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_EXEC_TERMINATE message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetTerminate(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t *puPID)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 2, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgProcTerminate Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_EXEC_TERMINATE);
        VbglHGCMParmUInt32Set(&Msg.pid, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


/**
 * Retrieves a HOST_EXEC_WAIT_FOR message.
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcGetWaitFor(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                              uint32_t *puPID, uint32_t *puWaitFlags, uint32_t *puTimeoutMS)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    AssertReturn(pCtx->uNumParms == 5, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puPID, VERR_INVALID_POINTER);

    int rc;
    do
    {
        HGCMMsgProcWaitFor Msg;
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, vbglR3GuestCtrlGetMsgFunctionNo(pCtx->uClientID), pCtx->uNumParms);
        VbglHGCMParmUInt32Set(&Msg.context, HOST_EXEC_WAIT_FOR);
        VbglHGCMParmUInt32Set(&Msg.pid, 0);
        VbglHGCMParmUInt32Set(&Msg.flags, 0);
        VbglHGCMParmUInt32Set(&Msg.timeout, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            Msg.context.GetUInt32(&pCtx->uContextID);
            Msg.pid.GetUInt32(puPID);
            Msg.flags.GetUInt32(puWaitFlags);
            Msg.timeout.GetUInt32(puTimeoutMS);
        }
    } while (rc == VERR_INTERRUPTED && g_fVbglR3GuestCtrlHavePeekGetCancel);
    return rc;
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbOpen(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                          uint32_t uRc, uint32_t uFileHandle)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_OPEN);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);
    VbglHGCMParmUInt32Set(&Msg.u.open.handle, uFileHandle);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSET_AFTER(HGCMReplyFileNotify, u.open));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbClose(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                           uint32_t uRc)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 3);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_CLOSE);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSETOF(HGCMReplyFileNotify, u));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbError(PVBGLR3GUESTCTRLCMDCTX pCtx, uint32_t uRc)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 3);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_ERROR);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSETOF(HGCMReplyFileNotify, u));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbRead(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                          uint32_t uRc,
                                          void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_READ);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);
    VbglHGCMParmPtrSet(&Msg.u.read.data, pvData, cbData);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSET_AFTER(HGCMReplyFileNotify, u.read));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbWrite(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                           uint32_t uRc, uint32_t uWritten)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_WRITE);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);
    VbglHGCMParmUInt32Set(&Msg.u.write.written, uWritten);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSET_AFTER(HGCMReplyFileNotify, u.write));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbSeek(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                          uint32_t uRc, uint64_t uOffActual)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_SEEK);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);
    VbglHGCMParmUInt64Set(&Msg.u.seek.offset, uOffActual);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSET_AFTER(HGCMReplyFileNotify, u.seek));
}


VBGLR3DECL(int) VbglR3GuestCtrlFileCbTell(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                          uint32_t uRc, uint64_t uOffActual)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMReplyFileNotify Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_FILE_NOTIFY, 4);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.type, GUEST_FILE_NOTIFYTYPE_TELL);
    VbglHGCMParmUInt32Set(&Msg.rc, uRc);
    VbglHGCMParmUInt64Set(&Msg.u.tell.offset, uOffActual);

    return VbglR3HGCMCall(&Msg.hdr, RT_UOFFSET_AFTER(HGCMReplyFileNotify, u.tell));
}


/**
 * Callback for reporting a guest process status (along with some other stuff) to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbStatus(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                            uint32_t uPID, uint32_t uStatus, uint32_t fFlags,
                                            void  *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMMsgProcStatus Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_EXEC_STATUS, 5);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.pid, uPID);
    VbglHGCMParmUInt32Set(&Msg.status, uStatus);
    VbglHGCMParmUInt32Set(&Msg.flags, fFlags);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


/**
 * Sends output (from stdout/stderr) from a running process.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbOutput(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                            uint32_t uPID,uint32_t uHandle, uint32_t fFlags,
                                            void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMMsgProcOutput Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_EXEC_OUTPUT, 5);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.pid, uPID);
    VbglHGCMParmUInt32Set(&Msg.handle, uHandle);
    VbglHGCMParmUInt32Set(&Msg.flags, fFlags);
    VbglHGCMParmPtrSet(&Msg.data, pvData, cbData);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}


/**
 * Callback for reporting back the input status of a guest process to the host.
 *
 * @returns VBox status code.
 ** @todo Docs!
 */
VBGLR3DECL(int) VbglR3GuestCtrlProcCbStatusInput(PVBGLR3GUESTCTRLCMDCTX pCtx,
                                                 uint32_t uPID, uint32_t uStatus,
                                                 uint32_t fFlags, uint32_t cbWritten)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    HGCMMsgProcStatusInput Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_EXEC_INPUT_STATUS, 5);
    VbglHGCMParmUInt32Set(&Msg.context, pCtx->uContextID);
    VbglHGCMParmUInt32Set(&Msg.pid, uPID);
    VbglHGCMParmUInt32Set(&Msg.status, uStatus);
    VbglHGCMParmUInt32Set(&Msg.flags, fFlags);
    VbglHGCMParmUInt32Set(&Msg.written, cbWritten);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

