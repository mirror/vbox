/* $Id$ */
/** @file
 * Drag and Drop Service.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_HGCM

#include "dndmanager.h"

//# define DO(s) RTPrintf s
#define DO(s) do {} while(0)
//#define DO(s) Log s

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

    static DECLCALLBACK(int) progressCallback(unsigned uPercentage, uint32_t uState, void *pvUser);
    int      hostMessage(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    void     modeSet(uint32_t u32Mode);
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
    modeSet(VBOX_DRAG_AND_DROP_MODE_OFF);

    m_pManager = new DnDManager(&DragAndDropService::progressCallback, this);

    return VINF_SUCCESS;
}

int DragAndDropService::uninit()
{
    delete m_pManager;

    return VINF_SUCCESS;
}

int DragAndDropService::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    LogFlowFunc(("New client (%ld) connected\n", u32ClientID));
    DO(("New client (%ld) connected\n", u32ClientID));
    if (m_cClients < UINT32_MAX)
        m_cClients++;
    else
        AssertMsgFailed(("Maximum number of clients reached\n"));
    return VINF_SUCCESS;
}

int DragAndDropService::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    /* Remove all waiters with this clientId. */
    for (size_t i = 0; i < m_clientQueue.size(); )
    {
        HGCM::Client *pClient = m_clientQueue.at(i);
        if (pClient->clientId() == u32ClientID)
        {
            m_pHelpers->pfnCallComplete(pClient->handle(), VERR_INTERRUPTED);
            m_clientQueue.removeAt(i);
            delete pClient;
        }
        else
            i++;
    }

    return VINF_SUCCESS;
}

void DragAndDropService::modeSet(uint32_t u32Mode)
{
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
    }
}

void DragAndDropService::guestCall(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n",
                 u32ClientID, u32Function, cParms, paParms));
//    RTPrintf("u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n",
//                 u32ClientID, u32Function, cParms, paParms);

    switch (u32Function)
    {
        case DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG:
        {
            DO(("GUEST_DND_GET_NEXT_HOST_MSG\n"));
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
                    m_clientQueue.append(new HGCM::Client(u32ClientID, callHandle, u32Function, cParms, paParms));
                    rc = VINF_HGCM_ASYNC_EXECUTE;
                }
            }
            break;
        }
        case DragAndDropSvc::GUEST_DND_HG_ACK_OP:
        {
            DO(("GUEST_DND_HG_ACK_OP\n"));
            if (   modeGet() != VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                && modeGet() != VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST)
            {
                DO(("=> ignoring!\n"));
                break;
            }

            if (   cParms != 1
                || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* action */)
                rc = VERR_INVALID_PARAMETER;
            else
            {
                DragAndDropSvc::VBOXDNDCBHGACKOPDATA data;
                data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_HG_ACK_OP;
                paParms[0].getUInt32(&data.uAction);
                if (m_pfnHostCallback)
                    rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
//                m_pHelpers->pfnCallComplete(callHandle, rc);
            }
            break;
        }
        case DragAndDropSvc::GUEST_DND_HG_REQ_DATA:
        {
            DO(("GUEST_DND_HG_REQ_DATA\n"));
            if (   modeGet() != VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                && modeGet() != VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST)
            {
                DO(("=> ignoring!\n"));
                break;
            }

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
//                m_pHelpers->pfnCallComplete(callHandle, rc);
//                if (data.pszFormat)
//                    RTMemFree(data.pszFormat);
//                if (data.pszTmpPath)
//                    RTMemFree(data.pszTmpPath);
            }
            break;
        }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::GUEST_DND_GH_ACK_PENDING:
        {
            DO(("GUEST_DND_GH_ACK_PENDING\n"));
            if (   modeGet() != VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                && modeGet() != VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST)
            {
                DO(("=> ignoring!\n"));
                break;
            }

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
            DO(("GUEST_DND_GH_SND_DATA\n"));
            if (   modeGet() != VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                && modeGet() != VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST)
            {
                DO(("=> ignoring\n"));
                break;
            }

            if (   cParms != 2
                || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* size */)
                rc = VERR_INVALID_PARAMETER;
            else
            {
                DragAndDropSvc::VBOXDNDCBSNDDATADATA data;
                data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_GH_SND_DATA;
                paParms[0].getPointer((void**)&data.pvData, &data.cbData);
                paParms[1].getUInt32(&data.cbAllSize);
                if (m_pfnHostCallback)
                    rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
            }
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
        {
            DO(("GUEST_DND_GH_EVT_ERROR\n"));
            if (   modeGet() != VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                && modeGet() != VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST)
            {
                DO(("=> ignoring!\n"));
                break;
            }

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
#endif
        default:
        {
            /* All other messages are handled by the DnD manager. */
            rc = m_pManager->nextMessage(u32Function, cParms, paParms);
            /* Check for error. Buffer overflow is allowed. It signals the
             * guest to ask for more data in the next event. */
            if (   RT_FAILURE(rc)
                && rc != VERR_CANCELLED
                && rc != VERR_BUFFER_OVERFLOW) /* Buffer overflow is allowed. */
            {
                m_clientQueue.append(new HGCM::Client(u32ClientID, callHandle, u32Function, cParms, paParms));
                rc = VINF_HGCM_ASYNC_EXECUTE;
            }
            break;
        }
    }
    /* If async execute is requested, we didn't notify the guest about
     * completion. The client is queued into the waiters list and will be
     * notified as soon as a new event is available. */
    if (rc != VINF_HGCM_ASYNC_EXECUTE)
        m_pHelpers->pfnCallComplete(callHandle, rc);
    DO(("guest call: %Rrc\n", rc));
}

int DragAndDropService::hostMessage(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;
#if 0
    HGCM::Message *pMessage = new HGCM::Message(u32Function, cParms, paParms);
    m_hostQueue.push(pMessage);
//    bool fPush = true;
    RTPrintf("client queue %u\n", m_clientQueue.size());
    RTPrintf("host   queue %u\n", m_hostQueue.size());
    if (!m_clientQueue.empty())
    {
        pMessage = m_hostQueue.front();
        HGCM::Client *pClient = m_clientQueue.front();
        /* Check if this was a request for getting the next host
         * message. If so, return the message id and the parameter
         * count. The message itself has to be queued. */
        if (pClient->message() == DragAndDropSvc::GUEST_GET_NEXT_HOST_MSG)
        {
            RTPrintf("client is waiting for next host msg\n");
//            rc = VERR_TOO_MUCH_DATA;
            pClient->addMessageInfo(pMessage);
            /* temp */
//        m_pHelpers->pfnCallComplete(pClient->handle(), rc);
//        m_clientQueue.pop();
//        delete pClient;
        }
        else
        {
            RTPrintf("client is waiting for host msg (%d)\n", u32Function);
            /* There is a request for a host message pending. Check
             * if this is the correct message and if so deliver. If
             * not the message will be queued. */
            rc = pClient->addMessage(pMessage);
            m_hostQueue.pop();
            delete pMessage;
//            if (RT_SUCCESS(rc))
//                fPush = false;
        }
        /* In any case mark this client request as done. */
        m_pHelpers->pfnCallComplete(pClient->handle(), rc);
        m_clientQueue.pop_front();
        delete pClient;
    }
//    if (fPush)
//    {
//        RTPrintf("push message\n");
//        m_hostQueue.push(pMessage);
//    }
//    else
//        delete pMessage;
#endif

    return rc;
}

int DragAndDropService::hostCall(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;
    if (u32Function == DragAndDropSvc::HOST_DND_SET_MODE)
    {
        if (cParms != 1)
            rc = VERR_INVALID_PARAMETER;
        else if (paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT)
            rc = VERR_INVALID_PARAMETER;
        else
            modeSet(paParms[0].u.uint32);
    }
    else if (modeGet() != VBOX_DRAG_AND_DROP_MODE_OFF)
    {
        rc = m_pManager->addMessage(u32Function, cParms, paParms);
        if (    RT_SUCCESS(rc)
            && !m_clientQueue.isEmpty())
        {
            HGCM::Client *pClient = m_clientQueue.first();
            /* Check if this was a request for getting the next host
             * message. If so, return the message id and the parameter
             * count. The message itself has to be queued. */
            if (pClient->message() == DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG)
            {
                DO(("client is waiting for next host msg\n"));
//              rc = m_pManager->nextMessageInfo(&paParms[0].u.uint32, &paParms[1].u.uint32);
                uint32_t uMsg1;
                uint32_t cParms1;
                rc = m_pManager->nextMessageInfo(&uMsg1, &cParms1);
                if (RT_SUCCESS(rc))
                {
                    pClient->addMessageInfo(uMsg1, cParms1);
                    m_pHelpers->pfnCallComplete(pClient->handle(), rc);
                    m_clientQueue.removeFirst();
                    delete pClient;
                }
                else
                    AssertMsgFailed(("Should not happen!"));
            }
            else
                AssertMsgFailed(("Should not happen!"));
        }
//      else
//          AssertMsgFailed(("Should not happen %Rrc!", rc));
    }

    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}

DECLCALLBACK(int) DragAndDropService::progressCallback(unsigned uPercentage, uint32_t uState, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    DragAndDropService *pSelf = static_cast<DragAndDropService *>(pvUser);

    if (pSelf->m_pfnHostCallback)
    {
        DO(("GUEST_DND_HG_EVT_PROGRESS %u\n", uPercentage));
        DragAndDropSvc::VBOXDNDCBHGEVTPROGRESSDATA data;
        data.hdr.u32Magic = DragAndDropSvc::CB_MAGIC_DND_HG_EVT_PROGRESS;
        data.uPercentage  = uPercentage;
        data.uState       = uState;

        return pSelf->m_pfnHostCallback(pSelf->m_pvHostData, DragAndDropSvc::GUEST_DND_HG_EVT_PROGRESS, &data, sizeof(data));
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

