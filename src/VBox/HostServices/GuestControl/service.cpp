/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_svc_guest_control   Guest Control HGCM Service
 *
 * This service acts as a proxy for handling and buffering host command requests
 * and clients on the guest. It tries to be as transparent as possible to let
 * the guest (client) and host side do their protocol handling as desired.
 *
 * The following terms are used:
 * - Host:   A host process (e.g. VBoxManage or another tool utilizing the Main API)
 *           which wants to control something on the guest.
 * - Client: A client (e.g. VBoxService) running inside the guest OS waiting for
 *           new host commands to perform. There can be multiple clients connected
 *           to this service. A client is represented by its unique HGCM client ID.
 * - Context ID: An (almost) unique ID automatically generated on the host (Main API)
 *               to not only distinguish clients but individual requests. Because
 *               the host does not know anything about connected clients it needs
 *               an indicator which it can refer to later. This context ID gets
 *               internally bound by the service to a client which actually processes
 *               the command in order to have a relationship between client<->context ID(s).
 *
 * The host can trigger commands which get buffered by the service (with full HGCM
 * parameter info). As soon as a client connects (or is ready to do some new work)
 * it gets a buffered host command to process it. This command then will be immediately
 * removed from the command list. If there are ready clients but no new commands to be
 * processed, these clients will be set into a deferred state (that is being blocked
 * to return until a new command is available).
 *
 * If a client needs to inform the host that something happened, it can send a
 * message to a low level HGCM callback registered in Main. This callback contains
 * the actual data as well as the context ID to let the host do the next necessary
 * steps for this context. This context ID makes it possible to wait for an event
 * inside the host's Main API function (like starting a process on the guest and
 * wait for getting its PID returned by the client) as well as cancelling blocking
 * host calls in order the client terminated/crashed (HGCM detects disconnected
 * clients and reports it to this service's callback).
 *
 * Starting at VBox 4.2 the context ID itself consists of a session ID, an object
 * ID (for example a process or file ID) and a count. This is necessary to not break
 * compatibility between older hosts and to manage guest session on the host.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/HostServices/GuestControlSvc.h>
#include <VBox/GuestHost/GuestControl.h> /** @todo r=bird: Why two headers??? */

#include <VBox/log.h>
#include <VBox/AssertGuest.h>
#include <VBox/VMMDev.h>
#include <VBox/vmm/ssm.h>
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/cpp/utils.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/list.h>
#include <iprt/req.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <map>
#include <new>      /* for std::nothrow*/
#include <string>
#include <list>


using namespace guestControl;


/**
 * Structure for maintaining a pending (that is, a deferred and not yet completed)
 * client command.
 */
typedef struct ClientConnection
{
    /** The call handle */
    VBOXHGCMCALLHANDLE mHandle;
    /** Number of parameters */
    uint32_t mNumParms;
    /** The call parameters */
    VBOXHGCMSVCPARM *mParms;
    /** The standard constructor. */
    ClientConnection(void)
        : mHandle(0), mNumParms(0), mParms(NULL) {}
} ClientConnection;

/**
 * Structure for holding a buffered host command which has
 * not been processed yet.
 */
typedef struct HostCommand
{
    /** Entry on the GstCtrlService::mHostCmdList list. */
    RTLISTNODE m_ListEntry;
    /** Reference counter for facilitating sending to both session and root. */
    uint32_t m_cRefs;
    union
    {
        /** The top two twomost bits are exploited for message destination.
         * See VBOX_GUESTCTRL_DST_XXX.  */
        uint64_t m_idContextAndDst;
        /** The context ID this command belongs to (extracted from the first parameter). */
        uint32_t m_idContext;
    };
    /** Dynamic structure for holding the HGCM parms */
    uint32_t mMsgType;
    /** Number of HGCM parameters. */
    uint32_t mParmCount;
    /** Array of HGCM parameters. */
    PVBOXHGCMSVCPARM mpParms;

    HostCommand()
        : m_cRefs(1)
        , m_idContextAndDst(0)
        , mMsgType(UINT32_MAX)
        , mParmCount(0)
        , mpParms(NULL)
    {
        RTListInit(&m_ListEntry);
    }


    /**
     * Retains a reference to the command.
     */
    uint32_t Retain(void)
    {
        uint32_t cRefs = ++m_cRefs;
        Log4(("[Cmd %RU32 (%s)] Adding reference, new m_cRefs=%u\n", mMsgType, GstCtrlHostFnName((eHostFn)mMsgType), cRefs));
        Assert(cRefs < 4);
        return cRefs;
    }


    /**
     * Releases the host command, properly deleting it if no further references.
     */
    uint32_t SaneRelease(void)
    {
        uint32_t cRefs = --m_cRefs;
        Log4(("[Cmd %RU32] sane release - cRefs=%u\n", mMsgType, cRefs));
        Assert(cRefs < 4);

        if (!cRefs)
        {
            LogFlowThisFunc(("[Cmd %RU32 (%s)] destroying\n", mMsgType, GstCtrlHostFnName((eHostFn)mMsgType)));
            RTListNodeRemove(&m_ListEntry);
            if (mpParms)
            {
                for (uint32_t i = 0; i < mParmCount; i++)
                    if (mpParms[i].type == VBOX_HGCM_SVC_PARM_PTR)
                    {
                        RTMemFree(mpParms[i].u.pointer.addr);
                        mpParms[i].u.pointer.addr = NULL;
                    }
                RTMemFree(mpParms);
                mpParms = NULL;
            }
            mParmCount = 0;
            delete this;
        }
        return cRefs;
    }


    /**
     * Initializes the command.
     *
     * The specified parameters are copied and any buffers referenced by it
     * duplicated as well.
     *
     * @return  IPRT status code.
     * @param   idFunction  The host function (message) number, eHostFn.
     * @param   cParms      Number of parameters in the HGCM request.
     * @param   paParms     Array of parameters.
     */
    int Init(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        LogFlowThisFunc(("[Cmd %RU32 (%s)] Allocating cParms=%RU32, paParms=%p\n",
                         idFunction, GstCtrlHostFnName((eHostFn)idFunction), cParms, paParms));
        Assert(mpParms == NULL);
        Assert(mParmCount == 0);
        Assert(m_cRefs == 1);

        /*
         * Fend of bad stuff.
         */
        AssertReturn(cParms > 0, VERR_WRONG_PARAMETER_COUNT); /* At least one parameter (context ID) must be present. */
        AssertReturn(cParms < VMMDEV_MAX_HGCM_PARMS, VERR_WRONG_PARAMETER_COUNT);
        AssertPtrReturn(paParms, VERR_INVALID_POINTER);

        /*
         * The first parameter is the context ID and the command destiation mask.
         */
        if (paParms[0].type == VBOX_HGCM_SVC_PARM_64BIT)
        {
            m_idContextAndDst = paParms[0].u.uint64;
            AssertReturn(m_idContextAndDst & VBOX_GUESTCTRL_DST_BOTH, VERR_INTERNAL_ERROR_3);
        }
        else if (paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT)
        {
            AssertMsgFailed(("idFunction=%u %s - caller must set dst!\n", idFunction, GstCtrlHostFnName((eHostFn)idFunction)));
            m_idContextAndDst = paParms[0].u.uint32 | VBOX_GUESTCTRL_DST_BOTH;
        }
        else
            AssertFailedReturn(VERR_WRONG_PARAMETER_TYPE);

        /*
         * Just make a copy of the parameters and any buffers.
         */
        mMsgType   = idFunction;
        mParmCount = cParms;
        mpParms    = (VBOXHGCMSVCPARM *)RTMemAllocZ(sizeof(VBOXHGCMSVCPARM) * mParmCount);
        AssertReturn(mpParms, VERR_NO_MEMORY);

        for (uint32_t i = 0; i < cParms; i++)
        {
            mpParms[i].type = paParms[i].type;
            switch (paParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    mpParms[i].u.uint32 = paParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    mpParms[i].u.uint64 = paParms[i].u.uint64;
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                    mpParms[i].u.pointer.size = paParms[i].u.pointer.size;
                    if (mpParms[i].u.pointer.size > 0)
                    {
                        mpParms[i].u.pointer.addr = RTMemDup(paParms[i].u.pointer.addr, mpParms[i].u.pointer.size);
                        AssertReturn(mpParms[i].u.pointer.addr, VERR_NO_MEMORY);
                    }
                    /* else: structure is zeroed by allocator. */
                    break;

                default:
                    AssertMsgFailedReturn(("idFunction=%u (%s) parameter #%u: type=%u\n",
                                           idFunction, GstCtrlHostFnName((eHostFn)idFunction), i, paParms[i].type),
                                          VERR_WRONG_PARAMETER_TYPE);
            }
        }

        /*
         * Morph the first parameter back to 32-bit.
         */
        mpParms[0].type     = VBOX_HGCM_SVC_PARM_32BIT;
        mpParms[0].u.uint32 = (uint32_t)paParms[0].u.uint64;

        return VINF_SUCCESS;
    }


    /**
     * Sets the GUEST_MSG_PEEK_WAIT GUEST_MSG_PEEK_NOWAIT return parameters.
     *
     * @param   paDstParms  The peek parameter vector.
     * @param   cDstParms   The number of peek parameters (at least two).
     * @remarks ASSUMES the parameters has been cleared by clientMsgPeek.
     */
    inline void setPeekReturn(PVBOXHGCMSVCPARM paDstParms, uint32_t cDstParms)
    {
        Assert(cDstParms >= 2);
        if (paDstParms[0].type == VBOX_HGCM_SVC_PARM_32BIT)
            paDstParms[0].u.uint32 = mMsgType;
        else
            paDstParms[0].u.uint64 = mMsgType;
        paDstParms[1].u.uint32 = mParmCount;

        uint32_t i = RT_MIN(cDstParms, mParmCount + 2);
        while (i-- > 2)
            switch (mpParms[i - 2].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT: paDstParms[i].u.uint32 = ~(uint32_t)sizeof(uint32_t); break;
                case VBOX_HGCM_SVC_PARM_64BIT: paDstParms[i].u.uint32 = ~(uint32_t)sizeof(uint64_t); break;
                case VBOX_HGCM_SVC_PARM_PTR:   paDstParms[i].u.uint32 = mpParms[i - 2].u.pointer.size; break;
            }
    }


    /** @name Support for old-style (GUEST_MSG_WAIT) operation.
     * @{
     */

    /**
     * Worker for Assign() that opies data from the buffered HGCM request to the
     * current HGCM request.
     *
     * @return  IPRT status code.
     * @param   paDstParms              Array of parameters of HGCM request to fill the data into.
     * @param   cDstParms               Number of parameters the HGCM request can handle.
     */
    int CopyTo(VBOXHGCMSVCPARM paDstParms[], uint32_t cDstParms) const
    {
        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, m_idContext=%RU32 (Session %RU32)\n",
                         mMsgType, mParmCount, m_idContext, VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(m_idContext)));

        int rc = VINF_SUCCESS;
        if (cDstParms != mParmCount)
        {
            LogFlowFunc(("Parameter count does not match (got %RU32, expected %RU32)\n",
                         cDstParms, mParmCount));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_SUCCESS(rc))
        {
            for (uint32_t i = 0; i < mParmCount; i++)
            {
                if (paDstParms[i].type != mpParms[i].type)
                {
                    LogFunc(("Parameter %RU32 type mismatch (got %RU32, expected %RU32)\n", i, paDstParms[i].type, mpParms[i].type));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    switch (mpParms[i].type)
                    {
                        case VBOX_HGCM_SVC_PARM_32BIT:
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %RU32 (uint32_t)\n",
                                         i, mpParms[i].u.uint32));
#endif
                            paDstParms[i].u.uint32 = mpParms[i].u.uint32;
                            break;

                        case VBOX_HGCM_SVC_PARM_64BIT:
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %RU64 (uint64_t)\n",
                                         i, mpParms[i].u.uint64));
#endif
                            paDstParms[i].u.uint64 = mpParms[i].u.uint64;
                            break;

                        case VBOX_HGCM_SVC_PARM_PTR:
                        {
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %p (ptr), size = %RU32\n",
                                         i, mpParms[i].u.pointer.addr, mpParms[i].u.pointer.size));
#endif
                            if (!mpParms[i].u.pointer.size)
                                continue; /* Only copy buffer if there actually is something to copy. */

                            if (!paDstParms[i].u.pointer.addr)
                                rc = VERR_INVALID_PARAMETER;
                            else if (paDstParms[i].u.pointer.size < mpParms[i].u.pointer.size)
                                rc = VERR_BUFFER_OVERFLOW;
                            else
                                memcpy(paDstParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.size);
                            break;
                        }

                        default:
                            LogFunc(("Parameter %RU32 of type %RU32 is not supported yet\n", i, mpParms[i].type));
                            rc = VERR_NOT_SUPPORTED;
                            break;
                    }
                }

                if (RT_FAILURE(rc))
                {
                    LogFunc(("Parameter %RU32 invalid (%Rrc), refusing\n", i, rc));
                    break;
                }
            }
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
    }

    int Assign(const ClientConnection *pConnection)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);

        int rc;

        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, mpParms=%p\n", mMsgType, mParmCount, mpParms));

        /* Does the current host command need more parameter space which
         * the client does not provide yet? */
        if (mParmCount > pConnection->mNumParms)
        {
            LogFlowThisFunc(("[Cmd %RU32] Requires %RU32 parms, only got %RU32 from client\n",
                             mMsgType, mParmCount, pConnection->mNumParms));
            /*
             * So this call apparently failed because the guest wanted to peek
             * how much parameters it has to supply in order to successfully retrieve
             * this command. Let's tell him so!
             */
            rc = VERR_TOO_MUCH_DATA;
        }
        else
        {
            rc = CopyTo(pConnection->mParms, pConnection->mNumParms);

            /*
             * Has there been enough parameter space but the wrong parameter types
             * were submitted -- maybe the client was just asking for the next upcoming
             * host message?
             *
             * Note: To keep this compatible to older clients we return VERR_TOO_MUCH_DATA
             *       in every case.
             */
            if (RT_FAILURE(rc))
                rc = VERR_TOO_MUCH_DATA;
        }

        return rc;
    }

    int Peek(const ClientConnection *pConnection)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);

        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, mpParms=%p\n", mMsgType, mParmCount, mpParms));

        if (pConnection->mNumParms >= 2)
        {
            HGCMSvcSetU32(&pConnection->mParms[0], mMsgType);   /* Message ID */
            HGCMSvcSetU32(&pConnection->mParms[1], mParmCount); /* Required parameters for message */
        }
        else
            LogFlowThisFunc(("Warning: Client has not (yet) submitted enough parameters (%RU32, must be at least 2) to at least peak for the next message\n",
                             pConnection->mNumParms));

        /*
         * Always return VERR_TOO_MUCH_DATA data here to
         * keep it compatible with older clients and to
         * have correct accounting (mHostRc + mHostCmdTries).
         */
        return VERR_TOO_MUCH_DATA;
    }

    /** @} */
} HostCommand;
typedef std::list< HostCommand *> HostCmdList;
typedef std::list< HostCommand *>::iterator HostCmdListIter;
typedef std::list< HostCommand *>::const_iterator HostCmdListIterConst;

/**
 * Per-client structure used for book keeping/state tracking a
 * certain host command.
 */
typedef struct ClientContext
{
    /* Pointer to list node of this command. */
    HostCommand *mpHostCmd;
    /** The standard constructor. */
    ClientContext(void) : mpHostCmd(NULL) {}
    /** Internal constrcutor. */
    ClientContext(HostCommand *pHostCmd) : mpHostCmd(pHostCmd) {}
} ClientContext;
typedef std::map< uint32_t, ClientContext > ClientContextMap;
typedef std::map< uint32_t, ClientContext >::iterator ClientContextMapIter;
typedef std::map< uint32_t, ClientContext >::const_iterator ClientContextMapIterConst;

/**
 * Structure for holding a connected guest client state.
 */
typedef struct ClientState
{
    PVBOXHGCMSVCHELPERS mSvcHelpers;
    /** The client's ID. */
    uint32_t mID;
    /** Host command list to process. */
    HostCmdList mHostCmdList;
    /** Pending client call (GUEST_MSG_PEEK_WAIT or GUEST_MSG_WAIT), zero if none pending.
     *
     * This means the client waits for a new host command to reply and won't return
     * from the waiting call until a new host command is available. */
    guestControl::eGuestFn mIsPending;
    /** The client's pending connection. */
    ClientConnection    mPendingCon;
    /** Set if we've got a pending wait cancel. */
    bool                m_fPendingCancel;
    /** Set if master. */
    bool                m_fIsMaster;
    /** The session ID for this client, UINT32_MAX if not set or master. */
    uint32_t            m_idSession;


    ClientState(void)
        : mSvcHelpers(NULL)
        , mID(0)
        , mIsPending((guestControl::eGuestFn)0)
        , m_fPendingCancel(false)
        , m_fIsMaster(false)
        , m_idSession(UINT32_MAX)
        , mHostCmdRc(VINF_SUCCESS)
        , mHostCmdTries(0)
        , mPeekCount(0)
    { }

    ClientState(PVBOXHGCMSVCHELPERS pSvcHelpers, uint32_t idClient)
        : mSvcHelpers(pSvcHelpers)
        , mID(idClient)
        , mIsPending((guestControl::eGuestFn)0)
        , m_fPendingCancel(false)
        , m_fIsMaster(false)
        , m_idSession(UINT32_MAX)
        , mHostCmdRc(VINF_SUCCESS)
        , mHostCmdTries(0)
        , mPeekCount(0)
    { }

    /**
     * Used by for Service::hostProcessCommand().
     */
    int EnqueueCommand(HostCommand *pHostCmd)
    {
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        try
        {
            mHostCmdList.push_back(pHostCmd);
        }
        catch (std::bad_alloc &)
        {
            return VERR_NO_MEMORY;
        }

        pHostCmd->Retain();
        return VINF_SUCCESS;
    }

    /**
     * Used by for Service::hostProcessCommand().
     *
     * @note This wakes up both GUEST_MSG_WAIT and GUEST_MSG_PEEK_WAIT sleepers.
     */
    int Wakeup(void)
    {
        int rc = VINF_NO_CHANGE;

        if (mIsPending != 0)
        {
            LogFlowFunc(("[Client %RU32] Waking up ...\n", mID));

            rc = VINF_SUCCESS;

            HostCmdListIter ItFirstCmd = mHostCmdList.begin();
            if (ItFirstCmd != mHostCmdList.end())
            {
                HostCommand *pFirstCmd = (*ItFirstCmd);
                AssertPtrReturn(pFirstCmd, VERR_INVALID_POINTER);

                LogFlowThisFunc(("[Client %RU32] Current host command is %RU32 (CID=%RU32, cParms=%RU32, m_cRefs=%RU32)\n",
                                 mID, pFirstCmd->mMsgType, pFirstCmd->m_idContext, pFirstCmd->mParmCount, pFirstCmd->m_cRefs));

                if (mIsPending == GUEST_MSG_PEEK_WAIT)
                {
                    pFirstCmd->setPeekReturn(mPendingCon.mParms, mPendingCon.mNumParms);
                    rc = mSvcHelpers->pfnCallComplete(mPendingCon.mHandle, VINF_SUCCESS);

                    mPendingCon.mHandle   = NULL;
                    mPendingCon.mParms    = NULL;
                    mPendingCon.mNumParms = 0;
                    mIsPending            = (guestControl::eGuestFn)0;
                }
                else if (mIsPending == GUEST_MSG_WAIT)
                    rc = OldRun(&mPendingCon, pFirstCmd);
                else
                    AssertMsgFailed(("mIsPending=%d\n", mIsPending));
            }
            else
                AssertMsgFailed(("Waking up client ID=%RU32 with no host command in queue is a bad idea\n", mID));

            return rc;
        }

        return VINF_NO_CHANGE;
    }

    /**
     * Used by Service::call() to handle GUEST_MSG_CANCEL.
     *
     * @note This cancels both GUEST_MSG_WAIT and GUEST_MSG_PEEK_WAIT sleepers.
     */
    int CancelWaiting()
    {
        LogFlowFunc(("[Client %RU32] Cancelling waiting thread, isPending=%d, pendingNumParms=%RU32, m_idSession=%x\n",
                     mID, mIsPending, mPendingCon.mNumParms, m_idSession));

        /*
         * The PEEK call is simple: At least two parameters, all set to zero before sleeping.
         */
        int rcComplete;
        if (mIsPending == GUEST_MSG_PEEK_WAIT)
        {
            HGCMSvcSetU32(&mPendingCon.mParms[0], HOST_CANCEL_PENDING_WAITS);
            rcComplete = VINF_TRY_AGAIN;
        }
        /*
         * The GUEST_MSG_WAIT call is complicated, though we're generally here
         * to wake up someone who is peeking and have two parameters.  If there
         * aren't two parameters, fail the call.
         */
        else if (mIsPending != 0)
        {
            Assert(mIsPending == GUEST_MSG_WAIT);
            if (mPendingCon.mNumParms > 0)
                HGCMSvcSetU32(&mPendingCon.mParms[0], HOST_CANCEL_PENDING_WAITS);
            if (mPendingCon.mNumParms > 1)
                HGCMSvcSetU32(&mPendingCon.mParms[1], 0);
            rcComplete = mPendingCon.mNumParms == 2 ? VINF_SUCCESS : VERR_TRY_AGAIN;
        }
        /*
         * If nobody is waiting, flag the next wait call as cancelled.
         */
        else
        {
            m_fPendingCancel = true;
            return VINF_SUCCESS;
        }

        mSvcHelpers->pfnCallComplete(mPendingCon.mHandle, rcComplete);

        mPendingCon.mHandle   = NULL;
        mPendingCon.mParms    = NULL;
        mPendingCon.mNumParms = 0;
        mIsPending            = (guestControl::eGuestFn)0;
        m_fPendingCancel      = false;
        return VINF_SUCCESS;
    }


    /** @name The GUEST_MSG_WAIT state and helpers.
     *
     * @note Don't try understand this, it is certificable!
     *
     * @{
     */

    /** Last (most recent) rc after handling the host command. */
    int mHostCmdRc;
    /** How many GUEST_MSG_WAIT calls the client has issued to retrieve one command.
     *
     * This is used as a heuristic to remove a message that the client appears not
     * to be able to successfully retrieve.  */
    uint32_t mHostCmdTries;
    /** Number of times we've peeked at a pending message.
     *
     * This is necessary for being compatible with older Guest Additions.  In case
     * there are commands which only have two (2) parameters and therefore would fit
     * into the GUEST_MSG_WAIT reply immediately, we now can make sure that the
     * client first gets back the GUEST_MSG_WAIT results first.
     */
    uint32_t mPeekCount;

    /**
     * Ditches the first host command and crazy GUEST_MSG_WAIT state.
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    void OldDitchFirstHostCmd()
    {
        Assert(!mHostCmdList.empty());
        HostCommand *pFirstCmd = *mHostCmdList.begin();
        AssertPtr(pFirstCmd);
        pFirstCmd->SaneRelease();
        mHostCmdList.pop_front();

        /* Reset state else. */
        mHostCmdRc    = VINF_SUCCESS;
        mHostCmdTries = 0;
        mPeekCount    = 0;
    }

    /**
     * Used by Wakeup() and OldRunCurrent().
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldRun(ClientConnection const *pConnection, HostCommand *pHostCmd)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);
        Assert(*mHostCmdList.begin() == pHostCmd);

        LogFlowFunc(("[Client %RU32] pConnection=%p, mHostCmdRc=%Rrc, mHostCmdTries=%RU32, mPeekCount=%RU32\n",
                      mID, pConnection, mHostCmdRc, mHostCmdTries, mPeekCount));

        int rc = mHostCmdRc = OldSendReply(pConnection, pHostCmd);

        LogFlowThisFunc(("[Client %RU32] Processing command %RU32 ended with rc=%Rrc\n", mID, pHostCmd->mMsgType, mHostCmdRc));

        bool fRemove = false;
        if (RT_FAILURE(rc))
        {
            mHostCmdTries++;

            /*
             * If the client understood the message but supplied too little buffer space
             * don't send this message again and drop it after 6 unsuccessful attempts.
             *
             * Note: Due to legacy reasons this the retry counter has to be even because on
             *       every peek there will be the actual command retrieval from the client side.
             *       To not get the actual command if the client actually only wants to peek for
             *       the next command, there needs to be two rounds per try, e.g. 3 rounds = 6 tries.
             */
            /** @todo Fix the mess stated above. GUEST_MSG_WAIT should be become GUEST_MSG_PEEK, *only*
             *        (and every time) returning the next upcoming host command (if any, blocking). Then
             *        it's up to the client what to do next, either peeking again or getting the actual
             *        host command via an own GUEST_ type message.
             */
            if (   rc == VERR_TOO_MUCH_DATA
                || rc == VERR_CANCELLED)
            {
                if (mHostCmdTries == 6)
                    fRemove = true;
            }
            /* Client did not understand the message or something else weird happened. Try again one
             * more time and drop it if it didn't get handled then. */
            else if (mHostCmdTries > 1)
                fRemove = true;
        }
        else
            fRemove = true; /* Everything went fine, remove it. */

        LogFlowThisFunc(("[Client %RU32] Tried command %RU32 for %RU32 times, (last result=%Rrc, fRemove=%RTbool)\n",
                         mID, pHostCmd->mMsgType, mHostCmdTries, rc, fRemove));

        if (fRemove)
        {
            Assert(*mHostCmdList.begin() == pHostCmd);
            OldDitchFirstHostCmd();
        }

        LogFlowFunc(("[Client %RU32] Returned with rc=%Rrc\n", mID, rc));
        return rc;
    }

    /**
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldRunCurrent(const ClientConnection *pConnection)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);

        /*
         * If the host command list is empty, the request must wait for one to be posted.
         */
        if (mHostCmdList.empty())
        {
            if (!m_fPendingCancel)
            {
                /* Go to sleep. */
                ASSERT_GUEST_RETURN(mIsPending == 0, VERR_WRONG_ORDER);
                mPendingCon = *pConnection;
                mIsPending  = GUEST_MSG_WAIT;
                LogFlowFunc(("[Client %RU32] Is now in pending mode\n", mID));
                return VINF_HGCM_ASYNC_EXECUTE;
            }

            /* Wait was cancelled. */
            m_fPendingCancel = false;
            if (pConnection->mNumParms > 0)
                HGCMSvcSetU32(&pConnection->mParms[0], HOST_CANCEL_PENDING_WAITS);
            if (pConnection->mNumParms > 1)
                HGCMSvcSetU32(&pConnection->mParms[1], 0);
            return pConnection->mNumParms == 2 ? VINF_SUCCESS : VERR_TRY_AGAIN;
        }

        /*
         * Return first host command.
         */
        HostCmdListIter curCmd = mHostCmdList.begin();
        Assert(curCmd != mHostCmdList.end());
        HostCommand *pHostCmd = *curCmd;
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        return OldRun(pConnection, pHostCmd);
    }

    /**
     * Internal worker for OldRun().
     * @note Only used for GUEST_MSG_WAIT.
     */
    int OldSendReply(ClientConnection const *pConnection,
                     HostCommand            *pHostCmd)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        /* In case of VERR_CANCELLED. */
        uint32_t const cSavedPeeks = mPeekCount;

        int rc;
        /* If the client is in pending mode, always send back
         * the peek result first. */
        if (mIsPending)
        {
            Assert(mIsPending == GUEST_MSG_WAIT);
            rc = pHostCmd->Peek(pConnection);
            mPeekCount++;
        }
        else
        {
            /* If this is the very first peek, make sure to *always* give back the peeking answer
             * instead of the actual command, even if this command would fit into the current
             * connection buffer. */
            if (!mPeekCount)
            {
                rc = pHostCmd->Peek(pConnection);
                mPeekCount++;
            }
            else
            {
                /* Try assigning the host command to the client and store the
                 * result code for later use. */
                rc = pHostCmd->Assign(pConnection);
                if (RT_FAILURE(rc)) /* If something failed, let the client peek (again). */
                {
                    rc = pHostCmd->Peek(pConnection);
                    mPeekCount++;
                }
                else
                    mPeekCount = 0;
            }
        }

        /* Reset pending status. */
        mIsPending = (guestControl::eGuestFn)0;

        /* In any case the client did something, so complete
         * the pending call with the result we just got. */
        AssertPtr(mSvcHelpers);
        int rc2 = mSvcHelpers->pfnCallComplete(pConnection->mHandle, rc);

        /* Rollback in case the guest cancelled the call. */
        if (rc2 == VERR_CANCELLED && RT_SUCCESS(rc))
        {
            mPeekCount = cSavedPeeks;
            rc = VERR_CANCELLED;
        }

        LogFlowThisFunc(("[Client %RU32] Command %RU32 ended with %Rrc (mPeekCount=%RU32, pConnection=%p)\n",
                         mID, pHostCmd->mMsgType, rc, mPeekCount, pConnection));
        return rc;
    }

    /** @} */
} ClientState;
typedef std::map< uint32_t, ClientState > ClientStateMap;
typedef std::map< uint32_t, ClientState >::iterator ClientStateMapIter;
typedef std::map< uint32_t, ClientState >::const_iterator ClientStateMapIterConst;

/**
 * Prepared session (GUEST_SESSION_PREPARE).
 */
typedef struct GstCtrlPreparedSession
{
    /** List entry. */
    RTLISTNODE  ListEntry;
    /** The session ID.   */
    uint32_t    idSession;
    /** The key size. */
    uint32_t    cbKey;
    /** The key bytes. */
    uint8_t     abKey[RT_FLEXIBLE_ARRAY];
} GstCtrlPreparedSession;


/**
 * Class containing the shared information service functionality.
 */
class GstCtrlService : public RTCNonCopyable
{

private:

    /** Type definition for use in callback functions. */
    typedef GstCtrlService SELF;
    /** HGCM helper functions. */
    PVBOXHGCMSVCHELPERS mpHelpers;
    /** Callback function supplied by the host for notification of updates to properties. */
    PFNHGCMSVCEXT mpfnHostCallback;
    /** User data pointer to be supplied to the host callback function. */
    void *mpvHostData;
    /** List containing all buffered host commands. */
    RTLISTANCHOR mHostCmdList;
    /** Map containing all connected clients, key is HGCM client ID. */
    ClientStateMap  mClientStateMap; /**< @todo Use VBOXHGCMSVCFNTABLE::cbClient for this! */
    /** The master HGCM client ID, UINT32_MAX if none. */
    uint32_t        m_idMasterClient;
    /** Set if we're in legacy mode (pre 6.0). */
    bool            m_fLegacyMode;
    /** Number of prepared sessions. */
    uint32_t        m_cPreparedSessions;
    /** List of prepared session (GstCtrlPreparedSession). */
    RTLISTANCHOR    m_PreparedSessions;

public:
    explicit GstCtrlService(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
        , m_idMasterClient(UINT32_MAX)
        , m_fLegacyMode(true)
        , m_cPreparedSessions(0)
    {
        RTListInit(&mHostCmdList);
        RTListInit(&m_PreparedSessions);
    }

    static DECLCALLBACK(int)  svcUnload(void *pvService);
    static DECLCALLBACK(int)  svcConnect(void *pvService, uint32_t u32ClientID, void *pvClient,
                                         uint32_t fRequestor, bool fRestoring);
    static DECLCALLBACK(int)  svcDisconnect(void *pvService, uint32_t u32ClientID, void *pvClient);
    static DECLCALLBACK(void) svcCall(void *pvService, VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient,
                                      uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival);
    static DECLCALLBACK(int)  svcHostCall(void *pvService, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    static DECLCALLBACK(int)  svcSaveState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM);
    static DECLCALLBACK(int)  svcLoadState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM, uint32_t uVersion);
    static DECLCALLBACK(int)  svcRegisterExtension(void *pvService, PFNHGCMSVCEXT pfnExtension, void *pvExtension);

private:
    int clientMakeMeMaster(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms);
    int clientMsgPeek(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait);
    int clientMsgGet(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgCancel(uint32_t idClient, uint32_t cParms);
    int clientMsgSkip(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionPrepare(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionCancelPrepared(uint32_t idClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionAccept(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionCloseOther(uint32_t idClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    int clientMsgOldGet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgOldFilterSet(uint32_t u32ClientID, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgOldSkip(uint32_t u32ClientID, uint32_t cParms);

    int hostCallback(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostProcessCommand(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(GstCtrlService);
};


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnUnload,
 *  Simply deletes the GstCtrlService object}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcUnload(void *pvService)
{
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    delete pThis;

    return VINF_SUCCESS;
}



/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect,
 *  Initializes the state for a new client.}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcConnect(void *pvService, uint32_t idClient, void *pvClient, uint32_t fRequestor, bool fRestoring)
{
    LogFlowFunc(("[Client %RU32] Connected\n", idClient));

    RT_NOREF(fRestoring, pvClient);
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertMsg(pThis->mClientStateMap.find(idClient) == pThis->mClientStateMap.end(),
              ("Client with ID=%RU32 already connected when it should not\n", idClient));

    /*
     * Create client state.
     */
    try
    {
        pThis->mClientStateMap[idClient] = ClientState(pThis->mpHelpers, idClient);
    }
    catch (std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }
    ClientState &rClientState = pThis->mClientStateMap[idClient];

    /*
     * For legacy compatibility reasons we have to pick a master client at some
     * point, so if the /dev/vboxguest requirements checks out we pick the first
     * one through the door.
     */
/** @todo make picking the master more dynamic/flexible. */
    if (   pThis->m_fLegacyMode
        && pThis->m_idMasterClient == UINT32_MAX)
    {
        if (   fRequestor == VMMDEV_REQUESTOR_LEGACY
            || !(fRequestor & VMMDEV_REQUESTOR_USER_DEVICE))
        {
            LogFunc(("Picking %u as master for now.\n", idClient));
            pThis->m_idMasterClient = idClient;
            rClientState.m_fIsMaster = true;
        }
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect,
 *  Handles a client which disconnected.}
 *
 * This functiond does some internal cleanup as well as sends notifications to
 * the host so that the host can do the same (if required).
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcDisconnect(void *pvService, uint32_t idClient, void *pvClient)
{
    RT_NOREF(pvClient);
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    LogFlowFunc(("[Client %RU32] Disconnected (%zu clients total)\n", idClient, pThis->mClientStateMap.size()));

    ClientStateMapIter ItClientState = pThis->mClientStateMap.find(idClient);
    AssertMsgReturn(ItClientState != pThis->mClientStateMap.end(),
                    ("Client ID=%RU32 not found in client list when it should be there\n", idClient),
                    VINF_SUCCESS);

    /*
     * Cancel all pending host commands, replying with GUEST_DISCONNECTED if final recipient.
     */
    {
        ClientState &rClientState = ItClientState->second;

        while (!rClientState.mHostCmdList.empty())
        {
            HostCommand *pHostCmd = *rClientState.mHostCmdList.begin();
            rClientState.mHostCmdList.pop_front();

            uint32_t idFunction = pHostCmd->mMsgType;
            uint32_t idContext  = pHostCmd->m_idContext;
            if (pHostCmd->SaneRelease() == 0)
            {
                VBOXHGCMSVCPARM Parm;
                HGCMSvcSetU32(&Parm, idContext);
                int rc2 = pThis->hostCallback(GUEST_DISCONNECTED, 1, &Parm);
                LogFlowFunc(("Cancelled host command %u (%s) with idContext=%#x -> %Rrc\n",
                             idFunction, GstCtrlHostFnName((eHostFn)idFunction), idContext, rc2));
                RT_NOREF(rc2, idFunction);
            }
        }
    }

    /*
     * Delete the client state.
     */
    pThis->mClientStateMap.erase(ItClientState);

    /*
     * If it's the master disconnecting, we need to reset related globals.
     */
    if (idClient == pThis->m_idMasterClient)
    {
        pThis->m_idMasterClient = UINT32_MAX;
        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&pThis->m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            RTListNodeRemove(&pCur->ListEntry);
            RTMemFree(pCur);
        }
        pThis->m_cPreparedSessions = 0;
    }

    if (pThis->mClientStateMap.empty())
        pThis->m_fLegacyMode = true;

    /*
     * Host command sanity check.
     */
    Assert(RTListIsEmpty(&pThis->mHostCmdList) || !pThis->mClientStateMap.empty());

    return VINF_SUCCESS;
}


/**
 * A client asks for the next message to process.
 *
 * This either fills in a pending host command into the client's parameter space
 * or defers the guest call until we have something from the host.
 *
 * @return  IPRT status code.
 * @param   u32ClientID                 The client's ID.
 * @param   callHandle                  The client's call handle.
 * @param   cParms                      Number of parameters.
 * @param   paParms                     Array of parameters.
 */
int GstCtrlService::clientMsgOldGet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                                    uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Lookup client in our map so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client with ID=%RU32 not found when it should be present\n",
                                                       u32ClientID));
    if (itClientState == mClientStateMap.end())
    {
        /* Should never happen. Complete the call on the guest side though. */
        AssertPtr(mpHelpers);
        mpHelpers->pfnCallComplete(callHandle, VERR_NOT_FOUND);

        return VERR_NOT_FOUND;
    }

    ClientState &clientState = itClientState->second;

    /* Use the current (inbound) connection. */
    ClientConnection thisCon;
    thisCon.mHandle   = callHandle;
    thisCon.mNumParms = cParms;
    thisCon.mParms    = paParms;

    return clientState.OldRunCurrent(&thisCon);
}


/**
 * Implements GUEST_MAKE_ME_MASTER.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success (we complete the message here).
 * @retval  VERR_ACCESS_DENIED if not using main VBoxGuest device not
 * @retval  VERR_RESOURCE_BUSY if there is already a master.
 * @retval  VERR_VERSION_MISMATCH if VBoxGuest didn't supply requestor info.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 */
int GstCtrlService::clientMakeMeMaster(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_RETURN(cParms == 0, VERR_WRONG_PARAMETER_COUNT);

    uint32_t fRequestor = mpHelpers->pfnGetRequestor(hCall);
    ASSERT_GUEST_LOGREL_MSG_RETURN(fRequestor != VMMDEV_REQUESTOR_LEGACY,
                                   ("Outdated VBoxGuest w/o requestor support. Please update!\n"),
                                   VERR_VERSION_MISMATCH);
    ASSERT_GUEST_LOGREL_MSG_RETURN(!(fRequestor & VMMDEV_REQUESTOR_USER_DEVICE), ("fRequestor=%#x\n", fRequestor),
                                   VERR_ACCESS_DENIED);

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    /*
     * Do the work.
     */
    ASSERT_GUEST_MSG_RETURN(m_idMasterClient == idClient || m_idMasterClient == UINT32_MAX,
                            ("Already have master session %RU32, refusing %RU32.\n", m_idMasterClient, idClient),
                            VERR_RESOURCE_BUSY);
    int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        m_idMasterClient = idClient;
        m_fLegacyMode    = false;
        rClientState.m_fIsMaster = true;
        Log(("[Client %RU32] is master.\n", idClient));
    }
    else
        LogFunc(("pfnCallComplete -> %Rrc\n", rc));

    return VINF_HGCM_ASYNC_EXECUTE;
}

/**
 * Implements GUEST_MSG_PEEK_WAIT and GUEST_MSG_PEEK_NOWAIT.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if a message was pending and is being returned.
 * @retval  VERR_TRY_AGAIN if no message pending and not blocking.
 * @retval  VERR_RESOURCE_BUSY if another read already made a waiting call.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 * @param   fWait       Set if we should wait for a message, clear if to return
 *                      immediately.
 */
int GstCtrlService::clientMsgPeek(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms >= 2, ("cParms=%u!\n", cParms), VERR_WRONG_PARAMETER_COUNT);

    uint64_t idRestoreCheck = 0;
    uint32_t i              = 0;
    if (paParms[i].type == VBOX_HGCM_SVC_PARM_64BIT)
    {
        idRestoreCheck = paParms[0].u.uint64;
        paParms[0].u.uint64 = 0;
        i++;
    }
    for (; i < cParms; i++)
    {
        ASSERT_GUEST_MSG_RETURN(paParms[i].type == VBOX_HGCM_SVC_PARM_32BIT, ("#%u type=%u\n", i, paParms[i].type),
                                VERR_WRONG_PARAMETER_TYPE);
        paParms[i].u.uint32 = 0;
    }

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    /*
     * Check restore session ID.
     */
    if (idRestoreCheck != 0)
    {
        uint64_t idRestore = mpHelpers->pfnGetVMMDevSessionId(mpHelpers);
        if (idRestoreCheck != idRestore)
        {
            paParms[0].u.uint32 = idRestore;
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_XXXX -> VERR_VM_RESTORED (%#RX64 -> %#RX64)\n",
                         idClient, idRestoreCheck, idRestore));
            return VERR_VM_RESTORED;
        }
        Assert(!mpHelpers->pfnIsCallRestored(hCall));
    }

    /*
     * Return information about the first command if one is pending in the list.
     */
    HostCmdListIter itFirstCmd = rClientState.mHostCmdList.begin();
    if (itFirstCmd != rClientState.mHostCmdList.end())
    {
        HostCommand *pFirstCmd = *itFirstCmd;
        pFirstCmd->setPeekReturn(paParms, cParms);
        LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_XXXX -> VINF_SUCCESS (idMsg=%u (%s), cParms=%u)\n",
                     idClient, pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount));
        return VINF_SUCCESS;
    }

    /*
     * If we cannot wait, fail the call.
     */
    if (!fWait)
    {
        LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_NOWAIT -> VERR_TRY_AGAIN\n", idClient));
        return VERR_TRY_AGAIN;
    }

    /*
     * Wait for the host to queue a message for this client.
     */
    ASSERT_GUEST_MSG_RETURN(rClientState.mIsPending == 0, ("Already pending! (idClient=%RU32)\n", idClient), VERR_RESOURCE_BUSY);
    rClientState.mPendingCon.mHandle    = hCall;
    rClientState.mPendingCon.mNumParms  = cParms;
    rClientState.mPendingCon.mParms     = paParms;
    rClientState.mIsPending             = GUEST_MSG_PEEK_WAIT;
    LogFlowFunc(("[Client %RU32] Is now in pending mode\n", idClient));
    return VINF_HGCM_ASYNC_EXECUTE;
}

/**
 * Implements GUEST_MSG_GET.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if message retrieved and removed from the pending queue.
 * @retval  VERR_TRY_AGAIN if no message pending.
 * @retval  VERR_BUFFER_OVERFLOW if a parmeter buffer is too small.  The buffer
 *          size was updated to reflect the required size, though this isn't yet
 *          forwarded to the guest.  (The guest is better of using peek with
 *          parameter count + 2 parameters to get the sizes.)
 * @retval  VERR_MISMATCH if the incoming message ID does not match the pending.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message was completed already.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int GstCtrlService::clientMsgGet(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate the request.
     *
     * The weird first parameter logic is due to GUEST_MSG_WAIT compatibility
     * (don't want to rewrite all the message structures).
     */
    uint32_t const idMsgExpected = cParms > 0 && paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT ? paParms[0].u.uint32
                                 : cParms > 0 && paParms[0].type == VBOX_HGCM_SVC_PARM_64BIT ? paParms[0].u.uint64
                                 : UINT32_MAX;

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);

    ClientState &rClientState = ItClientState->second;

    /*
     * Return information aobut the first command if one is pending in the list.
     */
    HostCmdListIter itFirstCmd = rClientState.mHostCmdList.begin();
    if (itFirstCmd != rClientState.mHostCmdList.end())
    {
        HostCommand *pFirstCmd = *itFirstCmd;

        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mMsgType == idMsgExpected || idMsgExpected == UINT32_MAX,
                                ("idMsg=%u (%s) cParms=%u, caller expected %u (%s) and %u\n",
                                 pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount,
                                 idMsgExpected, GstCtrlHostFnName((eHostFn)idMsgExpected), cParms),
                                VERR_MISMATCH);
        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mParmCount == cParms,
                                ("idMsg=%u (%s) cParms=%u, caller expected %u (%s) and %u\n",
                                 pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount,
                                 idMsgExpected, GstCtrlHostFnName((eHostFn)idMsgExpected), cParms),
                                VERR_WRONG_PARAMETER_COUNT);

        /* Check the parameter types. */
        for (uint32_t i = 0; i < cParms; i++)
            ASSERT_GUEST_MSG_RETURN(pFirstCmd->mpParms[i].type == paParms[i].type,
                                    ("param #%u: type %u, caller expected %u (idMsg=%u %s)\n", i, pFirstCmd->mpParms[i].type,
                                     paParms[i].type, pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType)),
                                    VERR_WRONG_PARAMETER_TYPE);

        /*
         * Copy out the parameters.
         *
         * No assertions on buffer overflows, and keep going till the end so we can
         * communicate all the required buffer sizes.
         */
        int rc = VINF_SUCCESS;
        for (uint32_t i = 0; i < cParms; i++)
            switch (pFirstCmd->mpParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    paParms[i].u.uint32 = pFirstCmd->mpParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    paParms[i].u.uint64 = pFirstCmd->mpParms[i].u.uint64;
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                {
                    uint32_t const cbSrc = pFirstCmd->mpParms[i].u.pointer.size;
                    uint32_t const cbDst = paParms[i].u.pointer.size;
                    paParms[i].u.pointer.size = cbSrc; /** @todo Check if this is safe in other layers...
                                                        * Update: Safe, yes, but VMMDevHGCM doesn't pass it along. */
                    if (cbSrc <= cbDst)
                        memcpy(paParms[i].u.pointer.addr, pFirstCmd->mpParms[i].u.pointer.addr, cbSrc);
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                    break;
                }

                default:
                    AssertMsgFailed(("#%u: %u\n", i, pFirstCmd->mpParms[i].type));
                    rc = VERR_INTERNAL_ERROR;
                    break;
            }
        if (RT_SUCCESS(rc))
        {
            /*
             * Complete the command and remove the pending message unless the
             * guest raced us and cancelled this call in the meantime.
             */
            AssertPtr(mpHelpers);
            rc = mpHelpers->pfnCallComplete(hCall, rc);
            if (rc != VERR_CANCELLED)
            {
                rClientState.mHostCmdList.erase(itFirstCmd);
                pFirstCmd->SaneRelease();
            }
            else
                LogFunc(("pfnCallComplete -> %Rrc\n", rc));
            return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
        }
        return rc;
    }

    paParms[0].u.uint32 = 0;
    paParms[1].u.uint32 = 0;
    LogFlowFunc(("[Client %RU32] GUEST_MSG_GET -> VERR_TRY_AGAIN\n", idClient));
    return VERR_TRY_AGAIN;
}

/**
 * Implements GUEST_MSG_CANCEL.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if cancelled any calls.
 * @retval  VWRN_NOT_FOUND if no callers.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   idClient    The client's ID.
 * @param   cParms      Number of parameters.
 */
int GstCtrlService::clientMsgCancel(uint32_t idClient, uint32_t cParms)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms == 0, ("cParms=%u!\n", cParms), VERR_WRONG_PARAMETER_COUNT);

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    if (rClientState.mIsPending != 0)
    {
        rClientState.CancelWaiting();
        return VINF_SUCCESS;
    }
    return VWRN_NOT_FOUND;
}


/**
 * Implements GUEST_MSG_SKIP.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_NOT_FOUND if no message pending.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientMsgSkip(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate the call.
     */
    ASSERT_GUEST_RETURN(cParms <= 2, VERR_WRONG_PARAMETER_COUNT);

    int32_t rcSkip = VERR_NOT_SUPPORTED;
    if (cParms >= 1)
    {
        ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
        rcSkip = (int32_t)paParms[0].u.uint32;
    }

    uint32_t idMsg = UINT32_MAX;
    if (cParms >= 2)
    {
        ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
        idMsg = paParms[1].u.uint32;
    }

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    /*
     * Do the job.
     */
    if (!rClientState.mHostCmdList.empty())
    {
        HostCommand *pFirstCmd = *rClientState.mHostCmdList.begin();
        if (   pFirstCmd->mMsgType == idMsg
            || idMsg == UINT32_MAX)
        {
            int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Remove the command from the queue.
                 */
                Assert(*rClientState.mHostCmdList.begin() == pFirstCmd);
                rClientState.mHostCmdList.pop_front();

                /*
                 * Compose a reply to the host service.
                 */
                VBOXHGCMSVCPARM aReplyParams[5];
                HGCMSvcSetU32(&aReplyParams[0], pFirstCmd->m_idContext);
                switch (pFirstCmd->mMsgType)
                {
                    case HOST_EXEC_CMD:
                        HGCMSvcSetU32(&aReplyParams[1], 0);              /* pid */
                        HGCMSvcSetU32(&aReplyParams[2], PROC_STS_ERROR); /* status */
                        HGCMSvcSetU32(&aReplyParams[3], rcSkip);         /* flags / whatever */
                        HGCMSvcSetPv(&aReplyParams[4], NULL, 0);         /* data buffer */
                        GstCtrlService::hostCallback(GUEST_EXEC_STATUS, 5, aReplyParams);
                        break;

                    case HOST_SESSION_CREATE:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_SESSION_NOTIFYTYPE_ERROR);    /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                            /* result */
                        GstCtrlService::hostCallback(GUEST_SESSION_NOTIFY, 3, aReplyParams);
                        break;

                    case HOST_EXEC_SET_INPUT:
                        HGCMSvcSetU32(&aReplyParams[1], pFirstCmd->mParmCount >= 2 ? pFirstCmd->mpParms[1].u.uint32 : 0);
                        HGCMSvcSetU32(&aReplyParams[2], INPUT_STS_ERROR);   /* status */
                        HGCMSvcSetU32(&aReplyParams[3], rcSkip);            /* flags / whatever */
                        HGCMSvcSetU32(&aReplyParams[4], 0);                 /* bytes consumed */
                        GstCtrlService::hostCallback(GUEST_EXEC_INPUT_STATUS, 5, aReplyParams);
                        break;

                    case HOST_FILE_OPEN:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_OPEN); /* type*/
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                     /* rc */
                        HGCMSvcSetU32(&aReplyParams[3], VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pFirstCmd->m_idContext)); /* handle */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_CLOSE:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_ERROR); /* type*/
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 3, aReplyParams);
                        break;
                    case HOST_FILE_READ:
                    case HOST_FILE_READ_AT:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_READ);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetPv(&aReplyParams[3], NULL, 0);                      /* data buffer */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_WRITE:
                    case HOST_FILE_WRITE_AT:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_WRITE); /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU32(&aReplyParams[3], 0);                           /* bytes written */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_SEEK:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_SEEK);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU64(&aReplyParams[3], 0);                           /* actual */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_TELL:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_TELL);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU64(&aReplyParams[3], 0);                           /* actual */
                        GstCtrlService::hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;

                    case HOST_EXEC_GET_OUTPUT: /** @todo This can't be right/work. */
                    case HOST_EXEC_TERMINATE:  /** @todo This can't be right/work. */
                    case HOST_EXEC_WAIT_FOR:   /** @todo This can't be right/work. */
                    case HOST_PATH_USER_DOCUMENTS:
                    case HOST_PATH_USER_HOME:
                    case HOST_PATH_RENAME:
                    case HOST_DIR_REMOVE:
                    default:
                        HGCMSvcSetU32(&aReplyParams[1], pFirstCmd->mMsgType);
                        HGCMSvcSetU32(&aReplyParams[2], (uint32_t)rcSkip);
                        HGCMSvcSetPv(&aReplyParams[3], NULL, 0);
                        GstCtrlService::hostCallback(GUEST_MSG_REPLY, 4, aReplyParams);
                        break;
                }

                /*
                 * Free the command.
                 */
                pFirstCmd->SaneRelease();
            }
            else
                LogFunc(("pfnCallComplete -> %Rrc\n", rc));
            return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
        }
        LogFunc(("Warning: GUEST_MSG_SKIP mismatch! Found %u, caller expected %u!\n", pFirstCmd->mMsgType, idMsg));
        return VERR_MISMATCH;
    }
    return VERR_NOT_FOUND;
}


/**
 * Implements GUEST_SESSION_PREPARE.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_OUT_OF_RESOURCES if too many pending sessions hanging around.
 * @retval  VERR_OUT_OF_RANGE if the session ID outside the allowed range.
 * @retval  VERR_BUFFER_OVERFLOW if key too large.
 * @retval  VERR_BUFFER_UNDERFLOW if key too small.
 * @retval  VERR_ACCESS_DENIED if not master or in legacy mode.
 * @retval  VERR_DUPLICATE if the session ID has been prepared already.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionPrepare(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(idSession >= 1, VERR_OUT_OF_RANGE);
    ASSERT_GUEST_RETURN(idSession <= 0xfff0, VERR_OUT_OF_RANGE);

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_PTR, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const cbKey = paParms[1].u.pointer.size;
    void const    *pvKey = paParms[1].u.pointer.addr;
    ASSERT_GUEST_RETURN(cbKey >= 64, VERR_BUFFER_UNDERFLOW);
    ASSERT_GUEST_RETURN(cbKey <= _16K, VERR_BUFFER_OVERFLOW);

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;
    ASSERT_GUEST_RETURN(rClientState.m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient == idClient);

    /* Now that we know it's the master, we can check for session ID duplicates. */
    GstCtrlPreparedSession *pCur;
    RTListForEach(&m_PreparedSessions, pCur, GstCtrlPreparedSession, ListEntry)
    {
        ASSERT_GUEST_RETURN(pCur->idSession != idSession, VERR_DUPLICATE);
    }

    /*
     * Make a copy of the session ID and key.
     */
    ASSERT_GUEST_RETURN(m_cPreparedSessions < 128, VERR_OUT_OF_RESOURCES);

    GstCtrlPreparedSession *pPrepped = (GstCtrlPreparedSession *)RTMemAlloc(RT_UOFFSETOF_DYN(GstCtrlPreparedSession, abKey[cbKey]));
    AssertReturn(pPrepped, VERR_NO_MEMORY);
    pPrepped->idSession = idSession;
    pPrepped->cbKey     = cbKey;
    memcpy(pPrepped->abKey, pvKey, cbKey);

    RTListAppend(&m_PreparedSessions, &pPrepped->ListEntry);
    m_cPreparedSessions++;

    /*
     * Try complete the command.
     */
    int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        LogFlow(("Prepared %u with a %#x byte key (%u pending).\n", idSession, cbKey, m_cPreparedSessions));
    else
    {
        LogFunc(("pfnCallComplete -> %Rrc\n", rc));
        RTListNodeRemove(&pPrepped->ListEntry);
        RTMemFree(pPrepped);
        m_cPreparedSessions--;
    }
    return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
}


/**
 * Implements GUEST_SESSION_CANCEL_PREPARED.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VWRN_NOT_FOUND if no session with the specified ID.
 * @retval  VERR_ACCESS_DENIED if not master or in legacy mode.
 *
 * @param   idClient    The client's ID.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionCancelPrepared(uint32_t idClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 1, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;
    ASSERT_GUEST_RETURN(rClientState.m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient == idClient);

    /*
     * Do the work.
     */
    int rc = VWRN_NOT_FOUND;
    if (idSession == UINT32_MAX)
    {
        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            RTListNodeRemove(&pCur->ListEntry);
            RTMemFree(pCur);
            rc = VINF_SUCCESS;
        }
        m_cPreparedSessions = 0;
    }
    else
    {
        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            if (pCur->idSession == idSession)
            {
                RTListNodeRemove(&pCur->ListEntry);
                RTMemFree(pCur);
                m_cPreparedSessions -= 1;
                rc = VINF_SUCCESS;
                break;
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Implements GUEST_SESSION_ACCEPT.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_NOT_FOUND if the specified session ID wasn't found.
 * @retval  VERR_MISMATCH if the key didn't match.
 * @retval  VERR_ACCESS_DENIED if we're in legacy mode or is master.
 * @retval  VERR_RESOURCE_BUSY if the client is already associated with a
 *          session.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionAccept(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(idSession >= 1, VERR_OUT_OF_RANGE);
    ASSERT_GUEST_RETURN(idSession <= 0xfff0, VERR_OUT_OF_RANGE);

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_PTR, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const cbKey = paParms[1].u.pointer.size;
    void const    *pvKey = paParms[1].u.pointer.addr;
    ASSERT_GUEST_RETURN(cbKey >= 64, VERR_BUFFER_UNDERFLOW);
    ASSERT_GUEST_RETURN(cbKey <= _16K, VERR_BUFFER_OVERFLOW);

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;
    ASSERT_GUEST_RETURN(!rClientState.m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient != idClient);
    ASSERT_GUEST_RETURN(rClientState.m_idSession == UINT32_MAX, VERR_RESOURCE_BUSY);

    /*
     * Look for the specified session and match the key to it.
     */
    GstCtrlPreparedSession *pCur;
    RTListForEach(&m_PreparedSessions, pCur, GstCtrlPreparedSession, ListEntry)
    {
        if (pCur->idSession == idSession)
        {
            if (   pCur->cbKey == cbKey
                && memcmp(pCur->abKey, pvKey, cbKey) == 0)
            {
                /*
                 * We've got a match. Try complete the request and
                 */
                int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
                if (RT_SUCCESS(rc))
                {
                    rClientState.m_idSession = idSession;

                    RTListNodeRemove(&pCur->ListEntry);
                    RTMemFree(pCur);
                    m_cPreparedSessions -= 1;
                    Log(("[Client %RU32] accepted session id %u.\n", idClient, idSession));
                }
                else
                    LogFunc(("pfnCallComplete -> %Rrc\n", rc));
                return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
            }
            LogFunc(("Key mismatch for %u!\n", idClient));
            return VERR_MISMATCH;
        }
    }

    LogFunc(("No client prepared for %u!\n", idClient));
    return VERR_NOT_FOUND;
}


/**
 * Client asks another client (guest) session to close.
 *
 * @return  IPRT status code.
 * @param   idClient        The client's ID.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::clientSessionCloseOther(uint32_t idClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate input.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idContext = paParms[0].u.uint32;

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const fFlags = paParms[1].u.uint32;

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;
    ASSERT_GUEST_RETURN(rClientState.m_fIsMaster, VERR_ACCESS_DENIED);

    /*
     * Forward the command to the destiation.
     * Since we modify the first parameter, we must make a copy of the parameters.
     */
    VBOXHGCMSVCPARM aParms[2];
    HGCMSvcSetU64(&aParms[0], idContext | VBOX_GUESTCTRL_DST_SESSION);
    HGCMSvcSetU32(&aParms[1], fFlags);
    int rc = hostProcessCommand(HOST_SESSION_CLOSE, RT_ELEMENTS(aParms), aParms);

    LogFlowFunc(("Closing guest context ID=%RU32 (from client ID=%RU32) returned with rc=%Rrc\n", idContext, idClient, rc));
    return rc;
}


/**
 * For compatiblity with old additions only - filtering / set session ID.
 *
 * @return VBox status code.
 * @param  idClient     The client's HGCM ID.
 * @param  cParms       Number of parameters.
 * @param  paParms      Array of parameters.
 */
int GstCtrlService::clientMsgOldFilterSet(uint32_t idClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate input and access.
     */
    ASSERT_GUEST_RETURN(cParms == 4, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t uValue = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t fMaskAdd = paParms[1].u.uint32;
    ASSERT_GUEST_RETURN(paParms[2].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t fMaskRemove = paParms[2].u.uint32;
    ASSERT_GUEST_RETURN(paParms[3].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* flags, unused */

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    /*
     * We have a bunch of expectations here:
     *  - Never called in non-legacy mode.
     *  - Only called once per session.
     *  - Never called by the master session.
     *  - Clients that doesn't wish for any messages passes all zeros.
     *  - All other calls has a unique session ID.
     */
    ASSERT_GUEST_LOGREL_RETURN(m_fLegacyMode, VERR_WRONG_ORDER);
    ASSERT_GUEST_LOGREL_MSG_RETURN(rClientState.m_idSession == UINT32_MAX, ("m_idSession=%#x\n", rClientState.m_idSession),
                                   VERR_WRONG_ORDER);
    ASSERT_GUEST_LOGREL_RETURN(!rClientState.m_fIsMaster, VERR_WRONG_ORDER);

    if (uValue == 0)
    {
        ASSERT_GUEST_LOGREL(fMaskAdd == 0);
        ASSERT_GUEST_LOGREL(fMaskRemove == 0);
        /* Nothing to do, already muted (UINT32_MAX). */
    }
    else
    {
        ASSERT_GUEST_LOGREL(fMaskAdd == UINT32_C(0xf8000000));
        ASSERT_GUEST_LOGREL(fMaskRemove == 0);

        uint32_t idSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uValue);
        ASSERT_GUEST_LOGREL_MSG_RETURN(idSession > 0, ("idSession=%u (%#x)\n", idSession, uValue), VERR_OUT_OF_RANGE);

        for (ClientStateMapIter It = mClientStateMap.begin(); It != mClientStateMap.end(); ++It)
            ASSERT_GUEST_LOGREL_MSG_RETURN(It->second.m_idSession != idSession,
                                           ("idSession=%u uValue=%#x idClient=%u; conflicting with client %u\n",
                                            idSession, uValue, idClient, It->second.mID),
                                           VERR_DUPLICATE);
        /* Commit it. */
        rClientState.m_idSession = idSession;
    }
    return VINF_SUCCESS;
}


/**
 * For compatibility with old additions only - skip the current command w/o
 * calling main code.
 *
 * Please note that we don't care if the caller cancelled the request, because
 * old additions code didn't give damn about VERR_INTERRUPT.
 *
 * @return VBox status code.
 * @param  idClient     The client's HGCM ID.
 * @param  cParms       Number of parameters.
 */
int GstCtrlService::clientMsgOldSkip(uint32_t idClient, uint32_t cParms)
{
    /*
     * Validate input and access.
     */
    ASSERT_GUEST_RETURN(cParms == 1, VERR_WRONG_PARAMETER_COUNT);

    ClientStateMapIter ItClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(ItClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_CLIENT_ID);
    ClientState &rClientState = ItClientState->second;

    /*
     * Execute the request.
     */
    if (!rClientState.mHostCmdList.empty())
        rClientState.OldDitchFirstHostCmd();

    LogFlowFunc(("[Client %RU32] Skipped current message - leagcy function\n", idClient));
    return VINF_SUCCESS;
}


/**
 * Notifies the host (using low-level HGCM callbacks) about an event
 * which was sent from the client.
 *
 * @return  IPRT status code.
 * @param   idFunction      Function (event) that occured.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::hostCallback(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("idFunction=%u (%s), cParms=%ld, paParms=%p\n", idFunction, GstCtrlGuestFnName((eGuestFn)idFunction), cParms, paParms));

    int rc;
    if (mpfnHostCallback)
    {
        VBOXGUESTCTRLHOSTCALLBACK data(cParms, paParms);
        /** @todo Not sure if this try/catch is necessary, I pushed it down here from
         * GstCtrlService::call where it was not needed for anything else that I
         * could spot.  I know this might be a tough, but I expect someone writing
         * this kind of code to know what can throw errors and handle them where it
         * is appropriate, rather than grand catch-all-at-the-top crap like this.
         * The reason why it is utter crap, is that you have no state cleanup code
         * where you might need it, which is why I despise exceptions in general */
        try
        {
            rc = mpfnHostCallback(mpvHostData, idFunction, (void *)(&data), sizeof(data));
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}


/**
 * Processes a command received from the host side and re-routes it to
 * a connect client on the guest.
 *
 * @return  IPRT status code.
 * @param   idFunction  Function code to process.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int GstCtrlService::hostProcessCommand(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * If no client is connected at all we don't buffer any host commands
     * and immediately return an error to the host. This avoids the host
     * waiting for a response from the guest side in case VBoxService on
     * the guest is not running/system is messed up somehow.
     */
    if (mClientStateMap.empty())
    {
        LogFlow(("GstCtrlService::hostProcessCommand: VERR_NOT_FOUND!\n"));
        return VERR_NOT_FOUND;
    }

    HostCommand *pHostCmd = new (std::nothrow) HostCommand();
    AssertReturn(pHostCmd, VERR_NO_MEMORY);

    int rc = pHostCmd->Init(idFunction, cParms, paParms);
    if (RT_SUCCESS(rc))
    {
        RTListAppend(&mHostCmdList, &pHostCmd->m_ListEntry);
        LogFlowFunc(("Handling host command m_idContextAndDst=%#RX64, idFunction=%RU32, cParms=%RU32, paParms=%p, cClients=%zu\n",
                     pHostCmd->m_idContextAndDst, idFunction, cParms, paParms, mClientStateMap.size()));

        /*
         * Find the message destination and post it to the client.  If the
         * session ID doesn't match any particular client it goes to the master.
         */
        AssertMsg(!mClientStateMap.empty(), ("Client state map is empty when it should not be!\n"));

        /* Dispatch to the session. */
        if (pHostCmd->m_idContextAndDst & VBOX_GUESTCTRL_DST_SESSION)
        {
            rc = VWRN_NOT_FOUND;
            uint32_t const idSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCmd->m_idContext);
            for (ClientStateMapIter It = mClientStateMap.begin(); It != mClientStateMap.end(); ++It)
            {
                ClientState &rClientState = It->second;
                if (rClientState.m_idSession == idSession)
                {
                    rc = rClientState.EnqueueCommand(pHostCmd);
                    if (RT_SUCCESS(rc))
                    {
                        int rc2 = rClientState.Wakeup();
                        LogFlowFunc(("Woke up client ID=%RU32 -> rc=%Rrc\n", rClientState.mID, rc2));
                        RT_NOREF(rc2);
                    }
                    break;
                }
            }
        }

        /* Does the message go to the root service? */
        if (   (pHostCmd->m_idContextAndDst & VBOX_GUESTCTRL_DST_ROOT_SVC)
            && RT_SUCCESS(rc))
        {
            ClientStateMapIter It = mClientStateMap.find(m_idMasterClient);
            if (It != mClientStateMap.end())
            {
                ClientState &rClientState = It->second;
                int rc2 = rClientState.EnqueueCommand(pHostCmd);
                if (RT_SUCCESS(rc2))
                {
                    rc2 = rClientState.Wakeup();
                    LogFlowFunc(("Woke up client ID=%RU32 (master) -> rc=%Rrc\n", rClientState.mID, rc2));
                }
                else
                    rc = rc2;
            }
            else
                rc = VERR_NOT_FOUND;
        }
    }

    /* Drop our command reference. */
    pHostCmd->SaneRelease();

    if (RT_FAILURE(rc))
        LogFunc(("Failed %Rrc (idFunction=%u, cParms=%u)\n", rc, idFunction, cParms));
    return rc;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnHostCall,
 *  Wraps to the hostProcessCommand() member function.}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcHostCall(void *pvService, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("fn=%RU32, cParms=%RU32, paParms=0x%p\n", u32Function, cParms, paParms));
    AssertReturn(u32Function != HOST_CANCEL_PENDING_WAITS, VERR_INVALID_FUNCTION);
    return pThis->hostProcessCommand(u32Function, cParms, paParms);
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnCall}
 *
 * @note    All functions which do not involve an unreasonable delay will be
 *          handled synchronously.  If needed, we will add a request handler
 *          thread in future for those which do.
 * @thread  HGCM
 */
/*static*/ DECLCALLBACK(void)
GstCtrlService::svcCall(void *pvService, VBOXHGCMCALLHANDLE hCall, uint32_t idClient, void *pvClient,
                        uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival)
{
    LogFlowFunc(("[Client %RU32] idFunction=%RU32 (%s), cParms=%RU32, paParms=0x%p\n",
                 idClient, idFunction, GstCtrlGuestFnName((eGuestFn)idFunction), cParms, paParms));
    RT_NOREF(tsArrival, pvClient);

    AssertLogRelReturnVoid(VALID_PTR(pvService));
    SELF *pThis= reinterpret_cast<SELF *>(pvService);
    AssertPtrReturnVoid(pThis);

    int rc;
    switch (idFunction)
    {
        case GUEST_MAKE_ME_MASTER:
            LogFlowFunc(("[Client %RU32] GUEST_MAKE_ME_MASTER\n", idClient));
            rc = pThis->clientMakeMeMaster(idClient, hCall, cParms);
            break;
        case GUEST_MSG_PEEK_NOWAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_NOWAIT\n", idClient));
            rc = pThis->clientMsgPeek(idClient, hCall, cParms, paParms, false /*fWait*/);
            break;
        case GUEST_MSG_PEEK_WAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_WAIT\n", idClient));
            rc = pThis->clientMsgPeek(idClient, hCall, cParms, paParms, true /*fWait*/);
            break;
        case GUEST_MSG_GET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_GET\n", idClient));
            rc = pThis->clientMsgGet(idClient, hCall, cParms, paParms);
            break;
        case GUEST_MSG_CANCEL:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_CANCEL\n", idClient));
            rc = pThis->clientMsgCancel(idClient, cParms);
            break;
        case GUEST_MSG_SKIP:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_SKIP\n", idClient));
            rc = pThis->clientMsgSkip(idClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_PREPARE:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_PREPARE\n", idClient));
            rc = pThis->clientSessionPrepare(idClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_CANCEL_PREPARED:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_CANCEL_PREPARED\n", idClient));
            rc = pThis->clientSessionCancelPrepared(idClient, cParms, paParms);
            break;
        case GUEST_SESSION_ACCEPT:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_ACCEPT\n", idClient));
            rc = pThis->clientSessionAccept(idClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_CLOSE:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_CLOSE\n", idClient));
            rc = pThis->clientSessionCloseOther(idClient, cParms, paParms);
            break;

        /*
         * Stuff the goes to various main objects:
         */
        case GUEST_DISCONNECTED:
        case GUEST_MSG_REPLY:
        case GUEST_MSG_PROGRESS_UPDATE:
        case GUEST_SESSION_NOTIFY:
        case GUEST_EXEC_OUTPUT:
        case GUEST_EXEC_STATUS:
        case GUEST_EXEC_INPUT_STATUS:
        case GUEST_EXEC_IO_NOTIFY:
        case GUEST_DIR_NOTIFY:
        case GUEST_FILE_NOTIFY:
            rc = pThis->hostCallback(idFunction, cParms, paParms);
            Assert(rc != VINF_HGCM_ASYNC_EXECUTE);
            break;

        /*
         * The remaining commands are here for compatibility with older
         * guest additions:
         */
        case GUEST_MSG_WAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_WAIT\n", idClient));
            pThis->clientMsgOldGet(idClient, hCall, cParms, paParms);
            rc = VINF_HGCM_ASYNC_EXECUTE;
            break;

        case GUEST_MSG_SKIP_OLD:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_SKIP_OLD\n", idClient));
            rc = pThis->clientMsgOldSkip(idClient, cParms);
            break;

        case GUEST_MSG_FILTER_SET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_SET\n", idClient));
            rc = pThis->clientMsgOldFilterSet(idClient, cParms, paParms);
            break;

        case GUEST_MSG_FILTER_UNSET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_UNSET\n", idClient));
            rc = VERR_NOT_IMPLEMENTED;
            break;

        /*
         * Anything else shall return fail with invalid function.
         *
         * Note! We used to return VINF_SUCCESS for these.  See bugref:9313
         *       and Guest::i_notifyCtrlDispatcher().
         */
        default:
            ASSERT_GUEST_MSG_FAILED(("idFunction=%d (%#x)\n", idFunction, idFunction));
            rc = VERR_INVALID_FUNCTION;
            break;
    }

    if (rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        /* Tell the client that the call is complete (unblocks waiting). */
        LogFlowFunc(("[Client %RU32] Calling pfnCallComplete w/ rc=%Rrc\n", idClient, rc));
        AssertPtr(pThis->mpHelpers);
        pThis->mpHelpers->pfnCallComplete(hCall, rc);
    }
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnSaveState}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcSaveState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM)
{
    RT_NOREF(pvClient);
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    SSMR3PutU32(pSSM, 1);
    SSMR3PutBool(pSSM, idClient == pThis->m_idMasterClient);
    return SSMR3PutBool(pSSM, pThis->m_fLegacyMode);
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnLoadState}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcLoadState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM, uint32_t uVersion)
{
    RT_NOREF(pvClient);
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (uVersion >= HGCM_SAVED_STATE_VERSION)
    {
        uint32_t uSubVersion;
        int rc = SSMR3GetU32(pSSM, &uSubVersion);
        AssertRCReturn(rc, rc);
        if (uSubVersion != 1)
            return SSMR3SetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                     "sub version %u, expected 1\n", uSubVersion);
        bool fLegacyMode;
        rc = SSMR3GetBool(pSSM, &fLegacyMode);
        AssertRCReturn(rc, rc);
        pThis->m_fLegacyMode = fLegacyMode;

        bool fIsMaster;
        rc = SSMR3GetBool(pSSM, &fIsMaster);
        AssertRCReturn(rc, rc);
        if (fIsMaster)
            pThis->m_idMasterClient = idClient;
        ClientStateMapIter It = pThis->mClientStateMap.find(idClient);
        if (It != pThis->mClientStateMap.end())
            It->second.m_fIsMaster = fIsMaster;
        else
            AssertFailed();
    }
    else
    {
        /*
         * For old saved states we have to guess at who should be the master.
         * Given how HGCMService::CreateAndConnectClient and associates manage
         * and saves the client, the first client connecting will be restored
         * first.  The only time this might go wrong if the there are zombie
         * VBoxService session processes in the restored guest, and I don't
         * we need to care too much about that scenario.
         *
         * Given how HGCM first re-connects the clients before this function
         * gets called, there isn't anything we need to do here it turns out. :-)
         */
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnRegisterExtension,
 *  Installs a host callback for notifications of property changes.}
 */
/*static*/ DECLCALLBACK(int) GstCtrlService::svcRegisterExtension(void *pvService, PFNHGCMSVCEXT pfnExtension, void *pvExtension)
{
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    pThis->mpfnHostCallback = pfnExtension;
    pThis->mpvHostData = pvExtension;
    return VINF_SUCCESS;
}


/**
 * @copydoc VBOXHGCMSVCLOAD
 */
extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pTable=%p\n", pTable));

    if (!VALID_PTR(pTable))
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        LogFlowFunc(("pTable->cbSize=%d, pTable->u32Version=0x%08X\n", pTable->cbSize, pTable->u32Version));

        if (   pTable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            || pTable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_VERSION_MISMATCH;
        }
        else
        {
            GstCtrlService *pService = NULL;
            /* No exceptions may propagate outside. */
            try
            {
                pService = new GstCtrlService(pTable->pHelpers);
            }
            catch (int rcThrown)
            {
                rc = rcThrown;
            }
            catch(std::bad_alloc &)
            {
                rc = VERR_NO_MEMORY;
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * We don't need an additional client data area on the host,
                 * because we're a class which can have members for that :-).
                 */
                pTable->cbClient = 0;  /** @todo this is where ClientState should live, isn't it? */

                /* Register functions. */
                pTable->pfnUnload               = GstCtrlService::svcUnload;
                pTable->pfnConnect              = GstCtrlService::svcConnect;
                pTable->pfnDisconnect           = GstCtrlService::svcDisconnect;
                pTable->pfnCall                 = GstCtrlService::svcCall;
                pTable->pfnHostCall             = GstCtrlService::svcHostCall;
                pTable->pfnSaveState            = GstCtrlService::svcSaveState;
                pTable->pfnLoadState            = GstCtrlService::svcLoadState;
                pTable->pfnRegisterExtension    = GstCtrlService::svcRegisterExtension;

                /* Service specific initialization. */
                pTable->pvService = pService;
            }
            else
            {
                if (pService)
                {
                    delete pService;
                    pService = NULL;
                }
            }
        }
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}

