/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Drag & Drop.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/uri.h>
#include <iprt/thread.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#include "VBGLR3Internal.h"

/* Here all the communication with the host over HGCM is handled platform
 * neutral. Also the receiving of URIs content (directory trees and files) is
 * done here. So the platform code of the guests, should not take care of that.
 *
 * Todo:
 * - Sending dirs/files in the G->H case
 * - Maybe the EOL converting of text MIME types (not fully sure, eventually
 *   better done on the host side)
 */

/******************************************************************************
 *    Private internal functions                                              *
 ******************************************************************************/

static int vbglR3DnDQueryNextHostMessageType(uint32_t uClientId, uint32_t *puMsg, uint32_t *pcParms, bool fWait)
{
    AssertPtrReturn(puMsg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDNEXTMSGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG;
    Msg.hdr.cParms      = 3;

    Msg.msg.SetUInt32(0);
    Msg.num_parms.SetUInt32(0);
    Msg.block.SetUInt32(fWait);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            rc = Msg.msg.GetUInt32(puMsg);         AssertRC(rc);
            rc = Msg.num_parms.GetUInt32(pcParms); AssertRC(rc);
        }
    }

    return rc;
}

static int vbglR3DnDHGProcessActionMessage(uint32_t  uClientId,
                                           uint32_t  uMsg,
                                           uint32_t *puScreenId,
                                           uint32_t *puX,
                                           uint32_t *puY,
                                           uint32_t *puDefAction,
                                           uint32_t *puAllActions,
                                           char     *pszFormats,
                                           uint32_t  cbFormats,
                                           uint32_t *pcbFormatsRecv)
{
    AssertPtrReturn(puScreenId,     VERR_INVALID_POINTER);
    AssertPtrReturn(puX,            VERR_INVALID_POINTER);
    AssertPtrReturn(puY,            VERR_INVALID_POINTER);
    AssertPtrReturn(puDefAction,    VERR_INVALID_POINTER);
    AssertPtrReturn(puAllActions,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszFormats,     VERR_INVALID_POINTER);
    AssertReturn(cbFormats,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatsRecv, VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDHGACTIONMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = uMsg;
    Msg.hdr.cParms      = 7;

    Msg.uScreenId.SetUInt32(0);
    Msg.uX.SetUInt32(0);
    Msg.uY.SetUInt32(0);
    Msg.uDefAction.SetUInt32(0);
    Msg.uAllActions.SetUInt32(0);
    Msg.pvFormats.SetPtr(pszFormats, cbFormats);
    Msg.cFormats.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            rc = Msg.uScreenId.GetUInt32(puScreenId);     AssertRC(rc);
            rc = Msg.uX.GetUInt32(puX);                   AssertRC(rc);
            rc = Msg.uY.GetUInt32(puY);                   AssertRC(rc);
            rc = Msg.uDefAction.GetUInt32(puDefAction);   AssertRC(rc);
            rc = Msg.uAllActions.GetUInt32(puAllActions); AssertRC(rc);
            rc = Msg.cFormats.GetUInt32(pcbFormatsRecv);  AssertRC(rc);

            AssertReturn(cbFormats >= *pcbFormatsRecv, VERR_TOO_MUCH_DATA);
        }
    }

    return rc;
}

static int vbglR3DnDHGProcessLeaveMessage(uint32_t uClientId)
{
    DragAndDropSvc::VBOXDNDHGLEAVEMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_EVT_LEAVE;
    Msg.hdr.cParms      = 0;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    return rc;
}

static int vbglR3DnDHGProcessCancelMessage(uint32_t uClientId)
{
    DragAndDropSvc::VBOXDNDHGCANCELMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_EVT_CANCEL;
    Msg.hdr.cParms      = 0;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    return rc;
}

static int vbglR3DnDHGProcessSendDirMessage(uint32_t  uClientId,
                                            char     *pszDirname,
                                            uint32_t  cbDirname,
                                            uint32_t *pcbDirnameRecv,
                                            uint32_t *pfMode)
{
    AssertPtrReturn(pszDirname,     VERR_INVALID_POINTER);
    AssertReturn(cbDirname,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDirnameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,         VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDHGSENDDIRMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_DIR;
    Msg.hdr.cParms      = 3;

    Msg.pvName.SetPtr(pszDirname, cbDirname);
    Msg.cbName.SetUInt32(0);
    Msg.fMode.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(Msg.hdr.result))
        {
            rc = Msg.cbName.GetUInt32(pcbDirnameRecv); AssertRC(rc);
            rc = Msg.fMode.GetUInt32(pfMode);          AssertRC(rc);

            AssertReturn(cbDirname >= *pcbDirnameRecv, VERR_TOO_MUCH_DATA);
        }
    }

    return rc;
}

static int vbglR3DnDHGProcessSendFileMessage(uint32_t  uClientId,
                                             char     *pszFilename,
                                             uint32_t  cbFilename,
                                             uint32_t *pcbFilenameRecv,
                                             void     *pvData,
                                             uint32_t  cbData,
                                             uint32_t *pcbDataRecv,
                                             uint32_t *pfMode)
{
    AssertPtrReturn(pszFilename,     VERR_INVALID_POINTER);
    AssertReturn(cbFilename,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFilenameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv,     VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,          VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDHGSENDFILEMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_FILE;
    Msg.hdr.cParms      = 5;

    Msg.pvName.SetPtr(pszFilename, cbFilename);
    Msg.cbName.SetUInt32(0);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);
    Msg.fMode.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            rc = Msg.cbName.GetUInt32(pcbFilenameRecv); AssertRC(rc);
            rc = Msg.cbData.GetUInt32(pcbDataRecv);     AssertRC(rc);
            rc = Msg.fMode.GetUInt32(pfMode);           AssertRC(rc);

            AssertReturn(cbFilename >= *pcbFilenameRecv, VERR_TOO_MUCH_DATA);
            AssertReturn(cbData     >= *pcbDataRecv,     VERR_TOO_MUCH_DATA);
        }
    }

    return rc;
}

static int vbglR3DnDHGProcessURIMessages(uint32_t   uClientId,
                                         uint32_t  *puScreenId,
                                         char      *pszFormat,
                                         uint32_t   cbFormat,
                                         uint32_t  *pcbFormatRecv,
                                         void     **ppvData,
                                         uint32_t   cbData,
                                         size_t    *pcbDataRecv)
{
    AssertPtrReturn(ppvData, VERR_INVALID_POINTER);
    AssertPtrReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv, VERR_INVALID_POINTER);

    if (!*pcbDataRecv)
        return VERR_INVALID_PARAMETER;

    /* Allocate temp buffer. */
    uint32_t cbTmpData = _64K; /** @todo Make this configurable? */
    void *pvTmpData = RTMemAlloc(cbTmpData);
    if (!pvTmpData)
        return VERR_NO_MEMORY;

    /* Create and query the (unique) drop target directory. */
    char pszDropDir[RTPATH_MAX];
    int rc = DnDDirCreateDroppedFiles(pszDropDir, sizeof(pszDropDir));
    if (RT_FAILURE(rc))
    {
        RTMemFree(pvTmpData);
        return rc;
    }

    /* Patch the old drop data with the new drop directory, so the drop target
     * can find the files. */
    DnDURIList lstURI;
    rc = lstURI.RootFromURIData(*ppvData, *pcbDataRecv,
                                0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        /* Cleanup the old data and write the new data back to the event. */
        RTMemFree(*ppvData);

        RTCString strData = lstURI.RootToString(pszDropDir);
        *ppvData = RTStrDupN(strData.c_str(), strData.length());
        *pcbDataRecv = strData.length() + 1;
    }

    /* Lists for holding created files & directories
     * in the case of a rollback. */
    RTCList<RTCString> guestDirList;
    RTCList<RTCString> guestFileList;

    char szPathName[RTPATH_MAX];
    uint32_t cbPathName = 0;

    bool fLoop = RT_SUCCESS(rc); /* No error occurred yet? */
    while (fLoop)
    {
        uint32_t uNextMsg;
        uint32_t cNextParms;
        rc = vbglR3DnDQueryNextHostMessageType(uClientId, &uNextMsg, &cNextParms, false /* fWait */);
        if (RT_SUCCESS(rc))
        {
            switch (uNextMsg)
            {
                case DragAndDropSvc::HOST_DND_HG_SND_DIR:
                {
                    uint32_t fMode = 0;
                    rc = vbglR3DnDHGProcessSendDirMessage(uClientId,
                                                          szPathName,
                                                          sizeof(szPathName),
                                                          &cbPathName,
                                                          &fMode);
#ifdef DEBUG_andy
                    LogFlowFunc(("HOST_DND_HG_SND_DIR pszPathName=%s, cbPathName=%RU32, fMode=0x%x, rc=%Rrc\n",
                                 szPathName, cbPathName, fMode, rc));
#endif
                    if (RT_SUCCESS(rc))
                        rc = DnDPathSanitize(szPathName, sizeof(szPathName));
                    if (RT_SUCCESS(rc))
                    {
                        char *pszNewDir = RTPathJoinA(pszDropDir, szPathName);
                        if (pszNewDir)
                        {
                            rc = RTDirCreate(pszNewDir, (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRWXU, 0);
                            if (!guestDirList.contains(pszNewDir))
                                guestDirList.append(pszNewDir);

                            RTStrFree(pszNewDir);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    break;
                }
                case DragAndDropSvc::HOST_DND_HG_SND_FILE:
                {
                    uint32_t cbDataRecv;
                    uint32_t fMode = 0;
                    rc = vbglR3DnDHGProcessSendFileMessage(uClientId,
                                                           szPathName,
                                                           sizeof(szPathName),
                                                           &cbPathName,
                                                           pvTmpData,
                                                           cbTmpData,
                                                           &cbDataRecv,
                                                           &fMode);
#ifdef DEBUG_andy
                    LogFlowFunc(("HOST_DND_HG_SND_FILE pszPathName=%s, cbPathName=%RU32, pvData=0x%p, cbDataRecv=%RU32, fMode=0x%x, rc=%Rrc\n",
                                 szPathName, cbPathName, pvTmpData, cbDataRecv, fMode, rc));
#endif
                    if (RT_SUCCESS(rc))
                        rc = DnDPathSanitize(szPathName, sizeof(szPathName));
                    if (RT_SUCCESS(rc))
                    {
                        char *pszPathAbs = RTPathJoinA(pszDropDir, szPathName);
                        if (pszPathAbs)
                        {
                            RTFILE hFile;
                            /** @todo r=andy Keep the file open and locked during the actual file transfer. Otherwise this will
                             *               create all sorts of funny races because we don't know if the guest has
                             *               modified the file in between the file data send calls. */
                            rc = RTFileOpen(&hFile, pszPathAbs,
                                            RTFILE_O_WRITE | RTFILE_O_APPEND | RTFILE_O_DENY_ALL | RTFILE_O_OPEN_CREATE);
                            if (RT_SUCCESS(rc))
                            {
                                /** @todo r=andy Not very safe to assume that we were last appending to the current file. */
                                rc = RTFileSeek(hFile, 0, RTFILE_SEEK_END, NULL);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = RTFileWrite(hFile, pvTmpData, cbDataRecv, 0);
                                    /* Valid UNIX mode? */
                                    if (   RT_SUCCESS(rc)
                                        && (fMode & RTFS_UNIX_MASK))
                                        rc = RTFileSetMode(hFile, (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR);
                                }

                                RTFileClose(hFile);

                                if (!guestFileList.contains(pszPathAbs))
                                    guestFileList.append(pszPathAbs);
                            }
#ifdef DEBUG
                            else
                                LogFlowFunc(("Opening file failed with rc=%Rrc\n", rc));
#endif
                            RTStrFree(pszPathAbs);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    break;
                }
                case DragAndDropSvc::HOST_DND_HG_EVT_CANCEL:
                {
                    rc = vbglR3DnDHGProcessCancelMessage(uClientId);
                    if (RT_SUCCESS(rc))
                        rc = VERR_CANCELLED;
                    /* Break out of the loop. */
                }
                default:
                    fLoop = false;
                    break;
            }
        }
        else
        {
            if (rc == VERR_NO_DATA)
                rc = VINF_SUCCESS;
            break;
        }

        if (RT_FAILURE(rc))
            break;

    } /* while */

    if (pvTmpData)
        RTMemFree(pvTmpData);

    /* Cleanup on failure or if the user has canceled. */
    if (RT_FAILURE(rc))
    {
        /* Remove any stuff created. */
        for (size_t i = 0; i < guestFileList.size(); ++i)
            RTFileDelete(guestFileList.at(i).c_str());
        for (size_t i = 0; i < guestDirList.size(); ++i)
            RTDirRemove(guestDirList.at(i).c_str());
        RTDirRemove(pszDropDir);

        LogFlowFunc(("Failed with rc=%Rrc\n", rc));
    }

    return rc;
}

static int vbglR3DnDHGProcessDataMessageInternal(uint32_t  uClientId,
                                                 uint32_t *puScreenId,
                                                 char     *pszFormat,
                                                 uint32_t  cbFormat,
                                                 uint32_t *pcbFormatRecv,
                                                 void     *pvData,
                                                 uint32_t  cbData,
                                                 uint32_t *pcbDataTotal)
{
    AssertPtrReturn(puScreenId,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszFormat,     VERR_INVALID_POINTER);
    AssertReturn(cbFormat,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,        VERR_INVALID_POINTER);
    AssertReturn(cbData,           VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataTotal,  VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDHGSENDDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_DATA;
    Msg.hdr.cParms      = 5;

    Msg.uScreenId.SetUInt32(0);
    Msg.pvFormat.SetPtr(pszFormat, cbFormat);
    Msg.cFormat.SetUInt32(0);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (   RT_SUCCESS(rc)
            || rc == VERR_BUFFER_OVERFLOW)
        {
            rc = Msg.uScreenId.GetUInt32(puScreenId);  AssertRC(rc);
            rc = Msg.cFormat.GetUInt32(pcbFormatRecv); AssertRC(rc);
            rc = Msg.cbData.GetUInt32(pcbDataTotal);   AssertRC(rc);

            AssertReturn(cbFormat >= *pcbFormatRecv, VERR_TOO_MUCH_DATA);
            AssertReturn(cbData   >= *pcbDataTotal,  VERR_TOO_MUCH_DATA);
        }
    }

    return rc;
}

static int vbglR3DnDHGProcessMoreDataMessageInternal(uint32_t  uClientId,
                                                     void     *pvData,
                                                     uint32_t  cbData,
                                                     uint32_t *pcbDataRecv)
{
    AssertPtrReturn(pvData,      VERR_INVALID_POINTER);
    AssertReturn(cbData,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv, VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDHGSENDMOREDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA;
    Msg.hdr.cParms      = 2;

    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (   RT_SUCCESS(rc)
            || rc == VERR_BUFFER_OVERFLOW)
        {
            rc = Msg.cbData.GetUInt32(pcbDataRecv); AssertRC(rc);
            AssertReturn(cbData >= *pcbDataRecv, VERR_TOO_MUCH_DATA);
        }
    }
    return rc;
}

static int vbglR3DnDHGProcessSendDataMessageLoop(uint32_t  uClientId,
                                                 uint32_t *puScreenId,
                                                 char     *pszFormat,
                                                 uint32_t  cbFormat,
                                                 uint32_t *pcbFormatRecv,
                                                 void    **ppvData,
                                                 uint32_t  cbData,
                                                 size_t   *pcbDataRecv)
{
    uint32_t cbDataRecv = 0;
    int rc = vbglR3DnDHGProcessDataMessageInternal(uClientId,
                                                   puScreenId,
                                                   pszFormat,
                                                   cbFormat,
                                                   pcbFormatRecv,
                                                   *ppvData,
                                                   cbData,
                                                   &cbDataRecv);
    uint32_t cbAllDataRecv = cbDataRecv;
    while (rc == VERR_BUFFER_OVERFLOW)
    {
        uint32_t uNextMsg;
        uint32_t cNextParms;
        rc = vbglR3DnDQueryNextHostMessageType(uClientId, &uNextMsg, &cNextParms, false);
        if (RT_SUCCESS(rc))
        {
            switch(uNextMsg)
            {
                case DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA:
                {
                    *ppvData = RTMemRealloc(*ppvData, cbAllDataRecv + cbData);
                    if (!*ppvData)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                    rc = vbglR3DnDHGProcessMoreDataMessageInternal(uClientId,
                                                                   &((char*)*ppvData)[cbAllDataRecv],
                                                                   cbData,
                                                                   &cbDataRecv);
                    cbAllDataRecv += cbDataRecv;
                    break;
                }
                case DragAndDropSvc::HOST_DND_HG_EVT_CANCEL:
                default:
                {
                    rc = vbglR3DnDHGProcessCancelMessage(uClientId);
                    if (RT_SUCCESS(rc))
                        rc = VERR_CANCELLED;
                    break;
                }
            }
        }
    }
    if (RT_SUCCESS(rc))
        *pcbDataRecv = cbAllDataRecv;

    return rc;
}

static int vbglR3DnDHGProcessSendDataMessage(uint32_t   uClientId,
                                             uint32_t  *puScreenId,
                                             char      *pszFormat,
                                             uint32_t   cbFormat,
                                             uint32_t  *pcbFormatRecv,
                                             void     **ppvData,
                                             uint32_t   cbData,
                                             size_t    *pcbDataRecv)
{
    int rc = vbglR3DnDHGProcessSendDataMessageLoop(uClientId,
                                                   puScreenId,
                                                   pszFormat,
                                                   cbFormat,
                                                   pcbFormatRecv,
                                                   ppvData,
                                                   cbData,
                                                   pcbDataRecv);
    if (RT_SUCCESS(rc))
    {
        /* Check if this is an URI event. If so, let VbglR3 do all the actual
         * data transfer + file/directory creation internally without letting
         * the caller know.
         *
         * This keeps the actual (guest OS-)dependent client (like VBoxClient /
         * VBoxTray) small by not having too much redundant code. */
        AssertPtr(pcbFormatRecv);
        if (DnDMIMEHasFileURLs(pszFormat, *pcbFormatRecv))
            rc = vbglR3DnDHGProcessURIMessages(uClientId,
                                               puScreenId,
                                               pszFormat,
                                               cbFormat,
                                               pcbFormatRecv,
                                               ppvData,
                                               cbData,
                                               pcbDataRecv);
    }

    return rc;
}

static int vbglR3DnDGHProcessRequestPendingMessage(uint32_t  uClientId,
                                                   uint32_t *puScreenId)
{
    AssertPtrReturn(puScreenId, VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDGHREQPENDINGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_GH_REQ_PENDING;
    Msg.hdr.cParms      = 1;

    Msg.uScreenId.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            rc = Msg.uScreenId.GetUInt32(puScreenId); AssertRC(rc);
        }
    }

    return rc;
}

static int vbglR3DnDGHProcessDroppedMessage(uint32_t  uClientId,
                                            char     *pszFormat,
                                            uint32_t  cbFormat,
                                            uint32_t *pcbFormatRecv,
                                            uint32_t *puAction)
{
    AssertPtrReturn(pszFormat,     VERR_INVALID_POINTER);
    AssertReturn(cbFormat,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(puAction,      VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDGHDROPPEDMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_GH_EVT_DROPPED;
    Msg.hdr.cParms      = 3;

    Msg.pvFormat.SetPtr(pszFormat, cbFormat);
    Msg.cFormat.SetUInt32(0);
    Msg.uAction.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            rc = Msg.cFormat.GetUInt32(pcbFormatRecv); AssertRC(rc);
            rc = Msg.uAction.GetUInt32(puAction);      AssertRC(rc);

            AssertReturn(cbFormat >= *pcbFormatRecv, VERR_TOO_MUCH_DATA);
        }
    }

    return rc;
}

/******************************************************************************
 *    Public functions                                                        *
 ******************************************************************************/

VBGLR3DECL(int) VbglR3DnDConnect(uint32_t *pu32ClientId)
{
    AssertPtrReturn(pu32ClientId, VERR_INVALID_POINTER);

    /* Initialize header */
    VBoxGuestHGCMConnectInfo Info;
    RT_ZERO(Info.Loc.u);
    Info.result      = VERR_WRONG_ORDER;
    Info.u32ClientID = UINT32_MAX;  /* try make valgrind shut up. */
    /* Initialize parameter */
    Info.Loc.type    = VMMDevHGCMLoc_LocalHost_Existing;
    int rc = RTStrCopy(Info.Loc.u.host.achName, sizeof(Info.Loc.u.host.achName), "VBoxDragAndDropSvc");
    if (RT_FAILURE(rc)) return rc;
    /* Do request */
    rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
    {
        rc = Info.result;
        if (RT_SUCCESS(rc))
            *pu32ClientId = Info.u32ClientID;
    }
    if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        rc = VINF_PERMISSION_DENIED;
    return rc;
}

VBGLR3DECL(int) VbglR3DnDDisconnect(uint32_t u32ClientId)
{
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result      = VERR_WRONG_ORDER;
    Info.u32ClientID = u32ClientId;

    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;

    return rc;
}

VBGLR3DECL(int) VbglR3DnDProcessNextMessage(uint32_t u32ClientId, CPVBGLR3DNDHGCMEVENT pEvent)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    uint32_t       uMsg       = 0;
    uint32_t       uNumParms  = 0;
    const uint32_t ccbFormats = _64K;
    const uint32_t ccbData    = _64K;
    int rc = vbglR3DnDQueryNextHostMessageType(u32ClientId, &uMsg, &uNumParms,
                                               true /* fWait */);
    if (RT_SUCCESS(rc))
    {
        switch(uMsg)
        {
            case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
            case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
            case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    rc = VERR_NO_MEMORY;

                if (RT_SUCCESS(rc))
                    rc = vbglR3DnDHGProcessActionMessage(u32ClientId,
                                                         uMsg,
                                                         &pEvent->uScreenId,
                                                         &pEvent->u.a.uXpos,
                                                         &pEvent->u.a.uYpos,
                                                         &pEvent->u.a.uDefAction,
                                                         &pEvent->u.a.uAllActions,
                                                         pEvent->pszFormats,
                                                         ccbFormats,
                                                         &pEvent->cbFormats);
                break;
            }
            case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
            {
                pEvent->uType = uMsg;
                rc = vbglR3DnDHGProcessLeaveMessage(u32ClientId);
                break;
            }
            case DragAndDropSvc::HOST_DND_HG_SND_DATA:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    rc = VERR_NO_MEMORY;

                if (RT_SUCCESS(rc))
                {
                    pEvent->u.b.pvData = RTMemAlloc(ccbData);
                    if (!pEvent->u.b.pvData)
                    {
                        RTMemFree(pEvent->pszFormats);
                        pEvent->pszFormats = NULL;
                        rc = VERR_NO_MEMORY;
                    }
                }

                if (RT_SUCCESS(rc))
                    rc = vbglR3DnDHGProcessSendDataMessage(u32ClientId,
                                                           &pEvent->uScreenId,
                                                           pEvent->pszFormats,
                                                           ccbFormats,
                                                           &pEvent->cbFormats,
                                                           &pEvent->u.b.pvData,
                                                           ccbData,
                                                           &pEvent->u.b.cbData);
                break;
            }
            case DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA:
            case DragAndDropSvc::HOST_DND_HG_SND_DIR:
            case DragAndDropSvc::HOST_DND_HG_SND_FILE:
            {
                pEvent->uType = uMsg;

                /* All messages in this case are handled internally
                 * by vbglR3DnDHGProcessSendDataMessage() and must
                 * be specified by a preceding HOST_DND_HG_SND_DATA call. */
                rc = VERR_WRONG_ORDER;
                break;
            }
            case DragAndDropSvc::HOST_DND_HG_EVT_CANCEL:
            {
                pEvent->uType = uMsg;
                rc = vbglR3DnDHGProcessCancelMessage(u32ClientId);
                break;
            }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
            {
                pEvent->uType = uMsg;
                rc = vbglR3DnDGHProcessRequestPendingMessage(u32ClientId,
                                                             &pEvent->uScreenId);
                break;
            }
            case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    rc = VERR_NO_MEMORY;

                if (RT_SUCCESS(rc))
                    rc = vbglR3DnDGHProcessDroppedMessage(u32ClientId,
                                                          pEvent->pszFormats,
                                                          ccbFormats,
                                                          &pEvent->cbFormats,
                                                          &pEvent->u.a.uDefAction);
                break;
            }
#endif
            default:
            {
                pEvent->uType = uMsg;

                rc = VERR_NOT_SUPPORTED;
                break;
            }
        }
    }

    return rc;
}

VBGLR3DECL(int) VbglR3DnDHGAcknowledgeOperation(uint32_t u32ClientId, uint32_t uAction)
{
    DragAndDropSvc::VBOXDNDHGACKOPMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_HG_ACK_OP;
    Msg.hdr.cParms      = 1;

    Msg.uAction.SetUInt32(uAction);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    return rc;
}

VBGLR3DECL(int) VbglR3DnDHGRequestData(uint32_t u32ClientId, const char* pcszFormat)
{
    AssertPtrReturn(pcszFormat, VERR_INVALID_PARAMETER);

    DragAndDropSvc::VBOXDNDHGREQDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_HG_REQ_DATA;
    Msg.hdr.cParms      = 1;

    Msg.pFormat.SetPtr((void*)pcszFormat, (uint32_t)strlen(pcszFormat) + 1);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    return rc;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
VBGLR3DECL(int) VbglR3DnDGHAcknowledgePending(uint32_t u32ClientId,
                                              uint32_t uDefAction, uint32_t uAllActions,
                                              const char* pcszFormats)
{
    AssertPtrReturn(pcszFormats, VERR_INVALID_POINTER);

    DragAndDropSvc::VBOXDNDGHACKPENDINGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_ACK_PENDING;
    Msg.hdr.cParms      = 3;

    Msg.uDefAction.SetUInt32(uDefAction);
    Msg.uAllActions.SetUInt32(uAllActions);
    Msg.pFormat.SetPtr((void*)pcszFormats, (uint32_t)strlen(pcszFormats) + 1);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    return rc;
}

static int vbglR3DnDGHSendDataInternal(uint32_t u32ClientId,
                                       void *pvData, uint32_t cbData,
                                       uint32_t cbAdditionalData)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    DragAndDropSvc::VBOXDNDGHSENDDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_SND_DATA;
    Msg.hdr.cParms      = 2;

    /* Total amount of bytes to send (including this data block). */
    Msg.cbTotalBytes.SetUInt32(cbData + cbAdditionalData);

    int rc;

    uint32_t cbMaxChunk = _64K; /** @todo Transfer max. 64K chunks per message. Configurable? */
    uint32_t cbSent     = 0;

    while (cbSent < cbData)
    {
        uint32_t cbCurChunk = RT_MIN(cbData - cbSent, cbMaxChunk);
        Msg.pvData.SetPtr(static_cast<uint8_t*>(pvData) + cbSent, cbCurChunk);

        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
            rc = Msg.hdr.result;

        if (RT_FAILURE(rc))
            break;

        cbSent += cbCurChunk;
    }

    if (RT_SUCCESS(rc))
        Assert(cbSent == cbData);

    LogFlowFunc(("Returning rc=%Rrc, cbData=%RU32, cbAddtionalData=%RU32, cbSent=%RU32\n",
                 rc, cbData, cbAdditionalData, cbSent));
    return rc;
}

static int vbglR3DnDGHSendDir(uint32_t u32ClientId, DnDURIObject &obj)
{
    AssertReturn(obj.GetType() == DnDURIObject::Directory, VERR_INVALID_PARAMETER);

    DragAndDropSvc::VBOXDNDGHSENDDIRMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_SND_DIR;
    Msg.hdr.cParms      = 3;

    RTCString strPath = obj.GetDestPath();
    LogFlowFunc(("strDir=%s (%zu), fMode=0x%x\n",
                 strPath.c_str(), strPath.length(), obj.GetMode()));

    Msg.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)(strPath.length() + 1));
    Msg.cbName.SetUInt32((uint32_t)(strPath.length() + 1));
    Msg.fMode.SetUInt32(obj.GetMode());

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3DnDGHSendFile(uint32_t u32ClientId, DnDURIObject &obj)
{
    AssertReturn(obj.GetType() == DnDURIObject::File, VERR_INVALID_PARAMETER);

    uint32_t cbBuf = _64K; /** @todo Make this configurable? */
    void *pvBuf = RTMemAlloc(cbBuf);
    if (!pvBuf)
        return VERR_NO_MEMORY;

    DragAndDropSvc::VBOXDNDGHSENDFILEMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_SND_FILE;
    Msg.hdr.cParms      = 5;

    RTCString strPath = obj.GetDestPath();
    LogFlowFunc(("strFile=%s (%zu), fMode=0x%x\n",
                 strPath.c_str(), strPath.length(), obj.GetMode()));

    Msg.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)(strPath.length() + 1));
    Msg.cbName.SetUInt32((uint32_t)(strPath.length() + 1));
    Msg.fMode.SetUInt32(obj.GetMode());

    int rc = VINF_SUCCESS;
    uint32_t cbData = obj.GetSize();

    do
    {
        uint32_t cbToRead = RT_MIN(cbData, cbBuf);
        uint32_t cbRead   = 0;
        if (cbToRead)
            rc = obj.Read(pvBuf, cbToRead, &cbRead);
        if (RT_SUCCESS(rc))
        {
             Msg.cbData.SetUInt32(cbRead);
             Msg.pvData.SetPtr(pvBuf, cbRead);

             rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
             if (RT_SUCCESS(rc))
                 rc = Msg.hdr.result;
        }

        if (RT_FAILURE(rc))
            break;

        Assert(cbRead <= cbData);
        cbData -= cbRead;

    } while (cbData);

    RTMemFree(pvBuf);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3DnDGHSendURIObject(uint32_t u32ClientId, DnDURIObject &obj)
{
    int rc;

    switch (obj.GetType())
    {
        case DnDURIObject::Directory:
            rc = vbglR3DnDGHSendDir(u32ClientId, obj);
            break;

        case DnDURIObject::File:
            rc = vbglR3DnDGHSendFile(u32ClientId, obj);
            break;

        default:
            AssertMsgFailed(("Type %ld not implemented\n",
                             obj.GetType()));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    return rc;
}

static int vbglR3DnDGHProcessURIMessages(uint32_t u32ClientId,
                                         const void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths =
        RTCString((const char *)pvData, cbData).split("\r\n");

    DnDURIList lstURI;
    int rc = lstURI.AppendURIPathsFromList(lstPaths, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        /* Send metadata; in this case it's the (non-recursive) file/directory
         * URI list the host needs to know to initialize the drag'n drop operation. */
        RTCString strRootDest = lstURI.RootToString();
        Assert(strRootDest.isNotEmpty());

        void *pvToSend = (void *)strRootDest.c_str();
        uint32_t cbToSend = (uint32_t)strRootDest.length() + 1;

        rc = vbglR3DnDGHSendDataInternal(u32ClientId, pvToSend, cbToSend,
                                         /* Include total bytes of all file paths,
                                          * file sizes etc. */
                                         lstURI.TotalBytes());
    }

    if (RT_SUCCESS(rc))
    {
        while (!lstURI.IsEmpty())
        {
            DnDURIObject &nextObj = lstURI.First();

            rc = vbglR3DnDGHSendURIObject(u32ClientId, nextObj);
            if (RT_FAILURE(rc))
                break;

            lstURI.RemoveFirst();
        }
    }

    return rc;
}

VBGLR3DECL(int) VbglR3DnDGHSendData(uint32_t u32ClientId,
                                    const char *pszFormat,
                                    void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pszFormat, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    int rc;
    if (DnDMIMEHasFileURLs(pszFormat, strlen(pszFormat)))
    {
        rc = vbglR3DnDGHProcessURIMessages(u32ClientId, pvData, cbData);
    }
    else
        rc = vbglR3DnDGHSendDataInternal(u32ClientId, pvData, cbData,
                                         0 /* cbAdditionalData */);
    if (RT_FAILURE(rc))
    {
        int rc2 = VbglR3DnDGHSendError(u32ClientId, rc);
        AssertRC(rc2);
    }

    return rc;
}

VBGLR3DECL(int) VbglR3DnDGHSendError(uint32_t u32ClientId, int rcErr)
{
    DragAndDropSvc::VBOXDNDGHEVTERRORMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_EVT_ERROR;
    Msg.hdr.cParms      = 1;

    Msg.uRC.SetUInt32((uint32_t)rcErr); /* uint32_t vs. int. */

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    LogFlowFunc(("Sending error %Rrc returned with rc=%Rrc\n", rcErr, rc));
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
