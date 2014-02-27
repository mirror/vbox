/* $Id$ */
/** @file
 * Drag and Drop Service.
 */

/*
 * Copyright (C) 2011-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_svc_guest_control   Guest Control HGCM Service
 *
 * This service acts as a proxy for handling and buffering host command requests
 * and clients on the guest. It tries to be as transparent as possible to let
 * the guest (client) and host side do their protocol handling as desired.
 *
 * The following terms are used:
 * - Host:   A host process (e.g. VBoxManage or another tool utilizing the Main API)
 *           which wants to control something on the guest.
 * - Client: A client (e.g. VBoxService) running inside the guest OS waiting for
 *           new host commands to perform. There can be multiple clients connected
 *           to a service. A client is represented by its HGCM client ID.
 * - Context ID: An (almost) unique ID automatically generated on the host (Main API)
 *               to not only distinguish clients but individual requests. Because
 *               the host does not know anything about connected clients it needs
 *               an indicator which it can refer to later. This context ID gets
 *               internally bound by the service to a client which actually processes
 *               the command in order to have a relationship between client<->context ID(s).
 *
 * The host can trigger commands which get buffered by the service (with full HGCM
 * parameter info). As soon as a client connects (or is ready to do some new work)
 * it gets a buffered host command to process it. This command then will be immediately
 * removed from the command list. If there are ready clients but no new commands to be
 * processed, these clients will be set into a deferred state (that is being blocked
 * to return until a new command is available).
 *
 * If a client needs to inform the host that something happened, it can send a
 * message to a low level HGCM callback registered in Main. This callback contains
 * the actual data as well as the context ID to let the host do the next necessary
 * steps for this context. This context ID makes it possible to wait for an event
 * inside the host's Main API function (like starting a process on the guest and
 * wait for getting its PID returned by the client) as well as cancelling blocking
 * host calls in order the client terminated/crashed (HGCM detects disconnected
 * clients and reports it to this service's callback).
 */

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/
#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND

#include "dndmanager.h"

/******************************************************************************
 *   Service class declaration                                                *
 ******************************************************************************/

/**
 * Specialized drag & drop service class.
 */
class DragAndDropService: public HGCM::AbstractService<DragAndDropService>
{
public:

    explicit DragAndDropService(PVBOXHGCMSVCHELPERS pHelpers)
        : HGCM::AbstractService<DragAndDropService>(pHelpers)
        , m_pManager(0)
        , m_cClients(0)
    {}

protected:
    /* HGCM service implementation */
    int  init(VBOXHGCMSVCFNTABLE *pTable);
    int  uninit();
    int  clientConnect(uint32_t u32ClientID, void *pvClient);
    int  clientDisconnect(uint32_t u32ClientID, void *pvClient);
    void guestCall(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int  hostCall(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    static DECLCALLBACK(int) progressCallback(uint32_t uPercentage, uint32_t uState, int rc, void *pvUser);
    int modeSet(uint32_t u32Mode);
    inline uint32_t modeGet() { return m_u32Mode; };

    DnDManager             *m_pManager;

    uint32_t                m_cClients;
    RTCList<HGCM::Client*>  m_clientQueue;
    uint32_t                m_u32Mode;
};

/******************************************************************************
 *   Service class implementation                                             *
 ******************************************************************************/

int DragAndDropService::init(VBOXHGCMSVCFNTABLE *pTable)
{
    /* Register functions. */
    pTable->pfnHostCall          = svcHostCall;
    pTable->pfnSaveState         = NULL;  /* The service is stateless, so the normal */
    pTable->pfnLoadState         = NULL;  /* construction done before restoring suffices */
    pTable->pfnRegisterExtension = svcRegisterExtension;

    /* Drag'n drop mode is disabled by default. */
    modeSet(VBOX_DRAG_AND_DROP_MODE_OFF);

    m_pManager = new DnDManager(&DragAndDropService::progressCallback, this);

    return VINF_SUCCESS;
}

int DragAndDropService::uninit(void)
{
    delete m_pManager;

    return VINF_SUCCESS;
}

int DragAndDropService::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("New client (%RU32) connected\n", u32ClientID));
    if (m_cClients < UINT32_MAX)
        m_cClients++;
    else
        AssertMsgFailed(("Maximum number of clients reached\n"));

    /*
     * Clear the message queue as soon as a new clients connect
     * to ensure that every client has the same state.
     */
    if (m_pManager)
        m_pManager->clear();

    return VINF_SUCCESS;
}

int DragAndDropService::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    /* Remove all waiters with this u32ClientID. */
    for (size_t i = 0; i < m_clientQueue.size(); )
    {
        HGCM::Client *pClient = m_clientQueue.at(i);
        if (pClient->clientId() == u32ClientID)
        {
            if (m_pHelpers)
                m_pHelpers->pfnCallComplete(pClient->handle(), VERR_INTERRUPTED);

            m_clientQueue.removeAt(i);
            delete pClient;
        }
        else
            i++;
    }

    return VINF_SUCCESS;
}

int DragAndDropService::modeSet(uint32_t u32Mode)
{
    /** @todo Validate mode. */
    switch (u32Mode)
    {
        case VBOX_DRAG_AND_DROP_MODE_OFF:
        case VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST:
        case VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST:
        case VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL:
            m_u32Mode = u32Mode;
            break;

        default:
            m_u32Mode = VBOX_DRAG_AND_DROP_MODE_OFF;
            break;
    }

    return VINF_SUCCESS;
}

void DragAndDropService::guestCall(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID,
                                   void *pvClient, uint32_t u32Function,
                                   uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("u32ClientID=%RU32, u32Function=%RU32, cParms=%RU32\n",
                 u32ClientID, u32Function, cParms));

    /* Check if we've the right mode set. */
    int rc = VERR_ACCESS_DENIED; /* Play safe. */
    switch (u32Function)
    {
        case DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG:
            if (modeGet() != VBOX_DRAG_AND_DROP_MODE_OFF)
            {
                rc = VINF_SUCCESS;
            }
            else
            {
                LogFlowFunc(("DnD disabled, deferring request\n"));
                rc = VINF_HGCM_ASYNC_EXECUTE;
            }
            break;
        case DragAndDropSvc::GUEST_DND_HG_ACK_OP:
        case DragAndDropSvc::GUEST_DND_HG_REQ_DATA:
            if (   modeGet() == VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                || modeGet() == VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST)
            {
                rc = VINF_SUCCESS;
            }
            else
                LogFlowFunc(("Host -> Guest DnD mode disabled, ignoring request\n"));
            break;
        case DragAndDropSvc::GUEST_DND_GH_ACK_PENDING:
        case DragAndDropSvc::GUEST_DND_GH_SND_DATA:
        case DragAndDropSvc::GUEST_DND_GH_SND_DIR:
        case DragAndDropSvc::GUEST_DND_GH_SND_FILE:
        case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            if (   modeGet() == VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                || modeGet() == VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST)
            {
                rc = VINF_SUCCESS;
            }
            else
#endif
                LogFlowFunc(("Guest -> Host DnD mode disabled, ignoring request\n"));
            break;
        default:
            /* Reach through to DnD manager. */
            rc = VINF_SUCCESS;
            break;
    }

#ifdef DEBUG_andy
    LogFlowFunc(("Mode check rc=%Rrc\n", rc));
#endif

    if (rc == VINF_SUCCESS) /* Note: rc might be VINF_HGCM_ASYNC_EXECUTE! */
    {
        switch (u32Function)
        {
            /* Note: Older VBox versions with enabled DnD guest->host support (< 4.4)
             *       used the same message ID (300) for GUEST_DND_GET_NEXT_HOST_MSG and
             *       HOST_DND_GH_REQ_PENDING, which led this service returning
             *       VERR_INVALID_PARAMETER when the guest wanted to actually
             *       handle HOST_DND_GH_REQ_PENDING. */
            case DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG:
            {
                LogFlowFunc(("GUEST_DND_GET_NEXT_HOST_MSG\n"));
                if (   cParms != 3
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* message */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* parameter count */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* blocking */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    rc = m_pManager->nextMessageInfo(&paParms[0].u.uint32, &paParms[1].u.uint32);
                    if (   RT_FAILURE(rc)
                        && paParms[2].u.uint32) /* Blocking? */
                    {
                        /* Defer client returning. */
                        rc = VINF_HGCM_ASYNC_EXECUTE;
                    }
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_HG_ACK_OP:
            {
                LogFlowFunc(("GUEST_DND_HG_ACK_OP\n"));
                if (   cParms != 1
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* action */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBHGACKOPDATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_HG_ACK_OP;
                    paParms[0].getUInt32(&data.uAction); /* Get drop action. */
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_HG_REQ_DATA:
            {
                LogFlowFunc(("GUEST_DND_HG_REQ_DATA\n"));
                if (   cParms != 1
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR /* format */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBHGREQDATADATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_HG_REQ_DATA;
                    uint32_t cTmp;
                    paParms[0].getPointer((void**)&data.pszFormat, &cTmp);
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case DragAndDropSvc::GUEST_DND_GH_ACK_PENDING:
            {
                LogFlowFunc(("GUEST_DND_GH_ACK_PENDING\n"));
                if (   cParms != 3
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* defaction */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* allactions */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_PTR   /* format */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBGHACKPENDINGDATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_ACK_PENDING;
                    paParms[0].getUInt32(&data.uDefAction);
                    paParms[1].getUInt32(&data.uAllActions);
                    uint32_t cTmp;
                    paParms[2].getPointer((void**)&data.pszFormat, &cTmp);
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_GH_SND_DATA:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_DATA\n"));
                if (   cParms != 2
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* size */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBSNDDATADATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_SND_DATA;
                    paParms[0].getPointer((void**)&data.pvData, &data.cbData);
                    paParms[1].getUInt32(&data.cbTotalSize);
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_GH_SND_DIR:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_DIR\n"));
                if (   cParms != 3
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* path */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* path length */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* creation mode */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBSNDDIRDATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_SND_DIR;
                    uint32_t cTmp;
                    paParms[0].getPointer((void**)&data.pszPath, &cTmp);
                    paParms[1].getUInt32(&data.cbPath);
                    paParms[2].getUInt32(&data.fMode);
#ifdef DEBUG_andy
                    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n",
                                 data.pszPath, data.cbPath, data.fMode));
#endif
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_GH_SND_FILE:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_FILE\n"));
                if (   cParms != 5
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* file path */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* file path length */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_PTR   /* file data */
                    || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT /* file data length */
                    || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* creation mode */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBSNDFILEDATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_SND_FILE;
                    uint32_t cTmp;
                    paParms[0].getPointer((void**)&data.pszFilePath, &cTmp);
                    paParms[1].getUInt32(&data.cbFilePath);
                    paParms[2].getPointer((void**)&data.pvData, &data.cbData);
                    /* paParms[3] is cbData. */
                    paParms[4].getUInt32(&data.fMode);
#ifdef DEBUG_andy
                    LogFlowFunc(("pszFilePath=%s, cbData=%RU32, pvData=0x%p, fMode=0x%x\n",
                                 data.pszFilePath, data.cbData, data.pvData, data.fMode));
#endif
                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
            case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
            {
                LogFlowFunc(("GUEST_DND_GH_EVT_ERROR\n"));
                if (   cParms != 1
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* rc */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DragAndDropSvc::VBOXDNDCBEVTERRORDATA data;
                    data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_EVT_ERROR;

                    uint32_t rcOp;
                    paParms[0].getUInt32(&rcOp);
                    data.rc = rcOp;

                    if (m_pfnHostCallback)
                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                }
                break;
            }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
            default:
            {
                /* All other messages are handled by the DnD manager. */
                rc = m_pManager->nextMessage(u32Function, cParms, paParms);
                break;
            }
        }
    }

    /* If async execution is requested, we didn't notify the guest yet about
     * completion. The client is queued into the waiters list and will be
     * notified as soon as a new event is available. */
    if (rc == VINF_HGCM_ASYNC_EXECUTE)
    {
        m_clientQueue.append(new HGCM::Client(u32ClientID, callHandle,
                                              u32Function, cParms, paParms));
    }

    if (   rc != VINF_HGCM_ASYNC_EXECUTE
        && m_pHelpers)
    {
        m_pHelpers->pfnCallComplete(callHandle, rc);
    }

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
}

int DragAndDropService::hostCall(uint32_t u32Function,
                                 uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("u32Function=%RU32, cParms=%RU32\n", u32Function, cParms));

    int rc;
    if (u32Function == DragAndDropSvc::HOST_DND_SET_MODE)
    {
        if (cParms != 1)
            rc = VERR_INVALID_PARAMETER;
        else if (paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT)
            rc = VERR_INVALID_PARAMETER;
        else
            rc = modeSet(paParms[0].u.uint32);
    }
    else if (modeGet() != VBOX_DRAG_AND_DROP_MODE_OFF)
    {
        if (!m_clientQueue.isEmpty()) /* At least one client on the guest connected? */
        {
            rc = m_pManager->addMessage(u32Function, cParms, paParms);
            if (RT_SUCCESS(rc))
            {
                HGCM::Client *pClient = m_clientQueue.first();
                AssertPtr(pClient);

                /* Check if this was a request for getting the next host
                 * message. If so, return the message id and the parameter
                 * count. The message itself has to be queued. */
                uint32_t uMsg = pClient->message();
                if (uMsg == DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG)
                {
                    LogFlowFunc(("Client %RU32 is waiting for next host msg\n", pClient->clientId()));

                    uint32_t uMsg1;
                    uint32_t cParms1;
                    rc = m_pManager->nextMessageInfo(&uMsg1, &cParms1);
                    if (RT_SUCCESS(rc))
                    {
                        pClient->addMessageInfo(uMsg1, cParms1);
                        if (m_pHelpers)
                            m_pHelpers->pfnCallComplete(pClient->handle(), rc);

                        m_clientQueue.removeFirst();
                        delete pClient;
                    }
                    else
                        AssertMsgFailed(("m_pManager::nextMessageInfo failed with rc=%Rrc\n", rc));
                }
                else
                    AssertMsgFailed(("Client ID=%RU32 in wrong state with uMsg=%RU32\n",
                                     pClient->clientId(), uMsg));
            }
            else
                AssertMsgFailed(("Adding new message of type=%RU32 failed with rc=%Rrc\n",
                                 u32Function, rc));
        }
        else
        {
            /* Tell the host that the guest does not support drag'n drop.
             * This might happen due to not installed Guest Additions or
             * not running VBoxTray/VBoxClient. */
            rc = VERR_NOT_SUPPORTED;
        }
    }
    else
    {
        /* Tell the host that a wrong drag'n drop mode is set. */
        rc = VERR_ACCESS_DENIED;
    }

    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}

DECLCALLBACK(int) DragAndDropService::progressCallback(uint32_t uPercentage, uint32_t uState, int rc, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    DragAndDropService *pSelf = static_cast<DragAndDropService *>(pvUser);
    AssertPtr(pSelf);

    if (pSelf->m_pfnHostCallback)
    {
        LogFlowFunc(("GUEST_DND_HG_EVT_PROGRESS: uPercentage=%RU32, uState=%RU32, rc=%Rrc\n",
                     uPercentage, uState, rc));
        DragAndDropSvc::VBOXDNDCBHGEVTPROGRESSDATA data;
        data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_HG_EVT_PROGRESS;
        data.uPercentage  = RT_MIN(uPercentage, 100);
        data.uState       = uState;
        data.rc           = rc;

        return pSelf->m_pfnHostCallback(pSelf->m_pvHostData,
                                        DragAndDropSvc::GUEST_DND_HG_EVT_PROGRESS,
                                        &data, sizeof(data));
    }

    return VINF_SUCCESS;
}

/**
 * @copydoc VBOXHGCMSVCLOAD
 */
extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *pTable)
{
    return DragAndDropService::svcLoad(pTable);
}

