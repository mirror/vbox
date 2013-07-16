/* $Id$ */
/** @file
 * VBoxIPC - IPC thread, acts as a (purely) local IPC server.
 *           Multiple sessions are supported, whereas every session
 *           has its own thread for processing requests.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <windows.h>
#include "VBoxTray.h"
#include "VBoxTrayMsg.h"
#include "VBoxHelpers.h"
#include "VBoxIPC.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/list.h>
#include <iprt/localipc.h>
#include <iprt/mem.h>
#include <VBoxGuestInternal.h>





/**
 * IPC context data.
 */
typedef struct VBOXIPCCONTEXT
{
    /** Pointer to the service environment. */
    const VBOXSERVICEENV      *pEnv;
    /** Handle for the local IPC server. */
    RTLOCALIPCSERVER           hServer;
    /** Critical section serializing access to the session list, the state,
     * the response event, the session event, and the thread event. */
    RTCRITSECT                 CritSect;
    /** List of all active IPC sessions. */
    RTLISTANCHOR               SessionList;

} VBOXIPCCONTEXT, *PVBOXIPCCONTEXT;
static VBOXIPCCONTEXT gCtx = {0};

/**
 * IPC per-session thread data.
 */
typedef struct VBOXIPCSESSION
{
    /** The list node required to be part of the
     *  IPC session list. */
    RTLISTNODE                          Node;
    /** Pointer to the IPC context data. */
    PVBOXIPCCONTEXT volatile            pCtx;
    /** The local ipc client handle. */
    RTLOCALIPCSESSION volatile          hSession;
    /** Indicate that the thread should terminate ASAP. */
    bool volatile                       fTerminate;
    /** The thread handle. */
    RTTHREAD                            hThread;

} VBOXIPCSESSION, *PVBOXIPCSESSION;

int vboxIPCSessionDestroyLocked(PVBOXIPCCONTEXT pCtx, PVBOXIPCSESSION pSession);

/**
 * Initializes the IPC communication.
 *
 * @return  IPRT status code.
 * @param   pEnv                        The IPC service's environment.
 * @param   ppInstance                  The instance pointer which refer to this object.
 * @param   pfStartThread               Pointer to flag whether the IPC service can be started or not.
 */
int VBoxIPCInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    /** ppInstance not used here. */
    AssertPtrReturn(pfStartThread, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    *pfStartThread = false;

    gCtx.pEnv = pEnv;
    gCtx.hServer = NIL_RTLOCALIPCSERVER;

    int rc = RTCritSectInit(&gCtx.CritSect);
    if (RT_SUCCESS(rc))
    {
        rc = RTLocalIpcServerCreate(&gCtx.hServer, "VBoxTrayIPCSvc", RTLOCALIPC_FLAGS_MULTI_SESSION);
        if (RT_FAILURE(rc))
        {
            LogRelFunc(("Creating local IPC server failed with rc=%Rrc\n", rc));
            return rc;
        }

        RTListInit(&gCtx.SessionList);

        *pfStartThread = true;
    }

    return rc;
}

void VBoxIPCStop(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    AssertPtr(pEnv);
    AssertPtr(pInstance);

    LogFunc(("Stopping pInstance=%p\n", pInstance));

    PVBOXIPCCONTEXT pCtx = (PVBOXIPCCONTEXT)pInstance;
    AssertPtr(pCtx);

    if (pCtx->hServer != NIL_RTLOCALIPCSERVER)
    {
        int rc2 = RTLocalIpcServerCancel(pCtx->hServer);
        if (RT_FAILURE(rc2))
            LogFunc(("Cancelling current listening call failed with rc=%Rrc\n", rc2));
    }
}

void VBoxIPCDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    AssertPtr(pEnv);
    AssertPtr(pInstance);

    LogFunc(("Destroying pInstance=%p\n", pInstance));

    PVBOXIPCCONTEXT pCtx = (PVBOXIPCCONTEXT)pInstance;
    AssertPtr(pCtx);

    int rc = RTCritSectEnter(&pCtx->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXIPCSESSION pSession;
        RTListForEach(&pCtx->SessionList, pSession, VBOXIPCSESSION, Node)
        {
            int rc2 = vboxIPCSessionDestroyLocked(pCtx, pSession);
            if (RT_FAILURE(rc2))
            {
                LogFunc(("Destroying IPC session %p failed with rc=%Rrc\n",
                         pSession, rc2));
                /* Keep going. */
            }
        }

        RTLocalIpcServerDestroy(pCtx->hServer);

        int rc2 = RTCritSectLeave(&pCtx->CritSect);
        AssertRC(rc2);

        rc2 = RTCritSectDelete(&pCtx->CritSect);
        AssertRC(rc2);
    }

    LogFunc(("Destroyed pInstance=%p, rc=%Rrc\n",
             pInstance, rc));
}

/**
 * Services a client session.
 *
 * @returns VINF_SUCCESS.
 * @param   hThread         The thread handle.
 * @param   pvSession       Pointer to the session instance data.
 */
static DECLCALLBACK(int) vboxIPCSessionThread(RTTHREAD hThread, void *pvSession)
{
    PVBOXIPCSESSION pThis = (PVBOXIPCSESSION)pvSession;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    RTLOCALIPCSESSION hSession = pThis->hSession;
    AssertReturn(hSession != NIL_RTLOCALIPCSESSION, VERR_INVALID_PARAMETER);

    LogFunc(("pThis=%p\n", pThis));

    /*
     * Process client requests until it quits or we're cancelled on termination.
     */
    while (!ASMAtomicUoReadBool(&pThis->fTerminate))
    {
        int rc = RTLocalIpcSessionWaitForData(hSession, 1000 /* Timeout in ms. */);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_CANCELLED)
            {
                LogFunc(("Waiting for data cancelled\n"));
                rc = VINF_SUCCESS;
                break;
            }
            else if (rc != VERR_TIMEOUT)
            {
                LogFunc(("Waiting for data failed, rc=%Rrc\n", rc));
                break;
            }
        }

        /** @todo Implement handler. */
    }

    /*
     * Clean up the session.
     */
    PVBOXIPCCONTEXT pCtx = ASMAtomicReadPtrT(&pThis->pCtx, PVBOXIPCCONTEXT);
    if (pCtx)
        RTCritSectEnter(&pCtx->CritSect);
    else
        AssertMsgFailed(("Session %p: No context found\n", pThis));

    ASMAtomicXchgHandle(&pThis->hSession, NIL_RTLOCALIPCSESSION, &hSession);
    if (hSession != NIL_RTLOCALIPCSESSION)
        RTLocalIpcSessionClose(hSession);
    else
        AssertMsgFailed(("Session %p: No/invalid session handle\n", pThis));

    if (pCtx)
    {
        //RTSemEventSignal(pCtx->hSessionEvent);
        RTCritSectLeave(&pCtx->CritSect);
    }

    LogFunc(("pThis=%p terminated\n", pThis));
    return VINF_SUCCESS;
}

static int vboxIPCSessionCreate(PVBOXIPCCONTEXT pCtx, RTLOCALIPCSESSION hSession)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(hSession != NIL_RTLOCALIPCSESSION, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pCtx->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXIPCSESSION pSession = (PVBOXIPCSESSION)RTMemAllocZ(sizeof(VBOXIPCSESSION));
        if (pSession)
        {
            pSession->pCtx = pCtx;
            pSession->hSession = hSession;
            pSession->fTerminate = false;
            pSession->hThread = NIL_RTTHREAD;

            /* Start IPC session thread. */
            LogFlowFunc(("Creating thread for session %p ...\n", pSession));
            rc = RTThreadCreate(&pSession->hThread, vboxIPCSessionThread, pSession, 0,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VBXTRYIPCSESS");
            if (RT_SUCCESS(rc))
            {
                /* Add session thread to session IPC list. */
                RTListAppend(&pCtx->SessionList, &pSession->Node);
            }
            else
            {
                int rc2 = RTLocalIpcSessionClose(hSession);
                if (RT_FAILURE(rc2))
                    LogFunc(("Failed closing session %p, rc=%Rrc\n", pSession, rc2));

                LogFunc(("Failed to create thread for session %p, rc=%Rrc\n", pSession, rc));
                RTMemFree(pSession);
            }
        }
        else
            rc = VERR_NO_MEMORY;

        int rc2 = RTCritSectLeave(&pCtx->CritSect);
        AssertRC(rc2);
    }

    return rc;
}

static int vboxIPCSessionDestroyLocked(PVBOXIPCCONTEXT pCtx, PVBOXIPCSESSION pSession)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    pSession->hThread = NIL_RTTHREAD;

    RTLOCALIPCSESSION hSession;
    ASMAtomicXchgHandle(&pSession->hSession, NIL_RTLOCALIPCSESSION, &hSession);
    if (hSession != NIL_RTLOCALIPCSESSION)
        RTLocalIpcSessionClose(hSession);

    RTListNodeRemove(&pSession->Node);

    RTMemFree(pSession);
    pSession = NULL;

    return VINF_SUCCESS;
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxIPCThread(void *pInstance)
{
    LogFlowFuncEnter();

    PVBOXIPCCONTEXT pCtx = (PVBOXIPCCONTEXT)pInstance;
    AssertPtr(pCtx);

    bool fShutdown = false;
    for (;;)
    {
        RTLOCALIPCSESSION hClientSession = NIL_RTLOCALIPCSESSION;
        int rc = RTLocalIpcServerListen(pCtx->hServer, &hClientSession);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_CANCELLED)
            {
                LogFlow(("Cancelled\n"));
                fShutdown = true;
            }
            else
                LogRelFunc(("Listening failed with rc=%Rrc\n", rc));
        }

        if (fShutdown)
            break;
        rc = vboxIPCSessionCreate(pCtx, hClientSession);
        if (RT_FAILURE(rc))
        {
            LogRelFunc(("Creating new IPC server session failed with rc=%Rrc\n", rc));
            /* Keep going. */
        }

        AssertPtr(pCtx->pEnv);
        if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0 /* No waiting */) == WAIT_OBJECT_0)
            break;
    }

    LogFlowFuncLeave();
    return 0;
}

