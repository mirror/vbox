/* $Id$ */
/** @file
 * Drag and Drop manager.
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

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND

#include "dndmanager.h"

#include <VBox/log.h>
#include <iprt/file.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/uri.h>

/******************************************************************************
 *   Private declarations                                                     *
 ******************************************************************************/

typedef DECLCALLBACK(int) FNDNDPRIVATEPROGRESS(size_t cbDone, void *pvUser);
typedef FNDNDPRIVATEPROGRESS *PFNDNDPRIVATEPROGRESS;

/**
 * Internal DnD message class for informing the
 * guest about a new directory.
 *
 * @see DnDHGSendDataMessage
 */
class DnDHGSendDirPrivate: public DnDMessage
{
public:

    DnDHGSendDirPrivate(DnDURIObject URIObject,
                        PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser)
        : m_URIObject(URIObject)
        , m_pfnProgressCallback(pfnProgressCallback)
        , m_pvProgressUser(pvProgressUser)
    {
        RTCString strPath = m_URIObject.GetDestPath();
        LogFlowFunc(("strPath=%s (%zu)\n", strPath.c_str(), strPath.length()));

        VBOXHGCMSVCPARM paTmpParms[3];
        paTmpParms[0].setString(strPath.c_str());
        paTmpParms[1].setUInt32((uint32_t)(strPath.length() + 1));
        paTmpParms[2].setUInt32(m_URIObject.GetMode());

        m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_DIR, 3, paTmpParms);
    }

public:

    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        int rc = DnDMessage::currentMessage(uMsg, cParms, paParms);
        /* Advance progress info */
        if (   RT_SUCCESS(rc)
            && m_pfnProgressCallback)
            rc = m_pfnProgressCallback(m_URIObject.GetSize(), m_pvProgressUser);

        return rc;
    }

protected:

    DnDURIObject           m_URIObject;

    /* Progress stuff. */
    PFNDNDPRIVATEPROGRESS  m_pfnProgressCallback;
    void                  *m_pvProgressUser;
};

/**
 * Internal DnD message class for informing the guest about a new file.
 *
 * @see DnDHGSendDataMessage
 */
class DnDHGSendFilePrivate: public DnDMessage
{
public:

    DnDHGSendFilePrivate(DnDURIObject URIObject,
                         PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser);
    virtual ~DnDHGSendFilePrivate(void);

public:

    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

protected:

    DnDURIObject           m_URIObject;
    /** Skeleton parameters for the next upcoming message in case
     *  the file data didn't fit completely into the first one. */
    VBOXHGCMSVCPARM        m_aSkelParms[5];

    /* Progress stuff. */
    PFNDNDPRIVATEPROGRESS  m_pfnProgressCallback;
    void                  *m_pvProgressUser;
};

/**
 * Internal DnD message class for informing the guest about new drag & drop
 * data.
 *
 * @see DnDHGSendDataMessage
 */
class DnDHGSendDataMessagePrivate: public DnDMessage
{
public:

    DnDHGSendDataMessagePrivate(uint32_t uMsg, uint32_t cParms,
                                VBOXHGCMSVCPARM paParms[],
                                PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser);
    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

protected:
    size_t                 m_cbSize;
    size_t                 m_cbDone;

    /* Progress stuff. */
    PFNDNDPRIVATEPROGRESS  m_pfnProgressCallback;
    void                  *m_pvProgressUser;
};

/******************************************************************************
 *   Implementation                                                           *
 ******************************************************************************/

/******************************************************************************
 *   DnDHGSendFilePrivate                                                     *
 ******************************************************************************/

DnDHGSendFilePrivate::DnDHGSendFilePrivate(DnDURIObject URIObject,
                                           PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser)
    : m_URIObject(URIObject)
    , m_pfnProgressCallback(pfnProgressCallback)
    , m_pvProgressUser(pvProgressUser)
{
    LogFlowFunc(("strPath=%s (%zu)\n",
                 m_URIObject.GetDestPath().c_str(), m_URIObject.GetDestPath().length()));

    m_aSkelParms[0].setString(m_URIObject.GetDestPath().c_str()); /* pvName */
    m_aSkelParms[1].setUInt32((uint32_t)(m_URIObject.GetDestPath().length() + 1)); /* cbName */
    m_aSkelParms[2].setPointer(NULL, 0); /* pvData */
    m_aSkelParms[3].setUInt32(0); /* cbData */
    m_aSkelParms[4].setUInt32(m_URIObject.GetMode()); /* fMode */

    m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_FILE, 5, m_aSkelParms);
}

DnDHGSendFilePrivate::~DnDHGSendFilePrivate(void)
{
}

int DnDHGSendFilePrivate::currentMessage(uint32_t uMsg, uint32_t cParms,
                                         VBOXHGCMSVCPARM paParms[])
{
    if (!m_pNextMsg)
        return VERR_NO_DATA;

    int rc = m_pNextMsg->getData(uMsg, cParms, paParms);
    clearNextMsg();
    if (RT_FAILURE(rc))
        return rc;

    uint32_t cbRead;
    if (RT_SUCCESS(rc))
    {
        /* Get buffer size + pointer to buffer from guest side. */
        uint32_t cbToRead = paParms[2].u.pointer.size; /* cbData */
        Assert(cbToRead);
        void *pvBuf = paParms[2].u.pointer.addr; /* pvData */
        AssertPtr(pvBuf);

        rc = m_URIObject.Read(pvBuf, cbToRead, &cbRead);
        LogFlowFunc(("Read %RU32 bytes (%RU32 bytes buffer) for \"%s\", rc=%Rrc\n",
                     cbRead, cbToRead, m_URIObject.GetDestPath().c_str(), rc));

        if (RT_LIKELY(RT_SUCCESS(rc)))
        {
            /* Tell the guest the actual size read. */
            paParms[3].setUInt32((uint32_t)cbRead); /* cbData */
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (!m_URIObject.IsComplete())
        {
            try
            {
                /* More data needed to send over. Prepare the next message. */
                m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_FILE, 5 /* cParms */,
                                               m_aSkelParms);
            }
            catch(std::bad_alloc &)
            {
                rc = VERR_NO_MEMORY;
            }
        }

        /* Advance progress info. */
        if (   RT_SUCCESS(rc)
            && m_pfnProgressCallback)
        {
            rc = m_pfnProgressCallback(cbRead, m_pvProgressUser);
        }
    }

    return rc;
}

/******************************************************************************
 *   DnDHGSendDataMessagePrivate                                                *
 ******************************************************************************/

DnDHGSendDataMessagePrivate::DnDHGSendDataMessagePrivate(uint32_t uMsg, uint32_t cParms,
                                                         VBOXHGCMSVCPARM paParms[],
                                                         PFNDNDPRIVATEPROGRESS pfnProgressCallback,
                                                         void *pvProgressUser)
    : m_cbSize(paParms[4].u.uint32)
    , m_cbDone(0)
    , m_pfnProgressCallback(pfnProgressCallback)
    , m_pvProgressUser(pvProgressUser)
{
    /* Create the initial data message. This might throw
     * a bad_alloc exception. */
    m_pNextMsg = new HGCM::Message(uMsg, cParms, paParms);
}

int DnDHGSendDataMessagePrivate::currentMessage(uint32_t uMsg, uint32_t cParms,
                                                VBOXHGCMSVCPARM paParms[])
{
    /** @todo Don't copy the data parts ... just move the data pointer in
     *        the original data ptr. */
    if (!m_pNextMsg)
        return VERR_NO_DATA;

    int rc = VINF_SUCCESS;

    HGCM::Message *pCurMsg = m_pNextMsg;
    AssertPtr(pCurMsg);

    m_pNextMsg = 0;
    rc = pCurMsg->getData(uMsg, cParms, paParms);

    /* Depending on the current message, the data pointer is on a
     * different position (HOST_DND_HG_SND_DATA=3;
     * HOST_DND_HG_SND_MORE_DATA=0). */
    int iPos = uMsg == DragAndDropSvc::HOST_DND_HG_SND_DATA ? 3 : 0;
    m_cbDone += paParms[iPos + 1].u.uint32;

    /* Info + data send already? */
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        paParms[iPos + 1].u.uint32 = paParms[iPos].u.pointer.size;
        VBOXHGCMSVCPARM paTmpParms[2];
        void     *pvOldData;
        uint32_t  cOldData;
        pCurMsg->getParmPtrInfo(iPos, &pvOldData, &cOldData);
        paTmpParms[0].setPointer(static_cast<uint8_t*>(pvOldData) + paParms[iPos].u.pointer.size, cOldData - paParms[iPos].u.pointer.size);
        paTmpParms[1].setUInt32(cOldData - paParms[iPos].u.pointer.size);

        try
        {
            m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA, 2, paTmpParms);
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (pCurMsg)
        delete pCurMsg;

    /* Advance progress info. */
    if (   RT_SUCCESS(rc)
        && m_pfnProgressCallback)
    {
        rc = m_pfnProgressCallback(m_cbDone, m_pvProgressUser);
    }

    return rc;
}

/******************************************************************************
 *   DnDHGSendDataMessage                                                       *
 ******************************************************************************/

/*
 * This class is a meta message class. It doesn't consist of any own message
 * data, but handle the meta info, the data itself as well as any files or
 * directories which have to be transfered to the guest.
 */
DnDHGSendDataMessage::DnDHGSendDataMessage(uint32_t uMsg, uint32_t cParms,
                                           VBOXHGCMSVCPARM paParms[],
                                           PFNDNDPROGRESS pfnProgressCallback,
                                           void *pvProgressUser)
    : m_cbTotal(0)
    , m_cbTransfered(0)
    , m_pfnProgressCallback(pfnProgressCallback)
    , m_pvProgressUser(pvProgressUser)
{
    if (cParms < 5) /* Paranoia. */
        return;

    const char *pszFormat = static_cast<const char*>(paParms[1].u.pointer.addr);
    uint32_t cbFormat = paParms[1].u.pointer.size;

    int rc = VINF_SUCCESS;
    RTCString strNewURIs;

    /* Do we need to build up a file tree? */
    if (DnDMIMEHasFileURLs(pszFormat, cbFormat))
    {
        const char *pszList = static_cast<const char*>(paParms[3].u.pointer.addr);
        AssertPtr(pszList);
        uint32_t cbList = paParms[3].u.pointer.size;
        Assert(cbList);

        LogFlowFunc(("Old data (%RU32 bytes): '%s'\n", cbList, pszList));

        /* The list is separated by newline (even if only one file is listed). */
        RTCList<RTCString> lstURIOrg
            = RTCString(pszList, cbList).split("\r\n");
        if (!lstURIOrg.isEmpty())
        {
            rc = m_lstURI.AppendURIPathsFromList(lstURIOrg, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                /* Add the total size of all meta data + files transferred to
                 * the message's total byte count. */
                m_cbTotal += m_lstURI.TotalBytes();

                /* We have to change the actual DnD data. Remove any host paths and
                 * just decode the filename into the new data. The Guest Additions will
                 * add the correct path again before sending the DnD drop event to
                 * some window. */
                strNewURIs = m_lstURI.RootToString();

                /* Note: We don't delete the old pointer here, cause this is done
                 *       by the caller. We just use the RTString data, which has the
                 *       scope of this ctor. This is enough cause the data is copied in
                 *       the DnDHGSendDataMessagePrivate anyway. */
                paParms[3].u.pointer.addr = (void *)strNewURIs.c_str();
                paParms[3].u.pointer.size = (uint32_t)(strNewURIs.length() + 1);
                paParms[4].u.uint32       = (uint32_t)(strNewURIs.length() + 1);

                LogFlowFunc(("Set new data (%RU32 bytes): '%s'\n",
                            paParms[3].u.pointer.size,
                            (const char*)paParms[3].u.pointer.addr));
            }
        }
    }

    /* Add the size of the data to the todo list. */
    m_cbTotal += paParms[4].u.uint32;
    LogFlowFunc(("cbTotal=%zu\n", m_cbTotal));

    /* The first message is the meta info for the data and the data itself. */
    m_pNextPathMsg = new DnDHGSendDataMessagePrivate(uMsg, cParms, paParms,
                                                     &DnDHGSendDataMessage::progressCallback, this);
}

DnDHGSendDataMessage::~DnDHGSendDataMessage(void)
{
    if (m_pNextPathMsg)
        delete m_pNextPathMsg;
}

HGCM::Message* DnDHGSendDataMessage::nextHGCMMessage(void)
{
    if (!m_pNextPathMsg)
        return NULL;

    return m_pNextPathMsg->nextHGCMMessage();
}

int DnDHGSendDataMessage::currentMessageInfo(uint32_t *puMsg, uint32_t *pcParms)
{
    if (!m_pNextPathMsg)
        return VERR_NO_DATA;

    return m_pNextPathMsg->currentMessageInfo(puMsg, pcParms);
}

int DnDHGSendDataMessage::currentMessage(uint32_t uMsg,
                                         uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    if (!m_pNextPathMsg)
        return VERR_NO_DATA;

    /* Fill the data out of our current queued message. */
    int rc = m_pNextPathMsg->currentMessage(uMsg, cParms, paParms);
    /* Has this message more data to deliver? */
    if (!m_pNextPathMsg->isMessageWaiting())
    {
        delete m_pNextPathMsg;
        m_pNextPathMsg = NULL;
    }

    /* File/directory data to send? */
    if (!m_pNextPathMsg)
    {
        if (m_lstURI.IsEmpty())
            return rc;

        /* Create new messages based on our internal path list. Currently
         * this could be directories or regular files. */
        const DnDURIObject &nextObj = m_lstURI.First();
        try
        {
            uint32_t fMode = nextObj.GetMode();
            LogFlowFunc(("Processing srcPath=%s, dstPath=%s, fMode=0x%x, cbSize=%RU32, fIsDir=%RTbool, fIsFile=%RTbool\n",
                         nextObj.GetSourcePath().c_str(), nextObj.GetDestPath().c_str(),
                         fMode, nextObj.GetSize(),
                         RTFS_IS_DIRECTORY(fMode), RTFS_IS_FILE(fMode)));

            if (RTFS_IS_DIRECTORY(fMode))
                m_pNextPathMsg = new DnDHGSendDirPrivate(nextObj,
                                                         &DnDHGSendDataMessage::progressCallback /* pfnProgressCallback */,
                                                         this /* pvProgressUser */);
            else if (RTFS_IS_FILE(fMode))
                m_pNextPathMsg = new DnDHGSendFilePrivate(nextObj,
                                                          &DnDHGSendDataMessage::progressCallback /* pfnProgressCallback */,
                                                          this /* pvProgressUser */);
            else
                AssertMsgFailedReturn(("fMode=0x%x is not supported for srcPath=%s, dstPath=%s\n",
                                       fMode, nextObj.GetSourcePath().c_str(), nextObj.GetDestPath().c_str()),
                                       VERR_NO_DATA);

            m_lstURI.RemoveFirst();
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

int DnDHGSendDataMessage::progressCallback(size_t cbDone, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    DnDHGSendDataMessage *pSelf = static_cast<DnDHGSendDataMessage *>(pvUser);
    AssertPtr(pSelf);

    /* How many bytes are transfered already. */
    pSelf->m_cbTransfered += cbDone;

    /* Advance progress info. */
    int rc = VINF_SUCCESS;
    if (   pSelf->m_pfnProgressCallback
        && pSelf->m_cbTotal)
    {
        AssertMsg(pSelf->m_cbTransfered <= pSelf->m_cbTotal,
                  ("More bytes transferred (%zu) than expected (%zu), cbDone=%zu\n",
                   pSelf->m_cbTransfered, pSelf->m_cbTotal, cbDone));

        unsigned uPercentage = (unsigned)((uint64_t)pSelf->m_cbTransfered * 100 / pSelf->m_cbTotal);
        rc = pSelf->m_pfnProgressCallback(RT_MIN(uPercentage, 100),
                                          DragAndDropSvc::DND_PROGRESS_RUNNING,
                                          VINF_SUCCESS /* rc */, pSelf->m_pvProgressUser);
    }

    return rc;
}

/******************************************************************************
 *   DnDManager                                                               *
 ******************************************************************************/

int DnDManager::addMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    try
    {
        switch (uMsg)
        {
            case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
            {
                clear();
                LogFlowFunc(("HOST_DND_HG_EVT_ENTER\n"));

                /* Verify parameter count and types. */
                if (   cParms != 7
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* x-pos */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* y-pos */
                    || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT /* default action */
                    || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* allowed actions */
                    || paParms[5].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                    || paParms[6].type != VBOX_HGCM_SVC_PARM_32BIT /* size */)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    m_fOpInProcess = true;
                    DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                    m_dndMessageQueue.append(pMessage);
                }
                break;
            }

            case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
            {
                LogFlowFunc(("HOST_DND_HG_EVT_MOVE\n"));

                /* Verify parameter count and types. */
                if (   cParms != 7
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* x-pos */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* y-pos */
                    || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT /* default action */
                    || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* allowed actions */
                    || paParms[5].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                    || paParms[6].type != VBOX_HGCM_SVC_PARM_32BIT /* size */)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    m_fOpInProcess = true;
                    DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                    m_dndMessageQueue.append(pMessage);
                }
                break;
            }

            case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
            {
                LogFlowFunc(("HOST_DND_HG_EVT_LEAVE\n"));

                /* Verify parameter count and types. */
                if (cParms != 0)
                    rc = VERR_INVALID_PARAMETER;
                else
                {
                    DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                    m_dndMessageQueue.append(pMessage);
                }

                m_fOpInProcess = false;
                break;
            }

            case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
            {
                LogFlowFunc(("HOST_DND_HG_EVT_DROPPED\n"));

                /* Verify parameter count and types. */
                if (   cParms != 7
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* x-pos */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* y-pos */
                    || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT /* default action */
                    || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* allowed actions */
                    || paParms[5].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                    || paParms[6].type != VBOX_HGCM_SVC_PARM_32BIT /* size */)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                    m_dndMessageQueue.append(pMessage);
                }
                break;
            }

            case DragAndDropSvc::HOST_DND_HG_SND_DATA:
            {
                LogFlowFunc(("HOST_DND_HG_SND_DATA\n"));

                /* Verify parameter count and types. */
                if (   cParms != 5
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* format */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* format size */
                    || paParms[3].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                    || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* data size */)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    DnDHGSendDataMessage *pMessage =
                        new DnDHGSendDataMessage(uMsg, cParms, paParms,
                                                 m_pfnProgressCallback, m_pvProgressUser);
                    m_dndMessageQueue.append(pMessage);
                }
                break;
            }

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
            {
                LogFlowFunc(("HOST_DND_GH_REQ_PENDING\n"));

                /* Verify parameter count and types. */
                if (   cParms != 1
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                    m_dndMessageQueue.append(pMessage);
                }
                break;
            }

            case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
            {
                LogFlowFunc(("HOST_DND_GH_EVT_DROPPED\n"));

                /* Verify parameter count and types. */
                if (   cParms != 3
                    || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* format */
                    || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* format size */
                    || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* action */)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    try
                    {
                        DnDGenericMessage *pMessage
                            = new DnDGenericMessage(uMsg, cParms, paParms);
                        m_dndMessageQueue.append(pMessage);
                    }
                    catch(std::bad_alloc &)
                    {
                        rc = VERR_NO_MEMORY;
                    }
                }
                break;
            }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }
    }
    catch(std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

HGCM::Message* DnDManager::nextHGCMMessage(void)
{
    if (m_pCurMsg)
        return m_pCurMsg->nextHGCMMessage();

    if (m_dndMessageQueue.isEmpty())
        return NULL;

    return m_dndMessageQueue.first()->nextHGCMMessage();
}

int DnDManager::nextMessageInfo(uint32_t *puMsg, uint32_t *pcParms)
{
    AssertPtrReturn(puMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (m_pCurMsg)
        rc = m_pCurMsg->currentMessageInfo(puMsg, pcParms);
    else
    {
        if (m_dndMessageQueue.isEmpty())
        {
            rc = VERR_NO_DATA;
//            if (m_pfnProgressCallback)
//                m_pfnProgressCallback(100.0, DragAndDropSvc::DND_OP_CANCELLED, m_pvProgressUser);
        }
        else
            rc = m_dndMessageQueue.first()->currentMessageInfo(puMsg, pcParms);
    }

    LogFlowFunc(("Returning puMsg=%RU32, pcParms=%RU32, rc=%Rrc\n", *puMsg, *pcParms, rc));
    return rc;
}

int DnDManager::nextMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("uMsg=%RU32, cParms=%RU32\n", uMsg, cParms));

    if (!m_pCurMsg)
    {
        /* Check for pending messages in our queue. */
        if (m_dndMessageQueue.isEmpty())
        {
            LogFlowFunc(("Message queue is empty, returning\n"));
            return VERR_NO_DATA;
        }

        m_pCurMsg = m_dndMessageQueue.first();
        m_dndMessageQueue.removeFirst();
    }

    /* Fetch the current message info */
    int rc = m_pCurMsg->currentMessage(uMsg, cParms, paParms);
    /* If this message doesn't provide any additional sub messages, clear it. */
    if (!m_pCurMsg->isMessageWaiting())
    {
        delete m_pCurMsg;
        m_pCurMsg = NULL;
    }

    /*
     * If there was an error handling the current message or the user has canceled
     * the operation, we need to cleanup all pending events and inform the progress
     * callback about our exit.
     */
    if (   RT_FAILURE(rc)
        && m_pfnProgressCallback)
    {
        /* Clear any pending messages. */
        clear();

        /* Create a new cancel message to inform the guest + call
         * the host whether the current transfer was canceled or aborted
         * due to an error. */
        try
        {
            Assert(!m_pCurMsg);
            m_pCurMsg = new DnDHGCancelMessage();
            m_pfnProgressCallback(100 /* Percent */,
                                    rc == VERR_CANCELLED
                                  ? DragAndDropSvc::DND_PROGRESS_CANCELLED
                                  : DragAndDropSvc::DND_PROGRESS_ERROR, rc, m_pvProgressUser);
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    LogFlowFunc(("Message processed with rc=%Rrc\n", rc));
    return rc;
}

void DnDManager::clear(void)
{
    if (m_pCurMsg)
    {
        delete m_pCurMsg;
        m_pCurMsg = 0;
    }

    while (!m_dndMessageQueue.isEmpty())
    {
        delete m_dndMessageQueue.last();
        m_dndMessageQueue.removeLast();
    }
}

