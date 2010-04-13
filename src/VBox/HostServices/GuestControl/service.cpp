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
 * Structure for holding a buffered host command
 */
struct HostCmd
{
    /** Dynamic structure for holding the HGCM parms */
    VBOXGUESTCTRPARAMBUFFER parmBuf;
};
/** The host cmd list type */
typedef std::list <HostCmd> HostCmdList;

/**
 * Structure for holding an uncompleted guest call
 */
struct GuestCall
{
    /** The call handle */
    VBOXHGCMCALLHANDLE mHandle;
    /** The function that was requested */
    uint32_t mFunction;
    /** The call parameters */
    VBOXHGCMSVCPARM *mParms;
    /** Number of parameters */
    uint32_t mNumParms;
    /** The default return value, used for passing warnings */
    int mRc;

    /** The standard constructor */
    GuestCall() : mFunction(0) {}
    /** The normal contructor */
    GuestCall(VBOXHGCMCALLHANDLE aHandle, uint32_t aFunction,
              VBOXHGCMSVCPARM aParms[], int cParms, int aRc)
              : mHandle(aHandle), mFunction(aFunction), mParms(aParms),
                mNumParms(cParms), mRc(aRc) {}
};
/** The guest call list type */
typedef std::list <GuestCall> CallList;

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
    /** The deferred calls list */
    CallList mGuestWaiters;
    /** The host command list */
    HostCmdList mHostCmds;

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
    int paramBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void paramBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf);
    int paramBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int prepareExecute(uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    static DECLCALLBACK(int) reqThreadFn(RTTHREAD ThreadSelf, void *pvUser);
    int clientConnect(uint32_t u32ClientID, void *pvClient);
    int clientDisconnect(uint32_t u32ClientID, void *pvClient);
    int processHostMsg(VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int notifyGuest(GuestCall *pCall, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int notifyHost(VBOXHGCMCALLHANDLE callHandle, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int processCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
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
int Service::paramBufferAllocate(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
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
    return VINF_SUCCESS;
}

/*
 * Either fills in parameters from a pending host command into our guest context or
 * defer the guest call until we have something from the host.
 */
int Service::processHostMsg(VBOXHGCMCALLHANDLE callHandle, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    if (cParms < 2)
    {
        LogFlowFunc(("Guest parameter buffer is too small!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        /*
         * If host command list is empty (nothing to do right now) just
         * defer the call until we got something to do (makes the client
         * wait, depending on the flags set).
         */
        if (mHostCmds.empty()) /* Command list is empty, defer ... */
            rc = VINF_HGCM_ASYNC_EXECUTE;

        if (rc != VINF_HGCM_ASYNC_EXECUTE)
        {
            /*
             * Get the next unassigned host command in the list.
             */
             HostCmd curCmd = mHostCmds.front();
             uint32_t uParmCount = curCmd.parmBuf.uParmCount;

             /* Sufficient parameter space? */
             if (uParmCount > cParms)
             {
                uint32_t uCmd = 0;
                if (uParmCount)
                    curCmd.parmBuf.pParms[0].getUInt32(&uCmd);

                 paParms[0].setUInt32(/*uCmd*/ 1); /* Message ID */
                 paParms[1].setUInt32(uParmCount); /* Required parameters for message */

                 /*
                  * So this call apparently failed because the guest wanted to peek
                  * how much parameters it has to supply in order to successfully retrieve
                  * this command. Let's tell him so!
                  */
                 rc = VERR_TOO_MUCH_DATA;
             }
             else
             {
                 rc = paramBufferAssign(&curCmd.parmBuf, cParms, paParms);
                 if (RT_SUCCESS(rc))
                 {
                     paramBufferFree(&curCmd.parmBuf);
                     mHostCmds.pop_front();
                 }
             }
        }
        else
        {
            /* Call is deferred because of reasons above. */
            mGuestWaiters.push_back(GuestCall(callHandle, GUEST_GET_HOST_MSG,
                                              paParms, cParms, rc));
        }
    }
    return rc;
}

/*
 * Sends a command notification to the first waiting (deferred) client/guest in line in
 * order to wake up and do some work.
 */
int Service::notifyGuest(GuestCall *pCall, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pCall);
    int rc = VINF_SUCCESS;

    int rc2 = processHostMsg(pCall->mHandle, pCall->mNumParms, pCall->mParms);
    if (RT_SUCCESS(rc2))
        rc2 = pCall->mRc;
    AssertPtr(mpHelpers);
    mpHelpers->pfnCallComplete(pCall->mHandle, rc2);
    return rc;
}

int Service::notifyHost(VBOXHGCMCALLHANDLE callHandle, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("eFunction=%ld, cParms=%ld, paParms=%p\n",
                 eFunction, cParms, paParms));
    ASMBreakpoint();
    int rc;
    if (   eFunction == GUEST_EXEC_SEND_STATUS
        && cParms    == 4)
    {
        HOSTEXECCALLBACKDATA data;
        data.u32Magic = HOSTCALLBACKMAGIC;
        paParms[0].getUInt32(&data.pid);
        paParms[1].getUInt32(&data.status);
        paParms[2].getUInt32(&data.flags);
        paParms[4].getPointer(&data.pvData, &data.cbData);

        rc = mpfnHostCallback(mpvHostData, eFunction,
                              (void *)(&data), sizeof(data));
    }
    else
        rc = VERR_NOT_SUPPORTED;
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

int Service::processCmd(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    HostCmd newCmd;
    rc = paramBufferAllocate(&newCmd.parmBuf, cParms, paParms);
    if (RT_SUCCESS(rc))
    {
        mHostCmds.push_back(newCmd);

        /* Limit list size by deleting oldest element. */
        if (mHostCmds.size() > 256) /** @todo Use a define! */
            mHostCmds.pop_front();
    }

    /* Some lazy guests to wake up which can process this command right now? */
    if (!mGuestWaiters.empty())
    {
        GuestCall curCall = mGuestWaiters.front();
        rc = notifyGuest(&curCall, eFunction, cParms, paParms);
        mGuestWaiters.pop_front();
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

    ASMBreakpoint();

    try
    {
        switch (eFunction)
        {
            /* The guest asks the host for the next messsage to process. */
            case GUEST_GET_HOST_MSG:
                LogFlowFunc(("GUEST_GET_HOST_MSG\n"));
                rc = processHostMsg(callHandle, cParms, paParms);
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
                rc = notifyHost(callHandle, eFunction, cParms, paParms);
                break;

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
        /*
         * If current call is not deferred, call the completion function.
         */
        if (rc != VINF_HGCM_ASYNC_EXECUTE)
        {
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
                rc = processCmd(eFunction, cParms, paParms);
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

