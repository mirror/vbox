/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Drag & Drop.
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

#include "VBGLR3Internal.h"
#include "VBox/HostServices/DragAndDropSvc.h"

#define VERBOSE 1

#if defined(VERBOSE) && defined(DEBUG_poetzsch)
# include <iprt/stream.h>
# define DO(s) RTPrintf s
#else
# define DO(s) do {} while(0)
//# define DO(s) Log s
#endif

/* Here all the communication with the host over HGCM is handled platform
 * neutral. Also the receiving of URIs content (directory trees and files) is
 * done here. So the platform code of the guests, should not take care of that.
 *
 * Todo:
 * - Sending dirs/files in the G->H case
 * - Maybe the EOL converting of text mime-types (not fully sure, eventually
 *   better done on the host side)
 */

/* Not really used at the moment (only one client is possible): */
uint32_t g_clientId = 0;

/******************************************************************************
 *    Private internal functions                                              *
 ******************************************************************************/

static int vbglR3DnDCreateDropDir(char* pszDropDir, size_t cbSize)
{
    /* Validate input */
    AssertPtrReturn(pszDropDir, VERR_INVALID_POINTER);
    AssertReturn(cbSize,        VERR_INVALID_PARAMETER);

    /* Get the users document directory (usually $HOME/Documents). */
    int rc = RTPathUserDocuments(pszDropDir, cbSize);
    if (RT_FAILURE(rc))
        return rc;
    /* Append our base drop directory. */
    rc = RTPathAppend(pszDropDir, cbSize, "VirtualBox Dropped Files");
    if (RT_FAILURE(rc))
        return rc;
    /* Create it when necessary. */
    if (!RTDirExists(pszDropDir))
    {
        rc = RTDirCreateFullPath(pszDropDir, RTFS_UNIX_IRWXU);
        if (RT_FAILURE(rc))
            return rc;
    }
    /* The actually drop directory consist of the current time stamp and a
     * unique number when necessary. */
    char pszTime[64];
    RTTIMESPEC time;
    if (!RTTimeSpecToString(RTTimeNow(&time), pszTime, sizeof(pszTime)))
        return VERR_BUFFER_OVERFLOW;
    rc = RTPathAppend(pszDropDir, cbSize, pszTime);
    if (RT_FAILURE(rc))
        return rc;

    /* Create it (only accessible by the current user) */
    return RTDirCreateUniqueNumbered(pszDropDir, cbSize, RTFS_UNIX_IRWXU, 3, '-');
}

static int vbglR3DnDQueryNextHostMessageType(uint32_t uClientId, uint32_t *puMsg, uint32_t *pcParms, bool fWait)
{
    /* Validate input */
    AssertPtrReturn(puMsg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDNEXTMSGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG;
    Msg.hdr.cParms      = 3;
    /* Initialize parameter */
    Msg.msg.SetUInt32(0);
    Msg.num_parms.SetUInt32(0);
    Msg.block.SetUInt32(fWait);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            /* Fetch results */
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
    /* Validate input */
    AssertPtrReturn(puScreenId,     VERR_INVALID_POINTER);
    AssertPtrReturn(puX,            VERR_INVALID_POINTER);
    AssertPtrReturn(puY,            VERR_INVALID_POINTER);
    AssertPtrReturn(puDefAction,    VERR_INVALID_POINTER);
    AssertPtrReturn(puAllActions,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszFormats,     VERR_INVALID_POINTER);
    AssertReturn(cbFormats,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatsRecv, VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGACTIONMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = uMsg;
    Msg.hdr.cParms      = 7;
    /* Initialize parameter */
    Msg.uScreenId.SetUInt32(0);
    Msg.uX.SetUInt32(0);
    Msg.uY.SetUInt32(0);
    Msg.uDefAction.SetUInt32(0);
    Msg.uAllActions.SetUInt32(0);
    Msg.pvFormats.SetPtr(pszFormats, cbFormats);
    Msg.cFormats.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            /* Fetch results */
            rc = Msg.uScreenId.GetUInt32(puScreenId);     AssertRC(rc);
            rc = Msg.uX.GetUInt32(puX);                   AssertRC(rc);
            rc = Msg.uY.GetUInt32(puY);                   AssertRC(rc);
            rc = Msg.uDefAction.GetUInt32(puDefAction);   AssertRC(rc);
            rc = Msg.uAllActions.GetUInt32(puAllActions); AssertRC(rc);
            rc = Msg.cFormats.GetUInt32(pcbFormatsRecv);  AssertRC(rc);
            /* A little bit paranoia */
            AssertReturn(cbFormats >= *pcbFormatsRecv, VERR_TOO_MUCH_DATA);
        }
    }
    return rc;
}

static int vbglR3DnDHGProcessLeaveMessage(uint32_t uClientId)
{
    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGLEAVEMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_EVT_LEAVE;
    Msg.hdr.cParms      = 0;
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}

static int vbglR3DnDHGProcessCancelMessage(uint32_t uClientId)
{
    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGCANCELMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_EVT_CANCEL;
    Msg.hdr.cParms      = 0;
    /* Do request */
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
    /* Validate input */
    AssertPtrReturn(pszDirname,     VERR_INVALID_POINTER);
    AssertReturn(cbDirname,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDirnameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,         VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGSENDDIRMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_DIR;
    Msg.hdr.cParms      = 3;
    /* Initialize parameter */
    Msg.pvName.SetPtr(pszDirname, cbDirname);
    Msg.cName.SetUInt32(0);
    Msg.fMode.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(Msg.hdr.result))
        {
            /* Fetch results */
            rc = Msg.cName.GetUInt32(pcbDirnameRecv); AssertRC(rc);
            rc = Msg.fMode.GetUInt32(pfMode);         AssertRC(rc);
            /* A little bit paranoia */
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
    /* Validate input */
    AssertPtrReturn(pszFilename,     VERR_INVALID_POINTER);
    AssertReturn(cbFilename,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFilenameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv,     VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,          VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGSENDFILEMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_FILE;
    Msg.hdr.cParms      = 5;
    /* Initialize parameter */
    Msg.pvName.SetPtr(pszFilename, cbFilename);
    Msg.cName.SetUInt32(0);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cData.SetUInt32(0);
    Msg.fMode.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            /* Fetch results */
            rc = Msg.cName.GetUInt32(pcbFilenameRecv); AssertRC(rc);
            rc = Msg.cData.GetUInt32(pcbDataRecv);     AssertRC(rc);
            rc = Msg.fMode.GetUInt32(pfMode);          AssertRC(rc);
            /* A little bit paranoia */
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
    /* Make a string list out of the uri data. */
    RTCList<RTCString> uriList = RTCString(static_cast<char*>(*ppvData), *pcbDataRecv - 1).split("\r\n");
    if (uriList.isEmpty())
        return VINF_SUCCESS;

    uint32_t cbTmpData = _1M * 10;
    void *pvTmpData = RTMemAlloc(cbTmpData);
    if (!pvTmpData)
        return VERR_NO_MEMORY;

    /* Create and query the drop target directory. */
    char pszDropDir[RTPATH_MAX];
    int rc = vbglR3DnDCreateDropDir(pszDropDir, sizeof(pszDropDir));
    if (RT_FAILURE(rc))
    {
        RTMemFree(pvTmpData);
        return rc;
    }

    /* Patch the old drop data with the new drop directory, so the drop target
     * can find the files. */
    RTCList<RTCString> guestUriList;
    for (size_t i = 0; i < uriList.size(); ++i)
    {
        const RTCString &strUri = uriList.at(i);
        /* Query the path component of a file URI. If this hasn't a
         * file scheme, null is returned. */
        if (char *pszFilePath = RTUriFilePath(strUri.c_str(), URI_FILE_FORMAT_AUTO))
        {
            RTCString strFullPath = RTCString().printf("%s%c%s", pszDropDir, RTPATH_SLASH, pszFilePath);
            char *pszNewUri = RTUriFileCreate(strFullPath.c_str());
            if (pszNewUri)
            {
                guestUriList.append(pszNewUri);
                RTStrFree(pszNewUri);
            }
        }
        else
            guestUriList.append(strUri);
    }

    /* Cleanup the old data and write the new data back to the event. */
    RTMemFree(*ppvData);
    RTCString newData = RTCString::join(guestUriList, "\r\n") + "\r\n";
    *ppvData = RTStrDupN(newData.c_str(), newData.length());
    *pcbDataRecv = newData.length() + 1;

    /* Lists for holding created files & directories in the case of a
     * rollback. */
    RTCList<RTCString> guestDirList;
    RTCList<RTCString> guestFileList;
    char pszPathname[RTPATH_MAX];
    uint32_t cbPathname = 0;
    bool fLoop = true;
    do
    {
        uint32_t uNextMsg;
        uint32_t cNextParms;
        rc = vbglR3DnDQueryNextHostMessageType(uClientId, &uNextMsg, &cNextParms, false);
        DO(("%Rrc - %d\n", rc , uNextMsg));
        if (RT_SUCCESS(rc))
        {
            switch(uNextMsg)
            {
                case DragAndDropSvc::HOST_DND_HG_SND_DIR:
                {
                    uint32_t fMode = 0;
                    rc = vbglR3DnDHGProcessSendDirMessage(uClientId,
                                                          pszPathname,
                                                          sizeof(pszPathname),
                                                          &cbPathname,
                                                          &fMode);
                    if (RT_SUCCESS(rc))
                    {
                        DO(("Got drop dir: %s - %o - %Rrc\n", pszPathname, fMode, rc));
                        char *pszNewDir = RTPathJoinA(pszDropDir, pszPathname);
                        rc = RTDirCreate(pszNewDir, (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRWXU, 0);
                        if (!guestDirList.contains(pszNewDir))
                            guestDirList.append(pszNewDir);
                    }
                    break;
                }
                case DragAndDropSvc::HOST_DND_HG_SND_FILE:
                {
                    uint32_t cbDataRecv;
                    uint32_t fMode = 0;
                    rc = vbglR3DnDHGProcessSendFileMessage(uClientId,
                                                           pszPathname,
                                                           sizeof(pszPathname),
                                                           &cbPathname,
                                                           pvTmpData,
                                                           cbTmpData,
                                                           &cbDataRecv,
                                                           &fMode);
                    if (RT_SUCCESS(rc))
                    {
                        char *pszNewFile = RTPathJoinA(pszDropDir, pszPathname);
                        DO(("Got drop file: %s - %d - %o - %Rrc\n", pszPathname, cbDataRecv, fMode, rc));
                        RTFILE hFile;
                        rc = RTFileOpen(&hFile, pszNewFile, RTFILE_O_WRITE | RTFILE_O_APPEND | RTFILE_O_DENY_ALL | RTFILE_O_OPEN_CREATE);
                        if (RT_SUCCESS(rc))
                        {
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
                            if (!guestFileList.contains(pszNewFile))
                                guestFileList.append(pszNewFile);
                        }
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
                default: fLoop = false; break;
            }
        } else
        {
            if (rc == VERR_NO_DATA)
                rc = VINF_SUCCESS;
            break;
        }
    }while(fLoop);

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
                                                 uint32_t *pcbDataRecv)
{
    /* Validate input */
    AssertPtrReturn(puScreenId,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszFormat,     VERR_INVALID_POINTER);
    AssertReturn(cbFormat,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,        VERR_INVALID_POINTER);
    AssertReturn(cbData,           VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv,   VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGSENDDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_DATA;
    Msg.hdr.cParms      = 5;
    /* Initialize parameter */
    Msg.uScreenId.SetUInt32(0);
    Msg.pvFormat.SetPtr(pszFormat, cbFormat);
    Msg.cFormat.SetUInt32(0);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cData.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (   RT_SUCCESS(rc)
            || rc == VERR_BUFFER_OVERFLOW)
        {
            /* Fetch results */
            rc = Msg.uScreenId.GetUInt32(puScreenId);  AssertRC(rc);
            rc = Msg.cFormat.GetUInt32(pcbFormatRecv); AssertRC(rc);
            rc = Msg.cData.GetUInt32(pcbDataRecv);     AssertRC(rc);
            /* A little bit paranoia */
            AssertReturn(cbFormat >= *pcbFormatRecv, VERR_TOO_MUCH_DATA);
            AssertReturn(cbData   >= *pcbDataRecv,   VERR_TOO_MUCH_DATA);
        }
    }
    return rc;
}

static int vbglR3DnDHGProcessMoreDataMessageInternal(uint32_t  uClientId,
                                                     void     *pvData,
                                                     uint32_t  cbData,
                                                     uint32_t *pcbDataRecv)
{
    /* Validate input */
    AssertPtrReturn(pvData,      VERR_INVALID_POINTER);
    AssertReturn(cbData,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv, VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGSENDMOREDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_HG_SND_MORE_DATA;
    Msg.hdr.cParms      = 2;
    /* Initialize parameter */
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cData.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (   RT_SUCCESS(rc)
            || rc == VERR_BUFFER_OVERFLOW)
        {
            /* Fetch results */
            rc = Msg.cData.GetUInt32(pcbDataRecv); AssertRC(rc);
            /* A little bit paranoia */
            AssertReturn(cbData >= *pcbDataRecv, VERR_TOO_MUCH_DATA);
        }
    }
    return rc;
}

static int vbglR3DnDHGProcessSendDataMessages(uint32_t  uClientId,
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

    size_t cbAllDataRecv = cbDataRecv;
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
    int rc = vbglR3DnDHGProcessSendDataMessages(uClientId,
                                                puScreenId,
                                                pszFormat,
                                                cbFormat,
                                                pcbFormatRecv,
                                                ppvData,
                                                cbData,
                                                pcbDataRecv);
    if (RT_SUCCESS(rc))
    {
        /* Check if this is a uri-event */
        if (RTStrNICmp(pszFormat, "text/uri-list", *pcbFormatRecv) == 0)
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
    /* Validate input */
    AssertPtrReturn(puScreenId, VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDGHREQPENDINGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_GH_REQ_PENDING;
    Msg.hdr.cParms      = 1;
    /* Initialize parameter */
    Msg.uScreenId.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            /* Fetch results */
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
    /* Validate input */
    AssertPtrReturn(pszFormat,     VERR_INVALID_POINTER);
    AssertReturn(cbFormat,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbFormatRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(puAction,      VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDGHDROPPEDMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.u32ClientID = uClientId;
    Msg.hdr.u32Function = DragAndDropSvc::HOST_DND_GH_EVT_DROPPED;
    Msg.hdr.cParms      = 3;
    /* Initialize parameter */
    Msg.pvFormat.SetPtr(pszFormat, cbFormat);
    Msg.cFormat.SetUInt32(0);
    Msg.uAction.SetUInt32(0);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.hdr.result;
        if (RT_SUCCESS(rc))
        {
            /* Fetch results */
            rc = Msg.cFormat.GetUInt32(pcbFormatRecv); AssertRC(rc);
            rc = Msg.uAction.GetUInt32(puAction);      AssertRC(rc);
            /* A little bit paranoia */
            AssertReturn(cbFormat >= *pcbFormatRecv, VERR_TOO_MUCH_DATA);
        }
    }
    return rc;
}

/******************************************************************************
 *    Public functions                                                        *
 ******************************************************************************/

/**
 * Initialize Drag & Drop.
 *
 * This will enable the Drag & Drop events.
 *
 * @returns VBox status code.
 */
VBGLR3DECL(int) VbglR3DnDInit(void)
{
    return VbglR3DnDConnect(&g_clientId);
}

/**
 * Terminate Drag and Drop.
 *
 * This will Drag and Drop events.
 *
 * @returns VBox status.
 */
VBGLR3DECL(int) VbglR3DnDTerm(void)
{
    return VbglR3DnDDisconnect(g_clientId);
}

VBGLR3DECL(int) VbglR3DnDConnect(uint32_t *pu32ClientId)
{
    /* Validate input */
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
    return rc;
}

VBGLR3DECL(int) VbglR3DnDDisconnect(uint32_t u32ClientId)
{
    /* Initialize header */
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result      = VERR_WRONG_ORDER;
    Info.u32ClientID = u32ClientId;
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;
    return rc;
}

VBGLR3DECL(int) VbglR3DnDProcessNextMessage(CPVBGLR3DNDHGCMEVENT pEvent)
{
    /* Validate input */
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    uint32_t       uMsg       = 0;
    uint32_t       uNumParms  = 0;
    const uint32_t ccbFormats = _64K;
    const uint32_t ccbData    = _1M;
    int rc = vbglR3DnDQueryNextHostMessageType(g_clientId, &uMsg, &uNumParms, true);
    if (RT_SUCCESS(rc))
    {
        DO(("Got message %d\n", uMsg));
        switch(uMsg)
        {
            case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
            case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
            case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    return VERR_NO_MEMORY;
                rc = vbglR3DnDHGProcessActionMessage(g_clientId,
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
                rc = vbglR3DnDHGProcessLeaveMessage(g_clientId);
                break;
            }
            case DragAndDropSvc::HOST_DND_HG_SND_DATA:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    return VERR_NO_MEMORY;
                pEvent->u.b.pvData = RTMemAlloc(ccbData);
                if (!pEvent->u.b.pvData)
                {
                    RTMemFree(pEvent->pszFormats);
                    pEvent->pszFormats = NULL;
                    return VERR_NO_MEMORY;
                }
                rc = vbglR3DnDHGProcessSendDataMessage(g_clientId,
                                                       &pEvent->uScreenId,
                                                       pEvent->pszFormats,
                                                       ccbFormats,
                                                       &pEvent->cbFormats,
                                                       &pEvent->u.b.pvData,
                                                       ccbData,
                                                       &pEvent->u.b.cbData);
                break;
            }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
            {
                pEvent->uType = uMsg;
                rc = vbglR3DnDGHProcessRequestPendingMessage(g_clientId,
                                                             &pEvent->uScreenId);
                break;
            }
            case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
            {
                pEvent->uType = uMsg;
                pEvent->pszFormats = static_cast<char*>(RTMemAlloc(ccbFormats));
                if (!pEvent->pszFormats)
                    return VERR_NO_MEMORY;
                rc = vbglR3DnDGHProcessDroppedMessage(g_clientId,
                                                      pEvent->pszFormats,
                                                      ccbFormats,
                                                      &pEvent->cbFormats,
                                                      &pEvent->u.a.uDefAction);
                break;
            }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
            case DragAndDropSvc::HOST_DND_HG_EVT_CANCEL:
            {
                pEvent->uType = uMsg;
                rc = vbglR3DnDHGProcessCancelMessage(g_clientId);
                if (RT_SUCCESS(rc))
                    rc = VERR_CANCELLED;
                break;
            }
            default: AssertMsgFailedReturn(("Message %u isn't expected in this context", uMsg), VERR_INVALID_PARAMETER); break;
        }
    }
    return rc;
}

VBGLR3DECL(int) VbglR3DnDHGAcknowledgeOperation(uint32_t uAction)
{
    DO(("ACK: %u\n", uAction));
    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGACKOPMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_HG_ACK_OP;
    Msg.hdr.cParms      = 1;
    /* Initialize parameter */
    Msg.uAction.SetUInt32(uAction);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}

VBGLR3DECL(int) VbglR3DnDHGRequestData(const char* pcszFormat)
{
    DO(("DATA_REQ: '%s'\n", pcszFormat));
    /* Validate input */
    AssertPtrReturn(pcszFormat, VERR_INVALID_PARAMETER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDHGREQDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_HG_REQ_DATA;
    Msg.hdr.cParms      = 1;
    /* Do request */
    Msg.pFormat.SetPtr((void*)pcszFormat, (uint32_t)strlen(pcszFormat) + 1);
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}

VBGLR3DECL(int) VbglR3DnDGHAcknowledgePending(uint32_t uDefAction, uint32_t uAllActions, const char* pcszFormat)
{
    DO(("PEND: %u: %u (%s)\n", uDefAction, uAllActions, pcszFormat));
    /* Validate input */
    AssertPtrReturn(pcszFormat, VERR_INVALID_POINTER);

    /* Initialize header */
    DragAndDropSvc::VBOXDNDGHACKPENDINGMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_ACK_PENDING;
    Msg.hdr.cParms      = 3;
    /* Initialize parameter */
    Msg.uDefAction.SetUInt32(uDefAction);
    Msg.uAllActions.SetUInt32(uAllActions);
    Msg.pFormat.SetPtr((void*)pcszFormat, static_cast<uint32_t>(strlen(pcszFormat) + 1));
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}

VBGLR3DECL(int) VbglR3DnDGHSendData(void *pvData, uint32_t cbData)
{
    DO(("DATA: %x (%u)\n", pvData, cbData));
    /* Validate input */
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData,    VERR_INVALID_PARAMETER);

    /* Todo: URI support. Currently only data is send over to the host. For URI
     * support basically the same as in the H->G case (see
     * HostServices/DragAndDrop/dndmanager.h/cpp) has to be done:
     * 1. Parse the urilist
     * 2. Recursively send "create dir" and "transfer file" msg to the host
     * 3. Patch the urilist by removing all base dirnames
     * 4. On the host all needs to received and the urilist patched afterwards
     *    to point to the new location
     */

    /* Initialize header */
    DragAndDropSvc::VBOXDNDGHSENDDATAMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_SND_DATA;
    Msg.hdr.cParms      = 2;
    Msg.uSize.SetUInt32(cbData);
    int rc          = VINF_SUCCESS;
    uint32_t cbMax  = _1M;
    uint32_t cbSend = 0;
    while(cbSend < cbData)
    {
        /* Initialize parameter */
        uint32_t cbToSend = RT_MIN(cbData - cbSend, cbMax);
        Msg.pData.SetPtr(static_cast<uint8_t*>(pvData) + cbSend, cbToSend);
        /* Do request */
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            rc = Msg.hdr.result;
            /* Did the host cancel the event? */
            if (rc == VERR_CANCELLED)
                break;
        }
        else
            break;
        cbSend += cbToSend;
//        RTThreadSleep(500);
    }
    return rc;
}

VBGLR3DECL(int) VbglR3DnDGHErrorEvent(int rcOp)
{
    DO(("GH_ERROR\n"));

    /* Initialize header */
    DragAndDropSvc::VBOXDNDGHEVTERRORMSG Msg;
    RT_ZERO(Msg);
    Msg.hdr.result      = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = g_clientId;
    Msg.hdr.u32Function = DragAndDropSvc::GUEST_DND_GH_EVT_ERROR;
    Msg.hdr.cParms      = 1;
    /* Initialize parameter */
    Msg.uRC.SetUInt32(rcOp);
    /* Do request */
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    return rc;
}
