/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/** @page pg_svc_guest_control   Guest Control HGCM Service
 *
 * @todo Write up some nice text here.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/HostServices/GuestControlSvc.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/cpp/utils.h>
#include <iprt/critsect.h>
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

struct Client
{
    VBOXGUESTCTRPARAMBUFFER parmBuf;
    VBOXHGCMCALLHANDLE callHandle;
    uint32_t uClientID;
};

struct HostCmd
{
    VBOXGUESTCTRPARAMBUFFER parmBuf;
    uint32_t uAssignedToClientID;
};

/** The client list list type */
typedef std::list <Client> ClientList;
/** The host cmd list type */
typedef std::list <HostCmd> HostCmdList;

/**
 * Class containing the shared information service functionality.
 */
class Service : public stdx::non_copyable
{
private:
    /** Type definition for use in callback functions */
    typedef Service SELF;
    /** HGCM helper functions. */
    PVBOXHGCMSVCHELPERS mpHelpers;
    /** @todo we should have classes for thread and request handler thread */
    /** Queue of outstanding property change notifications */
    RTREQQUEUE *mReqQueue;
    /** Request that we've left pending in a call to flushNotifications. */
    PRTREQ mPendingDummyReq;
    /** Thread for processing the request queue */
    RTTHREAD mReqThread;
    /** Tell the thread that it should exit */
    bool volatile mfExitThread;
    /** Callback function supplied by the host for notification of updates
     * to properties */
    PFNHGCMSVCEXT mpfnHostCallback;
    /** User data pointer to be supplied to the host callback function */
    void *mpvHostData;
    /** The client list */
    ClientList mClients;
    /** The host command list */
    HostCmdList mHostCmds;
    RTCRITSECT critsect;

public:
    explicit Service(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mPendingDummyReq(NULL)
        , mfExitThread(false)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
    {
        int rc = RTReqCreateQueue(&mReqQueue);
#ifndef VBOX_GUEST_CTRL_TEST_NOTHREAD
        if (RT_SUCCESS(rc))
            rc = RTThreadCreate(&mReqThread, reqThreadFn, this, 0,
                                RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                                "GuestCtrlReq");
#endif
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&critsect);

        if (RT_FAILURE(rc))
            throw rc;
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
    int execBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void execBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf);
    int execBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int prepareExecute(uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    static DECLCALLBACK(int) reqThreadFn(RTTHREAD ThreadSelf, void *pvUser);
    int clientConnect(uint32_t u32ClientID, void *pvClient);
    int clientDisconnect(uint32_t u32ClientID, void *pvClient);
    void call(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
              void *pvClient, uint32_t eFunction, uint32_t cParms,
              VBOXHGCMSVCPARM paParms[]);
    int hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit();
};


/**
 * Thread function for processing the request queue
 * @copydoc FNRTTHREAD
 */
/* static */
DECLCALLBACK(int) Service::reqThreadFn(RTTHREAD ThreadSelf, void *pvUser)
{
    SELF *pSelf = reinterpret_cast<SELF *>(pvUser);
    while (!pSelf->mfExitThread)
        RTReqProcess(pSelf->mReqQueue, RT_INDEFINITE_WAIT);
    return VINF_SUCCESS;
}

/** @todo Write some nice doc headers! */
/* Stores a HGCM request in an internal buffer (pEx). Needs to be freed later using execBufferFree(). */
int Service::execBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RTCritSectEnter(&critsect);

    AssertPtr(pBuf);
    int rc = VINF_SUCCESS;

    /*
     * Don't verify anything here (yet), because this function only buffers
     * the HGCM data into an internal structure and reaches it back to the guest (client)
     * in an unmodified state.
     */
    if (RT_SUCCESS(rc))
    {
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
    RTCritSectLeave(&critsect);
    return rc;
}

/* Frees a buffered HGCM request. */
void Service::execBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf)
{
    RTCritSectEnter(&critsect);
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
    RTCritSectLeave(&critsect);
}

/* Assigns data from a buffered HGCM request to the current HGCM request. */
int Service::execBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RTCritSectEnter(&critsect);

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
    RTCritSectLeave(&critsect);
    return rc;
}

int Service::clientConnect(uint32_t u32ClientID, void *pvClient)
{
ASMBreakpoint();

    bool bFound = false;
    for (ClientList::const_iterator it = mClients.begin();
         !bFound && it != mClients.end(); ++it)
    {
        if (it->uClientID == u32ClientID)
            bFound = true;
    }

    if (!bFound)
    {
        Client newClient;

        /** @todo Use a constructor here! */
        newClient.uClientID = u32ClientID;
        newClient.parmBuf.uParmCount = 0;
        newClient.parmBuf.pParms = NULL;

        mClients.push_back(newClient);
        LogFlowFunc(("New client = %ld connected\n", u32ClientID));
    }
    else
    {
        LogFlowFunc(("Exising client (%ld) connected another instance\n", u32ClientID));
    }
    return VINF_SUCCESS;
}

int Service::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
ASMBreakpoint();

    bool bFound = false;
    for (ClientList::iterator it = mClients.begin();
         !bFound && it != mClients.end(); ++it)
    {
        if (it->uClientID == u32ClientID)
        {
            mClients.erase(it);
            bFound = true;
        }
    }
    Assert(bFound);

    LogFlowFunc(("Client (%ld) disconnected\n", u32ClientID));
    return VINF_SUCCESS;
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

    ASMBreakpoint();

    try
    {
        switch (eFunction)
        {
            /* The guest asks the host for the next messsage to process. */
            case GUEST_GET_HOST_MSG:
                LogFlowFunc(("GUEST_GET_HOST_MSG\n"));                

/** @todo !!!!! LOCKING !!!!!!!!! */

                if (cParms < 2)
                {
                    LogFlowFunc(("Parameter buffer is too small!\n"));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {      
                    /* 
                     * If host command list is empty (nothing to do right now) just
                     * defer the call until we got something to do (makes the client
                     * wait, depending on the flags set).
                     */
                    if (mHostCmds.empty())
                        rc = VINF_HGCM_ASYNC_EXECUTE;
    
                    if (   RT_SUCCESS(rc)
                        && rc != VINF_HGCM_ASYNC_EXECUTE)
                    {
                        /*
                         * Get the next unassigned host command in the list.
                         */
                        bool bFound = false;
                        HostCmdList::iterator it;
                        for (it = mHostCmds.begin();
                             !bFound && it != mHostCmds.end(); ++it)
                        {
                            if (it->uAssignedToClientID == 0)
                            {
                                bFound = true;
                                break;
                            }
                        }
                        if (!bFound) /* No new command found, defer ... */
                            rc = VINF_HGCM_ASYNC_EXECUTE;

                        /*
                         * Okay we got a host command which is unassigned at the moment.                    
                         */
                        if (   RT_SUCCESS(rc)
                            && rc != VINF_HGCM_ASYNC_EXECUTE)
                        {
                            /* 
                             * Do *not* remove the command from host cmds list here yet, because
                             * the client could fail in retrieving the GUEST_GET_HOST_MSG_DATA message
                             * below. Then we just could repeat this one here.
                             */
                            uint32_t uCmd = 1; /** @todo HARDCODED FOR EXEC! */
                            uint32_t uParmCount = it->parmBuf.uParmCount;
                            if (uParmCount)
                                it->parmBuf.pParms[0].getUInt32(&uCmd);
                            paParms[0].setUInt32(uCmd); /* msg id */
                            paParms[1].setUInt32(uParmCount); /* parms count */
    
                            /* Assign this command to the specific client ID. */
                            it->uAssignedToClientID = u32ClientID;
                        }
                    }
                }
                break;

            case GUEST_GET_HOST_MSG_DATA:
                {
                    LogFlowFunc(("GUEST_GET_HOST_MSG_DATA\n"));

/** @todo !!!!! LOCKING !!!!!!!!! */

                    /*
                     * Get host command assigned to incoming client ID.                           
                     */
                    bool bFound = false;
                    HostCmdList::iterator it;
                    for (it = mHostCmds.begin();
                         !bFound && it != mHostCmds.end(); ++it)
                    {
                        if (it->uAssignedToClientID == u32ClientID)    
                        {
                            bFound = true;
                            break;
                        }
                    }
                    Assert(bFound);

                    /*
                     * Assign buffered data to client HGCM parms.
                     */
                    rc = execBufferAssign(&it->parmBuf, cParms, paParms);
        
                    /*
                     * Finally remove the command from the list. 
                     */
                    execBufferFree(&it->parmBuf);
                    mHostCmds.erase(it);
                }
                break;

            /* The guest notifies the host that some output at stdout is available. */
            case GUEST_EXEC_SEND_STDOUT:
                LogFlowFunc(("GUEST_EXEC_SEND_STDOUT\n"));
                break;

            /* The guest notifies the host that some output at stderr is available. */
            case GUEST_EXEC_SEND_STDERR:
                LogFlowFunc(("GUEST_EXEC_SEND_STDERR\n"));
                break;

            /* The guest notifies the host of the current client status. */
            case GUEST_EXEC_SEND_STATUS:
                LogFlowFunc(("SEND_STATUS\n"));
                break;

            default:
                rc = VERR_NOT_IMPLEMENTED;
        }
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowFunc(("rc = %Rrc\n", rc));
    if (rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        mpHelpers->pfnCallComplete(callHandle, rc);
    }
}

/**
 * Service call handler for the host.
 * @copydoc VBOXHGCMSVCFNTABLE::pfnHostCall
 * @thread  hgcm
 */
int Service::hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    ASMBreakpoint();

    LogFlowFunc(("fn = %d, cParms = %d, pparms = %d\n",
                 eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /* The host wants to execute something. */
            case HOST_EXEC_CMD:
                LogFlowFunc(("HOST_EXEC_CMD\n"));

/** @todo !!!!! LOCKING !!!!!!!!! */

                HostCmd newCmd;
                execBufferAllocate(&newCmd.parmBuf, cParms, paParms);
                newCmd.uAssignedToClientID = 0;
                mHostCmds.push_back(newCmd);
                break;

            /* The host wants to send something to the guest's stdin pipe. */
            case HOST_EXEC_SEND_STDIN:
                LogFlowFunc(("HOST_EXEC_SEND_STDIN\n"));
                break;

            case HOST_EXEC_GET_STATUS:
                LogFlowFunc(("HOST_EXEC_GET_STATUS\n"));
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
    int rc = VINF_SUCCESS;
    return rc;
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

