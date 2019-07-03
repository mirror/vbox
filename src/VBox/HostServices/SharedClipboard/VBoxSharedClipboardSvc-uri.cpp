/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal code for URI (list) handling.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/log.h>

#include <VBox/err.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-uri.h"


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern PFNHGCMSVCEXT g_pfnExtension;
extern void *g_pvExtension;

extern ClipboardClientQueue g_listClientsDeferred;


/*********************************************************************************************************************************
*   Provider implementation                                                                                                      *
*********************************************************************************************************************************/

DECLCALLBACK(int) vboxSvcClipboardURITransferOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxSvcClipboardURITransferClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxSvcClipboardURIListOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                              PVBOXCLIPBOARDLISTHDR pListHdr, PVBOXCLIPBOARDLISTHANDLE phList)
{
    RT_NOREF(pCtx, pListHdr, phList);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList)
{
    RT_NOREF(pCtx, hList);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                 VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENT pClient = (PVBOXCLIPBOARDCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PVBOXCLIPBOARDCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ);
    if (pMsg)
    {
        rc = vboxSvcClipboardMsgAdd(&pClient->pData->State, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
            rc = vboxSvcClipboardClientDeferredComplete(pClient, VINF_SUCCESS);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
        rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_READ,
                                                 30 * 1000 /* Timeout in ms */, &pPayload);
        if (RT_SUCCESS(rc))
        {
            Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDLISTHDR));
            //*ppListHdr = (PVBOXCLIPBOARDLISTHDR)pPayload->pvData;

            RTMemFree(pPayload);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                  VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                   VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    RT_NOREF(pCtx, hList, pListEntry);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pCtx->pvUser;
    AssertPtr(pClientData);

    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
    int rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_READ,
                                                 30 * 1000 /* Timeout in ms */, &pPayload);
    if (RT_SUCCESS(rc))
    {
        Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDLISTENTRY));

        PVBOXCLIPBOARDLISTENTRY pListEntry = (PVBOXCLIPBOARDLISTENTRY)pPayload->pvData;
        AssertPtr(pListEntry);

   /*     const uint32_t cbToRead = RT_MIN(cbChunk, pListEntry->cbData);

        memcpy(pvChunk, pListEntry->pvData, cbToRead);*/

        SharedClipboardURITransferPayloadFree(pPayload);

/*        if (pcbRead)
            *pcbRead = cbToRead;*/
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                    VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    RT_NOREF(pCtx, hList, pListEntry);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int vboxSvcClipboardURIObjOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const char *pszPath,
                               PVBOXCLIPBOARDCREATEPARMS pCreateParms, PSHAREDCLIPBOARDOBJHANDLE phObj)
{
    RT_NOREF(pCtx, pszPath, pCreateParms, phObj);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int vboxSvcClipboardURIObjClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj)
{
    RT_NOREF(pCtx, hObj);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int vboxSvcClipboardURIObjRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                               void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    RT_NOREF(pCtx, pCtx, hObj, pvData, cbData, fFlags, pcbRead);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int vboxSvcClipboardURIObjWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    RT_NOREF(pCtx, pCtx, hObj, pvData, cbData, fFlags, pcbWritten);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}


/*********************************************************************************************************************************
*   URI callbacks                                                                                                                *
*********************************************************************************************************************************/

DECLCALLBACK(void) VBoxSvcClipboardURITransferPrepareCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData)
{
    LogFlowFuncEnter();

    AssertPtrReturnVoid(pData);

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pData->pvUser;
    AssertPtrReturnVoid(pClientData);

    PSHAREDCLIPBOARDURITRANSFER pTransfer = pData->pTransfer;
    AssertPtrReturnVoid(pTransfer);

    /* Register needed events. */
    int rc2 = SharedClipboardURITransferEventRegister(pData->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_READ);
    AssertRC(rc2);
    rc2 = SharedClipboardURITransferEventRegister(pData->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_READ);
    AssertRC(rc2);

#if 0
    /* Tell the guest that it can start sending URI data. */
    rc2 = vboxSvcClipboardReportMsg(pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START,
                                    0 /* u32Formats == 0 means reading data */);
    AssertRC(rc2);
#endif
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc)
{
    LogFlowFuncEnter();

    RT_NOREF(pData, rc);

    LogRel2(("Shared Clipboard: Transfer complete\n"));
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferCanceledCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData)
{
    LogFlowFuncEnter();

    RT_NOREF(pData);

    LogRel2(("Shared Clipboard: Transfer canceled\n"));
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferErrorCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc)
{
    LogFlowFuncEnter();

    RT_NOREF(pData, rc);

    LogRel(("Shared Clipboard: Transfer failed with %Rrc\n", rc));
}

/**
 * Gets an URI list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListHdr            Where to store the list header.
 */
int VBoxSvcClipboardURIGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                  PVBOXCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_WRITE)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
        rc = HGCMSvcGetU64(&paParms[1], phList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[2], &pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[3], &pListHdr->cbTotalSize);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[4], &pListHdr->enmCompression);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[5], (uint32_t *)&pListHdr->enmChecksumType);

        if (RT_SUCCESS(rc))
        {
            /** @todo Validate pvMetaFmt + cbMetaFmt. */
            /** @todo Validate header checksum. */
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets an URI list header to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListHdr            Pointer to data to set to the HGCM parameters.
 */
int VBoxSvcClipboardURISetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ)
    {
        /** @todo Set pvMetaFmt + cbMetaFmt. */
        /** @todo Calculate header checksum. */

        /* Note: Context ID (paParms[0]) not used yet. */
        HGCMSvcSetU32(&paParms[1], pListHdr->fList);
        HGCMSvcSetU32(&paParms[2], pListHdr->fFeatures);
        HGCMSvcSetU32(&paParms[3], pListHdr->cbFilter);
        HGCMSvcSetPv (&paParms[4], pListHdr->pszFilter, pListHdr->cbFilter);
        HGCMSvcSetU64(&paParms[5], pListHdr->cTotalObjects);
        HGCMSvcSetU64(&paParms[6], pListHdr->cbTotalSize);
        HGCMSvcSetU32(&paParms[7], pListHdr->enmCompression);
        HGCMSvcSetU32(&paParms[8], (uint32_t)pListHdr->enmChecksumType);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets an URI list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListEntry          Where to store the list entry.
 */
int VBoxSvcClipboardURIGetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                    PVBOXCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_WRITE)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
        rc = HGCMSvcGetU64(&paParms[1], phList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pListEntry->fInfo);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &pListEntry->cbInfo);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&paParms[4], &pListEntry->pvInfo, &pListEntry->cbInfo);

        if (RT_SUCCESS(rc))
        {
            if (!SharedClipboardURIListEntryIsValid(pListEntry))
                rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets an URI data chunk to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListEntry          Pointer to data to set to the HGCM parameters.
 */
int VBoxSvcClipboardURISetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ)
    {
        /** @todo Calculate chunk checksum. */

        /* Note: Context ID (paParms[0]) not used yet. */
        HGCMSvcSetU32(&paParms[1], pListEntry->fInfo);
        HGCMSvcSetU32(&paParms[2], pListEntry->cbInfo);
        HGCMSvcSetPv (&paParms[3], pListEntry->pvInfo, pListEntry->cbInfo);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxSvcClipboardURIGetError(uint32_t cParms, VBOXHGCMSVCPARM paParms[], int *pRc)
{
    AssertPtrReturn(paParms, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pRc,     VERR_INVALID_PARAMETER);

    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_ERROR)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
        rc = HGCMSvcGetU32(&paParms[1], (uint32_t *)pRc); /** @todo int vs. uint32_t !!! */
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * URI client (guest) handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   pClient             Pointer to associated client.
 * @param   callHandle          The client's call handle of this call.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 * @param   tsArrival           Timestamp of arrival.
 */
int vboxSvcClipboardURIHandler(PVBOXCLIPBOARDCLIENT pClient,
                               VBOXHGCMCALLHANDLE callHandle,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival)
{
    RT_NOREF(paParms, tsArrival);

    LogFlowFunc(("uClient=%RU32, u32Function=%RU32, cParms=%RU32, g_pfnExtension=%p\n",
                 pClient->uClientID, u32Function, cParms, g_pfnExtension));

    const PVBOXCLIPBOARDCLIENTDATA pClientData = pClient->pData;
    AssertPtrReturn(pClientData, VERR_INVALID_POINTER);

    /* Check if we've the right mode set. */
    if (!vboxSvcClipboardURIMsgIsAllowed(vboxSvcClipboardGetMode(), u32Function))
    {
        LogFunc(("Wrong clipboard mode, denying access\n"));
        return VERR_ACCESS_DENIED;
    }

    /* A (valid) service extension is needed because VBoxSVC needs to keep track of the
     * clipboard areas cached on the host. */
    if (!g_pfnExtension)
    {
#ifdef DEBUG_andy
        AssertPtr(g_pfnExtension);
#endif
        LogFunc(("Invalid / no service extension set, skipping URI handling\n"));
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    /*
     * Pre-check: For certain messages we need to make sure that a (right) transfer is present.
     */
    PSHAREDCLIPBOARDURITRANSFER pTransfer = NULL;
    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_TRANSFER_REPORT:
            break;
        default:
        {
            if (!SharedClipboardURICtxGetTotalTransfers(&pClientData->URI))
            {
                LogFunc(("No transfers found\n"));
                rc = VERR_WRONG_ORDER;
                break;
            }

            const uint32_t uTransferID = 0; /* Only one transfer per client is supported at the moment. */

            pTransfer = SharedClipboardURICtxGetTransfer(&pClientData->URI, uTransferID);
            if (!pTransfer)
            {
                LogFunc(("Transfer with ID %RU32 not found\n", uTransferID));
                rc = VERR_WRONG_ORDER;
            }
            break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_TRANSFER_REPORT:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_TRANSFER_REPORT\n"));

            if (!SharedClipboardURICtxTransfersMaximumReached(&pClientData->URI))
            {
                SharedClipboardURICtxTransfersCleanup(&pClientData->URI);

                SHAREDCLIPBOARDURITRANSFERDIR enmDir = SHAREDCLIPBOARDURITRANSFERDIR_READ;

                PSHAREDCLIPBOARDURITRANSFER pTransfer;
                rc = SharedClipboardURITransferCreate(enmDir,
                                                      SHAREDCLIPBOARDSOURCE_REMOTE, &pTransfer);
                if (RT_SUCCESS(rc))
                {
                    rc = vboxSvcClipboardURIAreaRegister(&pClientData->State, pTransfer);
                    if (RT_SUCCESS(rc))
                    {
                        SHAREDCLIPBOARDPROVIDERCREATIONCTX creationCtx;
                        RT_ZERO(creationCtx);

                        creationCtx.enmSource = pClientData->State.enmSource;

                        RT_ZERO(creationCtx.Interface);
                        creationCtx.Interface.pfnTransferOpen  = vboxSvcClipboardURITransferOpen;
                        creationCtx.Interface.pfnTransferClose = vboxSvcClipboardURITransferClose;
                        creationCtx.Interface.pfnListOpen      = vboxSvcClipboardURIListOpen;
                        creationCtx.Interface.pfnListClose     = vboxSvcClipboardURIListClose;
                        creationCtx.Interface.pfnObjOpen       = vboxSvcClipboardURIObjOpen;
                        creationCtx.Interface.pfnObjClose      = vboxSvcClipboardURIObjClose;

                        if (enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
                        {
                            creationCtx.Interface.pfnListHdrRead   = vboxSvcClipboardURIListHdrRead;
                            creationCtx.Interface.pfnListEntryRead = vboxSvcClipboardURIListEntryRead;
                            creationCtx.Interface.pfnObjRead       = vboxSvcClipboardURIObjRead;
                        }
                        else
                        {
                            AssertFailed();
                        }

                        creationCtx.pvUser = pClient;

                        /* Register needed callbacks so that we can wait for the meta data to arrive here. */
                        SHAREDCLIPBOARDURITRANSFERCALLBACKS Callbacks;
                        RT_ZERO(Callbacks);

                        Callbacks.pvUser                = pClientData;

                        Callbacks.pfnTransferPrepare    = VBoxSvcClipboardURITransferPrepareCallback;
                        Callbacks.pfnTransferComplete   = VBoxSvcClipboardURITransferCompleteCallback;
                        Callbacks.pfnTransferCanceled   = VBoxSvcClipboardURITransferCanceledCallback;
                        Callbacks.pfnTransferError      = VBoxSvcClipboardURITransferErrorCallback;

                        SharedClipboardURITransferSetCallbacks(pTransfer, &Callbacks);

                        rc = SharedClipboardURITransferProviderCreate(pTransfer, &creationCtx);
                        if (RT_SUCCESS(rc))
                            rc = SharedClipboardURICtxTransferAdd(&pClientData->URI, pTransfer);
                    }

                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxClipboardSvcImplURITransferCreate(pClientData, pTransfer);
                        if (RT_SUCCESS(rc))
                            rc = VBoxClipboardSvcImplFormatAnnounce(pClientData, VBOX_SHARED_CLIPBOARD_FMT_URI_LIST);
                    }

                    if (RT_FAILURE(rc))
                    {
                        VBoxClipboardSvcImplURITransferDestroy(pClientData, pTransfer);
                        SharedClipboardURITransferDestroy(pTransfer);
                    }
                }
            }
            else
                rc = VERR_SHCLPB_MAX_TRANSFERS_REACHED;

            LogFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_TRANSFER_REPORT: %Rrc\n", rc));

            if (RT_FAILURE(rc))
                LogRel(("Shared Clipboard: Initializing transfer failed with %Rrc\n", rc));

            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG:
        {
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG)
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG\n"));
                rc = vboxSvcClipboardMsgGetNextInfo(&pClientData->State,
                                                    &paParms[0].u.uint32 /* uMsg */, &paParms[1].u.uint32 /* cParms */);

                /* No (new) messages available or some error occurred? */
                if (   rc == VERR_NO_DATA
                    || RT_FAILURE(rc))
                {
                    uint32_t fFlags = 0;
                    int rc2 = HGCMSvcGetU32(&paParms[2], &fFlags);
                    if (   RT_SUCCESS(rc2)
                        && fFlags) /* Blocking flag set? */
                    {
                        /* Defer client returning. */
                        rc = VINF_HGCM_ASYNC_EXECUTE;
                    }
                    else
                        rc = VERR_INVALID_PARAMETER;

                    LogFlowFunc(("Message queue is empty, returning %Rrc to guest\n", rc));
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN\n"));
            rc = VINF_SUCCESS;
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE\n"));
            rc = VINF_SUCCESS;
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_LIST_HDR\n"));

            VBOXCLIPBOARDLISTHANDLE hList;
            VBOXCLIPBOARDLISTHDR    hdrList;
            rc = VBoxSvcClipboardURIGetListHdr(cParms, paParms, &hList, &hdrList);
            if (RT_SUCCESS(rc))
            {
                if (RT_SUCCESS(rc))
                    rc = VBoxSvcClipboardURISetListHdr(cParms, paParms, &hdrList);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_LIST_HDR\n"));

            VBOXCLIPBOARDLISTHDR hdrList;
            rc = SharedClipboardURIListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                VBOXCLIPBOARDLISTHANDLE hList;
                rc = VBoxSvcClipboardURIGetListHdr(cParms, paParms, &hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListHdrDup(&hdrList);
                    uint32_t cbData = sizeof(VBOXCLIPBOARDLISTHDR);

                    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
                    rc = SharedClipboardURITransferPayloadAlloc(SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_READ,
                                                                pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferEventSignal(pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_READ,
                                                                   pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_LIST_ENTRY\n"));
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_LIST_ENTRY\n"));

            VBOXCLIPBOARDLISTENTRY entryList;
            rc = SharedClipboardURIListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                VBOXCLIPBOARDLISTHANDLE hList;
                rc = VBoxSvcClipboardURIGetListEntry(cParms, paParms, &hList, &entryList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListEntryDup(&entryList);
                    uint32_t cbData = sizeof(VBOXCLIPBOARDLISTENTRY);

                    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
                    rc = SharedClipboardURITransferPayloadAlloc(SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_READ,
                                                                pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferEventSignal(pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_READ,
                                                                   pPayload);
                    }

                }
            }
            break;
        }
#if 0
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR\n"));

            VBOXCLIPBOARDDIRDATA dirData;
            rc = VBoxSvcClipboardURIGetDir(cParms, paParms, &dirData);
            if (RT_SUCCESS(rc))
            {
                SharedClipboardArea *pArea = SharedClipboardURITransferGetArea(pTransfer);
                AssertPtrBreakStmt(pArea, rc = VERR_INVALID_POINTER);

                const char *pszCacheDir = pArea->GetDirAbs();
                char *pszDir = RTPathJoinA(pszCacheDir, dirData.pszPath);
                if (pszDir)
                {
                    LogFlowFunc(("pszDir=%s\n", pszDir));

                    rc = RTDirCreateFullPath(pszDir, dirData.fMode);
                    if (RT_SUCCESS(rc))
                    {
                        SHAREDCLIPBOARDAREAOBJ Obj = { SHAREDCLIPBOARDAREAOBJTYPE_DIR, SHAREDCLIPBOARDAREAOBJSTATE_COMPLETE };
                        int rc2 = pArea->AddObject(pszDir, Obj);
                        AssertRC(rc2);
                    }

                    RTStrFree(pszDir);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_HDR\n"));

            VBOXCLIPBOARDFILEHDR fileHdr;
            rc = VBoxSvcClipboardURISetFileHdr(cParms, paParms, &fileHdr);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR\n"));

            if (!SharedClipboardURIObjCtxIsValid(SharedClipboardURITransferGetCurrentObjCtx(pTransfer)))
            {
                pTransfer->State.ObjCtx.pObj = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File);
                if (pTransfer->State.ObjCtx.pObj) /** @todo Can this throw? */
                {
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else /* There still is another object being processed? */
                rc = VERR_WRONG_ORDER;

            if (RT_FAILURE(rc))
                break;

            VBOXCLIPBOARDFILEHDR fileHdr;
            rc = VBoxSvcClipboardURIGetFileHdr(cParms, paParms, &fileHdr);
            if (RT_SUCCESS(rc))
            {
                SharedClipboardArea *pArea = SharedClipboardURITransferGetArea(pTransfer);
                AssertPtrBreakStmt(pArea, rc = VERR_WRONG_ORDER);

                const char *pszCacheDir = pArea->GetDirAbs();

                char pszPathAbs[RTPATH_MAX];
                rc = RTPathJoin(pszPathAbs, sizeof(pszPathAbs), pszCacheDir, fileHdr.pszFilePath);
                if (RT_SUCCESS(rc))
                {
                    rc = SharedClipboardPathSanitize(pszPathAbs, sizeof(pszPathAbs));
                    if (RT_SUCCESS(rc))
                    {
                        PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx = SharedClipboardURITransferGetCurrentObjCtx(pTransfer);
                        AssertPtrBreakStmt(pObjCtx, VERR_INVALID_POINTER);

                        SharedClipboardURIObject *pObj = pObjCtx->pObj;
                        AssertPtrBreakStmt(pObj, VERR_INVALID_POINTER);

                        LogFlowFunc(("pszFile=%s\n", pszPathAbs));

                        /** @todo Add sparse file support based on fFlags? (Use Open(..., fFlags | SPARSE). */
                        rc = pObj->OpenFileEx(pszPathAbs, SharedClipboardURIObject::View_Target,
                                              RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE,
                                              (fileHdr.fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR);
                        if (RT_SUCCESS(rc))
                        {
                            rc = pObj->SetSize(fileHdr.cbSize);

                            /** @todo Unescape path before printing. */
                            LogRel2(("Clipboard: Transferring guest file '%s' to host (%RU64 bytes, mode 0x%x)\n",
                                     pObj->GetDestPathAbs().c_str(), pObj->GetSize(), pObj->GetMode()));

                            if (pObj->IsComplete()) /* 0-byte file? We're done already. */
                            {
                                /** @todo Sanitize path. */
                                LogRel2(("Clipboard: Transferring guest file '%s' (0 bytes) to host complete\n",
                                         pObj->GetDestPathAbs().c_str()));

                                SharedClipboardURIObjCtxDestroy(&pTransfer->State.ObjCtx);
                            }

                            SHAREDCLIPBOARDAREAOBJ Obj = { SHAREDCLIPBOARDAREAOBJTYPE_FILE, SHAREDCLIPBOARDAREAOBJSTATE_NONE };
                            int rc2 = pArea->AddObject(pszPathAbs, Obj);
                            AssertRC(rc2);
                        }
                        else
                            LogRel(("Clipboard: Error opening/creating guest file '%s' on host, rc=%Rrc\n", pszPathAbs, rc));
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA\n"));

            VBOXCLIPBOARDFILEDATA fileData;
            rc = VBoxSvcClipboardURISetFileData(cParms, paParms, &fileData);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA\n"));

            if (!SharedClipboardURIObjCtxIsValid(&pTransfer->State.ObjCtx))
            {
                rc = VERR_WRONG_ORDER;
                break;
            }

            VBOXCLIPBOARDFILEDATA fileData;
            rc = VBoxSvcClipboardURIGetFileData(cParms, paParms, &fileData);
            if (RT_SUCCESS(rc))
            {
                PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx = SharedClipboardURITransferGetCurrentObjCtx(pTransfer);
                AssertPtrBreakStmt(pObjCtx, VERR_INVALID_POINTER);

                SharedClipboardURIObject *pObj = pObjCtx->pObj;
                AssertPtrBreakStmt(pObj, VERR_INVALID_POINTER);

                uint32_t cbWritten;
                rc = pObj->Write(fileData.pvData, fileData.cbData, &cbWritten);
                if (RT_SUCCESS(rc))
                {
                    Assert(cbWritten <= fileData.cbData);
                    if (cbWritten < fileData.cbData)
                    {
                        /** @todo What to do when the host's disk is full? */
                        rc = VERR_DISK_FULL;
                    }

                    if (   pObj->IsComplete()
                        || RT_FAILURE(rc))
                        SharedClipboardURIObjCtxDestroy(&pTransfer->State.ObjCtx);
                }
                else
                    LogRel(("Clipboard: Error writing guest file data for '%s', rc=%Rrc\n", pObj->GetDestPathAbs().c_str(), rc));
            }
            break;
        }
#endif
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_CANCEL\n"));

            LogRel2(("Shared Clipboard: Transfer canceled\n"));
            break;

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_ERROR\n"));

            int rcGuest;
            rc = VBoxSvcClipboardURIGetError(cParms,paParms, &rcGuest);
            if (RT_SUCCESS(rc))
                LogRel(("Shared Clipboard: Transfer error: %Rrc\n", rcGuest));
            break;
        }

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    if (rc == VINF_HGCM_ASYNC_EXECUTE)
    {
        try
        {
            vboxSvcClipboardClientDefer(pClient, callHandle, u32Function, cParms, paParms);
            g_listClientsDeferred.push_back(pClient->uClientID);
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
            /* Don't report to guest. */
        }
    }
    else if (pClient)
    {
        int rc2 = vboxSvcClipboardClientComplete(pClient, callHandle, rc);
        AssertRC(rc2);
    }

    LogFlowFunc(("Returning uClient=%RU32, rc=%Rrc\n", pClient->uClientID, rc));
    return rc;
}

/**
 * URI host handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 */
int vboxSvcClipboardURIHostHandler(uint32_t u32Function,
                                   uint32_t cParms,
                                   VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(cParms, paParms);

    int rc = VERR_NOT_IMPLEMENTED; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_FN_CANCEL:
            /** @todo */
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_FN_ERROR:
            /** @todo */
            break;

        default:
            break;

    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Registers an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to register a clipboard area for.
 */
int vboxSvcClipboardURIAreaRegister(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is registered for this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc;

    if (g_pfnExtension)
    {
        VBOXCLIPBOARDEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = NIL_SHAREDCLIPBOARDAREAID;

        /* As the meta data is now complete, register a new clipboard on the host side. */
        rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_REGISTER, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
        {
            /* Note: Do *not* specify SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST as flags here, as VBoxSVC took care of the
             *       clipboard area creation already. */
            rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */,
                                            SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
        }

        LogFlowFunc(("Registered new clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                     parms.uID, pClientState->u32ClientID, rc));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Unregisters an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to unregister a clipboard area from.
 */
int vboxSvcClipboardURIAreaUnregister(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (g_pfnExtension)
    {
        VBOXCLIPBOARDEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = pTransfer->pArea->GetID();

        rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_UNREGISTER, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
        {
            rc = pTransfer->pArea->Close();
            if (RT_SUCCESS(rc))
            {
                delete pTransfer->pArea;
                pTransfer->pArea = NULL;
            }
        }

        LogFlowFunc(("Unregistered clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                     parms.uID, pClientState->u32ClientID, rc));
    }

    delete pTransfer->pArea;
    pTransfer->pArea = NULL;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Attaches to an existing (registered) URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to attach a clipboard area to.
 * @param   uID                 ID of clipboard area to to attach to. Specify 0 to attach to the most recent one.
 */
int vboxSvcClipboardURIAreaAttach(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                  SHAREDCLIPBOARDAREAID uID)
{
    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is attached to this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    if (g_pfnExtension)
    {
        VBOXCLIPBOARDEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = uID; /* 0 means most recent clipboard area. */

        /* The client now needs to attach to the most recent clipboard area
         * to keep a reference to it. The host does the actual book keeping / cleanup then.
         *
         * This might fail if the host does not have a most recent clipboard area (yet). */
        rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_ATTACH, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
            rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */);

        LogFlowFunc(("Attached client %RU32 to clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->u32ClientID, parms.uID, rc));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Detaches from an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to detach a clipboard area from.
 */
int vboxSvcClipboardURIAreaDetach(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    const uint32_t uAreaID = pTransfer->pArea->GetID();

    int rc = VINF_SUCCESS;

    if (g_pfnExtension)
    {
        VBOXCLIPBOARDEXTAREAPARMS parms;
        RT_ZERO(parms);
        parms.uID = uAreaID;

        rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_DETACH, &parms, sizeof(parms));

        LogFlowFunc(("Detached client %RU32 from clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->u32ClientID, uAreaID, rc));
    }

    delete pTransfer->pArea;
    pTransfer->pArea = NULL;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

