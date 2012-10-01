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
#ifndef ___VBoxMPCr_h__
#define ___VBoxMPCr_h__

#include <VBox/VBoxGuestLib.h>
#include <VBoxGuestR0LibCrOgl.h>

typedef struct VBOXMP_CRCTLCON
{
    HVBOXCRCTL hCrCtl;
    uint32_t cCrCtlRefs;
} VBOXMP_CRCTLCON, *PVBOXMP_CRCTLCON;

bool VBoxMpCrCtlConIs3DSupported();

int VBoxMpCrCtlConConnect(PVBOXMP_CRCTLCON pCrCtlCon,
        uint32_t crVersionMajor, uint32_t crVersionMinor,
        uint32_t *pu32ClientID);
int VBoxMpCrCtlConDisconnect(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID);
int VBoxMpCrCtlConCall(PVBOXMP_CRCTLCON pCrCtlCon, struct VBoxGuestHGCMCallInfo *pData, uint32_t cbData);
int VBoxMpCrCtlConCallUserData(PVBOXMP_CRCTLCON pCrCtlCon, struct VBoxGuestHGCMCallInfo *pData, uint32_t cbData);

#ifdef VBOX_WDDM_WITH_CRCMD
# include <cr_pack.h>

typedef struct VBOXMP_CRDATACON
{
    PVBOXMP_CRCTLCON pCtl;
    uint32_t u32ClientID;
} VBOXMP_CRDATACON, *PVBOXMP_CRDATACON;

DECLINLINE(int) VBoxMpCrDataConCreate(PVBOXMP_CRDATACON pDataCon, PVBOXMP_CRCTLCON pCtlCon)
{
    int rc = VBoxMpCrCtlConConnect(pCtlCon, CR_PROTOCOL_VERSION_MAJOR, CR_PROTOCOL_VERSION_MINOR, &pDataCon->u32ClientID);
    if (RT_SUCCESS(rc))
    {
        Assert(pDataCon->u32ClientID);
        pDataCon->pCtl = pCtlCon;
        return VINF_SUCCESS;
    }
    WARN(("VBoxMpCrCtlConConnect failed, rc %d", rc));
    return rc;
}

DECLINLINE(int) VBoxMpCrDataConDestroy(PVBOXMP_CRDATACON pDataCon)
{
    int rc = VBoxMpCrCtlConDisconnect(pDataCon->pCtl, pDataCon->u32ClientID);
    if (RT_SUCCESS(rc))
    {
        /* sanity */
        pDataCon->pCtl = NULL;
        pDataCon->u32ClientID = 0;
        return VINF_SUCCESS;
    }
    WARN(("VBoxMpCrCtlConDisconnect failed, rc %d", rc));
    return rc;
}

typedef struct VBOXMP_CRSHGSMICON_BUFDR
{
    uint32_t cbBuf;
    void *pvBuf;
} VBOXMP_CRSHGSMICON_BUFDR, *PVBOXMP_CRSHGSMICON_BUFDR;

typedef struct VBOXMP_CRSHGSMICON_BUFDR_CACHE
{
    volatile PVBOXMP_CRSHGSMICON_BUFDR pBufDr;
} VBOXMP_CRSHGSMICON_BUFDR_CACHE, *PVBOXMP_CRSHGSMICON_BUFDR_CACHE;

typedef struct VBOXMP_CRSHGSMITRANSPORT
{
    PVBOXMP_DEVEXT pDevExt;
    VBOXMP_CRSHGSMICON_BUFDR_CACHE WbDrCache;
} VBOXMP_CRSHGSMITRANSPORT, *PVBOXMP_CRSHGSMITRANSPORT;

/** the rx buffer passed here is only valid in the context of the callback.
 * the callee must NOT free it or use outside of the callback context.
 * */
typedef DECLCALLBACK(void) FNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEREADASYNC_COMPLETION(PVBOXMP_CRSHGSMITRANSPORT pCon, int rc, void *pvRx, uint32_t cbRx, void *pvCtx);
typedef FNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEREADASYNC_COMPLETION *PFNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEREADASYNC_COMPLETION;

typedef DECLCALLBACK(void) FNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEASYNC_COMPLETION(PVBOXMP_CRSHGSMITRANSPORT pCon, int rc, void *pvCtx);
typedef FNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEASYNC_COMPLETION *PFNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEASYNC_COMPLETION;

int VBoxMpCrShgsmiTransportCreate(PVBOXMP_CRSHGSMITRANSPORT pCon, PVBOXMP_DEVEXT pDevExt);
void VBoxMpCrShgsmiTransportTerm(PVBOXMP_CRSHGSMITRANSPORT pCon);
void* VBoxMpCrShgsmiTransportCmdCreateWriteReadAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, uint32_t u32ClientID, void *pvBuffer, uint32_t cbBuffer,
        PFNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEREADASYNC_COMPLETION pfnCompletion, uint32_t cbContextData);
void* VBoxMpCrShgsmiTransportCmdCreateWriteAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, uint32_t u32ClientID, void *pvBuffer, uint32_t cbBuffer,
        PFNVBOXMP_CRSHGSMITRANSPORT_SENDWRITEASYNC_COMPLETION pfnCompletion, uint32_t cbContextData);
int VBoxMpCrShgsmiTransportCmdSubmitWriteReadAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, void *pvContext);
int VBoxMpCrShgsmiTransportCmdSubmitWriteAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, void *pvContext);
void VBoxMpCrShgsmiTransportCmdTermWriteReadAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, void *pvContext);
void VBoxMpCrShgsmiTransportCmdTermWriteAsync(PVBOXMP_CRSHGSMITRANSPORT pCon, void *pvContext);

void* VBoxMpCrShgsmiTransportBufAlloc(PVBOXMP_CRSHGSMITRANSPORT pCon, uint32_t cbBuffer);
void VBoxMpCrShgsmiTransportBufFree(PVBOXMP_CRSHGSMITRANSPORT pCon, void* pvBuffer);

typedef struct VBOXMP_CRPACKER
{
    CRPackContext CrPacker;
    CRPackBuffer CrBuffer;
} VBOXMP_CRPACKER, *PVBOXMP_CRPACKER;

DECLINLINE(void) VBoxMpCrPackerInit(PVBOXMP_CRPACKER pPacker)
{
    memset(pPacker, 0, sizeof (*pPacker));
}

DECLINLINE(void) VBoxMpCrPackerTerm(PVBOXMP_CRPACKER pPacker)
{}

DECLINLINE(void) VBoxMpCrPackerTxBufferInit(PVBOXMP_CRPACKER pPacker, void *pvBuffer, uint32_t cbBuffer, uint32_t cCommands)
{
    crPackInitBuffer(&pPacker->CrBuffer, pvBuffer, cbBuffer, cbBuffer, cCommands);
    crPackSetBuffer(&pPacker->CrPacker, &pPacker->CrBuffer);
}

DECLINLINE(CRMessageOpcodes*) vboxMpCrPackerPrependHeader( const CRPackBuffer *pBuffer, uint32_t *cbData, void **ppvPackBuffer)
{
    UINT num_opcodes;
    CRMessageOpcodes *hdr;

    Assert(pBuffer);
    Assert(pBuffer->opcode_current < pBuffer->opcode_start);
    Assert(pBuffer->opcode_current >= pBuffer->opcode_end);
    Assert(pBuffer->data_current > pBuffer->data_start);
    Assert(pBuffer->data_current <= pBuffer->data_end);

    num_opcodes = (UINT)(pBuffer->opcode_start - pBuffer->opcode_current);
    hdr = (CRMessageOpcodes *)
        ( pBuffer->data_start - ( ( num_opcodes + 3 ) & ~0x3 ) - sizeof(*hdr) );

    Assert((void *) hdr >= pBuffer->pack);

    hdr->header.type = CR_MESSAGE_OPCODES;
    hdr->numOpcodes  = num_opcodes;

    *cbData = (UINT)(pBuffer->data_current - (unsigned char *) hdr);
    *ppvPackBuffer = pBuffer->pack;

    return hdr;
}

DECLINLINE(void*) VBoxMpCrPackerTxBufferComplete(PVBOXMP_CRPACKER pPacker, uint32_t *pcbBuffer, void **ppvPackBuffer)
{
    crPackReleaseBuffer(&pPacker->CrPacker);
    uint32_t cbData;
    CRMessageOpcodes *pHdr;
    void *pvPackBuffer;
    if (pPacker->CrBuffer.opcode_current != pPacker->CrBuffer.opcode_start)
        pHdr = vboxMpCrPackerPrependHeader(&pPacker->CrBuffer, &cbData, &pvPackBuffer);
    else
    {
        cbData = 0;
        pHdr = NULL;
        pvPackBuffer = NULL;
    }
    *pcbBuffer = cbData;
    *ppvPackBuffer = pvPackBuffer;
    return pHdr;
}

DECLINLINE(uint32_t) VBoxMpCrPackerTxBufferGetFreeBufferSize(PVBOXMP_CRPACKER pPacker)
{
    return (uint32_t)(pPacker->CrBuffer.data_end - pPacker->CrBuffer.data_start);
}

DECLINLINE(void) vboxMpCrUnpackerRxWriteback(CRMessageWriteback *pWb)
{
    int *pWriteback;
    memcpy(&pWriteback, &(pWb->writeback_ptr), sizeof (pWriteback));
    (*pWriteback)--;
}

DECLINLINE(void) vboxMpCrUnpackerRxReadback(CRMessageReadback *pRb, uint32_t cbRx)
{
    int cbPayload = cbRx - sizeof (*pRb);
    int *pWriteback;
    void *pDst;
    memcpy(&pWriteback, &(pRb->writeback_ptr), sizeof (pWriteback));
    memcpy(&pDst, &(pRb->readback_ptr), sizeof (pDst));

    (*pWriteback)--;
    memcpy(pDst, ((uint8_t*)pRb) + sizeof (*pRb), cbPayload);
}

DECLINLINE(int) VBoxMpCrUnpackerRxBufferProcess(void *pvBuffer, uint32_t cbBuffer)
{
    CRMessage *pMsg = (CRMessage*)pvBuffer;
    switch (pMsg->header.type)
    {
        case CR_MESSAGE_WRITEBACK:
            vboxMpCrUnpackerRxWriteback(&(pMsg->writeback));
            return VINF_SUCCESS;
        case CR_MESSAGE_READBACK:
            vboxMpCrUnpackerRxReadback(&(pMsg->readback), cbBuffer);
            return VINF_SUCCESS;
        default:
            WARN(("unknown msg code %d", pMsg->header.type));
            return VERR_NOT_SUPPORTED;
    }
}

#define VBOXMP_CRCMD_SIZE_WINDOWPOSITION 20
#define VBOXMP_CRCMD_SIZE_WINDOWVISIBLEREGIONS(_cRects) (16 + _cRects * 4 * sizeof(GLint))

#endif
#endif /* #ifndef ___VBoxMPCr_h__ */
