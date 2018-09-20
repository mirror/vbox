/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Drag & Drop.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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

#include <VBox/VBoxGuestLib.h>
#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/DragAndDropSvc.h>

using namespace DragAndDropSvc;

#include "VBoxGuestR3LibInternal.h"


/*********************************************************************************************************************************
*   Private internal functions                                                                                                   *
*********************************************************************************************************************************/

/**
 * Receives the next upcoming message for a given DnD context.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   puMsg               Where to store the message type.
 * @param   pcParms             Where to store the number of parameters required for receiving the message.
 * @param   fWait               Whether to wait (block) for a new message to arrive or not.
 */
static int vbglR3DnDGetNextMsgType(PVBGLR3GUESTDNDCMDCTX pCtx, uint32_t *puMsg, uint32_t *pcParms, bool fWait)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(puMsg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    VBOXDNDNEXTMSGMSG Msg;
    RT_ZERO(Msg);
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_GET_NEXT_HOST_MSG, 3);
    Msg.uMsg.SetUInt32(0);
    Msg.cParms.SetUInt32(0);
    Msg.fBlock.SetUInt32(fWait ? 1 : 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uMsg.GetUInt32(puMsg);     AssertRC(rc);
        rc = Msg.cParms.GetUInt32(pcParms); AssertRC(rc);
    }

    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive a so-called "action message" from the host.
 * Certain DnD messages use the same amount / sort of parameters and grouped as "action messages".
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   uMsg                Which kind of message to receive.
 * @param   puScreenID          Where to store the host screen ID the message is bound to. Optional.
 * @param   puX                 Where to store the absolute X coordinates. Optional.
 * @param   puY                 Where to store the absolute Y coordinates. Optional.
 * @param   puDefAction         Where to store the default action to perform. Optional.
 * @param   puAllActions        Where to store the available actions. Optional.
 * @param   ppszFormats         Where to store List of formats. Optional.
 * @param   pcbFormats          Size (in bytes) of where to store the list of formats. Optional.
 *
 * @todo r=andy Get rid of this function as soon as we resolved the protocol TODO #1.
 *              This was part of the initial protocol and needs to go.
 */
static int vbglR3DnDHGRecvAction(PVBGLR3GUESTDNDCMDCTX pCtx,
                                 uint32_t   uMsg,
                                 uint32_t  *puScreenID,
                                 uint32_t  *puX,
                                 uint32_t  *puY,
                                 uint32_t  *puDefAction,
                                 uint32_t  *puAllActions,
                                 char     **ppszFormats,
                                 uint32_t  *pcbFormats)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    /* The rest is optional. */

    const uint32_t cbFormatsTmp = pCtx->cbMaxChunkSize;

    char *pszFormatsTmp = static_cast<char *>(RTMemAlloc(cbFormatsTmp));
    if (!pszFormatsTmp)
        return VERR_NO_MEMORY;

    VBOXDNDHGACTIONMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, uMsg, 8);
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.uScreenId.SetUInt32(0);
    Msg.u.v3.uX.SetUInt32(0);
    Msg.u.v3.uY.SetUInt32(0);
    Msg.u.v3.uDefAction.SetUInt32(0);
    Msg.u.v3.uAllActions.SetUInt32(0);
    Msg.u.v3.pvFormats.SetPtr(pszFormatsTmp, cbFormatsTmp);
    Msg.u.v3.cbFormats.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        if (RT_SUCCESS(rc) && puScreenID)
            rc = Msg.u.v3.uScreenId.GetUInt32(puScreenID);
        if (RT_SUCCESS(rc) && puX)
            rc = Msg.u.v3.uX.GetUInt32(puX);
        if (RT_SUCCESS(rc) && puY)
            rc = Msg.u.v3.uY.GetUInt32(puY);
        if (RT_SUCCESS(rc) && puDefAction)
            rc = Msg.u.v3.uDefAction.GetUInt32(puDefAction);
        if (RT_SUCCESS(rc) && puAllActions)
            rc = Msg.u.v3.uAllActions.GetUInt32(puAllActions);
        if (RT_SUCCESS(rc) && pcbFormats)
            rc = Msg.u.v3.cbFormats.GetUInt32(pcbFormats);

        if (RT_SUCCESS(rc))
        {
            if (ppszFormats)
            {
                *ppszFormats = RTStrDup(pszFormatsTmp);
                if (!*ppszFormats)
                    rc = VERR_NO_MEMORY;
            }
        }
    }

    RTStrFree(pszFormatsTmp);

    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive a HOST_DND_HG_EVT_LEAVE message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 */
static int vbglR3DnDHGRecvLeave(PVBGLR3GUESTDNDCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBOXDNDHGLEAVEMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_EVT_LEAVE, 1);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Host -> Guest
 * Utility function to receive a HOST_DND_HG_EVT_CANCEL message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 */
static int vbglR3DnDHGRecvCancel(PVBGLR3GUESTDNDCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBOXDNDHGCANCELMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_EVT_CANCEL, 1);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Host -> Guest
 * Utility function to receive a HOST_DND_HG_SND_DIR message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pszDirname          Where to store the directory name of the directory being created.
 * @param   cbDirname           Size (in bytes) of where to store the directory name of the directory being created.
 * @param   pcbDirnameRecv      Size (in bytes) of the actual directory name received.
 * @param   pfMode              Where to store the directory creation mode.
 */
static int vbglR3DnDHGRecvDir(PVBGLR3GUESTDNDCMDCTX pCtx,
                              char     *pszDirname,
                              uint32_t  cbDirname,
                              uint32_t *pcbDirnameRecv,
                              uint32_t *pfMode)
{
    AssertPtrReturn(pCtx,           VERR_INVALID_POINTER);
    AssertPtrReturn(pszDirname,     VERR_INVALID_POINTER);
    AssertReturn(cbDirname,         VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDirnameRecv, VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,         VERR_INVALID_POINTER);

    VBOXDNDHGSENDDIRMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_SND_DIR, 4);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvName.SetPtr(pszDirname, cbDirname);
    Msg.u.v3.cbName.SetUInt32(cbDirname);
    Msg.u.v3.fMode.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        rc = Msg.u.v3.cbName.GetUInt32(pcbDirnameRecv); AssertRC(rc);
        rc = Msg.u.v3.fMode.GetUInt32(pfMode);          AssertRC(rc);

        AssertReturn(cbDirname >= *pcbDirnameRecv, VERR_TOO_MUCH_DATA);
    }

    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive a HOST_DND_HG_SND_FILE_DATA message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pvData              Where to store the file data chunk.
 * @param   cbData              Size (in bytes) of where to store the data chunk.
 * @param   pcbDataRecv         Size (in bytes) of the actual data chunk size received.
 */
static int vbglR3DnDHGRecvFileData(PVBGLR3GUESTDNDCMDCTX pCtx,
                                   void                 *pvData,
                                   uint32_t              cbData,
                                   uint32_t             *pcbDataRecv)
{
    AssertPtrReturn(pCtx,            VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDataRecv,     VERR_INVALID_POINTER);

    VBOXDNDHGSENDFILEDATAMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_SND_FILE_DATA, 5);
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvData.SetPtr(pvData, cbData);
    Msg.u.v3.cbData.SetUInt32(0);
    Msg.u.v3.pvChecksum.SetPtr(NULL, 0);
    Msg.u.v3.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        rc = Msg.u.v3.cbData.GetUInt32(pcbDataRecv); AssertRC(rc);
        AssertReturn(cbData >= *pcbDataRecv, VERR_TOO_MUCH_DATA);
        /** @todo Add checksum support. */
    }

    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive the HOST_DND_HG_SND_FILE_HDR message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pszFilename         Where to store the file name of the file being transferred.
 * @param   cbFilename          Size (in bytes) of where to store the file name of the file being transferred.
 * @param   puFlags             File transfer flags. Currently not being used.
 * @param   pfMode              Where to store the file creation mode.
 * @param   pcbTotal            Where to store the file size (in bytes).
 */
static int vbglR3DnDHGRecvFileHdr(PVBGLR3GUESTDNDCMDCTX  pCtx,
                                  char                  *pszFilename,
                                  uint32_t               cbFilename,
                                  uint32_t              *puFlags,
                                  uint32_t              *pfMode,
                                  uint64_t              *pcbTotal)
{
    AssertPtrReturn(pCtx,        VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(cbFilename,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(puFlags,     VERR_INVALID_POINTER);
    AssertPtrReturn(pfMode,      VERR_INVALID_POINTER);
    AssertReturn(pcbTotal,       VERR_INVALID_POINTER);

    VBOXDNDHGSENDFILEHDRMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_SND_FILE_HDR, 6);
    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.pvName.SetPtr(pszFilename, cbFilename);
    Msg.cbName.SetUInt32(cbFilename);
    Msg.uFlags.SetUInt32(0);
    Msg.fMode.SetUInt32(0);
    Msg.cbTotal.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Get context ID. */
        rc = Msg.uFlags.GetUInt32(puFlags);   AssertRC(rc);
        rc = Msg.fMode.GetUInt32(pfMode);     AssertRC(rc);
        rc = Msg.cbTotal.GetUInt64(pcbTotal); AssertRC(rc);
    }

    return rc;
}

/**
 * Host -> Guest
 * Helper function for receiving URI data from the host. Do not call directly.
 * This function also will take care of the file creation / locking on the guest.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pDataHdr            DnD data header to use. Needed for accounting.
 * @param   pDroppedFiles       Dropped files object to use for maintaining the file creation / locking.
 */
static int vbglR3DnDHGRecvURIData(PVBGLR3GUESTDNDCMDCTX pCtx, PVBOXDNDSNDDATAHDR pDataHdr, DnDDroppedFiles *pDroppedFiles)
{
    AssertPtrReturn(pCtx,          VERR_INVALID_POINTER);
    AssertPtrReturn(pDataHdr,      VERR_INVALID_POINTER);
    AssertPtrReturn(pDroppedFiles, VERR_INVALID_POINTER);

    /* Only count the raw data minus the already received meta data. */
    Assert(pDataHdr->cbTotal >= pDataHdr->cbMeta);
    uint64_t cbToRecvBytes = pDataHdr->cbTotal - pDataHdr->cbMeta;
    uint64_t cToRecvObjs   = pDataHdr->cObjects;

    LogFlowFunc(("cbToRecvBytes=%RU64, cToRecvObjs=%RU64, (cbTotal=%RU64, cbMeta=%RU32)\n",
                 cbToRecvBytes, cToRecvObjs, pDataHdr->cbTotal, pDataHdr->cbMeta));

    /* Anything to do at all? */
    /* Note: Do not check for cbToRecvBytes == 0 here, as this might be just
     *       a bunch of 0-byte files to be transferred. */
    if (!cToRecvObjs)
        return VINF_SUCCESS;

    /*
     * Allocate temporary chunk buffer.
     */
    uint32_t cbChunkMax = pCtx->cbMaxChunkSize;
    void *pvChunk = RTMemAlloc(cbChunkMax);
    if (!pvChunk)
        return VERR_NO_MEMORY;
    uint32_t cbChunkRead   = 0;

    uint64_t cbFileSize    = 0; /* Total file size (in bytes). */
    uint64_t cbFileWritten = 0; /* Written bytes. */

    /*
     * Create and query the (unique) drop target directory in the user's temporary directory.
     */
    int rc = pDroppedFiles->OpenTemp(0 /* fFlags */);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pvChunk);
        return VERR_NO_MEMORY;
    }

    const char *pszDropDir = pDroppedFiles->GetDirAbs();
    AssertPtr(pszDropDir);

    /*
     * Enter the main loop of retieving files + directories.
     */
    DnDURIObject objFile(DnDURIObject::File);

    char szPathName[RTPATH_MAX] = { 0 };
    uint32_t cbPathName = 0;
    uint32_t fFlags     = 0;
    uint32_t fMode      = 0;

    do
    {
        LogFlowFunc(("Wating for new message ...\n"));

        uint32_t uNextMsg;
        uint32_t cNextParms;
        rc = vbglR3DnDGetNextMsgType(pCtx, &uNextMsg, &cNextParms, true /* fWait */);
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("uNextMsg=%RU32, cNextParms=%RU32\n", uNextMsg, cNextParms));

            switch (uNextMsg)
            {
                case HOST_DND_HG_SND_DIR:
                {
                    rc = vbglR3DnDHGRecvDir(pCtx,
                                            szPathName,
                                            sizeof(szPathName),
                                            &cbPathName,
                                            &fMode);
                    LogFlowFunc(("HOST_DND_HG_SND_DIR pszPathName=%s, cbPathName=%RU32, fMode=0x%x, rc=%Rrc\n",
                                 szPathName, cbPathName, fMode, rc));

                    char *pszPathAbs = RTPathJoinA(pszDropDir, szPathName);
                    if (pszPathAbs)
                    {
#ifdef RT_OS_WINDOWS
                        uint32_t fCreationMode = (fMode & RTFS_DOS_MASK) | RTFS_DOS_NT_NORMAL;
#else
                        uint32_t fCreationMode = (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRWXU;
#endif
                        rc = RTDirCreate(pszPathAbs, fCreationMode, 0);
                        if (RT_SUCCESS(rc))
                            rc = pDroppedFiles->AddDir(pszPathAbs);

                        if (RT_SUCCESS(rc))
                        {
                            Assert(cToRecvObjs);
                            cToRecvObjs--;
                        }

                        RTStrFree(pszPathAbs);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                    break;
                }
                case HOST_DND_HG_SND_FILE_HDR:
                case HOST_DND_HG_SND_FILE_DATA:
                {
                    if (uNextMsg == HOST_DND_HG_SND_FILE_HDR)
                    {
                        rc = vbglR3DnDHGRecvFileHdr(pCtx,
                                                    szPathName,
                                                    sizeof(szPathName),
                                                    &fFlags,
                                                    &fMode,
                                                    &cbFileSize);
                        LogFlowFunc(("HOST_DND_HG_SND_FILE_HDR: "
                                     "szPathName=%s, fFlags=0x%x, fMode=0x%x, cbFileSize=%RU64, rc=%Rrc\n",
                                     szPathName, fFlags, fMode, cbFileSize, rc));
                    }
                    else
                    {
                        rc = vbglR3DnDHGRecvFileData(pCtx,
                                                     pvChunk,
                                                     cbChunkMax,
                                                     &cbChunkRead);
                        LogFlowFunc(("HOST_DND_HG_SND_FILE_DATA: "
                                     "cbChunkRead=%RU32, rc=%Rrc\n", cbChunkRead, rc));
                    }

                    if (   RT_SUCCESS(rc)
                        && uNextMsg == HOST_DND_HG_SND_FILE_HDR)
                    {
                        char *pszPathAbs = RTPathJoinA(pszDropDir, szPathName);
                        if (pszPathAbs)
                        {
                            LogFlowFunc(("Opening pszPathName=%s, cbPathName=%RU32, fMode=0x%x, cbFileSize=%zu\n",
                                         szPathName, cbPathName, fMode, cbFileSize));

                            uint64_t fOpen  =   RTFILE_O_WRITE | RTFILE_O_DENY_WRITE
                                              | RTFILE_O_CREATE_REPLACE;

                            /* Is there already a file open, e.g. in transfer? */
                            if (!objFile.IsOpen())
                            {
                                RTCString strPathAbs(pszPathAbs);
#ifdef RT_OS_WINDOWS
                                uint32_t fCreationMode = (fMode & RTFS_DOS_MASK) | RTFS_DOS_NT_NORMAL;
#else
                                uint32_t fCreationMode = (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR;
#endif
                                rc = objFile.OpenEx(strPathAbs, DnDURIObject::File, DnDURIObject::Target, fOpen, fCreationMode);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = pDroppedFiles->AddFile(strPathAbs.c_str());
                                    if (RT_SUCCESS(rc))
                                    {
                                        cbFileWritten = 0;
                                        objFile.SetSize(cbFileSize);
                                    }
                                }
                            }
                            else
                            {
                                AssertMsgFailed(("ObjType=%RU32\n", objFile.GetType()));
                                rc = VERR_WRONG_ORDER;
                            }

                            RTStrFree(pszPathAbs);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }

                    if (   RT_SUCCESS(rc)
                        && uNextMsg == HOST_DND_HG_SND_FILE_DATA)
                    {
                        uint32_t cbChunkWritten;
                        rc = objFile.Write(pvChunk, cbChunkRead, &cbChunkWritten);
                        if (RT_SUCCESS(rc))
                        {
                            LogFlowFunc(("HOST_DND_HG_SND_FILE_DATA "
                                         "cbChunkRead=%RU32, cbChunkWritten=%RU32, cbFileWritten=%RU64 cbFileSize=%RU64\n",
                                         cbChunkRead, cbChunkWritten, cbFileWritten + cbChunkWritten, cbFileSize));

                            cbFileWritten += cbChunkWritten;

                            Assert(cbChunkRead <= cbToRecvBytes);
                            cbToRecvBytes -= cbChunkRead;
                        }
                    }

                    /* Data transfer complete? Close the file. */
                    bool fClose = objFile.IsComplete();
                    if (fClose)
                    {
                        Assert(cToRecvObjs);
                        cToRecvObjs--;
                    }

                    /* Only since protocol v2 we know the file size upfront. */
                    Assert(cbFileWritten <= cbFileSize);

                    if (fClose)
                    {
                        LogFlowFunc(("Closing file\n"));
                        objFile.Close();
                    }

                    break;
                }
                case HOST_DND_HG_EVT_CANCEL:
                {
                    rc = vbglR3DnDHGRecvCancel(pCtx);
                    if (RT_SUCCESS(rc))
                        rc = VERR_CANCELLED;
                    break;
                }
                default:
                {
                    LogFlowFunc(("Message %RU32 not supported\n", uNextMsg));
                    rc = VERR_NOT_SUPPORTED;
                    break;
                }
            }
        }

        if (RT_FAILURE(rc))
            break;

        LogFlowFunc(("cbToRecvBytes=%RU64, cToRecvObjs=%RU64\n", cbToRecvBytes, cToRecvObjs));
        if (   !cbToRecvBytes
            && !cToRecvObjs)
        {
            break;
        }

    } while (RT_SUCCESS(rc));

    LogFlowFunc(("Loop ended with %Rrc\n", rc));

    /* All URI data processed? */
    if (rc == VERR_NO_DATA)
        rc = VINF_SUCCESS;

    /* Delete temp buffer again. */
    if (pvChunk)
        RTMemFree(pvChunk);

    /* Cleanup on failure or if the user has canceled the operation or
     * something else went wrong. */
    if (RT_FAILURE(rc))
    {
        objFile.Close();
        pDroppedFiles->Rollback();
    }
    else
    {
        /** @todo Compare the URI list with the dirs/files we really transferred. */
        /** @todo Implement checksum verification, if any. */
    }

    /*
     * Close the dropped files directory.
     * Don't try to remove it here, however, as the files are being needed
     * by the client's drag'n drop operation lateron.
     */
    int rc2 = pDroppedFiles->Reset(false /* fRemoveDropDir */);
    if (RT_FAILURE(rc2)) /* Not fatal, don't report back to host. */
        LogFlowFunc(("Closing dropped files directory failed with %Rrc\n", rc2));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive the HOST_DND_HG_SND_DATA message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pDataHdr            DnD data header to use. Need for accounting and stuff.
 * @param   pvData              Where to store the received data from the host.
 * @param   cbData              Size (in bytes) of where to store the received data.
 * @param   pcbDataRecv         Where to store the received amount of data (in bytes).
 */
static int vbglR3DnDHGRecvDataRaw(PVBGLR3GUESTDNDCMDCTX pCtx, PVBOXDNDSNDDATAHDR pDataHdr,
                                  void *pvData, uint32_t cbData, uint32_t *pcbDataRecv)
{
    AssertPtrReturn(pCtx,            VERR_INVALID_POINTER);
    AssertPtrReturn(pDataHdr,        VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,          VERR_INVALID_POINTER);
    AssertReturn(cbData,             VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbDataRecv, VERR_INVALID_POINTER);

    LogFlowFunc(("pvDate=%p, cbData=%RU32\n", pvData, cbData));

    VBOXDNDHGSENDDATAMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_SND_DATA, 5);
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvData.SetPtr(pvData, cbData);
    Msg.u.v3.cbData.SetUInt32(0);
    Msg.u.v3.pvChecksum.SetPtr(NULL, 0);
    Msg.u.v3.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        uint32_t cbDataRecv;
        rc = Msg.u.v3.cbData.GetUInt32(&cbDataRecv);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            /** @todo Use checksum for validating the received data. */
            if (pcbDataRecv)
                *pcbDataRecv = cbDataRecv;
            LogFlowFuncLeaveRC(rc);
            return rc;
        }
    }

    /* failure */
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest
 * Utility function to receive the HOST_DND_HG_SND_DATA_HDR message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pDataHdr            Where to store the receivd DnD data header.
 */
static int vbglR3DnDHGRecvDataHdr(PVBGLR3GUESTDNDCMDCTX pCtx, PVBOXDNDSNDDATAHDR pDataHdr)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    Assert(pCtx->uProtocol >= 3); /* Only for protocol v3 and up. */

    VBOXDNDHGSENDDATAHDRMSG Msg;
    RT_ZERO(Msg);
    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_HG_SND_DATA_HDR, 12);
    Msg.uContext.SetUInt32(0);
    Msg.uFlags.SetUInt32(0);
    Msg.uScreenId.SetUInt32(0);
    Msg.cbTotal.SetUInt64(0);
    Msg.cbMeta.SetUInt32(0);
    Msg.pvMetaFmt.SetPtr(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
    Msg.cbMetaFmt.SetUInt32(0);
    Msg.cObjects.SetUInt64(0);
    Msg.enmCompression.SetUInt32(0);
    Msg.enmChecksumType.SetUInt32(0);
    Msg.pvChecksum.SetPtr(pDataHdr->pvChecksum, pDataHdr->cbChecksum);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /* Msg.uContext not needed here. */
        Msg.uFlags.GetUInt32(&pDataHdr->uFlags);
        Msg.uScreenId.GetUInt32(&pDataHdr->uScreenId);
        Msg.cbTotal.GetUInt64(&pDataHdr->cbTotal);
        Msg.cbMeta.GetUInt32(&pDataHdr->cbMeta);
        Msg.cbMetaFmt.GetUInt32(&pDataHdr->cbMetaFmt);
        Msg.cObjects.GetUInt64(&pDataHdr->cObjects);
        Msg.enmCompression.GetUInt32(&pDataHdr->enmCompression);
        Msg.enmChecksumType.GetUInt32((uint32_t *)&pDataHdr->enmChecksumType);
        Msg.cbChecksum.GetUInt32(&pDataHdr->cbChecksum);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest
 * Helper function for receiving the actual DnD data from the host. Do not call directly.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pDataHdr            Where to store the data header data.
 * @param   ppvData             Returns the received meta data. Needs to be free'd by the caller.
 * @param   pcbData             Where to store the size (in bytes) of the received meta data.
 */
static int vbglR3DnDHGRecvDataLoop(PVBGLR3GUESTDNDCMDCTX pCtx, PVBOXDNDSNDDATAHDR pDataHdr,
                                   void **ppvData, uint64_t *pcbData)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvData,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcbData,  VERR_INVALID_POINTER);

    int rc;
    uint32_t cbDataRecv;

    LogFlowFuncEnter();

    rc = vbglR3DnDHGRecvDataHdr(pCtx, pDataHdr);
    if (RT_FAILURE(rc))
        return rc;

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU32\n", pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects));
    if (pDataHdr->cbMeta)
    {
        uint64_t cbDataTmp = 0;
        void    *pvDataTmp = RTMemAlloc(pDataHdr->cbMeta);
        if (!pvDataTmp)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            uint8_t *pvDataOff = (uint8_t *)pvDataTmp;
            while (cbDataTmp < pDataHdr->cbMeta)
            {
                rc = vbglR3DnDHGRecvDataRaw(pCtx, pDataHdr,
                                            pvDataOff, RT_MIN(pDataHdr->cbMeta - cbDataTmp, pCtx->cbMaxChunkSize),
                                            &cbDataRecv);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("cbDataRecv=%RU32, cbDataTmp=%RU64\n", cbDataRecv, cbDataTmp));
                    Assert(cbDataTmp + cbDataRecv <= pDataHdr->cbMeta);
                    cbDataTmp += cbDataRecv;
                    pvDataOff += cbDataRecv;
                }
                else
                    break;
            }

            if (RT_SUCCESS(rc))
            {
                Assert(cbDataTmp == pDataHdr->cbMeta);

                LogFlowFunc(("Received %RU64 bytes of data\n", cbDataTmp));

                *ppvData = pvDataTmp;
                *pcbData = cbDataTmp;
            }
            else
                RTMemFree(pvDataTmp);
        }
    }
    else
    {
        *ppvData = NULL;
        *pcbData = 0;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest
 * Main function for receiving the actual DnD data from the host, extended version.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pEnmType            Where to store the meta data type. Optional.
 * @param   ppvData             Returns the received meta data. Needs to be free'd by the caller.  Optional.
 * @param   pcbData             Where to store the size (in bytes) of the received meta data. Optional.
 */
static int vbglR3DnDHGRecvDataMainEx(PVBGLR3GUESTDNDCMDCTX        pCtx,
                                     VBGLR3GUESTDNDMETADATATYPE  *pEnmType,
                                     void                       **ppvData,
                                     uint32_t                    *pcbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    /* The rest is optional. */

    VBOXDNDDATAHDR dataHdr;
    RT_ZERO(dataHdr);

    AssertMsg(pCtx->cbMaxChunkSize, ("Maximum chunk size must not be 0\n"));

    dataHdr.cbMetaFmt = pCtx->cbMaxChunkSize;
    dataHdr.pvMetaFmt = RTMemAlloc(dataHdr.cbMetaFmt);
    if (!dataHdr.pvMetaFmt)
        return VERR_NO_MEMORY;

    DnDURIList lstURI;
    DnDDroppedFiles droppedFiles;

    void    *pvData = NULL;
    uint64_t cbData = 0;

    int rc = vbglR3DnDHGRecvDataLoop(pCtx, &dataHdr, &pvData, &cbData);
    if (RT_SUCCESS(rc))
    {
        /**
         * Check if this is an URI event. If so, let VbglR3 do all the actual
         * data transfer + file/directory creation internally without letting
         * the caller know.
         *
         * This keeps the actual (guest OS-)dependent client (like VBoxClient /
         * VBoxTray) small by not having too much redundant code.
         */
        Assert(dataHdr.cbMetaFmt);
        AssertPtr(dataHdr.pvMetaFmt);
        if (DnDMIMEHasFileURLs((char *)dataHdr.pvMetaFmt, dataHdr.cbMetaFmt)) /* URI data. */
        {
            AssertPtr(pvData);
            Assert(cbData);

            rc = lstURI.RootFromURIData(pvData, cbData, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
                rc = vbglR3DnDHGRecvURIData(pCtx, &dataHdr, &droppedFiles);

            if (RT_SUCCESS(rc)) /** @todo Remove this block as soon as we hand in DnDURIList. */
            {
                if (pvData)
                {
                    /* Reuse data buffer to fill in the transformed URI file list. */
                    RTMemFree(pvData);
                    pvData = NULL;
                }

                RTCString strData = lstURI.RootToString(droppedFiles.GetDirAbs());
                Assert(!strData.isEmpty());

                cbData = strData.length() + 1;
                LogFlowFunc(("URI list has %zu bytes\n", cbData));

                pvData = RTMemAlloc(cbData);
                if (pvData)
                {
                    memcpy(pvData, strData.c_str(), cbData);

                    if (pEnmType)
                        *pEnmType = VBGLR3GUESTDNDMETADATATYPE_URI_LIST;
                }
                else
                    rc =  VERR_NO_MEMORY;
            }
        }
        else /* Raw data. */
        {
            if (pEnmType)
                *pEnmType = VBGLR3GUESTDNDMETADATATYPE_RAW;
        }
    }

    if (dataHdr.pvMetaFmt)
        RTMemFree(dataHdr.pvMetaFmt);

    if (RT_SUCCESS(rc))
    {
        if (   pvData
            && cbData)
        {
            if (pcbData)
                *pcbData = cbData;
            if (ppvData)
                *ppvData = pvData;
            else
                RTMemFree(pvData);
        }
    }
    else if (   RT_FAILURE(rc)
             && rc != VERR_CANCELLED)
    {
        if (pvData)
            RTMemFree(pvData);

        int rc2 = VbglR3DnDHGSendProgress(pCtx, DND_PROGRESS_ERROR, 100 /* Percent */, rc);
        if (RT_FAILURE(rc2))
            LogFlowFunc(("Unable to send progress error %Rrc to host: %Rrc\n", rc, rc2));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest
 * Main function for receiving the actual DnD data from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pMeta               Where to store the actual meta data received from the host.
 */
static int vbglR3DnDHGRecvDataMain(PVBGLR3GUESTDNDCMDCTX   pCtx,
                                   PVBGLR3GUESTDNDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    int rc = vbglR3DnDHGRecvDataMainEx(pCtx,
                                       &pMeta->enmType,
                                       &pMeta->pvMeta,
                                       &pMeta->cbMeta);
    return rc;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
/**
 * Guest -> Host
 * Utility function to receive the HOST_DND_GH_REQ_PENDING message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   puScreenID          For which screen on the host the request is for. Optional.
 */
static int vbglR3DnDGHRecvPending(PVBGLR3GUESTDNDCMDCTX pCtx, uint32_t *puScreenID)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
   /* pScreenID is optional. */

    VBOXDNDGHREQPENDINGMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_GH_REQ_PENDING, 2);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.uScreenId.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        if (puScreenID)
            rc = Msg.u.v3.uContext.GetUInt32(puScreenID);
    }

    return rc;
}

/**
 * Guest -> Host
 * Utility function to receive the HOST_DND_GH_EVT_DROPPED message from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   ppszFormat          Requested data format from the host. Optional.
 * @param   pcbFormat           Size of requested data format (in bytes). Optional.
 * @param   puAction            Requested action from the host. Optional.
 */
static int vbglR3DnDGHRecvDropped(PVBGLR3GUESTDNDCMDCTX pCtx,
                                  char     **ppszFormat,
                                  uint32_t  *pcbFormat,
                                  uint32_t  *puAction)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    /* The rest is optional. */

    const uint32_t cbFormatTmp = pCtx->cbMaxChunkSize;

    char *pszFormatTmp = static_cast<char *>(RTMemAlloc(cbFormatTmp));
    if (!pszFormatTmp)
        return VERR_NO_MEMORY;

    VBOXDNDGHDROPPEDMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, HOST_DND_GH_EVT_DROPPED, 4);
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvFormat.SetPtr(pszFormatTmp, cbFormatTmp);
    Msg.u.v3.cbFormat.SetUInt32(0);
    Msg.u.v3.uAction.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
        if (pcbFormat)
            rc = Msg.u.v3.cbFormat.GetUInt32(pcbFormat);
        if (RT_SUCCESS(rc) && puAction)
            rc = Msg.u.v3.uAction.GetUInt32(puAction);

        if (RT_SUCCESS(rc))
        {
            *ppszFormat = RTStrDup(pszFormatTmp);
            if (!*ppszFormat)
                rc = VERR_NO_MEMORY;
        }
    }

    RTMemFree(pszFormatTmp);

    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */


/*********************************************************************************************************************************
*   Public functions                                                                                                             *
*********************************************************************************************************************************/

/**
 * Connects a DnD context to the DnD host service.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to connect.
 */
VBGLR3DECL(int) VbglR3DnDConnect(PVBGLR3GUESTDNDCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    /* Initialize header */
    int rc = VbglR3HGCMConnect("VBoxDragAndDropSvc", &pCtx->uClientID);
    if (RT_FAILURE(rc))
        return rc;

    /* Set the default protocol version to use. */
    pCtx->uProtocol = 3;
    Assert(pCtx->uClientID);

    /*
     * Get the VM's session ID.
     * This is not fatal in case we're running with an ancient VBox version.
     */
    pCtx->uSessionID = 0;
    int rc2 = VbglR3GetSessionId(&pCtx->uSessionID);
    LogFlowFunc(("uSessionID=%RU64, rc=%Rrc\n", pCtx->uSessionID, rc2));

    /*
     * Check if the host is >= VBox 5.0 which in case supports GUEST_DND_CONNECT.
     */
    bool fSupportsConnectReq = false;
    if (RT_SUCCESS(rc))
    {
        /* The guest property service might not be available. Not fatal. */
        uint32_t uGuestPropSvcClientID;
        rc2 = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
        if (RT_SUCCESS(rc2))
        {
            char *pszHostVersion;
            rc2 = VbglR3GuestPropReadValueAlloc(uGuestPropSvcClientID, "/VirtualBox/HostInfo/VBoxVer", &pszHostVersion);
            if (RT_SUCCESS(rc2))
            {
                fSupportsConnectReq = RTStrVersionCompare(pszHostVersion, "5.0") >= 0;
                LogFlowFunc(("pszHostVersion=%s, fSupportsConnectReq=%RTbool\n", pszHostVersion, fSupportsConnectReq));
                VbglR3GuestPropReadValueFree(pszHostVersion);
            }

            VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
        }

        if (RT_FAILURE(rc2))
            LogFlowFunc(("Retrieving host version failed with rc=%Rrc\n", rc2));
    }

    if (fSupportsConnectReq)
    {
        /*
         * Try sending the connect message to tell the protocol version to use.
         * Note: This might fail when the Guest Additions run on an older VBox host (< VBox 5.0) which
         *       does not implement this command.
         */
        VBOXDNDCONNECTMSG Msg;
        RT_ZERO(Msg);

        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_CONNECT, 3);
        /** @todo Context ID not used yet. */
        Msg.u.v3.uContext.SetUInt32(0);
        Msg.u.v3.uProtocol.SetUInt32(pCtx->uProtocol);
        Msg.u.v3.uFlags.SetUInt32(0); /* Unused at the moment. */

        rc2 = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_FAILURE(rc2))
            fSupportsConnectReq = false;

        LogFlowFunc(("Connection request ended with rc=%Rrc\n", rc2));
    }

    if (fSupportsConnectReq)
    {
        pCtx->cbMaxChunkSize = _64K; /** @todo Use a scratch buffer on the heap? */
    }
    else /* GUEST_DND_CONNECT not supported; the user needs to upgrade the host. */
        rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("uClient=%RU32, uProtocol=%RU32, rc=%Rrc\n", pCtx->uClientID, pCtx->uProtocol, rc));
    return rc;
}

/**
 * Disconnects a given DnD context from the DnD host service.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to disconnect.
 *                              The context is invalid afterwards on successful disconnection.
 */
VBGLR3DECL(int) VbglR3DnDDisconnect(PVBGLR3GUESTDNDCMDCTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    int rc = VbglR3HGCMDisconnect(pCtx->uClientID);
    if (RT_SUCCESS(rc))
        pCtx->uClientID = 0;
    return rc;
}

/**
 * Receives the next upcoming DnD event.
 *
 * This is the main function DnD clients call in order to implement any DnD functionality.
 * The purpose of it is to abstract the actual DnD protocol handling as much as possible from
 * the clients -- those only need to react to certain events, regardless of how the underlying
 * protocol actually is working.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to work with.
 * @param   ppEvent             Next DnD event received on success; needs to be free'd by the client calling
 *                              VbglR3DnDEventFree() when done.
 */
VBGLR3DECL(int) VbglR3DnDEventGetNext(PVBGLR3GUESTDNDCMDCTX pCtx, PVBGLR3DNDEVENT *ppEvent)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(ppEvent, VERR_INVALID_POINTER);

    PVBGLR3DNDEVENT pEvent = (PVBGLR3DNDEVENT)RTMemAllocZ(sizeof(VBGLR3DNDEVENT));
    if (!pEvent)
        return VERR_NO_MEMORY;

    uint32_t uMsg   = 0;
    uint32_t cParms = 0;
    int rc = vbglR3DnDGetNextMsgType(pCtx, &uMsg, &cParms, true /* fWait */);
    if (RT_SUCCESS(rc))
    {
        /* Check for VM session change. */
        uint64_t uSessionID;
        int rc2 = VbglR3GetSessionId(&uSessionID);
        if (   RT_SUCCESS(rc2)
            && (uSessionID != pCtx->uSessionID))
        {
            LogFlowFunc(("VM session ID changed to %RU64, doing reconnect\n", uSessionID));

            /* Try a reconnect to the DnD service. */
            rc2 = VbglR3DnDDisconnect(pCtx);
            AssertRC(rc2);
            rc2 = VbglR3DnDConnect(pCtx);
            AssertRC(rc2);

            /* At this point we continue processing the messsages with the new client ID. */
        }
    }

    if (RT_SUCCESS(rc))
    {
        LogFunc(("Handling uMsg=%RU32\n", uMsg));

        switch(uMsg)
        {
            case HOST_DND_HG_EVT_ENTER:
            {
                rc = vbglR3DnDHGRecvAction(pCtx,
                                           uMsg,
                                           &pEvent->u.HG_Enter.uScreenID,
                                           NULL /* puXPos */,
                                           NULL /* puYPos */,
                                           NULL /* uDefAction */,
                                           &pEvent->u.HG_Enter.uAllActions,
                                           &pEvent->u.HG_Enter.pszFormats,
                                           &pEvent->u.HG_Enter.cbFormats);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_ENTER;
                break;
            }
            case HOST_DND_HG_EVT_MOVE:
            {
                rc = vbglR3DnDHGRecvAction(pCtx,
                                           uMsg,
                                           NULL /* puScreenId */,
                                           &pEvent->u.HG_Move.uXpos,
                                           &pEvent->u.HG_Move.uYpos,
                                           &pEvent->u.HG_Move.uDefAction,
                                           NULL /* puAllActions */,
                                           NULL /* pszFormats */,
                                           NULL /* pcbFormats */);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_MOVE;
                break;
            }
            case HOST_DND_HG_EVT_DROPPED:
            {
                rc = vbglR3DnDHGRecvAction(pCtx,
                                           uMsg,
                                           NULL /* puScreenId */,
                                           &pEvent->u.HG_Drop.uXpos,
                                           &pEvent->u.HG_Drop.uYpos,
                                           &pEvent->u.HG_Drop.uDefAction,
                                           NULL /* puAllActions */,
                                           NULL /* pszFormats */,
                                           NULL /* pcbFormats */);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_DROP;
                break;
            }
            case HOST_DND_HG_EVT_LEAVE:
            {
                rc = vbglR3DnDHGRecvLeave(pCtx);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_LEAVE;
                break;
            }
            case HOST_DND_HG_SND_DATA_HDR:
            {
                rc = vbglR3DnDHGRecvDataMain(pCtx, &pEvent->u.HG_Received.Meta);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_RECEIVE;
                break;
            }
            case HOST_DND_HG_SND_DIR:
                RT_FALL_THROUGH();
            case HOST_DND_HG_SND_FILE_DATA:
            {
                /*
                 * All messages in this case are handled internally
                 * by vbglR3DnDHGRecvDataMain() and must be specified
                 * by preceeding HOST_DND_HG_SND_DATA or HOST_DND_HG_SND_DATA_HDR calls.
                 */
                rc = VERR_WRONG_ORDER;
                break;
            }
            case HOST_DND_HG_EVT_CANCEL:
            {
                rc = vbglR3DnDHGRecvCancel(pCtx);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_HG_CANCEL;
                break;
            }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
            case HOST_DND_GH_REQ_PENDING:
            {
                rc = vbglR3DnDGHRecvPending(pCtx, &pEvent->u.GH_IsPending.uScreenID);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_GH_REQ_PENDING;
                break;
            }
            case HOST_DND_GH_EVT_DROPPED:
            {
                rc = vbglR3DnDGHRecvDropped(pCtx,
                                            &pEvent->u.GH_Drop.pszFormat,
                                            &pEvent->u.GH_Drop.cbFormat,
                                            &pEvent->u.GH_Drop.uAction);
                if (RT_SUCCESS(rc))
                    pEvent->enmType = VBGLR3DNDEVENTTYPE_GH_DROP;
                break;
            }
#endif
            default:
            {
                rc = VERR_NOT_SUPPORTED;
                break;
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        VbglR3DnDEventFree(pEvent);
        LogFlowFunc(("Failed with %Rrc\n", rc));
    }
    else
        *ppEvent = pEvent;

    return rc;
}

/**
 * Frees (destroys) a formerly allocated DnD event.
 *
 * @returns IPRT status code.
 * @param   pEvent              Event to free (destroy).
 */
VBGLR3DECL(void) VbglR3DnDEventFree(PVBGLR3DNDEVENT pEvent)
{
    if (!pEvent)
        return;

    /* Some messages require additional cleanup. */
    switch (pEvent->enmType)
    {
        case VBGLR3DNDEVENTTYPE_HG_ENTER:
        {
            if (pEvent->u.HG_Enter.pszFormats)
                RTStrFree(pEvent->u.HG_Enter.pszFormats);
            break;
        }

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case VBGLR3DNDEVENTTYPE_GH_DROP:
        {
            if (pEvent->u.GH_Drop.pszFormat)
                RTStrFree(pEvent->u.GH_Drop.pszFormat);
            break;
        }
#endif
        case VBGLR3DNDEVENTTYPE_HG_RECEIVE:
        {
            PVBGLR3GUESTDNDMETADATA pMeta = &pEvent->u.HG_Received.Meta;
            if (pMeta->pvMeta)
            {
                Assert(pMeta->cbMeta);
                RTMemFree(pMeta->pvMeta);
                pMeta->cbMeta = 0;
            }
            break;
        }

        default:
            break;
    }

    RTMemFree(pEvent);
    pEvent = NULL;
}

VBGLR3DECL(int) VbglR3DnDHGSendAckOp(PVBGLR3GUESTDNDCMDCTX pCtx, uint32_t uAction)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBOXDNDHGACKOPMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_HG_ACK_OP, 2);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.uAction.SetUInt32(uAction);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Host -> Guest
 * Requests the actual DnD data to be sent from the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pcszFormat          Format to request the data from the host in.
 */
VBGLR3DECL(int) VbglR3DnDHGSendReqData(PVBGLR3GUESTDNDCMDCTX pCtx, const char* pcszFormat)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(pcszFormat, VERR_INVALID_POINTER);
    if (!RTStrIsValidEncoding(pcszFormat))
        return VERR_INVALID_PARAMETER;

    const uint32_t cbFormat = (uint32_t)strlen(pcszFormat) + 1; /* Include termination */

    VBOXDNDHGREQDATAMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_HG_REQ_DATA, 3);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvFormat.SetPtr((void*)pcszFormat, cbFormat);
    Msg.u.v3.cbFormat.SetUInt32(cbFormat);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Host -> Guest
 * Reports back its progress back to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   uStatus             DnD status to report.
 * @param   uPercent            Overall progress (in percent) to report.
 * @param   rcErr               Error code (IPRT-style) to report.
 */
VBGLR3DECL(int) VbglR3DnDHGSendProgress(PVBGLR3GUESTDNDCMDCTX pCtx, uint32_t uStatus, uint8_t uPercent, int rcErr)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertReturn(uStatus > DND_PROGRESS_UNKNOWN, VERR_INVALID_PARAMETER);

    VBOXDNDHGEVTPROGRESSMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_HG_EVT_PROGRESS, 4);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.uStatus.SetUInt32(uStatus);
    Msg.u.v3.uPercent.SetUInt32(uPercent);
    Msg.u.v3.rc.SetUInt32((uint32_t)rcErr); /* uint32_t vs. int. */

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
/**
 * Guest -> Host
 * Acknowledges that there currently is a drag'n drop operation in progress on the guest,
 * which eventually could be dragged over to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   uDefAction          Default action for the operation to report.
 * @param   uAllActions         All available actions for the operation to report.
 * @param   pcszFormats         Available formats for the operation to report.
 * @param   cbFormats           Size (in bytes) of formats to report.
 */
VBGLR3DECL(int) VbglR3DnDGHSendAckPending(PVBGLR3GUESTDNDCMDCTX pCtx,
                                          uint32_t uDefAction, uint32_t uAllActions, const char* pcszFormats, uint32_t cbFormats)
{
    AssertPtrReturn(pCtx,        VERR_INVALID_POINTER);
    AssertPtrReturn(pcszFormats, VERR_INVALID_POINTER);
    AssertReturn(cbFormats,      VERR_INVALID_PARAMETER);

    if (!RTStrIsValidEncoding(pcszFormats))
        return VERR_INVALID_UTF8_ENCODING;

    VBOXDNDGHACKPENDINGMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_GH_ACK_PENDING, 5);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.uDefAction.SetUInt32(uDefAction);
    Msg.u.v3.uAllActions.SetUInt32(uAllActions);
    Msg.u.v3.pvFormats.SetPtr((void*)pcszFormats, cbFormats);
    Msg.u.v3.cbFormats.SetUInt32(cbFormats);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Guest -> Host
 * Utility function to send DnD data from guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pvData              Data block to send.
 * @param   cbData              Size (in bytes) of data block to send.
 * @param   pDataHdr            Data header to use -- needed for accounting.
 */
static int vbglR3DnDGHSendDataInternal(PVBGLR3GUESTDNDCMDCTX pCtx,
                                       void *pvData, uint64_t cbData, PVBOXDNDSNDDATAHDR pDataHdr)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,   VERR_INVALID_POINTER);
    AssertReturn(cbData,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    VBOXDNDGHSENDDATAHDRMSG MsgHdr;
    RT_ZERO(MsgHdr);

    VBGL_HGCM_HDR_INIT(&MsgHdr.hdr, pCtx->uClientID, GUEST_DND_GH_SND_DATA_HDR, 12);
    MsgHdr.uContext.SetUInt32(0);                           /** @todo Not used yet. */
    MsgHdr.uFlags.SetUInt32(0);                             /** @todo Not used yet. */
    MsgHdr.uScreenId.SetUInt32(0);                          /** @todo Not used for guest->host (yet). */
    MsgHdr.cbTotal.SetUInt64(pDataHdr->cbTotal);
    MsgHdr.cbMeta.SetUInt32(pDataHdr->cbMeta);
    MsgHdr.pvMetaFmt.SetPtr(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
    MsgHdr.cbMetaFmt.SetUInt32(pDataHdr->cbMetaFmt);
    MsgHdr.cObjects.SetUInt64(pDataHdr->cObjects);
    MsgHdr.enmCompression.SetUInt32(0);                     /** @todo Not used yet. */
    MsgHdr.enmChecksumType.SetUInt32(RTDIGESTTYPE_INVALID); /** @todo Not used yet. */
    MsgHdr.pvChecksum.SetPtr(NULL, 0);                      /** @todo Not used yet. */
    MsgHdr.cbChecksum.SetUInt32(0);                         /** @todo Not used yet. */

    int rc = VbglR3HGCMCall(&MsgHdr.hdr, sizeof(MsgHdr));

    LogFlowFunc(("cbTotal=%RU64, cbMeta=%RU32, cObjects=%RU64, rc=%Rrc\n",
                 pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects, rc));

    if (RT_SUCCESS(rc))
    {
        VBOXDNDGHSENDDATAMSG MsgData;
        RT_ZERO(MsgData);

        VBGL_HGCM_HDR_INIT(&MsgData.hdr, pCtx->uClientID, GUEST_DND_GH_SND_DATA, 5);
        MsgData.u.v3.uContext.SetUInt32(0);      /** @todo Not used yet. */
        MsgData.u.v3.pvChecksum.SetPtr(NULL, 0); /** @todo Not used yet. */
        MsgData.u.v3.cbChecksum.SetUInt32(0);    /** @todo Not used yet. */

        uint32_t       cbCurChunk;
        const uint32_t cbMaxChunk = pCtx->cbMaxChunkSize;
        uint32_t       cbSent     = 0;

        HGCMFunctionParameter *pParm = &MsgData.u.v3.pvData;

        while (cbSent < cbData)
        {
            cbCurChunk = RT_MIN(cbData - cbSent, cbMaxChunk);
            pParm->SetPtr(static_cast<uint8_t *>(pvData) + cbSent, cbCurChunk);

            MsgData.u.v3.cbData.SetUInt32(cbCurChunk);

            rc = VbglR3HGCMCall(&MsgData.hdr, sizeof(MsgData));
            if (RT_FAILURE(rc))
                break;

            cbSent += cbCurChunk;
        }

        LogFlowFunc(("cbMaxChunk=%RU32, cbData=%RU64, cbSent=%RU32, rc=%Rrc\n",
                     cbMaxChunk, cbData, cbSent, rc));

        if (RT_SUCCESS(rc))
            Assert(cbSent == cbData);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Guest -> Host
 * Utility function to send a guest directory to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pObj                URI object containing the directory to send.
 */
static int vbglR3DnDGHSendDir(PVBGLR3GUESTDNDCMDCTX pCtx, DnDURIObject *pObj)
{
    AssertPtrReturn(pObj,                                    VERR_INVALID_POINTER);
    AssertPtrReturn(pCtx,                                    VERR_INVALID_POINTER);
    AssertReturn(pObj->GetType() == DnDURIObject::Directory, VERR_INVALID_PARAMETER);

    RTCString strPath = pObj->GetDestPath();
    LogFlowFunc(("strDir=%s (%zu), fMode=0x%x\n",
                 strPath.c_str(), strPath.length(), pObj->GetMode()));

    if (strPath.length() > RTPATH_MAX)
        return VERR_INVALID_PARAMETER;

    const uint32_t cbPath = (uint32_t)strPath.length() + 1; /* Include termination. */

    VBOXDNDGHSENDDIRMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_GH_SND_DIR, 4);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)cbPath);
    Msg.u.v3.cbName.SetUInt32((uint32_t)cbPath);
    Msg.u.v3.fMode.SetUInt32(pObj->GetMode());

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Guest -> Host
 * Utility function to send a file from the guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pObj                URI object containing the file to send.
 */
static int vbglR3DnDGHSendFile(PVBGLR3GUESTDNDCMDCTX pCtx, DnDURIObject *pObj)
{
    AssertPtrReturn(pCtx,                               VERR_INVALID_POINTER);
    AssertPtrReturn(pObj,                               VERR_INVALID_POINTER);
    AssertReturn(pObj->GetType() == DnDURIObject::File, VERR_INVALID_PARAMETER);
    AssertReturn(pObj->IsOpen(),                        VERR_INVALID_STATE);

    uint32_t cbBuf = _64K;           /** @todo Make this configurable? */
    void *pvBuf = RTMemAlloc(cbBuf); /** @todo Make this buffer part of PVBGLR3GUESTDNDCMDCTX? */
    if (!pvBuf)
        return VERR_NO_MEMORY;

    RTCString strPath = pObj->GetDestPath();

    LogFlowFunc(("strFile=%s (%zu), cbSize=%RU64, fMode=0x%x\n", strPath.c_str(), strPath.length(),
                 pObj->GetSize(), pObj->GetMode()));

    VBOXDNDGHSENDFILEHDRMSG MsgHdr;
    RT_ZERO(MsgHdr);

    VBGL_HGCM_HDR_INIT(&MsgHdr.hdr, pCtx->uClientID, GUEST_DND_GH_SND_FILE_HDR, 6);
    MsgHdr.uContext.SetUInt32(0);                                                    /* Context ID; unused at the moment. */
    MsgHdr.pvName.SetPtr((void *)strPath.c_str(), (uint32_t)(strPath.length() + 1));
    MsgHdr.cbName.SetUInt32((uint32_t)(strPath.length() + 1));
    MsgHdr.uFlags.SetUInt32(0);                                                      /* Flags; unused at the moment. */
    MsgHdr.fMode.SetUInt32(pObj->GetMode());                                         /* File mode */
    MsgHdr.cbTotal.SetUInt64(pObj->GetSize());                                       /* File size (in bytes). */

    int rc = VbglR3HGCMCall(&MsgHdr.hdr, sizeof(MsgHdr));

    LogFlowFunc(("Sending file header resulted in %Rrc\n", rc));

    if (RT_SUCCESS(rc))
    {
        /*
         * Send the actual file data, chunk by chunk.
         */
        VBOXDNDGHSENDFILEDATAMSG Msg;
        RT_ZERO(Msg);

        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_GH_SND_FILE_DATA, 5);
        Msg.u.v3.uContext.SetUInt32(0);
        Msg.u.v3.pvChecksum.SetPtr(NULL, 0);
        Msg.u.v3.cbChecksum.SetUInt32(0);

        uint64_t cbToReadTotal  = pObj->GetSize();
        uint64_t cbWrittenTotal = 0;
        while (cbToReadTotal)
        {
            uint32_t cbToRead = RT_MIN(cbToReadTotal, cbBuf);
            uint32_t cbRead   = 0;
            if (cbToRead)
                rc = pObj->Read(pvBuf, cbToRead, &cbRead);

            LogFlowFunc(("cbToReadTotal=%RU64, cbToRead=%RU32, cbRead=%RU32, rc=%Rrc\n",
                         cbToReadTotal, cbToRead, cbRead, rc));

            if (   RT_SUCCESS(rc)
                && cbRead)
            {
                Msg.u.v3.pvData.SetPtr(pvBuf, cbRead);
                Msg.u.v3.cbData.SetUInt32(cbRead);
                /** @todo Calculate + set checksums. */

                rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
            }

            if (RT_FAILURE(rc))
            {
                LogFlowFunc(("Reading failed with rc=%Rrc\n", rc));
                break;
            }

            Assert(cbRead  <= cbToReadTotal);
            cbToReadTotal  -= cbRead;
            cbWrittenTotal += cbRead;

            LogFlowFunc(("%RU64/%RU64 -- %RU8%%\n", cbWrittenTotal, pObj->GetSize(), cbWrittenTotal * 100 / pObj->GetSize()));
        };
    }

    RTMemFree(pvBuf);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Guest -> Host
 * Utility function to send an URI object from guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pObj                URI object to send from guest to the host.
 */
static int vbglR3DnDGHSendURIObject(PVBGLR3GUESTDNDCMDCTX pCtx, DnDURIObject *pObj)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);

    int rc;

    switch (pObj->GetType())
    {
        case DnDURIObject::Directory:
            rc = vbglR3DnDGHSendDir(pCtx, pObj);
            break;

        case DnDURIObject::File:
            rc = vbglR3DnDGHSendFile(pCtx, pObj);
            break;

        default:
            AssertMsgFailed(("Object type %ld not implemented\n", pObj->GetType()));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    return rc;
}

/**
 * Guest -> Host
 * Utility function to send raw data from guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pvData              Block to raw data to send.
 * @param   cbData              Size (in bytes) of raw data to send.
 */
static int vbglR3DnDGHSendRawData(PVBGLR3GUESTDNDCMDCTX pCtx, void *pvData, size_t cbData)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    /* cbData can be 0. */

    VBOXDNDDATAHDR dataHdr;
    RT_ZERO(dataHdr);

    /* For raw data only the total size is required to be specified. */
    dataHdr.cbTotal = cbData;

    return vbglR3DnDGHSendDataInternal(pCtx, pvData, cbData, &dataHdr);
}

/**
 * Guest -> Host
 * Utility function to send URI data from guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pvData              Block to URI data to send.
 * @param   cbData              Size (in bytes) of URI data to send.
 */
static int vbglR3DnDGHSendURIData(PVBGLR3GUESTDNDCMDCTX pCtx, const void *pvData, size_t cbData)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData,    VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths =
        RTCString((const char *)pvData, cbData).split("\r\n");

    /** @todo Add symlink support (DNDURILIST_FLAGS_RESOLVE_SYMLINKS) here. */
    /** @todo Add lazy loading (DNDURILIST_FLAGS_LAZY) here. */
    uint32_t fFlags = DNDURILIST_FLAGS_KEEP_OPEN;

    DnDURIList lstURI;
    int rc = lstURI.AppendURIPathsFromList(lstPaths, fFlags);
    if (RT_SUCCESS(rc))
    {
        /*
         * Send the (meta) data; in case of URIs it's the (non-recursive) file/directory
         * URI list the host needs to know upfront to set up the drag'n drop operation.
         */
        RTCString strRootDest = lstURI.RootToString();
        if (strRootDest.isNotEmpty())
        {
            void *pvURIList  = (void *)strRootDest.c_str(); /* URI root list. */
            uint32_t cbURLIist = (uint32_t)strRootDest.length() + 1; /* Include string termination. */

            /* The total size also contains the size of the meta data. */
            uint64_t cbTotal  = cbURLIist;
                     cbTotal += lstURI.TotalBytes();

            /* We're going to send an URI list in text format. */
            const char     szMetaFmt[] = "text/uri-list";
            const uint32_t cbMetaFmt   = (uint32_t)strlen(szMetaFmt) + 1; /* Include termination. */

            VBOXDNDDATAHDR dataHdr;
            dataHdr.uFlags    = 0; /* Flags not used yet. */
            dataHdr.cbTotal   = cbTotal;
            dataHdr.cbMeta    = cbURLIist;
            dataHdr.pvMetaFmt = (void *)szMetaFmt;
            dataHdr.cbMetaFmt = cbMetaFmt;
            dataHdr.cObjects  = lstURI.TotalCount();

            rc = vbglR3DnDGHSendDataInternal(pCtx,
                                             pvURIList, cbURLIist, &dataHdr);
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rc))
    {
        while (!lstURI.IsEmpty())
        {
            DnDURIObject *pNextObj = lstURI.First();

            rc = vbglR3DnDGHSendURIObject(pCtx, pNextObj);
            if (RT_FAILURE(rc))
                break;

            lstURI.RemoveFirst();
        }
    }

    return rc;
}

/**
 * Guest -> Host
 * Sends data, which either can be raw or URI data, from guest to the host. This function
 * initiates the actual data transfer from guest to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   pszFormat           In which format the data will be sent.
 * @param   pvData              Data block to send.
 * @param   cbData              Size (in bytes) of data block to send.
 */
VBGLR3DECL(int) VbglR3DnDGHSendData(PVBGLR3GUESTDNDCMDCTX pCtx, const char *pszFormat, void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx,      VERR_INVALID_POINTER);
    AssertPtrReturn(pszFormat, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,    VERR_INVALID_POINTER);
    AssertReturn(cbData,       VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszFormat=%s, pvData=%p, cbData=%RU32\n", pszFormat, pvData, cbData));

    int rc;
    if (DnDMIMEHasFileURLs(pszFormat, strlen(pszFormat)))
    {
        /* Send file data. */
        rc = vbglR3DnDGHSendURIData(pCtx, pvData, cbData);
    }
    else
        rc = vbglR3DnDGHSendRawData(pCtx, pvData, cbData);

    if (RT_FAILURE(rc))
    {
        int rc2 = VbglR3DnDGHSendError(pCtx, rc);
        if (RT_FAILURE(rc2))
            LogFlowFunc(("Unable to send error (%Rrc) to host, rc=%Rrc\n", rc, rc2));
    }

    return rc;
}

/**
 * Guest -> Host
 * Send an error back to the host.
 *
 * @returns IPRT status code.
 * @param   pCtx                DnD context to use.
 * @param   rcErr               Error (IPRT-style) to send.
 */
VBGLR3DECL(int) VbglR3DnDGHSendError(PVBGLR3GUESTDNDCMDCTX pCtx, int rcErr)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBOXDNDGHEVTERRORMSG Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, GUEST_DND_GH_EVT_ERROR, 2);
    /** @todo Context ID not used yet. */
    Msg.u.v3.uContext.SetUInt32(0);
    Msg.u.v3.rc.SetUInt32((uint32_t)rcErr); /* uint32_t vs. int. */

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    /*
     * Never return an error if the host did not accept the error at the current
     * time.  This can be due to the host not having any appropriate callbacks
     * set which would handle that error.
     *
     * bird: Looks like VERR_NOT_SUPPORTED is what the host will return if it
     *       doesn't an appropriate callback.  The code used to ignore ALL errors
     *       the host would return, also relevant ones.
     */
    if (RT_FAILURE(rc))
        LogFlowFunc(("Sending error %Rrc failed with rc=%Rrc\n", rcErr, rc));
    if (rc == VERR_NOT_SUPPORTED)
        rc = VINF_SUCCESS;

    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

