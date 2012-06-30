/** @file
 * Video DMA (VDMA) support.
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
//#include <VBox/VMMDev.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/VBoxVideo.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

#include "DevVGA.h"
#include "HGSMI/SHGSMIHost.h"
#include "HGSMI/HGSMIHostHlp.h"

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
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
#endif

typedef struct VBOXVDMAHOST
{
    PHGSMIINSTANCE pHgsmi;
    PVGASTATE pVGAState;
#ifdef VBOX_VDMA_WITH_WATCHDOG
    PTMTIMERR3 WatchDogTimer;
#endif
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    VBOXVDMAPIPE Pipe;
    HGSMILIST PendingList;
    RTTHREAD hWorkerThread;
    VBOXVDMAPIPE_CMD_POOL CmdPool;
#endif
} VBOXVDMAHOST, *PVBOXVDMAHOST;


#ifdef VBOX_WITH_CRHGSMI

typedef DECLCALLBACK(void) FNVBOXVDMACRCTL_CALLBACK(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext);
typedef FNVBOXVDMACRCTL_CALLBACK *PFNVBOXVDMACRCTL_CALLBACK;

typedef struct VBOXVDMACMD_CHROMIUM_CTL_PRIVATE
{
    uint32_t cRefs;
    int32_t rc;
    PFNVBOXVDMACRCTL_CALLBACK pfnCompletion;
    void *pvCompletion;
    VBOXVDMACMD_CHROMIUM_CTL Cmd;
} VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, *PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE;

#define VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(_p) ((PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, Cmd)))

static PVBOXVDMACMD_CHROMIUM_CTL vboxVDMACrCtlCreate(VBOXVDMACMD_CHROMIUM_CTL_TYPE enmCmd, uint32_t cbCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = (PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE)RTMemAllocZ(cbCmd + RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, Cmd));
    Assert(pHdr);
    if (pHdr)
    {
        pHdr->cRefs = 1;
        pHdr->rc = VERR_NOT_IMPLEMENTED;
        pHdr->Cmd.enmType = enmCmd;
        pHdr->Cmd.cbCmd = cbCmd;
        return &pHdr->Cmd;
    }

    return NULL;
}

DECLINLINE(void) vboxVDMACrCtlRelease (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    uint32_t cRefs = ASMAtomicDecU32(&pHdr->cRefs);
    if(!cRefs)
    {
        RTMemFree(pHdr);
    }
}

DECLINLINE(void) vboxVDMACrCtlRetain (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    ASMAtomicIncU32(&pHdr->cRefs);
}

DECLINLINE(int) vboxVDMACrCtlGetRc (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    return pHdr->rc;
}

static DECLCALLBACK(void) vboxVDMACrCtlCbSetEvent(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext)
{
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

static DECLCALLBACK(void) vboxVDMACrCtlCbReleaseCmd(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext)
{
    vboxVDMACrCtlRelease(pCmd);
}


static int vboxVDMACrCtlPostAsync (PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, uint32_t cbCmd, PFNVBOXVDMACRCTL_CALLBACK pfnCompletion, void *pvCompletion)
{
    if (   pVGAState->pDrv
        && pVGAState->pDrv->pfnCrHgsmiControlProcess)
    {
        PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
        pHdr->pfnCompletion = pfnCompletion;
        pHdr->pvCompletion = pvCompletion;
        pVGAState->pDrv->pfnCrHgsmiControlProcess(pVGAState->pDrv, pCmd, cbCmd);
        return VINF_SUCCESS;
    }
#ifdef DEBUG_misha
    Assert(0);
#endif
    return VERR_NOT_SUPPORTED;
}

static int vboxVDMACrCtlPost(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, uint32_t cbCmd)
{
    RTSEMEVENT hComplEvent;
    int rc = RTSemEventCreate(&hComplEvent);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        rc = vboxVDMACrCtlPostAsync (pVGAState, pCmd, cbCmd, vboxVDMACrCtlCbSetEvent, (void*)hComplEvent);
#ifdef DEBUG_misha
        AssertRC(rc);
#endif
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventWaitNoResume(hComplEvent, RT_INDEFINITE_WAIT);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                RTSemEventDestroy(hComplEvent);
            }
        }
        else
        {
            /* the command is completed */
            RTSemEventDestroy(hComplEvent);
        }
    }
    return rc;
}

static int vboxVDMACrCtlHgsmiSetup(struct VBOXVDMAHOST *pVdma)
{
    PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP pCmd;
    pCmd = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP) vboxVDMACrCtlCreate (VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP,
                                                                          sizeof (*pCmd));
    if (pCmd)
    {
        PVGASTATE pVGAState = pVdma->pVGAState;
        pCmd->pvVRamBase = pVGAState->vram_ptrR3;
        pCmd->cbVRam = pVGAState->vram_size;
        int rc = vboxVDMACrCtlPost(pVGAState, &pCmd->Hdr, sizeof (*pCmd));
        Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(&pCmd->Hdr);
        }
        vboxVDMACrCtlRelease(&pCmd->Hdr);
        return rc;
    }
    return VERR_NO_MEMORY;
}

static int vboxVDMACmdExecBpbTransfer(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer, uint32_t cbBuffer);

/* check if this is external cmd to be passed to chromium backend */
static int vboxVDMACmdCheckCrCmd(struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmdDr, uint32_t cbCmdDr)
{
    PVBOXVDMACMD pDmaCmd = NULL;
    uint32_t cbDmaCmd = 0;
    uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
    int rc = VINF_NOT_SUPPORTED;

    if (pCmdDr->fFlags & VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR)
    {
        if (cbCmdDr < sizeof (*pCmdDr) + VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        cbDmaCmd = pCmdDr->cbBuf;
        if (cbDmaCmd < cbCmdDr - sizeof (*pCmdDr) - VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid command buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        pDmaCmd = VBOXVDMACBUF_DR_TAIL(pCmdDr, VBOXVDMACMD);
    }

    if (pDmaCmd)
    {
        Assert(cbDmaCmd >= VBOXVDMACMD_HEADER_SIZE());
        uint32_t cbBody = VBOXVDMACMD_BODY_SIZE(cbDmaCmd);

        switch (pDmaCmd->enmType)
        {
            case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
                PVBOXVDMACMD_CHROMIUM_CMD pCrCmd = VBOXVDMACMD_BODY(pDmaCmd, VBOXVDMACMD_CHROMIUM_CMD);
                if (cbBody < sizeof (*pCrCmd))
                {
                    AssertMsgFailed(("invalid chromium command buffer size!"));
                    return VERR_INVALID_PARAMETER;
                }
                PVGASTATE pVGAState = pVdma->pVGAState;
                rc = VINF_SUCCESS;
                if (pVGAState->pDrv->pfnCrHgsmiCommandProcess)
                {
                    VBoxSHGSMICommandMarkAsynchCompletion(pCmdDr);
                    pVGAState->pDrv->pfnCrHgsmiCommandProcess(pVGAState->pDrv, pCrCmd, cbBody);
                    break;
                }
                else
                {
                    Assert(0);
                }

                int tmpRc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmdDr);
                AssertRC(tmpRc);
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER:
            {
                PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer = VBOXVDMACMD_BODY(pDmaCmd, VBOXVDMACMD_DMA_BPB_TRANSFER);
                if (cbBody < sizeof (*pTransfer))
                {
                    AssertMsgFailed(("invalid bpb transfer buffer size!"));
                    return VERR_INVALID_PARAMETER;
                }

                rc = vboxVDMACmdExecBpbTransfer(pVdma, pTransfer, sizeof (*pTransfer));
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    pCmdDr->rc = VINF_SUCCESS;
                    rc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmdDr);
                    AssertRC(rc);
                    rc = VINF_SUCCESS;
                }
                break;
            }
            default:
                break;
        }
    }
    return rc;
}

int vboxVDMACrHgsmiCommandCompleteAsync(PPDMIDISPLAYVBVACALLBACKS pInterface, PVBOXVDMACMD_CHROMIUM_CMD pCmd, int rc)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBOXVDMACMD *pDmaHdr = VBOXVDMACMD_FROM_BODY(pCmd);
    VBOXVDMACBUF_DR *pDr = VBOXVDMACBUF_DR_FROM_TAIL(pDmaHdr);
    AssertRC(rc);
    pDr->rc = rc;

    Assert(pVGAState->fGuestCaps & VBVACAPS_COMPLETEGCMD_BY_IOREAD);
    rc = VBoxSHGSMICommandComplete(pIns, pDr);
    AssertRC(rc);
    return rc;
}

int vboxVDMACrHgsmiControlCompleteAsync(PPDMIDISPLAYVBVACALLBACKS pInterface, PVBOXVDMACMD_CHROMIUM_CTL pCmd, int rc)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pCmdPrivate = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    pCmdPrivate->rc = rc;
    if (pCmdPrivate->pfnCompletion)
    {
        pCmdPrivate->pfnCompletion(pVGAState, pCmd, pCmdPrivate->pvCompletion);
    }
    return VINF_SUCCESS;
}

#endif

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
/* to simplify things and to avoid extra backend if modifications we assume the VBOXVDMA_RECTL is the same as VBVACMDHDR */
AssertCompile(sizeof(VBOXVDMA_RECTL) == sizeof(VBVACMDHDR));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, left) == RT_SIZEOFMEMB(VBVACMDHDR, x));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, top) == RT_SIZEOFMEMB(VBVACMDHDR, y));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, width) == RT_SIZEOFMEMB(VBVACMDHDR, w));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, height) == RT_SIZEOFMEMB(VBVACMDHDR, h));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, left) == RT_OFFSETOF(VBVACMDHDR, x));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, top) == RT_OFFSETOF(VBVACMDHDR, y));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, width) == RT_OFFSETOF(VBVACMDHDR, w));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, height) == RT_OFFSETOF(VBVACMDHDR, h));

static int vboxVDMANotifyPrimaryUpdate (PVGASTATE pVGAState, unsigned uScreenId, const VBOXVDMA_RECTL * pRectl)
{
    pVGAState->pDrv->pfnVBVAUpdateBegin (pVGAState->pDrv, uScreenId);

    /* Updates the rectangle and sends the command to the VRDP server. */
    pVGAState->pDrv->pfnVBVAUpdateProcess (pVGAState->pDrv, uScreenId,
            (const PVBVACMDHDR)pRectl /* <- see above AssertCompile's and comments */,
            sizeof (VBOXVDMA_RECTL));

    pVGAState->pDrv->pfnVBVAUpdateEnd (pVGAState->pDrv, uScreenId, pRectl->left, pRectl->top,
                                               pRectl->width, pRectl->height);

    return VINF_SUCCESS;
}
#endif

static int vboxVDMACmdExecBltPerform(PVBOXVDMAHOST pVdma,
        uint8_t *pvDstSurf, const uint8_t *pvSrcSurf,
        const PVBOXVDMA_SURF_DESC pDstDesc, const PVBOXVDMA_SURF_DESC pSrcDesc,
        const VBOXVDMA_RECTL * pDstRectl, const VBOXVDMA_RECTL * pSrcRectl)
{
    /* we do not support color conversion */
    Assert(pDstDesc->format == pSrcDesc->format);
    /* we do not support stretching */
    Assert(pDstRectl->height == pSrcRectl->height);
    Assert(pDstRectl->width == pSrcRectl->width);
    if (pDstDesc->format != pSrcDesc->format)
        return VERR_INVALID_FUNCTION;
    if (pDstDesc->width == pDstRectl->width
            && pSrcDesc->width == pSrcRectl->width
            && pSrcDesc->width == pDstDesc->width)
    {
        Assert(!pDstRectl->left);
        Assert(!pSrcRectl->left);
        uint32_t cbOff = pDstDesc->pitch * pDstRectl->top;
        uint32_t cbSize = pDstDesc->pitch * pDstRectl->height;
        memcpy(pvDstSurf + cbOff, pvSrcSurf + cbOff, cbSize);
    }
    else
    {
        uint32_t offDstLineStart = pDstRectl->left * pDstDesc->bpp >> 3;
        uint32_t offDstLineEnd = ((pDstRectl->left * pDstDesc->bpp + 7) >> 3) + ((pDstDesc->bpp * pDstRectl->width + 7) >> 3);
        uint32_t cbDstLine = offDstLineEnd - offDstLineStart;
        uint32_t offDstStart = pDstDesc->pitch * pDstRectl->top + offDstLineStart;
        Assert(cbDstLine <= pDstDesc->pitch);
        uint32_t cbDstSkip = pDstDesc->pitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t offSrcLineStart = pSrcRectl->left * pSrcDesc->bpp >> 3;
        uint32_t offSrcLineEnd = ((pSrcRectl->left * pSrcDesc->bpp + 7) >> 3) + ((pSrcDesc->bpp * pSrcRectl->width + 7) >> 3);
        uint32_t cbSrcLine = offSrcLineEnd - offSrcLineStart;
        uint32_t offSrcStart = pSrcDesc->pitch * pSrcRectl->top + offSrcLineStart;
        Assert(cbSrcLine <= pSrcDesc->pitch);
        uint32_t cbSrcSkip = pSrcDesc->pitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; ; ++i)
        {
            memcpy (pvDstStart, pvSrcStart, cbDstLine);
            if (i == pDstRectl->height)
                break;
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return VINF_SUCCESS;
}

static void vboxVDMARectlUnite(VBOXVDMA_RECTL * pRectl1, const VBOXVDMA_RECTL * pRectl2)
{
    if (!pRectl1->width)
        *pRectl1 = *pRectl2;
    else
    {
        int16_t x21 = pRectl1->left + pRectl1->width;
        int16_t x22 = pRectl2->left + pRectl2->width;
        if (pRectl1->left > pRectl2->left)
        {
            pRectl1->left = pRectl2->left;
            pRectl1->width = x21 < x22 ? x22 - pRectl1->left : x21 - pRectl1->left;
        }
        else if (x21 < x22)
            pRectl1->width = x22 - pRectl1->left;

        x21 = pRectl1->top + pRectl1->height;
        x22 = pRectl2->top + pRectl2->height;
        if (pRectl1->top > pRectl2->top)
        {
            pRectl1->top = pRectl2->top;
            pRectl1->height = x21 < x22 ? x22 - pRectl1->top : x21 - pRectl1->top;
        }
        else if (x21 < x22)
            pRectl1->height = x22 - pRectl1->top;
    }
}

/*
 * @return on success the number of bytes the command contained, otherwise - VERR_xxx error code
 */
static int vboxVDMACmdExecBlt(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_PRESENT_BLT pBlt, uint32_t cbBuffer)
{
    const uint32_t cbBlt = VBOXVDMACMD_BODY_FIELD_OFFSET(uint32_t, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects[pBlt->cDstSubRects]);
    Assert(cbBlt <= cbBuffer);
    if (cbBuffer < cbBlt)
        return VERR_INVALID_FUNCTION;

    /* we do not support stretching for now */
    Assert(pBlt->srcRectl.width == pBlt->dstRectl.width);
    Assert(pBlt->srcRectl.height == pBlt->dstRectl.height);
    if (pBlt->srcRectl.width != pBlt->dstRectl.width)
        return VERR_INVALID_FUNCTION;
    if (pBlt->srcRectl.height != pBlt->dstRectl.height)
        return VERR_INVALID_FUNCTION;
    Assert(pBlt->cDstSubRects);

    uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
    VBOXVDMA_RECTL updateRectl = {0, 0, 0, 0};

    if (pBlt->cDstSubRects)
    {
        VBOXVDMA_RECTL dstRectl, srcRectl;
        const VBOXVDMA_RECTL *pDstRectl, *pSrcRectl;
        for (uint32_t i = 0; i < pBlt->cDstSubRects; ++i)
        {
            pDstRectl = &pBlt->aDstSubRects[i];
            if (pBlt->dstRectl.left || pBlt->dstRectl.top)
            {
                dstRectl.left = pDstRectl->left + pBlt->dstRectl.left;
                dstRectl.top = pDstRectl->top + pBlt->dstRectl.top;
                dstRectl.width = pDstRectl->width;
                dstRectl.height = pDstRectl->height;
                pDstRectl = &dstRectl;
            }

            pSrcRectl = &pBlt->aDstSubRects[i];
            if (pBlt->srcRectl.left || pBlt->srcRectl.top)
            {
                srcRectl.left = pSrcRectl->left + pBlt->srcRectl.left;
                srcRectl.top = pSrcRectl->top + pBlt->srcRectl.top;
                srcRectl.width = pSrcRectl->width;
                srcRectl.height = pSrcRectl->height;
                pSrcRectl = &srcRectl;
            }

            int rc = vboxVDMACmdExecBltPerform(pVdma, pvRam + pBlt->offDst, pvRam + pBlt->offSrc,
                    &pBlt->dstDesc, &pBlt->srcDesc,
                    pDstRectl,
                    pSrcRectl);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
                return rc;

            vboxVDMARectlUnite(&updateRectl, pDstRectl);
        }
    }
    else
    {
        int rc = vboxVDMACmdExecBltPerform(pVdma, pvRam + pBlt->offDst, pvRam + pBlt->offSrc,
                &pBlt->dstDesc, &pBlt->srcDesc,
                &pBlt->dstRectl,
                &pBlt->srcRectl);
        AssertRC(rc);
        if (!RT_SUCCESS(rc))
            return rc;

        vboxVDMARectlUnite(&updateRectl, &pBlt->dstRectl);
    }

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    int iView = 0;
    /* @todo: fixme: check if update is needed and get iView */
    vboxVDMANotifyPrimaryUpdate (pVdma->pVGAState, iView, &updateRectl);
#endif

    return cbBlt;
}

static int vboxVDMACmdExecBpbTransfer(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer, uint32_t cbBuffer)
{
    if (cbBuffer < sizeof (*pTransfer))
        return VERR_INVALID_PARAMETER;

    PVGASTATE pVGAState = pVdma->pVGAState;
    uint8_t * pvRam = pVGAState->vram_ptrR3;
    PGMPAGEMAPLOCK SrcLock;
    PGMPAGEMAPLOCK DstLock;
    PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;
    const void * pvSrc;
    void * pvDst;
    int rc = VINF_SUCCESS;
    uint32_t cbTransfer = pTransfer->cbTransferSize;
    uint32_t cbTransfered = 0;
    bool bSrcLocked = false;
    bool bDstLocked = false;
    do
    {
        uint32_t cbSubTransfer = cbTransfer;
        if (pTransfer->fFlags & VBOXVDMACMD_DMA_BPB_TRANSFER_F_SRC_VRAMOFFSET)
        {
            pvSrc  = pvRam + pTransfer->Src.offVramBuf + cbTransfered;
        }
        else
        {
            RTGCPHYS phPage = pTransfer->Src.phBuf;
            phPage += cbTransfered;
            rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, phPage, 0, &pvSrc, &SrcLock);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                bSrcLocked = true;
                cbSubTransfer = RT_MIN(cbSubTransfer, 0x1000);
            }
            else
            {
                break;
            }
        }

        if (pTransfer->fFlags & VBOXVDMACMD_DMA_BPB_TRANSFER_F_DST_VRAMOFFSET)
        {
            pvDst  = pvRam + pTransfer->Dst.offVramBuf + cbTransfered;
        }
        else
        {
            RTGCPHYS phPage = pTransfer->Dst.phBuf;
            phPage += cbTransfered;
            rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, phPage, 0, &pvDst, &DstLock);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                bDstLocked = true;
                cbSubTransfer = RT_MIN(cbSubTransfer, 0x1000);
            }
            else
            {
                break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            memcpy(pvDst, pvSrc, cbSubTransfer);
            cbTransfer -= cbSubTransfer;
            cbTransfered += cbSubTransfer;
        }
        else
        {
            cbTransfer = 0; /* to break */
        }

        if (bSrcLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &SrcLock);
        if (bDstLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &DstLock);
    } while (cbTransfer);

    if (RT_SUCCESS(rc))
        return sizeof (*pTransfer);
    return rc;
}

static int vboxVDMACmdExec(PVBOXVDMAHOST pVdma, const uint8_t *pvBuffer, uint32_t cbBuffer)
{
    do
    {
        Assert(pvBuffer);
        Assert(cbBuffer >= VBOXVDMACMD_HEADER_SIZE());

        if (!pvBuffer)
            return VERR_INVALID_PARAMETER;
        if (cbBuffer < VBOXVDMACMD_HEADER_SIZE())
            return VERR_INVALID_PARAMETER;

        PVBOXVDMACMD pCmd = (PVBOXVDMACMD)pvBuffer;
        uint32_t cbCmd = 0;
        switch (pCmd->enmType)
        {
            case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
#ifdef VBOXWDDM_TEST_UHGSMI
                static int count = 0;
                static uint64_t start, end;
                if (count==0)
                {
                    start = RTTimeNanoTS();
                }
                ++count;
                if (count==100000)
                {
                    end = RTTimeNanoTS();
                    float ems = (end-start)/1000000.f;
                    LogRel(("100000 calls took %i ms, %i cps\n", (int)ems, (int)(100000.f*1000.f/ems) ));
                }
#endif
                /* todo: post the buffer to chromium */
                return VINF_SUCCESS;
            }
            case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
            {
                const PVBOXVDMACMD_DMA_PRESENT_BLT pBlt = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_PRESENT_BLT);
                int cbBlt = vboxVDMACmdExecBlt(pVdma, pBlt, cbBuffer);
                Assert(cbBlt >= 0);
                Assert((uint32_t)cbBlt <= cbBuffer);
                if (cbBlt >= 0)
                {
                    if ((uint32_t)cbBlt == cbBuffer)
                        return VINF_SUCCESS;
                    else
                    {
                        cbBuffer -= (uint32_t)cbBlt;
                        pvBuffer -= cbBlt;
                    }
                }
                else
                    return cbBlt; /* error */
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER:
            {
                const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_BPB_TRANSFER);
                int cbTransfer = vboxVDMACmdExecBpbTransfer(pVdma, pTransfer, cbBuffer);
                Assert(cbTransfer >= 0);
                Assert((uint32_t)cbTransfer <= cbBuffer);
                if (cbTransfer >= 0)
                {
                    if ((uint32_t)cbTransfer == cbBuffer)
                        return VINF_SUCCESS;
                    else
                    {
                        cbBuffer -= (uint32_t)cbTransfer;
                        pvBuffer -= cbTransfer;
                    }
                }
                else
                    return cbTransfer; /* error */
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_NOP:
                return VINF_SUCCESS;
            case VBOXVDMACMD_TYPE_CHILD_STATUS_IRQ:
                return VINF_SUCCESS;
            default:
                AssertBreakpoint();
                return VERR_INVALID_FUNCTION;
        }
    } while (1);

    /* we should not be here */
    AssertBreakpoint();
    return VERR_INVALID_STATE;
}

#ifdef VBOX_VDMA_WITH_WORKERTHREAD

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
#endif

static void vboxVDMACommandProcess(PVBOXVDMAHOST pVdma, PVBOXVDMACBUF_DR pCmd, uint32_t cbCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    const uint8_t * pvBuf;
    PGMPAGEMAPLOCK Lock;
    int rc;
    bool bReleaseLocked = false;

    do
    {
        PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;

        if (pCmd->fFlags & VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR)
            pvBuf = VBOXVDMACBUF_DR_TAIL(pCmd, const uint8_t);
        else if (pCmd->fFlags & VBOXVDMACBUF_FLAG_BUF_VRAM_OFFSET)
        {
            uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
            pvBuf = pvRam + pCmd->Location.offVramBuf;
        }
        else
        {
            RTGCPHYS phPage = pCmd->Location.phBuf & ~0xfffULL;
            uint32_t offset = pCmd->Location.phBuf & 0xfff;
            Assert(offset + pCmd->cbBuf <= 0x1000);
            if (offset + pCmd->cbBuf > 0x1000)
            {
                /* @todo: more advanced mechanism of command buffer proc is actually needed */
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            const void * pvPageBuf;
            rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, phPage, 0, &pvPageBuf, &Lock);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
            {
                /* @todo: if (rc == VERR_PGM_PHYS_PAGE_RESERVED) -> fall back on using PGMPhysRead ?? */
                break;
            }

            pvBuf = (const uint8_t *)pvPageBuf;
            pvBuf += offset;

            bReleaseLocked = true;
        }

        rc = vboxVDMACmdExec(pVdma, pvBuf, pCmd->cbBuf);
        AssertRC(rc);

        if (bReleaseLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &Lock);
    } while (0);

    pCmd->rc = rc;

    rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

static void vboxVDMAControlProcess(PVBOXVDMAHOST pVdma, PVBOXVDMA_CTL pCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    pCmd->i32Result = VINF_SUCCESS;
    int rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
typedef struct
{
    struct VBOXVDMAHOST *pVdma;
    VBOXVDMAPIPE_CMD_BODY Cmd;
    bool bHasCmd;
} VBOXVDMACMD_PROCESS_CONTEXT, *PVBOXVDMACMD_PROCESS_CONTEXT;

static DECLCALLBACK(bool) vboxVDMACommandProcessCb(PVBOXVDMAPIPE pPipe, void *pvCallback)
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

static DECLCALLBACK(int) vboxVDMAWorkerThread(RTTHREAD ThreadSelf, void *pvUser)
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
#endif

#ifdef VBOX_VDMA_WITH_WATCHDOG
static DECLCALLBACK(void) vboxVDMAWatchDogTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VBOXVDMAHOST *pVdma = (VBOXVDMAHOST *)pvUser;
    PVGASTATE pVGAState = pVdma->pVGAState;
    VBVARaiseIrq(pVGAState, HGSMIHOSTFLAGS_WATCHDOG);
}

static int vboxVDMAWatchDogCtl(struct VBOXVDMAHOST *pVdma, uint32_t cMillis)
{
    PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;
    if (cMillis)
        TMTimerSetMillies(pVdma->WatchDogTimer, cMillis);
    else
        TMTimerStop(pVdma->WatchDogTimer);
    return VINF_SUCCESS;
}
#endif

int vboxVDMAConstruct(PVGASTATE pVGAState, uint32_t cPipeElements)
{
    int rc;
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)RTMemAllocZ(RT_OFFSETOF(VBOXVDMAHOST, CmdPool.aCmds[cPipeElements]));
#else
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)RTMemAllocZ(sizeof(*pVdma));
#endif
    Assert(pVdma);
    if (pVdma)
    {
        pVdma->pHgsmi = pVGAState->pHGSMI;
        pVdma->pVGAState = pVGAState;

#ifdef VBOX_VDMA_WITH_WATCHDOG
        rc = PDMDevHlpTMTimerCreate(pVGAState->pDevInsR3, TMCLOCK_REAL, vboxVDMAWatchDogTimer,
                                    pVdma, TMTIMER_FLAGS_NO_CRIT_SECT,
                                    "VDMA WatchDog Timer", &pVdma->WatchDogTimer);
        AssertRC(rc);
#endif
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
        hgsmiListInit(&pVdma->PendingList);
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
# if 0 //def VBOX_WITH_CRHGSMI
                int tmpRc = vboxVDMACrCtlHgsmiSetup(pVdma);
# endif
#endif
                pVGAState->pVdma = pVdma;
#ifdef VBOX_WITH_CRHGSMI
                int rcIgnored = vboxVDMACrCtlHgsmiSetup(pVdma); NOREF(rcIgnored); /** @todo is this ignoring intentional? */
#endif
                return VINF_SUCCESS;
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
            }

            int tmpRc = vboxVDMAPipeDestruct(&pVdma->Pipe);
            AssertRC(tmpRc);
        }

        RTMemFree(pVdma);
#endif
    }
    else
        rc = VERR_OUT_OF_RESOURCES;

    return rc;
}

int vboxVDMADestruct(struct VBOXVDMAHOST *pVdma)
{
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    /* @todo: implement*/
    AssertBreakpoint();
#endif
    RTMemFree(pVdma);
    return VINF_SUCCESS;
}

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
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
#endif

int vboxVDMASaveStateExecPrep(struct VBOXVDMAHOST *pVdma, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_CRHGSMI
    PVGASTATE pVGAState = pVdma->pVGAState;
    PVBOXVDMACMD_CHROMIUM_CTL pCmd = (PVBOXVDMACMD_CHROMIUM_CTL)vboxVDMACrCtlCreate(
            VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_BEGIN, sizeof (*pCmd));
    Assert(pCmd);
    if (pCmd)
    {
        int rc = vboxVDMACrCtlPost(pVGAState, pCmd, sizeof (*pCmd));
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(pCmd);
        }
        vboxVDMACrCtlRelease(pCmd);
        return rc;
    }
    return VERR_NO_MEMORY;
#else
    return VINF_SUCCESS;
#endif
}

int vboxVDMASaveStateExecDone(struct VBOXVDMAHOST *pVdma, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_CRHGSMI
    PVGASTATE pVGAState = pVdma->pVGAState;
    PVBOXVDMACMD_CHROMIUM_CTL pCmd = (PVBOXVDMACMD_CHROMIUM_CTL)vboxVDMACrCtlCreate(
            VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_END, sizeof (*pCmd));
    Assert(pCmd);
    if (pCmd)
    {
        int rc = vboxVDMACrCtlPost(pVGAState, pCmd, sizeof (*pCmd));
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(pCmd);
        }
        vboxVDMACrCtlRelease(pCmd);
        return rc;
    }
    return VERR_NO_MEMORY;
#else
    return VINF_SUCCESS;
#endif
}

void vboxVDMAControl(struct VBOXVDMAHOST *pVdma, PVBOXVDMA_CTL pCmd, uint32_t cbCmd)
{
#if 1
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
#ifdef VBOX_VDMA_WITH_WATCHDOG
        case VBOXVDMA_CTL_TYPE_WATCHDOG:
            pCmd->i32Result = vboxVDMAWatchDogCtl(pVdma, pCmd->u32Offset);
            break;
#endif
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

void vboxVDMACommand(struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmd, uint32_t cbCmd)
{
    int rc = VERR_NOT_IMPLEMENTED;

#ifdef VBOX_WITH_CRHGSMI
    /* chromium commands are processed by crhomium hgcm thread independently from our internal cmd processing pipeline
     * this is why we process them specially */
    rc = vboxVDMACmdCheckCrCmd(pVdma, pCmd, cbCmd);
    if (rc == VINF_SUCCESS)
        return;

    if (RT_FAILURE(rc))
    {
        pCmd->rc = rc;
        rc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmd);
        AssertRC(rc);
        return;
    }
#endif

#ifndef VBOX_VDMA_WITH_WORKERTHREAD
    vboxVDMACommandProcess(pVdma, pCmd, cbCmd);
#else

# ifdef DEBUG_misha
    Assert(0);
# endif

    VBOXVDMACMD_SUBMIT_CONTEXT Context;
    Context.pVdma = pVdma;
    Context.Cmd.enmType = VBOXVDMAPIPE_CMD_TYPE_DMACMD;
    Context.Cmd.u.pDr = pCmd;

    rc = vboxVDMAPipeModifyClient(&pVdma->Pipe, vboxVDMACommandSubmitCb, &Context);
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
#endif
}
