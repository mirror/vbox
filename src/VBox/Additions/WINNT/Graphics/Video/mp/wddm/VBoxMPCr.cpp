/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"
#include "VBoxMPCr.h"

#include <VBox/HostServices/VBoxCrOpenGLSvc.h>

#ifdef VBOX_WITH_CROGL
#include <cr_protocol.h>

# ifdef VBOX_WDDM_WITH_CRCMD
#  include <cr_pack.h>

typedef struct VBOXMP_CRSHGSMICON_BUFDR
{
    uint32_t cbBuf;
    void *pvBuf;
} VBOXMP_CRSHGSMICON_BUFDR, *PVBOXMP_CRSHGSMICON_BUFDR;

typedef struct VBOXMP_CRSHGSMICON_BUFDR_CACHE
{
    volatile PVBOXMP_CRSHGSMICON_BUFDR pBufDr;
} VBOXMP_CRSHGSMICON_BUFDR_CACHE, *PVBOXMP_CRSHGSMICON_BUFDR_CACHE;

typedef struct VBOXMP_CRSHGSMICON
{
    PVBOXMP_DEVEXT pDevExt;
    PVBOXMP_CRCTLCON pCtlCon;
    uint32_t u32ClientID;
    VBOXMP_CRSHGSMICON_BUFDR_CACHE CmdDrCache;
    VBOXMP_CRSHGSMICON_BUFDR_CACHE WbDrCache;
} VBOXMP_CRSHGSMICON, *PVBOXMP_CRSHGSMICON;

typedef struct VBOXMP_CRSHGSMIPACKER
{
    PVBOXMP_CRSHGSMICON pCon;
    CRPackContext CrPacker;
    CRPackBuffer CrBuffer;
} VBOXMP_CRSHGSMIPACKER, *PVBOXMP_CRSHGSMIPACKER;

static void* vboxMpCrShgsmiBufferAlloc(PVBOXMP_DEVEXT pDevExt, HGSMISIZE cbData)
{
    return VBoxSHGSMIHeapBufferAlloc(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, cbData);
}

static VBOXVIDEOOFFSET vboxMpCrShgsmiBufferOffset(PVBOXMP_DEVEXT pDevExt, void *pvBuffer)
{
    return HGSMIPointerToOffset(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx.Heap.area, (const HGSMIBUFFERHEADER *)pvBuffer);
}

static void vboxMpCrShgsmiBufferFree(PVBOXMP_DEVEXT pDevExt, void *pvBuffer)
{
    VBoxSHGSMIHeapBufferFree(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pvBuffer);
}

static void* vboxMpCrShgsmiConAlloc(PVBOXMP_CRSHGSMICON pCon, uint32_t cbBuffer)
{
    return vboxMpCrShgsmiBufferAlloc(pCon->pDevExt, cbBuffer);
}

static VBOXVIDEOOFFSET vboxMpCrShgsmiConBufOffset(PVBOXMP_CRSHGSMICON pCon, void* pvBuffer)
{
    return vboxMpCrShgsmiBufferOffset(pCon->pDevExt, pvBuffer);
}


static void vboxMpCrShgsmiConFree(PVBOXMP_CRSHGSMICON pCon, void* pvBuffer)
{
    vboxMpCrShgsmiBufferFree(pCon->pDevExt, pvBuffer);
}

#define VBOXMP_CRSHGSMICON_DR_CMDBUF_OFFSET(_cBuffers) VBOXWDDM_ROUNDBOUND((VBOXVDMACMD_SIZE_FROMBODYSIZE(RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CMD, aBuffers[_cBuffers]))), 8)
#define VBOXMP_CRSHGSMICON_DR_GET_CMDBUF(_pDr, _cBuffers, _type) ((_type*)(((uint8_t*)(_pDr)) + VBOXMP_CRSHGSMICON_DR_CMDBUF_OFFSET(_cBuffers)))
#define VBOXMP_CRSHGSMICON_DR_SIZE(_cBuffers, _cbCmdBuf) ( VBOXMP_CRSHGSMICON_DR_CMDBUF_OFFSET(_cBuffers) + _cbCmdBuf)

static int vboxMpCrShgsmiBufCacheBufReinit(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache, PVBOXMP_CRSHGSMICON_BUFDR pDr, uint32_t cbRequested)
{
    if (pDr->cbBuf >= cbRequested)
        return VINF_SUCCESS;

    if (pDr->pvBuf)
        vboxMpCrShgsmiConFree(pCon, pDr->pvBuf);

    pDr->pvBuf = vboxMpCrShgsmiConAlloc(pCon, cbRequested);
    if (!pDr->pvBuf)
    {
        WARN(("vboxMpCrShgsmiConAlloc failed"));
        pDr->cbBuf = 0;
        return VERR_NO_MEMORY;
    }

    pDr->cbBuf = cbRequested;
    return VINF_SUCCESS;
}

static void vboxMpCrShgsmiBufCacheFree(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache, PVBOXMP_CRSHGSMICON_BUFDR pDr)
{
    if (ASMAtomicCmpXchgPtr(&pCache->pBufDr, pDr, NULL))
        return;

    /* the value is already cached, free the current one */
    vboxMpCrShgsmiConFree(pCon, pDr->pvBuf);
    vboxWddmMemFree(pDr);
}

static PVBOXMP_CRSHGSMICON_BUFDR vboxMpCrShgsmiBufCacheGetAllocDr(PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache)
{
    PVBOXMP_CRSHGSMICON_BUFDR pBufDr = (PVBOXMP_CRSHGSMICON_BUFDR)ASMAtomicXchgPtr((void * volatile *)&pCache->pBufDr, NULL);
    if (!pBufDr)
    {
        pBufDr = (PVBOXMP_CRSHGSMICON_BUFDR)vboxWddmMemAllocZero(sizeof (*pBufDr));
        if (!pBufDr)
        {
            WARN(("vboxWddmMemAllocZero failed!"));
            return NULL;
        }
    }
    return pBufDr;
}

static PVBOXMP_CRSHGSMICON_BUFDR vboxMpCrShgsmiBufCacheAlloc(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache, uint32_t cbBuffer)
{
    PVBOXMP_CRSHGSMICON_BUFDR pBufDr = vboxMpCrShgsmiBufCacheGetAllocDr(pCache);
    int rc = vboxMpCrShgsmiBufCacheBufReinit(pCon, pCache, pBufDr, cbBuffer);
    if (RT_SUCCESS(rc))
        return pBufDr;

    WARN(("vboxMpCrShgsmiBufCacheBufReinit failed, rc %d", rc));

    vboxMpCrShgsmiBufCacheFree(pCon, pCache, pBufDr);
    return NULL;
}

static PVBOXMP_CRSHGSMICON_BUFDR vboxMpCrShgsmiBufCacheAllocAny(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache, uint32_t cbBuffer)
{
    PVBOXMP_CRSHGSMICON_BUFDR pBufDr = vboxMpCrShgsmiBufCacheGetAllocDr(pCache);

    if (pBufDr->cbBuf)
        return pBufDr;

    int rc = vboxMpCrShgsmiBufCacheBufReinit(pCon, pCache, pBufDr, cbBuffer);
    if (RT_SUCCESS(rc))
        return pBufDr;

    WARN(("vboxMpCrShgsmiBufCacheBufReinit failed, rc %d", rc));

    vboxMpCrShgsmiBufCacheFree(pCon, pCache, pBufDr);
    return NULL;
}


static int vboxMpCrShgsmiBufCacheInit(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache)
{
    memset(pCache, 0, sizeof (*pCache));
    return VINF_SUCCESS;
}

static void vboxMpCrShgsmiBufCacheTerm(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_CRSHGSMICON_BUFDR_CACHE pCache)
{
    if (pCache->pBufDr)
        vboxMpCrShgsmiBufCacheFree(pCon, pCache, pCache->pBufDr);
}

static int vboxMpCrShgsmiConConnect(PVBOXMP_CRSHGSMICON pCon, PVBOXMP_DEVEXT pDevExt, PVBOXMP_CRCTLCON pCrCtlCon)
{
    memset(pCon, 0, sizeof (*pCon));
    int rc = vboxMpCrShgsmiBufCacheInit(pCon, &pCon->CmdDrCache);
    if (RT_SUCCESS(rc))
    {
        rc = vboxMpCrShgsmiBufCacheInit(pCon, &pCon->WbDrCache);
        if (RT_SUCCESS(rc))
        {
            rc = VBoxMpCrCtlConConnect(pCrCtlCon, CR_PROTOCOL_VERSION_MAJOR, CR_PROTOCOL_VERSION_MINOR, &pCon->u32ClientID);
            if (RT_SUCCESS(rc))
            {
                pCon->pDevExt = pDevExt;
                pCon->pCtlCon = pCrCtlCon;
                return VINF_SUCCESS;
            }
            else
            {
                WARN(("VBoxMpCrCtlConConnect failed rc %d", rc));
            }
            vboxMpCrShgsmiBufCacheTerm(pCon, &pCon->WbDrCache);
        }
        else
        {
            WARN(("vboxMpCrShgsmiBufCacheInit2 failed rc %d", rc));
        }
        vboxMpCrShgsmiBufCacheTerm(pCon, &pCon->CmdDrCache);
    }
    else
    {
        WARN(("vboxMpCrShgsmiBufCacheInit1 failed rc %d", rc));
    }

    return rc;
}

static int vboxMpCrShgsmiConDisconnect(PVBOXMP_CRSHGSMICON pCon)
{
    int rc = VBoxMpCrCtlConDisconnect(pCon->pCtlCon, pCon->u32ClientID);
    if (RT_FAILURE(rc))
    {
        WARN(("VBoxMpCrCtlConDisconnect failed rc %d", rc));
        return rc;
    }

    vboxMpCrShgsmiBufCacheTerm(pCon, &pCon->WbDrCache);
    vboxMpCrShgsmiBufCacheTerm(pCon, &pCon->CmdDrCache);

    return VINF_SUCCESS;
}

typedef DECLCALLBACK(void) FNVBOXMP_CRSHGSMICON_SEND_COMPLETION(PVBOXMP_CRSHGSMICON pCon, void *pvRx, uint32_t cbRx, void *pvCtx);
typedef FNVBOXMP_CRSHGSMICON_SEND_COMPLETION *PFNVBOXMP_CRSHGSMICON_SEND_COMPLETION;

typedef struct VBOXMP_CRSHGSMICON_SEND_COMPLETION
{
    PVBOXMP_CRSHGSMICON pCon;
    PFNVBOXMP_CRSHGSMICON_SEND_COMPLETION pvnCompletion;
    void *pvCompletion;
} VBOXMP_CRSHGSMICON_SEND_COMPLETION, *PVBOXMP_CRSHGSMICON_SEND_COMPLETION;

static DECLCALLBACK(VOID) vboxMpCrShgsmiConSendAsyncCompletion(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    /* we should be called from our DPC routine */
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PVBOXMP_CRSHGSMICON_SEND_COMPLETION pData = (PVBOXMP_CRSHGSMICON_SEND_COMPLETION)pvContext;
    PVBOXVDMACBUF_DR pDr = VBOXVDMACBUF_DR_FROM_DDI_CMD(pCmd);
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
    UINT cBufs = pBody->cBuffers;
    /* the first one is a command buffer: obtain it and get the result */

    /* if write back buffer is too small, issue read command.
     * we can use exactly the same command buffer for it */
    /* impl */
    Assert(0);
}

static int vboxMpCrShgsmiConSendAsync(PVBOXMP_CRSHGSMICON pCon, void *pvTx, uint32_t cbTx, PFNVBOXMP_CRSHGSMICON_SEND_COMPLETION pfnCompletion, void *pvCompletion)
{
    const uint32_t cBuffers = 3;
    const uint32_t cbCmd = VBOXMP_CRSHGSMICON_DR_SIZE(cBuffers, sizeof (CRVBOXHGSMIWRITEREAD));
    PVBOXMP_CRSHGSMICON_BUFDR pCmdDr = vboxMpCrShgsmiBufCacheAlloc(pCon, &pCon->CmdDrCache, cbCmd);
    if (!pCmdDr)
    {
        WARN(("vboxMpCrShgsmiBufCacheAlloc for cmd dr failed"));
        return VERR_NO_MEMORY;
    }
    PVBOXMP_CRSHGSMICON_BUFDR pWbDr = vboxMpCrShgsmiBufCacheAllocAny(pCon, &pCon->WbDrCache, 1000);
    if (!pWbDr)
    {
        WARN(("vboxMpCrShgsmiBufCacheAlloc for wb dr failed"));
        vboxMpCrShgsmiBufCacheFree(pCon, &pCon->CmdDrCache, pCmdDr);
        return VERR_NO_MEMORY;
    }

    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pCmdDr->pvBuf;
    pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
    pDr->cbBuf = cbCmd;
    pDr->rc = VERR_NOT_IMPLEMENTED;

    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    pHdr->enmType = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
    pHdr->u32CmdSpecific = 0;
    VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
    pBody->cBuffers = cBuffers;
    CRVBOXHGSMIWRITEREAD *pCmd = VBOXMP_CRSHGSMICON_DR_GET_CMDBUF(pDr, cBuffers, CRVBOXHGSMIWRITEREAD);
    pCmd->hdr.result      = VERR_WRONG_ORDER;
    pCmd->hdr.u32ClientID = pCon->u32ClientID;
    pCmd->hdr.u32Function = SHCRGL_GUEST_FN_WRITE_READ;
    //    pCmd->hdr.u32Reserved = 0;
    pCmd->iBuffer = 1;
    pCmd->iWriteback = 2;
    pCmd->cbWriteback = 0;

    VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[0];
    pBufCmd->offBuffer = vboxMpCrShgsmiConBufOffset(pCon, pCmd);
    pBufCmd->cbBuffer = sizeof (*pCmd);
    pBufCmd->u32GuestData = 0;
    pBufCmd->u64GuestData = (uint64_t)pCmdDr;

    pBufCmd = &pBody->aBuffers[1];
    pBufCmd->offBuffer = vboxMpCrShgsmiConBufOffset(pCon, pvTx);
    pBufCmd->cbBuffer = cbTx;
    pBufCmd->u32GuestData = 0;
    pBufCmd->u64GuestData = 0;

    pBufCmd = &pBody->aBuffers[2];
    pBufCmd->offBuffer = vboxMpCrShgsmiConBufOffset(pCon, pWbDr->pvBuf);
    pBufCmd->cbBuffer = pWbDr->cbBuf;
    pBufCmd->u32GuestData = 0;
    pBufCmd->u64GuestData = (uint64_t)pWbDr;

    PVBOXMP_DEVEXT pDevExt = pCon->pDevExt;
    PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
    vboxVdmaDdiCmdInit(pDdiCmd, 0, 0, vboxMpCrShgsmiConSendAsyncCompletion, pDr);
    /* mark command as submitted & invisible for the dx runtime since dx did not originate it */
    vboxVdmaDdiCmdSubmittedNotDx(pDdiCmd);
    int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
    if (RT_SUCCESS(rc))
    {
        return STATUS_SUCCESS;
    }

    /* impl failure branch */
    Assert(0);
    return rc;
}

static CRMessageOpcodes *
vboxMpCrPackerPrependHeader( CRPackBuffer *buf, unsigned int *len, unsigned int senderID )
{
    UINT num_opcodes;
    CRMessageOpcodes *hdr;

    Assert(buf);
    Assert(buf->opcode_current < buf->opcode_start);
    Assert(buf->opcode_current >= buf->opcode_end);
    Assert(buf->data_current > buf->data_start);
    Assert(buf->data_current <= buf->data_end);

    num_opcodes = (UINT)(buf->opcode_start - buf->opcode_current);
    hdr = (CRMessageOpcodes *)
        ( buf->data_start - ( ( num_opcodes + 3 ) & ~0x3 ) - sizeof(*hdr) );

    Assert((void *) hdr >= buf->pack);

    hdr->header.type = CR_MESSAGE_OPCODES;
    hdr->numOpcodes  = num_opcodes;

    *len = (UINT)(buf->data_current - (unsigned char *) hdr);

    return hdr;
}

static void vboxMpCrShgsmiPackerCbFlush(void *pvFlush)
{
    PVBOXMP_CRSHGSMIPACKER pPacker = (PVBOXMP_CRSHGSMIPACKER)pvFlush;

    crPackReleaseBuffer(&pPacker->CrPacker);

    if (pPacker->CrBuffer.opcode_current != pPacker->CrBuffer.opcode_start)
    {
        CRMessageOpcodes *pHdr;
        unsigned int len;
        pHdr = vboxMpCrPackerPrependHeader(&pPacker->CrBuffer, &len, 0);

        /*Send*/
    }


    crPackSetBuffer(&pPacker->CrPacker, &pPacker->CrBuffer);
    crPackResetPointers(&pPacker->CrPacker);
}

int vboxMpCrShgsmiPackerInit(PVBOXMP_CRSHGSMIPACKER pPacker, PVBOXMP_CRSHGSMICON pCon)
{
    memset(pPacker, 0, sizeof (*pPacker));

    static const HGSMISIZE cbBuffer = 1000;
    void *pvBuffer = vboxMpCrShgsmiConAlloc(pCon, cbBuffer);
    if (!pvBuffer)
    {
        WARN(("vboxMpCrShgsmiConAlloc failed"));
        return VERR_NO_MEMORY;
    }
    pPacker->pCon = pCon;
    crPackInitBuffer(&pPacker->CrBuffer, pvBuffer, cbBuffer, cbBuffer);
    crPackSetBuffer(&pPacker->CrPacker, &pPacker->CrBuffer);
    crPackFlushFunc(&pPacker->CrPacker, vboxMpCrShgsmiPackerCbFlush);
    crPackFlushArg(&pPacker->CrPacker, pPacker);
//    crPackSendHugeFunc( thread->packer, packspuHuge );
    return VINF_SUCCESS;
}
# endif
#endif

static int vboxMpCrCtlAddRef(PVBOXMP_CRCTLCON pCrCtlCon)
{
    if (pCrCtlCon->cCrCtlRefs++)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxCrCtlCreate(&pCrCtlCon->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        Assert(pCrCtlCon->hCrCtl);
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlCreate failed, rc (%d)", rc));

    --pCrCtlCon->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlRelease(PVBOXMP_CRCTLCON pCrCtlCon)
{
    Assert(pCrCtlCon->cCrCtlRefs);
    if (--pCrCtlCon->cCrCtlRefs)
    {
        return VINF_SUCCESS;
    }

    int rc = vboxCrCtlDestroy(pCrCtlCon->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        pCrCtlCon->hCrCtl = NULL;
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlDestroy failed, rc (%d)", rc));

    ++pCrCtlCon->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlConSetVersion(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID, uint32_t vMajor, uint32_t vMinor)
{
    CRVBOXHGCMSETVERSION parms;
    int rc;

    parms.hdr.result      = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_SET_VERSION;
    parms.hdr.cParms      = SHCRGL_CPARMS_SET_VERSION;

    parms.vMajor.type      = VMMDevHGCMParmType_32bit;
    parms.vMajor.u.value32 = vMajor;
    parms.vMinor.type      = VMMDevHGCMParmType_32bit;
    parms.vMinor.u.value32 = vMinor;

    rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, &parms.hdr, sizeof (parms));
    if (RT_FAILURE(rc))
    {
        WARN(("vboxCrCtlConCall failed, rc (%d)", rc));
        return rc;
    }

    if (RT_FAILURE(parms.hdr.result))
    {
        WARN(("version validation failed, rc (%d)", parms.hdr.result));
        return parms.hdr.result;
    }
    return VINF_SUCCESS;
}

static int vboxMpCrCtlConSetPID(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID)
{
    CRVBOXHGCMSETPID parms;
    int rc;

    parms.hdr.result      = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_SET_PID;
    parms.hdr.cParms      = SHCRGL_CPARMS_SET_PID;

    parms.u64PID.type     = VMMDevHGCMParmType_64bit;
    parms.u64PID.u.value64 = (uint64_t)PsGetCurrentProcessId();

    Assert(parms.u64PID.u.value64);

    rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, &parms.hdr, sizeof (parms));
    if (RT_FAILURE(rc))
    {
        WARN(("vboxCrCtlConCall failed, rc (%d)", rc));
        return rc;
    }

    if (RT_FAILURE(parms.hdr.result))
    {
        WARN(("set PID failed, rc (%d)", parms.hdr.result));
        return parms.hdr.result;
    }
    return VINF_SUCCESS;
}

int VBoxMpCrCtlConConnect(PVBOXMP_CRCTLCON pCrCtlCon,
        uint32_t crVersionMajor, uint32_t crVersionMinor,
        uint32_t *pu32ClientID)
{
    uint32_t u32ClientID;
    int rc = vboxMpCrCtlAddRef(pCrCtlCon);
    if (RT_SUCCESS(rc))
    {
        rc = vboxCrCtlConConnect(pCrCtlCon->hCrCtl, &u32ClientID);
        if (RT_SUCCESS(rc))
        {
            rc = vboxMpCrCtlConSetVersion(pCrCtlCon, u32ClientID, crVersionMajor, crVersionMinor);
            if (RT_SUCCESS(rc))
            {
                rc = vboxMpCrCtlConSetPID(pCrCtlCon, u32ClientID);
                if (RT_SUCCESS(rc))
                {
                    *pu32ClientID = u32ClientID;
                    return VINF_SUCCESS;
                }
                else
                {
                    WARN(("vboxMpCrCtlConSetPID failed, rc (%d)", rc));
                }
            }
            else
            {
                WARN(("vboxMpCrCtlConSetVersion failed, rc (%d)", rc));
            }
            vboxCrCtlConDisconnect(pCrCtlCon->hCrCtl, u32ClientID);
        }
        else
        {
            WARN(("vboxCrCtlConConnect failed, rc (%d)", rc));
        }
        vboxMpCrCtlRelease(pCrCtlCon);
    }
    else
    {
        WARN(("vboxMpCrCtlAddRef failed, rc (%d)", rc));
    }

    *pu32ClientID = 0;
    Assert(RT_FAILURE(rc));
    return rc;
}

int VBoxMpCrCtlConDisconnect(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID)
{
    int rc = vboxCrCtlConDisconnect(pCrCtlCon->hCrCtl, u32ClientID);
    if (RT_SUCCESS(rc))
    {
        vboxMpCrCtlRelease(pCrCtlCon);
        return VINF_SUCCESS;
    }
    else
    {
        WARN(("vboxCrCtlConDisconnect failed, rc (%d)", rc));
    }
    return rc;
}

int VBoxMpCrCtlConCall(PVBOXMP_CRCTLCON pCrCtlCon, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}

int VBoxMpCrCtlConCallUserData(PVBOXMP_CRCTLCON pCrCtlCon, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCallUserData(pCrCtlCon->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}

bool VBoxMpCrCtlConIs3DSupported()
{
#ifdef VBOX_WITH_CROGL
    VBOXMP_CRCTLCON CrCtlCon = {0};
    uint32_t u32ClientID = 0;
    int rc = VBoxMpCrCtlConConnect(&CrCtlCon, CR_PROTOCOL_VERSION_MAJOR, CR_PROTOCOL_VERSION_MINOR, &u32ClientID);
    if (RT_FAILURE(rc))
    {
        LOGREL(("VBoxMpCrCtlConConnect failed with rc(%d), 3D not supported!"));
        return false;
    }

    rc = VBoxMpCrCtlConDisconnect(&CrCtlCon, u32ClientID);
    if (RT_FAILURE(rc))
        WARN(("VBoxMpCrCtlConDisconnect failed, ignoring.."));

    return true;
#else
    return false;
#endif
}
