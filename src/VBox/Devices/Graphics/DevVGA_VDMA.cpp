/** @file
 * Video DMA (VDMA) support.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

#include <VBox/VBoxVideo3D.h>

#ifdef DEBUG_misha
#define WARN_BP() do { AssertFailed(); } while (0)
#else
#define WARN_BP() do { } while (0)
#endif
#define WARN(_msg) do { \
        LogRel(_msg); \
        WARN_BP(); \
    } while (0)

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


/* state transformations:
 *
 *   submitter   |    processor
 *   STOPPED
 *      |
 *      |
 *      >
 *  LISTENING   --->  PROCESSING
 *      ^               _/
 *      |             _/
 *      |           _/
 *      |         _/
 *      |       _/
 *      |     _/
 *      |    /
 *      <   >
 *     PAUSED
 *
 *  */
#define VBVAEXHOSTCONTEXT_STATE_STOPPED        0
#define VBVAEXHOSTCONTEXT_STATE_LISTENING      1
#define VBVAEXHOSTCONTEXT_STATE_PROCESSING     2
#define VBVAEXHOSTCONTEXT_STATE_PAUSED         3

typedef struct VBVAEXHOSTCONTEXT
{
    VBVABUFFER *pVBVA;
    uint32_t cbCurData;
    volatile uint32_t u32State;
    volatile uint32_t u32Pause;
    volatile uint32_t u32cOtherCommands;
} VBVAEXHOSTCONTEXT;

/* VBoxVBVAExHP**, i.e. processor functions, can NOT be called concurrently with each other,
 * but can be called with other VBoxVBVAExS** (submitter) functions except Init/Start/Term aparently.
 * Can only be called be the processor, i.e. the entity that acquired the processor state by direct or indirect call to the VBoxVBVAExHSCheckCommands
 * see mor edetailed comments in headers for function definitions */
static bool VBoxVBVAExHPCmdCheckRelease(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static int VBoxVBVAExHPCmdGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd);

/* VBoxVBVAExHP**, i.e. processor functions, can NOT be called concurrently with each other,
 * can be called concurrently with istelf as well as with other VBoxVBVAEx** functions except Init/Start/Term aparently */
static int VBoxVBVAExHSCheckCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva);

static void VBoxVBVAExHSInit(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static int VBoxVBVAExHSEnable(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVABUFFER *pVBVA);
static int VBoxVBVAExHSDisable(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static void VBoxVBVAExHSTerm(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static int VBoxVBVAExHSSaveState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM);
static int VBoxVBVAExHSLoadState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM, uint32_t u32Version);

typedef struct VBOXVDMAHOST
{
    PHGSMIINSTANCE pHgsmi;
    PVGASTATE pVGAState;
    VBVAEXHOSTCONTEXT CmdVbva;
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

static void vboxVDMACrCmdNotifyPerform(struct VBOXVDMAHOST *pVdma)
{
    PVGASTATE pVGAState = pVdma->pVGAState;
    pVGAState->pDrv->pfnCrCmdNotifyCmds(pVGAState->pDrv);
}

/*
 * @returns
 *
 */
static int vboxVDMACrCmdPreprocess(struct VBOXVDMAHOST *pVdma, uint8_t* pu8Cmd, uint32_t cbCmd)
{
    if (*pu8Cmd == VBOXCMDVBVA_OPTYPE_NOP)
        return VINF_EOF;

    PVBOXCMDVBVA_HDR pCmd = (PVBOXCMDVBVA_HDR)pu8Cmd;

    /* check if the command is cancelled */
    if (!ASMAtomicCmpXchgU8(&pCmd->u8State, VBOXCMDVBVA_STATE_IN_PROGRESS, VBOXCMDVBVA_STATE_SUBMITTED))
    {
        Assert(pCmd->u8State == VBOXCMDVBVA_STATE_CANCELLED);
        return VINF_EOF;
    }

    /* come commands can be handled right away? */
    switch (pCmd->u8OpCode)
    {
        case VBOXCMDVBVA_OPTYPE_NOPCMD:
            pCmd->i8Result = 0;
            return VINF_EOF;
        default:
            return VINF_SUCCESS;
    }
}

static DECLCALLBACK(int) vboxVDMACrCmdCltCmdGet(HVBOXCRCMDCLT hClt, PVBOXCMDVBVA_HDR *ppNextCmd, uint32_t *pcbNextCmd)
{
    struct VBOXVDMAHOST *pVdma = hClt;

    VBoxVBVAExHPCmdCheckRelease(&pVdma->CmdVbva);

    uint32_t cbCmd;
    uint8_t *pu8Cmd;

    for(;;)
    {
        int rc = VBoxVBVAExHPCmdGet(&pVdma->CmdVbva, &pu8Cmd, &cbCmd);
        switch (rc)
        {
            case VINF_SUCCESS:
            {
                rc = vboxVDMACrCmdPreprocess(pVdma, pu8Cmd, cbCmd);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        *ppNextCmd = (PVBOXCMDVBVA_HDR)pu8Cmd;
                        *pcbNextCmd = cbCmd;
                        return VINF_SUCCESS;
                    case VINF_EOF:
                        continue;
                    default:
                        Assert(!RT_FAILURE(rc));
                        return RT_FAILURE(rc) ? rc : VERR_INTERNAL_ERROR;
                }
                break;
            }
            case VINF_EOF:
                return VINF_EOF;
            case VINF_PERMISSION_DENIED:
                /* processing was paused, processing state was released, only VBoxVBVAExHS*** calls are now allowed */
                return VINF_EOF;
            case VINF_INTERRUPTED:
                /* command processing was interrupted, processor state remains set. client can process any commands */
                vboxVDMACrCmdNotifyPerform(pVdma);
                return VINF_EOF;
            default:
                Assert(!RT_FAILURE(rc));
                return RT_FAILURE(rc) ? rc : VERR_INTERNAL_ERROR;
        }
    }

    WARN(("Warning: vboxVDMACrCmdCltCmdGet unexpected state\n"));
    return VERR_INTERNAL_ERROR;
}

static DECLCALLBACK(int) vboxVDMACrCmdCltDmGet(HVBOXCRCMDCLT hClt, uint32_t idScreen, struct VBVAINFOSCREEN *pScreen, void **ppvVram)
{
    struct VBOXVDMAHOST *pVdma = hClt;
    PVGASTATE pVGAState = pVdma->pVGAState;

    return VBVAGetScreenInfo(pVGAState, idScreen, pScreen, ppvVram);
}

static int vboxVDMACrCtlHgsmiSetup(struct VBOXVDMAHOST *pVdma)
{
    PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP pCmd;
    pCmd = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP) vboxVDMACrCtlCreate (VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP,
                                                                          sizeof (*pCmd));
    if (pCmd)
    {
        VBOXCRCMD_CLTINFO CltInfo;
        CltInfo.hClient = pVdma;
        CltInfo.pfnCmdGet = vboxVDMACrCmdCltCmdGet;
        CltInfo.pfnDmGet = vboxVDMACrCmdCltDmGet;
        PVGASTATE pVGAState = pVdma->pVGAState;
        pCmd->pvVRamBase = pVGAState->vram_ptrR3;
        pCmd->cbVRam = pVGAState->vram_size;
        pCmd->pCrCmdClientInfo = &CltInfo;
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

    cbDmaCmd = pCmdDr->cbBuf;

    if (pCmdDr->fFlags & VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR)
    {
        if (cbCmdDr < sizeof (*pCmdDr) + VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        if (cbDmaCmd < cbCmdDr - sizeof (*pCmdDr) - VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid command buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        pDmaCmd = VBOXVDMACBUF_DR_TAIL(pCmdDr, VBOXVDMACMD);
    }
    else if (pCmdDr->fFlags & VBOXVDMACBUF_FLAG_BUF_VRAM_OFFSET)
    {
        VBOXVIDEOOFFSET offBuf = pCmdDr->Location.offVramBuf;
        if (offBuf + cbDmaCmd > pVdma->pVGAState->vram_size)
        {
            AssertMsgFailed(("invalid command buffer data from offset!"));
            return VERR_INVALID_PARAMETER;
        }
        pDmaCmd = (VBOXVDMACMD*)(pvRam + offBuf);
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
                VBoxVBVAExHSInit(&pVdma->CmdVbva);
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
    VBoxVBVAExHSTerm(&pVdma->CmdVbva);
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

/**/
static int vboxVBVAExHSProcessorAcquire(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State != VBVAEXHOSTCONTEXT_STATE_STOPPED);

    uint32_t oldState;
    if (!ASMAtomicReadU32(&pCmdVbva->u32Pause))
    {
        if (ASMAtomicCmpXchgExU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_PROCESSING, VBVAEXHOSTCONTEXT_STATE_LISTENING, &oldState))
            return VINF_SUCCESS;
        return oldState == VBVAEXHOSTCONTEXT_STATE_PROCESSING ? VERR_SEM_BUSY : VERR_INVALID_STATE;
    }
    return VERR_INVALID_STATE;
}

static bool vboxVBVAExHPCheckPause(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    if (!ASMAtomicReadU32(&pCmdVbva->u32Pause))
        return false;

    ASMAtomicWriteU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_PAUSED);
    return true;
}

static bool vboxVBVAExHPCheckOtherCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    return !!ASMAtomicUoReadU32(&pCmdVbva->u32cOtherCommands);
}

static void vboxVBVAExHPProcessorRelease(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    if (!vboxVBVAExHPCheckPause(pCmdVbva))
        ASMAtomicWriteU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_LISTENING);
    else
        ASMAtomicWriteU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_PAUSED);
}

static void vboxVBVAExHPHgEventSet(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    ASMAtomicOrU32(&pCmdVbva->pVBVA->hostFlags.u32HostEvents, VBVA_F_STATE_PROCESSING);
}

static void vboxVBVAExHPHgEventClear(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    ASMAtomicAndU32(&pCmdVbva->pVBVA->hostFlags.u32HostEvents, ~VBVA_F_STATE_PROCESSING);
}

static bool vboxVBVAExHPCmdCheckRelease(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    if (!pCmdVbva->cbCurData)
        return false;

    VBVABUFFER *pVBVA = pCmdVbva->pVBVA;
    pVBVA->off32Data = (pVBVA->off32Data + pCmdVbva->cbCurData) % pVBVA->cbData;

    pVBVA->indexRecordFirst = (pVBVA->indexRecordFirst + 1) % RT_ELEMENTS(pVBVA->aRecords);

    pCmdVbva->cbCurData = 0;

    return true;
}

static int vboxVBVAExHPCmdGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    VBVABUFFER *pVBVA = pCmdVbva->pVBVA;

    uint32_t indexRecordFirst = pVBVA->indexRecordFirst;
    uint32_t indexRecordFree = pVBVA->indexRecordFree;

    Log(("first = %d, free = %d\n",
                   indexRecordFirst, indexRecordFree));

    if (indexRecordFirst == indexRecordFree)
    {
        /* No records to process. Return without assigning output variables. */
        return VINF_EOF;
    }

    uint32_t cbRecordCurrent = ASMAtomicReadU32(&pVBVA->aRecords[indexRecordFirst].cbRecord);

    /* A new record need to be processed. */
    if (cbRecordCurrent & VBVA_F_RECORD_PARTIAL)
    {
        /* the record is being recorded, try again */
        return VINF_TRY_AGAIN;
    }

    uint32_t cbRecord = cbRecordCurrent & ~VBVA_F_RECORD_PARTIAL;

    if (!cbRecord)
    {
        /* the record is being recorded, try again */
        return VINF_TRY_AGAIN;
    }

    /* we should not get partial commands here actually */
    Assert(cbRecord);

    /* The size of largest contiguous chunk in the ring biffer. */
    uint32_t u32BytesTillBoundary = pVBVA->cbData - pVBVA->off32Data;

    /* The pointer to data in the ring buffer. */
    uint8_t *pSrc = &pVBVA->au8Data[pVBVA->off32Data];

    /* Fetch or point the data. */
    if (u32BytesTillBoundary >= cbRecord)
    {
        /* The command does not cross buffer boundary. Return address in the buffer. */
        *ppCmd = pSrc;
        *pcbCmd = cbRecord;
        pCmdVbva->cbCurData = cbRecord;
        return VINF_SUCCESS;
    }

    LogRel(("CmdVbva: cross-bound writes unsupported\n"));
    return VERR_INVALID_STATE;
}

/* Resumes command processing
 * @returns - same as VBoxVBVAExHSCheckCommands
 */
static int vboxVBVAExHSResume(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State != VBVAEXHOSTCONTEXT_STATE_STOPPED);

    ASMAtomicWriteU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_LISTENING);

    return VBoxVBVAExHSCheckCommands(pCmdVbva);
}

/* pause the command processing. this will make the processor stop the command processing and release the processing state
 * to resume the command processing the vboxVBVAExHSResume must be called */
static void vboxVBVAExHSPause(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State != VBVAEXHOSTCONTEXT_STATE_STOPPED);

    Assert(!pCmdVbva->u32Pause);

    ASMAtomicWriteU32(&pCmdVbva->u32Pause, 1);

    for(;;)
    {
        if (ASMAtomicCmpXchgU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_PAUSED, VBVAEXHOSTCONTEXT_STATE_LISTENING))
            break;

        if (ASMAtomicReadU32(&pCmdVbva->u32State) == VBVAEXHOSTCONTEXT_STATE_PAUSED)
            break;

        RTThreadSleep(2);
    }

    pCmdVbva->u32Pause = 0;
}

/* releases (completed) the command previously acquired by VBoxVBVAExHCmdGet
 * for convenience can be called if no command is currently acquired
 * in that case it will do nothing and return false.
 * if the completion notification is needed returns true. */
static bool VBoxVBVAExHPCmdCheckRelease(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    return vboxVBVAExHPCmdCheckRelease(pCmdVbva);
}

/*
 * @returns
 *  VINF_SUCCESS - new command is obtained
 *  VINF_EOF - processor has completed all commands and release the processing state, only VBoxVBVAExHS*** calls are now allowed
 *  VINF_PERMISSION_DENIED - processing was paused, processing state was released, only VBoxVBVAExHS*** calls are now allowed
 *  VINF_INTERRUPTED - command processing was interrupted, processor state remains set. client can process any commands,
 *                     and call VBoxVBVAExHPCmdGet again for further processing
 *  VERR_** - error happened, most likely guest corrupted VBVA data
 *
 */
static int VBoxVBVAExHPCmdGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd)
{
    Assert(pCmdVbva->u32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    for(;;)
    {
        if (vboxVBVAExHPCheckPause(pCmdVbva))
            return VINF_PERMISSION_DENIED;
        if (vboxVBVAExHPCheckOtherCommands(pCmdVbva))
            return VINF_INTERRUPTED;

        int rc = vboxVBVAExHPCmdGet(pCmdVbva, ppCmd, pcbCmd);
        switch (rc)
        {
            case VINF_SUCCESS:
                return VINF_SUCCESS;
            case VINF_EOF:
                vboxVBVAExHPHgEventClear(pCmdVbva);
                vboxVBVAExHPProcessorRelease(pCmdVbva);
                /* we need to prevent racing between us clearing the flag and command check/submission thread, i.e.
                 * 1. we check the queue -> and it is empty
                 * 2. submitter adds command to the queue
                 * 3. submitter checks the "processing" -> and it is true , thus it does not submit a notification
                 * 4. we clear the "processing" state
                 * 5. ->here we need to re-check the queue state to ensure we do not leak the notification of the above command
                 * 6. if the queue appears to be not-empty set the "processing" state back to "true"
                 **/
                if (VBoxVBVAExHSCheckCommands(pCmdVbva) == VINF_SUCCESS)
                    continue;
                return VINF_EOF;
            case VINF_TRY_AGAIN:
                RTThreadSleep(1);
                continue;
            default:
                /* this is something really unexpected, i.e. most likely guest has written something incorrect to the VBVA buffer */
                if (RT_FAILURE(rc))
                    return rc;

                WARN(("Warning: vboxVBVAExHCmdGet returned unexpected success status %d\n", rc));
                return VERR_INTERNAL_ERROR;
        }
    }

    WARN(("Warning: VBoxVBVAExHCmdGet unexpected state\n"));
    return VERR_INTERNAL_ERROR;
}

/* Checks whether the new commands are ready for processing
 * @returns
 *   VINF_SUCCESS - there are commands are in a queue, and the given thread is now the processor (i.e. typically it would delegate processing to a worker thread)
 *   VINF_EOF - no commands in a queue
 *   VINF_ALREADY_INITIALIZED - another thread already processing the commands
 *   VERR_INVALID_STATE - the VBVA is paused or pausing */
static int VBoxVBVAExHSCheckCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    if (ASMAtomicUoReadU32(&pCmdVbva->u32State) == VBVAEXHOSTCONTEXT_STATE_STOPPED)
        return VINF_EOF;

    int rc = vboxVBVAExHSProcessorAcquire(pCmdVbva);
    if (RT_SUCCESS(rc))
    {
        /* we are the processor now */
        VBVABUFFER *pVBVA = pCmdVbva->pVBVA;

        uint32_t indexRecordFirst = pVBVA->indexRecordFirst;
        uint32_t indexRecordFree = pVBVA->indexRecordFree;

        if (indexRecordFirst != indexRecordFree)
        {
            vboxVBVAExHPHgEventSet(pCmdVbva);
            return VINF_SUCCESS;
        }

        vboxVBVAExHPProcessorRelease(pCmdVbva);
        return VINF_EOF;
    }
    if (rc == VERR_SEM_BUSY)
        return VINF_ALREADY_INITIALIZED;
    Assert(rc == VERR_INVALID_STATE);
    return VERR_INVALID_STATE;
}

static void VBoxVBVAExHSInit(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    memset(pCmdVbva, 0, sizeof (*pCmdVbva));
}

static int VBoxVBVAExHSEnable(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVABUFFER *pVBVA)
{
    if (ASMAtomicUoReadU32(&pCmdVbva->u32State) != VBVAEXHOSTCONTEXT_STATE_STOPPED)
        return VINF_ALREADY_INITIALIZED;

    pCmdVbva->pVBVA = pVBVA;
    pCmdVbva->pVBVA->hostFlags.u32HostEvents = 0;
    ASMAtomicWriteU32(&pCmdVbva->u32State, VBVAEXHOSTCONTEXT_STATE_LISTENING);
    return VINF_SUCCESS;
}

static int VBoxVBVAExHSDisable(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    if (ASMAtomicUoReadU32(&pCmdVbva->u32State) == VBVAEXHOSTCONTEXT_STATE_STOPPED)
        return VINF_SUCCESS;

    /* ensure no commands pending and one tries to submit them */
    int rc = vboxVBVAExHSProcessorAcquire(pCmdVbva);
    if (RT_SUCCESS(rc))
    {
        pCmdVbva->pVBVA->hostFlags.u32HostEvents = 0;
        memset(pCmdVbva, 0, sizeof (*pCmdVbva));
        return VINF_SUCCESS;
    }
    return VERR_INVALID_STATE;
}

static void VBoxVBVAExHSTerm(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    /* ensure the processor is stopped */
    if (ASMAtomicUoReadU32(&pCmdVbva->u32State) == VBVAEXHOSTCONTEXT_STATE_STOPPED)
        return;

    /* ensure no one tries to submit the command */
    vboxVBVAExHSPause(pCmdVbva);
    pCmdVbva->pVBVA->hostFlags.u32HostEvents = 0;
    memset(pCmdVbva, 0, sizeof (*pCmdVbva));
}

/* Saves state
 * @returns - same as VBoxVBVAExHSCheckCommands, or failure on load state fail
 */
static int VBoxVBVAExHSSaveState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM)
{
    int rc;
    if (ASMAtomicUoReadU32(&pCmdVbva->u32State) != VBVAEXHOSTCONTEXT_STATE_STOPPED)
    {
        vboxVBVAExHSPause(pCmdVbva);
        rc = SSMR3PutU32(pSSM, (uint32_t)(((uint8_t*)pCmdVbva->pVBVA) - pu8VramBase));
        AssertRCReturn(rc, rc);
        return vboxVBVAExHSResume(pCmdVbva);
    }

    rc = SSMR3PutU32(pSSM, 0xffffffff);
    AssertRCReturn(rc, rc);

    return VINF_EOF;
}

/* Loads state
 * @returns - same as VBoxVBVAExHSCheckCommands, or failure on load state fail
 */
static int VBoxVBVAExHSLoadState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM, uint32_t u32Version)
{
    uint32_t u32;
    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);
    if (u32 != 0xffffffff)
    {
        VBVABUFFER *pVBVA = (VBVABUFFER*)pu8VramBase + u32;
        rc = VBoxVBVAExHSEnable(pCmdVbva, pVBVA);
        AssertRCReturn(rc, rc);
        return VBoxVBVAExHSCheckCommands(pCmdVbva);
    }

    return VINF_EOF;
}

int vboxCmdVBVAEnable(PVGASTATE pVGAState, VBVABUFFER *pVBVA)
{
    struct VBOXVDMAHOST *pVdma = pVGAState->pVdma;
    return VBoxVBVAExHSEnable(&pVdma->CmdVbva, pVBVA);
}

int vboxCmdVBVADisable(PVGASTATE pVGAState)
{
    struct VBOXVDMAHOST *pVdma = pVGAState->pVdma;
    return VBoxVBVAExHSDisable(&pVdma->CmdVbva);
}

static int vboxCmdVBVACmdSubmitPerform(PVGASTATE pVGAState)
{
    struct VBOXVDMAHOST *pVdma = pVGAState->pVdma;
    int rc = VBoxVBVAExHSCheckCommands(&pVdma->CmdVbva);
    switch (rc)
    {
        case VINF_SUCCESS:
            return pVGAState->pDrv->pfnCrCmdNotifyCmds(pVGAState->pDrv);
        case VINF_ALREADY_INITIALIZED:
        case VINF_EOF:
        case VERR_INVALID_STATE:
            return VINF_SUCCESS;
        default:
            Assert(!RT_FAILURE(rc));
            return RT_FAILURE(rc) ? rc : VERR_INTERNAL_ERROR;
    }
}

int vboxCmdVBVACmdSubmit(PVGASTATE pVGAState)
{
    return vboxCmdVBVACmdSubmitPerform(pVGAState);
}

int vboxCmdVBVACmdFlush(PVGASTATE pVGAState)
{
    return vboxCmdVBVACmdSubmitPerform(pVGAState);
}

void vboxCmdVBVACmdTimer(PVGASTATE pVGAState)
{
    vboxCmdVBVACmdSubmitPerform(pVGAState);
}
