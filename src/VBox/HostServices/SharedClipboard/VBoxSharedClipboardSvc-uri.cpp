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

#include <VBox/GuestHost/clipboard-helper.h>
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
extern SHCLEXTSTATE g_ExtState;
extern PVBOXHGCMSVCHELPERS g_pHelpers;
extern ClipboardClientQueue g_listClientsDeferred;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static int vboxSvcClipboardURISetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                          PVBOXSHCLMSGCTX pMsgCtx, PSHCLLISTOPENPARMS pOpenParms);
static int vboxSvcClipboardURISetListClose(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                           PVBOXSHCLMSGCTX pMsgCtx, SHCLLISTHANDLE hList);


/*********************************************************************************************************************************
*   Provider implementation                                                                                                      *
*********************************************************************************************************************************/

DECLCALLBACK(int) vboxSvcClipboardURITransferOpen(PSHCLPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxSvcClipboardURITransferClose(PSHCLPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxSvcClipboardURIGetRoots(PSHCLPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsgHdr = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_HDR_READ,
                                                               VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_HDR_READ);
    if (pMsgHdr)
    {
        SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsgHdr->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU32(&pMsgHdr->m_paParms[1], 0 /* fRoots */);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsgHdr, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayloadHdr;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent,
                                              pCtx->pTransfer->uTimeoutMs, &pPayloadHdr);
                if (RT_SUCCESS(rc))
                {
                    PSHCLROOTLISTHDR pSrcRootListHdr = (PSHCLROOTLISTHDR)pPayloadHdr->pvData;
                    Assert(pPayloadHdr->cbData == sizeof(SHCLROOTLISTHDR));

                    LogFlowFunc(("cRoots=%RU32, fRoots=0x%x\n", pSrcRootListHdr->cRoots, pSrcRootListHdr->fRoots));

                    PSHCLROOTLIST pRootList = SharedClipboardURIRootListAlloc();
                    if (pRootList)
                    {
                        if (pSrcRootListHdr->cRoots)
                        {
                            pRootList->paEntries =
                                (PSHCLROOTLISTENTRY)RTMemAllocZ(pSrcRootListHdr->cRoots * sizeof(SHCLROOTLISTENTRY));

                            if (pRootList->paEntries)
                            {
                                for (uint32_t i = 0; i < pSrcRootListHdr->cRoots; i++)
                                {
                                    PSHCLCLIENTMSG pMsgEntry = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_ENTRY_READ,
                                                                                                 VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_ENTRY_READ_REQ);

                                    uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

                                    HGCMSvcSetU32(&pMsgEntry->m_paParms[0],
                                                  VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
                                    HGCMSvcSetU32(&pMsgEntry->m_paParms[1], 0 /* fRoots */);
                                    HGCMSvcSetU32(&pMsgEntry->m_paParms[2], i /* uIndex */);

                                    rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
                                    AssertRC(rc2);

                                    rc = vboxSvcClipboardMsgAdd(pClient, pMsgEntry, true /* fAppend */);
                                    if (RT_FAILURE(rc))
                                        break;

                                    PSHCLEVENTPAYLOAD pPayloadEntry;
                                    rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent,
                                                                  pCtx->pTransfer->uTimeoutMs, &pPayloadEntry);
                                    if (RT_FAILURE(rc))
                                        break;

                                    PSHCLROOTLISTENTRY pSrcRootListEntry = (PSHCLROOTLISTENTRY)pPayloadEntry->pvData;
                                    Assert(pPayloadEntry->cbData == sizeof(SHCLROOTLISTENTRY));

                                    rc = SharedClipboardURIListEntryCopy(&pRootList->paEntries[i], pSrcRootListEntry);

                                    SharedClipboardPayloadFree(pPayloadEntry);

                                    SharedClipboardEventUnregister(&pCtx->pTransfer->Events, uEvent);

                                    if (RT_FAILURE(rc))
                                        break;
                                }
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }

                        if (RT_SUCCESS(rc))
                        {
                            pRootList->Hdr.cRoots = pSrcRootListHdr->cRoots;
                            pRootList->Hdr.fRoots = 0; /** @todo Implement this. */

                            *ppRootList = pRootList;
                        }
                        else
                            SharedClipboardURIRootListFree(pRootList);

                        SharedClipboardPayloadFree(pPayloadHdr);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
            }

            SharedClipboardEventUnregister(&pCtx->pTransfer->Events, uEvent);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeave();
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListOpen(PSHCLPROVIDERCTX pCtx,
                                              PSHCLLISTOPENPARMS pOpenParms, PSHCLLISTHANDLE phList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        pMsg->m_Ctx.uContextID = VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent);

        rc = vboxSvcClipboardURISetListOpen(pMsg->m_cParms, pMsg->m_paParms, &pMsg->m_Ctx, pOpenParms);
        if (RT_SUCCESS(rc))
        {
            rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
                AssertRC(rc2);

                rc = vboxSvcClipboardClientWakeup(pClient);
                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(pPayload->cbData == sizeof(SHCLREPLY));

                        PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                        AssertPtr(pReply);

                        Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN);

                        *phList = pReply->u.ListOpen.uHandle;

                        SharedClipboardPayloadFree(pPayload);
                    }
                }

                SharedClipboardEventUnregister(&pCtx->pTransfer->Events, uEvent);
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListClose(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        pMsg->m_Ctx.uContextID = VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent);

        rc = vboxSvcClipboardURISetListClose(pMsg->m_cParms, pMsg->m_paParms, &pMsg->m_Ctx, hList);
        if (RT_SUCCESS(rc))
        {
            rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
                AssertRC(rc2);

                rc = vboxSvcClipboardClientWakeup(pClient);
                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                        SharedClipboardPayloadFree(pPayload);
                }

                SharedClipboardEventUnregister(&pCtx->pTransfer->Events, uEvent);
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrRead(PSHCLPROVIDERCTX pCtx,
                                                 SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ_REQ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], hList);
        HGCMSvcSetU32(&pMsg->m_paParms[2], 0 /* fFlags */);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent,
                                                         pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTHDR));

                    *pListHdr = *(PSHCLLISTHDR)pPayload->pvData;

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrWrite(PSHCLPROVIDERCTX pCtx,
                                                  SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryRead(PSHCLPROVIDERCTX pCtx,
                                                   SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], hList);
        HGCMSvcSetU32(&pMsg->m_paParms[2], 0 /* fInfo */);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTENTRY));

                    rc = SharedClipboardURIListEntryCopy(pListEntry, (PSHCLLISTENTRY)pPayload->pvData);

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryWrite(PSHCLPROVIDERCTX pCtx,
                                                    SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    RT_NOREF(pCtx, hList, pListEntry);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int vboxSvcClipboardURIObjOpen(PSHCLPROVIDERCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms,
                               PSHCLOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_OPEN);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        LogFlowFunc(("pszPath=%s, fCreate=0x%x\n", pCreateParms->pszPath, pCreateParms->fCreate));

        const uint32_t cbPath = (uint32_t)strlen(pCreateParms->pszPath) + 1; /* Include terminating zero */

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], 0); /* uHandle */
        HGCMSvcSetU32(&pMsg->m_paParms[2], cbPath);
        HGCMSvcSetPv (&pMsg->m_paParms[3], pCreateParms->pszPath, cbPath);
        HGCMSvcSetU32(&pMsg->m_paParms[4], pCreateParms->fCreate);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));

                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN);

                    *phObj = pReply->u.ObjOpen.uHandle;

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int vboxSvcClipboardURIObjClose(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_CLOSE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], hObj);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));

#ifdef VBOX_STRICT
                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE);
#endif

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int vboxSvcClipboardURIObjRead(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                               void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_READ_REQ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], hObj);
        HGCMSvcSetU32(&pMsg->m_paParms[2], cbData);
        HGCMSvcSetU32(&pMsg->m_paParms[3], fFlags);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLOBJDATACHUNK));

                    PSHCLOBJDATACHUNK pDataChunk = (PSHCLOBJDATACHUNK)pPayload->pvData;
                    AssertPtr(pDataChunk);

                    const uint32_t cbRead = RT_MIN(cbData, pDataChunk->cbData);

                    memcpy(pvData, pDataChunk->pvData, cbRead);

                    if (pcbRead)
                        *pcbRead = cbRead;

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int vboxSvcClipboardURIObjWrite(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_WRITE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = SharedClipboardEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU32(&pMsg->m_paParms[0], VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->m_paParms[1], hObj);
        HGCMSvcSetU64(&pMsg->m_paParms[2], cbData);
        HGCMSvcSetU64(&pMsg->m_paParms[3], fFlags);

        rc = vboxSvcClipboardMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    const uint32_t cbRead = RT_MIN(cbData, pPayload->cbData);

                    memcpy(pvData, pPayload->pvData, cbRead);

                    if (pcbWritten)
                        *pcbWritten = cbRead;

                    SharedClipboardPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   URI callbacks                                                                                                                *
*********************************************************************************************************************************/

DECLCALLBACK(void) VBoxSvcClipboardURITransferPrepareCallback(PSHCLURITRANSFERCALLBACKDATA pData)
{
    RT_NOREF(pData);

    LogFlowFuncEnter();
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(pData, rc);

    LogFlowFuncEnter();

    LogRel2(("Shared Clipboard: Transfer complete\n"));
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferCanceledCallback(PSHCLURITRANSFERCALLBACKDATA pData)
{
    LogFlowFuncEnter();

    RT_NOREF(pData);

    LogRel2(("Shared Clipboard: Transfer canceled\n"));
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferErrorCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc)
{
    LogFlowFuncEnter();

    RT_NOREF(pData, rc);

    LogRel(("Shared Clipboard: Transfer failed with %Rrc\n", rc));
}


/*********************************************************************************************************************************
*   HGCM getters / setters                                                                                                       *
*********************************************************************************************************************************/

/**
 * Gets an URI message reply from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pReply              Where to store the reply.
 */
static int vboxSvcClipboardURIGetReply(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                       PSHCLREPLY pReply)
{
    int rc;

    if (cParms >= VBOX_SHARED_CLIPBOARD_CPARMS_REPLY_MIN)
    {
        uint32_t cbPayload = 0;

        rc = HGCMSvcGetU32(&paParms[1], &pReply->uType);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pReply->rc);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &cbPayload);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetPv(&paParms[4], &pReply->pvPayload, &pReply->cbPayload);
            AssertReturn(cbPayload == pReply->cbPayload, VERR_INVALID_PARAMETER);
        }

        if (RT_SUCCESS(rc))
        {
            rc = VERR_INVALID_PARAMETER; /* Play safe. */

            switch (pReply->uType)
            {
                case VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ListOpen.uHandle);
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ObjOpen.uHandle);
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ObjClose.uHandle);
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
 * Gets an URI root list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pRootLstHdr         Where to store the URI root list header on success.
 */
static int vboxSvcClipboardURIGetRootListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                             PSHCLROOTLISTHDR pRootLstHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_HDR)
    {
        rc = HGCMSvcGetU32(&paParms[1], &pRootLstHdr->fRoots);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pRootLstHdr->cRoots);
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets an URI root list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListEntry          Where to store the root list entry.
 */
static int vboxSvcClipboardURIGetRootListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                               PSHCLROOTLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_ENTRY)
    {
        rc = HGCMSvcGetU32(&paParms[1], &pListEntry->fInfo);
        /* Note: paParms[2] contains the entry index, currently being ignored. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&paParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&paParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
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
 * Gets an URI list open request from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pOpenParms          Where to store the open parameters of the request.
 */
static int vboxSvcClipboardURIGetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                          PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN)
    {
        uint32_t cbPath   = 0;
        uint32_t cbFilter = 0;

        rc = HGCMSvcGetU32(&paParms[1], &pOpenParms->fList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &cbPath);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetStr(&paParms[3], &pOpenParms->pszPath, &pOpenParms->cbPath);
            AssertReturn(cbPath == pOpenParms->cbPath, VERR_INVALID_PARAMETER);
        }
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[4], &cbFilter);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetStr(&paParms[5], &pOpenParms->pszFilter, &pOpenParms->cbFilter);
            AssertReturn(cbFilter == pOpenParms->cbFilter, VERR_INVALID_PARAMETER);
        }

        if (RT_SUCCESS(rc))
        {
            /** @todo Some more validation. */
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets an URI list open request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   pOpenParms          List open parameters to set.
 */
static int vboxSvcClipboardURISetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                          PVBOXSHCLMSGCTX pMsgCtx, PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN)
    {
        HGCMSvcSetU32(&paParms[0], pMsgCtx->uContextID);
        HGCMSvcSetU32(&paParms[1], pOpenParms->fList);
        HGCMSvcSetU32(&paParms[2], pOpenParms->cbFilter);
        HGCMSvcSetPv (&paParms[3], pOpenParms->pszFilter, pOpenParms->cbFilter);
        HGCMSvcSetU32(&paParms[4], pOpenParms->cbPath);
        HGCMSvcSetPv (&paParms[5], pOpenParms->pszPath, pOpenParms->cbPath);
        HGCMSvcSetU64(&paParms[6], 0); /* OUT: uHandle */

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets an URI list close request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   hList               Handle of list to close.
 */
static int vboxSvcClipboardURISetListClose(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                           PVBOXSHCLMSGCTX pMsgCtx, SHCLLISTHANDLE hList)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE)
    {
        HGCMSvcSetU32(&paParms[0], pMsgCtx->uContextID);
        HGCMSvcSetU64(&paParms[1], hList);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
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
static int vboxSvcClipboardURIGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                         PSHCLLISTHANDLE phList, PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR)
    {
        rc = HGCMSvcGetU64(&paParms[1], phList);
        /* Note: Flags (paParms[2]) not used here. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[4], &pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[5], &pListHdr->cbTotalSize);

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

#if 0
/**
 * Sets an URI list header to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   pListHdr            Pointer to data to set to the HGCM parameters.
 */
static int vboxSvcClipboardURISetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                         PVBOXSHCLMSGCTX pMsgCtx, PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR)
    {
        /** @todo Set pvMetaFmt + cbMetaFmt. */
        /** @todo Calculate header checksum. */

        HGCMSvcSetU32(&paParms[0], pMsgCtx->uContextID);
        HGCMSvcSetU32(&paParms[1], pListHdr->fFeatures);
        HGCMSvcSetU32(&paParms[2], 0 /* Features, will be returned on success */);
        HGCMSvcSetU64(&paParms[3], pListHdr->cTotalObjects);
        HGCMSvcSetU64(&paParms[4], pListHdr->cbTotalSize);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif

/**
 * Gets an URI list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListEntry          Where to store the list entry.
 */
static int vboxSvcClipboardURIGetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                           PSHCLLISTHANDLE phList, PSHCLLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
    {
        rc = HGCMSvcGetU64(&paParms[1], phList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pListEntry->fInfo);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&paParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&paParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
                AssertReturn(cbInfo == pListEntry->cbInfo, VERR_INVALID_PARAMETER);
            }
        }

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

#if 0
/**
 * Sets an URI data chunk to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   pListEntry          Pointer to data to set to the HGCM parameters.
 */
static int vboxSvcClipboardURISetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                           PVBOXSHCLMSGCTX pMsgCtx, PSHCLLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
    {
        /** @todo Calculate chunk checksum. */

        HGCMSvcSetU32(&paParms[0], pMsgCtx->uContextID);
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
#endif

/**
 * Gets an URI object data chunk from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pDataChunk          Where to store the object data chunk data.
 */
static int vboxSvcClipboardURIGetObjDataChunk(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PSHCLOBJDATACHUNK pDataChunk)
{
    AssertPtrReturn(paParms,    VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDataChunk, VERR_INVALID_PARAMETER);

    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_WRITE)
    {
        rc = HGCMSvcGetU64(&paParms[1], &pDataChunk->uHandle);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbData;
            rc = HGCMSvcGetU32(&paParms[2], &cbData);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[3], &pDataChunk->pvData, &pDataChunk->cbData);
                AssertReturn(cbData == pDataChunk->cbData, VERR_INVALID_PARAMETER);

                /** @todo Implement checksum handling. */
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets an URI error from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pRc                 Where to store the received error code.
 */
static int vboxSvcClipboardURIGetError(uint32_t cParms, VBOXHGCMSVCPARM paParms[], int *pRc)
{
    AssertPtrReturn(paParms, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pRc,     VERR_INVALID_PARAMETER);

    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_ERROR)
    {
        rc = HGCMSvcGetU32(&paParms[1], (uint32_t *)pRc); /** @todo int vs. uint32_t !!! */
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Handles a guest reply (VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY) message.
 *
 * @returns VBox status code.
 * @param   pClient             Pointer to associated client.
 * @param   pTransfer           Pointer to transfer to handle guest reply for.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters supplied.
 */
static int vboxSvcClipboardURITransferHandleReply(PSHCLCLIENT pClient, PSHCLURITRANSFER pTransfer,
                                                  uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pClient);

    int rc;

    uint32_t            cbReply = sizeof(SHCLREPLY);
    PSHCLREPLY pReply  = (PSHCLREPLY)RTMemAlloc(cbReply);
    if (pReply)
    {
        rc = vboxSvcClipboardURIGetReply(cParms, paParms, pReply);
        if (RT_SUCCESS(rc))
        {
            PSHCLEVENTPAYLOAD pPayload
                = (PSHCLEVENTPAYLOAD)RTMemAlloc(sizeof(SHCLEVENTPAYLOAD));
            if (pPayload)
            {
                pPayload->pvData = pReply;
                pPayload->cbData = cbReply;

                switch (pReply->uType)
                {
                    case VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_LIST_CLOSE:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE:
                    {
                        uint32_t uCID;
                        rc = HGCMSvcGetU32(&paParms[0], &uCID);
                        if (RT_SUCCESS(rc))
                        {
                            const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                            LogFlowFunc(("uCID=%RU32 -> uEvent=%RU32\n", uCID, uEvent));

                            rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
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
int vboxSvcClipboardURIHandler(PSHCLCLIENT pClient,
                               VBOXHGCMCALLHANDLE callHandle,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival)
{
    RT_NOREF(paParms, tsArrival);

    LogFlowFunc(("uClient=%RU32, u32Function=%RU32 (%s), cParms=%RU32, g_ExtState.pfnExtension=%p\n",
                 pClient->uClientID, u32Function, VBoxClipboardGuestMsgToStr(u32Function), cParms, g_ExtState.pfnExtension));

    /* Check if we've the right mode set. */
    if (!vboxSvcClipboardURIMsgIsAllowed(vboxSvcClipboardGetMode(), u32Function))
    {
        LogFunc(("Wrong clipboard mode, denying access\n"));
        return VERR_ACCESS_DENIED;
    }

    /* A (valid) service extension is needed because VBoxSVC needs to keep track of the
     * clipboard areas cached on the host. */
    if (!g_ExtState.pfnExtension)
    {
#ifdef DEBUG_andy
        AssertPtr(g_ExtState.pfnExtension);
#endif
        LogFunc(("Invalid / no service extension set, skipping URI handling\n"));
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    /*
     * Pre-check: For certain messages we need to make sure that a (right) transfer is present.
     */
    PSHCLURITRANSFER pTransfer = NULL;
    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS:
            break;
        default:
        {
            if (!SharedClipboardURICtxGetTotalTransfers(&pClient->URI))
            {
                LogFunc(("No transfers found\n"));
                rc = VERR_WRONG_ORDER;
                break;
            }

            const uint32_t uTransferID = 0; /* Only one transfer per client is supported at the moment. */

            pTransfer = SharedClipboardURICtxGetTransfer(&pClient->URI, uTransferID);
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
#if 0
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS:
        {
            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_STATUS)
                break;

            SHCLURITRANSFERSTATUS uStatus = SHCLURITRANSFERSTATUS_NONE;
            rc = HGCMSvcGetU32(&paParms[1], &uStatus);
            if (RT_FAILURE(rc))
                break;

            LogFlowFunc(("uStatus: %RU32\n", uStatus));

            SharedClipboardURICtxTransfersCleanup(&pClient->URI);

            if (SharedClipboardURICtxTransfersMaximumReached(&pClient->URI))
            {
                rc = VERR_SHCLPB_MAX_TRANSFERS_REACHED;
                break;
            }

            if (uStatus == SHCLURITRANSFERSTATUS_RUNNING)
            {
                const SHCLURITRANSFERDIR enmDir = SHCLURITRANSFERDIR_READ;

                PSHCLURITRANSFER pTransfer;
                rc = SharedClipboardURITransferCreate(enmDir,
                                                      SHCLSOURCE_REMOTE, &pTransfer);
                if (RT_SUCCESS(rc))
                {
                    rc = vboxSvcClipboardURIAreaRegister(&pClient->State, pTransfer);
                    if (RT_SUCCESS(rc))
                    {
                        SHCLPROVIDERCREATIONCTX creationCtx;
                        RT_ZERO(creationCtx);

                        creationCtx.enmSource = pClient->State.enmSource;

                        creationCtx.Interface.pfnTransferOpen  = vboxSvcClipboardURITransferOpen;
                        creationCtx.Interface.pfnTransferClose = vboxSvcClipboardURITransferClose;
                        creationCtx.Interface.pfnListOpen      = vboxSvcClipboardURIListOpen;
                        creationCtx.Interface.pfnListClose     = vboxSvcClipboardURIListClose;
                        creationCtx.Interface.pfnObjOpen       = vboxSvcClipboardURIObjOpen;
                        creationCtx.Interface.pfnObjClose      = vboxSvcClipboardURIObjClose;

                        if (enmDir == SHCLURITRANSFERDIR_READ)
                        {
                            creationCtx.Interface.pfnGetRoots        = vboxSvcClipboardURIGetRoots;
                            creationCtx.Interface.pfnListHdrRead     = vboxSvcClipboardURIListHdrRead;
                            creationCtx.Interface.pfnListEntryRead   = vboxSvcClipboardURIListEntryRead;
                            creationCtx.Interface.pfnObjRead         = vboxSvcClipboardURIObjRead;
                        }
                        else
                        {
                            AssertFailed();
                        }

                        creationCtx.pvUser = pClient;

                        /* Register needed callbacks so that we can wait for the meta data to arrive here. */
                        SHCLURITRANSFERCALLBACKS Callbacks;
                        RT_ZERO(Callbacks);

                        Callbacks.pvUser                = pClient;

                        Callbacks.pfnTransferPrepare    = VBoxSvcClipboardURITransferPrepareCallback;
                        Callbacks.pfnTransferComplete   = VBoxSvcClipboardURITransferCompleteCallback;
                        Callbacks.pfnTransferCanceled   = VBoxSvcClipboardURITransferCanceledCallback;
                        Callbacks.pfnTransferError      = VBoxSvcClipboardURITransferErrorCallback;

                        SharedClipboardURITransferSetCallbacks(pTransfer, &Callbacks);

                        rc = SharedClipboardURITransferSetInterface(pTransfer, &creationCtx);
                        if (RT_SUCCESS(rc))
                            rc = SharedClipboardURICtxTransferAdd(&pClient->URI, pTransfer);
                    }

                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxClipboardSvcImplURITransferCreate(pClient, pTransfer);
                        if (RT_SUCCESS(rc))
                            rc = VBoxClipboardSvcImplFormatAnnounce(pClient, VBOX_SHARED_CLIPBOARD_FMT_URI_LIST);
                    }

                    if (RT_FAILURE(rc))
                    {
                        VBoxClipboardSvcImplURITransferDestroy(pClient, pTransfer);
                        SharedClipboardURITransferDestroy(pTransfer);
                    }
                }
            }

            LogFlowFunc(("[Client %RU32] VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS: %Rrc\n", pClient->uClientID, rc));

            if (RT_FAILURE(rc))
                LogRel(("Shared Clipboard: Initializing transfer failed with %Rrc\n", rc));

            break;
        }
#endif

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY:
        {
            rc = vboxSvcClipboardURITransferHandleReply(pClient, pTransfer, cParms, paParms);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_READ:
        {
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_WRITE:
        {
            SHCLROOTLISTHDR lstHdr;
            rc = vboxSvcClipboardURIGetRootListHdr(cParms, paParms, &lstHdr);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = SharedClipboardURIRootListHdrDup(&lstHdr);
                uint32_t cbData = sizeof(SHCLROOTLISTHDR);

                uint32_t uCID;
                rc = HGCMSvcGetU32(&paParms[0], &uCID);
                if (RT_SUCCESS(rc))
                {
                    const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                    PSHCLEVENTPAYLOAD pPayload;
                    rc = SharedClipboardPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
                        if (RT_FAILURE(rc))
                            SharedClipboardPayloadFree(pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_READ:
        {
    #if 0
            SHCLROOTLISTENTRY lstEntry;
            rc = VBoxSvcClipboardURIGetRootListEntry(cParms, paParms, &lstEntry);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = SharedClipboardURIRootListEntryDup(&lstEntry);
                uint32_t cbData = sizeof(SHCLROOTLISTENTRY);

                PSHCLURITRANSFERPAYLOAD pPayload;
                rc = SharedClipboardURITransferPayloadAlloc(SHCLURITRANSFEREVENTTYPE_ROOT_LIST_HDR_READ,
                                                            pvData, cbData, &pPayload);
                if (RT_SUCCESS(rc))
                    rc = SharedClipboardURITransferEventSignal(pTransfer, SHCLURITRANSFEREVENTTYPE_ROOT_LIST_HDR_READ,
                                                               pPayload);
            }
            break;
    #endif
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_WRITE:
        {
            SHCLROOTLISTENTRY lstEntry;
            rc = vboxSvcClipboardURIGetRootListEntry(cParms, paParms, &lstEntry);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = SharedClipboardURIRootListEntryDup(&lstEntry);
                uint32_t cbData = sizeof(SHCLROOTLISTENTRY);

                uint32_t uCID;
                rc = HGCMSvcGetU32(&paParms[0], &uCID);
                if (RT_SUCCESS(rc))
                {
                    const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                    PSHCLEVENTPAYLOAD pPayload;
                    rc = SharedClipboardPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
                        if (RT_FAILURE(rc))
                            SharedClipboardPayloadFree(pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN:
        {
            SHCLLISTOPENPARMS listOpenParms;
            rc = vboxSvcClipboardURIGetListOpen(cParms, paParms, &listOpenParms);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = SharedClipboardURITransferListOpen(pTransfer, &listOpenParms, &hList);
                if (RT_SUCCESS(rc))
                {
                    /* Return list handle. */
                    HGCMSvcSetU32(&paParms[1], hList);
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE:
        {
            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList);
            if (RT_SUCCESS(rc))
            {
                rc = SharedClipboardURITransferListClose(pTransfer, hList);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ:
        {
            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHDR hdrList;
                rc = SharedClipboardURITransferListGetHeader(pTransfer, hList, &hdrList);
                /*if (RT_SUCCESS(rc))
                    rc = vboxSvcClipboardURISetListHdr(cParms, paParms, &hdrList);*/
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE:
        {
            SHCLLISTHDR hdrList;
            rc = SharedClipboardURIListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = vboxSvcClipboardURIGetListHdr(cParms, paParms, &hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListHdrDup(&hdrList);
                    uint32_t cbData = sizeof(SHCLLISTHDR);

                    uint32_t uCID;
                    rc = HGCMSvcGetU32(&paParms[0], &uCID);
                    if (RT_SUCCESS(rc))
                    {
                        const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                        PSHCLEVENTPAYLOAD pPayload;
                        rc = SharedClipboardPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                        if (RT_SUCCESS(rc))
                        {
                            rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
                            if (RT_FAILURE(rc))
                                SharedClipboardPayloadFree(pPayload);
                        }
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTENTRY entryList;
                rc = SharedClipboardURITransferListRead(pTransfer, hList, &entryList);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE:
        {
            SHCLLISTENTRY entryList;
            rc = SharedClipboardURIListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = vboxSvcClipboardURIGetListEntry(cParms, paParms, &hList, &entryList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListEntryDup(&entryList);
                    uint32_t cbData = sizeof(SHCLLISTENTRY);

                    uint32_t uCID;
                    rc = HGCMSvcGetU32(&paParms[0], &uCID);
                    if (RT_SUCCESS(rc))
                    {
                        const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                        PSHCLEVENTPAYLOAD pPayload;
                        rc = SharedClipboardPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                        if (RT_SUCCESS(rc))
                        {
                            rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
                            if (RT_FAILURE(rc))
                                SharedClipboardPayloadFree(pPayload);
                        }
                    }
                }
            }
            break;
        }

    #if 0
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN:
        {
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE:
        {
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ:
        {
            break;
        }
    #endif

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_WRITE:
        {
            SHCLOBJDATACHUNK dataChunk;
            rc = vboxSvcClipboardURIGetObjDataChunk(cParms, paParms, &dataChunk);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = SharedClipboardURIObjectDataChunkDup(&dataChunk);
                uint32_t cbData = sizeof(SHCLOBJDATACHUNK);

                uint32_t uCID;
                rc = HGCMSvcGetU32(&paParms[0], &uCID);
                if (RT_SUCCESS(rc))
                {
                    const SHCLEVENTID uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uCID);

                    PSHCLEVENTPAYLOAD pPayload;
                    rc = SharedClipboardPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardEventSignal(&pTransfer->Events, uEvent, pPayload);
                        if (RT_FAILURE(rc))
                            SharedClipboardPayloadFree(pPayload);
                    }
                }
            }

            break;
        }

    #if 0
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR\n"));

            SHCLDIRDATA dirData;
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
                        SHCLAREAOBJ Obj = { SHCLAREAOBJTYPE_DIR, SHCLAREAOBJSTATE_COMPLETE };
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

            SHCLFILEHDR fileHdr;
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

            SHCLFILEHDR fileHdr;
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
                        PSHCLCLIENTURIOBJCTX pObjCtx = SharedClipboardURITransferGetCurrentObjCtx(pTransfer);
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

                            SHCLAREAOBJ Obj = { SHCLAREAOBJTYPE_FILE, SHCLAREAOBJSTATE_NONE };
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

            SHCLFILEDATA fileData;
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

            SHCLFILEDATA fileData;
            rc = VBoxSvcClipboardURIGetFileData(cParms, paParms, &fileData);
            if (RT_SUCCESS(rc))
            {
                PSHCLCLIENTURIOBJCTX pObjCtx = SharedClipboardURITransferGetCurrentObjCtx(pTransfer);
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
        {
            LogRel2(("Shared Clipboard: Transfer canceled\n"));
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR:
        {
            int rcGuest;
            rc = vboxSvcClipboardURIGetError(cParms,paParms, &rcGuest);
            if (RT_SUCCESS(rc))
                LogRel(("Shared Clipboard: Transfer error: %Rrc\n", rcGuest));
            break;
        }

        default:
            LogFunc(("Not implemented\n"));
            break;
    }

    if (rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        /* Tell the client that the call is complete (unblocks waiting). */
        LogFlowFunc(("[Client %RU32] Calling pfnCallComplete w/ rc=%Rrc\n", pClient->uClientID, rc));
        AssertPtr(g_pHelpers);
        g_pHelpers->pfnCallComplete(callHandle, rc);
    }

    LogFlowFunc(("[Client %RU32] Returning rc=%Rrc\n", pClient->uClientID, rc));
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
        case VBOX_SHARED_CLIPBOARD_HOST_FN_CANCEL: /** @todo Implement this. */
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_FN_ERROR: /** @todo Implement this. */
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
int vboxSvcClipboardURIAreaRegister(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is registered for this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = NIL_SHCLAREAID;

        /* As the meta data is now complete, register a new clipboard on the host side. */
        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_REGISTER, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
        {
            /* Note: Do *not* specify SHCLAREA_OPEN_FLAGS_MUST_NOT_EXIST as flags here, as VBoxSVC took care of the
             *       clipboard area creation already. */
            rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */,
                                            SHCLAREA_OPEN_FLAGS_NONE);
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
int vboxSvcClipboardURIAreaUnregister(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = pTransfer->pArea->GetID();

        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_UNREGISTER, &parms, sizeof(parms));
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
int vboxSvcClipboardURIAreaAttach(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer,
                                  SHCLAREAID uID)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is attached to this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = uID; /* 0 means most recent clipboard area. */

        /* The client now needs to attach to the most recent clipboard area
         * to keep a reference to it. The host does the actual book keeping / cleanup then.
         *
         * This might fail if the host does not have a most recent clipboard area (yet). */
        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_ATTACH, &parms, sizeof(parms));
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
int vboxSvcClipboardURIAreaDetach(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    const uint32_t uAreaID = pTransfer->pArea->GetID();

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);
        parms.uID = uAreaID;

        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_DETACH, &parms, sizeof(parms));

        LogFlowFunc(("Detached client %RU32 from clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->u32ClientID, uAreaID, rc));
    }

    delete pTransfer->pArea;
    pTransfer->pArea = NULL;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

