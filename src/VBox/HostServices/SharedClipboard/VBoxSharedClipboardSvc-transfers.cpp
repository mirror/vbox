/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal code for transfer (list) handling.
 */

/*
 * Copyright (C) 2019-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/log.h>

#include <VBox/err.h>

#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <VBox/AssertGuest.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-transfers.h"


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern uint32_t             g_fTransferMode;
extern SHCLEXTSTATE         g_ExtState;
extern PVBOXHGCMSVCHELPERS  g_pHelpers;
extern ClipboardClientMap   g_mapClients;
extern ClipboardClientQueue g_listClientsDeferred;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static int shClSvcTransferSendStatusAsync(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer, SHCLTRANSFERSTATUS uStatus, int rcTransfer, PSHCLEVENT *ppEvent);
static int shClSvcTransferSetListOpen(uint32_t cParms, VBOXHGCMSVCPARM aParms[], uint64_t idCtx, PSHCLLISTOPENPARMS pOpenParms);
static int shClSvcTransferSetListClose(uint32_t cParms, VBOXHGCMSVCPARM aParms[], uint64_t idCtx, SHCLLISTHANDLE hList);


/**
 * Destroys all transfers of a Shared Clipboard client.
 *
 * @param   pClient             Client to destroy transfers for.
 */
void shClSvcTransferDestroyAll(PSHCLCLIENT pClient)
{
    if (!pClient)
        return;

    LogFlowFuncEnter();

    /* Unregister and destroy all transfers.
     * Also make sure to let the backend know that all transfers are getting destroyed. */
    uint32_t      uIdx = 0;
    PSHCLTRANSFER pTransfer;
    while ((pTransfer = ShClTransferCtxGetTransferByIndex(&pClient->Transfers.Ctx, uIdx++)))
        ShClSvcTransferDestroy(pClient, pTransfer);
}

/**
 * Reads a root list header from the guest, asynchronous version.
 *
 * @returns VBox status code.
 * @param   pClient             Client to read from.
 * @param   idTransfer          Transfer ID to read root list header for.
 * @param   ppEvent             Where to return the event to wait for.
 *                              Must be released by the caller with ShClEventRelease().
 */
int ShClSvcTransferGHRootListReadHdrAsync(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, PSHCLEVENT *ppEvent)
{
    LogFlowFuncEnter();

    int rc;

    PSHCLCLIENTMSG pMsgHdr = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_READ,
                                             VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ_REQ);
    if (pMsgHdr)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsgHdr->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                        idTransfer, pEvent->idEvent));
            HGCMSvcSetU32(&pMsgHdr->aParms[1], 0 /* fRoots */);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsgHdr, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            /* Remove event from list if caller did not request event handle or in case
             * of failure (in this case caller should not release event). */
            if (   RT_FAILURE(rc)
                || !ppEvent)
            {
                ShClEventRelease(pEvent);
                pEvent = NULL;
            }
            else if (ppEvent)
                *ppEvent = pEvent;
        }
        else
        {
            shClSvcMsgFree(pClient, pMsgHdr);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads a root list header from the guest.
 *
 * @returns VBox status code.
 * @param   pClient             Client to read from.
 * @param   idTransfer          Transfer ID to read root list header for.
 * @param   pHdr                Where to store the root list header on succeess.
 */
int ShClSvcTransferGHRootListReadHdr(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, PSHCLLISTHDR pHdr)
{
    PSHCLEVENT pEvent;
    int rc = ShClSvcTransferGHRootListReadHdrAsync(pClient, idTransfer, &pEvent);
    if (RT_SUCCESS(rc))
    {
        PSHCLEVENTPAYLOAD pPayload;
        rc = ShClEventWait(pEvent, SHCL_TIMEOUT_DEFAULT_MS, &pPayload);
        if (RT_SUCCESS(rc))
        {
            Assert(pPayload->cbData == sizeof(SHCLLISTHDR));

            memcpy(pHdr, (PSHCLLISTHDR)pPayload->pvData, sizeof(SHCLLISTHDR));

            LogFlowFunc(("cRoots=%RU32, fFeatures=0x%x\n", pHdr->cEntries, pHdr->fFeatures));

            ShClPayloadFree(pPayload);
        }

        ShClEventRelease(pEvent);
        pEvent = NULL;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads a root list entry from the guest, asynchronous version.
 *
 * @returns VBox status code.
 * @param   pClient             Client to read from.
 * @param   idTransfer          Transfer ID to read root list header for.
 * @param   idxEntry            Index of entry to read.
 * @param   ppEvent             Where to return the event to wait for.
 *                              Must be released by the caller with ShClEventRelease().
 */
int ShClSvcTransferGHRootListReadEntryAsync(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, uint64_t idxEntry,
                                            PSHCLEVENT *ppEvent)
{
    LogFlowFuncEnter();

    PSHCLCLIENTMSG pMsgEntry = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_READ,
                                               VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ_REQ);

    PSHCLEVENT pEvent;
    int rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
    if (RT_SUCCESS(rc))
    {
        HGCMSvcSetU64(&pMsgEntry->aParms[0],
                      VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uClientID, idTransfer, pEvent->idEvent));
        HGCMSvcSetU32(&pMsgEntry->aParms[1], 0 /* fFeatures */);
        HGCMSvcSetU64(&pMsgEntry->aParms[2], idxEntry /* uIndex */);

        shClSvcClientLock(pClient);

        shClSvcMsgAdd(pClient, pMsgEntry, true /* fAppend */);
        rc = shClSvcClientWakeup(pClient);

        shClSvcClientUnlock(pClient);

        /* Remove event from list if caller did not request event handle or in case
         * of failure (in this case caller should not release event). */
        if (   RT_FAILURE(rc)
            || !ppEvent)
        {
            ShClEventRelease(pEvent);
            pEvent = NULL;
        }
        else if (ppEvent)
            *ppEvent = pEvent;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeave();
    return rc;
}

/**
 * Reads a root list entry from the guest.
 *
 * @returns VBox status code.
 * @param   pClient             Client to read from.
 * @param   idTransfer          Transfer ID to read root list header for.
 * @param   idxEntry            Index of entry to read.
 * @param   ppListEntry         Where to return the allocated root list entry.
 */
int ShClSvcTransferGHRootListReadEntry(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, uint64_t idxEntry,
                                       PSHCLLISTENTRY *ppListEntry)
{
    AssertPtrReturn(ppListEntry, VERR_INVALID_POINTER);

    PSHCLEVENT pEvent;
    int rc = ShClSvcTransferGHRootListReadEntryAsync(pClient, idTransfer, idxEntry, &pEvent);
    if (RT_SUCCESS(rc))
    {
        PSHCLEVENTPAYLOAD pPayload;
        rc = ShClEventWait(pEvent, SHCL_TIMEOUT_DEFAULT_MS, &pPayload);
        if (RT_SUCCESS(rc))
        {
            *ppListEntry = (PSHCLLISTENTRY)pPayload->pvData; /* ppLisEntry own pPayload-pvData now. */

        }

        ShClEventRelease(pEvent);
        pEvent = NULL;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   Provider interface implementation                                                                                            *
*********************************************************************************************************************************/

/** @copydoc SHCLTXPROVIDERIFACE::pfnRootListRead */
DECLCALLBACK(int) shClSvcTransferIfaceGHRootListRead(PSHCLTXPROVIDERCTX pCtx)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    SHCLTRANSFERID const idTransfer = ShClTransferGetID(pCtx->pTransfer);

    SHCLLISTHDR Hdr;
    int rc = ShClSvcTransferGHRootListReadHdr(pClient, idTransfer, &Hdr);
    if (RT_SUCCESS(rc))
    {
        for (uint64_t i = 0; i < Hdr.cEntries; i++)
        {
            PSHCLLISTENTRY pEntry;
            rc = ShClSvcTransferGHRootListReadEntry(pClient, idTransfer, i, &pEntry);
            if (RT_SUCCESS(rc))
                rc = ShClTransferListAddEntry(&pCtx->pTransfer->lstRoots, pEntry, true /* fAppend */);

            if (RT_FAILURE(rc))
                break;
        }
    }

    LogFlowFuncLeave();
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListOpen */
DECLCALLBACK(int) shClSvcTransferIfaceGHListOpen(PSHCLTXPROVIDERCTX pCtx,
                                                 PSHCLLISTOPENPARMS pOpenParms, PSHCLLISTHANDLE phList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_LIST_OPEN,
                                          VBOX_SHCL_CPARMS_LIST_OPEN);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            pMsg->idCtx = VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID, pCtx->pTransfer->State.uID,
                                                   pEvent->idEvent);

            rc = shClSvcTransferSetListOpen(pMsg->cParms, pMsg->aParms, pMsg->idCtx, pOpenParms);
            if (RT_SUCCESS(rc))
            {
                shClSvcClientLock(pClient);

                shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
                rc = shClSvcClientWakeup(pClient);

                shClSvcClientUnlock(pClient);

                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(pPayload->cbData == sizeof(SHCLREPLY));

                        PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                        AssertPtr(pReply);

                        Assert(pReply->uType == VBOX_SHCL_TX_REPLYMSGTYPE_LIST_OPEN);

                        LogFlowFunc(("hList=%RU64\n", pReply->u.ListOpen.uHandle));

                        *phList = pReply->u.ListOpen.uHandle;

                        ShClPayloadFree(pPayload);
                    }
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListClose */
DECLCALLBACK(int) shClSvcTransferIfaceGHListClose(PSHCLTXPROVIDERCTX pCtx, SHCLLISTHANDLE hList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_LIST_CLOSE,
                                          VBOX_SHCL_CPARMS_LIST_CLOSE);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            pMsg->idCtx = VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID, pCtx->pTransfer->State.uID,
                                                   pEvent->idEvent);

            rc = shClSvcTransferSetListClose(pMsg->cParms, pMsg->aParms, pMsg->idCtx, hList);
            if (RT_SUCCESS(rc))
            {
                shClSvcClientLock(pClient);

                shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
                rc = shClSvcClientWakeup(pClient);

                shClSvcClientUnlock(pClient);

                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                        ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListHdrRead */
DECLCALLBACK(int) shClSvcTransferIfaceGHListHdrRead(PSHCLTXPROVIDERCTX pCtx,
                                                    SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_READ,
                                          VBOX_SHCL_CPARMS_LIST_HDR_READ_REQ);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], hList);
            HGCMSvcSetU32(&pMsg->aParms[2], 0 /* fFlags */);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTHDR));

                    *pListHdr = *(PSHCLLISTHDR)pPayload->pvData;

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListHdrWrite */
DECLCALLBACK(int) shClSvcTransferIfaceHGListHdrWrite(PSHCLTXPROVIDERCTX pCtx,
                                                     SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListEntryRead */
DECLCALLBACK(int) shClSvcTransferIfaceGHListEntryRead(PSHCLTXPROVIDERCTX pCtx,
                                                      SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_READ,
                                          VBOX_SHCL_CPARMS_LIST_ENTRY_READ);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], hList);
            HGCMSvcSetU32(&pMsg->aParms[2], 0 /* fInfo */);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTENTRY));

                    rc = ShClTransferListEntryCopy(pListEntry, (PSHCLLISTENTRY)pPayload->pvData);

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnListEntryWrite */
DECLCALLBACK(int) shClSvcTransferIfaceHGListEntryWrite(PSHCLTXPROVIDERCTX pCtx,
                                                       SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    RT_NOREF(pCtx, hList, pListEntry);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnObjOpen */
DECLCALLBACK(int) shClSvcTransferIfaceGHObjOpen(PSHCLTXPROVIDERCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms, PSHCLOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_OPEN,
                                          VBOX_SHCL_CPARMS_OBJ_OPEN);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("pszPath=%s, fCreate=0x%x\n", pCreateParms->pszPath, pCreateParms->fCreate));

            const uint32_t cbPath = (uint32_t)strlen(pCreateParms->pszPath) + 1; /* Include terminating zero */

            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], 0); /* uHandle */
            HGCMSvcSetPv (&pMsg->aParms[2], pCreateParms->pszPath, cbPath);
            HGCMSvcSetU32(&pMsg->aParms[3], pCreateParms->fCreate);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));

                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_OPEN);

                    LogFlowFunc(("hObj=%RU64\n", pReply->u.ObjOpen.uHandle));

                    *phObj = pReply->u.ObjOpen.uHandle;

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnObjClose */
DECLCALLBACK(int) shClSvcTransferIfaceGHObjClose(PSHCLTXPROVIDERCTX pCtx, SHCLOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_CLOSE,
                                          VBOX_SHCL_CPARMS_OBJ_CLOSE);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], hObj);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));
#ifdef VBOX_STRICT
                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_CLOSE);

                    LogFlowFunc(("hObj=%RU64\n", pReply->u.ObjClose.uHandle));
#endif
                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnObjRead */
DECLCALLBACK(int) shClSvcTransferIfaceGHObjRead(PSHCLTXPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_READ,
                                          VBOX_SHCL_CPARMS_OBJ_READ_REQ);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], hObj);
            HGCMSvcSetU32(&pMsg->aParms[2], cbData);
            HGCMSvcSetU32(&pMsg->aParms[3], fFlags);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLOBJDATACHUNK));

                    PSHCLOBJDATACHUNK pDataChunk = (PSHCLOBJDATACHUNK)pPayload->pvData;
                    AssertPtr(pDataChunk);

                    const uint32_t cbRead = RT_MIN(cbData, pDataChunk->cbData);

                    memcpy(pvData, pDataChunk->pvData, cbRead);

                    if (pcbRead)
                        *pcbRead = cbRead;

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @copydoc SHCLTXPROVIDERIFACE::pfnObjWrite */
DECLCALLBACK(int) shClSvcTransferIfaceHGObjWrite(PSHCLTXPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                                 void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_WRITE,
                                          VBOX_SHCL_CPARMS_OBJ_WRITE);
    if (pMsg)
    {
        PSHCLEVENT pEvent;
        rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
        if (RT_SUCCESS(rc))
        {
            HGCMSvcSetU64(&pMsg->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, pEvent->idEvent));
            HGCMSvcSetU64(&pMsg->aParms[1], hObj);
            HGCMSvcSetU64(&pMsg->aParms[2], cbData);
            HGCMSvcSetU64(&pMsg->aParms[3], fFlags);

            shClSvcClientLock(pClient);

            shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            rc = shClSvcClientWakeup(pClient);

            shClSvcClientUnlock(pClient);

            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(pEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    const uint32_t cbRead = RT_MIN(cbData, pPayload->cbData);

                    memcpy(pvData, pPayload->pvData, cbRead);

                    if (pcbWritten)
                        *pcbWritten = cbRead;

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventRelease(pEvent);
        }
        else
        {
            shClSvcMsgFree(pClient, pMsg);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   HGCM getters / setters                                                                                                       *
*********************************************************************************************************************************/

/**
 * Returns whether a HGCM message is allowed in a certain service mode or not.
 *
 * @returns \c true if message is allowed, \c false if not.
 * @param   uMode               Service mode to check allowance for.
 * @param   uMsg                HGCM message to check allowance for.
 */
bool shClSvcTransferMsgIsAllowed(uint32_t uMode, uint32_t uMsg)
{
    const bool fHostToGuest =    uMode == VBOX_SHCL_MODE_HOST_TO_GUEST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    const bool fGuestToHost =    uMode == VBOX_SHCL_MODE_GUEST_TO_HOST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    bool fAllowed = false; /* If in doubt, don't allow. */

    switch (uMsg)
    {
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_WRITE:
            fAllowed = fGuestToHost;
            break;

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_READ:
            fAllowed = fHostToGuest;
            break;

        case VBOX_SHCL_GUEST_FN_CONNECT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_NEGOTIATE_CHUNK_SIZE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_REPORT_FEATURES:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_QUERY_FEATURES:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_GET:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_REPLY:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ERROR:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_CLOSE:
            fAllowed = fHostToGuest || fGuestToHost;
            break;

        default:
            break;
    }

    LogFlowFunc(("uMsg=%RU32 (%s), uMode=%RU32 -> fAllowed=%RTbool\n", uMsg, ShClGuestMsgToStr(uMsg), uMode, fAllowed));
    return fAllowed;
}

/**
 * Gets a transfer message reply from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pReply              Where to store the reply.
 */
static int shClSvcTransferGetReply(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                   PSHCLREPLY pReply)
{
    int rc;

    if (cParms >= VBOX_SHCL_CPARMS_REPLY_MIN)
    {
        /* aParms[0] has the context ID. */
        rc = HGCMSvcGetU32(&aParms[1], &pReply->uType);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&aParms[2], &pReply->rc);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&aParms[3], &pReply->pvPayload, &pReply->cbPayload);

        if (RT_SUCCESS(rc))
        {
            rc = VERR_INVALID_PARAMETER; /* Play safe. */

            const unsigned idxParm = VBOX_SHCL_CPARMS_REPLY_MIN;

            switch (pReply->uType)
            {
                case VBOX_SHCL_TX_REPLYMSGTYPE_TRANSFER_STATUS:
                {
                    if (cParms > idxParm)
                        rc = HGCMSvcGetU32(&aParms[idxParm], &pReply->u.TransferStatus.uStatus);

                    LogFlowFunc(("uTransferStatus=%RU32\n", pReply->u.TransferStatus.uStatus));
                    break;
                }

                case VBOX_SHCL_TX_REPLYMSGTYPE_LIST_OPEN:
                {
                    if (cParms > idxParm)
                        rc = HGCMSvcGetU64(&aParms[idxParm], &pReply->u.ListOpen.uHandle);

                    LogFlowFunc(("hListOpen=%RU64\n", pReply->u.ListOpen.uHandle));
                    break;
                }

                case VBOX_SHCL_TX_REPLYMSGTYPE_LIST_CLOSE:
                {
                    if (cParms > idxParm)
                        rc = HGCMSvcGetU64(&aParms[idxParm], &pReply->u.ListClose.uHandle);

                    LogFlowFunc(("hListClose=%RU64\n", pReply->u.ListClose.uHandle));
                    break;
                }

                case VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_OPEN:
                {
                    if (cParms > idxParm)
                        rc = HGCMSvcGetU64(&aParms[idxParm], &pReply->u.ObjOpen.uHandle);

                    LogFlowFunc(("hObjOpen=%RU64\n", pReply->u.ObjOpen.uHandle));
                    break;
                }

                case VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_CLOSE:
                {
                    if (cParms > idxParm)
                        rc = HGCMSvcGetU64(&aParms[idxParm], &pReply->u.ObjClose.uHandle);

                    LogFlowFunc(("hObjClose=%RU64\n", pReply->u.ObjClose.uHandle));
                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer root list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pRootLstHdr         Where to store the transfer root list header on success.
 */
static int shClSvcTransferGetRootListHdr(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                         PSHCLLISTHDR pRootLstHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_ROOT_LIST_HDR_WRITE)
    {
        rc = HGCMSvcGetU32(&aParms[1], &pRootLstHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&aParms[2], &pRootLstHdr->cEntries);
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer root list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pListEntry          Where to store the root list entry.
 */
static int shClSvcTransferGetRootListEntry(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                           PSHCLLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_WRITE)
    {
        rc = HGCMSvcGetU32(&aParms[1], &pListEntry->fInfo);
        /* Note: aParms[2] contains the entry index, currently being ignored. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&aParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&aParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&aParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
                AssertReturn(cbInfo == pListEntry->cbInfo, VERR_INVALID_PARAMETER);
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list open request from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pOpenParms          Where to store the open parameters of the request.
 */
static int shClSvcTransferGetListOpen(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                      PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_OPEN)
    {
        rc = HGCMSvcGetU32(&aParms[1], &pOpenParms->fList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetStr(&aParms[2], &pOpenParms->pszFilter, &pOpenParms->cbFilter);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetStr(&aParms[3], &pOpenParms->pszPath, &pOpenParms->cbPath);

        /** @todo Some more validation. */
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a transfer list open request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   idCtx               Context ID to use.
 * @param   pOpenParms          List open parameters to set.
 */
static int shClSvcTransferSetListOpen(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                      uint64_t idCtx, PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_OPEN)
    {
        HGCMSvcSetU64(&aParms[0], idCtx);
        HGCMSvcSetU32(&aParms[1], pOpenParms->fList);
        HGCMSvcSetPv (&aParms[2], pOpenParms->pszFilter, pOpenParms->cbFilter);
        HGCMSvcSetPv (&aParms[3], pOpenParms->pszPath, pOpenParms->cbPath);
        HGCMSvcSetU64(&aParms[4], 0); /* OUT: uHandle */

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a transfer list close request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   idCtx               Context ID to use.
 * @param   hList               Handle of list to close.
 */
static int shClSvcTransferSetListClose(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                       uint64_t idCtx, SHCLLISTHANDLE hList)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_CLOSE)
    {
        HGCMSvcSetU64(&aParms[0], idCtx);
        HGCMSvcSetU64(&aParms[1], hList);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListHdr            Where to store the list header.
 */
static int shClSvcTransferGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                     PSHCLLISTHANDLE phList, PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_HDR)
    {
        rc = HGCMSvcGetU64(&aParms[1], phList);
        /* Note: Flags (aParms[2]) not used here. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&aParms[3], &pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&aParms[4], &pListHdr->cEntries);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&aParms[5], &pListHdr->cbTotalSize);

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
 * Sets a transfer list header to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pListHdr            Pointer to list header to set.
 */
static int shClSvcTransferSetListHdr(uint32_t cParms, VBOXHGCMSVCPARM aParms[], PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_HDR)
    {
        /** @todo Set pvMetaFmt + cbMetaFmt. */
        /** @todo Calculate header checksum. */

        HGCMSvcSetU32(&aParms[3], pListHdr->fFeatures);
        HGCMSvcSetU64(&aParms[4], pListHdr->cEntries);
        HGCMSvcSetU64(&aParms[5], pListHdr->cbTotalSize);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListEntry          Where to store the list entry.
 */
static int shClSvcTransferGetListEntry(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                       PSHCLLISTHANDLE phList, PSHCLLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_ENTRY)
    {
        rc = HGCMSvcGetU64(&aParms[1], phList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&aParms[2], &pListEntry->fInfo);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&aParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&aParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&aParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
                AssertReturn(cbInfo == pListEntry->cbInfo, VERR_INVALID_PARAMETER);
            }
        }

        if (RT_SUCCESS(rc))
        {
            if (!ShClTransferListEntryIsValid(pListEntry))
                rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a Shared Clipboard list entry to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pListEntry          Pointer list entry to set.
 */
static int shClSvcTransferSetListEntry(uint32_t cParms, VBOXHGCMSVCPARM aParms[],
                                       PSHCLLISTENTRY pListEntry)
{
    int rc;

    /* Sanity. */
    AssertReturn(ShClTransferListEntryIsValid(pListEntry), VERR_INVALID_PARAMETER);

    if (cParms == VBOX_SHCL_CPARMS_LIST_ENTRY)
    {
        HGCMSvcSetPv (&aParms[3], pListEntry->pszName, pListEntry->cbName);
        HGCMSvcSetU32(&aParms[4], pListEntry->cbInfo);
        HGCMSvcSetPv (&aParms[5], pListEntry->pvInfo, pListEntry->cbInfo);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer object data chunk from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a aParms.
 * @param   aParms              Array of HGCM parameters.
 * @param   pDataChunk          Where to store the object data chunk data.
 */
static int shClSvcTransferGetObjDataChunk(uint32_t cParms, VBOXHGCMSVCPARM aParms[], PSHCLOBJDATACHUNK pDataChunk)
{
    AssertPtrReturn(aParms,    VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDataChunk, VERR_INVALID_PARAMETER);

    int rc;

    if (cParms == VBOX_SHCL_CPARMS_OBJ_WRITE)
    {
        rc = HGCMSvcGetU64(&aParms[1], &pDataChunk->uHandle);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbToRead;
            rc = HGCMSvcGetU32(&aParms[2], &cbToRead);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&aParms[3], &pDataChunk->pvData, &pDataChunk->cbData);
                if (RT_SUCCESS(rc))
                    rc = cbToRead == pDataChunk->cbData ? VINF_SUCCESS : VERR_INVALID_PARAMETER;
            }

            /** @todo Implement checksum handling. */
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Handles a guest reply (VBOX_SHCL_GUEST_FN_REPLY) message.
 *
 * @returns VBox status code.
 * @param   pClient             Pointer to associated client.
 * @param   idTransfer          Transfer ID supplied from the guest.
 * @param   cParms              Number of function parameters supplied.
 * @param   aParms              Array function parameters supplied.
 */
static int shClSvcTransferHandleReply(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
{
    int rc;

    PSHCLTRANSFER pTransfer = NULL;

    uint32_t   cbReply = sizeof(SHCLREPLY);
    PSHCLREPLY pReply  = (PSHCLREPLY)RTMemAlloc(cbReply);
    if (pReply)
    {
        rc = shClSvcTransferGetReply(cParms, aParms, pReply);
        if (RT_SUCCESS(rc))
        {
            if (   pReply->uType                    == VBOX_SHCL_TX_REPLYMSGTYPE_TRANSFER_STATUS
                && pReply->u.TransferStatus.uStatus == SHCLTRANSFERSTATUS_REQUESTED)
            {
                /* SHCLTRANSFERSTATUS_REQUESTED is special, as it doesn't provide a transfer ID. */
            }
            else /* Everything else needs a valid transfer ID. */
            {
                pTransfer = ShClTransferCtxGetTransferById(&pClient->Transfers.Ctx, idTransfer);
                if (!pTransfer)
                {
                    LogRel2(("Shared Clipboard: Transfer with ID %RU16 not found\n", idTransfer));
                    rc = VERR_SHCLPB_TRANSFER_ID_NOT_FOUND;
                }
            }

            if (RT_FAILURE(rc))
            {
                RTMemFree(pReply);
                pReply = NULL;

                return rc;
            }

            PSHCLEVENTPAYLOAD pPayload
                = (PSHCLEVENTPAYLOAD)RTMemAlloc(sizeof(SHCLEVENTPAYLOAD));
            if (pPayload)
            {
                pPayload->pvData = pReply;
                pPayload->cbData = cbReply;

                switch (pReply->uType)
                {
                    case VBOX_SHCL_TX_REPLYMSGTYPE_TRANSFER_STATUS:
                    {
                        /* SHCLTRANSFERSTATUS_REQUESTED is special, as it doesn't provide a transfer ID. */
                        if (SHCLTRANSFERSTATUS_REQUESTED == pReply->u.TransferStatus.uStatus)
                        {
                            LogRel2(("Shared Clipboard: Guest requested a new host -> guest transfer\n"));
                        }
                        else
                            LogRel2(("Shared Clipboard: Guest reported status %s for transfer %RU32\n",
                                     ShClTransferStatusToStr(pReply->u.TransferStatus.uStatus), idTransfer));

                        switch (pReply->u.TransferStatus.uStatus)
                        {
                            case SHCLTRANSFERSTATUS_REQUESTED: /* Guest requests a H->G transfer. */
                            {
                                uint32_t const uMode = ShClSvcGetMode();
                                if (   uMode == VBOX_SHCL_MODE_HOST_TO_GUEST
                                    || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL)
                                {
                                    /* We only create (but not initialize) the transfer here. This is the most lightweight form of
                                     * having a pending transfer around. Report back the new transfer ID to the guest then. */
                                    if (pTransfer == NULL) /* Must not exist yet. */
                                    {
                                        rc = ShClSvcTransferCreate(pClient, SHCLTRANSFERDIR_TO_REMOTE, SHCLSOURCE_LOCAL,
                                                                   NIL_SHCLTRANSFERID /* Creates a new transfer ID */,
                                                                   &pTransfer);
                                        if (RT_SUCCESS(rc))
                                        {
                                            shClSvcClientLock(pClient);

                                            rc = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                                                                SHCLTRANSFERSTATUS_REQUESTED, VINF_SUCCESS,
                                                                                NULL);
                                            shClSvcClientUnlock(pClient);
                                        }
                                    }
                                    else
                                        rc = VERR_WRONG_ORDER;
                                }
                                else
                                    rc = VERR_INVALID_PARAMETER;

                                break;
                            }

                            case SHCLTRANSFERSTATUS_INITIALIZED: /* Guest reports the transfer as being initialized. */
                            {
                                switch (ShClTransferGetDir(pTransfer))
                                {
                                    case SHCLTRANSFERDIR_FROM_REMOTE: /* G->H */
                                        /* Already done locally when creating the transfer. */
                                        break;

                                    case SHCLTRANSFERDIR_TO_REMOTE:   /* H->G */
                                    {
                                        /* Initialize the transfer on the host side. */
                                        rc = ShClSvcTransferInit(pClient, pTransfer);
                                        break;
                                    }

                                    default:
                                        AssertFailed();
                                        break;
                                }

                                break;
                            }
                            case SHCLTRANSFERSTATUS_STARTED:     /* Guest has started the transfer on its side. */
                            {
                                /* We only need to start for H->G transfers here.
                                 * For G->H transfers we start this as soon as the host clipboard requests data. */
                                if (ShClTransferGetDir(pTransfer) == SHCLTRANSFERDIR_TO_REMOTE)
                                {
                                    /* Start the transfer on the host side. */
                                    rc = ShClSvcTransferStart(pClient, pTransfer);
                                }
                                break;
                            }

                            case SHCLTRANSFERSTATUS_CANCELED:
                                RT_FALL_THROUGH();
                            case SHCLTRANSFERSTATUS_KILLED:
                            {
                                LogRel(("Shared Clipboard: Guest has %s transfer %RU32\n",
                                        pReply->uType == SHCLTRANSFERSTATUS_CANCELED ? "canceled" : "killed", pTransfer->State.uID));

                                rc = ShClSvcTransferStop(pClient, pTransfer, false /* fWaitForGuest */);

                                /* Regardless of whether the guest was able to report back and/or stop the transfer, remove the transfer on the host
                                 * so that we don't risk of having stale transfers here. */
                                ShClSvcTransferDestroy(pClient, pTransfer);
                                pTransfer = NULL;
                                break;
                            }

                            case SHCLTRANSFERSTATUS_STOPPED:
                            {
                                LogRel(("Shared Clipboard: Guest has stopped transfer %RU32\n", pTransfer->State.uID));

                                rc = ShClSvcTransferStop(pClient, pTransfer, false /* fWaitForGuest */);
                                break;
                            }

                            case SHCLTRANSFERSTATUS_ERROR:
                            {
                                LogRel(("Shared Clipboard: Guest reported error %Rrc for transfer %RU32\n",
                                        pReply->rc, pTransfer->State.uID));
                                RT_FALL_THROUGH();
                            }
                            default:
                            {
                                /* Regardless of whether the guest was able to report back and/or stop the transfer, remove the transfer on the host
                                 * so that we don't risk of having stale transfers here. */
                                ShClSvcTransferDestroy(pClient, pTransfer);
                                pTransfer = NULL;
                                break;
                            }
                        }

                        /* Tell the backend. */
                        int rc2 = ShClBackendTransferHandleStatusReply(pClient->pBackend, pClient, pTransfer,
                                                                       SHCLSOURCE_REMOTE, pReply->u.TransferStatus.uStatus,
                                                                       pReply->rc);
                        if (RT_SUCCESS(rc))
                            rc = rc2;

                        RT_FALL_THROUGH();
                    }
                    case VBOX_SHCL_TX_REPLYMSGTYPE_LIST_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_TX_REPLYMSGTYPE_LIST_CLOSE:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_TX_REPLYMSGTYPE_OBJ_CLOSE:
                    {
                        uint64_t uCID;
                        rc = HGCMSvcGetU64(&aParms[0], &uCID);
                        if (RT_SUCCESS(rc))
                        {
                            const PSHCLEVENT pEvent
                                = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                            if (pEvent)
                            {
                                LogFlowFunc(("uCID=%RU64 -> idEvent=%RU32\n", uCID, pEvent->idEvent));

                                rc = ShClEventSignal(pEvent, pPayload);
                            }
                            /** @todo Silently skip? */
                        }
                        break;
                    }

                    default:
                        rc = VERR_NOT_FOUND;
                        break;
                }

                if (RT_FAILURE(rc))
                {
                    if (pPayload)
                        RTMemFree(pPayload);
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        if (pReply)
            RTMemFree(pReply);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Transfer client (guest) handler for the Shared Clipboard host service.
 *
 * @returns VBox status code, or VINF_HGCM_ASYNC_EXECUTE if returning to the client will be deferred.
 * @param   pClient             Pointer to associated client.
 * @param   callHandle          The client's call handle of this call.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   aParms              Array function parameters supplied.
 * @param   tsArrival           Timestamp of arrival.
 */
int shClSvcTransferHandler(PSHCLCLIENT pClient,
                           VBOXHGCMCALLHANDLE callHandle,
                           uint32_t u32Function,
                           uint32_t cParms,
                           VBOXHGCMSVCPARM aParms[],
                           uint64_t tsArrival)
{
    RT_NOREF(callHandle, aParms, tsArrival);

    LogFlowFunc(("uClient=%RU32, u32Function=%RU32 (%s), cParms=%RU32, g_ExtState.pfnExtension=%p\n",
                 pClient->State.uClientID, u32Function, ShClGuestMsgToStr(u32Function), cParms, g_ExtState.pfnExtension));

    /* Check if we've the right mode set. */
    if (!shClSvcTransferMsgIsAllowed(ShClSvcGetMode(), u32Function))
    {
        LogFunc(("Wrong clipboard mode, denying access\n"));
        return VERR_ACCESS_DENIED;
    }

    int rc = VERR_INVALID_PARAMETER; /* Play safe by default. */

    if (cParms < 1)
        return rc;
    ASSERT_GUEST_RETURN(aParms[0].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_WRONG_PARAMETER_TYPE);

    uint64_t uCID  = 0; /* Context ID */
    rc = HGCMSvcGetU64(&aParms[0], &uCID);
    if (RT_FAILURE(rc))
        return rc;

    const SHCLTRANSFERID idTransfer = VBOX_SHCL_CONTEXTID_GET_TRANSFER(uCID);

    /*
     * Pre-check: For certain messages we need to make sure that a (right) transfer is present.
     */
    PSHCLTRANSFER pTransfer = NULL;
    switch (u32Function)
    {
        case VBOX_SHCL_GUEST_FN_REPLY:
            /* Function does its own lookup. */
            break;

        default:
        {
            pTransfer = ShClTransferCtxGetTransferById(&pClient->Transfers.Ctx, idTransfer);
            if (!pTransfer)
            {
                LogRel(("Shared Clipboard: Transfer with ID %RU16 not found\n", idTransfer));
                rc = VERR_SHCLPB_TRANSFER_ID_NOT_FOUND;
            }
            break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHCL_GUEST_FN_REPLY:
        {
            rc = shClSvcTransferHandleReply(pClient, idTransfer, cParms, aParms);
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ)
                break;

            ASSERT_GUEST_RETURN(aParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* Features */
            ASSERT_GUEST_RETURN(aParms[2].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_WRONG_PARAMETER_TYPE); /* # Entries  */

            SHCLLISTHDR rootListHdr;
            RT_ZERO(rootListHdr);

            rootListHdr.cEntries = ShClTransferRootsCount(pTransfer);
            /** @todo BUGBUG What about the features? */

            HGCMSvcSetU64(&aParms[0], 0 /* Context ID */);
            HGCMSvcSetU32(&aParms[1], rootListHdr.fFeatures);
            HGCMSvcSetU64(&aParms[2], rootListHdr.cEntries);

            rc = VINF_SUCCESS;
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE:
        {
            SHCLLISTHDR lstHdr;
            rc = shClSvcTransferGetRootListHdr(cParms, aParms, &lstHdr);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferListHdrDup(&lstHdr);
                uint32_t cbData = sizeof(SHCLLISTHDR);

                const PSHCLEVENT pEvent = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                if (pEvent)
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClPayloadAlloc(pEvent->idEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClEventSignal(pEvent, pPayload);
                        if (RT_FAILURE(rc))
                            ShClPayloadFree(pPayload);
                    }
                }
                else
                    rc = VERR_SHCLPB_EVENT_ID_NOT_FOUND;
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ)
                break;

            ASSERT_GUEST_RETURN(aParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* Info flags */
            ASSERT_GUEST_RETURN(aParms[2].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_WRONG_PARAMETER_TYPE); /* Entry index # */
            ASSERT_GUEST_RETURN(aParms[3].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Entry name */
            ASSERT_GUEST_RETURN(aParms[4].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* Info size */
            ASSERT_GUEST_RETURN(aParms[5].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Info data */

            uint32_t fInfo;
            rc = HGCMSvcGetU32(&aParms[1], &fInfo);
            AssertRCBreak(rc);

            ASSERT_GUEST_RETURN(fInfo & VBOX_SHCL_INFO_F_FSOBJINFO, VERR_WRONG_PARAMETER_TYPE); /* Validate info flags.  */

            uint64_t uIdx;
            rc = HGCMSvcGetU64(&aParms[2], &uIdx);
            AssertRCBreak(rc);

            PCSHCLLISTENTRY pEntry = ShClTransferRootsEntryGet(pTransfer, uIdx);
            if (pEntry)
            {
                /* Entry name */
                void  *pvDst = aParms[3].u.pointer.addr;
                size_t cbDst = aParms[3].u.pointer.size;
                memcpy(pvDst, pEntry->pszName, RT_MIN(pEntry->cbName, cbDst));

                /* Info size */
                HGCMSvcSetU32(&aParms[4], pEntry->cbInfo);

                /* Info data */
                pvDst = aParms[5].u.pointer.addr;
                cbDst = aParms[5].u.pointer.size;
                memcpy(pvDst, pEntry->pvInfo, RT_MIN(pEntry->cbInfo, cbDst));
            }
            else
                rc = VERR_NOT_FOUND;

            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE:
        {
            SHCLLISTENTRY lstEntry;
            rc = shClSvcTransferGetRootListEntry(cParms, aParms, &lstEntry);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferListEntryDup(&lstEntry);
                uint32_t cbData = sizeof(SHCLLISTENTRY);

                const PSHCLEVENT pEvent = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                if (pEvent)
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClPayloadAlloc(pEvent->idEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClEventSignal(pEvent, pPayload);
                        if (RT_FAILURE(rc))
                            ShClPayloadFree(pPayload);
                    }
                }
                else
                    rc = VERR_SHCLPB_EVENT_ID_NOT_FOUND;
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_OPEN:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_OPEN)
                break;

            ASSERT_GUEST_RETURN(aParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* List flags */
            ASSERT_GUEST_RETURN(aParms[2].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Filter string */
            ASSERT_GUEST_RETURN(aParms[3].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Path */
            ASSERT_GUEST_RETURN(aParms[4].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_WRONG_PARAMETER_TYPE); /* List handle (output) */

            SHCLLISTOPENPARMS listOpenParms;
            rc = shClSvcTransferGetListOpen(cParms, aParms, &listOpenParms);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = ShClTransferListOpen(pTransfer, &listOpenParms, &hList);
                if (RT_SUCCESS(rc))
                {
                    /* Return list handle. */
                    HGCMSvcSetU64(&aParms[4], hList);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_CLOSE:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_CLOSE)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&aParms[1], &hList);
            if (RT_SUCCESS(rc))
            {
                rc = ShClTransferListClose(pTransfer, hList);
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_HDR_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_HDR)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&aParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHDR hdrList;
                rc = ShClTransferListGetHeader(pTransfer, hList, &hdrList);
                if (RT_SUCCESS(rc))
                    rc = shClSvcTransferSetListHdr(cParms, aParms, &hdrList);
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE:
        {
            SHCLLISTHDR hdrList;
            rc = ShClTransferListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = shClSvcTransferGetListHdr(cParms, aParms, &hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = ShClTransferListHdrDup(&hdrList);
                    uint32_t cbData = sizeof(SHCLLISTHDR);

                    const PSHCLEVENT pEvent = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                    if (pEvent)
                    {
                        PSHCLEVENTPAYLOAD pPayload;
                        rc = ShClPayloadAlloc(pEvent->idEvent, pvData, cbData, &pPayload);
                        if (RT_SUCCESS(rc))
                        {
                            rc = ShClEventSignal(pEvent, pPayload);
                            if (RT_FAILURE(rc))
                                ShClPayloadFree(pPayload);
                        }
                    }
                    else
                        rc = VERR_SHCLPB_EVENT_ID_NOT_FOUND;
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_ENTRY)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&aParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTENTRY entryList;
                rc = ShClTransferListEntryInit(&entryList);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClTransferListRead(pTransfer, hList, &entryList);
                    if (RT_SUCCESS(rc))
                        rc = shClSvcTransferSetListEntry(cParms, aParms, &entryList);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE:
        {
            SHCLLISTENTRY entryList;
            rc = ShClTransferListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = shClSvcTransferGetListEntry(cParms, aParms, &hList, &entryList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = ShClTransferListEntryDup(&entryList);
                    uint32_t cbData = sizeof(SHCLLISTENTRY);

                    const PSHCLEVENT pEvent = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                    if (pEvent)
                    {
                        PSHCLEVENTPAYLOAD pPayload;
                        rc = ShClPayloadAlloc(pEvent->idEvent, pvData, cbData, &pPayload);
                        if (RT_SUCCESS(rc))
                        {
                            rc = ShClEventSignal(pEvent, pPayload);
                            if (RT_FAILURE(rc))
                                ShClPayloadFree(pPayload);
                        }
                    }
                    else
                        rc = VERR_SHCLPB_EVENT_ID_NOT_FOUND;
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_OPEN:
        {
            ASSERT_GUEST_STMT_BREAK(cParms == VBOX_SHCL_CPARMS_OBJ_OPEN, VERR_WRONG_PARAMETER_COUNT);

            SHCLOBJOPENCREATEPARMS openCreateParms;
            RT_ZERO(openCreateParms);

            /* aParms[1] will return the object handle on success; see below. */
            rc = HGCMSvcGetStr(&aParms[2], &openCreateParms.pszPath, &openCreateParms.cbPath);
            if (RT_SUCCESS(rc))
                rc = HGCMSvcGetU32(&aParms[3], &openCreateParms.fCreate);

            if (RT_SUCCESS(rc))
            {
                SHCLOBJHANDLE hObj;
                rc = ShClTransferObjOpen(pTransfer, &openCreateParms, &hObj);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("hObj=%RU64\n", hObj));

                    HGCMSvcSetU64(&aParms[1], hObj);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_CLOSE:
        {
            if (cParms != VBOX_SHCL_CPARMS_OBJ_CLOSE)
                break;

            SHCLOBJHANDLE hObj;
            rc = HGCMSvcGetU64(&aParms[1], &hObj); /* Get object handle. */
            if (RT_SUCCESS(rc))
                rc = ShClTransferObjClose(pTransfer, hObj);
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_OBJ_READ)
                break;

            ASSERT_GUEST_RETURN(aParms[1].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_WRONG_PARAMETER_TYPE); /* Object handle */
            ASSERT_GUEST_RETURN(aParms[2].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* Bytes to read */
            ASSERT_GUEST_RETURN(aParms[3].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Data buffer */
            ASSERT_GUEST_RETURN(aParms[4].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* Checksum data size */
            ASSERT_GUEST_RETURN(aParms[5].type == VBOX_HGCM_SVC_PARM_PTR,   VERR_WRONG_PARAMETER_TYPE); /* Checksum data buffer*/

            SHCLOBJHANDLE hObj;
            rc = HGCMSvcGetU64(&aParms[1], &hObj); /* Get object handle. */
            AssertRCBreak(rc);

            uint32_t cbToRead = 0;
            rc = HGCMSvcGetU32(&aParms[2], &cbToRead);
            AssertRCBreak(rc);

            void    *pvBuf = NULL;
            uint32_t cbBuf = 0;
            rc = HGCMSvcGetPv(&aParms[3], &pvBuf, &cbBuf);
            AssertRCBreak(rc);

            LogFlowFunc(("hObj=%RU64, cbBuf=%RU32, cbToRead=%RU32, rc=%Rrc\n", hObj, cbBuf, cbToRead, rc));

            if (   RT_SUCCESS(rc)
                && (   !cbBuf
                    || !cbToRead
                    ||  cbBuf < cbToRead
                   )
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }

            if (RT_SUCCESS(rc))
            {
                uint32_t cbRead;
                rc = ShClTransferObjRead(pTransfer, hObj, pvBuf, cbToRead, 0 /* fFlags */, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    HGCMSvcSetU32(&aParms[2], cbRead);

                    /** @todo Implement checksum support. */
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_WRITE:
        {
            SHCLOBJDATACHUNK dataChunk;

            rc = shClSvcTransferGetObjDataChunk(cParms, aParms, &dataChunk);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferObjDataChunkDup(&dataChunk);
                uint32_t cbData = sizeof(SHCLOBJDATACHUNK);

                const PSHCLEVENT pEvent = ShClEventSourceGetFromId(&pClient->EventSrc, VBOX_SHCL_CONTEXTID_GET_EVENT(uCID));
                if (pEvent)
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClPayloadAlloc(pEvent->idEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClEventSignal(pEvent, pPayload);
                        if (RT_FAILURE(rc))
                            ShClPayloadFree(pPayload);
                    }
                }
                else
                    rc = VERR_SHCLPB_EVENT_ID_NOT_FOUND;
            }

            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    /* If anything wrong has happened, make sure to unregister the transfer again (if not done already) and tell the guest. */
    if (   RT_FAILURE(rc)
        && pTransfer)
    {
        shClSvcClientLock(pClient);

        /* Let the guest know. */
        int rc2 = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                                 SHCLTRANSFERSTATUS_ERROR, rc, NULL /* ppEvent */);
        AssertRC(rc2);

        ShClSvcTransferDestroy(pClient, pTransfer);

        shClSvcClientUnlock(pClient);
    }

    LogFlowFunc(("[Client %RU32] Returning rc=%Rrc\n", pClient->State.uClientID, rc));
    return rc;
}

/**
 * Transfer host handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   aParms              Array function parameters supplied.
 */
int shClSvcTransferHostHandler(uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM aParms[])
{
    RT_NOREF(cParms, aParms);

    int rc = VERR_NOT_IMPLEMENTED; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHCL_HOST_FN_CANCEL: /** @todo BUGBUG Implement this. */
            break;

        case VBOX_SHCL_HOST_FN_ERROR: /** @todo BUGBUG Implement this. */
            break;

        default:
            break;

    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int shClSvcTransferHostMsgHandler(PSHCLCLIENT pClient, PSHCLCLIENTMSG pMsg)
{
    RT_NOREF(pClient);

    int rc;

    switch (pMsg->idMsg)
    {
        default:
            rc = VINF_SUCCESS;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reports a transfer status to the guest.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   idTransfer          Transfer ID to report status for.
 * @param   enmDir              Transfer direction to report status for.
 * @param   uStatus             Status to report.
 * @param   rcTransfer          Result code to report. Optional and depending on status.
 * @param   ppEvent             Where to return the wait event on success. Optional.
 *                              Must be released by the caller with ShClEventRelease().
 *
 * @note    Caller must enter the client's critical section.
 */
static int shClSvcTransferSendStatusExAsync(PSHCLCLIENT pClient, SHCLTRANSFERID idTransfer, SHCLTRANSFERDIR enmDir, SHCLTRANSFERSTATUS uStatus,
                                            int rcTransfer, PSHCLEVENT *ppEvent)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    AssertReturn(idTransfer != NIL_SHCLTRANSFERID, VERR_INVALID_PARAMETER);
    /* ppEvent is optional. */

    Assert(RTCritSectIsOwner(&pClient->CritSect));

    PSHCLCLIENTMSG pMsgReadData = shClSvcMsgAlloc(pClient, VBOX_SHCL_HOST_MSG_TRANSFER_STATUS,
                                                  VBOX_SHCL_CPARMS_TRANSFER_STATUS);
    if (!pMsgReadData)
        return VERR_NO_MEMORY;

    PSHCLEVENT pEvent;
    int rc = ShClEventSourceGenerateAndRegisterEvent(&pClient->EventSrc, &pEvent);
    if (RT_SUCCESS(rc))
    {
        HGCMSvcSetU64(&pMsgReadData->aParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID, idTransfer, pEvent->idEvent));
        HGCMSvcSetU32(&pMsgReadData->aParms[1], enmDir);
        HGCMSvcSetU32(&pMsgReadData->aParms[2], uStatus);
        HGCMSvcSetU32(&pMsgReadData->aParms[3], (uint32_t)rcTransfer); /** @todo uint32_t vs. int. */
        HGCMSvcSetU32(&pMsgReadData->aParms[4], 0 /* fFlags, unused */);

        shClSvcMsgAdd(pClient, pMsgReadData, true /* fAppend */);

        rc = shClSvcClientWakeup(pClient);
        if (RT_SUCCESS(rc))
        {
            LogRel2(("Shared Clipboard: Reported status %s (rc=%Rrc) of transfer %RU32 to guest\n",
                     ShClTransferStatusToStr(uStatus), rcTransfer, idTransfer));

            if (ppEvent)
            {
                *ppEvent = pEvent; /* Takes ownership. */
            }
            else /* If event is not consumed by the caller, release the event again. */
                ShClEventRelease(pEvent);
        }
        else
            ShClEventRelease(pEvent);
    }
    else
        rc = VERR_SHCLPB_MAX_EVENTS_REACHED;

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Reporting status %s (%Rrc) for transfer %RU32 to guest failed with %Rrc\n",
                ShClTransferStatusToStr(uStatus), rcTransfer, idTransfer, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reports a transfer status to the guest, internal version.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to report status for.
 * @param   uStatus             Status to report.
 * @param   rcTransfer          Result code to report. Optional and depending on status.
 * @param   ppEvent             Where to return the wait event on success. Optional.
 *                              Must be released by the caller with ShClEventRelease().
 *
 * @note    Caller must enter the client's critical section.
 */
static int shClSvcTransferSendStatusAsync(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer, SHCLTRANSFERSTATUS uStatus,
                                          int rcTransfer, PSHCLEVENT *ppEvent)
{
    AssertPtrReturn(pClient,   VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    /* ppEvent is optional. */

   return shClSvcTransferSendStatusExAsync(pClient, ShClTransferGetID(pTransfer), ShClTransferGetDir(pTransfer),
                                           uStatus, rcTransfer, ppEvent);
}

/**
 * Reports a transfer status to the guest.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to report status for.
 * @param   uStatus             Status to report.
 * @param   rcTransfer          Result code to report. Optional and depending on status.
 * @param   ppEvent             Where to return the wait event on success. Optional.
 *                              Must be released by the caller with ShClEventRelease().
 */
int ShClSvcTransferSendStatusAsync(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer, SHCLTRANSFERSTATUS uStatus,
                                   int rcTransfer, PSHCLEVENT *ppEvent)
{
    return shClSvcTransferSendStatusAsync(pClient, pTransfer, uStatus, rcTransfer, ppEvent);
}

/**
 * Cleans up (unregisters and destroys) all transfers not in started state (anymore).
 *
 * @param   pClient             Client to clean up transfers for.
 *
 * @note    Caller needs to take the critical section.
 */
static void shClSvcTransferCleanupAllUnused(PSHCLCLIENT pClient)
{
    Assert(RTCritSectIsOwner(&pClient->CritSect));

    LogFlowFuncEnter();

    PSHCLTRANSFERCTX pTxCtx = &pClient->Transfers.Ctx;

    PSHCLTRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pTxCtx->List, pTransfer, pTransferNext, SHCLTRANSFER, Node)
    {
        SHCLTRANSFERSTATUS const enmSts = ShClTransferGetStatus(pTransfer);
        if (enmSts != SHCLTRANSFERSTATUS_STARTED)
        {
            /* Let the guest know. */
            int rc2 = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                                     SHCLTRANSFERSTATUS_UNINITIALIZED, VINF_SUCCESS, NULL /* ppEvent */);
            AssertRC(rc2);

            ShClTransferCtxUnregisterById(pTxCtx, pTransfer->State.uID);

            ShClTransferDestroy(pTransfer);

            RTMemFree(pTransfer);
            pTransfer = NULL;
        }
    }
}

/**
 * Creates a new transfer on the host.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   enmDir              Transfer direction to create.
 * @param   enmSource           Transfer source to create.
 * @param   idTransfer          Transfer ID to use for creation.
 *                              If set to NIL_SHCLTRANSFERID, a new transfer ID will be created.
 * @param   ppTransfer          Where to return the created transfer on success. Optional and can be NULL.
 */
int ShClSvcTransferCreate(PSHCLCLIENT pClient, SHCLTRANSFERDIR enmDir, SHCLSOURCE enmSource, SHCLTRANSFERID idTransfer, PSHCLTRANSFER *ppTransfer)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    /* ppTransfer is optional. */

    LogFlowFuncEnter();

    shClSvcClientLock(pClient);

    /* When creating a new transfer, this is a good time to clean up old stuff we don't need anymore. */
    shClSvcTransferCleanupAllUnused(pClient);

    PSHCLTRANSFER pTransfer;
    int rc = ShClTransferCreate(enmDir, enmSource, &pClient->Transfers.Callbacks, &pTransfer);
    if (RT_SUCCESS(rc))
    {
        if (idTransfer == NIL_SHCLTRANSFERID)
            rc = ShClTransferCtxRegister(&pClient->Transfers.Ctx, pTransfer, &idTransfer);
        else
            rc = ShClTransferCtxRegisterById(&pClient->Transfers.Ctx, pTransfer, idTransfer);
        if (RT_SUCCESS(rc))
        {
            if (ppTransfer)
                *ppTransfer = pTransfer;
        }
    }

    shClSvcClientUnlock(pClient);

    if (RT_FAILURE(rc))
    {
        ShClTransferDestroy(pTransfer);

        RTMemFree(pTransfer);
        pTransfer = NULL;
    }

    if (RT_FAILURE(rc))
       LogRel(("Shared Clipboard: Creating transfer failed with %Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys a transfer on the host.
 *
 * @param   pClient             Client to destroy transfer for.
 * @param   pTransfer           Transfer to destroy.
 *                              The pointer will be invalid after return.
 */
void ShClSvcTransferDestroy(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    if (!pTransfer)
        return;

    LogFlowFuncEnter();

    PSHCLTRANSFERCTX pTxCtx = &pClient->Transfers.Ctx;

    ShClTransferCtxUnregisterById(pTxCtx, pTransfer->State.uID);

    ShClTransferDestroy(pTransfer);

    RTMemFree(pTransfer);
    pTransfer = NULL;

    LogFlowFuncLeave();
}

/**
 * Initializes a (created) transfer on the host.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to initialize.
 */
int ShClSvcTransferInit(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    shClSvcClientLock(pClient);

    Assert(ShClTransferGetStatus(pTransfer) == SHCLTRANSFERSTATUS_NONE);

    PSHCLTRANSFERCTX pTxCtx = &pClient->Transfers.Ctx;

    int rc;

    if (!ShClTransferCtxIsMaximumReached(pTxCtx))
    {
        SHCLTRANSFERDIR const enmDir = ShClTransferGetDir(pTransfer);

        LogRel2(("Shared Clipboard: Initializing %s transfer ...\n",
                 enmDir == SHCLTRANSFERDIR_FROM_REMOTE ? "guest -> host" : "host -> guest"));

        rc = ShClTransferInit(pTransfer);
        if (RT_SUCCESS(rc))
        {
            /* Sanity: Make sure that the transfer we're gonna report as INITIALIZED to the guest
             *         actually has some root entries set, as the guest can query for those at any time then. */
            if (enmDir == SHCLTRANSFERDIR_TO_REMOTE)
                AssertMsgStmt(ShClTransferRootsCount(pTransfer), ("Transfer has no root entries set\n"), rc = VERR_WRONG_ORDER);
        }
    }
    else
        rc = VERR_SHCLPB_MAX_TRANSFERS_REACHED;

    /* Tell the guest the outcome. */
    int rc2 = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                               RT_SUCCESS(rc)
                                             ? SHCLTRANSFERSTATUS_INITIALIZED : SHCLTRANSFERSTATUS_ERROR, rc,
                                             NULL /* ppEvent */);
    if (RT_SUCCESS(rc))
        rc2 = rc;

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Initializing transfer failed with %Rrc\n", rc));

    shClSvcClientUnlock(pClient);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Starts a transfer, communicating the status to the guest side.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to start.
 */
int ShClSvcTransferStart(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    LogRel2(("Shared Clipboard: Starting transfer %RU32 ...\n", pTransfer->State.uID));

    shClSvcClientLock(pClient);

    int rc = ShClTransferStart(pTransfer);

    /* Let the guest know in any case
     * (so that it can tear down the transfer on error as well). */
    int rc2 = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                              RT_SUCCESS(rc)
                                            ? SHCLTRANSFERSTATUS_STARTED : SHCLTRANSFERSTATUS_ERROR, rc,
                                            NULL /* ppEvent */);
    if (RT_SUCCESS(rc))
        rc = rc2;

    shClSvcClientUnlock(pClient);
    return rc;
}

/**
 * Stops (and destroys) a transfer, communicating the status to the guest side.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to stop. The pointer will be invalid on success.
 * @param   fWaitForGuest       Set to \c true to wait for acknowledgement from guest, or \c false to skip waiting.
 */
int ShClSvcTransferStop(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer, bool fWaitForGuest)
{
    LogRel2(("Shared Clipboard: Stopping transfer %RU32 ...\n", pTransfer->State.uID));

    shClSvcClientLock(pClient);

    PSHCLEVENT pEvent;
    int rc = shClSvcTransferSendStatusAsync(pClient, pTransfer,
                                            SHCLTRANSFERSTATUS_STOPPED, VINF_SUCCESS, &pEvent);
    if (   RT_SUCCESS(rc)
        && fWaitForGuest)
    {
        LogRel2(("Shared Clipboard: Waiting for stop of transfer %RU32 on guest ...\n", pTransfer->State.uID));

        shClSvcClientUnlock(pClient);

        rc = ShClEventWait(pEvent, pTransfer->uTimeoutMs, NULL /* ppPayload */);
        if (RT_SUCCESS(rc))
            LogRel2(("Shared Clipboard: Stopped transfer %RU32 on guest\n", pTransfer->State.uID));

        ShClEventRelease(pEvent);

        shClSvcClientLock(pClient);
    }

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Unable to stop transfer %RU32 on guest, rc=%Rrc\n",
                pTransfer->State.uID, rc));

    shClSvcClientUnlock(pClient);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets the host service's (file) transfer mode.
 *
 * @returns VBox status code.
 * @param   fMode               Transfer mode to set.
 */
int shClSvcTransferModeSet(uint32_t fMode)
{
    if (fMode & ~VBOX_SHCL_TRANSFER_MODE_F_VALID_MASK)
        return VERR_INVALID_FLAGS;

    g_fTransferMode = fMode;

    LogRel2(("Shared Clipboard: File transfers are now %s\n",
             g_fTransferMode & VBOX_SHCL_TRANSFER_MODE_F_ENABLED ? "enabled" : "disabled"));

    /* If file transfers are being disabled, make sure to also reset (destroy) all pending transfers. */
    if (!(g_fTransferMode & VBOX_SHCL_TRANSFER_MODE_F_ENABLED))
    {
        ClipboardClientMap::const_iterator itClient = g_mapClients.begin();
        while (itClient != g_mapClients.end())
        {
            PSHCLCLIENT pClient = itClient->second;
            AssertPtr(pClient);

            shClSvcTransferDestroyAll(pClient);

            ++itClient;
        }
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}
