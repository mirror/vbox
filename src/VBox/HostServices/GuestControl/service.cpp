/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 *           to a service. A client is represented by its HGCM client ID.
 * - Context ID: A (almost) unique ID automatically generated on the host (Main API)
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
 * If a client needs to inform the host that something happend, it can send a
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
 * mainting the relationship between a client and its context IDs.
 */
struct ClientContexts
{
    /** This client ID. */
    uint32_t mClientID;
    /** The list of contexts a client is assigned to. */
    std::list< uint32_t > mContextList;

    /** The normal contructor. */
    ClientContexts(uint32_t aClientID)
                   : mClientID(aClientID) {}
};
/** The client list + iterator type */
typedef std::list< ClientContexts > ClientContextsList;
typedef std::list< ClientContexts >::iterator ClientContextsListIter;
typedef std::list< ClientContexts >::const_iterator ClientContextsListIterConst;

/**
 * Structure for holding a buffered host command.
 */
struct HostCmd
{
    /** The context ID this command belongs to. Will be extracted
      * from the HGCM parameters. */
    uint32_t mContextID;
    /** Dynamic structure for holding the HGCM parms */
    VBOXGUESTCTRPARAMBUFFER mParmBuf;

    /** The standard contructor. */
    HostCmd() : mContextID(0) {}
};
/** The host cmd list + iterator type */
typedef std::list< HostCmd > HostCmdList;
typedef std::list< HostCmd >::iterator HostCmdListIter;
typedef std::list< HostCmd >::const_iterator HostCmdListIterConst;

/**
 * Structure for holding an uncompleted guest call.
 */
struct GuestCall
{
    /** Client ID; a client can have multiple handles! */
    uint32_t mClientID;
    /** The call handle */
    VBOXHGCMCALLHANDLE mHandle;
    /** The call parameters */
    VBOXHGCMSVCPARM *mParms;
    /** Number of parameters */
    uint32_t mNumParms;

    /** The standard contructor. */
    GuestCall() : mClientID(0), mHandle(0), mParms(NULL), mNumParms(0) {}
    /** The normal contructor. */
    GuestCall(uint32_t aClientID, VBOXHGCMCALLHANDLE aHandle,
              VBOXHGCMSVCPARM aParms[], uint32_t cParms)
              : mClientID(aClientID), mHandle(aHandle), mParms(aParms), mNumParms(cParms) {}
};
/** The guest call list type */
typedef std::list< GuestCall > CallList;
typedef std::list< GuestCall >::iterator CallListIter;
typedef std::list< GuestCall >::const_iterator CallListIterConst;

/**
 * Class containing the shared information service functionality.
 */
class Service : public stdx::non_copyable
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
    CallList mClientList;
    /** The host command list. */
    HostCmdList mHostCmds;
    /** Client contexts list. */
    ClientContextsList mClientContextsList;

public:
    explicit Service(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
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
    int sendHostCmdToGuest(HostCmd *pCmd, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int retrieveNextHostCmd(uint32_t u32ClientID, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int cancelPendingWaits(uint32_t u32ClientID);
    int notifyHost(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int processHostCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
              void *pvClient, uint32_t eFunction, uint32_t cParms,
              VBOXHGCMSVCPARM paParms[]);
    int hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit();
};


/** @todo Write some nice doc headers! */
/* Stores a HGCM request in an internal buffer (pEx). Needs to be freed later using execBufferFree(). */
int Service::paramBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pBuf);
    int rc = VINF_SUCCESS;

    /* Paranoia. */
    if (cParms > 256)
        cParms = 256;

    /*
     * Don't verify anything here (yet), because this function only buffers
     * the HGCM data into an internal structure and reaches it back to the guest (client)
     * in an unmodified state.
     */
    if (RT_SUCCESS(rc))
    {
        pBuf->uMsg = uMsg;
        pBuf->uParmCount = cParms;
        pBuf->pParms = (VBOXHGCMSVCPARM*)RTMemAlloc(sizeof(VBOXHGCMSVCPARM) * pBuf->uParmCount);
        if (NULL == pBuf->pParms)
        {
            rc = VERR_NO_MEMORY;
        }
        else
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
                        break;

                    default:
                        break;
                }
                if (RT_FAILURE(rc))
                    break;
            }
        }
    }
    return rc;
}

/* Frees a buffered HGCM request. */
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

/* Assigns data from a buffered HGCM request to the current HGCM request. */
int Service::paramBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pBuf);
    int rc = VINF_SUCCESS;
    if (cParms != pBuf->uParmCount)
    {
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
                    memcpy(paParms[i].u.pointer.addr,
                           pBuf->pParms[i].u.pointer.addr,
                           pBuf->pParms[i].u.pointer.size);
                    break;

                default:
                    break;
            }
        }
    }
    return rc;
}

int Service::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("New client (%ld) connected\n", u32ClientID));
    return VINF_SUCCESS;
}

int Service::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("Client (%ld) disconnected\n", u32ClientID));

    /*
     * Throw out all stale clients.
     */
    int rc = VINF_SUCCESS;

    ClientContextsListIter it = mClientContextsList.begin();
    while (   it != mClientContextsList.end()
           && RT_SUCCESS(rc))
    {
        if (it->mClientID == u32ClientID)
        {
            std::list< uint32_t >::iterator itContext = it->mContextList.begin();
            while (   itContext != it->mContextList.end()
                   && RT_SUCCESS(rc))
            {
                LogFlowFunc(("Notifying host context %u of disconnect ...\n", (*itContext)));

                /*
                 * Notify the host that clients with u32ClientID are no longer
                 * around and need to be cleaned up (canceling waits etc).
                 */
                if (mpfnHostCallback)
                {
                    CALLBACKDATACLIENTDISCONNECTED data;
                    data.hdr.u32Magic = CALLBACKDATAMAGICCLIENTDISCONNECTED;
                    data.hdr.u32ContextID = (*itContext);
                    rc = mpfnHostCallback(mpvHostData, GUEST_DISCONNECTED, (void *)(&data), sizeof(data));
                    if (RT_FAILURE(rc))
                        LogFlowFunc(("Notification of host context %u failed with %Rrc\n", rc));
                }
                itContext++;
            }
            it = mClientContextsList.erase(it);
        }
        else
            it++;
    }
    return rc;
}

int Service::sendHostCmdToGuest(HostCmd *pCmd, VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pCmd);
    int rc;

    /* Sufficient parameter space? */
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
    return rc;
}

/*
 * Either fills in parameters from a pending host command into our guest context or
 * defer the guest call until we have something from the host.
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
        mClientList.push_back(GuestCall(u32ClientID, callHandle, paParms, cParms));
        rc = VINF_HGCM_ASYNC_EXECUTE;
    }
    else
    {
        /*
         * Get the next unassigned host command in the list.
         */
         HostCmd curCmd = mHostCmds.front();
         rc = sendHostCmdToGuest(&curCmd, callHandle, cParms, paParms);
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
    }
    return rc;
}

/*
 * Client asks itself (in another thread) to cancel all pending waits which are blocking the client
 * from shutting down / doing something else.
 */
int Service::cancelPendingWaits(uint32_t u32ClientID)
{
    int rc = VINF_SUCCESS;
    CallListIter it = mClientList.begin();
    while (it != mClientList.end())
    {
        if (it->mClientID == u32ClientID)
        {
            if (it->mNumParms >= 2)
            {
                it->mParms[0].setUInt32(GETHOSTMSG_EXEC_HOST_CANCEL_WAIT); /* Message ID. */
                it->mParms[1].setUInt32(0);                                /* Required parameters for message. */
            }
            if (mpHelpers)
                mpHelpers->pfnCallComplete(it->mHandle, rc);
            it = mClientList.erase(it);
        }
        else
            it++;
    }
    return rc;
}

int Service::notifyHost(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("eFunction=%ld, cParms=%ld, paParms=%p\n",
                 eFunction, cParms, paParms));
    int rc = VINF_SUCCESS;
    if (   eFunction == GUEST_EXEC_SEND_STATUS
        && cParms    == 5)
    {
        CALLBACKDATAEXECSTATUS data;
        data.hdr.u32Magic = CALLBACKDATAMAGICEXECSTATUS;
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
        data.hdr.u32Magic = CALLBACKDATAMAGICEXECOUT;
        paParms[0].getUInt32(&data.hdr.u32ContextID);

        paParms[1].getUInt32(&data.u32PID);
        paParms[2].getUInt32(&data.u32HandleId);
        paParms[3].getUInt32(&data.u32Flags);
        paParms[4].getPointer(&data.pvData, &data.cbData);

        if (mpfnHostCallback)
            rc = mpfnHostCallback(mpvHostData, eFunction,
                                  (void *)(&data), sizeof(data));
    }
    else
        rc = VERR_NOT_SUPPORTED;
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

int Service::processHostCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    HostCmd newCmd;
    rc = paramBufferAllocate(&newCmd.mParmBuf, eFunction, cParms, paParms);
    if (   RT_SUCCESS(rc)
        && newCmd.mParmBuf.uParmCount > 0)
    {
        /*
         * Assume that the context ID *always* is the first parameter,
         * assign the context ID to the command.
         */
        newCmd.mParmBuf.pParms[0].getUInt32(&newCmd.mContextID);
        Assert(newCmd.mContextID > 0);
    }

    bool fProcessed = false;
    if (RT_SUCCESS(rc))
    {
        /* Can we wake up a waiting client on guest? */
        if (!mClientList.empty())
        {
            GuestCall guest = mClientList.front();
            rc = sendHostCmdToGuest(&newCmd,
                                    guest.mHandle, guest.mNumParms, guest.mParms);

            /* In any case the client did something, so wake up and remove from list. */
            AssertPtr(mpHelpers);
            mpHelpers->pfnCallComplete(guest.mHandle, rc);
            mClientList.pop_front();

            /* If we got VERR_TOO_MUCH_DATA we buffer the host command in the next block
             * and return success to the host. */
            if (rc == VERR_TOO_MUCH_DATA)
            {
                rc = VINF_SUCCESS;
            }
            else /* If command was understood by the client, free and remove from host commands list. */
            {
                paramBufferFree(&newCmd.mParmBuf);
                fProcessed = true;
            }
        }

        /* If not processed, buffer it ... */
        if (!fProcessed)
        {
            mHostCmds.push_back(newCmd);
#if 0
            /* Limit list size by deleting oldest element. */
            if (mHostCmds.size() > 256) /** @todo Use a define! */
                mHostCmds.pop_front();
#endif
        }
    }
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
    LogFlowFunc(("u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n",
                 u32ClientID, eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /* The guest asks the host for the next messsage to process. */
            case GUEST_GET_HOST_MSG:
                LogFlowFunc(("GUEST_GET_HOST_MSG\n"));
                rc = retrieveNextHostCmd(u32ClientID, callHandle, cParms, paParms);
                break;

            case GUEST_CANCEL_PENDING_WAITS:
                LogFlowFunc(("GUEST_CANCEL_PENDING_WAITS\n"));
                rc = cancelPendingWaits(u32ClientID);
                break;

            /* The guest notifies the host that some output at stdout/stderr is available. */
            case GUEST_EXEC_SEND_OUTPUT:
                LogFlowFunc(("GUEST_EXEC_SEND_OUTPUT\n"));
                rc = notifyHost(eFunction, cParms, paParms);
                break;

            /* The guest notifies the host of the current client status. */
            case GUEST_EXEC_SEND_STATUS:
                LogFlowFunc(("SEND_STATUS\n"));
                rc = notifyHost(eFunction, cParms, paParms);
                break;

            default:
                rc = VERR_NOT_SUPPORTED;
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
    int rc = VINF_SUCCESS;
    LogFlowFunc(("fn = %d, cParms = %d, pparms = %d\n",
                 eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /* The host wants to execute something. */
            case HOST_EXEC_CMD:
                LogFlowFunc(("HOST_EXEC_CMD\n"));
                rc = processHostCmd(eFunction, cParms, paParms);
                break;

            /* The host wants to send something to the guest's stdin pipe. */
            case HOST_EXEC_SET_INPUT:
                LogFlowFunc(("HOST_EXEC_SET_INPUT\n"));
                break;

            case HOST_EXEC_GET_OUTPUT:
                LogFlowFunc(("HOST_EXEC_GET_OUTPUT\n"));
                rc = processHostCmd(eFunction, cParms, paParms);
                break;

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
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
    /* Free allocated buffered host commands. */
    HostCmdListIter it;
    for (it = mHostCmds.begin(); it != mHostCmds.end(); it++)
        paramBufferFree(&it->mParmBuf);
    mHostCmds.clear();

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
            /* No exceptions may propogate outside. */
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

