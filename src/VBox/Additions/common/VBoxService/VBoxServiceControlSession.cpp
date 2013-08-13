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
static int                  gstcntlSessionFileAdd(PVBOXSERVICECTRLSESSION pSession, PVBOXSERVICECTRLFILE pFile);
static PVBOXSERVICECTRLFILE gstcntlSessionFileGetLocked(const PVBOXSERVICECTRLSESSION pSession, uint32_t uHandle);
static DECLCALLBACK(int)    gstcntlSessionThread(RTTHREAD ThreadSelf, void *pvUser);
/* Host -> Guest handlers. */
static int                  gstcntlSessionHandleFileOpen(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleFileClose(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleFileRead(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleFileWrite(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf);
static int                  gstcntlSessionHandleFileSeek(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleFileTell(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleProcExec(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleProcInput(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf);
static int                  gstcntlSessionHandleProcOutput(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleProcTerminate(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);
static int                  gstcntlSessionHandleProcWaitFor(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx);


/** Generic option indices for session fork arguments. */
enum
{
    VBOXSERVICESESSIONOPT_FIRST = 1000, /* For initialization. */
#ifdef DEBUG
    VBOXSERVICESESSIONOPT_DUMP_STDOUT,
    VBOXSERVICESESSIONOPT_DUMP_STDERR,
#endif
    VBOXSERVICESESSIONOPT_LOG_FILE,
    VBOXSERVICESESSIONOPT_USERNAME,
    VBOXSERVICESESSIONOPT_SESSION_ID,
    VBOXSERVICESESSIONOPT_SESSION_PROTO,
    VBOXSERVICESESSIONOPT_THREAD_ID
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


static PVBOXSERVICECTRLFILE gstcntlSessionFileGetLocked(const PVBOXSERVICECTRLSESSION pSession,
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


static int gstcntlSessionHandleFileOpen(PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLCMDCTX pHostCtx)
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
        int rc2 = VbglR3GuestCtrlFileCbOpen(pHostCtx, rc, uHandle);
        if (RT_FAILURE(rc2))
            VBoxServiceError("[File %s]: Failed to report file open status, rc=%Rrc\n",
                             szFile, rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


static int gstcntlSessionHandleFileClose(const PVBOXSERVICECTRLSESSION pSession,
                                         PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;

    int rc = VbglR3GuestCtrlFileGetClose(pHostCtx, &uHandle /* File handle to close */);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            rc = gstcntlSessionFileDestroy(pFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        int rc2 = VbglR3GuestCtrlFileCbClose(pHostCtx, rc);
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file close status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


static int gstcntlSessionHandleFileRead(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLCMDCTX pHostCtx,
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

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
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
        int rc2 = VbglR3GuestCtrlFileCbRead(pHostCtx, rc, pvDataRead, (uint32_t)cbRead);
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
                                          PVBGLR3GUESTCTRLCMDCTX pHostCtx,
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

        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
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
        int rc2 = VbglR3GuestCtrlFileCbRead(pHostCtx, rc, pvDataRead, (uint32_t)cbRead);
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
                                         PVBGLR3GUESTCTRLCMDCTX pHostCtx,
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
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWrite(pFile->hFile, pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        int rc2 = VbglR3GuestCtrlFileCbWrite(pHostCtx, rc, (uint32_t)cbWritten);
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileWriteAt(const PVBOXSERVICECTRLSESSION pSession,
                                           PVBGLR3GUESTCTRLCMDCTX pHostCtx,
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
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWriteAt(pFile->hFile, iOffset,
                               pvScratchBuf, cbScratchBuf, &cbWritten);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        int rc2 = VbglR3GuestCtrlFileCbWrite(pHostCtx, rc, (uint32_t)cbWritten);
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file write status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileSeek(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLCMDCTX pHostCtx)
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
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
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
        int rc2 = VbglR3GuestCtrlFileCbSeek(pHostCtx, rc, uOffsetActual);
        if (RT_FAILURE(rc2))
            VBoxServiceError("Failed to report file seek status, rc=%Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


static int gstcntlSessionHandleFileTell(const PVBOXSERVICECTRLSESSION pSession,
                                        PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uHandle;
    uint64_t uOffsetActual = 0;

    int rc = VbglR3GuestCtrlFileGetTell(pHostCtx, &uHandle);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = gstcntlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            uOffsetActual = RTFileTell(pFile->hFile);
        }
        else
            rc = VERR_NOT_FOUND;

        /* Report back in any case. */
        int rc2 = VbglR3GuestCtrlFileCbTell(pHostCtx, rc, uOffsetActual);
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
 * @param   pHostCtx        Host context.
 */
int gstcntlSessionHandleProcExec(PVBOXSERVICECTRLSESSION pSession,
                                 PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    bool fStartAllowed = false; /* Flag indicating whether starting a process is allowed or not. */

    switch (pHostCtx->uProtocol)
    {
        case 1: /* Guest Additions < 4.3. */
            if (pHostCtx->uNumParms != 11)
                rc = VERR_NOT_SUPPORTED;
            break;

        case 2: /* Guest Additions >= 4.3. */
            if (pHostCtx->uNumParms != 12)
                rc = VERR_NOT_SUPPORTED;
            break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        VBOXSERVICECTRLPROCSTARTUPINFO startupInfo;
        RT_ZERO(startupInfo);

        /* Initialize maximum environment block size -- needed as input
         * parameter to retrieve the stuff from the host. On output this then
         * will contain the actual block size. */
        startupInfo.cbEnv = sizeof(startupInfo.szEnv);

        rc = VbglR3GuestCtrlProcGetStart(pHostCtx,
                                         /* Command */
                                         startupInfo.szCmd,      sizeof(startupInfo.szCmd),
                                         /* Flags */
                                         &startupInfo.uFlags,
                                         /* Arguments */
                                         startupInfo.szArgs,     sizeof(startupInfo.szArgs),    &startupInfo.uNumArgs,
                                         /* Environment */
                                         startupInfo.szEnv,      &startupInfo.cbEnv,            &startupInfo.uNumEnvVars,
                                         /* Credentials; for hosts with VBox < 4.3 (protocol version 1).
                                          * For protocl v2 and up the credentials are part of the session
                                          * opening call. */
                                         startupInfo.szUser,     sizeof(startupInfo.szUser),
                                         startupInfo.szPassword, sizeof(startupInfo.szPassword),
                                         /* Timeout (in ms) */
                                         &startupInfo.uTimeLimitMS,
                                         /* Process priority */
                                         &startupInfo.uPriority,
                                         /* Process affinity */
                                         startupInfo.uAffinity,  sizeof(startupInfo.uAffinity), &startupInfo.uNumAffinity);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Request to start process szCmd=%s, uFlags=0x%x, szArgs=%s, szEnv=%s, uTimeout=%RU32\n",
                               startupInfo.szCmd, startupInfo.uFlags,
                               startupInfo.uNumArgs ? startupInfo.szArgs : "<None>",
                               startupInfo.uNumEnvVars ? startupInfo.szEnv : "<None>",
                               startupInfo.uTimeLimitMS);

            rc = GstCntlSessionProcessStartAllowed(pSession, &fStartAllowed);
            if (RT_SUCCESS(rc))
            {
                if (fStartAllowed)
                {
                    rc = GstCntlProcessStart(pSession, &startupInfo, pHostCtx->uContextID);
                }
                else
                    rc = VERR_MAX_PROCS_REACHED; /* Maximum number of processes reached. */
            }
        }
    }

    /* In case of an error we need to notify the host to not wait forever for our response. */
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Starting process failed with rc=%Rrc, protocol=%RU32, parameters=%RU32\n",
                         rc, pHostCtx->uProtocol, pHostCtx->uNumParms);

        /* Don't report back if we didn't supply sufficient buffer for getting
         * the actual command -- we don't have the matching context ID. */
        if (rc != VERR_TOO_MUCH_DATA)
        {
            /*
             * Note: The context ID can be 0 because we mabye weren't able to fetch the command
             *       from the host. The host in case has to deal with that!
             */
            int rc2 = VbglR3GuestCtrlProcCbStatus(pHostCtx, 0 /* PID, invalid */,
                                                  PROC_STS_ERROR, rc,
                                                  NULL /* pvData */, 0 /* cbData */);
            if (RT_FAILURE(rc2))
                VBoxServiceError("Error sending start process status to host, rc=%Rrc\n", rc2);
        }
    }

    return rc;
}


/**
 * Sends stdin input to a specific guest process.
 *
 * @returns IPRT status code.
 * @param pSession            The session which is in charge.
 * @param pHostCtx            The host context to use.
 * @param pvScratchBuf        The scratch buffer.
 * @param cbScratchBuf        The scratch buffer size for retrieving the input data.
 */
int gstcntlSessionHandleProcInput(PVBOXSERVICECTRLSESSION pSession,
                                  PVBGLR3GUESTCTRLCMDCTX pHostCtx,
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
        VBoxServiceError("Failed to retrieve process input command for PID=%RU32, rc=%Rrc\n",
                         uPID, rc);
    }
    else if (cbSize > cbScratchBuf)
    {
        VBoxServiceError("Too much process input received, rejecting: uPID=%RU32, cbSize=%RU32, cbScratchBuf=%RU32\n",
                         uPID, cbSize, cbScratchBuf);
        rc = VERR_TOO_MUCH_DATA;
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
#ifdef DEBUG_andy
            VBoxServiceVerbose(4, "Got last process input block for PID=%RU32 (%RU32 bytes) ...\n",
                               uPID, cbSize);
#endif
        }

        PVBOXSERVICECTRLPROCESS pProcess = GstCntlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = GstCntlProcessHandleInput(pProcess, pHostCtx, fPendingClose,
                                           pvScratchBuf, cbSize);
            if (RT_FAILURE(rc))
                VBoxServiceError("Error handling input command for PID=%RU32, rc=%Rrc\n",
                                 uPID, rc);
            GstCntlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;
    }


#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "Setting input for PID=%RU32 resulted in rc=%Rrc\n",
                       uPID, rc);
#endif
    return rc;
}


/**
 * Gets stdout/stderr output of a specific guest process.
 *
 * @return IPRT status code.
 * @param pSession            The session which is in charge.
 * @param pHostCtx            The host context to use.
 */
int gstcntlSessionHandleProcOutput(PVBOXSERVICECTRLSESSION pSession,
                                   PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlProcGetOutput(pHostCtx, &uPID, &uHandleID, &uFlags);
#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "Getting output for PID=%RU32, CID=%RU32, uHandleID=%RU32, uFlags=%RU32\n",
                       uPID, pHostCtx->uContextID, uHandleID, uFlags);
#endif
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLPROCESS pProcess = GstCntlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = GstCntlProcessHandleOutput(pProcess, pHostCtx,
                                            uHandleID, _64K /* cbToRead */, uFlags);
            if (RT_FAILURE(rc))
                VBoxServiceError("Error getting output for PID=%RU32, rc=%Rrc\n",
                                 uPID, rc);
            GstCntlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;
    }

#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "Getting output for PID=%RU32 resulted in rc=%Rrc\n",
                       uPID, rc);
#endif
    return rc;
}


/**
 * Tells a guest process to terminate.
 *
 * @return  IPRT status code.
 * @param pSession            The session which is in charge.
 * @param pHostCtx            The host context to use.
 */
int gstcntlSessionHandleProcTerminate(const PVBOXSERVICECTRLSESSION pSession,
                                      PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    int rc = VbglR3GuestCtrlProcGetTerminate(pHostCtx, &uPID);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLPROCESS pProcess = GstCntlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = GstCntlProcessHandleTerm(pProcess);

            GstCntlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;
    }

#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "Terminating PID=%RU32 resulted in rc=%Rrc\n",
                       uPID, rc);
#endif
    return rc;
}


int gstcntlSessionHandleProcWaitFor(const PVBOXSERVICECTRLSESSION pSession,
                                    PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    uint32_t uPID;
    uint32_t uWaitFlags; uint32_t uTimeoutMS;

    int rc = VbglR3GuestCtrlProcGetWaitFor(pHostCtx, &uPID, &uWaitFlags, &uTimeoutMS);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLPROCESS pProcess = GstCntlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = VERR_NOT_IMPLEMENTED; /** @todo */
            GstCntlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;
    }

    return rc;
}


int GstCntlSessionHandler(PVBOXSERVICECTRLSESSION pSession,
                          uint32_t uMsg, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                          void *pvScratchBuf, size_t cbScratchBuf,
                          volatile bool *pfShutdown)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pfShutdown, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    /**
     * Only anonymous sessions (that is, sessions which run with local
     * service privileges) or forked session processes can do certain
     * operations.
     */
    bool fImpersonated = (   pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_FORK
                          || pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_ANONYMOUS);

    switch (uMsg)
    {
        case HOST_CANCEL_PENDING_WAITS:
            VBoxServiceVerbose(1, "We were asked to quit ...\n");
            /* Fall thru is intentional. */
        case HOST_SESSION_CLOSE:
            /* Shutdown this fork. */
            rc = GstCntlSessionClose(pSession);
            *pfShutdown = true; /* Shutdown in any case. */
            break;

        case HOST_EXEC_CMD:
            rc = gstcntlSessionHandleProcExec(pSession, pHostCtx);
            break;

        case HOST_EXEC_SET_INPUT:
            rc = gstcntlSessionHandleProcInput(pSession, pHostCtx,
                                               pvScratchBuf, cbScratchBuf);
            break;

        case HOST_EXEC_GET_OUTPUT:
            rc = gstcntlSessionHandleProcOutput(pSession, pHostCtx);
            break;

        case HOST_EXEC_TERMINATE:
            rc = gstcntlSessionHandleProcTerminate(pSession, pHostCtx);
            break;

        case HOST_EXEC_WAIT_FOR:
            rc = gstcntlSessionHandleProcWaitFor(pSession, pHostCtx);
            break;

        case HOST_FILE_OPEN:
            rc = fImpersonated
               ? gstcntlSessionHandleFileOpen(pSession, pHostCtx)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_CLOSE:
            rc = fImpersonated
               ? gstcntlSessionHandleFileClose(pSession, pHostCtx)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_READ:
            rc = fImpersonated
               ? gstcntlSessionHandleFileRead(pSession, pHostCtx,
                                              pvScratchBuf, cbScratchBuf)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_READ_AT:
            rc = fImpersonated
               ? gstcntlSessionHandleFileReadAt(pSession, pHostCtx,
                                                pvScratchBuf, cbScratchBuf)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_WRITE:
            rc = fImpersonated
               ? gstcntlSessionHandleFileWrite(pSession, pHostCtx,
                                               pvScratchBuf, cbScratchBuf)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_WRITE_AT:
            rc = fImpersonated
               ? gstcntlSessionHandleFileWriteAt(pSession, pHostCtx,
                                                 pvScratchBuf, cbScratchBuf)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_SEEK:
            rc = fImpersonated
               ? gstcntlSessionHandleFileSeek(pSession, pHostCtx)
               : VERR_NOT_SUPPORTED;
            break;

        case HOST_FILE_TELL:
            rc = fImpersonated
               ? gstcntlSessionHandleFileTell(pSession, pHostCtx)
               : VERR_NOT_SUPPORTED;
            break;

        default:
            rc = VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID);
            VBoxServiceVerbose(3, "Unsupported message (uMsg=%RU32, cParms=%RU32) from host, skipping\n",
                               uMsg, pHostCtx->uNumParms);
            break;
    }

    return rc;
}


/**
 * Thread main routine for a forked guest session process.
 * This thread runs in the main executable to control the forked
 * session process.
 *
 * @return IPRT status code.
 * @param  RTTHREAD             Pointer to the thread's data.
 * @param  void*                User-supplied argument pointer.
 *
 */
static DECLCALLBACK(int) gstcntlSessionThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLSESSIONTHREAD pThread = (PVBOXSERVICECTRLSESSIONTHREAD)pvUser;
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    uint32_t uSessionID = pThread->StartupInfo.uSessionID;

    uint32_t uClientID;
    int rc = VbglR3GuestCtrlConnect(&uClientID);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "Session ID=%RU32 thread running, client ID=%RU32\n",
                           uSessionID, uClientID);

        /* The session thread is not interested in receiving any commands;
         * tell the host service. */
        rc = VbglR3GuestCtrlMsgFilterSet(uClientID, 0 /* Skip all */,
                                         0 /* Filter mask to add */, 0 /* Filter mask to remove */);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("Unable to set message filter, rc=%Rrc\n", rc);
            /* Non-critical. */
            rc = VINF_SUCCESS;
        }
    }
    else
        VBoxServiceError("Error connecting to guest control service, rc=%Rrc\n", rc);

    if (RT_FAILURE(rc))
        pThread->fShutdown = true;

    /* Let caller know that we're done initializing, regardless of the result. */
    int rc2 = RTThreadUserSignal(RTThreadSelf());
    AssertRC(rc2);

    if (RT_FAILURE(rc))
        return rc;

    bool fProcessAlive = true;
    RTPROCSTATUS ProcessStatus;
    RT_ZERO(ProcessStatus);

    int rcWait;
    if (RT_SUCCESS(rc))
    {
        uint32_t uTimeoutsMS = 5 * 60 * 1000; /** @todo Make this configurable. Later. */
        uint64_t u64TimeoutStart = 0;

        for (;;)
        {
            rcWait = RTProcWaitNoResume(pThread->hProcess, RTPROCWAIT_FLAGS_NOBLOCK,
                                        &ProcessStatus);
            if (RT_UNLIKELY(rcWait == VERR_INTERRUPTED))
                continue;
            else if (   rcWait == VINF_SUCCESS
                     || rcWait == VERR_PROCESS_NOT_FOUND)
            {
                fProcessAlive = false;
                break;
            }
            else
                AssertMsgBreak(rcWait == VERR_PROCESS_RUNNING,
                               ("Got unexpected rc=%Rrc while waiting for session process termination\n", rcWait));

            if (ASMAtomicReadBool(&pThread->fShutdown))
            {
                if (!u64TimeoutStart)
                {
                    VBoxServiceVerbose(3, "Guest session ID=%RU32 thread was asked to terminate, waiting for session process to exit ...\n",
                                       uSessionID);
                    u64TimeoutStart = RTTimeMilliTS();
                    continue; /* Don't waste time on waiting. */
                }
                if (RTTimeMilliTS() - u64TimeoutStart > uTimeoutsMS)
                {
                     VBoxServiceVerbose(3, "Guest session ID=%RU32 process did not shut down within time\n",
                                        uSessionID);
                     break;
                }
            }

            RTThreadSleep(100); /* Wait a bit. */
        }

        if (!fProcessAlive)
        {
            VBoxServiceVerbose(2, "Guest session ID=%RU32 process terminated with rc=%Rrc, reason=%ld, status=%d\n",
                               uSessionID, rcWait,
                               ProcessStatus.enmReason, ProcessStatus.iStatus);
        }
    }

    uint32_t uSessionStatus = GUEST_SESSION_NOTIFYTYPE_UNDEFINED;
    uint32_t uSessionRc = VINF_SUCCESS; /** uint32_t vs. int. */

    if (fProcessAlive)
    {
        for (int i = 0; i < 3; i++)
        {
            VBoxServiceVerbose(2, "Guest session ID=%RU32 process still alive, killing attempt %d/3\n",
                               uSessionID, i + 1);

            rc = RTProcTerminate(pThread->hProcess);
            if (RT_SUCCESS(rc))
                break;
            RTThreadSleep(3000);
        }

        VBoxServiceVerbose(2, "Guest session ID=%RU32 process termination resulted in rc=%Rrc\n",
                           uSessionID, rc);

        uSessionStatus = RT_SUCCESS(rc)
                       ? GUEST_SESSION_NOTIFYTYPE_TOK : GUEST_SESSION_NOTIFYTYPE_TOA;
    }
    else
    {
        if (RT_SUCCESS(rcWait))
        {
            switch (ProcessStatus.enmReason)
            {
                case RTPROCEXITREASON_NORMAL:
                    uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEN;
                    break;

                case RTPROCEXITREASON_ABEND:
                    uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEA;
                    break;

                case RTPROCEXITREASON_SIGNAL:
                    uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TES;
                    break;

                default:
                    AssertMsgFailed(("Unhandled process termination reason (%ld)\n",
                                     ProcessStatus.enmReason));
                    uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEA;
                    break;
            }
        }
        else
        {
            /* If we didn't find the guest process anymore, just assume it
             * terminated normally. */
            uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEN;
        }
    }

    VBoxServiceVerbose(3, "Guest session ID=%RU32 thread ended with sessionStatus=%RU32, sessionRc=%Rrc\n",
                       uSessionID, uSessionStatus, uSessionRc);

    /* Report final status. */
    Assert(uSessionStatus != GUEST_SESSION_NOTIFYTYPE_UNDEFINED);
    VBGLR3GUESTCTRLCMDCTX ctx = { uClientID, VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(uSessionID) };
    rc2 = VbglR3GuestCtrlSessionNotify(&ctx,
                                       uSessionStatus, uSessionRc);
    if (RT_FAILURE(rc2))
        VBoxServiceError("Reporting session ID=%RU32 final status failed with rc=%Rrc\n",
                         uSessionID, rc2);

    VbglR3GuestCtrlDisconnect(uClientID);

    VBoxServiceVerbose(3, "Session ID=%RU32 thread ended with rc=%Rrc\n",
                       uSessionID, rc);
    return rc;
}


RTEXITCODE gstcntlSessionForkWorker(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, RTEXITCODE_FAILURE);

    bool fSessionFilter = true;

    VBoxServiceVerbose(0, "Hi, this is guest session ID=%RU32\n",
                       pSession->StartupInfo.uSessionID);

    uint32_t uClientID;
    int rc = VbglR3GuestCtrlConnect(&uClientID);
    if (RT_SUCCESS(rc))
    {
        /* Set session filter. This prevents the guest control
         * host service to send messages which belong to another
         * session we don't want to handle. */
        uint32_t uFilterAdd =
            VBOX_GUESTCTRL_FILTER_BY_SESSION(pSession->StartupInfo.uSessionID);
        rc = VbglR3GuestCtrlMsgFilterSet(uClientID,
                                         VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(pSession->StartupInfo.uSessionID),
                                         uFilterAdd, 0 /* Filter remove */);
        VBoxServiceVerbose(3, "Setting message filterAdd=0x%x returned %Rrc\n",
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

    /* Report started status. */
    VBGLR3GUESTCTRLCMDCTX ctx = { uClientID, VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(pSession->StartupInfo.uSessionID) };
    int rc2 = VbglR3GuestCtrlSessionNotify(&ctx,
                                           GUEST_SESSION_NOTIFYTYPE_STARTED, VINF_SUCCESS);
    if (RT_FAILURE(rc2))
        VBoxServiceError("Reporting session ID=%RU32 started status failed with rc=%Rrc\n",
                         pSession->StartupInfo.uSessionID, rc2);

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

        VBGLR3GUESTCTRLCMDCTX ctxHost = { uClientID, 0 /* Context ID, zeroed */,
                                          pSession->StartupInfo.uProtocol };
        for (;;)
        {
            VBoxServiceVerbose(3, "Waiting for host msg ...\n");
            uint32_t uMsg = 0;
            uint32_t cParms = 0;
            rc = VbglR3GuestCtrlMsgWaitFor(uClientID, &uMsg, &cParms);
            if (rc == VERR_TOO_MUCH_DATA)
            {
#ifdef DEBUG
                VBoxServiceVerbose(4, "Message requires %RU32 parameters, but only 2 supplied -- retrying request (no error!)...\n", cParms);
#endif
                rc = VINF_SUCCESS; /* Try to get "real" message in next block below. */
            }
            else if (RT_FAILURE(rc))
                VBoxServiceVerbose(3, "Getting host message failed with %Rrc\n", rc); /* VERR_GEN_IO_FAILURE seems to be normal if ran into timeout. */
            if (RT_SUCCESS(rc))
            {
#ifdef DEBUG
                VBoxServiceVerbose(3, "Msg=%RU32 (%RU32 parms) retrieved\n", uMsg, cParms);
#endif
                /* Set number of parameters for current host context. */
                ctxHost.uNumParms = cParms;

                /* ... and pass it on to the session handler. */
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
 * Finds a (formerly) started guest process given by its PID and increases
 * its reference count. Must be decreased by the caller with GstCntlProcessRelease().
 * Note: This does *not lock the process!
 *
 * @return  PVBOXSERVICECTRLTHREAD      Guest process if found, otherwise NULL.
 * @param   PVBOXSERVICECTRLSESSION     Pointer to guest session where to search process in.
 * @param   uPID                        PID to search for.
 */
PVBOXSERVICECTRLPROCESS GstCntlSessionRetainProcess(PVBOXSERVICECTRLSESSION pSession, uint32_t uPID)
{
    AssertPtrReturn(pSession, NULL);

    PVBOXSERVICECTRLPROCESS pProcess = NULL;
    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLPROCESS pCurProcess;
        RTListForEach(&pSession->lstProcesses, pCurProcess, VBOXSERVICECTRLPROCESS, Node)
        {
            if (pCurProcess->uPID == uPID)
            {
                rc = RTCritSectEnter(&pCurProcess->CritSect);
                if (RT_SUCCESS(rc))
                {
                    pCurProcess->cRefs++;
                    rc = RTCritSectLeave(&pCurProcess->CritSect);
                    AssertRC(rc);
                }

                if (RT_SUCCESS(rc))
                    pProcess = pCurProcess;
                break;
            }
        }

        rc = RTCritSectLeave(&pSession->CritSect);
        AssertRC(rc);
    }

    return pProcess;
}


int GstCntlSessionClose(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    VBoxServiceVerbose(0, "Session %RU32 is about to close ...\n",
                       pSession->StartupInfo.uSessionID);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        /*
         * Close all guest processes.
         */
        VBoxServiceVerbose(0, "Stopping all guest processes ...\n");

        /* Signal all guest processes in the active list that we want to shutdown. */
        size_t cProcesses = 0;
        PVBOXSERVICECTRLPROCESS pProcess;
        RTListForEach(&pSession->lstProcesses, pProcess, VBOXSERVICECTRLPROCESS, Node)
        {
            GstCntlProcessStop(pProcess);
            cProcesses++;
        }

        VBoxServiceVerbose(1, "%zu guest processes were signalled to stop\n", cProcesses);

        /* Wait for all active threads to shutdown and destroy the active thread list. */
        pProcess = RTListGetFirst(&pSession->lstProcesses, VBOXSERVICECTRLPROCESS, Node);
        while (pProcess)
        {
            PVBOXSERVICECTRLPROCESS pNext = RTListNodeGetNext(&pProcess->Node, VBOXSERVICECTRLPROCESS, Node);
            bool fLast = RTListNodeIsLast(&pSession->lstProcesses, &pProcess->Node);

            int rc2 = RTCritSectLeave(&pSession->CritSect);
            AssertRC(rc2);

            rc2 = GstCntlProcessWait(pProcess,
                                     30 * 1000 /* Wait 30 seconds max. */,
                                     NULL /* rc */);

            int rc3 = RTCritSectEnter(&pSession->CritSect);
            AssertRC(rc3);

            if (RT_SUCCESS(rc2))
                GstCntlProcessFree(pProcess);

            if (fLast)
                break;

            pProcess = pNext;
        }

#ifdef DEBUG
        pProcess = RTListGetFirst(&pSession->lstProcesses, VBOXSERVICECTRLPROCESS, Node);
        while (pProcess)
        {
            PVBOXSERVICECTRLPROCESS pNext = RTListNodeGetNext(&pProcess->Node, VBOXSERVICECTRLPROCESS, Node);
            bool fLast = RTListNodeIsLast(&pSession->lstProcesses, &pProcess->Node);

            VBoxServiceVerbose(1, "Process %p (PID %RU32) still in list\n",
                               pProcess, pProcess->uPID);
            if (fLast)
                break;

            pProcess = pNext;
        }
#endif
        AssertMsg(RTListIsEmpty(&pSession->lstProcesses),
                  ("Guest process list still contains entries when it should not\n"));

        /*
         * Close all left guest files.
         */
        VBoxServiceVerbose(0, "Closing all guest files ...\n");

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
                if (RT_SUCCESS(rc))
                    rc = rc2;
                /* Keep going. */
            }

            if (fLast)
                break;

            pFile = pNext;
        }

        AssertMsg(RTListIsEmpty(&pSession->lstFiles),
                  ("Guest file list still contains entries when it should not\n"));

        int rc2 = RTCritSectLeave(&pSession->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


int GstCntlSessionDestroy(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    int rc = GstCntlSessionClose(pSession);

    /* Destroy critical section. */
    RTCritSectDelete(&pSession->CritSect);

    return rc;
}


int GstCntlSessionInit(PVBOXSERVICECTRLSESSION pSession, uint32_t uFlags)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    RTListInit(&pSession->lstProcesses);
    RTListInit(&pSession->lstFiles);

    pSession->uFlags = uFlags;

    /* Init critical section for protecting the thread lists. */
    int rc = RTCritSectInit(&pSession->CritSect);
    AssertRC(rc);

    return rc;
}


/**
 * Adds a guest process to a session's process list.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session to add process to.
 * @param   pProcess                Guest process to add.
 */
int GstCntlSessionProcessAdd(PVBOXSERVICECTRLSESSION pSession,
                             PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "Adding process (PID %RU32) to session ID=%RU32\n",
                           pProcess->uPID, pSession->StartupInfo.uSessionID);

        /* Add process to session list. */
        /* rc = */ RTListAppend(&pSession->lstProcesses, &pProcess->Node);

        int rc2 = RTCritSectLeave(&pSession->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return VINF_SUCCESS;
}


/**
 * Removes a guest process from a session's process list.
 *
 * @return  IPRT status code.
 * @param   pSession                Guest session to remove process from.
 * @param   pProcess                Guest process to remove.
 */
int GstCntlSessionProcessRemove(PVBOXSERVICECTRLSESSION pSession,
                                PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "Removing process (PID %RU32) from session ID=%RU32\n",
                           pProcess->uPID, pSession->StartupInfo.uSessionID);
        Assert(pProcess->cRefs == 0);

        RTListNodeRemove(&pProcess->Node);

        int rc2 = RTCritSectLeave(&pSession->CritSect);
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

    int rc = RTCritSectEnter(&pSession->CritSect);
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
            PVBOXSERVICECTRLPROCESS pProcess;
            RTListForEach(&pSession->lstProcesses, pProcess, VBOXSERVICECTRLPROCESS, Node)
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

        int rc2 = RTCritSectLeave(&pSession->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


/**
 * Creates a guest session. This will spawn a new VBoxService.exe instance under
 * behalf of the given user which then will act as a session host. On successful
 * open, the session will be added to the given session thread list.
 *
 * @return  IPRT status code.
 * @param   pList                   Which list to use to store the session thread in.
 * @param   pSessionStartupInfo     Session startup info.
 * @param   ppSessionThread         Returns newly created session thread on success.
 *                                  Optional.
 */
int GstCntlSessionThreadCreate(PRTLISTANCHOR pList,
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
            AssertMsgFailed(("Guest session thread ID=%RU32 (%p) already exists when it should not\n",
                             pSessionCur->StartupInfo.uSessionID, pSessionCur));
            return VERR_ALREADY_EXISTS;
        }
    }
#endif
    int rc = VINF_SUCCESS;

    /* Static counter to help tracking session thread <-> process relations. */
    static uint32_t s_uCtrlSessionThread = 0;
    if (s_uCtrlSessionThread++ == UINT32_MAX)
        s_uCtrlSessionThread = 0; /* Wrap around to not let IPRT freak out. */

    PVBOXSERVICECTRLSESSIONTHREAD pSessionThread =
        (PVBOXSERVICECTRLSESSIONTHREAD)RTMemAllocZ(sizeof(VBOXSERVICECTRLSESSIONTHREAD));
    if (pSessionThread)
    {
        /* Copy over session startup info. */
        memcpy(&pSessionThread->StartupInfo, pSessionStartupInfo,
               sizeof(VBOXSERVICECTRLSESSIONSTARTUPINFO));

        pSessionThread->fShutdown = false;
        pSessionThread->fStarted  = false;
        pSessionThread->fStopped  = false;

        /* Is this an anonymous session? */
        /* Anonymous sessions run with the same privileges as the main VBoxService executable. */
        bool fAnonymous = !RT_BOOL(strlen(pSessionThread->StartupInfo.szUser));
        if (fAnonymous)
        {
            Assert(!strlen(pSessionThread->StartupInfo.szPassword));
            Assert(!strlen(pSessionThread->StartupInfo.szDomain));

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

        rc = RTCritSectInit(&pSessionThread->CritSect);
        AssertRC(rc);

        /* Fork child doing the actual session handling. */
        char szExeName[RTPATH_MAX];
        char *pszExeName = RTProcGetExecutablePath(szExeName, sizeof(szExeName));
        if (pszExeName)
        {
            char szParmUserName[GUESTPROCESS_MAX_USER_LEN + 32];
            if (!fAnonymous)
            {
                if (!RTStrPrintf(szParmUserName, sizeof(szParmUserName), "--user=%s", pSessionThread->StartupInfo.szUser))
                    rc = VERR_BUFFER_OVERFLOW;
            }
            char szParmSessionID[32];
            if (RT_SUCCESS(rc) && !RTStrPrintf(szParmSessionID, sizeof(szParmSessionID), "--session-id=%RU32",
                                               pSessionThread->StartupInfo.uSessionID))
            {
                rc = VERR_BUFFER_OVERFLOW;
            }
            char szParmSessionProto[32];
            if (RT_SUCCESS(rc) && !RTStrPrintf(szParmSessionProto, sizeof(szParmSessionProto), "--session-proto=%RU32",
                                               pSessionThread->StartupInfo.uProtocol))
            {
                rc = VERR_BUFFER_OVERFLOW;
            }
#ifdef DEBUG
            char szParmThreadId[32];
            if (RT_SUCCESS(rc) && !RTStrPrintf(szParmThreadId, sizeof(szParmThreadId), "--thread-id=%RU32",
                                               s_uCtrlSessionThread))
            {
                rc = VERR_BUFFER_OVERFLOW;
            }
#endif /* DEBUG */
            if (RT_SUCCESS(rc))
            {
                int iOptIdx = 0; /* Current index in argument vector. */

                char const *papszArgs[16];
                papszArgs[iOptIdx++] = pszExeName;
                papszArgs[iOptIdx++] = "guestsession";
                papszArgs[iOptIdx++] = szParmSessionID;
                papszArgs[iOptIdx++] = szParmSessionProto;
#ifdef DEBUG
                papszArgs[iOptIdx++] = szParmThreadId;
#endif /* DEBUG */
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
#ifndef DEBUG
                        if (RTStrAPrintf(&pszLogSuffix, "-%RU32-%s",
                                         pSessionStartupInfo->uSessionID,
                                         pSessionStartupInfo->szUser) < 0)
                        {
                            rc2 = VERR_NO_MEMORY;
                        }
#else
                        if (RTStrAPrintf(&pszLogSuffix, "-%RU32-%RU32-%s",
                                         pSessionStartupInfo->uSessionID,
                                         s_uCtrlSessionThread,
                                         pSessionStartupInfo->szUser) < 0)
                        {
                            rc2 = VERR_NO_MEMORY;
                        }
#endif /* DEBUG */
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

                    rc = rc2;
                }
                else if (RT_FAILURE(rc2))
                    rc = rc2;
#ifdef DEBUG
                VBoxServiceVerbose(4, "rc=%Rrc, session flags=%x\n",
                                   rc, g_Session.uFlags);
                char szParmDumpStdOut[32];
                if (   RT_SUCCESS(rc)
                    && g_Session.uFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT)
                {
                    if (!RTStrPrintf(szParmDumpStdOut, sizeof(szParmDumpStdOut), "--dump-stdout"))
                        rc = VERR_BUFFER_OVERFLOW;
                    if (RT_SUCCESS(rc))
                        papszArgs[iOptIdx++] = szParmDumpStdOut;
                }
                char szParmDumpStdErr[32];
                if (   RT_SUCCESS(rc)
                    && g_Session.uFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR)
                {
                    if (!RTStrPrintf(szParmDumpStdErr, sizeof(szParmDumpStdErr), "--dump-stderr"))
                        rc = VERR_BUFFER_OVERFLOW;
                    if (RT_SUCCESS(rc))
                        papszArgs[iOptIdx++] = szParmDumpStdErr;
                }
#endif
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
                if (RT_SUCCESS(rc))
                    rc = RTFileOpenBitBucket(&hStdIn.u.hFile, RTFILE_O_READ);
                if (RT_SUCCESS(rc))
                {
                    hStdIn.enmType = RTHANDLETYPE_FILE;

                    RTHANDLE hStdOutAndErr;
                    rc = RTFileOpenBitBucket(&hStdOutAndErr.u.hFile, RTFILE_O_WRITE);
                    if (RT_SUCCESS(rc))
                    {
                        hStdOutAndErr.enmType = RTHANDLETYPE_FILE;

                        /** @todo Set custom/cloned guest session environment block. */
                        rc = RTProcCreateEx(pszExeName, papszArgs, RTENV_DEFAULT, uProcFlags,
                                            &hStdIn, &hStdOutAndErr, &hStdOutAndErr,
                                            !fAnonymous ? pSessionThread->StartupInfo.szUser : NULL,
                                            !fAnonymous ? pSessionThread->StartupInfo.szPassword : NULL,
                                            &pSessionThread->hProcess);

                        RTFileClose(hStdOutAndErr.u.hFile);
                    }

                    RTFileClose(hStdIn.u.hFile);
                }
#endif
            }
        }
        else
            rc = VERR_FILE_NOT_FOUND;

        if (RT_SUCCESS(rc))
        {
            /* Start session thread. */
            rc = RTThreadCreateF(&pSessionThread->Thread, gstcntlSessionThread,
                                 pSessionThread /*pvUser*/, 0 /*cbStack*/,
                                 RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "sess%u", s_uCtrlSessionThread);
            if (RT_FAILURE(rc))
            {
                VBoxServiceError("Creating session thread failed, rc=%Rrc\n", rc);
            }
            else
            {
                /* Wait for the thread to initialize. */
                rc = RTThreadUserWait(pSessionThread->Thread, 60 * 1000 /* 60s timeout */);
                if (   ASMAtomicReadBool(&pSessionThread->fShutdown)
                    || RT_FAILURE(rc))
                {
                    VBoxServiceError("Thread for session ID=%RU32 failed to start, rc=%Rrc\n",
                                     pSessionThread->StartupInfo.uSessionID, rc);
                    if (RT_SUCCESS(rc))
                        rc = VERR_CANT_CREATE; /** @todo Find a better rc. */
                }
                else
                {
                    VBoxServiceVerbose(2, "Thread for session ID=%RU32 started\n",
                                       pSessionThread->StartupInfo.uSessionID);

                    ASMAtomicXchgBool(&pSessionThread->fStarted, true);

                    /* Add session to list. */
                    /* rc = */ RTListAppend(pList, &pSessionThread->Node);
                    if (ppSessionThread) /* Return session if wanted. */
                        *ppSessionThread = pSessionThread;
                }
            }
        }

        if (RT_FAILURE(rc))
        {
            RTMemFree(pSessionThread);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    VBoxServiceVerbose(3, "Forking session thread returned returned rc=%Rrc\n", rc);
    return rc;
}


/**
 * Waits for a formerly opened guest session process to close.
 *
 * @return  IPRT status code.
 * @param   pThread                 Guest session thread to wait for.
 * @param   uTimeoutMS              Waiting timeout (in ms).
 * @param   uFlags                  Closing flags.
 */
int GstCntlSessionThreadWait(PVBOXSERVICECTRLSESSIONTHREAD pThread,
                             uint32_t uTimeoutMS, uint32_t uFlags)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    /** @todo Validate closing flags. */

    if (pThread->Thread == NIL_RTTHREAD)
    {
        AssertMsgFailed(("Guest session thread of session %p does not exist when it should\n",
                         pThread));
        return VERR_NOT_FOUND;
    }

    int rc = VINF_SUCCESS;

    /*
     * The fork should have received the same closing request,
     * so just wait for the process to close.
     */
    if (ASMAtomicReadBool(&pThread->fStarted))
    {
        /* Ask the thread to shutdown. */
        ASMAtomicXchgBool(&pThread->fShutdown, true);

        VBoxServiceVerbose(3, "Waiting for session thread ID=%RU32 to close (%RU32ms) ...\n",
                           pThread->StartupInfo.uSessionID, uTimeoutMS);

        int rcThread;
        rc = RTThreadWait(pThread->Thread, uTimeoutMS, &rcThread);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("Waiting for session thread ID=%RU32 to close failed with rc=%Rrc\n",
                             pThread->StartupInfo.uSessionID, rc);
        }
        else
            VBoxServiceVerbose(3, "Session thread ID=%RU32 ended with rc=%Rrc\n",
                               pThread->StartupInfo.uSessionID, rcThread);
    }

    return rc;
}

/**
 * Waits for the specified session thread to end and remove
 * it from the session thread list.
 *
 * @return  IPRT status code.
 * @param   pThread                 Session thread to destroy.
 * @param   uFlags                  Closing flags.
 */
int GstCntlSessionThreadDestroy(PVBOXSERVICECTRLSESSIONTHREAD pThread, uint32_t uFlags)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    int rc = GstCntlSessionThreadWait(pThread,
                                      5 * 60 * 1000 /* 5 minutes timeout */, uFlags);

    /* Remove session from list and destroy object. */
    RTListNodeRemove(&pThread->Node);

    RTMemFree(pThread);
    pThread = NULL;

    return rc;
}

/**
 * Close all formerly opened guest session threads.
 * Note: Caller is responsible for locking!
 *
 * @return  IPRT status code.
 * @param   pList                   Which list to close the session threads for.
 * @param   uFlags                  Closing flags.
 */
int GstCntlSessionThreadDestroyAll(PRTLISTANCHOR pList, uint32_t uFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PVBOXSERVICECTRLSESSIONTHREAD pSessionThread
         = RTListGetFirst(pList, VBOXSERVICECTRLSESSIONTHREAD, Node);
    while (pSessionThread)
    {
        PVBOXSERVICECTRLSESSIONTHREAD pSessionThreadNext =
            RTListGetNext(pList, pSessionThread, VBOXSERVICECTRLSESSIONTHREAD, Node);
        bool fLast = RTListNodeIsLast(pList, &pSessionThread->Node);

        int rc2 = GstCntlSessionThreadDestroy(pSessionThread, uFlags);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("Closing session thread failed with rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
            /* Keep going. */
        }

        if (fLast)
            break;

        pSessionThread = pSessionThreadNext;
    }

    return rc;
}

RTEXITCODE VBoxServiceControlSessionForkInit(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
#ifdef DEBUG
        { "--dump-stdout",     VBOXSERVICESESSIONOPT_DUMP_STDOUT,     RTGETOPT_REQ_NOTHING },
        { "--dump-stderr",     VBOXSERVICESESSIONOPT_DUMP_STDERR,     RTGETOPT_REQ_NOTHING },
#endif
        { "--logfile",         VBOXSERVICESESSIONOPT_LOG_FILE,        RTGETOPT_REQ_STRING },
        { "--user",            VBOXSERVICESESSIONOPT_USERNAME,        RTGETOPT_REQ_STRING },
        { "--session-id",      VBOXSERVICESESSIONOPT_SESSION_ID,      RTGETOPT_REQ_UINT32 },
        { "--session-proto",   VBOXSERVICESESSIONOPT_SESSION_PROTO,   RTGETOPT_REQ_UINT32 },
#ifdef DEBUG
        { "--thread-id",       VBOXSERVICESESSIONOPT_THREAD_ID,       RTGETOPT_REQ_UINT32 },
#endif /* DEBUG */
        { "--verbose",         'v',                                   RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    uint32_t uSessionFlags = VBOXSERVICECTRLSESSION_FLAG_FORK;

    /* Protocol and session ID must be specified explicitly. */
    g_Session.StartupInfo.uProtocol  = UINT32_MAX;
    g_Session.StartupInfo.uSessionID = UINT32_MAX;

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
#ifdef DEBUG
            case VBOXSERVICESESSIONOPT_DUMP_STDOUT:
                uSessionFlags |= VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT;
                break;

            case VBOXSERVICESESSIONOPT_DUMP_STDERR:
                uSessionFlags |= VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR;
                break;
#endif
            case VBOXSERVICESESSIONOPT_USERNAME:
                /** @todo Information not needed right now, skip. */
                break;

            case VBOXSERVICESESSIONOPT_SESSION_ID:
                g_Session.StartupInfo.uSessionID = ValueUnion.u32;
                break;

            case VBOXSERVICESESSIONOPT_SESSION_PROTO:
                g_Session.StartupInfo.uProtocol = ValueUnion.u32;
                break;

            case VBOXSERVICESESSIONOPT_THREAD_ID:
                /* Not handled. */
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

    if (g_Session.StartupInfo.uProtocol == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No protocol version specified");

    if (g_Session.StartupInfo.uSessionID == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No session ID specified");

    /* Init the session object. */
    rc = GstCntlSessionInit(&g_Session, uSessionFlags);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize session object, rc=%Rrc\n", rc);

    rc = VBoxServiceLogCreate(strlen(g_szLogFile) ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create release log (%s, %Rrc)",
                              strlen(g_szLogFile) ? g_szLogFile : "<None>", rc);

    RTEXITCODE rcExit = gstcntlSessionForkWorker(&g_Session);

    VBoxServiceLogDestroy();
    return rcExit;
}

