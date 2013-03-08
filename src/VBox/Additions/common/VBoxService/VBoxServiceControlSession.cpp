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
#include <iprt/asm.h>
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

/*******************************************************************************
*   Externals                                                                  *
*******************************************************************************/
extern RTLISTANCHOR                g_lstControlSessionThreads;
extern VBOXSERVICECTRLSESSION      g_Session;

extern int                  VBoxServiceLogCreate(const char *pszLogFile);
extern void                 VBoxServiceLogDestroy(void);

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  gstcntlSessionFileDestroy(PVBOXSERVICECTRLFILE pFile);
static int                  gstcntlSessionForkShutdown(PVBOXSERVICECTRLSESSION pSession);
static PVBOXSERVICECTRLFILE gstcntlSessionGetFile(const PVBOXSERVICECTRLSESSION pSession, uint32_t uHandle);
static int                  gstcntlSessionGetOutput(const PVBOXSERVICECTRLSESSION pSession, uint32_t uPID, uint32_t uCID, uint32_t uHandleId, uint32_t cMsTimeout, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead);
static int                  gstcntlSessionHandleFileOpen(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
static int                  gstcntlSessionHandleFileClose(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
static int                  gstcntlSessionHandleFileRead(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
static int                  gstcntlSessionHandleFileWrite(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf);
static int                  gstcntlSessionHandleFileSeek(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
static int                  gstcntlSessionHandleFileTell(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLHOSTCTX pHostCtx);
static int                  gstcntlSessionSetInput(const PVBOXSERVICECTRLSESSION pSession, uint32_t uPID, uint32_t uCID, bool fPendingClose, void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten);
static DECLCALLBACK(int)    gstcntlSessionThread(RTTHREAD ThreadSelf, void *pvUser);

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


static PVBOXSERVICECTRLFILE gstcntlSessionGetFile(const PVBOXSERVICECTRLSESSION pSession,
                                                  uint32_t uHandle)
{
    AssertPtrReturn(pSession, NULL);

    PVBOXSERVICECTRLFILE pFileCur = NULL;
    /** @todo Use a map later! */
    RTListForEach(&pSession->lstFiles, pFileCur, VBOXSERVICECTRLFILE, Node)
    {
        if (pFileCur->uHandle == uHandle)
            return pFileCur;
    }

    return NULL;
}


#ifdef DEBUG
static int gstcntlSessionDumpToFile(const char *pszFileName, void *pvBuf, size_t cbBuf)
{
    AssertPtrReturn(pszFileName, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);

    if (!cbBuf)
        return VINF_SUCCESS;

    char szFile[RTPATH_MAX];

    int rc = RTPathTemp(szFile, sizeof(szFile));
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(szFile, sizeof(szFile), pszFileName);

    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(4, "Dumping %ld bytes to \"%s\"\n", cbBuf, szFile);

        RTFILE fh;
        rc = RTFileOpen(&fh, szFile, RTFILE_O_OPEN_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileWrite(fh, pvBuf, cbBuf, NULL /* pcbWritten */);
            RTFileClose(fh);
        }
    }

    return rc;
}
#endif


static int gstcntlSessionHandleFileOpen(PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    char szFile[RTPATH_MAX];
    char szOpenMode[64];
    char szDisposition[64];
    uint32_t uCreationMode = 0;
    uint64_t uOffset = 0;

    uint32_t uHandle = 0;
    int rc = VbglR3GuestCtrlFileGetOpen(pHostCtx,
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
                uHandle = VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pHostCtx->uContextID);
                pFile->uHandle = uHandle;
                /* rc = */ RTListAppend(&pSession->lstFiles, &pFile->Node);

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
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_OPEN, &cplOpen, sizeof(cplOpen));
        if (RT_FAILURE(rc2))
            VBoxServiceError("[File %s]: Failed to report file open status, rc=%Rrc\n",
                             szFile, rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


static int gstcntlSessionHandleFileClose(const PVBOXSERVICECTRLSESSION pSession,
                                         PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;

    int rc = VbglR3GuestCtrlFileGetClose(pHostCtx, &uHandle /* File handle to close */);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
        if (pFile)
        {
            rc = gstcntlSessionFileDestroy(pFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_CLOSE cplClose = { rc };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_CLOSE, &cplClose, sizeof(cplClose));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file close status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileRead(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                        void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;
    uint32_t cbToRead;

    int rc = VbglR3GuestCtrlFileGetRead(pHostCtx, &uHandle, &cbToRead);
    if (RT_SUCCESS(rc))
    {
        void *pvDataRead = pvScratchBuf;
        size_t cbRead = 0;

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
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
        CALLBACKPAYLOAD_FILE_NOTFIY_READ cplRead = { rc, (uint32_t)cbRead, pvDataRead };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_READ, &cplRead, sizeof(cplRead));
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


static int gstcntlSessionHandleFileReadAt(const PVBOXSERVICECTRLSESSION pSession,
                                          PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                          void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;
    uint32_t cbToRead; int64_t iOffset;

    int rc = VbglR3GuestCtrlFileGetReadAt(pHostCtx,
                                          &uHandle, &cbToRead, (uint64_t*)&iOffset);
    if (RT_SUCCESS(rc))
    {
        void *pvDataRead = pvScratchBuf;
        size_t cbRead = 0;

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
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
        CALLBACKPAYLOAD_FILE_NOTFIY_READ cplRead = { rc, (uint32_t)cbRead, pvDataRead };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_READ, &cplRead, sizeof(cplRead));
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


static int gstcntlSessionHandleFileWrite(const PVBOXSERVICECTRLSESSION pSession,
                                         PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                         void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(cbScratchBuf, VERR_INVALID_PARAMETER);

    uint32_t uHandle;
    uint32_t cbToWrite;

    int rc = VbglR3GuestCtrlFileGetWrite(pHostCtx, &uHandle,
                                         pvScratchBuf, cbScratchBuf,
                                         &cbToWrite);
    if (RT_SUCCESS(rc))
    {
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWrite(pFile->hFile, pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_WRITE cplWrite = { rc, (uint32_t)cbWritten };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_WRITE, &cplWrite, sizeof(cplWrite));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileWriteAt(const PVBOXSERVICECTRLSESSION pSession,
                                           PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                           void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(cbScratchBuf, VERR_INVALID_PARAMETER);

    uint32_t uHandle;
    uint32_t cbToWrite; int64_t iOffset;

    int rc = VbglR3GuestCtrlFileGetWriteAt(pHostCtx, &uHandle,
                                           pvScratchBuf, cbScratchBuf,
                                           &cbToWrite, (uint64_t*)&iOffset);
    if (RT_SUCCESS(rc))
    {
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWriteAt(pFile->hFile, iOffset,
                               pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_WRITE cplWrite = { rc, (uint32_t)cbWritten };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_WRITE, &cplWrite, sizeof(cplWrite));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileSeek(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;
    uint32_t uSeekMethod;
    uint64_t uOffset; /* Will be converted to int64_t. */

    uint64_t uOffsetActual = 0;

    int rc = VbglR3GuestCtrlFileGetSeek(pHostCtx, &uHandle,
                                        &uSeekMethod, &uOffset);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
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
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_SEEK, &cplSeek, sizeof(cplSeek));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file seek status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileTell(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;
    uint64_t uOffsetActual = 0;

    int rc = VbglR3GuestCtrlFileGetTell(pHostCtx, &uHandle);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionGetFile(pSession, uHandle);
        if (pFile)
        {
            uOffsetActual = RTFileTell(pFile->hFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        CALLBACKPAYLOAD_FILE_NOTFIY_TELL cplTell = { rc, uOffsetActual };
        int rc2 = VbglR3GuestCtrlFileNotify(pHostCtx->uClientID, pHostCtx->uContextID,
                                            GUEST_FILE_NOTIFYTYPE_TELL, &cplTell, sizeof(cplTell));
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file tell status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Handles starting a guest processes.
 *
 * @returns IPRT status code.
 * @param   pSession        Guest session.
 * @param   uClientID       The HGCM client session ID.
 * @param   cParms          The number of parameters the host is offering.
 */
int GstCntlSessionHandleProcExec(PVBOXSERVICECTRLSESSION pSession,
                                 PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    int rc;
    bool fStartAllowed = false; /* Flag indicating whether starting a process is allowed or not. */

    if (   (pHostCtx->uProtocol < 2  && pHostCtx->uNumParms == 11)
        || (pHostCtx->uProtocol >= 2 && pHostCtx->uNumParms == 12)
       )
    {
        VBOXSERVICECTRLPROCSTARTUPINFO proc;
        RT_ZERO(proc);

        /* Initialize maximum environment block size -- needed as input
         * parameter to retrieve the stuff from the host. On output this then
         * will contain the actual block size. */
        proc.cbEnv = sizeof(proc.szEnv);

        rc = VbglR3GuestCtrlProcGetStart(pHostCtx,
                                         /* Command */
                                         proc.szCmd,      sizeof(proc.szCmd),
                                         /* Flags */
                                         &proc.uFlags,
                                         /* Arguments */
                                         proc.szArgs,     sizeof(proc.szArgs), &proc.uNumArgs,
                                         /* Environment */
                                         proc.szEnv, &proc.cbEnv, &proc.uNumEnvVars,
                                         /* Credentials; for hosts with VBox < 4.3. */
                                         proc.szUser,     sizeof(proc.szUser),
                                         proc.szPassword, sizeof(proc.szPassword),
                                         /* Timelimit */
                                         &proc.uTimeLimitMS,
                                         /* Process priority */
                                         &proc.uPriority,
                                         /* Process affinity */
                                         proc.uAffinity,  sizeof(proc.uAffinity), &proc.uNumAffinity);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Request to start process szCmd=%s, uFlags=0x%x, szArgs=%s, szEnv=%s, szUser=%s, szPassword=%s, uTimeout=%RU32\n",
                               proc.szCmd, proc.uFlags,
                               proc.uNumArgs ? proc.szArgs : "<None>",
                               proc.uNumEnvVars ? proc.szEnv : "<None>",
                               proc.szUser,
#ifdef DEBUG
                               proc.szPassword,
#else
                               "XXX", /* Never show passwords in release mode. */
#endif
                               proc.uTimeLimitMS);

            rc = GstCntlSessionReapThreads(pSession);
            if (RT_FAILURE(rc))
                VBoxServiceError("Reaping stopped processes failed with rc=%Rrc\n", rc);
            /* Keep going. */

            rc = GstCntlSessionProcessStartAllowed(pSession, &fStartAllowed);
            if (RT_SUCCESS(rc))
            {
                if (fStartAllowed)
                {
                    rc = GstCntlProcessStart(pHostCtx->uContextID, &proc);
                }
                else
                    rc = VERR_MAX_PROCS_REACHED; /* Maximum number of processes reached. */
            }
        }
    }
    else
        rc = VERR_NOT_SUPPORTED; /* Unsupported number of parameters. */

    /* In case of an error we need to notify the host to not wait forever for our response. */
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Starting process failed with rc=%Rrc\n", rc);

        /*
         * Note: The context ID can be 0 because we mabye weren't able to fetch the command
         *       from the host. The host in case has to deal with that!
         */
        int rc2 = VbglR3GuestCtrlProcCbStatus(pHostCtx->uClientID, pHostCtx->uContextID,
                                              0 /* PID, invalid */,
                                              PROC_STS_ERROR, rc,
                                              NULL /* pvData */, 0 /* cbData */);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("Error sending start process status to host, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


/**
 * Handles input for a started process by copying the received data into its
 * stdin pipe.
 *
 * @returns IPRT status code.
 * @param   pvScratchBuf                The scratch buffer.
 * @param   cbScratchBuf                The scratch buffer size for retrieving the input data.
 */
int GstCntlSessionHandleProcInput(PVBOXSERVICECTRLSESSION pSession,
                                  PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                                  void *pvScratchBuf, size_t cbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(cbScratchBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);

    uint32_t uPID;
    uint32_t uFlags;
    uint32_t cbSize;

    uint32_t uStatus = INPUT_STS_UNDEFINED; /* Status sent back to the host. */
    uint32_t cbWritten = 0; /* Number of bytes written to the guest. */

    /*
     * Ask the host for the input data.
     */
    int rc = VbglR3GuestCtrlProcGetInput(pHostCtx, &uPID, &uFlags,
                                         pvScratchBuf, cbScratchBuf, &cbSize);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("[PID %RU32]: Failed to retrieve exec input command! Error: %Rrc\n",
                         uPID, rc);
    }
    else if (cbSize > cbScratchBuf)
    {
        VBoxServiceError("[PID %RU32]: Too much input received! cbSize=%u, cbScratchBuf=%u\n",
                         uPID, cbSize, cbScratchBuf);
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        /*
         * Is this the last input block we need to deliver? Then let the pipe know ...
         */
        bool fPendingClose = false;
        if (uFlags & INPUT_FLAG_EOF)
        {
            fPendingClose = true;
            VBoxServiceVerbose(4, "[PID %RU32]: Got last input block of size %u ...\n",
                               uPID, cbSize);
        }

        rc = gstcntlSessionSetInput(pSession, uPID,
                                    pHostCtx->uContextID, fPendingClose, pvScratchBuf,
                                    cbSize, &cbWritten);
        VBoxServiceVerbose(4, "[PID %RU32]: Written input, CID=%u, rc=%Rrc, uFlags=0x%x, fPendingClose=%d, cbSize=%u, cbWritten=%u\n",
                           uPID, pHostCtx->uContextID, rc, uFlags, fPendingClose, cbSize, cbWritten);
        if (RT_SUCCESS(rc))
        {
            uStatus = INPUT_STS_WRITTEN;
            uFlags = 0; /* No flags at the moment. */
        }
        else
        {
            if (rc == VERR_BAD_PIPE)
                uStatus = INPUT_STS_TERMINATED;
            else if (rc == VERR_BUFFER_OVERFLOW)
                uStatus = INPUT_STS_OVERFLOW;
        }
    }

    /*
     * If there was an error and we did not set the host status
     * yet, then do it now.
     */
    if (   RT_FAILURE(rc)
        && uStatus == INPUT_STS_UNDEFINED)
    {
        uStatus = INPUT_STS_ERROR;
        uFlags = rc;
    }
    Assert(uStatus > INPUT_STS_UNDEFINED);

    VBoxServiceVerbose(3, "[PID %RU32]: Input processed, CID=%u, uStatus=%u, uFlags=0x%x, cbWritten=%u\n",
                       uPID, pHostCtx->uContextID, uStatus, uFlags, cbWritten);

    /* Note: Since the context ID is unique the request *has* to be completed here,
     *       regardless whether we got data or not! Otherwise the progress object
     *       on the host never will get completed! */
    rc = VbglR3GuestCtrlProcCbStatusInput(pHostCtx->uClientID, pHostCtx->uContextID, uPID,
                                          uStatus, uFlags, (uint32_t)cbWritten);

    if (RT_FAILURE(rc))
        VBoxServiceError("[PID %RU32]: Failed to report input status! Error: %Rrc\n",
                         uPID, rc);
    return rc;
}


/**
 * Handles the guest control output command.
 *
 * @return  IPRT status code.
 */
int GstCntlSessionHandleProcOutput(PVBOXSERVICECTRLSESSION pSession,
                                   PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlProcGetOutput(pHostCtx, &uPID, &uHandleID, &uFlags);
    if (RT_SUCCESS(rc))
    {
        uint8_t *pBuf = (uint8_t*)RTMemAlloc(_64K);
        if (pBuf)
        {
            uint32_t cbRead = 0;
            rc = gstcntlSessionGetOutput(pSession, uPID,
                                         pHostCtx->uContextID, uHandleID, RT_INDEFINITE_WAIT /* Timeout */,
                                         pBuf, _64K /* cbSize */, &cbRead);
            VBoxServiceVerbose(3, "[PID %RU32]: Got output, rc=%Rrc, CID=%u, cbRead=%u, uHandle=%u, uFlags=%u\n",
                               uPID, rc, pHostCtx->uContextID, cbRead, uHandleID, uFlags);

#ifdef DEBUG
            if (   (pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT)
                && (uHandleID == OUTPUT_HANDLE_ID_STDERR))
            {
                char szDumpFile[RTPATH_MAX];
                if (!RTStrPrintf(szDumpFile, sizeof(szDumpFile), "VBoxService_Session%RU32_PID%RU32_StdOut.txt",
                                 pSession->StartupInfo.uSessionID, uPID))
                    rc = VERR_BUFFER_UNDERFLOW;
                if (RT_SUCCESS(rc))
                    rc = gstcntlSessionDumpToFile(szDumpFile, pBuf, cbRead);
            }
            else if (   (pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR)
                     && (   uHandleID == OUTPUT_HANDLE_ID_STDOUT
                         || uHandleID == OUTPUT_HANDLE_ID_STDOUT_DEPRECATED))
            {
                char szDumpFile[RTPATH_MAX];
                if (!RTStrPrintf(szDumpFile, sizeof(szDumpFile), "VBoxService_Session%RU32_PID%RU32_StdErr.txt",
                                 pSession->StartupInfo.uSessionID, uPID))
                    rc = VERR_BUFFER_UNDERFLOW;
                if (RT_SUCCESS(rc))
                    rc = gstcntlSessionDumpToFile(szDumpFile, pBuf, cbRead);
                AssertRC(rc);
            }
#endif
            /** Note: Don't convert/touch/modify/whatever the output data here! This might be binary
             *        data which the host needs to work with -- so just pass through all data unfiltered! */

            /* Note: Since the context ID is unique the request *has* to be completed here,
             *       regardless whether we got data or not! Otherwise the progress object
             *       on the host never will get completed! */
            int rc2 = VbglR3GuestCtrlProcCbOutput(pHostCtx->uClientID, pHostCtx->uContextID, uPID, uHandleID, uFlags,
                                                  pBuf, cbRead);
            if (RT_SUCCESS(rc))
                rc = rc2;
            else if (rc == VERR_NOT_FOUND) /* It's not critical if guest process (PID) is not found. */
                rc = VINF_SUCCESS;

            RTMemFree(pBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        VBoxServiceError("[PID %RU32]: Error handling output command! Error: %Rrc\n",
                         uPID, rc);
    return rc;
}


int GstCntlSessionHandleProcTerminate(const PVBOXSERVICECTRLSESSION pSession,
                                      PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    int rc = VbglR3GuestCtrlProcGetTerminate(pHostCtx, &uPID);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLREQUEST pRequest;
        rc = GstCntlProcessRequestAllocEx(&pRequest, VBOXSERVICECTRLREQUEST_PROC_TERM,
                                          NULL /* pvBuf */, 0 /* cbBuf */, pHostCtx->uContextID);
        if (RT_SUCCESS(rc))
        {
            PVBOXSERVICECTRLTHREAD pProcess = GstCntlSessionAcquireProcess(pSession, uPID);
            if (pProcess)
            {
                rc = GstCntlProcessPerform(pProcess, pRequest);
                GstCntlProcessRelease(pProcess);
            }
            else
                rc = VERR_NOT_FOUND;

            GstCntlProcessRequestFree(pRequest);
        }
    }

    return rc;
}


int GstCntlSessionHandleProcWaitFor(const PVBOXSERVICECTRLSESSION pSession,
                                    PVBGLR3GUESTCTRLHOSTCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    uint32_t uWaitFlags; uint32_t uTimeoutMS;

    int rc = VbglR3GuestCtrlProcGetWaitFor(pHostCtx, &uPID, &uWaitFlags, &uTimeoutMS);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLREQUEST pRequest;
        VBOXSERVICECTRLREQDATA_WAIT_FOR reqData = { uWaitFlags, uTimeoutMS };
        rc = GstCntlProcessRequestAllocEx(&pRequest, VBOXSERVICECTRLREQUEST_WAIT_FOR,
                                          &reqData, sizeof(reqData), pHostCtx->uContextID);
        if (RT_SUCCESS(rc))
        {
            PVBOXSERVICECTRLTHREAD pProcess = GstCntlSessionAcquireProcess(pSession, uPID);
            if (pProcess)
            {
                rc = GstCntlProcessPerform(pProcess, pRequest);
                GstCntlProcessRelease(pProcess);
            }
            else
                rc = VERR_NOT_FOUND;

            GstCntlProcessRequestFree(pRequest);
        }
    }

    return rc;
}


int GstCntlSessionHandler(PVBOXSERVICECTRLSESSION pSession,
                          uint32_t uMsg, PVBGLR3GUESTCTRLHOSTCTX pHostCtx,
                          void *pvScratchBuf, size_t cbScratchBuf,
                          volatile bool *pfShutdown)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pfShutdown, VERR_INVALID_POINTER);

    int rc;

    switch (uMsg)
    {
        case HOST_CANCEL_PENDING_WAITS:
            VBoxServiceVerbose(1, "We were asked to quit ...\n");
            /* Fall thru is intentional. */
        case HOST_SESSION_CLOSE:
            /* Shutdown this fork. */
            rc = gstcntlSessionForkShutdown(pSession);
            *pfShutdown = true; /* Shutdown in any case. */
            break;

        case HOST_EXEC_CMD:
            rc = GstCntlSessionHandleProcExec(pSession, pHostCtx);
            break;

        case HOST_EXEC_SET_INPUT:
            rc = GstCntlSessionHandleProcInput(pSession, pHostCtx,
                                               pvScratchBuf, cbScratchBuf);
            break;

        case HOST_EXEC_GET_OUTPUT:
            rc = GstCntlSessionHandleProcOutput(pSession, pHostCtx);
            break;

        case HOST_EXEC_TERMINATE:
            rc = GstCntlSessionHandleProcTerminate(pSession, pHostCtx);
            break;

        case HOST_EXEC_WAIT_FOR:
            rc = GstCntlSessionHandleProcWaitFor(pSession, pHostCtx);
            break;

        case HOST_FILE_OPEN:
            rc = gstcntlSessionHandleFileOpen(pSession, pHostCtx);
            break;

        case HOST_FILE_CLOSE:
            rc = gstcntlSessionHandleFileClose(pSession, pHostCtx);
            break;

        case HOST_FILE_READ:
            rc = gstcntlSessionHandleFileRead(pSession, pHostCtx,
                                              pvScratchBuf, cbScratchBuf);
            break;

        case HOST_FILE_READ_AT:
            rc = gstcntlSessionHandleFileReadAt(pSession, pHostCtx,
                                                pvScratchBuf, cbScratchBuf);
            break;

        case HOST_FILE_WRITE:
            rc = gstcntlSessionHandleFileWrite(pSession, pHostCtx,
                                               pvScratchBuf, cbScratchBuf);
            break;

        case HOST_FILE_WRITE_AT:
            rc = gstcntlSessionHandleFileWriteAt(pSession, pHostCtx,
                                                 pvScratchBuf, cbScratchBuf);
            break;

        case HOST_FILE_SEEK:
            rc = gstcntlSessionHandleFileSeek(pSession, pHostCtx);
            break;

        case HOST_FILE_TELL:
            rc = gstcntlSessionHandleFileTell(pSession, pHostCtx);
            break;

        default:
            VBoxServiceVerbose(3, "Unsupported message from host, uMsg=%RU32, cParms=%RU32\n",
                               uMsg, pHostCtx->uNumParms);
            /* Don't terminate here; just wait for the next message. */
            break;
    }

    return rc;
}


/**
 * Thread main routine for a forked guest session. This
 * thread runs in the main executable to control the forked
 * session process.
 *
 * @return IPRT status code.
 * @param  RTTHREAD             Pointer to the thread's data.
 * @param  void*                User-supplied argument pointer.
 *
 */
static DECLCALLBACK(int) gstcntlSessionThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLSESSIONTHREAD pSession = (PVBOXSERVICECTRLSESSIONTHREAD)pvUser;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    VBoxServiceVerbose(3, "Session ID=%RU32 thread running\n",
                       pSession->StartupInfo.uSessionID);

    int rc = VINF_SUCCESS;

    /* Let caller know that we're done initializing. */
    int rc2 = RTThreadUserSignal(RTThreadSelf());
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS ProcessStatus;
        int rc2;
        for (;;)
        {
            rc2 = RTProcWaitNoResume(pSession->hProcess, RTPROCWAIT_FLAGS_BLOCK,
                                     &ProcessStatus);
            if (RT_UNLIKELY(rc2 == VERR_INTERRUPTED))
                continue;
            else if (   rc2 == VINF_SUCCESS
                     || rc2 == VERR_PROCESS_NOT_FOUND)
            {
                break;
            }
            else
                AssertMsgBreak(rc2 == VERR_PROCESS_RUNNING,
                               ("Got unexpected rc=%Rrc while waiting for session process termination\n", rc2));
        }

        VBoxServiceVerbose(3, "Guest session process ID=%RU32 terminated with rc=%Rrc, reason=%ld, status=%d\n",
                           pSession->StartupInfo.uSessionID, rc,
                           ProcessStatus.enmReason, ProcessStatus.iStatus);
    }

    VBoxServiceVerbose(3, "Session ID=%RU32 thread ended\n", pSession->StartupInfo.uSessionID);
    return rc;
}


RTEXITCODE gstcntlSessionWorker(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, RTEXITCODE_SYNTAX);

    bool fSessionFilter = true;

    VBoxServiceVerbose(0, "Hi, this is guest session ID=%RU32\n",
                       pSession->StartupInfo.uSessionID);

    uint32_t uClientID;
    int rc = VbglR3GuestCtrlConnect(&uClientID);
    if (RT_SUCCESS(rc))
    {
        /* Set session filter. */
        uint32_t uFilterAdd =
            VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(pSession->StartupInfo.uSessionID);

        rc = VbglR3GuestCtrlMsgSetFilter(uClientID, uFilterAdd, 0 /* Filter remove */);
        VBoxServiceVerbose(3, "Setting message filterAdd=%RU32 returned %Rrc\n",
                           uFilterAdd, rc);

        if (   RT_FAILURE(rc)
            && rc == VERR_NOT_SUPPORTED)
        {
            /* No session filter available. Skip. */
            fSessionFilter = false;

            rc = VINF_SUCCESS;
        }

        VBoxServiceVerbose(1, "Using client ID=%RU32\n", uClientID);
    }
    else
        VBoxServiceError("Error connecting to guest control service, rc=%Rrc\n", rc);

    /* Allocate a scratch buffer for commands which also send
     * payload data with them. */
    uint32_t cbScratchBuf = _64K; /** @todo Make buffer size configurable via guest properties/argv! */
    AssertReturn(RT_IS_POWER_OF_TWO(cbScratchBuf), RTEXITCODE_FAILURE);
    uint8_t *pvScratchBuf = NULL;

    if (RT_SUCCESS(rc))
    {
        pvScratchBuf = (uint8_t*)RTMemAlloc(cbScratchBuf);
        if (!pvScratchBuf)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        bool fShutdown = false;

        VBGLR3GUESTCTRLHOSTCTX ctxHost = { uClientID,
                                           pSession->StartupInfo.uProtocol };
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

                rc = GstCntlSessionHandler(pSession, uMsg, &ctxHost,
                                           pvScratchBuf, cbScratchBuf, &fShutdown);
            }

            if (fShutdown)
                break;

            /* Let's sleep for a bit and let others run ... */
            RTThreadYield();
        }
    }

    VBoxServiceVerbose(0, "Session %RU32 ended\n", pSession->StartupInfo.uSessionID);

    if (pvScratchBuf)
        RTMemFree(pvScratchBuf);

    VBoxServiceVerbose(3, "Disconnecting client ID=%RU32 ...\n", uClientID);
    VbglR3GuestCtrlDisconnect(uClientID);

    VBoxServiceVerbose(3, "Session worker returned with rc=%Rrc\n", rc);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Finds a (formerly) started guest process given by its PID and locks it.
 * Must be unlocked by the caller with GstCntlProcessRelease().
 *
 * @return  PVBOXSERVICECTRLTHREAD      Locked guest process if found, otherwise NULL.
 * @param   PVBOXSERVICECTRLSESSION     Pointer to guest session where to search process in.
 * @param   uPID                        PID to search for.
 */
PVBOXSERVICECTRLTHREAD GstCntlSessionAcquireProcess(PVBOXSERVICECTRLSESSION pSession, uint32_t uPID)
{
    AssertPtrReturn(pSession, NULL);

    PVBOXSERVICECTRLTHREAD pThread = NULL;
    int rc = RTCritSectEnter(&pSession->csControlThreads);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pThreadCur;
        RTListForEach(&pSession->lstControlThreadsActive, pThreadCur, VBOXSERVICECTRLTHREAD, Node)
        {
            if (pThreadCur->uPID == uPID)
            {
                rc = RTCritSectEnter(&pThreadCur->CritSect);
                if (RT_SUCCESS(rc))
                    pThread = pThreadCur;
                break;
            }
        }

        int rc2 = RTCritSectLeave(&pSession->csControlThreads);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return pThread;
}


int GstCntlSessionCloseAll(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    /* Signal all threads in the active list that we want to shutdown. */
    PVBOXSERVICECTRLTHREAD pThread;
    RTListForEach(&pSession->lstControlThreadsActive, pThread, VBOXSERVICECTRLTHREAD, Node)
        GstCntlProcessStop(pThread);

    /* Wait for all active threads to shutdown and destroy the active thread list. */
    pThread = RTListGetFirst(&pSession->lstControlThreadsActive, VBOXSERVICECTRLTHREAD, Node);
    while (pThread)
    {
        PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pThread->Node, VBOXSERVICECTRLTHREAD, Node);
        bool fLast = RTListNodeIsLast(&pSession->lstControlThreadsActive, &pThread->Node);

        int rc2 = GstCntlProcessWait(pThread,
                                     30 * 1000 /* Wait 30 seconds max. */,
                                     NULL /* rc */);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("Guest process thread failed to stop; rc=%Rrc\n", rc2);
            /* Keep going. */
        }

        if (fLast)
            break;

        pThread = pNext;
    }

    int rc = GstCntlSessionReapThreads(pSession);
    if (RT_FAILURE(rc))
        VBoxServiceError("Reaping inactive threads failed with rc=%Rrc\n", rc);

    AssertMsg(RTListIsEmpty(&pSession->lstControlThreadsActive),
              ("Guest process active thread list still contains entries when it should not\n"));
    AssertMsg(RTListIsEmpty(&pSession->lstControlThreadsInactive),
              ("Guest process inactive thread list still contains entries when it should not\n"));

    return rc;
}


int GstCntlSessionDestroy(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    int rc = GstCntlSessionCloseAll(pSession);

    /* Destroy critical section. */
    RTCritSectDelete(&pSession->csControlThreads);

    return rc;
}


/**
 * Gets output from stdout/stderr of a specified guest process.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session.
 * @param   uPID                    PID of process to retrieve the output from.
 * @param   uCID                    Context ID.
 * @param   uHandleId               Stream ID (stdout = 0, stderr = 2) to get the output from.
 * @param   cMsTimeout              Timeout (in ms) to wait for output becoming
 *                                  available.
 * @param   pvBuf                   Pointer to a pre-allocated buffer to store the output.
 * @param   cbBuf                   Size (in bytes) of the pre-allocated buffer.
 * @param   pcbRead                 Pointer to number of bytes read.  Optional.
 */
static int gstcntlSessionGetOutput(const PVBOXSERVICECTRLSESSION pSession,
                                   uint32_t uPID, uint32_t uCID,
                                   uint32_t uHandleId, uint32_t cMsTimeout,
                                   void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbRead, VERR_INVALID_POINTER);

    int                         rc      = VINF_SUCCESS;
    VBOXSERVICECTRLREQUESTTYPE  reqType = VBOXSERVICECTRLREQUEST_UNKNOWN; /* (gcc maybe, well, wrong.) */
    switch (uHandleId)
    {
        case OUTPUT_HANDLE_ID_STDERR:
            reqType = VBOXSERVICECTRLREQUEST_PROC_STDERR;
            break;

        case OUTPUT_HANDLE_ID_STDOUT:
        case OUTPUT_HANDLE_ID_STDOUT_DEPRECATED:
            reqType = VBOXSERVICECTRLREQUEST_PROC_STDOUT;
            break;

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLREQUEST pRequest;
        rc = GstCntlProcessRequestAllocEx(&pRequest, reqType, pvBuf, cbBuf, uCID);
        if (RT_SUCCESS(rc))
        {
            PVBOXSERVICECTRLTHREAD pProcess = GstCntlSessionAcquireProcess(pSession, uPID);
            if (pProcess)
            {
                rc = GstCntlProcessPerform(pProcess, pRequest);
                GstCntlProcessRelease(pProcess);
            }
            else
                rc = VERR_NOT_FOUND;

            if (RT_SUCCESS(rc) && pcbRead)
                *pcbRead = pRequest->cbData;
            GstCntlProcessRequestFree(pRequest);
        }
    }

    return rc;
}


int GstCntlSessionInit(PVBOXSERVICECTRLSESSION pSession, uint32_t uFlags)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    RTListInit(&pSession->lstControlThreadsActive);
    RTListInit(&pSession->lstControlThreadsInactive);
    RTListInit(&pSession->lstFiles);

    pSession->uFlags = uFlags;

    if (pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_FORK)
    {
        /* Protocol must be specified explicitly. */
        pSession->StartupInfo.uProtocol = UINT32_MAX;

        /* Session ID must be specified explicitly. */
        pSession->StartupInfo.uSessionID = UINT32_MAX;
    }

    /* Init critical section for protecting the thread lists. */
    int rc = RTCritSectInit(&pSession->csControlThreads);
    AssertRC(rc);

    return rc;
}


/**
 * Sets the specified guest thread to a certain list.
 ** @todo Still needed?
 *
 * @return  IPRT status code.
 * @param   enmList                 List to move thread to.
 * @param   pThread                 Thread to set inactive.
 */
int GstCntlSessionListSet(PVBOXSERVICECTRLSESSION pSession,
                          VBOXSERVICECTRLTHREADLISTTYPE enmList,
                          PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertReturn(enmList > VBOXSERVICECTRLTHREADLIST_UNKNOWN, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->csControlThreads);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "Setting thread (PID %RU32) to list %d\n",
                           pThread->uPID, enmList);

        PRTLISTANCHOR pAnchor = NULL;
        switch (enmList)
        {
            case VBOXSERVICECTRLTHREADLIST_STOPPED:
                pAnchor = &pSession->lstControlThreadsInactive;
                break;

            case VBOXSERVICECTRLTHREADLIST_RUNNING:
                pAnchor = &pSession->lstControlThreadsActive;
                break;

            default:
                AssertMsgFailed(("Unknown list type: %u", enmList));
                break;
        }

        if (!pAnchor)
            rc = VERR_INVALID_PARAMETER;

        if (RT_SUCCESS(rc))
        {
            if (pThread->pAnchor != NULL)
            {
                /* If thread was assigned to a list before,
                 * remove the thread from the old list first. */
                /* rc = */ RTListNodeRemove(&pThread->Node);
            }

            /* Add thread to desired list. */
            /* rc = */ RTListAppend(pAnchor, &pThread->Node);
            pThread->pAnchor = pAnchor;
        }

        int rc2 = RTCritSectLeave(&pSession->csControlThreads);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return VINF_SUCCESS;
}





/**
 * Determines whether starting a new guest process according to the
 * maximum number of concurrent guest processes defined is allowed or not.
 *
 * @return  IPRT status code.
 * @param   pbAllowed           True if starting (another) guest process
 *                              is allowed, false if not.
 */
int GstCntlSessionProcessStartAllowed(const PVBOXSERVICECTRLSESSION pSession,
                                      bool *pbAllowed)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pbAllowed, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->csControlThreads);
    if (RT_SUCCESS(rc))
    {
        /*
         * Check if we're respecting our memory policy by checking
         * how many guest processes are started and served already.
         */
        bool fLimitReached = false;
        if (pSession->uProcsMaxKept) /* If we allow unlimited processes (=0), take a shortcut. */
        {
            uint32_t uProcsRunning = 0;
            PVBOXSERVICECTRLTHREAD pThread;
            RTListForEach(&pSession->lstControlThreadsActive, pThread, VBOXSERVICECTRLTHREAD, Node)
                uProcsRunning++;

            VBoxServiceVerbose(3, "Maximum served guest processes set to %u, running=%u\n",
                               pSession->uProcsMaxKept, uProcsRunning);

            int32_t iProcsLeft = (pSession->uProcsMaxKept - uProcsRunning - 1);
            if (iProcsLeft < 0)
            {
                VBoxServiceVerbose(3, "Maximum running guest processes reached (%u)\n",
                                   pSession->uProcsMaxKept);
                fLimitReached = true;
            }
        }

        *pbAllowed = !fLimitReached;

        int rc2 = RTCritSectLeave(&pSession->csControlThreads);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


/**
 * Reaps all inactive guest process threads.
 *
 * @return  IPRT status code.
 */
int GstCntlSessionReapThreads(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->csControlThreads);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pThread =
            RTListGetFirst(&pSession->lstControlThreadsInactive, VBOXSERVICECTRLTHREAD, Node);
        while (pThread)
        {
            PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pThread->Node, VBOXSERVICECTRLTHREAD, Node);
            bool fLast = RTListNodeIsLast(&pSession->lstControlThreadsInactive, &pThread->Node);
            int rc2 = GstCntlProcessWait(pThread, 30 * 1000 /* 30 seconds max. */,
                                         NULL /* rc */);
            if (RT_SUCCESS(rc2))
            {
                RTListNodeRemove(&pThread->Node);

                rc2 = GstCntlProcessFree(pThread);
                if (RT_FAILURE(rc2))
                {
                    VBoxServiceError("Freeing guest process thread failed with rc=%Rrc\n", rc2);
                    if (RT_SUCCESS(rc)) /* Keep original failure. */
                        rc = rc2;
                }
            }
            else
                VBoxServiceError("Waiting on guest process thread failed with rc=%Rrc\n", rc2);
            /* Keep going. */

            if (fLast)
                break;

            pThread = pNext;
        }

        int rc2 = RTCritSectLeave(&pSession->csControlThreads);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    VBoxServiceVerbose(4, "Reaping threads returned with rc=%Rrc\n", rc);
    return rc;
}


/**
 * Injects input to a specified running guest process.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session.
 * @param   uPID                    PID of process to set the input for.
 * @param   uCID                    Context ID to use for reporting back.
 * @param   fPendingClose           Flag indicating whether this is the last input block sent to the process.
 * @param   pvBuf                   Pointer to a buffer containing the actual input data.
 * @param   cbBuf                   Size (in bytes) of the input buffer data.
 * @param   pcbWritten              Pointer to number of bytes written to the process.  Optional.
 */
int gstcntlSessionSetInput(const PVBOXSERVICECTRLSESSION pSession,
                           uint32_t uPID, uint32_t uCID,
                           bool fPendingClose,
                           void *pvBuf, uint32_t cbBuf,
                           uint32_t *pcbWritten)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    /* pvBuf is optional. */
    /* cbBuf is optional. */
    /* pcbWritten is optional. */

    PVBOXSERVICECTRLREQUEST pRequest;
    int rc = GstCntlProcessRequestAllocEx(&pRequest,
                                          fPendingClose
                                          ? VBOXSERVICECTRLREQUEST_PROC_STDIN_EOF
                                          : VBOXSERVICECTRLREQUEST_PROC_STDIN,
                                          pvBuf, cbBuf, uCID);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pProcess = GstCntlSessionAcquireProcess(pSession, uPID);
        if (pProcess)
        {
            rc = GstCntlProcessPerform(pProcess, pRequest);
            GstCntlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;

        if (RT_SUCCESS(rc))
        {
            if (pcbWritten)
                *pcbWritten = pRequest->cbData;
        }

        GstCntlProcessRequestFree(pRequest);
    }

    return rc;
}


/**
 * Creates a guest session. This will spawn a new VBoxService.exe instance under
 * behalf of the given user which then will act as a session host. On successful
 * open, the session will be added to the session list.
 *
 * @return  IPRT status code.
 * @param   pList                   Which list to use to store the session thread in.
 * @param   pSessionStartupInfo     Session startup info.
 * @param   ppSessionThread         Returns newly created session thread on success.
 *                                  Optional.
 */
int GstCntlSessionThreadOpen(PRTLISTANCHOR pList,
                             const PVBOXSERVICECTRLSESSIONSTARTUPINFO pSessionStartupInfo,
                             PVBOXSERVICECTRLSESSIONTHREAD *ppSessionThread)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pSessionStartupInfo, VERR_INVALID_POINTER);
    /* ppSessionThread is optional. */

#ifdef DEBUG
    PVBOXSERVICECTRLSESSIONTHREAD pSessionCur;
    /* Check for existing session in debug mode. Should never happen because of
     * Main consistency. */
    RTListForEach(pList, pSessionCur, VBOXSERVICECTRLSESSIONTHREAD, Node)
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

    PVBOXSERVICECTRLSESSIONTHREAD pSession = (PVBOXSERVICECTRLSESSIONTHREAD)RTMemAllocZ(sizeof(VBOXSERVICECTRLSESSIONTHREAD));
    if (pSession)
    {
        /* Copy over session startup info. */
        memcpy(&pSession->StartupInfo, pSessionStartupInfo, sizeof(VBOXSERVICECTRLSESSIONSTARTUPINFO));

        pSession->fShutdown = false;
        pSession->fStarted  = false;
        pSession->fStopped  = false;

        /* Is this an anonymous session? */
        /* Anonymous sessions run with the same privileges as the main VBoxService executable. */
        bool fAnonymous = !RT_BOOL(strlen(pSession->StartupInfo.szUser));
        if (fAnonymous)
        {
            Assert(!strlen(pSession->StartupInfo.szPassword));
            Assert(!strlen(pSession->StartupInfo.szDomain));

            VBoxServiceVerbose(3, "New anonymous guest session ID=%RU32 created, uFlags=%x, using protocol %RU32\n",
                               pSessionStartupInfo->uSessionID,
                               pSessionStartupInfo->uFlags,
                               pSessionStartupInfo->uProtocol);
        }
        else
        {
            VBoxServiceVerbose(3, "Forking new guest session ID=%RU32, szUser=%s, szPassword=%s, szDomain=%s, uFlags=%x, using protocol %RU32\n",
                               pSessionStartupInfo->uSessionID,
                               pSessionStartupInfo->szUser,
#ifdef DEBUG
                               pSessionStartupInfo->szPassword,
#else
                               "XXX", /* Never show passwords in release mode. */
#endif
                               pSessionStartupInfo->szDomain,
                               pSessionStartupInfo->uFlags,
                               pSessionStartupInfo->uProtocol);
        }

        rc = RTCritSectInit(&pSession->CritSect);
        AssertRC(rc);

        /* Fork child doing the actual session handling. */
        char szExeName[RTPATH_MAX];
        char *pszExeName = RTProcGetExecutablePath(szExeName, sizeof(szExeName));
        if (pszExeName)
        {
            char szParmUserName[GUESTPROCESS_MAX_USER_LEN + 32];
            if (!fAnonymous)
            {
                if (!RTStrPrintf(szParmUserName, sizeof(szParmUserName), "--username=%s", pSession->StartupInfo.szUser))
                    rc = VERR_BUFFER_OVERFLOW;
            }
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
                if (!fAnonymous)
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
                                                 "--logfile=%s", pszLogFile))
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
                                                !fAnonymous ? pSession->StartupInfo.szUser : NULL,
                                                !fAnonymous ? pSession->StartupInfo.szPassword : NULL,
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
                RTHANDLE hStdIn;
                rc = RTFileOpenBitBucket(&hStdIn.u.hFile, RTFILE_O_READ);
                if (RT_SUCCESS(rc))
                {
                    hStdIn.enmType = RTHANDLETYPE_FILE;

                    RTHANDLE hStdOutAndErr;
                    rc = RTFileOpenBitBucket(&hStdOutAndErr.u.hFile, RTFILE_O_WRITE);
                    if (RT_SUCCESS(rc))
                    {
                        hStdOutAndErr.enmType = RTHANDLETYPE_FILE;

                        /** @todo Do we need a custom/cloned environment block? */
                        rc = RTProcCreateEx(pszExeName, papszArgs, RTENV_DEFAULT, uProcFlags,
                                            &hStdIn, &hStdOutAndErr, &hStdOutAndErr,
                                            !fAnonymous ? pSession->StartupInfo.szUser : NULL,
                                            !fAnonymous ? pSession->StartupInfo.szPassword : NULL,
                                            &pSession->hProcess);

                        RTFileClose(hStdOutAndErr.u.hFile);
                    }

                    RTFileClose(hStdOutAndErr.u.hFile);
                }
#endif
            }
        }
        else
            rc = VERR_FILE_NOT_FOUND;

        if (RT_SUCCESS(rc))
        {
            /* Start session thread. */
            static uint32_t s_uCtrlSessionThread = 0;
            if (s_uCtrlSessionThread++ == UINT32_MAX)
                s_uCtrlSessionThread = 0; /* Wrap around to not let IPRT freak out. */
            rc = RTThreadCreateF(&pSession->Thread, gstcntlSessionThread,
                                 pSession /*pvUser*/, 0 /*cbStack*/,
                                 RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "sess%u", s_uCtrlSessionThread);
            if (RT_FAILURE(rc))
            {
                VBoxServiceError("Creating session thread failed, rc=%Rrc\n", rc);
            }
            else
            {
                /* Wait for the thread to initialize. */
                rc = RTThreadUserWait(pSession->Thread, 60 * 1000 /* 60 seconds max. */);
                AssertRC(rc);
                if (   ASMAtomicReadBool(&pSession->fShutdown)
                    || RT_FAILURE(rc))
                {
                    VBoxServiceError("Thread for session ID=%RU32 failed to start\n",
                                     pSession->StartupInfo.uSessionID);
                    rc = VERR_CANT_CREATE; /** @todo Find a better rc. */
                }
                else
                {
                    ASMAtomicXchgBool(&pSession->fStarted, true);

                    /* Add session to list. */
                    /* rc = */ RTListAppend(pList, &pSession->Node);
                    if (ppSessionThread) /* Return session if wanted. */
                        *ppSessionThread = pSession;
                }
            }
        }

        if (RT_FAILURE(rc))
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
 * Closes a formerly opened guest session and removes it from
 * the session list.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session to close.
 * @param   uFlags                  Closing flags.
 */
int GstCntlSessionThreadClose(PVBOXSERVICECTRLSESSIONTHREAD pSession, uint32_t uFlags)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    /* The fork should have received the same closing request,
     * so just wait 30s for the process to close. On timeout kill
     * it in a not so gentle manner. */
    if (   pSession->Thread != NIL_RTTHREAD
        && ASMAtomicReadBool(&pSession->fStarted))
    {
        bool fAlive = true;
        uint64_t u64Start = RTTimeMilliTS();
        uint32_t cMsTimeout = 30 * 1000; /* 30 seconds. */

        VBoxServiceVerbose(3, "Waiting for session thread ID=%RU32 to close (%RU32ms) ...\n",
                           pSession->StartupInfo.uSessionID, cMsTimeout);

        int rcThread;
        rc = RTThreadWait(pSession->Thread, cMsTimeout, &rcThread);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("Waiting for session thread ID=%RU32 to close failed with rc=%Rrc\n",
                             pSession->StartupInfo.uSessionID, rc);
        }
        else
        {
            fAlive = false;
            VBoxServiceVerbose(3, "Session thread ID=%RU32 ended with rc=%Rrc\n",
                               pSession->StartupInfo.uSessionID, rcThread);
        }
#if 0
        for (;;)
        {
            uint64_t u64Now = RTTimeMilliTS();
            uint64_t cMsElapsed = u64Now - u64Start;

            if (cMsElapsed >= cMsTimeout)
                break;

            RTPROCSTATUS ProcessStatus;
            int rc2 = RTProcWaitNoResume(pSession->hProcess, RTPROCWAIT_FLAGS_NOBLOCK,
                                         &ProcessStatus);
            if (RT_UNLIKELY(rc2 == VERR_INTERRUPTED))
                continue;
            else if (   rc2 == VINF_SUCCESS
                     || rc2 == VERR_PROCESS_NOT_FOUND)
            {
                fAlive = false;
                break;
            }
            else
                AssertMsgBreak(rc2 == VERR_PROCESS_RUNNING,
                               ("Got unexpected rc=%Rrc while waiting for session process termination\n", rc2));

            RTThreadSleep(100); /* Wait a bit. */
        }
#endif
        if (fAlive)
        {
            VBoxServiceVerbose(3, "Session thread ID=%RU32 still alive, killing ...\n",
                               pSession->StartupInfo.uSessionID);
            int rc2 = GstCntlSessionThreadTerminate(pSession);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
        else
            VBoxServiceVerbose(3, "Session ID=%RU32 has been closed\n",
                               pSession->StartupInfo.uSessionID);

        /* Remove session from list and destroy object. */
        RTListNodeRemove(&pSession->Node);

        RTMemFree(pSession);
        pSession = NULL;
    }

    return rc;
}


/**
 * Close all formerly opened guest session threads.
 *
 * @return  IPRT status code.
 * @param   pList                   Which list to close the session threads for.
 * @param   uFlags                  Closing flags.
 */
int GstCntlSessionThreadCloseAll(PRTLISTANCHOR pList, uint32_t uFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PVBOXSERVICECTRLSESSIONTHREAD pSessionCur;
    RTListForEach(pList, pSessionCur, VBOXSERVICECTRLSESSIONTHREAD, Node)
    {
        int rc2 = GstCntlSessionThreadClose(pSessionCur, uFlags);
        if (RT_SUCCESS(rc))
        {
            rc = rc2;
            /* Keep going. */
        }
    }

    return rc;
}


/**
 * Terminates a formerly opened guest session thread. Only
 * use this as a last action!
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session to terminate.
 */
int GstCntlSessionThreadTerminate(PVBOXSERVICECTRLSESSIONTHREAD pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    if (pSession->hProcess == NIL_RTPROCESS)
        return VINF_SUCCESS; /* Nothing to terminate. */

    int rc;
    for (int i = 0; i < 3; i++)
    {
        rc = RTProcTerminate(pSession->hProcess);
        if (RT_SUCCESS(rc))
            break;
        RTThreadSleep(1000);
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

    uint32_t uSessionFlags = VBOXSERVICECTRLSESSION_FLAG_FORK;

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
                g_Session.StartupInfo.uSessionID = ValueUnion.u32;
                break;

            case VBOXSERVICESESSIONOPT_SESSION_PROTO:
                g_Session.StartupInfo.uProtocol = ValueUnion.u32;
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

    /* Init the session object. */
    rc = GstCntlSessionInit(&g_Session, uSessionFlags);

    if (g_Session.StartupInfo.uProtocol == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No protocol version specified");

    if (g_Session.StartupInfo.uSessionID == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No session ID specified");

    rc = VBoxServiceLogCreate(strlen(g_szLogFile) ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create release log (%s, %Rrc)",
                              strlen(g_szLogFile) ? g_szLogFile : "<None>", rc);

    RTEXITCODE rcExit = gstcntlSessionWorker(&g_Session);

    VBoxServiceLogDestroy();
    return rcExit;
}

static int gstcntlSessionForkShutdown(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    VBoxServiceVerbose(0, "Session %RU32 is about to shutdown ...\n",
                       pSession->StartupInfo.uSessionID);

    /* Close all left guest files. */
    PVBOXSERVICECTRLFILE pFile;
    pFile = RTListGetFirst(&pSession->lstFiles, VBOXSERVICECTRLFILE, Node);
    while (pFile)
    {
        PVBOXSERVICECTRLFILE pNext = RTListNodeGetNext(&pFile->Node, VBOXSERVICECTRLFILE, Node);
        bool fLast = RTListNodeIsLast(&pSession->lstFiles, &pFile->Node);

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

    AssertMsg(RTListIsEmpty(&pSession->lstFiles),
              ("Guest file list still contains entries when it should not\n"));

    /** @todo Add guest process termination here. Later. */

    return VINF_SUCCESS; /** @todo */
}

