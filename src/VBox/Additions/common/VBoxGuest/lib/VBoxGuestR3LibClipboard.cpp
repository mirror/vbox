/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Shared Clipboard.
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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <iprt/dir.h>
# include <iprt/file.h>
# include <iprt/path.h>
#endif
#include <iprt/string.h>
#include <iprt/cpp/ministring.h>

#include "VBoxGuestR3LibInternal.h"


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int vbglR3ClipboardWriteErrorInternal(HGCMCLIENTID idClient, int rcErr);
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

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG, VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG);
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

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_READ_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA);
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Reads a (meta) data header from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Where to store the read meta data header.
 */
VBGLR3DECL(int) VbglR3ClipboardReadDataHdr(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    VBoxClipboardReadDataHdrMsg Msg;
    RT_ZERO(Msg);
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_FN_READ_DATA_HDR, VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA_HDR);
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
 * Sends a guest clipboard data header to the host.
 *
 * This is usually called in reply to a VBOX_SHARED_CLIPBOARD_HOST_MSG_WRITE_DATA_HDR message
 * from the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr        Pointer to data header to send.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteDataHdr(HGCMCLIENTID idClient, const PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    VBoxClipboardWriteDataHdrMsg Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_HDR, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_HDR);

    Msg.uContext.SetUInt32(0);                                          /** @todo Not used yet. */
    Msg.uFlags.SetUInt32(pDataHdr->uFlags);                             /** @todo Not used yet. */
    Msg.uScreenId.SetUInt32(pDataHdr->uScreenId);                       /** @todo Not used for guest->host (yet). */
    Msg.cbTotal.SetUInt64(pDataHdr->cbTotal);
    Msg.cbMeta.SetUInt32(pDataHdr->cbMeta);
    Msg.cbMetaFmt.SetPtr(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
    Msg.cbMetaFmt.SetUInt32(pDataHdr->cbMetaFmt);
    Msg.cObjects.SetUInt64(pDataHdr->cObjects);
    Msg.enmCompression.SetUInt32(pDataHdr->enmCompression);             /** @todo Not used yet. */
    Msg.enmChecksumType.SetUInt32(pDataHdr->enmChecksumType);           /** @todo Not used yet. */
    Msg.cbChecksum.SetPtr(pDataHdr->pvChecksum, pDataHdr->cbChecksum);  /** @todo Not used yet. */
    Msg.cbChecksum.SetUInt32(pDataHdr->cbChecksum);                     /** @todo Not used yet. */

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Reads a (meta) data chunk from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Data header to use. Need for accounting and stuff.
 * @param   pvData              Where to store the received data from the host.
 * @param   cbData              Size (in bytes) of where to store the received data.
 * @param   pcbDataRecv         Where to store the received amount of data (in bytes).
 */
static int vbglR3ClipboardReadDataChunk(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                        void *pvData, uint32_t cbData, uint32_t *pcbDataRecv)
{
    AssertPtrReturn(pDataHdr,        VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbDataRecv, VERR_INVALID_POINTER);

    LogFlowFunc(("pvDate=%p, cbData=%RU32\n", pvData, cbData));

    VBoxClipboardReadDataChunkMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_FN_READ_DATA_CHUNK, VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA_CHUNK);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        uint32_t cbDataRecv;
        rc = Msg.cbData.GetUInt32(&cbDataRecv);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            /** @todo Use checksum for validating the received data. */
            if (pcbDataRecv)
                *pcbDataRecv = cbDataRecv;
            LogFlowFuncLeaveRC(rc);
            return rc;
        }
    }

    /* failure */
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Helper function for reading the actual clipboard (meta) data from the host. Do not call directly.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Where to store the received meta data.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbRead             How much bytes were read on success. Optional.
 */
static int vbglR3ClipboardReadMetaDataLoop(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                           void *pvMeta, uint32_t cbMeta, uint32_t *pcbRead)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(pvMeta,   VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU32\n", pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects));
    if (pDataHdr->cbMeta)
    {
        uint32_t cbToRead = RT_MIN(cbMeta, pDataHdr->cbMeta);
        while (cbToRead)
        {
            uint32_t cbRead;
            rc = vbglR3ClipboardReadDataChunk(idClient, pDataHdr,
                                              (uint8_t *)pvMeta + cbReadTotal, cbToRead, &cbRead);
            if (RT_FAILURE(rc))
                break;

            Assert(cbToRead >= cbRead);
            cbToRead -= cbRead;

            cbReadTotal += cbRead;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbReadTotal;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads the actual meta data from the host, internal version.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Where to store the received meta data.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbRead             How much bytes were read on success. Optional.
 */
static int vbglR3ClipboardReadMetaDataInternal(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                               void *pvMeta, uint32_t cbMeta, uint32_t *pcbRead)
{
   LogFlowFuncEnter();

    int rc = vbglR3ClipboardReadMetaDataLoop(idClient, pDataHdr, pvMeta, cbMeta, pcbRead);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads the actual meta data from the host, extended version.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvMeta              Where to store the received meta data.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbRead             How much bytes were read on success. Optional.
 */
VBGLR3DECL(int) VbglR3ClipboardReadMetaDataEx(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                              void *pvMeta, uint32_t cbMeta, uint32_t *pcbRead)
{
    return vbglR3ClipboardReadMetaDataInternal(idClient, pDataHdr, pvMeta, cbMeta, pcbRead);
}

/**
 * Reads the actual meta data from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvMeta              Where to store the received meta data.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbRead             How much bytes were read on success. Optional.
 */
VBGLR3DECL(int) VbglR3ClipboardReadMetaData(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                            void *pvMeta, uint32_t cbMeta, uint32_t *pcbRead)
{
    return VbglR3ClipboardReadMetaDataEx(idClient, pDataHdr, pvMeta, cbMeta, pcbRead);
}

/**
 * Writes a (meta) data chunk to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Data header to use. Need for accounting and stuff.
 * @param   pvData              Where to store the received data from the host.
 * @param   cbData              Size (in bytes) of where to store the received data.
 * @param   pcbDataWritten      Where to store the written amount of data (in bytes).
 */
static int vbglR3ClipboardWriteDataChunk(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                         void *pvData, uint32_t cbData, uint32_t *pcbDataWritten)
{
    AssertPtrReturn(pDataHdr,        VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbDataWritten, VERR_INVALID_POINTER);

    LogFlowFunc(("pvDate=%p, cbData=%RU32\n", pvData, cbData));

    VBoxClipboardReadDataChunkMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_CHUNK);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        if (pcbDataWritten)
            *pcbDataWritten = cbData;
    }

    /* failure */
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Helper function for writing the actual clipboard (meta) data to the host. Do not call directly.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Meta data buffer to write.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
static int vbglR3ClipboardWriteMetaDataLoop(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                            const void *pvMeta, uint32_t cbMeta, uint32_t *pcbWritten)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(pvMeta,   VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU32\n", pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects));
    if (pDataHdr->cbMeta)
    {
        uint32_t cbToWrite = RT_MIN(cbMeta, pDataHdr->cbMeta);
        while (cbToWrite)
        {
            uint32_t cbWritten;
            rc = vbglR3ClipboardWriteDataChunk(idClient, pDataHdr,
                                               (uint8_t *)pvMeta + cbWrittenTotal, cbToWrite, &cbWritten);
            if (RT_FAILURE(rc))
                break;

            Assert(cbToWrite >= cbWritten);
            cbToWrite -= cbWritten;

            cbWrittenTotal += cbWritten;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWrittenTotal;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes the actual meta data to the host, internal version.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Meta data buffer to write.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
static int vbglR3ClipboardWriteMetaDataInternal(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                                const void *pvMeta, uint32_t cbMeta, uint32_t *pcbWritten)
{
    LogFlowFuncEnter();

    int rc = vbglR3ClipboardWriteMetaDataLoop(idClient, pDataHdr, pvMeta, cbMeta, pcbWritten);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes the actual meta data to the host, extended version.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteMetaDataEx(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                               const void *pvMeta, uint32_t cbMeta, uint32_t *pcbWritten)
{
    int rc = vbglR3ClipboardWriteMetaDataInternal(idClient, pDataHdr, pvMeta, cbMeta, pcbWritten);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes the actual meta data to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pDataHdr            Pointer to data header to use.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteMetaData(HGCMCLIENTID idClient, PVBOXCLIPBOARDDATAHDR pDataHdr,
                                             const void *pvMeta, uint32_t cbMeta, uint32_t *pcbWritten)
{
    return VbglR3ClipboardWriteMetaDataEx(idClient, pDataHdr, pvMeta, cbMeta, pcbWritten);
}

/**
 * Reads a directory entry from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pszDirname          Where to store the directory name of the directory being created.
 * @param   cbDirname           Size (in bytes) of where to store the directory name of the directory being created.
 * @param   pcbDirnameRecv      Size (in bytes) of the actual directory name received.
 * @param   pfMode              Where to store the directory creation mode.
 */
VBGLR3DECL(int) VbglR3ClipboardReadDir(HGCMCLIENTID idClient,
                                       char     *pszDirname,
                                       uint32_t  cbDirname,
                                       uint32_t *pcbDirnameRecv,
                                       uint32_t *pfMode)
{
    AssertPtrReturn(pszDirname,     VERR_INVALID_POINTER);
    AssertReturn(cbDirname,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDirnameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,         VERR_INVALID_POINTER);

    VBoxClipboardReadDirMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_READ_DIR, VBOX_SHARED_CLIPBOARD_CPARMS_READ_DIR);
    /** @todo Context ID not used yet. */
    Msg.uContext.SetUInt32(0);
    Msg.pvName.SetPtr(pszDirname, cbDirname);
    Msg.cbName.SetUInt32(cbDirname);
    Msg.fMode.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        rc = Msg.cbName.GetUInt32(pcbDirnameRecv); AssertRC(rc);
        rc = Msg.fMode.GetUInt32(pfMode);          AssertRC(rc);

        AssertReturn(cbDirname >= *pcbDirnameRecv, VERR_TOO_MUCH_DATA);
    }

    return rc;
}

/**
 * Writes a guest directory to the host.
 *
 * @returns IPRT status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   pszPath         Directory path.
 * @param   cbPath          Size (in bytes) of directory path.
 * @param   fMode           File mode for directory (IPRT-style).
 */
VBGLR3DECL(int) VbglR3ClipboardWriteDir(HGCMCLIENTID idClient,
                                        const char  *pszPath,
                                        uint32_t     cbPath,
                                        uint32_t     fMode)
{
    const size_t cchDir = strlen(pszPath);

    if (   !cchDir
        ||  cchDir >  RTPATH_MAX
        ||  cchDir != cbPath) /* UTF-8 */
        return VERR_INVALID_PARAMETER;

    const uint32_t cbPathSz = (uint32_t)cchDir + 1; /* Include termination. */

    VBoxClipboardWriteDirMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DIR, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DIR);
    /** @todo Context ID not used yet. */
    Msg.pvName.SetPtr((void *)pszPath, (uint32_t)cbPathSz);
    Msg.cbName.SetUInt32(cbPathSz);
    Msg.fMode.SetUInt32(fMode);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Receives a file header from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pszFilename         Where to store the file name of the file being transferred.
 * @param   cbFilename          Size (in bytes) of where to store the file name of the file being transferred.
 * @param   puFlags             File transfer flags. Currently not being used.
 * @param   pfMode              Where to store the file creation mode.
 * @param   pcbTotal            Where to store the file size (in bytes).
 */
VBGLR3DECL(int) VbglR3ClipboardReadFileHdr(HGCMCLIENTID  idClient,
                                           char         *pszFilename,
                                           uint32_t      cbFilename,
                                           uint32_t     *puFlags,
                                           uint32_t     *pfMode,
                                           uint64_t     *pcbTotal)
{
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(cbFilename,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(puFlags,     VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,      VERR_INVALID_POINTER);
    AssertReturn(pcbTotal,       VERR_INVALID_POINTER);

    VBoxClipboardReadFileHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_FN_READ_FILE_HDR, VBOX_SHARED_CLIPBOARD_CPARMS_READ_FILE_HDR);
    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.pvName.SetPtr(pszFilename, cbFilename);
    Msg.cbName.SetUInt32(cbFilename);
    Msg.uFlags.SetUInt32(0);
    Msg.fMode.SetUInt32(0);
    Msg.cbTotal.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Get context ID. */
        rc = Msg.uFlags.GetUInt32(puFlags);   AssertRC(rc);
        rc = Msg.fMode.GetUInt32(pfMode);     AssertRC(rc);
        rc = Msg.cbTotal.GetUInt64(pcbTotal); AssertRC(rc);
    }

    return rc;
}

/**
 * Writes a file header from the guest to the host.
 *
 * @returns VBox status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pszFilename         File name this header belong to.
 * @param   cbFilename          Size (in bytes) of file name.
 * @param   fFlags              Transfer flags; not being used and will be ignored.
 * @param   fMode               File mode.
 * @param   cbTotal             File size (in bytes).
 */
VBGLR3DECL(int) VbglR3ClipboardWriteFileHdr(HGCMCLIENTID idClient, const char *pszFilename, uint32_t cbFilename,
                                            uint32_t fFlags, uint32_t fMode, uint64_t cbTotal)
{
    RT_NOREF(fFlags);

    const size_t cchFile = strlen(pszFilename);

    if (   !cchFile
        ||  cchFile >  RTPATH_MAX
        ||  cchFile != cbFilename)
        return VERR_INVALID_PARAMETER;

    const uint32_t cbFileSz = (uint32_t)cchFile + 1; /* Include termination. */

    VBoxClipboardWriteFileHdrMsg MsgHdr;
    RT_ZERO(MsgHdr);

    VBGL_HGCM_HDR_INIT(&MsgHdr.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_FILE_HDR);
    MsgHdr.uContext.SetUInt32(0);                                                    /* Context ID; unused at the moment. */
    MsgHdr.pvName.SetPtr((void *)pszFilename, (uint32_t)(cbFileSz));
    MsgHdr.cbName.SetUInt32(cbFileSz);
    MsgHdr.uFlags.SetUInt32(0);                                                      /* Flags; unused at the moment. */
    MsgHdr.fMode.SetUInt32(fMode);                                                   /* File mode */
    MsgHdr.cbTotal.SetUInt64(cbTotal);                                               /* File size (in bytes). */

    return VbglR3HGCMCall(&MsgHdr.hdr, sizeof(MsgHdr));
}

/**
 * Reads a file data chunk from the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvData              Where to store the file data chunk.
 * @param   cbData              Size (in bytes) of where to store the data chunk.
 * @param   pcbRead             Size (in bytes) of the actual data chunk size read.
 */
VBGLR3DECL(int) VbglR3ClipboardReadFileData(HGCMCLIENTID          idClient,
                                            void                 *pvData,
                                            uint32_t              cbData,
                                            uint32_t             *pcbRead)
{
    AssertPtrReturn(pvData,  VERR_INVALID_POINTER);
    AssertReturn(cbData,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);

    VBoxClipboardReadFileDataMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_READ_FILE_DATA);
    Msg.uContext.SetUInt32(0);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        rc = Msg.cbData.GetUInt32(pcbRead); AssertRC(rc);
        AssertReturn(cbData >= *pcbRead, VERR_TOO_MUCH_DATA);
        /** @todo Add checksum support. */
    }

    return rc;
}

/**
 * Writes a file data chunk to the host.
 *
 * @returns VBox status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   pvData              Data chunk to write.
 * @param   cbData              Size (in bytes) of data chunk to write.
 * @param   pcbWritten          Returns how many bytes written.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteFileData(HGCMCLIENTID idClient, void *pvData, uint32_t cbData, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvData,     VERR_INVALID_POINTER);
    AssertReturn(cbData,        VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);

    VBoxClipboardWriteFileDataMsg MsgData;
    RT_ZERO(MsgData);

    VBGL_HGCM_HDR_INIT(&MsgData.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_READ_FILE_DATA);
    MsgData.uContext.SetUInt32(0);                                                    /* Context ID; unused at the moment. */
    MsgData.pvData.SetPtr(pvData, cbData);
    MsgData.cbData.SetUInt32(cbData);
    MsgData.pvChecksum.SetPtr(NULL, 0);
    MsgData.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&MsgData.hdr, sizeof(MsgData));
    if (RT_SUCCESS(rc))
    {
        *pcbWritten = cbData;
    }

    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

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

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_REPORT_FORMATS, VBOX_SHARED_CLIPBOARD_CPARMS_FORMATS);
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
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA);
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
    int rc  = vbglR3ClipboardWriteDataRaw(idClient, fFormat, pv, cb);
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (RT_FAILURE(rc))
    {
        int rc2 = vbglR3ClipboardWriteErrorInternal(idClient, rc);
        if (RT_FAILURE(rc2))
            LogFlowFunc(("Unable to send error (%Rrc) to host, rc=%Rrc\n", rc, rc2));
    }
#endif

    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Writes an error to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
static int vbglR3ClipboardWriteErrorInternal(HGCMCLIENTID idClient, int rcErr)
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
 * Writes an error back to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
VBGLR3DECL(int) vbglR3ClipboardWriteError(HGCMCLIENTID idClient, int rcErr)
{
    return vbglR3ClipboardWriteErrorInternal(idClient, rcErr);
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

