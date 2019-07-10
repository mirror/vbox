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
extern PVBOXHGCMSVCHELPERS g_pHelpers;

extern ClipboardClientQueue g_listClientsDeferred;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
int VBoxSvcClipboardURISetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                   PVBOXCLIPBOARDLISTOPENPARMS pOpenParms);


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

DECLCALLBACK(int) vboxSvcClipboardURIGetRoots(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                              char **ppapszRoots, uint32_t *pcRoots)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENT pClient = (PVBOXCLIPBOARDCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    size_t   cbRootsRecv = 0;

    char    *pszRoots = NULL;
    uint32_t cRoots   = 0;

    /* There might be more than one message needed for retrieving all root items. */
    for (;;)
    {
        PVBOXCLIPBOARDCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOTS,
                                                                VBOX_SHARED_CLIPBOARD_CPARMS_ROOTS);
        if (pMsg)
        {
            HGCMSvcSetU32(&pMsg->m_paParms[0], 0 /* uContextID */);
            HGCMSvcSetU32(&pMsg->m_paParms[1], 0 /* fRoots */);
            HGCMSvcSetU32(&pMsg->m_paParms[2], 0 /* fMore */);
            HGCMSvcSetU32(&pMsg->m_paParms[3], 0 /* cRoots */);

            uint32_t  cbData = _64K;
            void     *pvData = RTMemAlloc(cbData);
            AssertPtrBreakStmt(pvData, rc = VERR_NO_MEMORY);

            HGCMSvcSetU32(&pMsg->m_paParms[4], cbData);
            HGCMSvcSetPv (&pMsg->m_paParms[5], pvData, cbData);

            rc = vboxSvcClipboardMsgAdd(pClient->pData, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = SharedClipboardURITransferEventRegister(pCtx->pTransfer,
                                                                  SHAREDCLIPBOARDURITRANSFEREVENTTYPE_GET_ROOTS);
                AssertRC(rc2);

                vboxSvcClipboardClientWakeup(pClient);
            }
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
            rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_GET_ROOTS,
                                                     30 * 1000 /* Timeout in ms */, &pPayload);
            if (RT_SUCCESS(rc))
            {
                PVBOXCLIPBOARDROOTS pRoots = (PVBOXCLIPBOARDROOTS)pPayload->pvData;
                Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDROOTS));

                LogFlowFunc(("cbRoots=%RU32, fRoots=%RU32, fMore=%RTbool\n", pRoots->cbRoots, pRoots->fRoots, pRoots->fMore));

                if (!pRoots->cbRoots)
                    break;
                AssertPtr(pRoots->pszRoots);

                if (pszRoots == NULL)
                    pszRoots = (char *)RTMemDup((void *)pRoots->pszRoots, pRoots->cbRoots);
                else
                    pszRoots = (char *)RTMemRealloc(pszRoots, cbRootsRecv + pRoots->cbRoots);

                AssertPtrBreakStmt(pszRoots, rc = VERR_NO_MEMORY);

                cbRootsRecv += pRoots->cbRoots;

                if (cbRootsRecv > _32M) /* Don't allow more than 32MB root entries for now. */
                {
                    rc = VERR_ALLOCATION_TOO_BIG; /** @todo Find a better rc. */
                    break;
                }

                cRoots += pRoots->cRoots;

                const bool fDone = !RT_BOOL(pRoots->fMore); /* More root entries to be retrieved? Otherwise bail out. */

                SharedClipboardURITransferPayloadFree(pPayload);

                if (fDone)
                    break;
            }
        }

        if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("cRoots=%RU32\n", cRoots));

        *ppapszRoots = pszRoots;
        *pcRoots     = cRoots;
    }
    else
    {
        RTMemFree(pszRoots);
        pszRoots = NULL;
    }

    LogFlowFuncLeave();
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                              PVBOXCLIPBOARDLISTOPENPARMS pOpenParms, PSHAREDCLIPBOARDLISTHANDLE phList)
{
    RT_NOREF(phList);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENT pClient = (PVBOXCLIPBOARDCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PVBOXCLIPBOARDCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN);
    if (pMsg)
    {
        rc = VBoxSvcClipboardURISetListOpen(pMsg->m_cParms, pMsg->m_paParms, pOpenParms);
        if (RT_SUCCESS(rc))
        {
            rc = vboxSvcClipboardMsgAdd(pClient->pData, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = SharedClipboardURITransferEventRegister(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_OPEN);
                AssertRC(rc2);

                vboxSvcClipboardClientWakeup(pClient);
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
        rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_OPEN,
                                                 30 * 1000 /* Timeout in ms */, &pPayload);
        if (RT_SUCCESS(rc))
        {
            Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDREPLY));

            PVBOXCLIPBOARDREPLY pReply = (PVBOXCLIPBOARDREPLY)pPayload->pvData;
            AssertPtr(pReply);

            Assert(pReply->uType == VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_OPEN);

            *phList = pReply->u.ListOpen.uHandle;

            SharedClipboardURITransferPayloadFree(pPayload);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDLISTHANDLE hList)
{
    RT_NOREF(pCtx, hList);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                 SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENT pClient = (PVBOXCLIPBOARDCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PVBOXCLIPBOARDCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ_REQ);
    if (pMsg)
    {
        HGCMSvcSetU32(&pMsg->m_paParms[0], 0 /* uContextID */);
        HGCMSvcSetU64(&pMsg->m_paParms[1], hList);
        HGCMSvcSetU32(&pMsg->m_paParms[2], 0 /* fFlags */);

        rc = vboxSvcClipboardMsgAdd(pClient->pData, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardURITransferEventRegister(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_WRITE);
            AssertRC(rc2);

            vboxSvcClipboardClientWakeup(pClient);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
        rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_WRITE,
                                                 30 * 1000 /* Timeout in ms */, &pPayload);
        if (RT_SUCCESS(rc))
        {
            Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDLISTHDR));

            *pListHdr = *(PVBOXCLIPBOARDLISTHDR)pPayload->pvData;

            SharedClipboardURITransferPayloadFree(pPayload);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListHdrWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                  SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                   SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCLIENT pClient = (PVBOXCLIPBOARDCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PVBOXCLIPBOARDCLIENTMSG pMsg = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ,
                                                            VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ_REQ);
    if (pMsg)
    {
        HGCMSvcSetU32(&pMsg->m_paParms[0], 0 /* uContextID */);
        HGCMSvcSetU64(&pMsg->m_paParms[1], hList);
        HGCMSvcSetU32(&pMsg->m_paParms[2], 0 /* fInfo */);

        rc = vboxSvcClipboardMsgAdd(pClient->pData, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardURITransferEventRegister(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_WRITE);
            if (rc2 == VERR_ALREADY_EXISTS)
                rc2 = VINF_SUCCESS;
            AssertRC(rc2);

            vboxSvcClipboardClientWakeup(pClient);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
        rc = SharedClipboardURITransferEventWait(pCtx->pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_WRITE,
                                                 30 * 1000 /* Timeout in ms */, &pPayload);
        if (RT_SUCCESS(rc))
        {
            Assert(pPayload->cbData == sizeof(VBOXCLIPBOARDLISTENTRY));

            rc = SharedClipboardURIListEntryCopy(pListEntry, (PVBOXCLIPBOARDLISTENTRY)pPayload->pvData);

            SharedClipboardURITransferPayloadFree(pPayload);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) vboxSvcClipboardURIListEntryWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                                    SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pListEntry)
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
    RT_NOREF(pData);

    LogFlowFuncEnter();
}

DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(pData, rc);

    LogFlowFuncEnter();

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
 * Gets an URI message reply from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pReply              Where to store the reply.
 */
int VBoxSvcClipboardURIGetReply(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                PVBOXCLIPBOARDREPLY pReply)
{
    int rc;

    if (cParms >= VBOX_SHARED_CLIPBOARD_CPARMS_REPLY_MIN)
    {
        uint32_t cbPayload = 0;

        /* Note: Context ID (paParms[0]) not used yet. */
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
            switch (pReply->uType)
            {
                case VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_OPEN:
                {
                    if (cParms >= 6)
                    {
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ListOpen.uHandle);
                    }
                    else
                        rc = VERR_INVALID_PARAMETER;
                    break;
                }

                case VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_OBJ_OPEN:
                {
                    if (cParms >= 6)
                    {
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ObjOpen.uHandle);
                    }
                    else
                        rc = VERR_INVALID_PARAMETER;
                    break;
                }

                default:
                    break;
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxSvcClipboardURIGetRoots(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                PVBOXCLIPBOARDROOTS pRoots)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_ROOTS)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
        rc = HGCMSvcGetU32(&paParms[1], &pRoots->fRoots);
        if (RT_SUCCESS(rc))
        {
            uint32_t fMore;
            rc = HGCMSvcGetU32(&paParms[2], &fMore);
            if (RT_SUCCESS(rc))
                pRoots->fMore = RT_BOOL(fMore);
        }
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &pRoots->cRoots);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbRoots;
            rc = HGCMSvcGetU32(&paParms[4], &cbRoots);
            if (RT_SUCCESS(rc))
                rc = HGCMSvcGetPv(&paParms[5], (void **)&pRoots->pszRoots, &pRoots->cbRoots);

            AssertReturn(cbRoots == pRoots->cbRoots, VERR_INVALID_PARAMETER);
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
int VBoxSvcClipboardURIGetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                   PVBOXCLIPBOARDLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN)
    {
        uint32_t cbPath   = 0;
        uint32_t cbFilter = 0;

        /* Note: Context ID (paParms[0]) not used yet. */
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

int VBoxSvcClipboardURISetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                   PVBOXCLIPBOARDLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN)
    {
        HGCMSvcSetU32(&paParms[0], 0 /* uContextID */);
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
 * Gets an URI list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListHdr            Where to store the list header.
 */
int VBoxSvcClipboardURIGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                  PSHAREDCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
        rc = HGCMSvcGetU64(&paParms[1], phList);
        /* Note: Flags (paParms[2]) not used here. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[4], &pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[5], &pListHdr->cbTotalSize);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[6], &pListHdr->enmCompression);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[7], (uint32_t *)&pListHdr->enmChecksumType);

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

    if (   cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR
        || cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR)
    {
        /** @todo Set pvMetaFmt + cbMetaFmt. */
        /** @todo Calculate header checksum. */

        HGCMSvcSetU32(&paParms[0], 0 /* uContextID */);
        HGCMSvcSetU32(&paParms[1], pListHdr->fFeatures);
        HGCMSvcSetU32(&paParms[2], 0 /* Features, will be returned on success */);
        HGCMSvcSetU64(&paParms[3], pListHdr->cTotalObjects);
        HGCMSvcSetU64(&paParms[4], pListHdr->cbTotalSize);
        HGCMSvcSetU32(&paParms[5], pListHdr->enmCompression);
        HGCMSvcSetU32(&paParms[6], pListHdr->enmChecksumType);

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
                                    PSHAREDCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
    {
        /* Note: Context ID (paParms[0]) not used yet. */
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

    if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
    {
        /** @todo Calculate chunk checksum. */

        HGCMSvcSetU32(&paParms[0], 0 /* uContextID */);
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

/**
 * Gets an URI error from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pRc                 Where to store the received error code.
 */
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

int VBoxSvcClipboardURITransferHandleReply(PVBOXCLIPBOARDCLIENT pClient, PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                           uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pClient);

    int rc;

    uint32_t            cbReply = sizeof(VBOXCLIPBOARDREPLY);
    PVBOXCLIPBOARDREPLY pReply  = (PVBOXCLIPBOARDREPLY)RTMemAlloc(cbReply);
    if (pReply)
    {
        rc = VBoxSvcClipboardURIGetReply(cParms, paParms, pReply);
        if (RT_SUCCESS(rc))
        {
            PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload
                = (PSHAREDCLIPBOARDURITRANSFERPAYLOAD)RTMemAlloc(sizeof(SHAREDCLIPBOARDURITRANSFERPAYLOAD));
            if (pPayload)
            {
                pPayload->pvData = pReply;
                pPayload->cbData = cbReply;

                switch (pReply->uType)
                {
                    case VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_OPEN:
                    {
                        rc = SharedClipboardURITransferEventSignal(pTransfer,
                                                                   SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_OPEN, pPayload);
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
int vboxSvcClipboardURIHandler(PVBOXCLIPBOARDCLIENT pClient,
                               VBOXHGCMCALLHANDLE callHandle,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival)
{
    RT_NOREF(paParms, tsArrival);

    LogFlowFunc(("uClient=%RU32, u32Function=%RU32 (%s), cParms=%RU32, g_pfnExtension=%p\n",
                 pClient->uClientID, u32Function, VBoxSvcClipboardGuestMsgToStr(u32Function), cParms, g_pfnExtension));

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
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_NOWAIT:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS:
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
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS:
        {
            LogFlowFunc(("[Client %RU32] VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS\n", pClient->uClientID));

            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_STATUS)
                break;

            SHAREDCLIPBOARDURITRANSFERSTATUS uStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_NONE;
            rc = HGCMSvcGetU32(&paParms[1], &uStatus);
            if (RT_FAILURE(rc))
                break;

            LogFlowFunc(("uStatus: %RU32\n", uStatus));

            if (   uStatus == SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING
                && !SharedClipboardURICtxTransfersMaximumReached(&pClientData->URI))
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
                            creationCtx.Interface.pfnGetRoots      = vboxSvcClipboardURIGetRoots;
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

                        rc = SharedClipboardURITransferSetInterface(pTransfer, &creationCtx);
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

            LogFlowFunc(("[Client %RU32] VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS: %Rrc\n", pClient->uClientID, rc));

            if (RT_FAILURE(rc))
                LogRel(("Shared Clipboard: Initializing transfer failed with %Rrc\n", rc));

            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_NOWAIT:
        {
            rc = vboxSvcClipboardMsgPeek(pClient, callHandle, cParms, paParms, false /*fWait*/);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT:
        {
            rc = vboxSvcClipboardMsgPeek(pClient, callHandle, cParms, paParms, true /*fWait*/);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET:
        {
            rc = vboxSvcClipboardMsgGet(pClient, callHandle, cParms, paParms);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY:
        {
            rc = VBoxSvcClipboardURITransferHandleReply(pClient, pTransfer, cParms, paParms);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOTS:
        {
            VBOXCLIPBOARDROOTS Roots;
            rc = VBoxSvcClipboardURIGetRoots(cParms, paParms, &Roots);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = SharedClipboardURIRootsDup(&Roots);
                uint32_t cbData = sizeof(VBOXCLIPBOARDROOTS);

                PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
                rc = SharedClipboardURITransferPayloadAlloc(SHAREDCLIPBOARDURITRANSFEREVENTTYPE_GET_ROOTS,
                                                            pvData, cbData, &pPayload);
                if (RT_SUCCESS(rc))
                    rc = SharedClipboardURITransferEventSignal(pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_GET_ROOTS,
                                                               pPayload);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN:
        {
            VBOXCLIPBOARDLISTOPENPARMS listOpenParms;
            rc = VBoxSvcClipboardURIGetListOpen(cParms, paParms, &listOpenParms);
            if (RT_SUCCESS(rc))
            {
                SHAREDCLIPBOARDLISTHANDLE hList;
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

            SHAREDCLIPBOARDLISTHANDLE hList;
            /* Note: Context ID (paParms[0]) not used yet. */
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

            SHAREDCLIPBOARDLISTHANDLE hList;
            /* Note: Context ID (paParms[0]) not used yet. */
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                VBOXCLIPBOARDLISTHDR hdrList;
                rc = SharedClipboardURITransferListGetHeader(pTransfer, hList, &hdrList);
                if (RT_SUCCESS(rc))
                    rc = VBoxSvcClipboardURISetListHdr(cParms, paParms, &hdrList);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE:
        {
            VBOXCLIPBOARDLISTHDR hdrList;
            rc = SharedClipboardURIListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                SHAREDCLIPBOARDLISTHANDLE hList;
                rc = VBoxSvcClipboardURIGetListHdr(cParms, paParms, &hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListHdrDup(&hdrList);
                    uint32_t cbData = sizeof(VBOXCLIPBOARDLISTHDR);

                    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
                    rc = SharedClipboardURITransferPayloadAlloc(SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_WRITE,
                                                                pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferEventSignal(pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_WRITE,
                                                                   pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY)
                break;

            SHAREDCLIPBOARDLISTHANDLE hList;
            /* Note: Context ID (paParms[0]) not used yet. */
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                VBOXCLIPBOARDLISTENTRY entryList;
                rc = SharedClipboardURITransferListRead(pTransfer, hList, &entryList);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE:
        {
            VBOXCLIPBOARDLISTENTRY entryList;
            rc = SharedClipboardURIListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                SHAREDCLIPBOARDLISTHANDLE hList;
                rc = VBoxSvcClipboardURIGetListEntry(cParms, paParms, &hList, &entryList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = SharedClipboardURIListEntryDup(&entryList);
                    uint32_t cbData = sizeof(VBOXCLIPBOARDLISTENTRY);

                    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
                    rc = SharedClipboardURITransferPayloadAlloc(SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_WRITE,
                                                                pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferEventSignal(pTransfer, SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_WRITE,
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
        {
            LogRel2(("Shared Clipboard: Transfer canceled\n"));
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR:
        {
            int rcGuest;
            rc = VBoxSvcClipboardURIGetError(cParms,paParms, &rcGuest);
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

