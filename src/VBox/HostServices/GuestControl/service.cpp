/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/HostServices/GuestControlSvc.h>

#include <VBox/log.h>
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

#include "gctrl.h"

namespace guestControl {

/** Flag for indicating that the client only is interested in
 *  messages for specific contexts. */
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
    ClientConnection(void) : mHandle(0), mNumParms(0), mParms(NULL) {}
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
        return ++mRefCount;
    }

    uint32_t Release(void)
    {
        LogFlowFunc(("Releasing CID=%RU32, refCount=%RU32\n",
                     mContextID, mRefCount));

        /* Release reference for current command. */
        Assert(mRefCount);
        if (--mRefCount == 0)
            Free();

        return mRefCount;
    }

    /**
     * Allocates the command with an HGCM request. Needs to be free'd using Free().
     *
     * @return  IPRT status code.
     * @param   uMsg                    Message type.
     * @param   cParms                  Number of parameters of HGCM request.
     * @param   paParms                 Array of parameters of HGCM request.
     */
    int Allocate(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        LogFlowFunc(("Allocating uMsg=%RU32, cParms=%RU32, paParms=%p\n",
                     uMsg, cParms, paParms));

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
            mpParms = (VBOXHGCMSVCPARM*)RTMemAllocZ(sizeof(VBOXHGCMSVCPARM) * mParmCount);
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
                        /* Not supported yet. */
                        break;

                    case VBOX_HGCM_SVC_PARM_PTR:
                        mpParms[i].u.pointer.size = paParms[i].u.pointer.size;
                        if (mpParms[i].u.pointer.size > 0)
                        {
                            mpParms[i].u.pointer.addr = RTMemAlloc(mpParms[i].u.pointer.size);
                            if (NULL == mpParms[i].u.pointer.addr)
                            {
                                rc = VERR_NO_MEMORY;
                                break;
                            }
                            else
                                memcpy(mpParms[i].u.pointer.addr,
                                       paParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.size);
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
            rc = mpParms[0].getUInt32(&mContextID);
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
    }

    /**
     * Frees the buffered HGCM request.
     *
     * @return  IPRT status code.
     */
    void Free(void)
    {
        AssertMsg(mRefCount == 0, ("Command still being used by a client (%RU32 refs), cannot free yet\n",
                                   mRefCount));

        LogFlowFunc(("Freeing host command CID=%RU32, mMsgType=%RU32, mParmCount=%RU32, mpParms=%p\n",
                     mContextID, mMsgType, mParmCount, mpParms));

        for (uint32_t i = 0; i < mParmCount; i++)
        {
            switch (mpParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_PTR:
                    if (mpParms[i].u.pointer.size > 0)
                        RTMemFree(mpParms[i].u.pointer.addr);
                    break;
            }
        }

        if (mpParms)
            RTMemFree(mpParms);

        mParmCount = 0;
    }

    /**
     * Copies data from the buffered HGCM request to the current HGCM request.
     *
     * @return  IPRT status code.
     * @param   paDstParms              Array of parameters of HGCM request to fill the data into.
     * @param   cPDstarms               Number of parameters the HGCM request can handle.
     * @param   pSrcBuf                 Parameter buffer to assign.
     */
    int CopyTo(VBOXHGCMSVCPARM paDstParms[], uint32_t cDstParms) const
    {
        int rc = VINF_SUCCESS;
        if (cDstParms != mParmCount)
        {
            LogFlowFunc(("Parameter count does not match (got %RU32, expected %RU32)\n",
                         cDstParms, mParmCount));
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            for (uint32_t i = 0; i < mParmCount; i++)
            {
                if (paDstParms[i].type != mpParms[i].type)
                {
                    LogFlowFunc(("Parameter %RU32 type mismatch (got %RU32, expected %RU32)\n",
                                 i, paDstParms[i].type, mpParms[i].type));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    switch (mpParms[i].type)
                    {
                        case VBOX_HGCM_SVC_PARM_32BIT:
                            paDstParms[i].u.uint32 = mpParms[i].u.uint32;
                            break;

                        case VBOX_HGCM_SVC_PARM_PTR:
                        {
                            if (!mpParms[i].u.pointer.size)
                                continue; /* Only copy buffer if there actually is something to copy. */

                            if (!paDstParms[i].u.pointer.addr)
                                rc = VERR_INVALID_PARAMETER;

                            if (paDstParms[i].u.pointer.size < mpParms[i].u.pointer.size)
                                rc = VERR_BUFFER_OVERFLOW;

                            if (RT_SUCCESS(rc))
                            {
                                memcpy(paDstParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.size);
                            }

                            break;
                        }

                        case VBOX_HGCM_SVC_PARM_64BIT:
                            /* Fall through is intentional. */
                        default:
                            LogFlowFunc(("Parameter %RU32 of type %RU32 is not supported yet\n",
                                         i, mpParms[i].type));
                            rc = VERR_NOT_SUPPORTED;
                            break;
                    }
                }

                if (RT_FAILURE(rc))
                {
                    LogFlowFunc(("Parameter %RU32 invalid (rc=%Rrc), refusing\n",
                                 i, rc));
                    break;
                }
            }
        }

        return rc;
    }

    int AssignToConnection(const ClientConnection *pConnection)
    {
        int rc;

        LogFlowFunc(("mMsgType=%RU32, mParmCount=%RU32, mpParms=%p\n",
                     mMsgType, mParmCount, mpParms));

        /* Does the current host command need more parameter space which
         * the client does not provide yet? */
        if (mParmCount > pConnection->mNumParms)
        {
            pConnection->mParms[0].setUInt32(mMsgType);   /* Message ID */
            pConnection->mParms[1].setUInt32(mParmCount); /* Required parameters for message */

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

            /* Has there been enough parameter space but the wrong parameter types
             * were submitted -- maybe the client was just asking for the next upcoming
             * host message?
             *
             * Note: To keep this compatible to older clients we return VERR_TOO_MUCH_DATA
             *       in every case. */
            if (RT_FAILURE(rc))
                rc = VERR_TOO_MUCH_DATA;
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
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
    uint32_t mParmCount;
    PVBOXHGCMSVCPARM mpParms;
} HostCommand;

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
 * Structure for holding all clients with their
 * generated host contexts. This is necessary for
 * maintaining the relationship between a client and its context ID(s).
 */
typedef struct ClientState
{
    ClientState(PVBOXHGCMSVCHELPERS pSvcHelpers)
        : mSvcHelpers(pSvcHelpers), mpHostCmd(NULL),
          mFlags(0), mContextFilter(0), mIsPending(false),
          mHostCmdTries(0), mHostCmdRc(VINF_SUCCESS) {}

    ClientState(void)
        : mSvcHelpers(NULL), mpHostCmd(NULL),
          mFlags(0), mContextFilter(0), mIsPending(false),
          mHostCmdTries(0), mHostCmdRc(VINF_SUCCESS) {}

    bool WantsHostCommand(const HostCommand *pHostCmd) const
    {
        AssertPtrReturn(pHostCmd, false);

        /*
         * If a sesseion filter is set, only obey those sessions we're interested in.
         */
        if (mFlags & CLIENTSTATE_FLAG_CONTEXTFILTER)
        {
            if (VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCmd->mContextID) == mContextFilter)
                return true;
        }
        else /* Client is interested in all commands. */
            return true;

        return false;
    }

    int SetPending(const ClientConnection *pConnection)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);

        if (mpHostCmd == NULL)
        {
            AssertMsg(mIsPending == false,
                      ("Client %p already is pending but tried to receive a new host command\n", this));

            mPending.mHandle   = pConnection->mHandle;
            mPending.mNumParms = pConnection->mNumParms;
            mPending.mParms    = pConnection->mParms;

            mIsPending = true;

            LogFlowFunc(("Client now is in pending mode\n"));

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

    int Run(const ClientConnection *pConnection,
            const RTLISTANCHOR     *pHostCmdList)
    {
        int rc = VINF_SUCCESS;

        LogFlowFunc(("Client pConnection=%p, pHostCmdList=%p\n",
                      pConnection, pHostCmdList));
        LogFlowFunc(("Client hostCmd=%p, mHostCmdRc=%Rrc, mHostCmdTries=%RU32\n",
                      mpHostCmd, mHostCmdRc, mHostCmdTries));

        /* Do we have a current command? */
        if (mpHostCmd)
        {
            bool fRemove = false;
            if (RT_FAILURE(mHostCmdRc))
            {
                mHostCmdTries++;

                /*
                 * If the client understood the message but supplied too little buffer space
                 * don't send this message again and drop it after 3 unsuccessful attempts.
                 * The host then should take care of next actions (maybe retry it with a smaller buffer).
                 */
                if (   mHostCmdRc    == VERR_TOO_MUCH_DATA
                    && mHostCmdTries >= 3)
                {
                    fRemove = true;
                }
                /* Client did not understand the message or something else weird happened. Try again one
                 * more time and drop it if it didn't get handled then. */
                else if (mHostCmdTries > 1)
                    fRemove = true;
            }
            else
                fRemove = true; /* Everything went fine, remove it. */

            LogFlowFunc(("Client tried CID=%RU32 for %RU32 times, (last result=%Rrc, fRemove=%RTbool)\n",
                         mpHostCmd->mContextID, mHostCmdTries, mHostCmdRc, fRemove));

            if (fRemove)
            {
                LogFlowFunc(("Client removes itself from command CID=%RU32\n",
                              mpHostCmd->mContextID));

                /* Remove command from context map. */
                mContextMap.erase(mpHostCmd->mContextID);

                /* Release reference for current command. */
                if (mpHostCmd->Release() == 0)
                {
                    LogFlowFunc(("Destroying and removing command CID=%RU32 from list\n",
                                 mpHostCmd->mContextID));

                    /* Last reference removes the command from the list. */
                    RTListNodeRemove(&mpHostCmd->Node);

                    RTMemFree(mpHostCmd);
                    mpHostCmd = NULL;
                }

                /* Reset everything else. */
                mHostCmdRc    = VINF_SUCCESS;
                mHostCmdTries = 0;
            }
        }

        /* ... or don't we have one (anymore?) Try getting a new one to process now. */
        if (mpHostCmd == NULL)
        {
            /* Get the next host command the clienet is interested in. */
            bool fFoundCmd = false;
            HostCommand *pCurCmd;
            RTListForEach(pHostCmdList, pCurCmd, HostCommand, Node)
            {
                fFoundCmd = WantsHostCommand(pCurCmd);
                if (fFoundCmd)
                {
                    mpHostCmd = pCurCmd;
                    AssertPtr(mpHostCmd);
                    mpHostCmd->AddRef();

                    /* Create a command context to keep track of client-specific
                     * information about a certain command. */
                    Assert(mContextMap.find(mpHostCmd->mContextID) == mContextMap.end());
                    mContextMap[mpHostCmd->mContextID] = ClientContext(mpHostCmd);
                    /** @todo Exception handling! */

                    LogFlowFunc(("Assigning next host comamnd CID=%RU32, cmdType=%RU32, cmdParms=%RU32, new refCount=%RU32\n",
                                 mpHostCmd->mContextID, mpHostCmd->mMsgType, mpHostCmd->mParmCount, mpHostCmd->mRefCount));
                    break;
                }
            }

            LogFlowFunc(("Client %s new command\n",
                         fFoundCmd ? "found" : "did not find a"));

            /* If no new command was found, set client into pending state. */
            if (!fFoundCmd)
                rc = SetPending(pConnection);
        }

        if (mpHostCmd)
        {
            AssertPtr(mpHostCmd);
            mHostCmdRc = SendReply(pConnection, mpHostCmd);
            if (RT_FAILURE(mHostCmdRc))
            {
                LogFlowFunc(("Processing command CID=%RU32 ended with rc=%Rrc\n",
                             mpHostCmd->mContextID, mHostCmdRc));
            }

            if (RT_SUCCESS(rc))
                rc = mHostCmdRc;
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
    }

    int RunNow(const ClientConnection *pConnection,
               const PRTLISTANCHOR     pHostCmdList)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmdList, VERR_INVALID_POINTER);

        AssertMsgReturn(!mIsPending, ("Can't use another connection when client still is in pending mode\n"),
                        VERR_INVALID_PARAMETER);

        int rc = Run(pConnection, pHostCmdList);

        LogFlowFunc(("Returned with rc=%Rrc\n"));
        return rc;
    }

    int Wakeup(const PRTLISTANCHOR pHostCmdList)
    {
        AssertMsgReturn(mIsPending, ("Cannot wake up a client which is not in pending mode\n"),
                        VERR_INVALID_PARAMETER);

        int rc = Run(&mPending, pHostCmdList);

        /* Reset pending state. */
        mIsPending = false;

        LogFlowFunc(("Returned with rc=%Rrc\n"));
        return rc;
    }

    int CancelWaiting(int rcPending)
    {
        LogFlowFunc(("Cancelling waiting with %Rrc, isPending=%RTbool, pendingNumParms=%RU32, flags=%x\n",
                     rcPending, mIsPending, mPending.mNumParms, mFlags));

        if (   mIsPending
            && mPending.mNumParms >= 2)
        {
            mPending.mParms[0].setUInt32(HOST_CANCEL_PENDING_WAITS); /* Message ID. */
            mPending.mParms[1].setUInt32(0);                         /* Required parameters for message. */

            AssertPtr(mSvcHelpers);
            mSvcHelpers->pfnCallComplete(mPending.mHandle, rcPending);

            mIsPending = false;
        }

        return VINF_SUCCESS;
    }

    int SendReply(const ClientConnection *pConnection,
                        HostCommand      *pHostCmd)
    {
        AssertPtrReturn(pConnection, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        /* Try assigning the host command to the client and store the
         * result code for later use. */
        int rc = pHostCmd->AssignToConnection(pConnection);

        /* In any case the client did something, so wake up and remove from list. */
        AssertPtr(mSvcHelpers);
        mSvcHelpers->pfnCallComplete(pConnection->mHandle, rc);

        LogFlowFunc(("pConnection=%p, pHostCmd=%p, rc=%Rrc\n",
                     pConnection, pHostCmd, rc));
        return rc;
    }

    PVBOXHGCMSVCHELPERS mSvcHelpers;
    /** Client flags. @sa CLIENTSTATE_FLAG_ flags. */
    uint32_t mFlags;
    /** The context ID filter, based on the flags set. */
    uint32_t mContextFilter;
    /** Map containing all context IDs a client is assigned to. */
    //std::map< uint32_t, ClientContext > mContextMap;
    /** Pointer to current host command to process. */
    HostCommand *mpHostCmd;
    /** Last (most recent) rc after handling the
     *  host command. */
    int mHostCmdRc;
    /** How many times the host service has tried to deliver this
     *  command to the according client. */
    uint32_t mHostCmdTries;
    /** Map containing all context IDs a client is assigned to. */
    ClientContextMap mContextMap;
    /** Flag indicating whether the client currently is pending. */
    bool mIsPending;
    /** The client's pending connection. */
    ClientConnection mPending;
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
     * @copydoc VBOXHGCMSVCHELPERS::pfnUnload
     * Simply deletes the service object
     */
    static DECLCALLBACK(int) svcUnload (void *pvService)
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
     * @copydoc VBOXHGCMSVCHELPERS::pfnConnect
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcConnect (void *pvService,
                                         uint32_t u32ClientID,
                                         void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc (("pvService=%p, u32ClientID=%u, pvClient=%p\n", pvService, u32ClientID, pvClient));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        return pSelf->clientConnect(u32ClientID, pvClient);
    }

    /**
     * @copydoc VBOXHGCMSVCHELPERS::pfnConnect
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcDisconnect (void *pvService,
                                            uint32_t u32ClientID,
                                            void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc (("pvService=%p, u32ClientID=%u, pvClient=%p\n", pvService, u32ClientID, pvClient));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        return pSelf->clientDisconnect(u32ClientID, pvClient);
    }

    /**
     * @copydoc VBOXHGCMSVCHELPERS::pfnCall
     * Wraps to the call member function
     */
    static DECLCALLBACK(void) svcCall (void * pvService,
                                       VBOXHGCMCALLHANDLE callHandle,
                                       uint32_t u32ClientID,
                                       void *pvClient,
                                       uint32_t u32Function,
                                       uint32_t cParms,
                                       VBOXHGCMSVCPARM paParms[])
    {
        AssertLogRelReturnVoid(VALID_PTR(pvService));
        LogFlowFunc (("pvService=%p, callHandle=%p, u32ClientID=%u, pvClient=%p, u32Function=%u, cParms=%u, paParms=%p\n",
                      pvService, callHandle, u32ClientID, pvClient, u32Function, cParms, paParms));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        pSelf->call(callHandle, u32ClientID, pvClient, u32Function, cParms, paParms);
    }

    /**
     * @copydoc VBOXHGCMSVCHELPERS::pfnHostCall
     * Wraps to the hostCall member function
     */
    static DECLCALLBACK(int) svcHostCall (void *pvService,
                                          uint32_t u32Function,
                                          uint32_t cParms,
                                          VBOXHGCMSVCPARM paParms[])
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc (("pvService=%p, u32Function=%u, cParms=%u, paParms=%p\n", pvService, u32Function, cParms, paParms));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        return pSelf->hostCall(u32Function, cParms, paParms);
    }

    /**
     * @copydoc VBOXHGCMSVCHELPERS::pfnRegisterExtension
     * Installs a host callback for notifications of property changes.
     */
    static DECLCALLBACK(int) svcRegisterExtension (void *pvService,
                                                   PFNHGCMSVCEXT pfnExtension,
                                                   void *pvExtension)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        pSelf->mpfnHostCallback = pfnExtension;
        pSelf->mpvHostData = pvExtension;
        return VINF_SUCCESS;
    }

private:

    int prepareExecute(uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientConnect(uint32_t u32ClientID, void *pvClient);
    int clientDisconnect(uint32_t u32ClientID, void *pvClient);
    int clientGetCommand(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSetMsgFilter(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int cancelHostCmd(uint32_t u32ContextID);
    int cancelPendingWaits(uint32_t u32ClientID, int rcPending);
    int hostCallback(VBOXHGCMCALLHANDLE callHandle, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostProcessCommand(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit(void);
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
    LogFlowFunc(("New client with ID=%RU32 connected\n", u32ClientID));
#ifdef VBOX_STRICT
    ClientStateMapIterConst it = mClientStateMap.find(u32ClientID);
    if (it != mClientStateMap.end())
    {
        AssertMsgFailed(("Client with ID=%RU32 already connected when it should not\n",
                         u32ClientID));
        return VERR_ALREADY_EXISTS;
    }
#endif
    ClientState cs(mpHelpers);
    mClientStateMap[u32ClientID] = cs;
    /** @todo Exception handling! */
    return VINF_SUCCESS;
}

/**
 * Handles a client which disconnected. This functiond does some
 * internal cleanup as well as sends notifications to the host so
 * that the host can do the same (if required).
 *
 * @return  IPRT status code.
 * @param   u32ClientID             The client's ID of which disconnected.
 * @param   pvClient                User data, not used at the moment.
 */
int Service::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("Client with ID=%RU32 (%zu clients total) disconnected\n",
                 u32ClientID, mClientStateMap.size()));

    /* If this was the last connected (guest) client we need to
     * unblock all eventually queued up (waiting) host calls. */
    bool fAllClientsDisconnected = mClientStateMap.size() == 0;
    if (fAllClientsDisconnected)
        LogFlowFunc(("No connected clients left, notifying all queued up host callbacks\n"));

    /*
     * Throw out all stale clients.
     */
    int rc = VINF_SUCCESS;

    ClientStateMapIter itClientState = mClientStateMap.begin();
    while (   itClientState != mClientStateMap.end()
           && RT_SUCCESS(rc))
    {
        /*
         * Unblock/call back all queued items of the specified client
         * or for all items in case there is no waiting client around
         * anymore.
         */
        if (   itClientState->first == u32ClientID
            || fAllClientsDisconnected)
        {
            LogFlowFunc(("Cancelling %RU32 context of client ID=%RU32\n",
                         itClientState->second.mContextMap.size(), u32ClientID));

            ClientContextMapIter itContext = itClientState->second.mContextMap.begin();
            while (itContext != itClientState->second.mContextMap.end())
            {
                uint32_t uContextID = itContext->first;

                /*
                 * Notify the host that clients with u32ClientID are no longer
                 * around and need to be cleaned up (canceling waits etc).
                 */
                LogFlowFunc(("Notifying CID=%RU32 of disconnect ...\n", uContextID));
                int rc2 = cancelHostCmd(uContextID);
                if (RT_FAILURE(rc))
                {
                    LogFlowFunc(("Cancelling assigned client CID=%RU32 failed with rc=%Rrc\n",
                                 uContextID, rc2));
                    /* Keep going. */
                }

                AssertPtr(itContext->second.mpHostCmd);
                itContext->second.mpHostCmd->Release();

                LogFlowFunc(("Released host command CID=%RU32 returned with rc=%Rrc\n",
                             uContextID, rc2));

                itContext++;
            }
            itClientState = mClientStateMap.erase(itClientState);
        }
        else
            itClientState++;
    }

    if (fAllClientsDisconnected)
    {
        /*
         * If all clients disconnected we also need to make sure that all buffered
         * host commands need to be notified, because Main is waiting a notification
         * via a (multi stage) progress object.
         */
        HostCommand *pCurCmd = RTListGetFirst(&mHostCmdList, HostCommand, Node);
        while (pCurCmd)
        {
            HostCommand *pNext = RTListNodeGetNext(&pCurCmd->Node, HostCommand, Node);
            bool fLast = RTListNodeIsLast(&mHostCmdList, &pCurCmd->Node);

            int rc2 = cancelHostCmd(pCurCmd->mContextID);
            if (RT_FAILURE(rc2))
            {
                LogFlowFunc(("Cancelling host command with CID=%u (refCount=%RU32) failed with rc=%Rrc\n",
                             pCurCmd->mContextID, pCurCmd->mRefCount, rc2));
                /* Keep going. */
            }

            pCurCmd->Free();

            RTListNodeRemove(&pCurCmd->Node);
            RTMemFree(pCurCmd);

            if (fLast)
                break;

            pCurCmd = pNext;
        }

        Assert(RTListIsEmpty(&mHostCmdList));
    }

    return rc;
}

/**
 * Either fills in parameters from a pending host command into our guest context or
 * defer the guest call until we have something from the host.
 *
 * @return  IPRT status code.
 * @param   u32ClientID                 The client's ID.
 * @param   callHandle                  The client's call handle.
 * @param   cParms                      Number of parameters.
 * @param   paParms                     Array of parameters.
 */
int Service::clientGetCommand(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                              uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client with ID=%RU32 not found when it should be present\n",
                                                       u32ClientID));
    if (itClientState == mClientStateMap.end())
        return VERR_NOT_FOUND; /* Should never happen. */

    ClientState &clientState = itClientState->second;

    /* Use the current (inbound) connection. */
    ClientConnection thisCon;
    thisCon.mHandle   = callHandle;
    thisCon.mNumParms = cParms;
    thisCon.mParms    = paParms;

    /*
     * If host command list is empty (nothing to do right now) just
     * defer the call until we got something to do (makes the client
     * wait).
     */
    int rc;
    if (RTListIsEmpty(&mHostCmdList))
    {
        rc = clientState.SetPending(&thisCon);
    }
    else
    {
        rc = clientState.RunNow(&thisCon, &mHostCmdList);
    }

    LogFlowFunc(("Returned with rc=%Rrc\n", rc));
    return rc;
}

int Service::clientSetMsgFilter(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                                uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    AssertMsg(itClientState != mClientStateMap.end(), ("Client with ID=%RU32 not found when it should be present\n",
                                                       u32ClientID));
    if (itClientState == mClientStateMap.end())
        return VERR_NOT_FOUND; /* Should never happen. */

    if (cParms != 2)
        return VERR_INVALID_PARAMETER;

    uint32_t uMaskAdd;
    int rc = paParms[0].getUInt32(&uMaskAdd);
    if (RT_SUCCESS(rc))
    {
        /* paParms[1] unused yet. */

        ClientState &clientState = itClientState->second;

        clientState.mFlags |= CLIENTSTATE_FLAG_CONTEXTFILTER;
        clientState.mContextFilter = uMaskAdd;

        LogFlowFunc(("Client ID=%RU32 now has filter=%x enabled (flags=%x)\n",
                     u32ClientID, clientState.mContextFilter, clientState.mFlags));
    }

    LogFlowFunc(("Returned with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Cancels a buffered host command to unblock waiting on Main side
 * via callbacks.
 *
 * @return  IPRT status code.
 * @param   u32ContextID                Context ID of host command to cancel.
 */
int Service::cancelHostCmd(uint32_t u32ContextID)
{
    Assert(mpfnHostCallback);

    LogFlowFunc(("Cancelling CID=%u ...\n", u32ContextID));

    CALLBACKDATA_CLIENT_DISCONNECTED data;
    data.hdr.uContextID = u32ContextID;

    AssertPtr(mpfnHostCallback);
    AssertPtr(mpvHostData);

    return mpfnHostCallback(mpvHostData, GUEST_DISCONNECTED, (void *)(&data), sizeof(data));
}

/**
 * Client asks itself (in another thread) to cancel all pending waits which are blocking the client
 * from shutting down / doing something else.
 *
 * @return  IPRT status code.
 * @param   u32ClientID                 The client's ID.
 * @param   rcPending                   Result code for completing pending operation.
 */
int Service::cancelPendingWaits(uint32_t u32ClientID, int rcPending)
{
    ClientStateMapIter itClientState = mClientStateMap.find(u32ClientID);
    if (itClientState != mClientStateMap.end())
        return itClientState->second.CancelWaiting(rcPending);

    return VINF_SUCCESS;
}

/**
 * Notifies the host (using low-level HGCM callbacks) about an event
 * which was sent from the client.
 *
 * @return  IPRT status code.
 * @param   callHandle              Call handle.
 * @param   eFunction               Function (event) that occured.
 * @param   cParms                  Number of parameters.
 * @param   paParms                 Array of parameters.
 */
int Service::hostCallback(VBOXHGCMCALLHANDLE callHandle,
                          uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("eFunction=%ld, cParms=%ld, paParms=%p\n",
                 eFunction, cParms, paParms));

    int rc;
    if (mpfnHostCallback)
    {
        VBOXGUESTCTRLHOSTCALLBACK data(cParms, paParms);
        rc = mpfnHostCallback(mpvHostData, eFunction,
                              (void *)(&data), sizeof(data));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

/**
 * Processes a command receiveed from the host side and re-routes it to
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
    if (mClientStateMap.size() == 0)
        return VERR_NOT_FOUND;

    int rc;
    HostCommand *pHostCmd = (HostCommand*)RTMemAllocZ(sizeof(HostCommand));
    if (pHostCmd)
    {
        rc = pHostCmd->Allocate(eFunction, cParms, paParms);
        if (RT_SUCCESS(rc))
            RTListAppend(&mHostCmdList, &pHostCmd->Node);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("Handling host command CID=%RU32, numClients=%zu\n",
                     pHostCmd->mContextID, mClientStateMap.size()));

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
            if (itClientState->second.mIsPending) /* Only wake up pending clients. */
            {
                LogFlowFunc(("Waking up client ID=%RU32 (isPending=%RTbool) ...\n",
                             itClientState->first, itClientState->second.mIsPending));

                ClientState &clientState = itClientState->second;
                int rc2 = clientState.Wakeup(&mHostCmdList);
                LogFlowFunc(("Client ID=%RU32 wakeup ended with rc=%Rrc\n",
                             itClientState->first, rc2));
#ifdef DEBUG
                uClientsWokenUp++;
#endif
            }
            else
                LogFlowFunc(("Client ID=%RU32 is not in pending state\n",
                             itClientState->first));

            itClientState++;
        }

#ifdef DEBUG
        LogFlowFunc(("%RU32 clients have been succcessfully woken up\n",
                      uClientsWokenUp));
#endif
    }

    LogFlowFunc(("Returned with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Handle an HGCM service call.
 * @copydoc VBOXHGCMSVCFNTABLE::pfnCall
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
    LogFlowFunc(("u32ClientID=%RU32, fn=%RU32, cParms=%RU32, paParms=0x%p\n",
                 u32ClientID, eFunction, cParms, paParms));
    try
    {
        /*
         * The guest asks the host for the next message to process.
         */
        if (eFunction == GUEST_MSG_WAIT)
        {
            LogFlowFunc(("GUEST_MSG_GET\n"));
            rc = clientGetCommand(u32ClientID, callHandle, cParms, paParms);
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
                    LogFlowFunc(("GUEST_CANCEL_PENDING_WAITS\n"));
                    rc = cancelPendingWaits(u32ClientID, VINF_SUCCESS /* Pending result */);
                    break;

                /*
                 * The guest only wants certain messages set by the filter mask(s).
                 * Since VBox 4.3+.
                 */
                case GUEST_MSG_FILTER:
                    LogFlowFunc(("GUEST_MSG_FILTER\n"));
                    rc = clientSetMsgFilter(u32ClientID, callHandle, cParms, paParms);
                    break;

                /*
                 * For all other regular commands we call our hostCallback
                 * function. If the current command does not support notifications,
                 * notifyHost will return VERR_NOT_SUPPORTED.
                 */
                default:
                    rc = hostCallback(callHandle, eFunction, cParms, paParms);
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
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
}

/**
 * Service call handler for the host.
 * @copydoc VBOXHGCMSVCFNTABLE::pfnHostCall
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
                    int rc2 = itClientState->second.CancelWaiting(VINF_SUCCESS /* Pending rc. */);
                    if (RT_FAILURE(rc2))
                        LogFlowFunc(("Cancelling waiting for client ID=%RU32 failed with rc=%Rrc",
                                     itClientState->first, rc2));
                    itClientState++;
                }
                rc = VINF_SUCCESS;
                break;
            }

            default:
                rc = hostProcessCommand(eFunction, cParms, paParms);
                break;
        }
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

int Service::uninit()
{
    Assert(RTListIsEmpty(&mHostCmdList));

    return VINF_SUCCESS;
}

} /* namespace guestControl */

using guestControl::Service;

/**
 * @copydoc VBOXHGCMSVCLOAD
 */
extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("ptable = %p\n", ptable));

    if (!VALID_PTR(ptable))
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        LogFlowFunc(("ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", ptable->cbSize, ptable->u32Version));

        if (   ptable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            || ptable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_VERSION_MISMATCH;
        }
        else
        {
            std::auto_ptr<Service> apService;
            /* No exceptions may propagate outside. */
            try {
                apService = std::auto_ptr<Service>(new Service(ptable->pHelpers));
            } catch (int rcThrown) {
                rc = rcThrown;
            } catch (...) {
                rc = VERR_UNRESOLVED_ERROR;
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * We don't need an additional client data area on the host,
                 * because we're a class which can have members for that :-).
                 */
                ptable->cbClient = 0;

                /* Register functions. */
                ptable->pfnUnload             = Service::svcUnload;
                ptable->pfnConnect            = Service::svcConnect;
                ptable->pfnDisconnect         = Service::svcDisconnect;
                ptable->pfnCall               = Service::svcCall;
                ptable->pfnHostCall           = Service::svcHostCall;
                ptable->pfnSaveState          = NULL;  /* The service is stateless, so the normal */
                ptable->pfnLoadState          = NULL;  /* construction done before restoring suffices */
                ptable->pfnRegisterExtension  = Service::svcRegisterExtension;

                /* Service specific initialization. */
                ptable->pvService = apService.release();
            }
        }
    }

    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

