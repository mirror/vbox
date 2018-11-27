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
#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/HostServices/GuestControlSvc.h>

#include <VBox/log.h>
#include <VBox/AssertGuest.h>
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
#include <memory>  /* for auto_ptr */
#include <string>
#include <list>


namespace guestControl {

/** Flag for indicating that the client only is interested in
 *  messages of specific context IDs. */
#define CLIENTSTATE_FLAG_CONTEXTFILTER      RT_BIT(0)

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
    RTLISTNODE Node;

    uint32_t AddRef(void)
    {
#ifdef DEBUG_andy
        LogFlowThisFunc(("[Cmd %RU32] Adding reference, new refCount=%RU32\n", mMsgType, mRefCount + 1));
#endif
        return ++mRefCount;
    }

    uint32_t Release(void)
    {
#ifdef DEBUG_andy
        LogFlowThisFunc(("[Cmd %RU32] Releasing reference, new refCount=%RU32\n", mMsgType, mRefCount - 1));
#endif
        /* Release reference for current command. */
        Assert(mRefCount);
        if (--mRefCount == 0)
            Free();

        return mRefCount;
    }

    /**
     * Allocates the command with an HGCM request - or put more accurately, it
     * duplicates the given HGCM reguest (it does not allocate a HostCommand).
     *
     * Needs to be freed using Free().
     *
     * @return  IPRT status code.
     * @param   uMsg                    Message type.
     * @param   cParms                  Number of parameters of HGCM request.
     * @param   paParms                 Array of parameters of HGCM request.
     */
    int Allocate(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        LogFlowThisFunc(("[Cmd %RU32] Allocating cParms=%RU32, paParms=%p\n", uMsg, cParms, paParms));

        if (!cParms) /* At least one parameter (context ID) must be present. */
            return VERR_INVALID_PARAMETER;

        AssertPtrReturn(paParms, VERR_INVALID_POINTER);

        /* Paranoia. */
        if (cParms > 256)
            cParms = 256;

        int rc = VINF_SUCCESS;

        /*
         * Don't verify anything here (yet), because this function only buffers
         * the HGCM data into an internal structure and reaches it back to the guest (client)
         * in an unmodified state.
         */
        mMsgType = uMsg;
        mParmCount = cParms;
        if (mParmCount)
        {
            mpParms = (VBOXHGCMSVCPARM *)RTMemAllocZ(sizeof(VBOXHGCMSVCPARM) * mParmCount);
            if (NULL == mpParms)
                rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            for (uint32_t i = 0; i < mParmCount; i++)
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
                            mpParms[i].u.pointer.addr = RTMemAlloc(mpParms[i].u.pointer.size);
                            if (mpParms[i].u.pointer.addr != NULL)
                                memcpy(mpParms[i].u.pointer.addr,
                                       paParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.size);
                            else
                                rc = VERR_NO_MEMORY;
                        }
                        else
                        {
                            /* Size is 0 -- make sure we don't have any pointer. */
                            mpParms[i].u.pointer.addr = NULL;
                        }
                        break;

                    default:
                        break;
                }
                if (RT_FAILURE(rc))
                    break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            /*
             * Assume that the context ID *always* is the first parameter,
             * assign the context ID to the command.
             */
            rc = HGCMSvcGetU32(&mpParms[0], &mContextID);

            /* Set timestamp so that clients can distinguish between already
             * processed commands and new ones. */
            mTimestamp = RTTimeNanoTS();
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
    }

    /**
     * Frees the buffered HGCM request (not the HostCommand structure itself).
     */
    void Free(void)
    {
        AssertMsg(mRefCount == 0, ("uMsg=%RU32, CID=%RU32 still being used by a client (%RU32 refs), cannot free yet\n",
                                   mMsgType, mContextID, mRefCount));

        LogFlowThisFunc(("[Cmd %RU32] Freeing\n", mMsgType));

        for (uint32_t i = 0; i < mParmCount; i++)
        {
            switch (mpParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_PTR:
                    if (mpParms[i].u.pointer.size > 0)
                        RTMemFree(mpParms[i].u.pointer.addr);
                    break;

                default:
                    break;
            }
        }

        if (mpParms)
        {
            RTMemFree(mpParms);
            mpParms = NULL;
        }

        mParmCount = 0;

        /* Removes the command from its list */
        RTListNodeRemove(&Node);
    }

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
        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, mContextID=%RU32 (Session %RU32)\n",
                         mMsgType, mParmCount, mContextID, VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(mContextID)));

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

    /** Reference count for keeping track how many connected
     *  clients still need to process this command until it can
     *  be removed. */
    uint32_t mRefCount;
    /** The context ID this command belongs to. Will be extracted
     *  *always* from HGCM parameter [0]. */
    uint32_t mContextID;
    /** Dynamic structure for holding the HGCM parms */
    uint32_t mMsgType;
    /** Number of HGCM parameters. */
    uint32_t mParmCount;
    /** Array of HGCM parameters. */
    PVBOXHGCMSVCPARM mpParms;
    /** Incoming timestamp (nanoseconds). */
    uint64_t mTimestamp;
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
    ClientState(void)
        : mSvcHelpers(NULL),
          mID(0),
          mFlags(0),
          mFilterMask(0), mFilterValue(0),
          mHostCmdRc(VINF_SUCCESS), mHostCmdTries(0),
          mHostCmdTS(0),
          mIsPending((guestControl::eGuestFn)0),
          mPeekCount(0)
    { }

    ClientState(PVBOXHGCMSVCHELPERS pSvcHelpers, uint32_t uClientID)
        : mSvcHelpers(pSvcHelpers),
          mID(uClientID),
          mFlags(0),
          mFilterMask(0), mFilterValue(0),
          mHostCmdRc(VINF_SUCCESS), mHostCmdTries(0),
          mHostCmdTS(0),
          mIsPending((guestControl::eGuestFn)0),
          mPeekCount(0)
    { }

    void DequeueAll(void)
    {
        HostCmdListIter curItem = mHostCmdList.begin();
        while (curItem != mHostCmdList.end())
            curItem = Dequeue(curItem);
    }

    void DequeueCurrent(void)
    {
        HostCmdListIter curCmd = mHostCmdList.begin();
        if (curCmd != mHostCmdList.end())
            Dequeue(curCmd);
    }

    HostCmdListIter Dequeue(HostCmdListIter &curItem)
    {
        HostCommand *pHostCmd = *curItem;
        AssertPtr(pHostCmd);

        if (pHostCmd->Release() == 0)
        {
            LogFlowThisFunc(("[Client %RU32] Destroying command %RU32\n", mID, pHostCmd->mMsgType));

            delete pHostCmd;
            pHostCmd = NULL;
        }

        HostCmdListIter nextItem = mHostCmdList.erase(curItem);

        /* Reset everything else. */
        mHostCmdRc    = VINF_SUCCESS;
        mHostCmdTries = 0;
        mPeekCount    = 0;

        return nextItem;
    }

    int EnqueueCommand(HostCommand *pHostCmd)
    {
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        int rc = VINF_SUCCESS;

        try
        {
            mHostCmdList.push_back(pHostCmd);
            pHostCmd->AddRef();
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }

        return rc;
    }

#if 0 /* not used by anyone */
    /** Returns the pointer to the current host command in a client's list.
     *  NULL if no current command available. */
    const HostCommand *GetCurrent(void)
    {
        if (mHostCmdList.empty())
            return NULL;

        return (*mHostCmdList.begin());
    }
#endif

    bool WantsHostCommand(const HostCommand *pHostCmd) const
    {
        AssertPtrReturn(pHostCmd, false);

#ifdef DEBUG_andy
        LogFlowFunc(("mHostCmdTS=%RU64, pHostCmdTS=%RU64\n",
                     mHostCmdTS, pHostCmd->mTimestamp));
#endif

        /* Only process newer commands. */
        /** @todo r=bird: This seems extremely bogus given that I cannot see
         *        ClientState::mHostCmdTS being set anywhere at all. */
        if (pHostCmd->mTimestamp <= mHostCmdTS)
            return false;

        /*
         * If a session filter is set, only obey those commands we're interested in
         * by applying our context ID filter mask and compare the result with the
         * original context ID.
         */
        bool fWant;
        if (mFlags & CLIENTSTATE_FLAG_CONTEXTFILTER)
            fWant = (pHostCmd->mContextID & mFilterMask) == mFilterValue;
        else /* Client is interested in all commands. */
            fWant = true;

        LogFlowFunc(("[Client %RU32] mFlags=0x%x, mContextID=%RU32 (session %RU32), mFilterMask=0x%x, mFilterValue=%RU32, fWant=%RTbool\n",
                     mID, mFlags, pHostCmd->mContextID,
                     VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCmd->mContextID),
                     mFilterMask, mFilterValue, fWant));

        return fWant;
    }

    /**
     * Set to indicate that a client call (GUEST_MSG_WAIT) is pending.
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldSetPending(const ClientConnection *pConnection)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);

        if (mIsPending != 0)
        {
            LogFlowFunc(("[Client %RU32] Already is in pending mode\n", mID));

            /*
             * Signal that we don't and can't return yet.
             */
            return VINF_HGCM_ASYNC_EXECUTE;
        }

        if (mHostCmdList.empty())
        {
            AssertMsg(mIsPending == 0, ("Client ID=%RU32 already is pending but tried to receive a new host command\n", mID));

            mPendingCon.mHandle   = pConnection->mHandle;
            mPendingCon.mNumParms = pConnection->mNumParms;
            mPendingCon.mParms    = pConnection->mParms;

            mIsPending = GUEST_MSG_WAIT;

            LogFlowFunc(("[Client %RU32] Is now in pending mode\n", mID));

            /*
             * Signal that we don't and can't return yet.
             */
            return VINF_HGCM_ASYNC_EXECUTE;
        }

        /*
         * Signal that there already is a connection pending.
         * Shouldn't happen in daily usage.
         */
        AssertMsgFailed(("Client already has a connection pending\n"));
        return VERR_SIGNAL_PENDING;
    }

    /**
     * Used by Wakeup() and OldRunCurrent().
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldRun(ClientConnection const *pConnection,
               HostCommand            *pHostCmd)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

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
            /** @todo Fix this (slow) lookup. Too late today. */
            HostCmdListIter curItem = mHostCmdList.begin();
            while (curItem != mHostCmdList.end())
            {
                if ((*curItem) == pHostCmd)
                {
                    Dequeue(curItem);
                    break;
                }

                ++curItem;
            }
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

        int rc;
        if (mHostCmdList.empty())
        {
            rc = OldSetPending(pConnection);
        }
        else
        {
            AssertMsgReturn(mIsPending == 0,
                            ("Client ID=%RU32 still is in pending mode; can't use another connection\n", mID), VERR_INVALID_PARAMETER);

            HostCmdListIter curCmd = mHostCmdList.begin();
            Assert(curCmd != mHostCmdList.end());
            HostCommand *pHostCmd = *curCmd;
            AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

            rc = OldRun(pConnection, pHostCmd);
        }

        return rc;
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

            HostCmdListIter curCmd = mHostCmdList.begin();
            if (curCmd != mHostCmdList.end())
            {
                HostCommand *pHostCmd = (*curCmd);
                AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

                LogFlowThisFunc(("[Client %RU32] Current host command is %RU32 (CID=%RU32, cParms=%RU32, refCount=%RU32)\n",
                                 mID, pHostCmd->mMsgType, pHostCmd->mContextID, pHostCmd->mParmCount, pHostCmd->mRefCount));

                if (mIsPending == GUEST_MSG_PEEK_WAIT)
                {
                    HGCMSvcSetU32(&mPendingCon.mParms[0], pHostCmd->mMsgType);
                    HGCMSvcSetU32(&mPendingCon.mParms[1], pHostCmd->mParmCount);
                    for (uint32_t i = pHostCmd->mParmCount; i >= 2; i--)
                        switch (pHostCmd->mpParms[i - 2].type)
                        {
                            case VBOX_HGCM_SVC_PARM_32BIT: mPendingCon.mParms[i].u.uint32 = ~(uint32_t)sizeof(uint32_t); break;
                            case VBOX_HGCM_SVC_PARM_64BIT: mPendingCon.mParms[i].u.uint32 = ~(uint32_t)sizeof(uint64_t); break;
                            case VBOX_HGCM_SVC_PARM_PTR:   mPendingCon.mParms[i].u.uint32 = pHostCmd->mpParms[i - 2].u.pointer.size; break;
                        }

                    rc = mSvcHelpers->pfnCallComplete(mPendingCon.mHandle, VINF_SUCCESS);
                    mIsPending = (guestControl::eGuestFn)0;
                }
                else if (mIsPending == GUEST_MSG_WAIT)
                    rc = OldRun(&mPendingCon, pHostCmd);
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
     * Used by Service::call() to handle GUEST_CANCEL_PENDING_WAITS.
     *
     * @note This cancels both GUEST_MSG_WAIT and GUEST_MSG_PEEK_WAIT sleepers.
     */
    int CancelWaiting()
    {
        LogFlowFunc(("[Client %RU32] Cancelling waiting thread, isPending=%d, pendingNumParms=%RU32, flags=%x\n",
                     mID, mIsPending, mPendingCon.mNumParms, mFlags));

        int rc;
        if (   mIsPending != 0
            && mPendingCon.mNumParms >= 2)
        {
            HGCMSvcSetU32(&mPendingCon.mParms[0], HOST_CANCEL_PENDING_WAITS); /* Message ID. */
            HGCMSvcSetU32(&mPendingCon.mParms[1], 0);                         /* Required parameters for message. */

            AssertPtr(mSvcHelpers);
            mSvcHelpers->pfnCallComplete(mPendingCon.mHandle, mIsPending == GUEST_MSG_WAIT ? VINF_SUCCESS : VINF_TRY_AGAIN);

            mIsPending = (guestControl::eGuestFn)0;

            rc = VINF_SUCCESS;
        }
        else if (mPendingCon.mNumParms < 2)
            rc = VERR_BUFFER_OVERFLOW;
        else /** @todo Enqueue command instead of dropping? */
            rc = VERR_WRONG_ORDER;

        return rc;
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

    PVBOXHGCMSVCHELPERS mSvcHelpers;
    /** The client's ID. */
    uint32_t mID;
    /** Client flags. @sa CLIENTSTATE_FLAG_ flags. */
    uint32_t mFlags;
    /** The context ID filter mask, if any. */
    uint32_t mFilterMask;
    /** The context ID filter value, if any. */
    uint32_t mFilterValue;
    /** Host command list to process. */
    HostCmdList mHostCmdList;
    /** Last (most recent) rc after handling the host command. */
    int mHostCmdRc;
    /** How many GUEST_MSG_WAIT calls the client has issued to retrieve one command.
     *
     * This is used as a heuristic to remove a message that the client appears not
     * to be able to successfully retrieve.  */
    uint32_t mHostCmdTries;
    /** Timestamp (nanoseconds) of last host command processed.
     * @todo r=bird: Where is this set?  */
    uint64_t mHostCmdTS;
    /** Pending client call (GUEST_MSG_PEEK_WAIT or GUEST_MSG_WAIT), zero if none pending.
     *
     * This means the client waits for a new host command to reply and won't return
     * from the waiting call until a new host command is available.
     */
    guestControl::eGuestFn mIsPending;
    /** Number of times we've peeked at a pending message.
     *
     * This is necessary for being compatible with older Guest Additions.  In case
     * there are commands which only have two (2) parameters and therefore would fit
     * into the GUEST_MSG_WAIT reply immediately, we now can make sure that the
     * client first gets back the GUEST_MSG_WAIT results first.
     */
    uint32_t mPeekCount;
    /** The client's pending connection. */
    ClientConnection mPendingCon;
} ClientState;
typedef std::map< uint32_t, ClientState > ClientStateMap;
typedef std::map< uint32_t, ClientState >::iterator ClientStateMapIter;
typedef std::map< uint32_t, ClientState >::const_iterator ClientStateMapIterConst;

/**
 * Class containing the shared information service functionality.
 */
class Service : public RTCNonCopyable
{

private:

    /** Type definition for use in callback functions. */
    typedef Service SELF;
    /** HGCM helper functions. */
    PVBOXHGCMSVCHELPERS mpHelpers;
    /**
     * Callback function supplied by the host for notification of updates
     * to properties.
     */
    PFNHGCMSVCEXT mpfnHostCallback;
    /** User data pointer to be supplied to the host callback function. */
    void *mpvHostData;
    /** List containing all buffered host commands. */
    RTLISTANCHOR mHostCmdList;
    /** Map containing all connected clients. The primary key contains
     *  the HGCM client ID to identify the client. */
    ClientStateMap mClientStateMap;
public:
    explicit Service(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
    {
        RTListInit(&mHostCmdList);
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnUnload}
     * Simply deletes the service object
     */
    static DECLCALLBACK(int) svcUnload(void *pvService)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        int rc = pSelf->uninit();
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            delete pSelf;
        return rc;
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect}
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcConnect(void *pvService,
                                        uint32_t u32ClientID,
                                        void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        AssertPtrReturn(pSelf, VERR_INVALID_POINTER);
        return pSelf->clientConnect(u32ClientID, pvClient);
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect}
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcDisconnect(void *pvService,
                                           uint32_t u32ClientID,
                                           void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        AssertPtrReturn(pSelf, VERR_INVALID_POINTER);
        return pSelf->clientDisconnect(u32ClientID, pvClient);
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnCall}
     * Wraps to the call member function
     */
    static DECLCALLBACK(void) svcCall(void * pvService,
                                      VBOXHGCMCALLHANDLE callHandle,
                                      uint32_t u32ClientID,
                                      void *pvClient,
                                      uint32_t u32Function,
                                      uint32_t cParms,
                                      VBOXHGCMSVCPARM paParms[],
                                      uint64_t tsArrival)
    {
        AssertLogRelReturnVoid(VALID_PTR(pvService));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        AssertPtrReturnVoid(pSelf);
        RT_NOREF_PV(tsArrival);
        pSelf->call(callHandle, u32ClientID, pvClient, u32Function, cParms, paParms);
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnHostCall}
     * Wraps to the hostCall member function
     */
    static DECLCALLBACK(int) svcHostCall(void *pvService,
                                         uint32_t u32Function,
                                         uint32_t cParms,
                                         VBOXHGCMSVCPARM paParms[])
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        AssertPtrReturn(pSelf, VERR_INVALID_POINTER);
        return pSelf->hostCall(u32Function, cParms, paParms);
    }

    /**
     * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnRegisterExtension}
     * Installs a host callback for notifications of property changes.
     */
    static DECLCALLBACK(int) svcRegisterExtension(void *pvService,
                                                  PFNHGCMSVCEXT pfnExtension,
                                                  void *pvExtension)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        AssertPtrReturn(pSelf, VERR_INVALID_POINTER);
        pSelf->mpfnHostCallback = pfnExtension;
        pSelf->mpvHostData = pvExtension;
        return VINF_SUCCESS;
    }

private:

    int prepareExecute(uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientConnect(uint32_t u32ClientID, void *pvClient);
    int clientDisconnect(uint32_t u32ClientID, void *pvClient);
    int clientMsgOldGet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgPeek(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait);
    int clientMsgGet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgCancel(uint32_t u32ClientID, uint32_t cParms);
    int clientMsgFilterSet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgFilterUnset(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgSkip(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostCallback(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostProcessCommand(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int sessionClose(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit(void);

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(Service);
};

/**
 * Handles a client which just connected.
 *
 * @return  IPRT status code.
 * @param   u32ClientID
 * @param   pvClient
 */
int Service::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    RT_NOREF(pvClient);
    LogFlowFunc(("[Client %RU32] Connected\n", u32ClientID));
#ifdef VBOX_STRICT
    ClientStateMapIterConst it = mClientStateMap.find(u32ClientID);
    if (it != mClientStateMap.end())
    {
        AssertMsgFailed(("Client with ID=%RU32 already connected when it should not\n",
                         u32ClientID));
        return VERR_ALREADY_EXISTS;
    }
#endif
    ClientState clientState(mpHelpers, u32ClientID);
    mClientStateMap[u32ClientID] = clientState;
    /** @todo Exception handling! */
    return VINF_SUCCESS;
}

/**
 * Handles a client which disconnected.
 *
 * This functiond does some internal cleanup as well as sends notifications to
 * the host so that the host can do the same (if required).
 *
 * @return  IPRT status code.
 * @param   u32ClientID             The client's ID of which disconnected.
 * @param   pvClient                User data, not used at the moment.
 */
int Service::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    RT_NOREF(pvClient);
    LogFlowFunc(("[Client %RU32] Disconnected (%zu clients total)\n",
                 u32ClientID, mClientStateMap.size()));

    AssertMsg(!mClientStateMap.empty(),
              ("No clients in list anymore when there should (client ID=%RU32)\n", u32ClientID));

    int rc = VINF_SUCCESS;

    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(),
              ("Client ID=%RU32 not found in client list when it should be there\n", u32ClientID));

    if (itClientState != mClientStateMap.end())
    {
        itClientState->second.DequeueAll();

        mClientStateMap.erase(itClientState);
    }

    bool fAllClientsDisconnected = mClientStateMap.empty();
    if (fAllClientsDisconnected)
    {
        LogFlowFunc(("All clients disconnected, cancelling all host commands ...\n"));

        /*
         * If all clients disconnected we also need to make sure that all buffered
         * host commands need to be notified, because Main is waiting a notification
         * via a (multi stage) progress object.
         */
        /** @todo r=bird: We have RTListForEachSafe for this purpose...  Would save a
         *        few lines and bother here. */
        HostCommand *pCurCmd = RTListGetFirst(&mHostCmdList, HostCommand, Node);
        while (pCurCmd)
        {
            HostCommand *pNext = RTListNodeGetNext(&pCurCmd->Node, HostCommand, Node);
            bool fLast = RTListNodeIsLast(&mHostCmdList, &pCurCmd->Node);

            uint32_t cParms = 0;
            VBOXHGCMSVCPARM arParms[2];
            HGCMSvcSetU32(&arParms[cParms++], pCurCmd->mContextID);

            int rc2 = hostCallback(GUEST_DISCONNECTED, cParms, arParms);
            if (RT_FAILURE(rc2))
            {
                LogFlowFunc(("Cancelling host command with CID=%u (refCount=%RU32) failed with rc=%Rrc\n",
                             pCurCmd->mContextID, pCurCmd->mRefCount, rc2));
                /* Keep going. */
            }

            while (pCurCmd->Release())
                ;
            delete pCurCmd;
            pCurCmd = NULL;

            if (fLast)
                break;

            pCurCmd = pNext;
        }

        Assert(RTListIsEmpty(&mHostCmdList));
    }

    return rc;
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
int Service::clientMsgOldGet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
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
 * Implements GUEST_MSG_PEEK_WAIT and GUEST_MSG_PEEK_NOWAIT.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if a message was pending and is being returned.
 * @retval  VERR_INVALID_HANDLE if invalid client ID.
 * @retval  VERR_INVALID_PARAMETER if incorrect parameter count or types.
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
int Service::clientMsgPeek(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms >= 2, ("cParms=%u!\n", cParms), VERR_INVALID_PARAMETER);
    for (uint32_t i = 0; i < cParms; i++)
    {
        ASSERT_GUEST_MSG_RETURN(paParms[i].type == VBOX_HGCM_SVC_PARM_32BIT, ("#%u type=%u\n", i, paParms[i].type),
                                VERR_INVALID_PARAMETER);
        paParms[i].u.uint32 = 0;
    }

    ClientStateMapIter itClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(itClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_HANDLE);

    ClientState &rClientState = itClientState->second;

    /*
     * Return information about the first command if one is pending in the list.
     */
    HostCmdListIter itFirstCmd = rClientState.mHostCmdList.begin();
    if (itFirstCmd != rClientState.mHostCmdList.end())
    {
        HostCommand *pFirstCmd = *itFirstCmd;
        paParms[0].u.uint32 = pFirstCmd->mMsgType;
        paParms[1].u.uint32 = pFirstCmd->mParmCount;
        for (uint32_t i = pFirstCmd->mParmCount; i >= 2; i--)
            switch (pFirstCmd->mpParms[i - 2].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT: paParms[i].u.uint32 = ~(uint32_t)sizeof(uint32_t); break;
                case VBOX_HGCM_SVC_PARM_64BIT: paParms[i].u.uint32 = ~(uint32_t)sizeof(uint64_t); break;
                case VBOX_HGCM_SVC_PARM_PTR:   paParms[i].u.uint32 = pFirstCmd->mpParms[i - 2].u.pointer.size; break;
            }

        LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_XXXX -> VINF_SUCCESS (idMsg=%u, cParms=%u)\n",
                     idClient, pFirstCmd->mMsgType, pFirstCmd->mParmCount));
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
 *          size was updated to reflect the required size.
 * @retval  VERR_MISMATCH if the incoming message ID does not match the pending.
 * @retval  VERR_OUT_OF_RANGE if the wrong parameter count.
 * @retval  VERR_WRONG_TYPE if a parameter has the wrong type.
 * @retval  VERR_INVALID_HANDLE if invalid client ID.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   idClient    The client's ID.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int Service::clientMsgGet(uint32_t idClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
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

    ClientStateMapIter itClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(itClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_HANDLE);

    ClientState &rClientState = itClientState->second;

    /*
     * Return information aobut the first command if one is pending in the list.
     */
    HostCmdListIter itFirstCmd = rClientState.mHostCmdList.begin();
    if (itFirstCmd != rClientState.mHostCmdList.end())
    {
        HostCommand *pFirstCmd = *itFirstCmd;

        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mMsgType == idMsgExpected || idMsgExpected == UINT32_MAX,
                                ("idMsg=%u cParms=%u, caller expected %u and %u\n",
                                 pFirstCmd->mMsgType, pFirstCmd->mParmCount, idMsgExpected, cParms),
                                VERR_MISMATCH);
        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mParmCount == cParms,
                                ("idMsg=%u cParms=%u, caller expected %u and %u\n",
                                 pFirstCmd->mMsgType, pFirstCmd->mParmCount, idMsgExpected, cParms),
                                VERR_OUT_OF_RANGE);

        /* Check the parameter types. */
        for (uint32_t i = 0; i < cParms; i++)
            ASSERT_GUEST_MSG_RETURN(pFirstCmd->mpParms[i].type == paParms[i].type,
                                    ("param #%u: type %u, caller expected %u\n", i, pFirstCmd->mpParms[i].type, paParms[i].type),
                                    VERR_WRONG_TYPE);

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
                if (pFirstCmd->Release() == 0)
                {
                    delete pFirstCmd;
                    LogFlow(("[Client %RU32] Destroying command (idMsg=%u cParms=%u)\n", idClient, idMsgExpected, cParms));
                }
            }
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
 * @retval  VERR_INVALID_PARAMETER if any parameters specified (expects zero).
 * @retval  VERR_INVALID_HANDLE if invalid client ID.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   idClient    The client's ID.
 * @param   cParms      Number of parameters.
 */
int Service::clientMsgCancel(uint32_t idClient, uint32_t cParms)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms == 0, ("cParms=%u!\n", cParms), VERR_INVALID_PARAMETER);

    ClientStateMapIter itClientState = mClientStateMap.find(idClient);
    ASSERT_GUEST_MSG_RETURN(itClientState != mClientStateMap.end(), ("idClient=%RU32\n", idClient), VERR_INVALID_HANDLE);

    ClientState &rClientState = itClientState->second;
    if (rClientState.mIsPending != 0)
    {
        rClientState.CancelWaiting();
        return VINF_SUCCESS;
    }
    return VWRN_NOT_FOUND;
}


/**
 * A client tells this service to set a message filter.
 * That way a client only will get new messages which matches the filter.
 *
 * @return VBox status code.
 * @param  u32ClientID                  The client's ID.
 * @param  callHandle                   The client's call handle.
 * @param  cParms                       Number of parameters.
 * @param  paParms                      Array of parameters.
 */
int Service::clientMsgFilterSet(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                                uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(callHandle);

    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client with ID=%RU32 not found when it should be present\n",
                                                       u32ClientID));
    if (itClientState == mClientStateMap.end())
        return VERR_NOT_FOUND; /* Should never happen. */

    if (cParms != 4)
        return VERR_INVALID_PARAMETER;

    uint32_t uValue;
    int rc = HGCMSvcGetU32(&paParms[0], &uValue);
    if (RT_SUCCESS(rc))
    {
        uint32_t uMaskAdd;
        rc = HGCMSvcGetU32(&paParms[1], &uMaskAdd);
        if (RT_SUCCESS(rc))
        {
            uint32_t uMaskRemove;
            rc = HGCMSvcGetU32(&paParms[2], &uMaskRemove);
            /** @todo paParm[3] (flags) not used yet. */
            if (RT_SUCCESS(rc))
            {
                ClientState &clientState = itClientState->second;

                clientState.mFlags |= CLIENTSTATE_FLAG_CONTEXTFILTER;
                if (uMaskAdd)
                    clientState.mFilterMask |= uMaskAdd;
                if (uMaskRemove)
                    clientState.mFilterMask &= ~uMaskRemove;

                clientState.mFilterValue = uValue;

                LogFlowFunc(("[Client %RU32] Setting message filterMask=0x%x, filterVal=%RU32 set (flags=0x%x, maskAdd=0x%x, maskRemove=0x%x)\n",
                             u32ClientID, clientState.mFilterMask, clientState.mFilterValue,
                             clientState.mFlags, uMaskAdd, uMaskRemove));
            }
        }
    }

    return rc;
}

/**
 * A client tells this service to unset (clear) its message filter.
 *
 * @return VBox status code.
 * @param  u32ClientID                  The client's ID.
 * @param  callHandle                   The client's call handle.
 * @param  cParms                       Number of parameters.
 * @param  paParms                      Array of parameters.
 */
int Service::clientMsgFilterUnset(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                                  uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(callHandle, paParms);

    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client with ID=%RU32 not found when it should be present\n",
                                                       u32ClientID));
    if (itClientState == mClientStateMap.end())
        return VERR_NOT_FOUND; /* Should never happen. */

    if (cParms != 1)
        return VERR_INVALID_PARAMETER;

    ClientState &clientState = itClientState->second;

    clientState.mFlags &= ~CLIENTSTATE_FLAG_CONTEXTFILTER;
    clientState.mFilterMask = 0;
    clientState.mFilterValue = 0;

    LogFlowFunc(("[Client %RU32} Unset message filter\n", u32ClientID));
    return VINF_SUCCESS;
}

/**
 * A client tells this service that the current command can be skipped and therefore can be removed
 * from the internal command list.
 *
 * @return VBox status code.
 * @param  u32ClientID                  The client's ID.
 * @param  callHandle                   The client's call handle.
 * @param  cParms                       Number of parameters.
 * @param  paParms                      Array of parameters.
 */
int Service::clientMsgSkip(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                           uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(callHandle, cParms, paParms);

    int rc;

    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client ID=%RU32 not found when it should be present\n", u32ClientID));
    if (itClientState != mClientStateMap.end())
    {
        itClientState->second.DequeueCurrent();
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFunc(("[Client %RU32] Skipped current message, rc=%Rrc\n", u32ClientID, rc));
    return rc;
}

/**
 * Notifies the host (using low-level HGCM callbacks) about an event
 * which was sent from the client.
 *
 * @return  IPRT status code.
 * @param   eFunction               Function (event) that occured.
 * @param   cParms                  Number of parameters.
 * @param   paParms                 Array of parameters.
 */
int Service::hostCallback(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("eFunction=%ld, cParms=%ld, paParms=%p\n",
                 eFunction, cParms, paParms));

    int rc;
    if (mpfnHostCallback)
    {
        VBOXGUESTCTRLHOSTCALLBACK data(cParms, paParms);
        rc = mpfnHostCallback(mpvHostData, eFunction, (void *)(&data), sizeof(data));
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
 * @param   eFunction               Function code to process.
 * @param   cParms                  Number of parameters.
 * @param   paParms                 Array of parameters.
 */
int Service::hostProcessCommand(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * If no client is connected at all we don't buffer any host commands
     * and immediately return an error to the host. This avoids the host
     * waiting for a response from the guest side in case VBoxService on
     * the guest is not running/system is messed up somehow.
     */
    if (mClientStateMap.empty())
        return VERR_NOT_FOUND;

    int rc;

    HostCommand *pHostCmd = NULL;
    try
    {
        pHostCmd = new HostCommand();
        rc = pHostCmd->Allocate(eFunction, cParms, paParms);
        if (RT_SUCCESS(rc))
            /* rc = */ RTListAppend(&mHostCmdList, &pHostCmd->Node);
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("Handling host command CID=%RU32, eFunction=%RU32, cParms=%RU32, paParms=%p, numClients=%zu\n",
                     pHostCmd->mContextID, eFunction, cParms, paParms, mClientStateMap.size()));

        /*
         * Wake up all pending clients which are interested in this
         * host command.
         */
#ifdef DEBUG
        uint32_t uClientsWokenUp = 0;
#endif
        ClientStateMapIter itClientState = mClientStateMap.begin();
        AssertMsg(itClientState != mClientStateMap.end(), ("Client state map is empty when it should not\n"));
        while (itClientState != mClientStateMap.end())
        {
            ClientState &clientState = itClientState->second;

            /* If a client indicates that it it wants the new host command,
             * add a reference to not delete it.*/
            if (clientState.WantsHostCommand(pHostCmd))
            {
                clientState.EnqueueCommand(pHostCmd);

                int rc2 = clientState.Wakeup();
                if (RT_FAILURE(rc2))
                    LogFlowFunc(("Waking up client ID=%RU32 failed with rc=%Rrc\n",
                                 itClientState->first, rc2));
#ifdef DEBUG
                uClientsWokenUp++;
#endif
                /** @todo r=bird: Do we need to queue commands on more than one client? */
            }

            ++itClientState;
        }

#ifdef DEBUG
        LogFlowFunc(("%RU32 clients have been woken up\n", uClientsWokenUp));
#endif
    }
    /** @todo r=bird: If pHostCmd->Allocate fails, you leak stuff.  It's not
     *        likely, since it'll only fail if the host gives us an incorrect
     *        parameter list (first param isn't uint32_t) or we're out of memory.
     *        In the latter case, of course, you're not exactly helping...  */

    return rc;
}

/**
 * Worker for svcCall() that helps implement VBOXHGCMSVCFNTABLE::pfnCall.
 *
 * @note    All functions which do not involve an unreasonable delay will be
 *          handled synchronously.  If needed, we will add a request handler
 *          thread in future for those which do.
 *
 * @thread  HGCM
 */
void Service::call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
                   void * /* pvClient */, uint32_t eFunction, uint32_t cParms,
                   VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("[Client %RU32] eFunction=%RU32, cParms=%RU32, paParms=0x%p\n",
                 u32ClientID, eFunction, cParms, paParms));
    try
    {
        /*
         * The guest asks the host for the next message to process.
         */
        if (eFunction == GUEST_MSG_WAIT)
        {
            LogFlowFunc(("[Client %RU32] GUEST_MSG_WAIT\n", u32ClientID));
            rc = clientMsgOldGet(u32ClientID, callHandle, cParms, paParms);
        }
        else
        {
            switch (eFunction)
            {
                /*
                 * A client wants to shut down and asks us (this service) to cancel
                 * all blocking/pending waits (VINF_HGCM_ASYNC_EXECUTE) so that the
                 * client can gracefully shut down.
                 */
                case GUEST_CANCEL_PENDING_WAITS:
                {
                    LogFlowFunc(("[Client %RU32] GUEST_CANCEL_PENDING_WAITS\n", u32ClientID));
                    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
                    if (itClientState != mClientStateMap.end())
                        rc = itClientState->second.CancelWaiting();
                    break;
                }

                /*
                 * The guest only wants certain messages set by the filter mask(s).
                 * Since VBox 4.3+.
                 */
                case GUEST_MSG_FILTER_SET:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_SET\n", u32ClientID));
                    rc = clientMsgFilterSet(u32ClientID, callHandle, cParms, paParms);
                    break;

                /*
                 * Unsetting the message filter flag.
                 */
                case GUEST_MSG_FILTER_UNSET:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_UNSET\n", u32ClientID));
                    rc = clientMsgFilterUnset(u32ClientID, callHandle, cParms, paParms);
                    break;

                /*
                 * The guest only wants skip the currently assigned messages. Neded
                 * for dropping its assigned reference of the current assigned host
                 * command in queue.
                 * Since VBox 4.3+.
                 */
                case GUEST_MSG_SKIP:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_SKIP\n", u32ClientID));
                    rc = clientMsgSkip(u32ClientID, callHandle, cParms, paParms);
                    break;

                /*
                 * New message peeking and retrieval functions replacing GUEST_MSG_WAIT.
                 */
                case GUEST_MSG_PEEK_NOWAIT:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_NOWAIT\n", u32ClientID));
                    rc = clientMsgPeek(u32ClientID, callHandle, cParms, paParms, false /*fWait*/);
                    break;
                case GUEST_MSG_PEEK_WAIT:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_WAIT\n", u32ClientID));
                    rc = clientMsgPeek(u32ClientID, callHandle, cParms, paParms, true /*fWait*/);
                    break;
                case GUEST_MSG_GET:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_GET\n", u32ClientID));
                    rc = clientMsgGet(u32ClientID, callHandle, cParms, paParms);
                    break;
                case GUEST_MSG_CANCEL:
                    LogFlowFunc(("[Client %RU32] GUEST_MSG_CANCEL\n", u32ClientID));
                    rc = clientMsgCancel(u32ClientID, cParms);
                    break;

                /*
                 * The guest wants to close specific guest session. This is handy for
                 * shutting down dedicated guest session processes from another process.
                 */
                case GUEST_SESSION_CLOSE:
                    LogFlowFunc(("[Client %RU32] GUEST_SESSION_CLOSE\n", u32ClientID));
                    rc = sessionClose(u32ClientID, callHandle, cParms, paParms);
                    break;

                /*
                 * For all other regular commands we call our hostCallback
                 * function. If the current command does not support notifications,
                 * notifyHost will return VERR_NOT_SUPPORTED.
                 */
                default:
                    rc = hostCallback(eFunction, cParms, paParms);
                    break;
            }

            if (rc != VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Tell the client that the call is complete (unblocks waiting). */
                AssertPtr(mpHelpers);
                mpHelpers->pfnCallComplete(callHandle, rc);
            }
        }
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }
}

/**
 * Service call handler for the host.
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnHostCall}
 * @thread  hgcm
 */
int Service::hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VERR_NOT_SUPPORTED;
    LogFlowFunc(("fn=%RU32, cParms=%RU32, paParms=0x%p\n",
                 eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /**
             * Host
             */
            case HOST_CANCEL_PENDING_WAITS:
            {
                LogFlowFunc(("HOST_CANCEL_PENDING_WAITS\n"));
                ClientStateMapIter itClientState = mClientStateMap.begin();
                while (itClientState != mClientStateMap.end())
                {
                    int rc2 = itClientState->second.CancelWaiting();
                    if (RT_FAILURE(rc2))
                        LogFlowFunc(("Cancelling waiting for client ID=%RU32 failed with rc=%Rrc",
                                     itClientState->first, rc2));
                    ++itClientState;
                }
                rc = VINF_SUCCESS;
                break;
            }

            default:
                rc = hostProcessCommand(eFunction, cParms, paParms);
                break;
        }
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

/**
 * Client asks another client (guest) session to close.
 *
 * @return  IPRT status code.
 * @param   u32ClientID                 The client's ID.
 * @param   callHandle                  The client's call handle.
 * @param   cParms                      Number of parameters.
 * @param   paParms                     Array of parameters.
 */
int Service::sessionClose(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(u32ClientID, callHandle);
    if (cParms < 2)
        return VERR_INVALID_PARAMETER;

    uint32_t uContextID, uFlags;
    int rc = HGCMSvcGetU32(&paParms[0], &uContextID);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[1], &uFlags);

    uint32_t uSessionID = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID);

    if (RT_SUCCESS(rc))
        rc = hostProcessCommand(HOST_SESSION_CLOSE, cParms, paParms);

    LogFlowFunc(("Closing guest session ID=%RU32 (from client ID=%RU32) returned with rc=%Rrc\n",
                 uSessionID, u32ClientID, rc)); NOREF(uSessionID);
    return rc;
}

int Service::uninit(void)
{
    return VINF_SUCCESS;
}

} /* namespace guestControl */

using guestControl::Service;

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
            Service *pService = NULL;
            /* No exceptions may propagate outside. */
            try
            {
                pService = new Service(pTable->pHelpers);
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
                pTable->cbClient = 0;

                /* Register functions. */
                pTable->pfnUnload             = Service::svcUnload;
                pTable->pfnConnect            = Service::svcConnect;
                pTable->pfnDisconnect         = Service::svcDisconnect;
                pTable->pfnCall               = Service::svcCall;
                pTable->pfnHostCall           = Service::svcHostCall;
                pTable->pfnSaveState          = NULL;  /* The service is stateless, so the normal */
                pTable->pfnLoadState          = NULL;  /* construction done before restoring suffices */
                pTable->pfnRegisterExtension  = Service::svcRegisterExtension;

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

