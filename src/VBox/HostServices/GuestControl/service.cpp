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
    /** The execution data to hold (atm only one buffer!) */
    VBOXGUESTCTRPARAMBUFFER mExec;

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
    static DECLCALLBACK(int) svcConnectDisconnect (void * /* pvService */,
                                                   uint32_t /* u32ClientID */,
                                                   void * /* pvClient */)
    {
        return VINF_SUCCESS;
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
    void call (VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
               void *pvClient, uint32_t eFunction, uint32_t cParms,
               VBOXHGCMSVCPARM paParms[]);
    int hostCall (uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int uninit ();
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
void Service::execBufferFree(PVBOXGUESTCTRPARAMBUFFER pBuf)
{
    AssertPtr(pBuf);
    for (uint32_t i = 0; i < pBuf->uParmCount; i++)
    {
        switch (pBuf->pParms[i].type)
        {
            case VBOX_HGCM_SVC_PARM_PTR:
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
int Service::execBufferAssign(PVBOXGUESTCTRPARAMBUFFER pBuf, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertPtr(pBuf);
    if (cParms != pBuf->uParmCount)
        return VERR_INVALID_PARAMETER;
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
                paParms[i].u.pointer.size = pBuf->pParms[i].u.pointer.size;
                paParms[i].u.pointer.addr = pBuf->pParms[i].u.pointer.addr;
                break;

            default:                        
                break;
        }
    }

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
    try
    {
        switch (eFunction)
        {
            /* The guest asks the host for the next messsage to process. */
            case GUEST_GET_HOST_MSG:
                LogFlowFunc(("GUEST_GET_HOST_MSG\n"));
                paParms[0].setUInt32(HOST_EXEC_CMD); /* msg id */
                paParms[1].setUInt32(12); /* parms count */
                break;

            case GUEST_GET_HOST_MSG_DATA:
                LogFlowFunc(("GUEST_GET_HOST_MSG_DATA\n"));
                rc = execBufferAssign(&mExec, cParms, paParms);
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
        mpHelpers->pfnCallComplete (callHandle, rc);
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

    LogFlowFunc(("fn = %d, cParms = %d, pparms = %d\n",
                 eFunction, cParms, paParms));
    try
    {
        switch (eFunction)
        {
            /* The host wants to execute something. */
            case HOST_EXEC_CMD:
                LogFlowFunc(("HOST_EXEC_CMD\n"));
                rc = execBufferAllocate(&mExec, cParms, paParms);
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
                ptable->pfnConnect            = Service::svcConnectDisconnect;
                ptable->pfnDisconnect         = Service::svcConnectDisconnect;
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

