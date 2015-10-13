/* $Id$ */
/** @file
 * Drag and Drop Service.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_svc_guest_control   Drag and drop HGCM Service
 *
 * TODO
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND

#include <map>

#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/Service.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#include "dndmanager.h"

using namespace DragAndDropSvc;

/*********************************************************************************************************************************
*   Service class declaration                                                                                                    *
*********************************************************************************************************************************/

/** Map holding pointers to HGCM clients. Key is the (unique) HGCM client ID. */
typedef std::map<uint32_t, HGCM::Client*> DnDClientMap;

/**
 * Specialized drag & drop service class.
 */
class DragAndDropService : public HGCM::AbstractService<DragAndDropService>
{
public:

    explicit DragAndDropService(PVBOXHGCMSVCHELPERS pHelpers)
        : HGCM::AbstractService<DragAndDropService>(pHelpers)
        , m_pManager(NULL) {}

protected:

    int  init(VBOXHGCMSVCFNTABLE *pTable);
    int  uninit(void);
    int  clientConnect(uint32_t u32ClientID, void *pvClient);
    int  clientDisconnect(uint32_t u32ClientID, void *pvClient);
    void guestCall(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int  hostCall(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    int modeSet(uint32_t u32Mode);
    inline uint32_t modeGet() { return m_u32Mode; };

protected:

    static DECLCALLBACK(int) progressCallback(uint32_t uStatus, uint32_t uPercentage, int rc, void *pvUser);

protected:

    DnDManager             *m_pManager;
    /** Map of all connected clients. */
    DnDClientMap            m_clientMap;
    /** List of all clients which are queued up (deferred return) and ready
     *  to process new commands. */
    RTCList<HGCM::Client*>  m_clientQueue;
    uint32_t                m_u32Mode;
};


/*********************************************************************************************************************************
*   Service class implementation                                                                                                 *
*********************************************************************************************************************************/

int DragAndDropService::init(VBOXHGCMSVCFNTABLE *pTable)
{
    /* Register functions. */
    pTable->pfnHostCall          = svcHostCall;
    pTable->pfnSaveState         = NULL;  /* The service is stateless, so the normal */
    pTable->pfnLoadState         = NULL;  /* construction done before restoring suffices */
    pTable->pfnRegisterExtension = svcRegisterExtension;

    /* Drag'n drop mode is disabled by default. */
    modeSet(VBOX_DRAG_AND_DROP_MODE_OFF);

    int rc = VINF_SUCCESS;

    try
    {
        m_pManager = new DnDManager(&DragAndDropService::progressCallback, this);
    }
    catch(std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragAndDropService::uninit(void)
{
    if (m_pManager)
    {
        delete m_pManager;
        m_pManager = NULL;
    }

    return VINF_SUCCESS;
}

int DragAndDropService::clientConnect(uint32_t u32ClientID, void *pvClient)
{
    if (m_clientMap.size() >= UINT8_MAX) /* Don't allow too much clients at the same time. */
    {
        AssertMsgFailed(("Maximum number of clients reached\n"));
        return VERR_MAX_PROCS_REACHED;
    }

    int rc = VINF_SUCCESS;

    /*
     * Add client to our client map.
     */
    if (m_clientMap.find(u32ClientID) != m_clientMap.end())
        rc = VERR_ALREADY_EXISTS;

    if (RT_SUCCESS(rc))
    {
        try
        {
            m_clientMap[u32ClientID] = new HGCM::Client(u32ClientID);
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            /*
             * Clear the message queue as soon as a new clients connect
             * to ensure that every client has the same state.
             */
            if (m_pManager)
                m_pManager->clear();
        }
    }

    LogFlowFunc(("Client %RU32 connected, rc=%Rrc\n", u32ClientID, rc));
    return rc;
}

int DragAndDropService::clientDisconnect(uint32_t u32ClientID, void *pvClient)
{
    /* Client not found? Bail out early. */
    DnDClientMap::iterator itClient =  m_clientMap.find(u32ClientID);
    if (itClient == m_clientMap.end())
        return VERR_NOT_FOUND;

    /*
     * Remove from waiters queue.
     */
    for (size_t i = 0; i < m_clientQueue.size(); i++)
    {
        HGCM::Client *pClient = m_clientQueue.at(i);
        if (pClient->clientId() == u32ClientID)
        {
            if (m_pHelpers)
                m_pHelpers->pfnCallComplete(pClient->handle(), VERR_INTERRUPTED);

            m_clientQueue.removeAt(i);
            delete pClient;

            break;
        }
    }

    /*
     * Remove from client map and deallocate.
     */
    AssertPtr(itClient->second);
    delete itClient->second;

    m_clientMap.erase(itClient);

    LogFlowFunc(("Client %RU32 disconnected\n", u32ClientID));
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
        case GUEST_DND_GET_NEXT_HOST_MSG:
        {
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
        }

        /* New since protocol v2. */
        case GUEST_DND_CONNECT:
        {
            /*
             * Never block the initial connect call, as the clients do this when
             * initializing and might get stuck if drag and drop is set to "disabled" at
             * that time.
             */
            rc = VINF_SUCCESS;
            break;
        }
        case GUEST_DND_HG_ACK_OP:
            /* Fall through is intentional. */
        case GUEST_DND_HG_REQ_DATA:
            /* Fall through is intentional. */
        case GUEST_DND_HG_EVT_PROGRESS:
        {
            if (   modeGet() == VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL
                || modeGet() == VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST)
            {
                rc = VINF_SUCCESS;
            }
            else
                LogFlowFunc(("Host -> Guest DnD mode disabled, ignoring request\n"));
            break;
        }

        case GUEST_DND_GH_ACK_PENDING:
        case GUEST_DND_GH_SND_DATA_HDR:
        case GUEST_DND_GH_SND_DATA:
        case GUEST_DND_GH_SND_DIR:
        case GUEST_DND_GH_SND_FILE_HDR:
        case GUEST_DND_GH_SND_FILE_DATA:
        case GUEST_DND_GH_EVT_ERROR:
        {
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
        }

        default:
            /* Reach through to DnD manager. */
            rc = VINF_SUCCESS;
            break;
    }

#ifdef DEBUG_andy
    LogFlowFunc(("Mode (%RU32) check rc=%Rrc\n", modeGet(), rc));
#endif

#define DO_HOST_CALLBACK();                                                     \
    if (   RT_SUCCESS(rc)                                                       \
        && m_pfnHostCallback)                                                   \
    {                                                                           \
        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data)); \
    }

    if (rc == VINF_SUCCESS) /* Note: rc might be VINF_HGCM_ASYNC_EXECUTE! */
    {
        DnDClientMap::iterator itClient =  m_clientMap.find(u32ClientID);
        Assert(itClient != m_clientMap.end());

        HGCM::Client *pClient = itClient->second;
        AssertPtr(pClient);

        LogFlowFunc(("Client %RU32: Protocol v%RU32\n", pClient->clientId(), pClient->protocol()));

        rc = VERR_INVALID_PARAMETER; /* Play safe. */

        switch (u32Function)
        {
            /*
             * Note: Older VBox versions with enabled DnD guest->host support (< 5.0)
             *       used the same message ID (300) for GUEST_DND_GET_NEXT_HOST_MSG and
             *       HOST_DND_GH_REQ_PENDING, which led this service returning
             *       VERR_INVALID_PARAMETER when the guest wanted to actually
             *       handle HOST_DND_GH_REQ_PENDING.
             */
            case GUEST_DND_GET_NEXT_HOST_MSG:
            {
                LogFlowFunc(("GUEST_DND_GET_NEXT_HOST_MSG\n"));
                if (cParms == 3)
                {
                    rc = m_pManager->nextMessageInfo(&paParms[0].u.uint32 /* uMsg */, &paParms[1].u.uint32 /* cParms */);
                    if (RT_FAILURE(rc)) /* No queued messages available? */
                    {
                        if (m_pfnHostCallback) /* Try asking the host. */
                        {
                            VBOXDNDCBHGGETNEXTHOSTMSG data;
                            data.hdr.u32Magic = CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG;
                            rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                            if (RT_SUCCESS(rc))
                            {
                                paParms[0].u.uint32 = data.uMsg;   /* uMsg */
                                paParms[1].u.uint32 = data.cParms; /* cParms */
                                /* Note: paParms[2] was set by the guest as blocking flag. */
                            }
                        }
                        else /* No host callback in place, so drag and drop is not supported by the host. */
                            rc = VERR_NOT_SUPPORTED;

                        if (RT_FAILURE(rc))
                            rc = m_pManager->nextMessage(u32Function, cParms, paParms);

                        /* Some error occurred or no (new) messages available? */
                        if (RT_FAILURE(rc))
                        {
                            if (paParms[2].u.uint32) /* Blocking flag set? */
                            {
                                /* Defer client returning. */
                                rc = VINF_HGCM_ASYNC_EXECUTE;
                            }

                            LogFlowFunc(("Message queue is empty, returning %Rrc to guest\n", rc));
                        }
                    }
                }
                break;
            }
            case GUEST_DND_CONNECT:
            {
                LogFlowFunc(("GUEST_DND_CONNECT\n"));
                if (cParms == 2)
                {
                    uint32_t uProtocol;
                    rc = paParms[0].getUInt32(&uProtocol); /* Get protocol version. */
                    if (RT_SUCCESS(rc))
                        rc = pClient->setProtocol(uProtocol);
                    if (RT_SUCCESS(rc))
                    {
                        LogFlowFunc(("Client %RU32 is now using protocol v%RU32\n", pClient->clientId(), pClient->protocol()));
                        /** @todo Handle connection flags (paParms[1]). */
                    }

                    /* Note: Does not reach the host; the client's protocol version
                     *       is only kept in this service. */
                }
                break;
            }
            case GUEST_DND_HG_ACK_OP:
            {
                LogFlowFunc(("GUEST_DND_HG_ACK_OP\n"));
                if (cParms == 1)
                {
                    VBOXDNDCBHGACKOPDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_HG_ACK_OP;
                    rc = paParms[0].getUInt32(&data.uAction); /* Get drop action. */
                    DO_HOST_CALLBACK();
                }
                break;
            }
            case GUEST_DND_HG_REQ_DATA:
            {
                LogFlowFunc(("GUEST_DND_HG_REQ_DATA\n"));
                if (cParms == 1)
                {
                    VBOXDNDCBHGREQDATADATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_HG_REQ_DATA;
                    rc = paParms[0].getPointer((void**)&data.pszFormat, &data.cbFormat);
                    DO_HOST_CALLBACK();
                }
                break;
            }
            case GUEST_DND_HG_EVT_PROGRESS:
            {
                LogFlowFunc(("GUEST_DND_HG_EVT_PROGRESS\n"));
                if (cParms == 3)
                {
                    VBOXDNDCBHGEVTPROGRESSDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_HG_EVT_PROGRESS;
                    rc = paParms[0].getUInt32(&data.uStatus);
                    if (RT_SUCCESS(rc))
                        rc = paParms[1].getUInt32(&data.uPercentage);
                    if (RT_SUCCESS(rc))
                        rc = paParms[2].getUInt32(&data.rc);
                    DO_HOST_CALLBACK();
                }
                break;
            }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case GUEST_DND_GH_ACK_PENDING:
            {
                LogFlowFunc(("GUEST_DND_GH_ACK_PENDING\n"));
                if (cParms == 3)
                {
                    VBOXDNDCBGHACKPENDINGDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_GH_ACK_PENDING;
                    rc = paParms[0].getUInt32(&data.uDefAction);
                    if (RT_SUCCESS(rc))
                        rc = paParms[1].getUInt32(&data.uAllActions);
                    if (RT_SUCCESS(rc))
                        rc = paParms[2].getPointer((void**)&data.pszFormat, &data.cbFormat);
                    DO_HOST_CALLBACK();
                }
                break;
            }
            /* New since protocol v3. */
            case GUEST_DND_GH_SND_DATA_HDR:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_DATA_HDR\n"));
                if (cParms == 12)
                {
                    VBOXDNDCBSNDDATAHDRDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_DATA_HDR;
                    rc = paParms[0].getUInt32(&data.hdr.u32ContextID);
                    if (RT_SUCCESS(rc))
                        rc = paParms[1].getUInt32(&data.data.uFlags);
                    if (RT_SUCCESS(rc))
                        rc = paParms[2].getUInt32(&data.data.uScreenId);
                    if (RT_SUCCESS(rc))
                        rc = paParms[3].getUInt64(&data.data.cbTotal);
                    if (RT_SUCCESS(rc))
                        rc = paParms[4].getUInt32(&data.data.cbMeta);
                    if (RT_SUCCESS(rc))
                        rc = paParms[5].getPointer(&data.data.pvMetaFmt, &data.data.cbMetaFmt);
                    if (RT_SUCCESS(rc))
                        rc = paParms[6].getUInt32(&data.data.cbMetaFmt);
                    if (RT_SUCCESS(rc))
                        rc = paParms[7].getUInt64(&data.data.cObjects);
                    if (RT_SUCCESS(rc))
                        rc = paParms[8].getUInt32(&data.data.enmCompression);
                    if (RT_SUCCESS(rc))
                        rc = paParms[9].getUInt32((uint32_t *)&data.data.enmChecksumType);
                    if (RT_SUCCESS(rc))
                        rc = paParms[10].getPointer(&data.data.pvChecksum, &data.data.cbChecksum);
                    if (RT_SUCCESS(rc))
                        rc = paParms[11].getUInt32(&data.data.cbChecksum);

                    LogFlowFunc(("fFlags=0x%x, cbTotalSize=%RU64, cObj=%RU64\n",
                                 data.data.uFlags, data.data.cbTotal, data.data.cObjects));
                    DO_HOST_CALLBACK();
                }
                break;
            }
            case GUEST_DND_GH_SND_DATA:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_DATA\n"));
                switch (pClient->protocol())
                {
                    case 3:
                    {
                        if (cParms == 5)
                        {
                            VBOXDNDCBSNDDATADATA data;
                            data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_DATA;
                            rc = paParms[0].getUInt32(&data.hdr.u32ContextID);
                            if (RT_SUCCESS(rc))
                                rc = paParms[1].getPointer((void**)&data.data.u.v3.pvData, &data.data.u.v3.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[2].getUInt32(&data.data.u.v3.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[3].getPointer((void**)&data.data.u.v3.pvChecksum, &data.data.u.v3.cbChecksum);
                            if (RT_SUCCESS(rc))
                                rc = paParms[4].getUInt32(&data.data.u.v3.cbChecksum);
                            DO_HOST_CALLBACK();
                        }
                        break;
                    }

                    case 2:
                    default:
                    {
                        if (cParms == 2)
                        {
                            VBOXDNDCBSNDDATADATA data;
                            data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_DATA;
                            rc = paParms[0].getPointer((void**)&data.data.u.v1.pvData, &data.data.u.v1.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[1].getUInt32(&data.data.u.v1.cbTotalSize);
                            DO_HOST_CALLBACK();
                        }
                        break;
                    }
                }
                break;
            }
            case GUEST_DND_GH_SND_DIR:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_DIR\n"));
                if (cParms == 3)
                {
                    VBOXDNDCBSNDDIRDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_DIR;
                    uint32_t cTmp;
                    rc = paParms[0].getPointer((void**)&data.pszPath, &cTmp);
                    if (RT_SUCCESS(rc))
                        rc = paParms[1].getUInt32(&data.cbPath);
                    if (RT_SUCCESS(rc))
                        rc = paParms[2].getUInt32(&data.fMode);

                    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n", data.pszPath, data.cbPath, data.fMode));
                    DO_HOST_CALLBACK();
                }
                break;
            }
            /* New since protocol v2 (>= VBox 5.0). */
            case GUEST_DND_GH_SND_FILE_HDR:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_FILE_HDR\n"));
                if (cParms == 6)
                {
                    VBOXDNDCBSNDFILEHDRDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_FILE_HDR;
                    uint32_t cTmp;
                    /* paParms[0] is context ID; unused yet. */
                    rc = paParms[1].getPointer((void**)&data.pszFilePath, &cTmp);
                    if (RT_SUCCESS(rc))
                        rc = paParms[2].getUInt32(&data.cbFilePath);
                    if (RT_SUCCESS(rc))
                        rc = paParms[3].getUInt32(&data.fFlags);
                    if (RT_SUCCESS(rc))
                        rc = paParms[4].getUInt32(&data.fMode);
                    if (RT_SUCCESS(rc))
                        rc = paParms[5].getUInt64(&data.cbSize);

                    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x, cbSize=%RU64\n",
                                 data.pszFilePath, data.cbFilePath, data.fMode, data.cbSize));
                    DO_HOST_CALLBACK();
                }
                break;
            }
            case GUEST_DND_GH_SND_FILE_DATA:
            {
                LogFlowFunc(("GUEST_DND_GH_SND_FILE_DATA\n"));

                switch (pClient->protocol())
                {
                    /* Protocol v3 adds (optional) checksums. */
                    case 3:
                    {
                        if (cParms == 5)
                        {
                            VBOXDNDCBSNDFILEDATADATA data;
                            data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_FILE_DATA;
                            rc = paParms[0].getUInt32(&data.hdr.u32ContextID);
                            if (RT_SUCCESS(rc))
                                rc = paParms[1].getPointer((void**)&data.pvData, &data.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[2].getUInt32(&data.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[3].getPointer((void**)&data.u.v3.pvChecksum, &data.u.v3.cbChecksum);
                            if (RT_SUCCESS(rc))
                                rc = paParms[4].getUInt32(&data.u.v3.cbChecksum);

                            LogFlowFunc(("pvData=0x%p, cbData=%RU32\n", data.pvData, data.cbData));
                            DO_HOST_CALLBACK();
                        }
                        break;
                    }
                    /* Protocol v2 only sends the next data chunks to reduce traffic. */
                    case 2:
                    {
                        if (cParms == 3)
                        {
                            VBOXDNDCBSNDFILEDATADATA data;
                            data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_FILE_DATA;
                            rc = paParms[0].getUInt32(&data.hdr.u32ContextID);
                            if (RT_SUCCESS(rc))
                                rc = paParms[1].getPointer((void**)&data.pvData, &data.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[2].getUInt32(&data.cbData);

                            LogFlowFunc(("cbData=%RU32, pvData=0x%p\n", data.cbData, data.pvData));
                            DO_HOST_CALLBACK();
                        }
                        break;
                    }
                    /* Protocol v1 sends the file path and attributes for every file chunk (!). */
                    default:
                    {
                        if (cParms == 5)
                        {
                            VBOXDNDCBSNDFILEDATADATA data;
                            data.hdr.u32Magic = CB_MAGIC_DND_GH_SND_FILE_DATA;
                            uint32_t cTmp;
                            rc = paParms[0].getPointer((void**)&data.u.v1.pszFilePath, &cTmp);
                            if (RT_SUCCESS(rc))
                                rc = paParms[1].getUInt32(&data.u.v1.cbFilePath);
                            if (RT_SUCCESS(rc))
                                rc = paParms[2].getPointer((void**)&data.pvData, &cTmp);
                            if (RT_SUCCESS(rc))
                                rc = paParms[3].getUInt32(&data.cbData);
                            if (RT_SUCCESS(rc))
                                rc = paParms[4].getUInt32(&data.u.v1.fMode);

                            LogFlowFunc(("pszFilePath=%s, cbData=%RU32, pvData=0x%p, fMode=0x%x\n",
                                         data.u.v1.pszFilePath, data.cbData, data.pvData, data.u.v1.fMode));
                            DO_HOST_CALLBACK();
                        }
                        break;
                    }
                }
                break;
            }
            case GUEST_DND_GH_EVT_ERROR:
            {
                LogFlowFunc(("GUEST_DND_GH_EVT_ERROR\n"));
                if (cParms == 1)
                {
                    VBOXDNDCBEVTERRORDATA data;
                    data.hdr.u32Magic = CB_MAGIC_DND_GH_EVT_ERROR;

                    uint32_t rcOp;
                    rc = paParms[0].getUInt32(&rcOp);
                    if (RT_SUCCESS(rc))
                        data.rc = rcOp;

                    DO_HOST_CALLBACK();
                }
                break;
            }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
            default:
            {
                /* All other messages are handled by the DnD manager. */
                rc = m_pManager->nextMessage(u32Function, cParms, paParms);
                if (rc == VERR_NO_DATA) /* Manager has no new messsages? Try asking the host. */
                {
                    if (m_pfnHostCallback)
                    {
                        VBOXDNDCBHGGETNEXTHOSTMSGDATA data;
                        data.hdr.u32Magic = CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG_DATA;
                        data.uMsg    = u32Function;
                        data.cParms  = cParms;
                        data.paParms = paParms;

                        rc = m_pfnHostCallback(m_pvHostData, u32Function, &data, sizeof(data));
                        if (RT_SUCCESS(rc))
                        {
                            cParms  = data.cParms;
                            paParms = data.paParms;
                        }
                    }
                    else /* No host callback in place, so drag and drop is not supported by the host. */
                        rc = VERR_NOT_SUPPORTED;
                }
                break;
            }
        }
    }

    /*
     * If async execution is requested, we didn't notify the guest yet about
     * completion. The client is queued into the waiters list and will be
     * notified as soon as a new event is available.
     */
    if (rc == VINF_HGCM_ASYNC_EXECUTE)
    {
        try
        {
            LogFlowFunc(("Deferring guest call completion of client ID=%RU32\n", u32ClientID));
            m_clientQueue.append(new HGCM::Client(u32ClientID, callHandle,
                                                  u32Function, cParms, paParms));
        }
        catch (std::bad_alloc)
        {
            rc = VERR_NO_MEMORY;
            /* Don't report to guest. */
        }
    }
    else if (m_pHelpers)
    {
        /* Complete call on guest side. */
        m_pHelpers->pfnCallComplete(callHandle, rc);
    }
    else
        rc = VERR_NOT_IMPLEMENTED;

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
}

int DragAndDropService::hostCall(uint32_t u32Function,
                                 uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("u32Function=%RU32, cParms=%RU32, cClients=%zu, cQueue=%zu\n",
                 u32Function, cParms, m_clientMap.size(), m_clientQueue.size()));

    int rc;
    if (u32Function == HOST_DND_SET_MODE)
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
        if (m_clientMap.size()) /* At least one client on the guest connected? */
        {
            rc = m_pManager->addMessage(u32Function, cParms, paParms, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                if (m_clientQueue.size()) /* Any clients in our queue ready for processing the next command? */
                {
                    HGCM::Client *pClient = m_clientQueue.first();
                    AssertPtr(pClient);

                    /*
                     * Check if this was a request for getting the next host
                     * message. If so, return the message ID and the parameter
                     * count. The message itself has to be queued.
                     */
                    uint32_t uMsg = pClient->message();
                    if (uMsg == GUEST_DND_GET_NEXT_HOST_MSG)
                    {
                        LogFlowFunc(("Client %RU32 is waiting for next host msg\n", pClient->clientId()));

                        uint32_t uMsg1;
                        uint32_t cParms1;
                        rc = m_pManager->nextMessageInfo(&uMsg1, &cParms1);
                        if (RT_SUCCESS(rc))
                        {
                            pClient->addMessageInfo(uMsg1, cParms1);
                            if (   m_pHelpers
                                && m_pHelpers->pfnCallComplete)
                            {
                                m_pHelpers->pfnCallComplete(pClient->handle(), rc);
                            }

                            m_clientQueue.removeFirst();

                            delete pClient;
                            pClient = NULL;
                        }
                        else
                            AssertMsgFailed(("m_pManager::nextMessageInfo failed with rc=%Rrc\n", rc));
                    }
                    else
                        AssertMsgFailed(("Client ID=%RU32 in wrong state with uMsg=%RU32\n",
                                         pClient->clientId(), uMsg));
                }
                else
                    LogFlowFunc(("All clients busy; delaying execution\n"));
            }
            else
                AssertMsgFailed(("Adding new message of type=%RU32 failed with rc=%Rrc\n",
                                 u32Function, rc));
        }
        else
        {
            /*
             * Tell the host that the guest does not support drag'n drop.
             * This might happen due to not installed Guest Additions or
             * not running VBoxTray/VBoxClient.
             */
            rc = VERR_NOT_SUPPORTED;
        }
    }
    else
    {
        /* Tell the host that a wrong drag'n drop mode is set. */
        rc = VERR_ACCESS_DENIED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) DragAndDropService::progressCallback(uint32_t uStatus, uint32_t uPercentage, int rc, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    DragAndDropService *pSelf = static_cast<DragAndDropService *>(pvUser);
    AssertPtr(pSelf);

    if (pSelf->m_pfnHostCallback)
    {
        LogFlowFunc(("GUEST_DND_HG_EVT_PROGRESS: uStatus=%RU32, uPercentage=%RU32, rc=%Rrc\n",
                     uStatus, uPercentage, rc));

        VBOXDNDCBHGEVTPROGRESSDATA data;
        data.hdr.u32Magic = CB_MAGIC_DND_HG_EVT_PROGRESS;
        data.uPercentage  = RT_MIN(uPercentage, 100);
        data.uStatus      = uStatus;
        data.rc           = rc; /** @todo uin32_t vs. int. */

        return pSelf->m_pfnHostCallback(pSelf->m_pvHostData,
                                        GUEST_DND_HG_EVT_PROGRESS,
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

