/** @file
 * Video DMA (VDMA) support.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
//#include <VBox/VMMDev.h>
#include <VBox/pdmdev.h>
#include <VBox/VBoxVideo.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/mem.h>

#include "DevVGA.h"
#include "HGSMI/SHGSMIHost.h"
#include "HGSMI/HGSMIHostHlp.h"

typedef enum
{
    VBOXVDMAPIPE_STATE_CLOSED    = 0,
    VBOXVDMAPIPE_STATE_CREATED   = 1,
    VBOXVDMAPIPE_STATE_OPENNED   = 2,
    VBOXVDMAPIPE_STATE_CLOSING   = 3
} VBOXVDMAPIPE_STATE;

typedef struct VBOXVDMAPIPE
{
    RTSEMEVENT hEvent;
    /* critical section for accessing pipe properties */
    RTCRITSECT hCritSect;
    VBOXVDMAPIPE_STATE enmState;
    /* true iff the other end needs Event notification */
    bool bNeedNotify;
} VBOXVDMAPIPE, *PVBOXVDMAPIPE;

typedef enum
{
    VBOXVDMAPIPE_CMD_TYPE_UNDEFINED = 0,
    VBOXVDMAPIPE_CMD_TYPE_DMACMD    = 1,
    VBOXVDMAPIPE_CMD_TYPE_DMACTL    = 2
} VBOXVDMAPIPE_CMD_TYPE;

typedef struct VBOXVDMAPIPE_CMD_BODY
{
    VBOXVDMAPIPE_CMD_TYPE enmType;
    union
    {
        PVBOXVDMACBUF_DR pDr;
        PVBOXVDMA_CTL    pCtl;
        void            *pvCmd;
    } u;
}VBOXVDMAPIPE_CMD_BODY, *PVBOXVDMAPIPE_CMD_BODY;

typedef struct VBOXVDMAPIPE_CMD
{
    HGSMILISTENTRY Entry;
    VBOXVDMAPIPE_CMD_BODY Cmd;
} VBOXVDMAPIPE_CMD, *PVBOXVDMAPIPE_CMD;

#define VBOXVDMAPIPE_CMD_FROM_ENTRY(_pE)  ( (PVBOXVDMAPIPE_CMD)((uint8_t *)(_pE) - RT_OFFSETOF(VBOXVDMAPIPE_CMD, Entry)) )

typedef struct VBOXVDMAPIPE_CMD_POOL
{
    HGSMILIST List;
    uint32_t cCmds;
    VBOXVDMAPIPE_CMD aCmds[1];
} VBOXVDMAPIPE_CMD_POOL, *PVBOXVDMAPIPE_CMD_POOL;

typedef struct VBOXVDMAHOST
{
    VBOXVDMAPIPE Pipe;
    HGSMILIST PendingList;
    RTTHREAD hWorkerThread;
    PHGSMIINSTANCE pHgsmi;
    VBOXVDMAPIPE_CMD_POOL CmdPool;
} VBOXVDMAHOST, *PVBOXVDMAHOST;

int vboxVDMAPipeConstruct(PVBOXVDMAPIPE pPipe)
{
    int rc = RTSemEventCreate(&pPipe->hEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectInit(&pPipe->hCritSect);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            pPipe->enmState = VBOXVDMAPIPE_STATE_CREATED;
            pPipe->bNeedNotify = true;
            return VINF_SUCCESS;
//            RTCritSectDelete(pPipe->hCritSect);
        }
        RTSemEventDestroy(pPipe->hEvent);
    }
    return rc;
}

int vboxVDMAPipeOpenServer(PVBOXVDMAPIPE pPipe)
{
    int rc = RTCritSectEnter(&pPipe->hCritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED);
        switch (pPipe->enmState)
        {
            case VBOXVDMAPIPE_STATE_CREATED:
                pPipe->enmState = VBOXVDMAPIPE_STATE_OPENNED;
                pPipe->bNeedNotify = false;
                rc = VINF_SUCCESS;
                break;
            case VBOXVDMAPIPE_STATE_OPENNED:
                pPipe->bNeedNotify = false;
                rc = VINF_ALREADY_INITIALIZED;
                break;
            default:
                AssertBreakpoint();
                rc = VERR_INVALID_STATE;
                break;
        }

        RTCritSectLeave(&pPipe->hCritSect);
    }
    return rc;
}

int vboxVDMAPipeCloseServer(PVBOXVDMAPIPE pPipe)
{
    int rc = RTCritSectEnter(&pPipe->hCritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED
                || pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING);
        switch (pPipe->enmState)
        {
            case VBOXVDMAPIPE_STATE_CLOSING:
                pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSED;
                rc = VINF_SUCCESS;
                break;
            case VBOXVDMAPIPE_STATE_CLOSED:
                rc = VINF_ALREADY_INITIALIZED;
                break;
            default:
                AssertBreakpoint();
                rc = VERR_INVALID_STATE;
                break;
        }

        RTCritSectLeave(&pPipe->hCritSect);
    }
    return rc;
}

int vboxVDMAPipeCloseClient(PVBOXVDMAPIPE pPipe)
{
    int rc = RTCritSectEnter(&pPipe->hCritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        bool bNeedNotify = false;
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED
                || pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED
                ||  pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED);
        switch (pPipe->enmState)
        {
            case VBOXVDMAPIPE_STATE_OPENNED:
                pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSING;
                bNeedNotify = pPipe->bNeedNotify;
                pPipe->bNeedNotify = false;
                break;
            case VBOXVDMAPIPE_STATE_CREATED:
                pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSED;
                pPipe->bNeedNotify = false;
                break;
            case VBOXVDMAPIPE_STATE_CLOSED:
                rc = VINF_ALREADY_INITIALIZED;
                break;
            default:
                AssertBreakpoint();
                rc = VERR_INVALID_STATE;
                break;
        }

        RTCritSectLeave(&pPipe->hCritSect);

        if (bNeedNotify)
        {
            rc = RTSemEventSignal(pPipe->hEvent);
            AssertRC(rc);
        }
    }
    return rc;
}


typedef DECLCALLBACK(bool) FNHVBOXVDMARWCB(PVBOXVDMAPIPE pPipe, void *pvCallback);
typedef FNHVBOXVDMARWCB *PFNHVBOXVDMARWCB;

int vboxVDMAPipeModifyServer(PVBOXVDMAPIPE pPipe, PFNHVBOXVDMARWCB pfnCallback, void * pvCallback)
{
    int rc = RTCritSectEnter(&pPipe->hCritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        do
        {
            Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED
                    || pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING);

            if (pPipe->enmState >= VBOXVDMAPIPE_STATE_OPENNED)
            {
                bool bProcessing = pfnCallback(pPipe, pvCallback);
                pPipe->bNeedNotify = !bProcessing;
                if (bProcessing)
                {
                    RTCritSectLeave(&pPipe->hCritSect);
                    rc = VINF_SUCCESS;
                    break;
                }
                else if (pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING)
                {
                    pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSED;
                    RTCritSectLeave(&pPipe->hCritSect);
                    rc = VINF_EOF;
                    break;
                }
            }
            else
            {
                AssertBreakpoint();
                rc = VERR_INVALID_STATE;
                RTCritSectLeave(&pPipe->hCritSect);
                break;
            }

            RTCritSectLeave(&pPipe->hCritSect);

            rc = RTSemEventWait(pPipe->hEvent, RT_INDEFINITE_WAIT);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
                break;

            rc = RTCritSectEnter(&pPipe->hCritSect);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
                break;
        } while (1);
    }

    return rc;
}

int vboxVDMAPipeModifyClient(PVBOXVDMAPIPE pPipe, PFNHVBOXVDMARWCB pfnCallback, void * pvCallback)
{
    int rc = RTCritSectEnter(&pPipe->hCritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        bool bNeedNotify = false;
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED);
        if (pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED)
        {
            bool bModified = pfnCallback(pPipe, pvCallback);
            if (bModified)
            {
                bNeedNotify = pPipe->bNeedNotify;
                pPipe->bNeedNotify = false;
            }
        }
        else
            rc = VERR_INVALID_STATE;

        RTCritSectLeave(&pPipe->hCritSect);

        if (bNeedNotify)
        {
            rc = RTSemEventSignal(pPipe->hEvent);
            AssertRC(rc);
        }
    }
    return rc;
}

int vboxVDMAPipeDestruct(PVBOXVDMAPIPE pPipe)
{
    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED
            || pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED);
    /* ensure the pipe is closed */
    vboxVDMAPipeCloseClient(pPipe);

    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED);

    if (pPipe->enmState != VBOXVDMAPIPE_STATE_CLOSED)
        return VERR_INVALID_STATE;

    int rc = RTCritSectDelete(&pPipe->hCritSect);
    AssertRC(rc);

    rc = RTSemEventDestroy(pPipe->hEvent);
    AssertRC(rc);

    return VINF_SUCCESS;
}

void vboxVDMACommandProcess(PVBOXVDMAHOST pVdma, PVBOXVDMACBUF_DR pCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    pCmd->rc = VINF_SUCCESS;
    int rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

void vboxVDMAControlProcess(PVBOXVDMAHOST pVdma, PVBOXVDMA_CTL pCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    pCmd->i32Result = VINF_SUCCESS;
    int rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

typedef struct
{
    struct VBOXVDMAHOST *pVdma;
    VBOXVDMAPIPE_CMD_BODY Cmd;
    bool bHasCmd;
} VBOXVDMACMD_PROCESS_CONTEXT, *PVBOXVDMACMD_PROCESS_CONTEXT;

DECLCALLBACK(bool) vboxVDMACommandProcessCb(PVBOXVDMAPIPE pPipe, void *pvCallback)
{
    PVBOXVDMACMD_PROCESS_CONTEXT pContext = (PVBOXVDMACMD_PROCESS_CONTEXT)pvCallback;
    struct VBOXVDMAHOST *pVdma = pContext->pVdma;
    HGSMILISTENTRY *pEntry = hgsmiListRemoveHead(&pVdma->PendingList);
    if (pEntry)
    {
        PVBOXVDMAPIPE_CMD pPipeCmd = VBOXVDMAPIPE_CMD_FROM_ENTRY(pEntry);
        Assert(pPipeCmd);
        pContext->Cmd = pPipeCmd->Cmd;
        hgsmiListPrepend(&pVdma->CmdPool.List, pEntry);
        pContext->bHasCmd = true;
        return true;
    }

    pContext->bHasCmd = false;
    return false;
}

DECLCALLBACK(int) vboxVDMAWorkerThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)pvUser;
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    VBOXVDMACMD_PROCESS_CONTEXT Context;
    Context.pVdma = pVdma;

    int rc = vboxVDMAPipeOpenServer(&pVdma->Pipe);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        do
        {
            rc = vboxVDMAPipeModifyServer(&pVdma->Pipe, vboxVDMACommandProcessCb, &Context);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                switch (Context.Cmd.enmType)
                {
                    case VBOXVDMAPIPE_CMD_TYPE_DMACMD:
                    {
                        PVBOXVDMACBUF_DR pDr = Context.Cmd.u.pDr;
                        vboxVDMACommandProcess(pVdma, pDr);
                        break;
                    }
                    case VBOXVDMAPIPE_CMD_TYPE_DMACTL:
                    {
                        PVBOXVDMA_CTL pCtl = Context.Cmd.u.pCtl;
                        vboxVDMAControlProcess(pVdma, pCtl);
                        break;
                    }
                    default:
                        AssertBreakpoint();
                        break;
                }

                if (rc == VINF_EOF)
                {
                    rc = VINF_SUCCESS;
                    break;
                }
            }
            else
                break;
        } while (1);
    }

    /* always try to close the pipe to make sure the client side is notified */
    int tmpRc = vboxVDMAPipeCloseServer(&pVdma->Pipe);
    AssertRC(tmpRc);
    return rc;
}

int vboxVDMAConstruct(PVGASTATE pVGAState, struct VBOXVDMAHOST **ppVdma, uint32_t cPipeElements)
{
    int rc;
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)RTMemAllocZ (RT_OFFSETOF(VBOXVDMAHOST, CmdPool.aCmds[cPipeElements]));
    Assert(pVdma);
    if (pVdma)
    {
        hgsmiListInit(&pVdma->PendingList);
        pVdma->pHgsmi = pVGAState->pHGSMI;

        rc = vboxVDMAPipeConstruct(&pVdma->Pipe);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreate(&pVdma->hWorkerThread, vboxVDMAWorkerThread, pVdma, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "VDMA");
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                hgsmiListInit(&pVdma->CmdPool.List);
                pVdma->CmdPool.cCmds = cPipeElements;
                for (uint32_t i = 0; i < cPipeElements; ++i)
                {
                    hgsmiListAppend(&pVdma->CmdPool.List, &pVdma->CmdPool.aCmds[i].Entry);
                }
                *ppVdma = pVdma;
                return VINF_SUCCESS;
            }

            int tmpRc = vboxVDMAPipeDestruct(&pVdma->Pipe);
            AssertRC(tmpRc);
        }

        RTMemFree(pVdma);
    }
    else
        rc = VERR_OUT_OF_RESOURCES;

    return rc;
}

int vboxVDMADestruct(struct VBOXVDMAHOST **pVdma)
{
    AssertBreakpoint();
    return VINF_SUCCESS;
}

typedef struct
{
    struct VBOXVDMAHOST *pVdma;
    VBOXVDMAPIPE_CMD_BODY Cmd;
    bool bQueued;
} VBOXVDMACMD_SUBMIT_CONTEXT, *PVBOXVDMACMD_SUBMIT_CONTEXT;

DECLCALLBACK(bool) vboxVDMACommandSubmitCb(PVBOXVDMAPIPE pPipe, void *pvCallback)
{
    PVBOXVDMACMD_SUBMIT_CONTEXT pContext = (PVBOXVDMACMD_SUBMIT_CONTEXT)pvCallback;
    struct VBOXVDMAHOST *pVdma = pContext->pVdma;
    HGSMILISTENTRY *pEntry = hgsmiListRemoveHead(&pVdma->CmdPool.List);
    Assert(pEntry);
    if (pEntry)
    {
        PVBOXVDMAPIPE_CMD pPipeCmd = VBOXVDMAPIPE_CMD_FROM_ENTRY(pEntry);
        pPipeCmd->Cmd = pContext->Cmd;
        VBoxSHGSMICommandMarkAsynchCompletion(pContext->Cmd.u.pvCmd);
        pContext->bQueued = true;
        hgsmiListAppend(&pVdma->PendingList, pEntry);
        return true;
    }

    /* @todo: should we try to flush some commands here? */
    pContext->bQueued = false;
    return false;
}

void vboxVDMAControl(struct VBOXVDMAHOST *pVdma, PVBOXVDMA_CTL pCmd)
{
#if 0
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;

    switch (pCmd->enmCtl)
    {
        case VBOXVDMA_CTL_TYPE_ENABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_DISABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_FLUSH:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        default:
            AssertBreakpoint();
            pCmd->i32Result = VERR_NOT_SUPPORTED;
    }

    int rc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(rc);
#else
    /* test asinch completion */
    VBOXVDMACMD_SUBMIT_CONTEXT Context;
    Context.pVdma = pVdma;
    Context.Cmd.enmType = VBOXVDMAPIPE_CMD_TYPE_DMACTL;
    Context.Cmd.u.pCtl = pCmd;

    int rc = vboxVDMAPipeModifyClient(&pVdma->Pipe, vboxVDMACommandSubmitCb, &Context);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(Context.bQueued);
        if (Context.bQueued)
        {
            /* success */
            return;
        }
        rc = VERR_OUT_OF_RESOURCES;
    }

    /* failure */
    Assert(RT_FAILURE(rc));
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;
    pCmd->i32Result = rc;
    int tmpRc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(tmpRc);

#endif
}

void vboxVDMACommand(struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmd)
{
    VBOXVDMACMD_SUBMIT_CONTEXT Context;
    Context.pVdma = pVdma;
    Context.Cmd.enmType = VBOXVDMAPIPE_CMD_TYPE_DMACMD;
    Context.Cmd.u.pDr = pCmd;

    int rc = vboxVDMAPipeModifyClient(&pVdma->Pipe, vboxVDMACommandSubmitCb, &Context);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(Context.bQueued);
        if (Context.bQueued)
        {
            /* success */
            return;
        }
        rc = VERR_OUT_OF_RESOURCES;
    }

    /* failure */
    Assert(RT_FAILURE(rc));
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;
    pCmd->rc = rc;
    int tmpRc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(tmpRc);
}

