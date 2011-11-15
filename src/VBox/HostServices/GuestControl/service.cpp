/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/HostServices/GuestControlSvc.h>

#include <VBox/log.h>
#include <iprt/asm.h> /* For ASMBreakpoint(). */
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/cpp/utils.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <memory>  /* for auto_ptr */
#include <string>
#include <list>

#include "gctrl.h"

namespace guestControl {

/**
 * Structure for holding all clients with their
 * generated host contexts. This is necessary for
 * maintaining the relationship between a client and its context IDs.
 */
struct ClientContexts
{
    /** This client ID. */
    uint32_t mClientID;
    /** The list of contexts a client is assigned to. */
    std::list< uint32_t > mContextList;

    /** The normal constructor. */
    ClientContexts(uint32_t aClientID)
                   : mClientID(aClientID) {}
};
/** The client list + iterator type */
typedef std::list< ClientContexts > ClientContextsList;
typedef std::list< ClientContexts >::iterator ClientContextsListIter;
typedef std::list< ClientContexts >::const_iterator ClientContextsListIterConst;

/**
 * Structure for holding an uncompleted guest call.
 */
struct ClientWaiter
{
    /** Client ID; a client can have multiple handles! */
    uint32_t mClientID;
    /** The call handle */
    VBOXHGCMCALLHANDLE mHandle;
    /** The call parameters */
    VBOXHGCMSVCPARM *mParms;
    /** Number of parameters */
    uint32_t mNumParms;

    /** The standard constructor. */
    ClientWaiter() : mClientID(0), mHandle(0), mParms(NULL), mNumParms(0) {}
    /** The normal constructor. */
    ClientWaiter(uint32_t aClientID, VBOXHGCMCALLHANDLE aHandle,
              VBOXHGCMSVCPARM aParms[], uint32_t cParms)
              : mClientID(aClientID), mHandle(aHandle), mParms(aParms), mNumParms(cParms) {}
};
/** The guest call list type */
typedef std::list< ClientWaiter > ClientWaiterList;
typedef std::list< ClientWaiter >::iterator CallListIter;
typedef std::list< ClientWaiter >::const_iterator CallListIterConst;

/**
 * Structure for holding a buffered host command.
 */
struct HostCmd
{
    /** The context ID this command belongs to. Will be extracted
      * from the HGCM parameters. */
    uint32_t mContextID;
    /** How many times the host service has tried to deliver this
     *  command to the guest. */
    uint32_t mTries;
    /** Dynamic structure for holding the HGCM parms */
    VBOXGUESTCTRPARAMBUFFER mParmBuf;

    /** The standard constructor. */
    HostCmd() : mContextID(0), mTries(0) {}
};
/** The host cmd list + iterator type */
typedef std::list< HostCmd > HostCmdList;
typedef std::list< HostCmd >::iterator HostCmdListIter;
typedef std::list< HostCmd >::const_iterator HostCmdListIterConst;

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
    /*
     * Callback function supplied by the host for notification of updates
     * to properties.
     */
    PFNHGCMSVCEXT mpfnHostCallback;
    /** User data pointer to be supplied to the host callback function. */
    void *mpvHostData;
    /** The deferred calls list. */
    ClientWaiterList mClientWaiterList;
    /** The host command list. */
    HostCmdList mHostCmds;
    /** Client contexts list. */
    ClientContextsList mClientContextsList;
    /** Number of connected clients. */
    uint32_t mNumClients;
public:
    explicit Service(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
        , mNumClients(0)
    {
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
        int rc = pSelf->clientConnect(u32ClientID, pvClient);
        LogFlowFunc (("rc=%Rrc\n", rc));
        return rc;
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
        int rc = pSelf->clientDisconnect(u32ClientID, pvClient);
        LogFlowFunc (("rc=%Rrc\n", rc));
        return rc;
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
        LogFlowFunc (("pvService=%p, callHandle=%p, u32ClientID=%u, pvClient=%p, u32Function=%u, cParms=%u, paParms=%p\n", pvService, callHandle, u32ClientID, pvClient, u32Function, cParms, paParms));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        pSelf->call(callHandle, u32ClientID, pvClient, u32Function, cParms, paParms);
        LogFlowFunc (("returning\n"));
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
        int rc = pSelf->hostCall(u32Function, cParms, paParms);
        LogFlowFunc (("rc=%Rrc\n", rc));
        return rc;
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
    int paramBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void paramBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf);
    int paramBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int prepareExecute(uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientConnect(uint32_t u32ClientID, void *pvClient);
    int clientDisconnect(uint32_t u32ClientID, void *pvClient);
    int assignHostCmdToGuest(HostCmd *pCmd, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int retrieveNextHostCmd(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int cancelHostCmd(uint32_t u32ContextID);
    int cancelPendingWaits(uint32_t u32ClientID);
    int notifyHost(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int processHostCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
              void *pvClient, uint32_t eFunction, uint32_t cParms,
              VBOXHGCMSVCPARM paParms[]);
    int hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit();
};


/**
 * Stores a HGCM request in an internal buffer. Needs to be free'd using paramBufferFree().
 *
 * @return  IPRT status code.
 * @param   pBuf                    Buffer to store the HGCM request into.
 * @param   uMsg                    Message type.
 * @param   cParms                  Number of parameters of HGCM request.
 * @param   paParms                 Array of parameters of HGCM request.
 */
int Service::paramBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    if (cParms)
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
    pBuf->uMsg = uMsg;
    pBuf->uParmCount = cParms;
    if (pBuf->uParmCount)
    {
        pBuf->pParms = (VBOXHGCMSVCPARM*)RTMemAlloc(sizeof(VBOXHGCMSVCPARM) * pBuf->uParmCount);
        if (NULL == pBuf->pParms)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < pBuf->uParmCount; i++)
        {
            pBuf->pParms[i].type = paParms[i].type;
            switch (paParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    pBuf->pParms[i].u.uint32 = paParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    /* Not supported yet. */
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                    pBuf->pParms[i].u.pointer.size = paParms[i].u.pointer.size;
                    if (pBuf->pParms[i].u.pointer.size > 0)
                    {
                        pBuf->pParms[i].u.pointer.addr = RTMemAlloc(pBuf->pParms[i].u.pointer.size);
                        if (NULL == pBuf->pParms[i].u.pointer.addr)
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }
                        else
                            memcpy(pBuf->pParms[i].u.pointer.addr,
                                   paParms[i].u.pointer.addr,
                                   pBuf->pParms[i].u.pointer.size);
                    }
                    else
                    {
                        /* Size is 0 -- make sure we don't have any pointer. */
                        pBuf->pParms[i].u.pointer.addr = NULL;
                    }
                    break;

                default:
                    break;
            }
            if (RT_FAILURE(rc))
                break;
        }
    }
    return rc;
}

/**
 * Frees a buffered HGCM request.
 *
 * @return  IPRT status code.
 * @param   pBuf                    Parameter buffer to free.
 */
void Service::paramBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf)
{
    AssertPtr(pBuf);
    for (uint32_t i = 0; i < pBuf->uParmCount; i++)
    {
        switch (pBuf->pParms[i].type)
        {
            case VBOX_HGCM_SVC_PARM_PTR:
                if (pBuf->pParms[i].u.pointer.size > 0)
                    RTMemFree(pBuf->pParms[i].u.pointer.addr);
                break;
        }
    }
    if (pBuf->uParmCount)
    {
        RTMemFree(pBuf->pParms);
        pBuf->uParmCount = 0;
    }
}

/**
 * Assigns data from a buffered HGCM request to the current HGCM request.
 *
 * @return  IPRT status code.
 * @param   pBuf                    Parameter buffer to assign.
 * @param   cParms                  Number of parameters the HGCM request can handle.
 * @param   paParms                 Array of parameters of HGCM request to fill the data into.
 */
int Service::paramBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pBuf);
    int rc = VINF_SUCCESS;
    if (cParms != pBuf->uParmCount)
    {
        LogFlowFunc(("Parameter count does not match: %u (host) vs. %u (guest)\n",
                     pBuf->uParmCount, cParms));
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        /** @todo Add check to verify if the HGCM request is the same *type* as the buffered one! */
        for (uint32_t i = 0; i < pBuf->uParmCount; i++)
        {
            paParms[i].type = pBuf->pParms[i].type;
            switch (paParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    paParms[i].u.uint32 = pBuf->pParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    /* Not supported yet. */
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                    if (paParms[i].u.pointer.size >= pBuf->pParms[i].u.pointer.size)
                    {
                        /* Only copy buffer if there actually is something to copy. */
                        if (pBuf->pParms[i].u.pointer.size)
                        {
                            AssertPtr(pBuf->pParms[i].u.pointer.addr);
                            memcpy(paParms[i].u.pointer.addr,
                                   pBuf->pParms[i].u.pointer.addr,
                                   pBuf->pParms[i].u.pointer.size);
                        }
                    }
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                    break;

                default:
                    break;
            }
        }
    }
    return rc;
}

/**
 * Handles a client which just connected.
 *
 * @return  IPRT status code.
 * @param   u32ClientID
 * @param   pvClient
 */
int Service::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("New client (%ld) connected\n", u32ClientID));
    if (mNumClients < UINT32_MAX)
        mNumClients++;
    else
        AssertMsgFailed(("Max. number of clients reached\n"));
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
    LogFlowFunc(("Client (ID=%u, %u clients total) disconnected\n",
                 u32ClientID, mNumClients));
    Assert(mNumClients > 0);
    mNumClients--;

    /* If this was the last connected (guest) client we need to
     * unblock all eventually queued up (waiting) host calls. */
    bool fAllClientsDisconnected = mNumClients == 0;
    if (fAllClientsDisconnected)
        LogFlowFunc(("No connected clients left, notifying all queued up callbacks\n"));

    /*
     * Throw out all stale clients.
     */
    int rc = VINF_SUCCESS;

    CallListIter itCall = mClientWaiterList.begin();
    while (itCall != mClientWaiterList.end())
    {
        if (itCall->mClientID == u32ClientID)
        {
            itCall = mClientWaiterList.erase(itCall);
        }
        else
            itCall++;
    }

    ClientContextsListIter it = mClientContextsList.begin();
    while (   it != mClientContextsList.end()
           && RT_SUCCESS(rc))
    {
        /*
         * Unblock/call back all queued items of the specified client
         * or for all items in case there is no waiting client around
         * anymore.
         */
        if (   it->mClientID == u32ClientID
            || fAllClientsDisconnected)
        {
            std::list< uint32_t >::iterator itContext = it->mContextList.begin();
            while (itContext != it->mContextList.end())
            {
                uint32_t uContextID = (*itContext);

                /*
                 * Notify the host that clients with u32ClientID are no longer
                 * around and need to be cleaned up (canceling waits etc).
                 */
                LogFlowFunc(("Notifying CID=%u of disconnect ...\n", uContextID));
                rc = cancelHostCmd(uContextID);
                if (RT_FAILURE(rc))
                {
                    LogFlowFunc(("Cancelling of CID=%u failed with rc=%Rrc\n",
                                 uContextID, rc));
                    /* Keep going. */
                }

                itContext++;
            }
            it = mClientContextsList.erase(it);
        }
        else
            it++;
    }

    if (fAllClientsDisconnected)
    {
        /*
         * If all clients disconnected we also need to make sure that all buffered
         * host commands need to be notified, because Main is waiting a notification
         * via a (multi stage) progress object.
         */
        HostCmdListIter it;
        for (it = mHostCmds.begin(); it != mHostCmds.end(); it++)
        {
            rc = cancelHostCmd(it->mContextID);
            if (RT_FAILURE(rc))
            {
                LogFlowFunc(("Cancelling of buffered CID=%u failed with rc=%Rrc\n",
                             it->mContextID, rc));
                /* Keep going. */
            }

            paramBufferFree(&it->mParmBuf);
        }

        mHostCmds.clear();
    }

    return rc;
}

/**
 * Assigns a specified host command to a client.
 *
 * @return  IPRT status code.
 * @param   pCmd                    Host comamnd to send.
 * @param   callHandle              Call handle of the client to send the command to.
 * @param   cParms                  Number of parameters.
 * @param   paParms                 Array of parameters.
 */
int Service::assignHostCmdToGuest(HostCmd *pCmd, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtrReturn(pCmd, VERR_INVALID_POINTER);
    int rc;

    /* Does the current host command need more parameter space which
     * the client does not provide yet? */
    if (pCmd->mParmBuf.uParmCount > cParms)
    {
        paParms[0].setUInt32(pCmd->mParmBuf.uMsg);       /* Message ID */
        paParms[1].setUInt32(pCmd->mParmBuf.uParmCount); /* Required parameters for message */

        /*
        * So this call apparently failed because the guest wanted to peek
        * how much parameters it has to supply in order to successfully retrieve
        * this command. Let's tell him so!
        */
        rc = VERR_TOO_MUCH_DATA;
    }
    else
    {
        rc = paramBufferAssign(&pCmd->mParmBuf, cParms, paParms);
    }

    LogFlowFunc(("Returned with rc=%Rrc\n", rc));
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
int Service::retrieveNextHostCmd(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle,
                                 uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    /*
     * Lookup client in our list so that we can assign the context ID of
     * a command to that client.
     */
    std::list< ClientContexts >::reverse_iterator it = mClientContextsList.rbegin();
    while (it != mClientContextsList.rend())
    {
        if (it->mClientID == u32ClientID)
            break;
        it++;
    }

    /* Not found? Add client to list. */
    if (it == mClientContextsList.rend())
    {
        mClientContextsList.push_back(ClientContexts(u32ClientID));
        it = mClientContextsList.rbegin();
    }
    Assert(it != mClientContextsList.rend());

    /*
     * If host command list is empty (nothing to do right now) just
     * defer the call until we got something to do (makes the client
     * wait, depending on the flags set).
     */
    if (mHostCmds.empty()) /* If command list is empty, defer ... */
    {
        mClientWaiterList.push_back(ClientWaiter(u32ClientID, callHandle, paParms, cParms));
        rc = VINF_HGCM_ASYNC_EXECUTE;
    }
    else
    {
        /*
         * Get the next unassigned host command in the list.
         */
         HostCmd curCmd = mHostCmds.front();
         rc = assignHostCmdToGuest(&curCmd, callHandle, cParms, paParms);
         if (RT_SUCCESS(rc))
         {
             /* Remember which client processes which context (for
              * later reference & cleanup). */
             Assert(curCmd.mContextID > 0);
             /// @todo r=bird: check if already in the list.
             it->mContextList.push_back(curCmd.mContextID);

             /* Only if the guest really got and understood the message remove it from the list. */
             paramBufferFree(&curCmd.mParmBuf);
             mHostCmds.pop_front();
         }
         else if (rc == VERR_BUFFER_OVERFLOW)
         {
             /* If the client understood the message but supplied too little buffer space
              * don't send this message again and drop it after 3 unsuccessful attempts.
              * The host then should take care of next actions (maybe retry it with a smaller buffer). */
             if (++curCmd.mTries >= 3)
             {
                 paramBufferFree(&curCmd.mParmBuf);
                 mHostCmds.pop_front();
             }
         }
         else
         {
             /* Client did not understand the message or something else weird happened. Try again one
              * more time and drop it if it didn't get handled then. */
             if (++curCmd.mTries > 1)
             {
                 paramBufferFree(&curCmd.mParmBuf);
                 mHostCmds.pop_front();
             }
         }
    }
    return rc;
}

/**
 * Cancels a buffered host command to unblock waits on Main side
 * (via (multi stage) progress objects.
 *
 * @return  IPRT status code.
 * @param   u32ContextID                Context ID of host command to cancel.
 */
int Service::cancelHostCmd(uint32_t u32ContextID)
{
    AssertReturn(u32ContextID, VERR_INVALID_PARAMETER);
    Assert(mpfnHostCallback);

    LogFlowFunc(("Cancelling CID=%u ...\n", u32ContextID));

    CALLBACKDATACLIENTDISCONNECTED data;
    data.hdr.u32Magic = CALLBACKDATAMAGIC_CLIENT_DISCONNECTED;
    data.hdr.u32ContextID = u32ContextID;

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
 */
int Service::cancelPendingWaits(uint32_t u32ClientID)
{
    int rc = VINF_SUCCESS;
    CallListIter it = mClientWaiterList.begin();
    while (it != mClientWaiterList.end())
    {
        if (it->mClientID == u32ClientID)
        {
            if (it->mNumParms >= 2)
            {
                it->mParms[0].setUInt32(HOST_CANCEL_PENDING_WAITS); /* Message ID. */
                it->mParms[1].setUInt32(0);                         /* Required parameters for message. */
            }
            if (mpHelpers)
                mpHelpers->pfnCallComplete(it->mHandle, rc);
            it = mClientWaiterList.erase(it);
        }
        else
            it++;
    }
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
int Service::notifyHost(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("eFunction=%ld, cParms=%ld, paParms=%p\n",
                 eFunction, cParms, paParms));
    int rc = VINF_SUCCESS;
    if (   eFunction == GUEST_EXEC_SEND_STATUS
        && cParms    == 5)
    {
        CALLBACKDATAEXECSTATUS data;
        data.hdr.u32Magic = CALLBACKDATAMAGIC_EXEC_STATUS;
        paParms[0].getUInt32(&data.hdr.u32ContextID);

        paParms[1].getUInt32(&data.u32PID);
        paParms[2].getUInt32(&data.u32Status);
        paParms[3].getUInt32(&data.u32Flags);
        paParms[4].getPointer(&data.pvData, &data.cbData);

        if (mpfnHostCallback)
            rc = mpfnHostCallback(mpvHostData, eFunction,
                                  (void *)(&data), sizeof(data));
    }
    else if (   eFunction == GUEST_EXEC_SEND_OUTPUT
             && cParms    == 5)
    {
        CALLBACKDATAEXECOUT data;
        data.hdr.u32Magic = CALLBACKDATAMAGIC_EXEC_OUT;
        paParms[0].getUInt32(&data.hdr.u32ContextID);

        paParms[1].getUInt32(&data.u32PID);
        paParms[2].getUInt32(&data.u32HandleId);
        paParms[3].getUInt32(&data.u32Flags);
        paParms[4].getPointer(&data.pvData, &data.cbData);

        if (mpfnHostCallback)
            rc = mpfnHostCallback(mpvHostData, eFunction,
                                  (void *)(&data), sizeof(data));
    }
    else if (   eFunction == GUEST_EXEC_SEND_INPUT_STATUS
             && cParms    == 5)
    {
        CALLBACKDATAEXECINSTATUS data;
        data.hdr.u32Magic = CALLBACKDATAMAGIC_EXEC_IN_STATUS;
        paParms[0].getUInt32(&data.hdr.u32ContextID);

        paParms[1].getUInt32(&data.u32PID);
        paParms[2].getUInt32(&data.u32Status);
        paParms[3].getUInt32(&data.u32Flags);
        paParms[4].getUInt32(&data.cbProcessed);

        if (mpfnHostCallback)
            rc = mpfnHostCallback(mpvHostData, eFunction,
                                  (void *)(&data), sizeof(data));
    }
    else
        rc = VERR_NOT_SUPPORTED;
    LogFlowFunc(("returning %Rrc\n", rc));
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
int Service::processHostCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * If no client is connected at all we don't buffer any host commands
     * and immediately return an error to the host. This avoids the host
     * waiting for a response from the guest side in case VBoxService on
     * the guest is not running/system is messed up somehow.
     */
    if (mNumClients == 0)
        return VERR_NOT_FOUND;
    HostCmd newCmd;
    int rc = paramBufferAllocate(&newCmd.mParmBuf, eFunction, cParms, paParms);
    if (   RT_SUCCESS(rc)
        && cParms) /* Make sure we at least get one parameter (that is, the context ID). */
    {
        /*
         * Assume that the context ID *always* is the first parameter,
         * assign the context ID to the command.
         */
        newCmd.mParmBuf.pParms[0].getUInt32(&newCmd.mContextID);
        Assert(newCmd.mContextID > 0);
    }
    else if (!cParms)
        rc = VERR_INVALID_PARAMETER;

    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("Handling host command CID = %u\n",
                     newCmd.mContextID));

        bool fProcessed = false;

        /* Can we wake up a waiting client on guest? */
        if (!mClientWaiterList.empty())
        {
            ClientWaiter guest = mClientWaiterList.front();
            rc = assignHostCmdToGuest(&newCmd,
                                      guest.mHandle, guest.mNumParms, guest.mParms);

            /* In any case the client did something, so wake up and remove from list. */
            AssertPtr(mpHelpers);
            mpHelpers->pfnCallComplete(guest.mHandle, rc);
            mClientWaiterList.pop_front();

            /*
             * If we got back an error (like VERR_TOO_MUCH_DATA or VERR_BUFFER_OVERFLOW)
             * we buffer the host command in the next block and return success to the host.
             */
            if (RT_FAILURE(rc))
            {
                rc = VINF_SUCCESS;
            }
            else /* If command was understood by the client, free and remove from host commands list. */
            {
                LogFlowFunc(("Host command CID = %u processed with rc=%Rrc\n",
                             newCmd.mContextID, rc));

                paramBufferFree(&newCmd.mParmBuf);
            }
        }

        if (!fProcessed)
        {
            LogFlowFunc(("Buffering host command CID = %u (rc=%Rrc)\n",
                         newCmd.mContextID, rc));

            mHostCmds.push_back(newCmd);
        }
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
    LogFlowFunc(("u32ClientID = %u, fn = %u, cParms = %u, paParms = 0x%p\n",
                 u32ClientID, eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /*
             * The guest asks the host for the next message to process.
             */
            case GUEST_GET_HOST_MSG:
                LogFlowFunc(("GUEST_GET_HOST_MSG\n"));
                rc = retrieveNextHostCmd(u32ClientID, callHandle, cParms, paParms);
                break;

            /*
             * The guest wants to shut down and asks us (this service) to cancel
             * all blocking pending waits (VINF_HGCM_ASYNC_EXECUTE) so that the
             * guest can gracefully shut down.
             */
            case GUEST_CANCEL_PENDING_WAITS:
                LogFlowFunc(("GUEST_CANCEL_PENDING_WAITS\n"));
                rc = cancelPendingWaits(u32ClientID);
                break;

            /*
             * For all other regular commands we call our notifyHost
             * function. If the current command does not support notifications
             * notifyHost will return VERR_NOT_SUPPORTED.
             */
            default:
                rc = notifyHost(eFunction, cParms, paParms);
                break;
        }
        if (rc != VINF_HGCM_ASYNC_EXECUTE)
        {
            /* Tell the client that the call is complete (unblocks waiting). */
            AssertPtr(mpHelpers);
            mpHelpers->pfnCallComplete(callHandle, rc);
        }
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowFunc(("rc = %Rrc\n", rc));
}

/**
 * Service call handler for the host.
 * @copydoc VBOXHGCMSVCFNTABLE::pfnHostCall
 * @thread  hgcm
 */
int Service::hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VERR_NOT_SUPPORTED;
    LogFlowFunc(("fn = %u, cParms = %u, paParms = 0x%p\n",
                 eFunction, cParms, paParms));
    try
    {
        rc = processHostCmd(eFunction, cParms, paParms);
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

int Service::uninit()
{
    Assert(mHostCmds.empty());

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

