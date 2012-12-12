/* $Id$ */
/** @file
 * Drag and Drop manager.
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

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/

#define LOG_GROUP LOG_GROUP_HGCM

#include "dndmanager.h"

#include <VBox/log.h>
#include <iprt/file.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#define VERBOSE 1

#if defined(VERBOSE) && defined(DEBUG_poetzsch)
# include <iprt/stream.h>
# define DO(s) RTPrintf s
#else
# define DO(s) do {} while(0)
//# define DO(s) Log s
#endif

/******************************************************************************
 *   Private declarations                                                     *
 ******************************************************************************/

typedef DECLCALLBACK(int) FNDNDPRIVATEPROGRESS(size_t cbDone, void *pvUser);
typedef FNDNDPRIVATEPROGRESS *PFNDNDPRIVATEPROGRESS;

/**
 * Internal DnD message class for informing the guest about a new directory.
 *
 * @see DnDHGSendDataMessage
 */
class DnDHGSendDirPrivate: public DnDMessage
{
public:
    DnDHGSendDirPrivate(const RTCString &strPath, uint32_t fMode, uint64_t cbSize, PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser)
      : m_strPath(strPath)
      , m_cbSize(cbSize)
      , m_pfnProgressCallback(pfnProgressCallback)
      , m_pvProgressUser(pvProgressUser)
    {
        VBOXHGCMSVCPARM paTmpParms[3];
        paTmpParms[0].setString(m_strPath.c_str());
        paTmpParms[1].setUInt32(m_strPath.length() + 1);
        paTmpParms[2].setUInt32(fMode);
        m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_DIR, 3, paTmpParms);
    }

    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        int rc = DnDMessage::currentMessage(uMsg, cParms, paParms);
        /* Advance progress info */
        if (   RT_SUCCESS(rc)
            && m_pfnProgressCallback)
            rc = m_pfnProgressCallback(m_cbSize, m_pvProgressUser);

        return rc;
    }

protected:
    RTCString m_strPath;

    /* Progress stuff */
    size_t                 m_cbSize;
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
    DnDHGSendFilePrivate(const RTCString &strHostPath, const RTCString &strGuestPath, uint32_t fMode, uint64_t cbSize, PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser);
    ~DnDHGSendFilePrivate();

    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

protected:
    RTCString              m_strHostPath;
    RTCString              m_strGuestPath;
    uint64_t               m_cbSize;
    uint64_t               m_cbDone;
    RTFILE                 m_hCurFile;
    VBOXHGCMSVCPARM        m_paSkelParms[5];

    /* Progress stuff */
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
    DnDHGSendDataMessagePrivate(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[], PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser);
    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

protected:
    size_t                 m_cbSize;
    size_t                 m_cbDone;

    /* Progress stuff */
    PFNDNDPRIVATEPROGRESS  m_pfnProgressCallback;
    void                  *m_pvProgressUser;
};

/******************************************************************************
 *   Implementation                                                           *
 ******************************************************************************/

/******************************************************************************
 *   DnDHGSendFilePrivate                                                *
 ******************************************************************************/

DnDHGSendFilePrivate::DnDHGSendFilePrivate(const RTCString &strHostPath, const RTCString &strGuestPath, uint32_t fMode, uint64_t cbSize, PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser)
  : m_strHostPath(strHostPath)
  , m_strGuestPath(strGuestPath)
  , m_cbSize(cbSize)
  , m_cbDone(0)
  , m_hCurFile(0)
  , m_pfnProgressCallback(pfnProgressCallback)
  , m_pvProgressUser(pvProgressUser)
{
    m_paSkelParms[0].setString(m_strGuestPath.c_str());
    m_paSkelParms[1].setUInt32(m_strGuestPath.length() + 1);
    m_paSkelParms[2].setPointer(NULL, 0);
    m_paSkelParms[3].setUInt32(0);
    m_paSkelParms[4].setUInt32(fMode);
    m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_FILE, 5, m_paSkelParms);
}

DnDHGSendFilePrivate::~DnDHGSendFilePrivate()
{
    if (m_hCurFile)
        RTFileClose(m_hCurFile);
}

int DnDHGSendFilePrivate::currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    if (!m_pNextMsg)
        return VERR_NO_DATA;

    int rc = m_pNextMsg->getData(uMsg, cParms, paParms);
    clearNextMsg();
    if (RT_FAILURE(rc))
        return rc;

    if (!m_hCurFile)
    {
        rc = RTFileOpen(&m_hCurFile, m_strHostPath.c_str(), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_ALL);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* How big is the pointer provided by the guest? */
    uint32_t cbToRead = paParms[2].u.pointer.size;
    size_t cbRead;
    rc = RTFileRead(m_hCurFile, paParms[2].u.pointer.addr, cbToRead, &cbRead);
    if (RT_FAILURE(rc))
    {
        /* On error, immediately close the file. */
        RTFileClose(m_hCurFile);
        m_hCurFile = 0;
        return rc;
    }
    m_cbDone += cbRead;
    /* Tell the guest the actual size. */
    paParms[3].setUInt32(cbRead);
    /* Check if we are done. */
    if (m_cbSize == m_cbDone)
    {
        RTFileClose(m_hCurFile);
        m_hCurFile = 0;
    }
    else
    {
        /* More data! Prepare the next message. */
        m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_FILE, 5, m_paSkelParms);
    }

    /* Advance progress info */
    if (   RT_SUCCESS(rc)
        && m_pfnProgressCallback)
        rc = m_pfnProgressCallback(cbRead, m_pvProgressUser);

    return rc;
}

/******************************************************************************
 *   DnDHGSendDataMessagePrivate                                                *
 ******************************************************************************/

DnDHGSendDataMessagePrivate::DnDHGSendDataMessagePrivate(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[], PFNDNDPRIVATEPROGRESS pfnProgressCallback, void *pvProgressUser)
  : m_cbSize(paParms[4].u.uint32)
  , m_cbDone(0)
  , m_pfnProgressCallback(pfnProgressCallback)
  , m_pvProgressUser(pvProgressUser)
{
    /* Create the initial data message. */
    m_pNextMsg = new HGCM::Message(uMsg, cParms, paParms);
}

int DnDHGSendDataMessagePrivate::currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /* Todo: don't copy the data parts ... just move the data pointer in
     * the original data ptr. */
    if (!m_pNextMsg)
        return VERR_NO_DATA;

    int rc = VINF_SUCCESS;

    HGCM::Message *pCurMsg = m_pNextMsg;
    m_pNextMsg = 0;
    rc = pCurMsg->getData(uMsg, cParms, paParms);
    /* Depending on the current message, the data pointer is on a
     * different position (HOST_DND_HG_SND_DATA=3;
     * HOST_DND_HG_SND_MORE_DATA=0). */
    int iPos = uMsg == DragAndDropSvc::HOST_DND_HG_SND_DATA ? 3 : 0;
    m_cbDone += paParms[iPos + 1].u.uint32;
    /* Info & data send already? */
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        paParms[iPos + 1].u.uint32 = paParms[iPos].u.pointer.size;
        VBOXHGCMSVCPARM paTmpParms[2];
        void     *pvOldData;
        uint32_t  cOldData;
        pCurMsg->getParmPtrInfo(iPos, &pvOldData, &cOldData);
        paTmpParms[0].setPointer(static_cast<uint8_t*>(pvOldData) + paParms[iPos].u.pointer.size, cOldData - paParms[iPos].u.pointer.size);
        paTmpParms[1].setUInt32(cOldData - paParms[iPos].u.pointer.size);
        m_pNextMsg = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA, 2, paTmpParms);
    }
    delete pCurMsg;

    /* Advance progress info */
    if (   RT_SUCCESS(rc)
        && m_pfnProgressCallback)
        rc = m_pfnProgressCallback(m_cbDone, m_pvProgressUser);

    return rc;
}

/******************************************************************************
 *   DnDHGSendDataMessage                                                       *
 ******************************************************************************/

/*
 * This class is a meta message class. It doesn't consist of any own message
 * data, but handle the meta info, the data itself as well any files or
 * directories which have to be transfered to the guest.
 */
DnDHGSendDataMessage::DnDHGSendDataMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[], PFNDNDPROGRESS pfnProgressCallback, void *pvProgressUser)
  : m_cbAll(0)
  , m_cbTransfered(0)
  , m_pfnProgressCallback(pfnProgressCallback)
  , m_pvProgressUser(pvProgressUser)
{
    RTCString strNewUris;
    /* Check the format for any uri type. */
    if (hasFileUrls(static_cast<const char*>(paParms[1].u.pointer.addr), paParms[1].u.pointer.size))
    {
        DO(("old data '%s'\n", (char*)paParms[3].u.pointer.addr));
        /* The list is separated by newline (Even if only one file is
         * listed). */
        RTCList<RTCString> oldUriList = RTCString(static_cast<const char*>(paParms[3].u.pointer.addr), paParms[3].u.pointer.size).split("\r\n");
        if (!oldUriList.isEmpty())
        {
            RTCList<RTCString> newUriList;
            for (size_t i = 0; i < oldUriList.size(); ++i)
            {
                const RTCString &strUri = oldUriList.at(i);
                /* Query the path component of a file URI. If this hasn't a
                 * file scheme null is returned. */
                if (char *pszFilePath = RTUriFilePath(strUri.c_str(), URI_FILE_FORMAT_AUTO))
                {
                    /* Add the path to our internal file list (recursive in
                     * the case of a directory). */
                    if (char *pszFilename = RTPathFilename(pszFilePath))
                    {
                        char *pszNewUri = RTUriFileCreate(pszFilename);
                        if (pszNewUri)
                        {
                            newUriList.append(pszNewUri);
                            RTStrFree(pszNewUri);
                            buildFileTree(pszFilePath, pszFilename - pszFilePath);
                        }
                    }
                    RTStrFree(pszFilePath);
                }
                else
                    newUriList.append(strUri);
            }
            /* We have to change the actual DnD data. Remove any host paths and
             * just decode the filename into the new data. The guest tools will
             * add the correct path again, before sending the DnD drop event to
             * some window. */
            strNewUris = RTCString::join(newUriList, "\r\n") + "\r\n";
            /* Remark: We don't delete the old pointer here, cause this is done
             * by the caller. We just use the RTString data, which has the
             * scope of this ctor. This is enough cause the data is copied in
             * the DnDHGSendDataMessagePrivate anyway. */
            paParms[3].u.pointer.addr = (void*)strNewUris.c_str();
            paParms[3].u.pointer.size = strNewUris.length() + 1;
            paParms[4].u.uint32       = strNewUris.length() + 1;
        }
    }
    /* Add the size of the data to the todo list. */
    m_cbAll += paParms[4].u.uint32;
    /* The first message is the meta info for the data and the data itself. */
    m_pNextPathMsg = new DnDHGSendDataMessagePrivate(uMsg, cParms, paParms, &DnDHGSendDataMessage::progressCallback, this);

    DO(("new data '%s'\n", (char*)paParms[3].u.pointer.addr));
    DO(("cbAll: %u\n", m_cbAll));
    DO(("cbData: %u\n", paParms[4].u.uint32));

    for (size_t i = 0; i < m_uriList.size(); ++i)
        DO(("file: %s : %s - %o - %ld\n", m_uriList.at(i).m_strHostPath.c_str(), m_uriList.at(i).m_strGuestPath.c_str(), m_uriList.at(i).m_fMode, m_uriList.at(i).m_cbSize));
}

DnDHGSendDataMessage::~DnDHGSendDataMessage()
{
    if (m_pNextPathMsg)
        delete m_pNextPathMsg;
}

HGCM::Message* DnDHGSendDataMessage::nextHGCMMessage()
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

int DnDHGSendDataMessage::currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
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

    /* File data to send? */
    if (!m_pNextPathMsg)
    {
        if (m_uriList.isEmpty())
            return rc;
        /* Create new messages based on our internal path list. Currently
         * this could be directories or regular files. */
        PathEntry nextPath = m_uriList.first();
        if (RTFS_IS_DIRECTORY(nextPath.m_fMode))
            m_pNextPathMsg = new DnDHGSendDirPrivate(nextPath.m_strGuestPath, nextPath.m_fMode, nextPath.m_cbSize, &DnDHGSendDataMessage::progressCallback, this);
        else if (RTFS_IS_FILE(nextPath.m_fMode))
            m_pNextPathMsg = new DnDHGSendFilePrivate(nextPath.m_strHostPath, nextPath.m_strGuestPath, nextPath.m_fMode, nextPath.m_cbSize, &DnDHGSendDataMessage::progressCallback, this);
        else
            AssertMsgFailedReturn(("type '%d' is not supported for path '%s'", nextPath.m_fMode, nextPath.m_strHostPath.c_str()), VERR_NO_DATA);
        m_uriList.removeFirst();
    }
    return rc;
}

bool DnDHGSendDataMessage::hasFileUrls(const char *pcszFormat, size_t cbMax) const
{
    DO(("format %s\n", pcszFormat));
    /* text/uri also an official variant? */
    return    RTStrNICmp(pcszFormat, "text/uri-list", cbMax)             == 0
           || RTStrNICmp(pcszFormat, "x-special/gnome-icon-list", cbMax) == 0;
}

int DnDHGSendDataMessage::buildFileTree(const char *pcszPath, size_t cbBaseLen)
{
    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return rc;

    /* These are the types we currently support. Symlinks are not directly
     * supported. First the guest could be an OS which doesn't support it and
     * second the symlink could point to a file which is out of the base tree.
     * Both things are hard to support. For now we just copy the target file in
     * this case. */
    if (!(   RTFS_IS_DIRECTORY(objInfo.Attr.fMode)
          || RTFS_IS_FILE(objInfo.Attr.fMode)
          || RTFS_IS_SYMLINK(objInfo.Attr.fMode)))
        return VINF_SUCCESS;

    uint64_t cbSize = 0;
    rc = RTFileQuerySize(pcszPath, &cbSize);
    if (rc == VERR_IS_A_DIRECTORY)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
        return rc;
    m_uriList.append(PathEntry(pcszPath, &pcszPath[cbBaseLen], objInfo.Attr.fMode, cbSize));
    m_cbAll += cbSize;
    DO(("cbFile: %u\n", cbSize));

    PRTDIR hDir;
    /* We have to try to open even symlinks, cause they could be symlinks
     * to directories. */
    rc = RTDirOpen(&hDir, pcszPath);
    /* The following error happens when this was a symlink to an file or a
     * regular file. */
    if (rc == VERR_PATH_NOT_FOUND)
        return VINF_SUCCESS;
    if (RT_FAILURE(rc))
        return rc;

    while (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirEntry;
        rc = RTDirRead(hDir, &DirEntry, NULL);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_NO_MORE_FILES)
                rc = VINF_SUCCESS;
            break;
        }
        switch (DirEntry.enmType)
        {
            case RTDIRENTRYTYPE_DIRECTORY:
            {
                /* Skip "." and ".." entries. */
                if (   RTStrCmp(DirEntry.szName, ".")  == 0
                    || RTStrCmp(DirEntry.szName, "..") == 0)
                    break;
                if (char *pszRecDir = RTStrAPrintf2("%s%c%s", pcszPath, RTPATH_DELIMITER, DirEntry.szName))
                {
                    rc = buildFileTree(pszRecDir, cbBaseLen);
                    RTStrFree(pszRecDir);
                }
                else
                    rc = VERR_NO_MEMORY;
                break;
            }
            case RTDIRENTRYTYPE_SYMLINK:
            case RTDIRENTRYTYPE_FILE:
            {
                if (char *pszNewFile = RTStrAPrintf2("%s%c%s", pcszPath, RTPATH_DELIMITER, DirEntry.szName))
                {
                    /* We need the size and the mode of the file. */
                    RTFSOBJINFO objInfo1;
                    rc = RTPathQueryInfo(pszNewFile, &objInfo1, RTFSOBJATTRADD_NOTHING);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = RTFileQuerySize(pszNewFile, &cbSize);
                    if (RT_FAILURE(rc))
                        break;
                    m_uriList.append(PathEntry(pszNewFile, &pszNewFile[cbBaseLen], objInfo1.Attr.fMode, cbSize));
                    m_cbAll += cbSize;
                    RTStrFree(pszNewFile);
                }
                else
                    rc = VERR_NO_MEMORY;
                break;
            }
            default: break;
        }
    }
    RTDirClose(hDir);

    return rc;
}

int DnDHGSendDataMessage::progressCallback(size_t cbDone, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    DnDHGSendDataMessage *pSelf = static_cast<DnDHGSendDataMessage *>(pvUser);

    /* How many bytes are transfered already. */
    pSelf->m_cbTransfered += cbDone;

    /* Advance progress info */
    int rc = VINF_SUCCESS;
    if (   pSelf->m_pfnProgressCallback
        && pSelf->m_cbAll)
        rc = pSelf->m_pfnProgressCallback((uint64_t)pSelf->m_cbTransfered * 100 / pSelf->m_cbAll,
                                          DragAndDropSvc::DND_PROGRESS_RUNNING, pSelf->m_pvProgressUser);

    return rc;
}

/******************************************************************************
 *   DnDManager                                                               *
 ******************************************************************************/

int DnDManager::addMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;
    switch (uMsg)
    {
        case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
        {
            clear();
            LogFlowFunc(("HOST_DND_HG_EVT_ENTER\n"));
            DO(("HOST_DND_HG_EVT_ENTER\n"));
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
            DO(("HOST_DND_HG_EVT_MOVE\n"));
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
        case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
        {
            LogFlowFunc(("HOST_DND_HG_EVT_LEAVE\n"));
            DO(("HOST_DND_HG_EVT_LEAVE\n"));

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
            DO(("HOST_DND_HG_EVT_DROPPED\n"));
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
                DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                m_dndMessageQueue.append(pMessage);
            }
            break;
        }
        case DragAndDropSvc::HOST_DND_HG_SND_DATA:
        {
            LogFlowFunc(("HOST_DND_HG_SND_DATA\n"));
            DO(("HOST_DND_HG_SND_DATA\n"));

            /* Verify parameter count and types. */
            if (   cParms != 5
                || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* format */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* format size */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_PTR   /* data */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT /* data size */)
                rc = VERR_INVALID_PARAMETER;
            else
            {
                DnDHGSendDataMessage *pMessage = new DnDHGSendDataMessage(uMsg, cParms, paParms, m_pfnProgressCallback, m_pvProgressUser);
                m_dndMessageQueue.append(pMessage);
            }
            break;
        }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
        {
            LogFlowFunc(("HOST_DND_GH_REQ_PENDING\n"));
            DO(("HOST_DND_GH_REQ_PENDING\n"));

            /* Verify parameter count and types. */
            if (   cParms != 1
                || paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* screen id */)
                rc = VERR_INVALID_PARAMETER;
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
            DO(("HOST_DND_GH_EVT_DROPPED\n"));

            /* Verify parameter count and types. */
            if (   cParms != 3
                || paParms[0].type != VBOX_HGCM_SVC_PARM_PTR   /* format */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT /* format size */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* action */)
                rc = VERR_INVALID_PARAMETER;
            else
            {
                DnDGenericMessage *pMessage = new DnDGenericMessage(uMsg, cParms, paParms);
                m_dndMessageQueue.append(pMessage);
            }
            break;
        }
#endif
        default: rc = VERR_NOT_IMPLEMENTED; break;
    }

    return rc;
}

HGCM::Message* DnDManager::nextHGCMMessage()
{
    if (m_pCurMsg)
        return m_pCurMsg->nextHGCMMessage();
    else
    {
        if (m_dndMessageQueue.isEmpty())
            return 0;

        return m_dndMessageQueue.first()->nextHGCMMessage();
    }
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

    DO(("next msg info: %d %d %Rrc\n", *puMsg, *pcParms, rc));
    return rc;
}

int DnDManager::nextMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    if (!m_pCurMsg)
    {
        /* Check for pending messages in our queue. */
        if (m_dndMessageQueue.isEmpty())
            return VERR_NO_DATA;
        m_pCurMsg = m_dndMessageQueue.first();
        m_dndMessageQueue.removeFirst();
    }

    /* Fetch the current message info */
    int rc = m_pCurMsg->currentMessage(uMsg, cParms, paParms);
    /* If this message not provide any additional sub messages, clear it. */
    if (!m_pCurMsg->isMessageWaiting())
    {
        delete m_pCurMsg;
        m_pCurMsg = 0;
    }

    /* If the user has canceled the operation, we need to cleanup all pending
     * events and inform the progress callback about our successful cleanup. */
    if (   rc == VERR_CANCELLED
        && m_pfnProgressCallback)
    {
        /* Clear any pending messages */
        clear();
        /* Create a new cancel message to inform the guest. */
        m_pCurMsg = new DnDHGCancelMessage();
        m_pfnProgressCallback(100, DragAndDropSvc::DND_PROGRESS_CANCELLED, m_pvProgressUser);
    }

    DO(("next msg: %d %d %Rrc\n", uMsg, cParms, rc));
    return rc;
}

void DnDManager::clear()
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

