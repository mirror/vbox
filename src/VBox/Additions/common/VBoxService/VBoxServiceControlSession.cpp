/* $Id$ */
/** @file
 * VBoxServiceControlSession - Guest session handling. Also handles
 *                             the forked session processes.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>

#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServiceControl.h"

using namespace guestControl;

/** List of guest control sessions (VBOXSERVICECTRLSESSION). */
extern RTLISTANCHOR         g_lstControlSessions;
extern int                  VBoxServiceLogCreate(const char *pszLogFile);
extern void                 VBoxServiceLogDestroy(void);

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  gstcntlSessionFileDestroy(PVBOXSERVICECTRLFILE pFile);
static int                  gstcntlSessionForkShutdown(uint32_t uClientId, uint32_t cParms);
static PVBOXSERVICECTRLFILE gstcntlSessionGetFile(uint32_t uHandle);
static int                  gstcntlSessionHandleFileOpen(uint32_t uClientId, uint32_t cParms);
static int                  gstcntlSessionHandleFileClose(uint32_t uClientId, uint32_t cParms);
static int                  gstcntlSessionHandleFileRead(uint32_t uClientId, uint32_t cParms);
static int                  gstcntlSessionHandleFileWrite(uint32_t uClientId, uint32_t cParms, void *pvScratchBuf, size_t cbScratchBuf);
static int                  gstcntlSessionHandleFileSeek(uint32_t uClientId, uint32_t cParms);
static int                  gstcntlSessionHandleFileTell(uint32_t uClientId, uint32_t cParms);

/** The session ID of the forked session process. */
static uint32_t             g_uSessionID = UINT32_MAX;
/** The session HGCM protocol of the forked session process. */
static uint32_t             g_uSessionProto = 1; /* Use the legacy protocol by default (< VBox 4.3). */
/** List of guest control files (VBOXSERVICECTRLFILE). */
static RTLISTANCHOR         g_lstSessionFiles;

/** Generic option indices for session fork arguments. */
enum
{
    VBOXSERVICESESSIONOPT_LOG_FILE = 1000,
    VBOXSERVICESESSIONOPT_USERNAME,
    VBOXSERVICESESSIONOPT_SESSION_ID,
    VBOXSERVICESESSIONOPT_SESSION_PROTO
};


static int gstcntlSessionFileDestroy(PVBOXSERVICECTRLFILE pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);

    int rc = RTFileClose(pFile->hFile);
    if (RT_SUCCESS(rc))
    {
        /* Remove file entry in any case. */
        RTListNodeRemove(&pFile->Node);
        /* Destroy this object. */
        RTMemFree(pFile);
    }

    return rc;
}


static PVBOXSERVICECTRLFILE gstcntlSessionGetFile(uint32_t uHandle)
{
    PVBOXSERVICECTRLFILE pFileCur = NULL;
    /** @todo Use a map later! */
    RTListForEach(&g_lstSessionFiles, pFileCur, VBOXSERVICECTRLFILE, Node)
    {
        if (pFileCur->uHandle == uHandle)
            return pFileCur;
    }

    return NULL;
}


static int gstcntlSessionHandleFileOpen(uint32_t uClientId, uint32_t cParms)
{
    char szFile[RTPATH_MAX];
    char szOpenMode[64];
    char szDisposition[64];
    uint32_t uCreationMode = 0;
    uint64_t uOffset = 0;

    uint32_t uHandle = 0;
    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetOpen(&ctx,
                                        /* File to open. */
                                        szFile, sizeof(szFile),
                                        /* Open mode. */
                                        szOpenMode, sizeof(szOpenMode),
                                        /* Disposition. */
                                        szDisposition, sizeof(szDisposition),
                                        /* Creation mode. */
                                        &uCreationMode,
                                        /* Offset. */
                                        &uOffset);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = (PVBOXSERVICECTRLFILE)RTMemAlloc(sizeof(VBOXSERVICECTRLFILE));
        if (pFile)
        {
            if (!RTStrPrintf(pFile->szName, sizeof(pFile->szName), "%s", szFile))
                rc = VERR_BUFFER_OVERFLOW;

            if (RT_SUCCESS(rc))
            {
                uint64_t fFlags = RTFILE_O_OPEN_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE; /** @todo Modes! */
                rc = RTFileOpen(&pFile->hFile, pFile->szName, fFlags);
                if (   RT_SUCCESS(rc)
                    && uOffset)
                {
                    /* Seeking is optional. */
                    int rc2 = RTFileSeek(pFile->hFile, (int64_t)uOffset, RTFILE_SEEK_BEGIN, NULL /* Current offset */);
                    if (RT_FAILURE(rc2))
                        VBoxServiceVerbose(3, "[File %s]: Seeking to offset %RU64 failed; rc=%Rrc\n",
                                           pFile->szName, uOffset, rc);
                }
                else
                    VBoxServiceVerbose(3, "[File %s]: Opening failed; rc=%Rrc\n",
                                       pFile->szName, rc);
            }

            if (RT_SUCCESS(rc))
            {
                uHandle = VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(ctx.uContextID);
                pFile->uHandle = uHandle;
                /* rc = */ RTListAppend(&g_lstSessionFiles, &pFile->Node);

                VBoxServiceVerbose(3, "[File %s]: Opened (ID=%RU32)\n",
                                   pFile->szName, pFile->uHandle);
            }

            if (RT_FAILURE(rc))
                RTMemFree(pFile);
        }
        else
            rc = VERR_NO_MEMORY;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_OPEN cplOpen = { rc, uHandle };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_OPEN,
                                            &cplOpen, sizeof(cplOpen));
        if (RT_FAILURE(rc2))
            VBoxServiceError("[File %s]: Failed to report file open status, rc=%Rrc\n",
                             szFile, rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


static int gstcntlSessionHandleFileClose(uint32_t uClientId, uint32_t cParms)
{
    uint32_t uHandle;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetClose(&ctx, &uHandle /* File handle to close */);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            rc = gstcntlSessionFileDestroy(pFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_CLOSE cplClose = { rc };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_CLOSE,
                                            &cplClose, sizeof(cplClose));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file close status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileRead(uint32_t uClientId, uint32_t cParms,
                                        void *pvScratchBuf, size_t cbScratchBuf)
{
    uint32_t uHandle;
    uint32_t cbToRead;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetRead(&ctx, &uHandle, &cbToRead);
    if (RT_SUCCESS(rc))
    {
        void *pvDataRead = pvScratchBuf;
        size_t cbRead = 0;

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            if (cbToRead)
            {
                if (cbToRead > cbScratchBuf)
                {
                    pvDataRead = RTMemAlloc(cbToRead);
                    if (!pvDataRead)
                        rc = VERR_NO_MEMORY;
                }

                if (RT_LIKELY(RT_SUCCESS(rc)))
                    rc = RTFileRead(pFile->hFile, pvDataRead, cbToRead, &cbRead);
            }
            else
                rc = VERR_BUFFER_UNDERFLOW;
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_READ cplRead = { rc, cbRead, pvDataRead };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_READ,
                                            &cplRead, sizeof(cplRead));
        if (   cbToRead > cbScratchBuf
            && pvDataRead)
            RTMemFree(pvDataRead);

        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file read status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileReadAt(uint32_t uClientId, uint32_t cParms,
                                          void *pvScratchBuf, size_t cbScratchBuf)
{
    uint32_t uHandle;
    uint32_t cbToRead; int64_t iOffset;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetReadAt(&ctx, &uHandle, &cbToRead, (uint64_t*)&iOffset);
    if (RT_SUCCESS(rc))
    {
        void *pvDataRead = pvScratchBuf;
        size_t cbRead = 0;

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            if (cbToRead)
            {
                if (cbToRead > cbScratchBuf)
                {
                    pvDataRead = RTMemAlloc(cbToRead);
                    if (!pvDataRead)
                        rc = VERR_NO_MEMORY;
                }

                if (RT_LIKELY(RT_SUCCESS(rc)))
                    rc = RTFileReadAt(pFile->hFile, iOffset, pvDataRead, cbToRead, &cbRead);
            }
            else
                rc = VERR_BUFFER_UNDERFLOW;
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_READ cplRead = { rc, cbRead, pvDataRead };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_READ,
                                            &cplRead, sizeof(cplRead));
        if (   cbToRead > cbScratchBuf
            && pvDataRead)
            RTMemFree(pvDataRead);

        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file read status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileWrite(uint32_t uClientId, uint32_t cParms,
                                         void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(cbScratchBuf, VERR_INVALID_PARAMETER);

    uint32_t uHandle;
    uint32_t cbToWrite;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetWrite(&ctx, &uHandle,
                                         pvScratchBuf, cbScratchBuf,
                                         &cbToWrite);
    if (RT_SUCCESS(rc))
    {
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            rc = RTFileWrite(pFile->hFile, pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_WRITE cplWrite = { rc, (uint32_t)cbWritten };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_WRITE,
                                            &cplWrite, sizeof(cplWrite));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileWriteAt(uint32_t uClientId, uint32_t cParms,
                                           void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(cbScratchBuf, VERR_INVALID_PARAMETER);

    uint32_t uHandle;
    uint32_t cbToWrite; int64_t iOffset;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetWriteAt(&ctx, &uHandle,
                                           pvScratchBuf, cbScratchBuf,
                                           &cbToWrite, (uint64_t*)&iOffset);
    if (RT_SUCCESS(rc))
    {
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            rc = RTFileWriteAt(pFile->hFile, iOffset,
                               pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_WRITE cplWrite = { rc, (uint32_t)cbWritten };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_WRITE,
                                            &cplWrite, sizeof(cplWrite));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileSeek(uint32_t uClientId, uint32_t cParms)
{
    uint32_t uHandle;
    uint32_t uSeekMethod;
    uint64_t uOffset; /* Will be converted to int64_t. */

    uint64_t uOffsetActual = 0;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetSeek(&ctx, &uHandle,
                                        &uSeekMethod, &uOffset);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            unsigned uSeekMethodIPRT;
            switch (uSeekMethod)
            {
                case GUEST_FILE_SEEKTYPE_BEGIN:
                    uSeekMethodIPRT = RTFILE_SEEK_BEGIN;
                    break;

                case GUEST_FILE_SEEKTYPE_CURRENT:
                    uSeekMethodIPRT = RTFILE_SEEK_CURRENT;
                    break;

                case GUEST_FILE_SEEKTYPE_END:
                    uSeekMethodIPRT = RTFILE_SEEK_END;
                    break;

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }

            if (RT_SUCCESS(rc))
                rc = RTFileSeek(pFile->hFile, (int64_t)uOffset,
                                uSeekMethodIPRT, &uOffsetActual);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_SEEK cplSeek = { rc, uOffsetActual };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_SEEK,
                                            &cplSeek, sizeof(cplSeek));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file seek status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileTell(uint32_t uClientId, uint32_t cParms)
{
    uint32_t uHandle;
    uint64_t uOffsetActual = 0;

    VBGLR3GUESTCTRLHOSTCTX ctx = { uClientId, cParms };
    int rc = VbglR3GuestCtrlFileGetTell(&ctx, &uHandle);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(uHandle);
        if (pFile)
        {
            uOffsetActual = RTFileTell(pFile->hFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_TELL cplTell = { rc, uOffsetActual };
        int rc2 = VbglR3GuestCtrlFileNotify(uClientId, ctx.uContextID, GUEST_FILE_NOTIFYTYPE_TELL,
                                            &cplTell, sizeof(cplTell));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file tell status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


RTEXITCODE gstcntlSessionWorker(void)
{
    bool fSessionFilter = true;

    uint32_t uClientID;
    int rc = VbglR3GuestCtrlConnect(&uClientID);
    if (RT_SUCCESS(rc))
    {
        /* Set session filter. */
        uint32_t uFilterAdd = VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(g_uSessionID);

        rc = VbglR3GuestCtrlMsgSetFilter(uClientID, uFilterAdd, 0 /* Filter remove */);
        if (   RT_FAILURE(rc)
            && rc == VERR_NOT_SUPPORTED)
        {
            /* No session filter available. Skip. */
            fSessionFilter = false;

            rc = VINF_SUCCESS;
        }
    }

    /* Allocate a scratch buffer for commands which also send
     * payload data with them. */
    uint32_t cbScratchBuf = _64K; /** @todo Make buffer size configurable via guest properties/argv! */
    AssertReturn(RT_IS_POWER_OF_TWO(cbScratchBuf), RTEXITCODE_FAILURE);
    uint8_t *pvScratchBuf = NULL;

    if (RT_SUCCESS(rc))
    {
        RTListInit(&g_lstSessionFiles);
        pvScratchBuf = (uint8_t*)RTMemAlloc(cbScratchBuf);
        if (!pvScratchBuf)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        bool fShutdown = false;

        for (;;)
        {
            VBoxServiceVerbose(3, "Waiting for host msg ...\n");
            uint32_t uMsg = 0;
            uint32_t cParms = 0;
            rc = VbglR3GuestCtrlMsgWaitFor(uClientID, &uMsg, &cParms);
            if (rc == VERR_TOO_MUCH_DATA)
            {
                VBoxServiceVerbose(4, "Message requires %RU32 parameters, but only 2 supplied -- retrying request (no error!)...\n", cParms);
                rc = VINF_SUCCESS; /* Try to get "real" message in next block below. */
            }
            else if (RT_FAILURE(rc))
                VBoxServiceVerbose(3, "Getting host message failed with %Rrc\n", rc); /* VERR_GEN_IO_FAILURE seems to be normal if ran into timeout. */
            if (RT_SUCCESS(rc))
            {
                VBoxServiceVerbose(3, "Msg=%RU32 (%RU32 parms) retrieved\n", uMsg, cParms);
            }

            VBoxServiceVerbose(3, "Msg=%RU32 (%RU32 parms) retrieved\n", uMsg, cParms);

            /** @todo Guest session ID change detection? */

            switch (uMsg)
            {
                case HOST_CANCEL_PENDING_WAITS:
                    /* Fall thru is intentional. */
                case HOST_SESSION_CLOSE:
                    /* Shutdown this fork. */
                    rc = gstcntlSessionForkShutdown(uClientID, cParms);
                    fShutdown = true; /* Shutdown in any case. */
                    break;

                case HOST_FILE_OPEN:
                    rc = gstcntlSessionHandleFileOpen(uClientID, cParms);
                    break;

                case HOST_FILE_CLOSE:
                    rc = gstcntlSessionHandleFileClose(uClientID, cParms);
                    break;

                case HOST_FILE_READ:
                    rc = gstcntlSessionHandleFileRead(uClientID, cParms,
                                                      pvScratchBuf, cbScratchBuf);
                    break;

                case HOST_FILE_READ_AT:
                    rc = gstcntlSessionHandleFileReadAt(uClientID, cParms,
                                                        pvScratchBuf, cbScratchBuf);
                    break;

                case HOST_FILE_WRITE:
                    rc = gstcntlSessionHandleFileWrite(uClientID, cParms,
                                                       pvScratchBuf, cbScratchBuf);
                    break;

                case HOST_FILE_WRITE_AT:
                    rc = gstcntlSessionHandleFileWriteAt(uClientID, cParms,
                                                         pvScratchBuf, cbScratchBuf);
                    break;

                case HOST_FILE_SEEK:
                    rc = gstcntlSessionHandleFileSeek(uClientID, cParms);
                    break;

                case HOST_FILE_TELL:
                    rc = gstcntlSessionHandleFileTell(uClientID, cParms);
                    break;

                default:
                    VBoxServiceVerbose(3, "Unsupported message from host, uMsg=%RU32, cParms=%RU32\n",
                                       uMsg, cParms);
                    /* Don't terminate here; just wait for the next message. */
                    break;
            }

            if (fShutdown)
                break;
        }
    }

    VBoxServiceVerbose(0, "Session %RU32 ended\n", g_uSessionID);

    if (pvScratchBuf)
        RTMemFree(pvScratchBuf);

    VBoxServiceVerbose(3, "Disconnecting client ID=%RU32 ...\n", uClientID);
    VbglR3GuestCtrlDisconnect(uClientID);

    VBoxServiceVerbose(3, "Session worker returned with rc=%Rrc\n", rc);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Creates a guest session. This will spawn a new VBoxService.exe instance under
 * behalf of the given user which then will act as a session host.
 *
 * @return  IPRT status code.
 * @param   pSessionStartupInfo     Session startup info.
 * @param   pNode                   Returns newly created session node on success.
 *                                  Optional.
 */
int GstCntlSessionOpen(const PVBOXSERVICECTRLSESSIONSTARTUPINFO pSessionStartupInfo,
                       PRTLISTNODE pNode)
{
    AssertPtrReturn(pSessionStartupInfo, VERR_INVALID_POINTER);
    /* pNode is optional. */

#ifdef DEBUG
    PVBOXSERVICECTRLSESSION pSessionCur;
    /* Check for existing session in debug mode. Should never happen because of
     * Main consistency. */
    RTListForEach(&g_lstControlSessions, pSessionCur, VBOXSERVICECTRLSESSION, Node)
    {
        if (pSessionCur->StartupInfo.uSessionID == pSessionStartupInfo->uSessionID)
        {
            AssertMsgFailed(("Guest session %RU32 (%p) already exists when it should not\n",
                             pSessionCur->StartupInfo.uSessionID, pSessionCur));
            return VERR_ALREADY_EXISTS;
        }
    }
#endif
    int rc = VINF_SUCCESS;

    PVBOXSERVICECTRLSESSION pSession = (PVBOXSERVICECTRLSESSION)RTMemAllocZ(sizeof(VBOXSERVICECTRLSESSION));
    if (pSession)
    {
        /* Copy over session startup info. */
        memcpy(&pSession->StartupInfo, pSessionStartupInfo, sizeof(VBOXSERVICECTRLSESSIONSTARTUPINFO));

        VBoxServiceVerbose(3, "Forking new guest session szUser=%s, szPassword=%s, szDomain=%s, uFlags=%x, using protocol %RU32\n",
                           pSessionStartupInfo->szUser,
#ifdef DEBUG
                           pSessionStartupInfo->szPassword,
#else
                           "XXX", /* Never show passwords in release mode. */
#endif
                           pSessionStartupInfo->szDomain,
                           pSessionStartupInfo->uFlags,
                           pSessionStartupInfo->uProtocol);

        rc = RTCritSectInit(&pSession->CritSect);
        AssertRC(rc);

        /* Fork child doing the actual session handling. */
        char szExeName[RTPATH_MAX];
        char *pszExeName = RTProcGetExecutablePath(szExeName, sizeof(szExeName));
        if (pszExeName)
        {
            char szParmUserName[GUESTPROCESS_MAX_USER_LEN + 32];
            if (!RTStrPrintf(szParmUserName, sizeof(szParmUserName), "--username=%s", pSession->StartupInfo.szUser))
                rc = VERR_BUFFER_OVERFLOW;
            char szParmSessionID[32];
            if (RT_SUCCESS(rc) && !RTStrPrintf(szParmSessionID, sizeof(szParmSessionID), "--session-id=%RU32",
                                               pSession->StartupInfo.uSessionID))
            {
                rc = VERR_BUFFER_OVERFLOW;
            }
            char szParmSessionProto[32];
            if (RT_SUCCESS(rc) && !RTStrPrintf(szParmSessionProto, sizeof(szParmSessionProto), "--session-proto=%RU32",
                                               pSession->StartupInfo.uProtocol))
            {
                rc = VERR_BUFFER_OVERFLOW;
            }

            if (RT_SUCCESS(rc))
            {
                int iOptIdx = 0; /* Current index in argument vector. */

                char const *papszArgs[8];
                papszArgs[iOptIdx++] = pszExeName;
                papszArgs[iOptIdx++] = "guestsession";
                papszArgs[iOptIdx++] = szParmSessionID;
                papszArgs[iOptIdx++] = szParmSessionProto;
                papszArgs[iOptIdx++] = szParmUserName;

                /* Add same verbose flags as parent process. */
                int rc2 = VINF_SUCCESS;
                char szParmVerbose[32] = { 0 };
                for (int i = 0; i < g_cVerbosity && RT_SUCCESS(rc2); i++)
                {
                    if (i == 0)
                        rc2 = RTStrCat(szParmVerbose, sizeof(szParmVerbose), "-");
                    if (RT_FAILURE(rc2))
                        break;
                    rc2 = RTStrCat(szParmVerbose, sizeof(szParmVerbose), "v");
                }
                if (RT_SUCCESS(rc2))
                    papszArgs[iOptIdx++] = szParmVerbose;

                /* Add log file handling. Each session will have an own
                 * log file, naming based on the parent log file. */
                char szParmLogFile[RTPATH_MAX];
                if (   RT_SUCCESS(rc2)
                    && strlen(g_szLogFile))
                {
                    char *pszLogFile = RTStrDup(g_szLogFile);
                    if (pszLogFile)
                    {
                        char *pszLogExt = NULL;
                        if (RTPathHasExt(pszLogFile))
                            pszLogExt = RTStrDup(RTPathExt(pszLogFile));
                        RTPathStripExt(pszLogFile);
                        char *pszLogSuffix;
                        if (RTStrAPrintf(&pszLogSuffix, "-%RU32-%s",
                                         pSessionStartupInfo->uSessionID,
                                         pSessionStartupInfo->szUser) < 0)
                        {
                            rc2 = VERR_NO_MEMORY;
                        }
                        else
                        {
                            rc2 = RTStrAAppend(&pszLogFile, pszLogSuffix);
                            if (RT_SUCCESS(rc2) && pszLogExt)
                                rc2 = RTStrAAppend(&pszLogFile, pszLogExt);
                            if (RT_SUCCESS(rc2))
                            {
                                if (!RTStrPrintf(szParmLogFile, sizeof(szParmLogFile),
                                                 "--logfile %s", pszLogFile))
                                {
                                    rc2 = VERR_BUFFER_OVERFLOW;
                                }
                            }
                            RTStrFree(pszLogSuffix);
                        }
                        if (RT_FAILURE(rc2))
                            VBoxServiceError("Error building session logfile string for session %RU32 (user %s), rc=%Rrc\n",
                                             pSessionStartupInfo->uSessionID, pSessionStartupInfo->szUser, rc2);
                        if (pszLogExt)
                            RTStrFree(pszLogExt);
                        RTStrFree(pszLogFile);
                    }
                    if (RT_SUCCESS(rc2))
                        papszArgs[iOptIdx++] = szParmLogFile;
                    papszArgs[iOptIdx++] = NULL;
                }
                else
                    papszArgs[iOptIdx++] = NULL;

                if (g_cVerbosity > 3)
                {
                    VBoxServiceVerbose(4, "Forking parameters:\n");

                    iOptIdx = 0;
                    while (papszArgs[iOptIdx])
                        VBoxServiceVerbose(4, "\t%s\n", papszArgs[iOptIdx++]);
                }

                uint32_t uProcFlags = RTPROC_FLAGS_SERVICE
                                    | RTPROC_FLAGS_HIDDEN; /** @todo More flags from startup info? */

#if 0 /* Pipe handling not needed (yet). */
                /* Setup pipes. */
                rc = GstcntlProcessSetupPipe("|", 0 /*STDIN_FILENO*/,
                                             &pSession->StdIn.hChild, &pSession->StdIn.phChild, &pSession->hStdInW);
                if (RT_SUCCESS(rc))
                {
                    rc = GstcntlProcessSetupPipe("|", 1 /*STDOUT_FILENO*/,
                                                 &pSession->StdOut.hChild, &pSession->StdOut.phChild, &pSession->hStdOutR);
                    if (RT_SUCCESS(rc))
                    {
                        rc = GstcntlProcessSetupPipe("|", 2 /*STDERR_FILENO*/,
                                                     &pSession->StdErr.hChild, &pSession->StdErr.phChild, &pSession->hStdErrR);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTPollSetCreate(&pSession->hPollSet);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(pSession->hPollSet, pSession->hStdInW, RTPOLL_EVT_ERROR,
                                                      VBOXSERVICECTRLPIPEID_STDIN);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(pSession->hPollSet, pSession->hStdOutR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR,
                                                      VBOXSERVICECTRLPIPEID_STDOUT);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(pSession->hPollSet, pSession->hStdErrR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR,
                                                      VBOXSERVICECTRLPIPEID_STDERR);
                        }

                        if (RT_SUCCESS(rc))
                        {
                            /* Fork the thing. */
                            /** @todo Do we need a custom environment block? */
                            rc = RTProcCreateEx(pszExeName, papszArgs, RTENV_DEFAULT, uProcFlags,
                                                pSession->StdIn.phChild, pSession->StdOut.phChild, pSession->StdErr.phChild,
                                                pSession->StartupInfo.szUser,
                                                pSession->StartupInfo.szPassword,
                                                &pSession->hProcess);
                        }

                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Close the child ends of any pipes and redirected files.
                             */
                            int rc2 = RTHandleClose(pSession->StdIn.phChild); AssertRC(rc2);
                            pSession->StdIn.phChild     = NULL;
                            rc2 = RTHandleClose(pSession->StdOut.phChild);    AssertRC(rc2);
                            pSession->StdOut.phChild    = NULL;
                            rc2 = RTHandleClose(pSession->StdErr.phChild);    AssertRC(rc2);
                            pSession->StdErr.phChild    = NULL;
                        }
                    }
                }
#else
                /** @todo Do we need a custom environment block? */
                rc = RTProcCreateEx(pszExeName, papszArgs, RTENV_DEFAULT, uProcFlags,
                                    NULL /* StdIn */, NULL /* StdOut */, NULL /* StdErr */,
                                    pSession->StartupInfo.szUser,
                                    pSession->StartupInfo.szPassword,
                                    &pSession->hProcess);
#endif
            }
        }
        else
            rc = VERR_FILE_NOT_FOUND;

        if (RT_SUCCESS(rc))
        {
            /* Add session to list. */
            /* rc = */ RTListAppend(&g_lstControlSessions, &pSession->Node);
            if (pNode) /* Return node if wanted. */
                pNode = &pSession->Node;
        }
        else
        {
            RTMemFree(pSession);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    VBoxServiceVerbose(3, "Forking returned returned rc=%Rrc\n", rc);
    return rc;
}


/**
 * Closes a formerly opened guest session.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session to close.
 * @param   uFlags                  Termination flags.
 */
int GstCntlSessionClose(PVBOXSERVICECTRLSESSION pSession, uint32_t uFlags)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    /** @todo Implement session termination. */
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Close all formerly opened guest sessions.
 *
 * @return  IPRT status code.
 * @param   uFlags                  Termination flags.
 */
int GstCntlSessionCloseAll(uint32_t uFlags)
{
    int rc = VINF_SUCCESS;

    VBoxServiceVerbose(3, "GstCntlSessionCloseAll\n");

    PVBOXSERVICECTRLSESSION pSessionCur;
    RTListForEach(&g_lstControlSessions, pSessionCur, VBOXSERVICECTRLSESSION, Node)
    {
        int rc2 = GstCntlSessionClose(pSessionCur, uFlags);
        if (RT_SUCCESS(rc))
        {
            rc = rc2;
            /* Keep going. */
        }
    }

    return rc;
}


RTEXITCODE VBoxServiceControlSessionForkInit(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--logfile",         VBOXSERVICESESSIONOPT_LOG_FILE,        RTGETOPT_REQ_STRING },
        { "--username",        VBOXSERVICESESSIONOPT_USERNAME,        RTGETOPT_REQ_STRING },
        { "--session-id",      VBOXSERVICESESSIONOPT_SESSION_ID,      RTGETOPT_REQ_UINT32 },
        { "--session-proto",   VBOXSERVICESESSIONOPT_SESSION_PROTO,   RTGETOPT_REQ_UINT32 },
        { "--verbose",         'v',                                   RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int rc = VINF_SUCCESS;

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case VBOXSERVICESESSIONOPT_LOG_FILE:
                if (!RTStrPrintf(g_szLogFile, sizeof(g_szLogFile), "%s", ValueUnion.psz))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unable to set logfile name to '%s'",
                                          ValueUnion.psz);
                break;

            case VBOXSERVICESESSIONOPT_USERNAME:
                /** @todo. */
                break;

            case VBOXSERVICESESSIONOPT_SESSION_ID:
                g_uSessionID = ValueUnion.u32;
                break;

            case VBOXSERVICESESSIONOPT_SESSION_PROTO:
                g_uSessionProto = ValueUnion.u32;
                break;

            /** @todo Implement help? */

            case 'v':
                g_cVerbosity++;
                break;

            case VINF_GETOPT_NOT_OPTION:
                /* Ignore; might be "guestsession" main command. */
                break;

            default:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Unknown command '%s'", ValueUnion.psz);
                break; /* Never reached. */
        }
    }

    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Initialization failed with rc=%Rrc", rc);

    if (g_uSessionID == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No session ID specified");

    rc = VBoxServiceLogCreate(strlen(g_szLogFile) ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create release log (%s, %Rrc)",
                              strlen(g_szLogFile) ? g_szLogFile : "<None>", rc);

    RTEXITCODE rcExit = gstcntlSessionWorker();

    VBoxServiceLogDestroy();
    return rcExit;
}

static int gstcntlSessionForkShutdown(uint32_t uClientId, uint32_t cParms)
{
    VBoxServiceVerbose(0, "Session %RU32 is going to shutdown ...\n", g_uSessionID);

    /* Close all left guest files. */
    PVBOXSERVICECTRLFILE pFile;
    pFile = RTListGetFirst(&g_lstSessionFiles, VBOXSERVICECTRLFILE, Node);
    while (pFile)
    {
        PVBOXSERVICECTRLFILE pNext = RTListNodeGetNext(&pFile->Node, VBOXSERVICECTRLFILE, Node);
        bool fLast = RTListNodeIsLast(&g_lstSessionFiles, &pFile->Node);

        int rc2 = gstcntlSessionFileDestroy(pFile);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("Unable to close file \"%s\"; rc=%Rrc\n",
                             pFile->szName, rc2);
            /* Keep going. */
        }

        if (fLast)
            break;

        pFile = pNext;
    }

    AssertMsg(RTListIsEmpty(&g_lstSessionFiles),
              ("Guest file list still contains entries when it should not\n"));

    /** @todo Add guest process termination here. Later. */

    return VINF_SUCCESS; /** @todo */
}

