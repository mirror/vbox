/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Clipboard.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/GuestHost/SharedClipboard.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/cpp/ministring.h>
#include "VBoxGuestR3LibInternal.h"


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int vbglR3ClipboardSendErrorInternal(HGCMCLIENTID idClient, int rcErr);
static int vbglR3ClipboardSendURIData(HGCMCLIENTID idClient, const void *pvData, size_t cbData);
#endif


/**
 * Connects to the clipboard service.
 *
 * @returns VBox status code
 * @param   pidClient       Where to put the client id on success. The client id
 *                          must be passed to all the other clipboard calls.
 */
VBGLR3DECL(int) VbglR3ClipboardConnect(HGCMCLIENTID *pidClient)
{
    int rc = VbglR3HGCMConnect("VBoxSharedClipboard", pidClient);
    if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        rc = VINF_PERMISSION_DENIED;
    return rc;
}


/**
 * Disconnect from the clipboard service.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 */
VBGLR3DECL(int) VbglR3ClipboardDisconnect(HGCMCLIENTID idClient)
{
    return VbglR3HGCMDisconnect(idClient);
}


/**
 * Get a host message.
 *
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   pidMsg          Where to store the message id.
 * @param   pfFormats       Where to store the format(s) the message applies to.
 */
VBGLR3DECL(int) VbglR3ClipboardGetHostMsg(HGCMCLIENTID idClient, uint32_t *pidMsg, uint32_t *pfFormats)
{
    VBoxClipboardGetHostMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG, 2);
    VbglHGCMParmUInt32Set(&Msg.msg, 0);
    VbglHGCMParmUInt32Set(&Msg.formats, 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.msg, pidMsg);
        if (RT_SUCCESS(rc))
        {
            rc2 = VbglHGCMParmUInt32Get(&Msg.formats, pfFormats);
            if (RT_SUCCESS(rc2))
                return rc;
        }
        rc = rc2;
    }
    *pidMsg    = UINT32_MAX - 1;
    *pfFormats = UINT32_MAX;
    return rc;
}


/**
 * Reads data from the host clipboard.
 *
 * @returns VBox status code.
 * @retval  VINF_BUFFER_OVERFLOW    If there is more data available than the caller provided buffer space for.
 *
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format we're requesting the data in.
 * @param   pv              Where to store the data.
 * @param   cb              The size of the buffer pointed to by pv.
 * @param   pcb             The actual size of the host clipboard data. May be larger than cb.
 */
VBGLR3DECL(int) VbglR3ClipboardReadData(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb, uint32_t *pcb)
{
    VBoxClipboardReadDataMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_READ_DATA, 3);
    VbglHGCMParmUInt32Set(&Msg.format, fFormat);
    VbglHGCMParmPtrSet(&Msg.ptr, pv, cb);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        uint32_t cbActual;
        int rc2 = VbglHGCMParmUInt32Get(&Msg.size, &cbActual);
        if (RT_SUCCESS(rc2))
        {
            *pcb = cbActual;
            if (cbActual > cb)
                return VINF_BUFFER_OVERFLOW;
            return rc;
        }
        rc = rc2;
    }
    return rc;
}

/**
 * Reads the meta data header from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Where to store the read meta data header.
 */
static int vbglR3ClipboardReadMetaDataHdr(HGCMCLIENTID idClient, PVBOXDNDSNDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    VBOXDNDHGSENDDATAHDRMSG Msg;
    RT_ZERO(Msg);
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, HOST_DND_HG_SND_DATA_HDR, 12);
    Msg.uContext.SetUInt32(0);
    Msg.uFlags.SetUInt32(0);
    Msg.uScreenId.SetUInt32(0);
    Msg.cbTotal.SetUInt64(0);
    Msg.cbMeta.SetUInt32(0);
    Msg.pvMetaFmt.SetPtr(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
    Msg.cbMetaFmt.SetUInt32(0);
    Msg.cObjects.SetUInt64(0);
    Msg.enmCompression.SetUInt32(0);
    Msg.enmChecksumType.SetUInt32(0);
    Msg.pvChecksum.SetPtr(pDataHdr->pvChecksum, pDataHdr->cbChecksum);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /* Msg.uContext not needed here. */
        Msg.uFlags.GetUInt32(&pDataHdr->uFlags);
        Msg.uScreenId.GetUInt32(&pDataHdr->uScreenId);
        Msg.cbTotal.GetUInt64(&pDataHdr->cbTotal);
        Msg.cbMeta.GetUInt32(&pDataHdr->cbMeta);
        Msg.cbMetaFmt.GetUInt32(&pDataHdr->cbMetaFmt);
        Msg.cObjects.GetUInt64(&pDataHdr->cObjects);
        Msg.enmCompression.GetUInt32(&pDataHdr->enmCompression);
        Msg.enmChecksumType.GetUInt32((uint32_t *)&pDataHdr->enmChecksumType);
        Msg.cbChecksum.GetUInt32(&pDataHdr->cbChecksum);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Helper function for reading the actual clipboard meta data from the host. Do not call directly.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Where to store the data header data.
 * @param   ppvData             Returns the received meta data. Needs to be free'd by the caller.
 * @param   pcbData             Where to store the size (in bytes) of the received meta data.
 */
static int vbglR3ClipboardReadMetaDataLoop(HGCMCLIENTID idClient, PVBOXDNDSNDDATAHDR pDataHdr,
                                           void **ppvData, uint64_t *pcbData)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvData,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcbData,  VERR_INVALID_POINTER);

    int rc;
    uint32_t cbDataRecv;

    LogFlowFuncEnter();

    rc = vbglR3DnDHGRecvDataHdr(pCtx, pDataHdr);
    if (RT_FAILURE(rc))
        return rc;

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU32\n", pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects));
    if (pDataHdr->cbMeta)
    {
        uint64_t cbDataTmp = 0;
        void    *pvDataTmp = RTMemAlloc(pDataHdr->cbMeta);
        if (!pvDataTmp)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            uint8_t *pvDataOff = (uint8_t *)pvDataTmp;
            while (cbDataTmp < pDataHdr->cbMeta)
            {
                rc = vbglR3DnDHGRecvDataRaw(pCtx, pDataHdr,
                                            pvDataOff, RT_MIN(pDataHdr->cbMeta - cbDataTmp, pCtx->cbMaxChunkSize),
                                            &cbDataRecv);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("cbDataRecv=%RU32, cbDataTmp=%RU64\n", cbDataRecv, cbDataTmp));
                    Assert(cbDataTmp + cbDataRecv <= pDataHdr->cbMeta);
                    cbDataTmp += cbDataRecv;
                    pvDataOff += cbDataRecv;
                }
                else
                    break;
            }

            if (RT_SUCCESS(rc))
            {
                Assert(cbDataTmp == pDataHdr->cbMeta);

                LogFlowFunc(("Received %RU64 bytes of data\n", cbDataTmp));

                *ppvData = pvDataTmp;
                *pcbData = cbDataTmp;
            }
            else
                RTMemFree(pvDataTmp);
        }
    }
    else
    {
        *ppvData = NULL;
        *pcbData = 0;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function for reading the actual meta data from the host, extended version.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pEnmType            Where to store the meta data type. Optional.
 * @param   ppvData             Returns the received meta data. Needs to be free'd by the caller.  Optional.
 * @param   pcbData             Where to store the size (in bytes) of the received meta data. Optional.
 */
static int vbglR3ClipboardReadMetaDataMainEx(HGCMCLIENTID                 idClient,
                                             VBGLR3GUESTDNDMETADATATYPE  *pEnmType,
                                             void                       **ppvData,
                                             uint32_t                    *pcbData)
{
    /* The rest is optional. */

    VBOXDNDDATAHDR dataHdr;
    RT_ZERO(dataHdr);

    AssertMsg(pCtx->cbMaxChunkSize, ("Maximum chunk size must not be 0\n"));

    dataHdr.cbMetaFmt = pCtx->cbMaxChunkSize;
    dataHdr.pvMetaFmt = RTMemAlloc(dataHdr.cbMetaFmt);
    if (!dataHdr.pvMetaFmt)
        return VERR_NO_MEMORY;

    DnDURIList lstURI;
    DnDDroppedFiles droppedFiles;

    void    *pvData = NULL;
    uint64_t cbData = 0;

    int rc = vbglR3DnDHGRecvDataLoop(pCtx, &dataHdr, &pvData, &cbData);
    if (RT_SUCCESS(rc))
    {
        /**
         * Check if this is an URI event. If so, let VbglR3 do all the actual
         * data transfer + file/directory creation internally without letting
         * the caller know.
         *
         * This keeps the actual (guest OS-)dependent client (like VBoxClient /
         * VBoxTray) small by not having too much redundant code.
         */
        Assert(dataHdr.cbMetaFmt);
        AssertPtr(dataHdr.pvMetaFmt);
        if (DnDMIMEHasFileURLs((char *)dataHdr.pvMetaFmt, dataHdr.cbMetaFmt)) /* URI data. */
        {
            AssertPtr(pvData);
            Assert(cbData);

            rc = lstURI.SetFromURIData(pvData, cbData, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
                rc = vbglR3DnDHGRecvURIData(pCtx, &dataHdr, &droppedFiles);

            if (RT_SUCCESS(rc)) /** @todo Remove this block as soon as we hand in DnDURIList. */
            {
                if (pvData)
                {
                    /* Reuse data buffer to fill in the transformed URI file list. */
                    RTMemFree(pvData);
                    pvData = NULL;
                }

                RTCString strData = lstURI.GetRootEntries(droppedFiles.GetDirAbs());
                Assert(!strData.isEmpty());

                cbData = strData.length() + 1;
                LogFlowFunc(("URI list has %zu bytes\n", cbData));

                pvData = RTMemAlloc(cbData);
                if (pvData)
                {
                    memcpy(pvData, strData.c_str(), cbData);

                    if (pEnmType)
                        *pEnmType = VBGLR3GUESTDNDMETADATATYPE_URI_LIST;
                }
                else
                    rc =  VERR_NO_MEMORY;
            }
        }
        else /* Raw data. */
        {
            if (pEnmType)
                *pEnmType = VBGLR3GUESTDNDMETADATATYPE_RAW;
        }
    }

    if (dataHdr.pvMetaFmt)
        RTMemFree(dataHdr.pvMetaFmt);

    if (RT_SUCCESS(rc))
    {
        if (   pvData
            && cbData)
        {
            if (pcbData)
                *pcbData = cbData;
            if (ppvData)
                *ppvData = pvData;
            else
                RTMemFree(pvData);
        }
    }
    else if (   RT_FAILURE(rc)
             && rc != VERR_CANCELLED)
    {
        if (pvData)
            RTMemFree(pvData);

        int rc2 = VbglR3DnDHGSendProgress(pCtx, DND_PROGRESS_ERROR, 100 /* Percent */, rc);
        if (RT_FAILURE(rc2))
            LogFlowFunc(("Unable to send progress error %Rrc to host: %Rrc\n", rc, rc2));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function for reading the actual meta data from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pMeta               Where to store the actual meta data received from the host.
 */
static int vbglR3ClipboardReadMetaDataMain(HGCMCLIENTID idClient, PVBGLR3GUESTDNDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    int rc = vbglR3ClipboardReadMetaDataMainEx(idClient,
                                               &pMeta->enmType,
                                               &pMeta->pvMeta,
                                               &pMeta->cbMeta);
    return rc;
}

/**
 * Reports (advertises) guest clipboard formats to the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormats        The formats to advertise.
 */
VBGLR3DECL(int) VbglR3ClipboardReportFormats(HGCMCLIENTID idClient, uint32_t fFormats)
{
    VBoxClipboardWriteFormatsMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_REPORT_FORMATS, 1);
    VbglHGCMParmUInt32Set(&Msg.formats, fFormats);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Sends guest clipboard data to the host.
 *
 * This is usually called in reply to a VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message
 * from the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format of the data.
 * @param   pv              The data.
 * @param   cb              The size of the data.
 */
static int vbglR3ClipboardWriteDataRaw(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb)
{
    VBoxClipboardWriteDataMsg Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA, 2);
    VbglHGCMParmUInt32Set(&Msg.format, fFormat);
    VbglHGCMParmPtrSet(&Msg.ptr, pv, cb);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Send guest clipboard data to the host.
 *
 * This is usually called in reply to a VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message
 * from the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format of the data.
 * @param   pv              The data.
 * @param   cb              The size of the data.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteData(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb)
{
    int rc;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (fFormat == VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
    {
        rc = vbglR3ClipboardSendURIData(idClient, pv, cb);
    }
    else
#else
        RT_NOREF(fFormat);
#endif
        rc = vbglR3ClipboardWriteDataRaw(idClient, fFormat, pv, cb);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (RT_FAILURE(rc))
    {
        int rc2 = vbglR3ClipboardSendErrorInternal(idClient, rc);
        if (RT_FAILURE(rc2))
            LogFlowFunc(("Unable to send error (%Rrc) to host, rc=%Rrc\n", rc, rc2));
    }
#endif

    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Guest -> Host
 * Utility function to send clipboard data from guest to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvData              Data block to send.
 * @param   cbData              Size (in bytes) of data block to send.
 * @param   pDataHdr            Data header to use -- needed for accounting.
 */
static int vbglR3ClipboardSendDataInternal(HGCMCLIENTID idClient,
                                           void *pvData, uint64_t cbData, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pvData,   VERR_INVALID_POINTER);
    AssertReturn(cbData,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    VBoxClipboardWriteDataHdrMsg MsgHdr;
    RT_ZERO(MsgHdr);

    VBGL_HGCM_HDR_INIT(&MsgHdr.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_HDR, 12);
    MsgHdr.uContext.SetUInt32(0);                           /** @todo Not used yet. */
    MsgHdr.uFlags.SetUInt32(0);                             /** @todo Not used yet. */
    MsgHdr.uScreenId.SetUInt32(0);                          /** @todo Not used for guest->host (yet). */
    MsgHdr.cbTotal.SetUInt64(pDataHdr->cbTotal);
    MsgHdr.cbMeta.SetUInt32(pDataHdr->cbMeta);
    MsgHdr.pvMetaFmt.SetPtr(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
    MsgHdr.cbMetaFmt.SetUInt32(pDataHdr->cbMetaFmt);
    MsgHdr.cObjects.SetUInt64(pDataHdr->cObjects);
    MsgHdr.enmCompression.SetUInt32(0);                     /** @todo Not used yet. */
    MsgHdr.enmChecksumType.SetUInt32(RTDIGESTTYPE_INVALID); /** @todo Not used yet. */
    MsgHdr.pvChecksum.SetPtr(NULL, 0);                      /** @todo Not used yet. */
    MsgHdr.cbChecksum.SetUInt32(0);                         /** @todo Not used yet. */

    int rc = VbglR3HGCMCall(&MsgHdr.hdr, sizeof(MsgHdr));

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU64, rc=%Rrc\n",
                 pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects, rc));

    if (RT_SUCCESS(rc))
    {
        VBoxClipboardWriteDataChunkMsg MsgData;
        RT_ZERO(MsgData);

        VBGL_HGCM_HDR_INIT(&MsgData.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK, 5);
        MsgData.uContext.SetUInt32(0);      /** @todo Not used yet. */
        MsgData.pvChecksum.SetPtr(NULL, 0); /** @todo Not used yet. */
        MsgData.cbChecksum.SetUInt32(0);    /** @todo Not used yet. */

        uint32_t       cbCurChunk;
        const uint32_t cbMaxChunk = VBOX_SHARED_CLIPBOARD_MAX_CHUNK_SIZE;
        uint32_t       cbSent     = 0;

        HGCMFunctionParameter *pParm = &MsgData.pvData;

        while (cbSent < cbData)
        {
            cbCurChunk = RT_MIN(cbData - cbSent, cbMaxChunk);
            pParm->SetPtr(static_cast<uint8_t *>(pvData) + cbSent, cbCurChunk);

            MsgData.cbData.SetUInt32(cbCurChunk);

            rc = VbglR3HGCMCall(&MsgData.hdr, sizeof(MsgData));
            if (RT_FAILURE(rc))
                break;

            cbSent += cbCurChunk;
        }

        LogFlowFunc(("cbMaxChunk=%RU32, cbData=%RU64, cbSent=%RU32, rc=%Rrc\n",
                     cbMaxChunk, cbData, cbSent, rc));

        if (RT_SUCCESS(rc))
            Assert(cbSent == cbData);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Guest -> Host
 * Utility function to send a guest directory to the host.
 *
 * @returns IPRT status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   pObj            URI object containing the directory to send.
 */
static int vbglR3ClipboardSendDir(HGCMCLIENTID idClient, SharedClipboardURIObject *pObj)
{
    AssertPtrReturn(pObj,                                         VERR_INVALID_POINTER);
    AssertReturn(pObj->GetType() == SharedClipboardURIObject::Type_Directory, VERR_INVALID_PARAMETER);

    RTCString strPath = pObj->GetDestPathAbs();
    LogFlowFunc(("strDir=%s (%zu), fMode=0x%x\n",
                 strPath.c_str(), strPath.length(), pObj->GetMode()));

    if (strPath.length() > RTPATH_MAX)
        return VERR_INVALID_PARAMETER;

    const uint32_t cbPath = (uint32_t)strPath.length() + 1; /* Include termination. */

    VBoxClipboardWriteDirMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DIR, 4);
    /** @todo Context ID not used yet. */
    Msg.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)cbPath);
    Msg.cbName.SetUInt32((uint32_t)cbPath);
    Msg.fMode.SetUInt32(pObj->GetMode());

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Guest -> Host
 * Utility function to send a file from the guest to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pObj                URI object containing the file to send.
 */
static int vbglR3ClipboardSendFile(HGCMCLIENTID idClient, SharedClipboardURIObject *pObj)
{
    AssertPtrReturn(pObj,                                    VERR_INVALID_POINTER);
    AssertReturn(pObj->GetType() == SharedClipboardURIObject::Type_File, VERR_INVALID_PARAMETER);
    AssertReturn(pObj->IsOpen(),                             VERR_INVALID_STATE);

    uint32_t cbBuf = _64K;           /** @todo Make this configurable? */
    void *pvBuf = RTMemAlloc(cbBuf);
    if (!pvBuf)
        return VERR_NO_MEMORY;

    RTCString strPath = pObj->GetDestPathAbs();

    LogFlowFunc(("strFile=%s (%zu), cbSize=%RU64, fMode=0x%x\n", strPath.c_str(), strPath.length(),
                 pObj->GetSize(), pObj->GetMode()));

    VBoxClipboardWriteFileHdrMsg MsgHdr;
    RT_ZERO(MsgHdr);

    VBGL_HGCM_HDR_INIT(&MsgHdr.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR, 6);
    MsgHdr.uContext.SetUInt32(0);                                                    /* Context ID; unused at the moment. */
    MsgHdr.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)(strPath.length() + 1));
    MsgHdr.cbName.SetUInt32((uint32_t)(strPath.length() + 1));
    MsgHdr.uFlags.SetUInt32(0);                                                      /* Flags; unused at the moment. */
    MsgHdr.fMode.SetUInt32(pObj->GetMode());                                         /* File mode */
    MsgHdr.cbTotal.SetUInt64(pObj->GetSize());                                       /* File size (in bytes). */

    int rc = VbglR3HGCMCall(&MsgHdr.hdr, sizeof(MsgHdr));

    LogFlowFunc(("Sending file header resulted in %Rrc\n", rc));

    if (RT_SUCCESS(rc))
    {
        /*
         * Send the actual file data, chunk by chunk.
         */
        VBoxClipboardWriteFileDataMsg Msg;
        RT_ZERO(Msg);

        VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA, 5);
        Msg.uContext.SetUInt32(0);
        Msg.pvChecksum.SetPtr(NULL, 0);
        Msg.cbChecksum.SetUInt32(0);

        uint64_t cbToReadTotal  = pObj->GetSize();
        uint64_t cbWrittenTotal = 0;
        while (cbToReadTotal)
        {
            uint32_t cbToRead = RT_MIN(cbToReadTotal, cbBuf);
            uint32_t cbRead   = 0;
            if (cbToRead)
                rc = pObj->Read(pvBuf, cbToRead, &cbRead);

            LogFlowFunc(("cbToReadTotal=%RU64, cbToRead=%RU32, cbRead=%RU32, rc=%Rrc\n",
                         cbToReadTotal, cbToRead, cbRead, rc));

            if (   RT_SUCCESS(rc)
                && cbRead)
            {
                Msg.pvData.SetPtr(pvBuf, cbRead);
                Msg.cbData.SetUInt32(cbRead);
                /** @todo Calculate + set checksums. */

                rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
            }

            if (RT_FAILURE(rc))
            {
                LogFlowFunc(("Reading failed with rc=%Rrc\n", rc));
                break;
            }

            Assert(cbRead  <= cbToReadTotal);
            cbToReadTotal  -= cbRead;
            cbWrittenTotal += cbRead;

            LogFlowFunc(("%RU64/%RU64 -- %RU8%%\n", cbWrittenTotal, pObj->GetSize(), cbWrittenTotal * 100 / pObj->GetSize()));
        };
    }

    RTMemFree(pvBuf);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Guest -> Host
 * Utility function to send an URI object from guest to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pObj                URI object to send from guest to the host.
 */
static int vbglR3ClipboardSendURIObject(HGCMCLIENTID idClient, SharedClipboardURIObject *pObj)
{
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);

    int rc;

    switch (pObj->GetType())
    {
        case SharedClipboardURIObject::Type_Directory:
            rc = vbglR3ClipboardSendDir(idClient, pObj);
            break;

        case SharedClipboardURIObject::Type_File:
            rc = vbglR3ClipboardSendFile(idClient, pObj);
            break;

        default:
            AssertMsgFailed(("Object type %ld not implemented\n", pObj->GetType()));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    return rc;
}

/**
 * Guest -> Host
 * Utility function to send URI data from guest to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvData              Block to URI data to send.
 * @param   cbData              Size (in bytes) of URI data to send.
 */
static int vbglR3ClipboardSendURIData(HGCMCLIENTID idClient, const void *pvData, size_t cbData)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData,    VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths =
        RTCString((const char *)pvData, cbData).split("\r\n");

    /** @todo Add symlink support (SHAREDCLIPBOARDURILIST_FLAGS_RESOLVE_SYMLINKS) here. */
    /** @todo Add lazy loading (SHAREDCLIPBOARDURILIST_FLAGS_LAZY) here. */
    uint32_t fFlags = SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN;

    SharedClipboardURIList lstURI;
    int rc = lstURI.AppendURIPathsFromList(lstPaths, fFlags);
    if (RT_SUCCESS(rc))
    {
        /*
         * Send the (meta) data; in case of URIs it's the (non-recursive) file/directory
         * URI list the host needs to know upfront to set up the Shared Clipboard operation.
         */
        RTCString strRootDest = lstURI.GetRootEntries();
        if (strRootDest.isNotEmpty())
        {
            void *pvURIList  = (void *)strRootDest.c_str(); /* URI root list. */
            uint32_t cbURLIist = (uint32_t)strRootDest.length() + 1; /* Include string termination. */

            /* The total size also contains the size of the meta data. */
            uint64_t cbTotal  = cbURLIist;
                     cbTotal += lstURI.GetTotalBytes();

            /* We're going to send an URI list in text format. */
            const char     szMetaFmt[] = "text/uri-list";
            const uint32_t cbMetaFmt   = (uint32_t)strlen(szMetaFmt) + 1; /* Include termination. */

            VBOXCLIPBOARDDATAHDR dataHdr;
            dataHdr.uFlags    = 0; /* Flags not used yet. */
            dataHdr.cbTotal   = cbTotal;
            dataHdr.cbMeta    = cbURLIist;
            dataHdr.pvMetaFmt = (void *)szMetaFmt;
            dataHdr.cbMetaFmt = cbMetaFmt;
            dataHdr.cObjects  = lstURI.GetTotalCount();

            rc = vbglR3ClipboardSendDataInternal(idClient,
                                                 pvURIList, cbURLIist, &dataHdr);
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rc))
    {
        while (!lstURI.IsEmpty())
        {
            SharedClipboardURIObject *pNextObj = lstURI.First();

            rc = vbglR3ClipboardSendURIObject(idClient, pNextObj);
            if (RT_FAILURE(rc))
                break;

            lstURI.RemoveFirst();
        }
    }

    return rc;
}

/**
 * Guest -> Host
 * Sends an error back to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
static int vbglR3ClipboardSendErrorInternal(HGCMCLIENTID idClient, int rcErr)
{
    VBoxClipboardWriteErrorMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR, 2);
    /** @todo Context ID not used yet. */
    Msg.uContext.SetUInt32(0);
    Msg.rc.SetUInt32((uint32_t)rcErr); /* uint32_t vs. int. */

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    /*
     * Never return an error if the host did not accept the error at the current
     * time.  This can be due to the host not having any appropriate callbacks
     * set which would handle that error.
     *
     * bird: Looks like VERR_NOT_SUPPORTED is what the host will return if it
     *       doesn't an appropriate callback.  The code used to ignore ALL errors
     *       the host would return, also relevant ones.
     */
    if (RT_FAILURE(rc))
        LogFlowFunc(("Sending error %Rrc failed with rc=%Rrc\n", rcErr, rc));
    if (rc == VERR_NOT_SUPPORTED)
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Guest -> Host
 * Send an error back to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
VBGLR3DECL(int) VbglR3ClipboardSendError(HGCMCLIENTID idClient, int rcErr)
{
    return vbglR3ClipboardSendErrorInternal(idClient, rcErr);
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

