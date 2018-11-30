/* $Id$ */
/** @file
 * VBoxServiceControlSession - Guest session handling. Also handles the spawned session processes.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
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
#include <iprt/rand.h>

#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServiceControl.h"

using namespace guestControl;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Generic option indices for session spawn arguments. */
enum
{
    VBOXSERVICESESSIONOPT_FIRST = 1000, /* For initialization. */
    VBOXSERVICESESSIONOPT_DOMAIN,
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


/**
 * Helper that grows the scratch buffer.
 * @returns Success indicator.
 */
static bool vgsvcGstCtrlSessionGrowScratchBuf(void **ppvScratchBuf, uint32_t *pcbScratchBuf, uint32_t cbMinBuf)
{
    uint32_t cbNew = *pcbScratchBuf * 2;
    if (   cbNew    <= VMMDEV_MAX_HGCM_DATA_SIZE
        && cbMinBuf <= VMMDEV_MAX_HGCM_DATA_SIZE)
    {
        while (cbMinBuf > cbNew)
            cbNew *= 2;
        void *pvNew = RTMemRealloc(*ppvScratchBuf, cbNew);
        if (pvNew)
        {
            *ppvScratchBuf = pvNew;
            *pcbScratchBuf = cbNew;
            return true;
        }
    }
    return false;
}



static int vgsvcGstCtrlSessionFileDestroy(PVBOXSERVICECTRLFILE pFile)
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


/** @todo No locking done yet! */
static PVBOXSERVICECTRLFILE vgsvcGstCtrlSessionFileGetLocked(const PVBOXSERVICECTRLSESSION pSession, uint32_t uHandle)
{
    AssertPtrReturn(pSession, NULL);

    /** @todo Use a map later! */
    PVBOXSERVICECTRLFILE pFileCur;
    RTListForEach(&pSession->lstFiles, pFileCur, VBOXSERVICECTRLFILE, Node)
    {
        if (pFileCur->uHandle == uHandle)
            return pFileCur;
    }

    return NULL;
}


static int vgsvcGstCtrlSessionHandleDirRemove(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the message.
     */
    char        szDir[RTPATH_MAX];
    uint32_t    fFlags; /* DIRREMOVE_FLAG_XXX */
    int rc = VbglR3GuestCtrlDirGetRemove(pHostCtx, szDir, sizeof(szDir), &fFlags);
    if (RT_SUCCESS(rc))
    {
        /*
         * Do some validating before executing the job.
         */
        if (!(fFlags & ~DIRREMOVE_FLAG_VALID_MASK))
        {
            if (fFlags & DIRREMOVE_FLAG_RECURSIVE)
            {
                uint32_t fFlagsRemRec = RTDIRRMREC_F_CONTENT_AND_DIR; /* Set default. */
                if (fFlags & DIRREMOVE_FLAG_CONTENT_ONLY)
                    fFlagsRemRec |= RTDIRRMREC_F_CONTENT_ONLY;
                rc = RTDirRemoveRecursive(szDir, fFlagsRemRec);
                VGSvcVerbose(4, "[Dir %s]: rmdir /s (%#x) -> rc=%Rrc\n", szDir, fFlags, rc);
            }
            else
            {
                /* Only delete directory if not empty. */
                rc = RTDirRemove(szDir);
                VGSvcVerbose(4, "[Dir %s]: rmdir (%#x), rc=%Rrc\n", szDir, fFlags, rc);
            }
        }
        else
        {
            VGSvcError("[Dir %s]: Unsupported flags: %#x (all %#x)\n", szDir, (fFlags & ~DIRREMOVE_FLAG_VALID_MASK), fFlags);
            rc = VERR_NOT_SUPPORTED;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlMsgReply(pHostCtx, rc);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("[Dir %s]: Failed to report removing status, rc=%Rrc\n", szDir, rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for rmdir operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }

    VGSvcVerbose(6, "Removing directory '%s' returned rc=%Rrc\n", szDir, rc);
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileOpen(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the message.
     */
    char     szFile[RTPATH_MAX];
    char     szAccess[64];
    char     szDisposition[64];
    char     szSharing[64];
    uint32_t uCreationMode = 0;
    uint64_t offOpen       = 0;
    uint32_t uHandle       = 0;
    int rc = VbglR3GuestCtrlFileGetOpen(pHostCtx,
                                        /* File to open. */
                                        szFile, sizeof(szFile),
                                        /* Open mode. */
                                        szAccess, sizeof(szAccess),
                                        /* Disposition. */
                                        szDisposition, sizeof(szDisposition),
                                        /* Sharing. */
                                        szSharing, sizeof(szSharing),
                                        /* Creation mode. */
                                        &uCreationMode,
                                        /* Offset. */
                                        &offOpen);
    VGSvcVerbose(4, "[File %s]: szAccess=%s, szDisposition=%s, szSharing=%s, offOpen=%RU64, rc=%Rrc\n",
                 szFile, szAccess, szDisposition, szSharing, offOpen, rc);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = (PVBOXSERVICECTRLFILE)RTMemAllocZ(sizeof(VBOXSERVICECTRLFILE));
        if (pFile)
        {
            pFile->hFile = NIL_RTFILE; /* Not zero or NULL! */
            if (szFile[0])
            {
                RTStrCopy(pFile->szName, sizeof(pFile->szName), szFile);

/** @todo
 * Implement szSharing!
 */
                uint64_t fFlags;
                rc = RTFileModeToFlagsEx(szAccess, szDisposition, NULL /* pszSharing, not used yet */, &fFlags);
                VGSvcVerbose(4, "[File %s] Opening with fFlags=0x%x, rc=%Rrc\n", pFile->szName, fFlags, rc);
                if (RT_SUCCESS(rc))
                {
                    rc = RTFileOpen(&pFile->hFile, pFile->szName, fFlags);
                    if (RT_SUCCESS(rc))
                    {
                        /* Seeking is optional. However, the whole operation
                         * will fail if we don't succeed seeking to the wanted position. */
                        if (offOpen)
                            rc = RTFileSeek(pFile->hFile, (int64_t)offOpen, RTFILE_SEEK_BEGIN, NULL /* Current offset */);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Succeeded!
                             */
                            uHandle = VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pHostCtx->uContextID);
                            pFile->uHandle = uHandle;
                            RTListAppend(&pSession->lstFiles, &pFile->Node);
                            VGSvcVerbose(2, "[File %s] Opened (ID=%RU32)\n", pFile->szName, pFile->uHandle);
                        }
                        else
                            VGSvcError("[File %s] Seeking to offset %RU64 failed: rc=%Rrc\n", pFile->szName, offOpen, rc);
                    }
                    else
                        VGSvcError("[File %s] Opening failed with rc=%Rrc\n", pFile->szName, rc);
                }
            }
            else
            {
                VGSvcError("[File %s] empty filename!\n");
                rc = VERR_INVALID_NAME;
            }

            /* clean up if we failed. */
            if (RT_FAILURE(rc))
            {
                if (pFile->hFile != NIL_RTFILE)
                    RTFileClose(pFile->hFile);
                RTMemFree(pFile);
            }
        }
        else
            rc = VERR_NO_MEMORY;

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbOpen(pHostCtx, rc, uHandle);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("[File %s]: Failed to report file open status, rc=%Rrc\n", szFile, rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for open file operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }

    VGSvcVerbose(4, "[File %s] Opening (open mode='%s', disposition='%s', creation mode=0x%x) returned rc=%Rrc\n",
                 szFile, szAccess, szDisposition, uCreationMode, rc);
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileClose(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the message.
     */
    uint32_t uHandle = 0;
    int rc = VbglR3GuestCtrlFileGetClose(pHostCtx, &uHandle /* File handle to close */);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            VGSvcVerbose(2, "[File %s] Closing (handle=%RU32)\n", pFile ? pFile->szName : "<Not found>", uHandle);
            rc = vgsvcGstCtrlSessionFileDestroy(pFile);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbClose(pHostCtx, rc);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file close status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for close file operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileRead(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                                             void **ppvScratchBuf, uint32_t *pcbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uHandle = 0;
    uint32_t cbToRead;
    int rc = VbglR3GuestCtrlFileGetRead(pHostCtx, &uHandle, &cbToRead);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the file and do the reading.
         *
         * If the request is larger than our scratch buffer, try grow it - just
         * ignore failure as the host better respect our buffer limits.
         */
        size_t cbRead = 0;
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            if (*pcbScratchBuf < cbToRead)
                 vgsvcGstCtrlSessionGrowScratchBuf(ppvScratchBuf, pcbScratchBuf, cbToRead);

            rc = RTFileRead(pFile->hFile, *ppvScratchBuf, RT_MIN(cbToRead, *pcbScratchBuf), &cbRead);
            VGSvcVerbose(5, "[File %s] Read %zu/%RU32 bytes, rc=%Rrc\n", pFile->szName, cbRead, cbToRead, rc);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result and data back to the host.
         */
        int rc2 = VbglR3GuestCtrlFileCbRead(pHostCtx, rc, *ppvScratchBuf, (uint32_t)cbRead);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file read status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file read operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileReadAt(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                                               void **ppvScratchBuf, uint32_t *pcbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uHandle = 0;
    uint32_t cbToRead;
    uint64_t offReadAt;
    int rc = VbglR3GuestCtrlFileGetReadAt(pHostCtx, &uHandle, &cbToRead, &offReadAt);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the file and do the reading.
         *
         * If the request is larger than our scratch buffer, try grow it - just
         * ignore failure as the host better respect our buffer limits.
         */
        size_t cbRead = 0;
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            if (*pcbScratchBuf < cbToRead)
                 vgsvcGstCtrlSessionGrowScratchBuf(ppvScratchBuf, pcbScratchBuf, cbToRead);

            rc = RTFileReadAt(pFile->hFile, (RTFOFF)offReadAt, *ppvScratchBuf, RT_MIN(cbToRead, *pcbScratchBuf), &cbRead);
            VGSvcVerbose(5, "[File %s] Read %zu bytes @ %RU64, rc=%Rrc\n", pFile->szName, cbRead, offReadAt, rc);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result and data back to the host.
         */
        int rc2 = VbglR3GuestCtrlFileCbRead(pHostCtx, rc, *ppvScratchBuf, (uint32_t)cbRead);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file read at status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file read at operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileWrite(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                                              void **ppvScratchBuf, uint32_t *pcbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request and data to write.
     */
    uint32_t uHandle = 0;
    uint32_t cbToWrite;
    int rc = VbglR3GuestCtrlFileGetWrite(pHostCtx, &uHandle, *ppvScratchBuf, *pcbScratchBuf, &cbToWrite);
    if (   rc == VERR_BUFFER_OVERFLOW
        && vgsvcGstCtrlSessionGrowScratchBuf(ppvScratchBuf, pcbScratchBuf, cbToWrite))
        rc = VbglR3GuestCtrlFileGetWrite(pHostCtx, &uHandle, *ppvScratchBuf, *pcbScratchBuf, &cbToWrite);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the file and do the writing.
         */
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWrite(pFile->hFile, *ppvScratchBuf, RT_MIN(cbToWrite, *pcbScratchBuf), &cbWritten);
            VGSvcVerbose(5, "[File %s] Writing %p LB %RU32 =>  %Rrc, cbWritten=%zu\n",
                         pFile->szName, *ppvScratchBuf, RT_MIN(cbToWrite, *pcbScratchBuf), rc, cbWritten);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbWrite(pHostCtx, rc, (uint32_t)cbWritten);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file write status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file write operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileWriteAt(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                                                void **ppvScratchBuf, uint32_t *pcbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request and data to write.
     */
    uint32_t uHandle = 0;
    uint32_t cbToWrite;
    uint64_t offWriteAt;
    int rc = VbglR3GuestCtrlFileGetWriteAt(pHostCtx, &uHandle, *ppvScratchBuf, *pcbScratchBuf, &cbToWrite, &offWriteAt);
    if (   rc == VERR_BUFFER_OVERFLOW
        && vgsvcGstCtrlSessionGrowScratchBuf(ppvScratchBuf, pcbScratchBuf, cbToWrite))
        rc = VbglR3GuestCtrlFileGetWriteAt(pHostCtx, &uHandle, *ppvScratchBuf, *pcbScratchBuf, &cbToWrite, &offWriteAt);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the file and do the writing.
         */
        size_t cbWritten = 0;
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            rc = RTFileWriteAt(pFile->hFile, (RTFOFF)offWriteAt, *ppvScratchBuf, RT_MIN(cbToWrite, *pcbScratchBuf), &cbWritten);
            VGSvcVerbose(5, "[File %s] Writing %p LB %RU32 @ %RU64 =>  %Rrc, cbWritten=%zu\n",
                         pFile->szName, *ppvScratchBuf, RT_MIN(cbToWrite, *pcbScratchBuf), offWriteAt, rc, cbWritten);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbWrite(pHostCtx, rc, (uint32_t)cbWritten);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file write status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file write at operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileSeek(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uHandle = 0;
    uint32_t uSeekMethod;
    uint64_t offSeek; /* Will be converted to int64_t. */
    int rc = VbglR3GuestCtrlFileGetSeek(pHostCtx, &uHandle, &uSeekMethod, &offSeek);
    if (RT_SUCCESS(rc))
    {
        uint64_t offActual = 0;

        /*
         * Validate and convert the seek method to IPRT speak.
         */
        static const uint8_t s_abMethods[GUEST_FILE_SEEKTYPE_END + 1] =
        {
            UINT8_MAX, RTFILE_SEEK_BEGIN, UINT8_MAX, UINT8_MAX, RTFILE_SEEK_CURRENT,
            UINT8_MAX, UINT8_MAX, UINT8_MAX, GUEST_FILE_SEEKTYPE_END
        };
        if (   uSeekMethod < RT_ELEMENTS(s_abMethods)
            && s_abMethods[uSeekMethod] != UINT8_MAX)
        {
            /*
             * Locate the file and do the seek.
             */
            PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
            if (pFile)
            {
                rc = RTFileSeek(pFile->hFile, (int64_t)offSeek, s_abMethods[uSeekMethod], &offActual);
                VGSvcVerbose(5, "[File %s]: Seeking to offSeek=%RI64, uSeekMethodIPRT=%u, rc=%Rrc\n",
                             pFile->szName, offSeek, s_abMethods[uSeekMethod], rc);
            }
            else
            {
                VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
                rc = VERR_NOT_FOUND;
            }
        }
        else
        {
            VGSvcError("Invalid seek method: %#x\n", uSeekMethod);
            rc = VERR_NOT_SUPPORTED;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbSeek(pHostCtx, rc, offActual);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file seek status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file seek operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandleFileTell(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uHandle = 0;
    int rc = VbglR3GuestCtrlFileGetTell(pHostCtx, &uHandle);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the file and ask for the current position.
         */
        uint64_t offCurrent = 0;
        PVBOXSERVICECTRLFILE pFile = vgsvcGstCtrlSessionFileGetLocked(pSession, uHandle);
        if (pFile)
        {
            offCurrent = RTFileTell(pFile->hFile);
            VGSvcVerbose(5, "[File %s]: Telling offCurrent=%RU64\n", pFile->szName, offCurrent);
        }
        else
        {
            VGSvcError("File %u (%#x) not found!\n", uHandle, uHandle);
            rc = VERR_NOT_FOUND;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlFileCbTell(pHostCtx, rc, offCurrent);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report file tell status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for file tell operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


static int vgsvcGstCtrlSessionHandlePathRename(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    char     szSource[RTPATH_MAX];
    char     szDest[RTPATH_MAX];
    uint32_t fFlags = 0; /* PATHRENAME_FLAG_XXX */
    int rc = VbglR3GuestCtrlPathGetRename(pHostCtx, szSource, sizeof(szSource), szDest, sizeof(szDest), &fFlags);
    if (RT_SUCCESS(rc))
    {
        /*
         * Validate the flags (kudos for using the same as IPRT), then do the renaming.
         */
        AssertCompile(PATHRENAME_FLAG_NO_REPLACE  == RTPATHRENAME_FLAGS_NO_REPLACE);
        AssertCompile(PATHRENAME_FLAG_REPLACE     == RTPATHRENAME_FLAGS_REPLACE);
        AssertCompile(PATHRENAME_FLAG_NO_SYMLINKS == RTPATHRENAME_FLAGS_NO_SYMLINKS);
        AssertCompile(PATHRENAME_FLAG_VALID_MASK  == (RTPATHRENAME_FLAGS_NO_REPLACE | RTPATHRENAME_FLAGS_REPLACE | RTPATHRENAME_FLAGS_NO_SYMLINKS));
        if (!(fFlags & ~PATHRENAME_FLAG_VALID_MASK))
        {
            VGSvcVerbose(4, "Renaming '%s' to '%s', fFlags=%#x, rc=%Rrc\n", szSource, szDest, fFlags, rc);
            rc = RTPathRename(szSource, szDest, fFlags);
        }
        else
        {
            VGSvcError("Invalid rename flags: %#x\n", fFlags);
            rc = VERR_NOT_SUPPORTED;
        }

        /*
         * Report result back to host.
         */
        int rc2 = VbglR3GuestCtrlMsgReply(pHostCtx, rc);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report renaming status, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for rename operation: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    VGSvcVerbose(5, "Renaming '%s' to '%s' returned rc=%Rrc\n", szSource, szDest, rc);
    return rc;
}


/**
 * Handles getting the user's documents directory.
 *
 * @returns VBox status code.
 * @param   pSession        Guest session.
 * @param   pHostCtx        Host context.
 */
static int vgsvcGstCtrlSessionHandlePathUserDocuments(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    int rc = VbglR3GuestCtrlPathGetUserDocuments(pHostCtx);
    if (RT_SUCCESS(rc))
    {
        /*
         * Get the path and pass it back to the host..
         */
        char szPath[RTPATH_MAX];
        rc = RTPathUserDocuments(szPath, sizeof(szPath));
#ifdef DEBUG
        VGSvcVerbose(2, "User documents is '%s', rc=%Rrc\n", szPath, rc);
#endif

        int rc2 = VbglR3GuestCtrlMsgReplyEx(pHostCtx, rc, 0 /* Type */, szPath,
                                            RT_SUCCESS(rc) ? (uint32_t)strlen(szPath) + 1 /* Include terminating zero */ : 0);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report user documents, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for user documents path request: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


/**
 * Handles getting the user's home directory.
 *
 * @returns VBox status code.
 * @param   pSession        Guest session.
 * @param   pHostCtx        Host context.
 */
static int vgsvcGstCtrlSessionHandlePathUserHome(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    int rc = VbglR3GuestCtrlPathGetUserHome(pHostCtx);
    if (RT_SUCCESS(rc))
    {
        /*
         * Get the path and pass it back to the host..
         */
        char szPath[RTPATH_MAX];
        rc = RTPathUserHome(szPath, sizeof(szPath));

#ifdef DEBUG
        VGSvcVerbose(2, "User home is '%s', rc=%Rrc\n", szPath, rc);
#endif
        /* Report back in any case. */
        int rc2 = VbglR3GuestCtrlMsgReplyEx(pHostCtx, rc, 0 /* Type */, szPath,
                                            RT_SUCCESS(rc) ?(uint32_t)strlen(szPath) + 1 /* Include terminating zero */ : 0);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Failed to report user home, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for user home directory path request: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


/**
 * Handles starting a guest processes.
 *
 * @returns VBox status code.
 * @param   pSession        Guest session.
 * @param   pHostCtx        Host context.
 */
static int vgsvcGstCtrlSessionHandleProcExec(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

/** @todo this hardcoded stuff needs redoing.   */

    /* Initialize maximum environment block size -- needed as input
     * parameter to retrieve the stuff from the host. On output this then
     * will contain the actual block size. */
    VBOXSERVICECTRLPROCSTARTUPINFO startupInfo;
    RT_ZERO(startupInfo);
    startupInfo.cbEnv = sizeof(startupInfo.szEnv);

    int rc = VbglR3GuestCtrlProcGetStart(pHostCtx,
                                         /* Command */
                                         startupInfo.szCmd,      sizeof(startupInfo.szCmd),
                                         /* Flags */
                                         &startupInfo.uFlags,
                                         /* Arguments */
                                         startupInfo.szArgs,     sizeof(startupInfo.szArgs),    &startupInfo.uNumArgs,
                                         /* Environment */
                                         startupInfo.szEnv,      &startupInfo.cbEnv,            &startupInfo.uNumEnvVars,
                                         /* Credentials; for hosts with VBox < 4.3 (protocol version 1).
                                          * For protocol v2 and up the credentials are part of the session
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
        VGSvcVerbose(3, "Request to start process szCmd=%s, fFlags=0x%x, szArgs=%s, szEnv=%s, uTimeout=%RU32\n",
                     startupInfo.szCmd, startupInfo.uFlags,
                     startupInfo.uNumArgs ? startupInfo.szArgs : "<None>",
                     startupInfo.uNumEnvVars ? startupInfo.szEnv : "<None>",
                     startupInfo.uTimeLimitMS);

        bool fStartAllowed = false; /* Flag indicating whether starting a process is allowed or not. */
        rc = VGSvcGstCtrlSessionProcessStartAllowed(pSession, &fStartAllowed);
        if (RT_SUCCESS(rc))
        {
            if (fStartAllowed)
                rc = VGSvcGstCtrlProcessStart(pSession, &startupInfo, pHostCtx->uContextID);
            else
                rc = VERR_MAX_PROCS_REACHED; /* Maximum number of processes reached. */
        }

        /* We're responsible for signaling errors to the host (it will wait for ever otherwise). */
        if (RT_FAILURE(rc))
        {
            VGSvcError("Starting process failed with rc=%Rrc, protocol=%RU32, parameters=%RU32\n",
                       rc, pHostCtx->uProtocol, pHostCtx->uNumParms);
            int rc2 = VbglR3GuestCtrlProcCbStatus(pHostCtx, 0 /*nil-PID*/, PROC_STS_ERROR, rc, NULL /*pvData*/, 0 /*cbData*/);
            if (RT_FAILURE(rc2))
                VGSvcError("Error sending start process status to host, rc=%Rrc\n", rc2);
        }
    }
    else
    {
        VGSvcError("Failed to retrieve parameters for process start: %Rrc (cParms=%u)\n", rc, pHostCtx->uNumParms);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


/**
 * Sends stdin input to a specific guest process.
 *
 * @returns VBox status code.
 * @param   pSession            The session which is in charge.
 * @param   pHostCtx            The host context to use.
 * @param   ppvScratchBuf       The scratch buffer, we may grow it.
 * @param   pcbScratchBuf       The scratch buffer size for retrieving the input
 *                              data.
 */
static int vgsvcGstCtrlSessionHandleProcInput(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                                              void **ppvScratchBuf, uint32_t *pcbScratchBuf)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the data from the host.
     */
    uint32_t uPID;
    uint32_t fFlags;
    uint32_t cbInput;
    int rc = VbglR3GuestCtrlProcGetInput(pHostCtx, &uPID, &fFlags, *ppvScratchBuf, *pcbScratchBuf, &cbInput);
    if (   rc == VERR_BUFFER_OVERFLOW
        && vgsvcGstCtrlSessionGrowScratchBuf(ppvScratchBuf, pcbScratchBuf, cbInput))
        rc = VbglR3GuestCtrlProcGetInput(pHostCtx, &uPID, &fFlags, *ppvScratchBuf, *pcbScratchBuf, &cbInput);
    if (RT_SUCCESS(rc))
    {
        if (fFlags & INPUT_FLAG_EOF)
            VGSvcVerbose(4, "Got last process input block for PID=%RU32 (%RU32 bytes) ...\n", uPID, cbInput);

        /*
         * Locate the process and feed it.
         */
        PVBOXSERVICECTRLPROCESS pProcess = VGSvcGstCtrlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = VGSvcGstCtrlProcessHandleInput(pProcess, pHostCtx, RT_BOOL(fFlags & INPUT_FLAG_EOF),
                                                *ppvScratchBuf, RT_MIN(cbInput, *pcbScratchBuf));
            if (RT_FAILURE(rc))
                VGSvcError("Error handling input command for PID=%RU32, rc=%Rrc\n", uPID, rc);
            VGSvcGstCtrlProcessRelease(pProcess);
        }
        else
        {
            VGSvcError("Could not find PID %u for feeding %u bytes to it.\n", uPID, cbInput);
            rc = VERR_PROCESS_NOT_FOUND;
            VbglR3GuestCtrlProcCbStatusInput(pHostCtx, uPID, INPUT_STS_ERROR, rc, 0);
        }
    }
    else
    {
        VGSvcError("Failed to retrieve parameters for process input: %Rrc (scratch %u bytes)\n", rc, *pcbScratchBuf);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }

    VGSvcVerbose(6, "Feeding input to PID=%RU32 resulted in rc=%Rrc\n", uPID, rc);
    return rc;
}


/**
 * Gets stdout/stderr output of a specific guest process.
 *
 * @returns VBox status code.
 * @param   pSession            The session which is in charge.
 * @param   pHostCtx            The host context to use.
 */
static int vgsvcGstCtrlSessionHandleProcOutput(PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t fFlags;
    int rc = VbglR3GuestCtrlProcGetOutput(pHostCtx, &uPID, &uHandleID, &fFlags);
#ifdef DEBUG_andy
    VGSvcVerbose(4, "Getting output for PID=%RU32, CID=%RU32, uHandleID=%RU32, fFlags=%RU32\n",
                 uPID, pHostCtx->uContextID, uHandleID, fFlags);
#endif
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the process and hand it the output request.
         */
        PVBOXSERVICECTRLPROCESS pProcess = VGSvcGstCtrlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = VGSvcGstCtrlProcessHandleOutput(pProcess, pHostCtx, uHandleID, _64K /* cbToRead */, fFlags);
            if (RT_FAILURE(rc))
                VGSvcError("Error getting output for PID=%RU32, rc=%Rrc\n", uPID, rc);
            VGSvcGstCtrlProcessRelease(pProcess);
        }
        else
        {
            VGSvcError("Could not find PID %u for draining handle %u (%#x).\n", uPID, uHandleID, uHandleID);
            rc = VERR_PROCESS_NOT_FOUND;
/** @todo r=bird:
 *
 *  No way to report status status code for output requests?
 *
 */
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for process output request: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }

#ifdef DEBUG_andy
    VGSvcVerbose(4, "Getting output for PID=%RU32 resulted in rc=%Rrc\n", uPID, rc);
#endif
    return rc;
}


/**
 * Tells a guest process to terminate.
 *
 * @returns VBox status code.
 * @param   pSession            The session which is in charge.
 * @param   pHostCtx            The host context to use.
 */
static int vgsvcGstCtrlSessionHandleProcTerminate(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uPID;
    int rc = VbglR3GuestCtrlProcGetTerminate(pHostCtx, &uPID);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the process and terminate it.
         */
        PVBOXSERVICECTRLPROCESS pProcess = VGSvcGstCtrlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = VGSvcGstCtrlProcessHandleTerm(pProcess);

            VGSvcGstCtrlProcessRelease(pProcess);
        }
        else
        {
            VGSvcError("Could not find PID %u for termination.\n", uPID);
            rc = VERR_PROCESS_NOT_FOUND;
/** @todo r=bird:
 *
 *  No way to report status status code for output requests?
 *
 */
        }
    }
    else
    {
        VGSvcError("Error fetching parameters for process termination request: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
#ifdef DEBUG_andy
    VGSvcVerbose(4, "Terminating PID=%RU32 resulted in rc=%Rrc\n", uPID, rc);
#endif
    return rc;
}


static int vgsvcGstCtrlSessionHandleProcWaitFor(const PVBOXSERVICECTRLSESSION pSession, PVBGLR3GUESTCTRLCMDCTX pHostCtx)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);

    /*
     * Retrieve the request.
     */
    uint32_t uPID;
    uint32_t uWaitFlags;
    uint32_t uTimeoutMS;
    int rc = VbglR3GuestCtrlProcGetWaitFor(pHostCtx, &uPID, &uWaitFlags, &uTimeoutMS);
    if (RT_SUCCESS(rc))
    {
        /*
         * Locate the process and the realize that this call makes no sense
         * since we'll notify the host when a process terminates anyway and
         * hopefully don't need any additional encouragement.
         */
        PVBOXSERVICECTRLPROCESS pProcess = VGSvcGstCtrlSessionRetainProcess(pSession, uPID);
        if (pProcess)
        {
            rc = VERR_NOT_IMPLEMENTED; /** @todo */
            VGSvcGstCtrlProcessRelease(pProcess);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else
    {
        VGSvcError("Error fetching parameters for process wait request: %Rrc\n", rc);
        VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, rc, UINT32_MAX);
    }
    return rc;
}


int VGSvcGstCtrlSessionHandler(PVBOXSERVICECTRLSESSION pSession, uint32_t uMsg, PVBGLR3GUESTCTRLCMDCTX pHostCtx,
                               void **ppvScratchBuf, uint32_t *pcbScratchBuf, volatile bool *pfShutdown)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(*ppvScratchBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pfShutdown, VERR_INVALID_POINTER);


    /*
     * Only anonymous sessions (that is, sessions which run with local
     * service privileges) or spawned session processes can do certain
     * operations.
     */
    bool const fImpersonated = RT_BOOL(pSession->fFlags & (  VBOXSERVICECTRLSESSION_FLAG_SPAWN
                                                           | VBOXSERVICECTRLSESSION_FLAG_ANONYMOUS));
    int rc = VERR_NOT_SUPPORTED; /* Play safe by default. */

    switch (uMsg)
    {
        case HOST_SESSION_CLOSE:
            /* Shutdown (this spawn). */
            rc = VGSvcGstCtrlSessionClose(pSession);
            *pfShutdown = true; /* Shutdown in any case. */
            break;

        case HOST_DIR_REMOVE:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleDirRemove(pSession, pHostCtx);
            break;

        case HOST_EXEC_CMD:
            rc = vgsvcGstCtrlSessionHandleProcExec(pSession, pHostCtx);
            break;

        case HOST_EXEC_SET_INPUT:
            rc = vgsvcGstCtrlSessionHandleProcInput(pSession, pHostCtx, ppvScratchBuf, pcbScratchBuf);
            break;

        case HOST_EXEC_GET_OUTPUT:
            rc = vgsvcGstCtrlSessionHandleProcOutput(pSession, pHostCtx);
            break;

        case HOST_EXEC_TERMINATE:
            rc = vgsvcGstCtrlSessionHandleProcTerminate(pSession, pHostCtx);
            break;

        case HOST_EXEC_WAIT_FOR:
            rc = vgsvcGstCtrlSessionHandleProcWaitFor(pSession, pHostCtx);
            break;

        case HOST_FILE_OPEN:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileOpen(pSession, pHostCtx);
            break;

        case HOST_FILE_CLOSE:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileClose(pSession, pHostCtx);
            break;

        case HOST_FILE_READ:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileRead(pSession, pHostCtx, ppvScratchBuf, pcbScratchBuf);
            break;

        case HOST_FILE_READ_AT:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileReadAt(pSession, pHostCtx, ppvScratchBuf, pcbScratchBuf);
            break;

        case HOST_FILE_WRITE:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileWrite(pSession, pHostCtx, ppvScratchBuf, pcbScratchBuf);
            break;

        case HOST_FILE_WRITE_AT:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileWriteAt(pSession, pHostCtx, ppvScratchBuf, pcbScratchBuf);
            break;

        case HOST_FILE_SEEK:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileSeek(pSession, pHostCtx);
            break;

        case HOST_FILE_TELL:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandleFileTell(pSession, pHostCtx);
            break;

        case HOST_PATH_RENAME:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandlePathRename(pSession, pHostCtx);
            break;

        case HOST_PATH_USER_DOCUMENTS:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandlePathUserDocuments(pSession, pHostCtx);
            break;

        case HOST_PATH_USER_HOME:
            if (fImpersonated)
                rc = vgsvcGstCtrlSessionHandlePathUserHome(pSession, pHostCtx);
            break;

        default: /* Not supported, see next code block. */
            break;
    }
    if (RT_SUCCESS(rc))
    { /* likely */ }
    else if (rc != VERR_NOT_SUPPORTED) /* Note: Reply to host must must be sent by above handler. */
        VGSvcError("Error while handling message (uMsg=%RU32, cParms=%RU32), rc=%Rrc\n", uMsg, pHostCtx->uNumParms, rc);
    else
    {
        /* We must skip and notify host here as best we can... */
        VGSvcVerbose(3, "Unsupported message (uMsg=%RU32, cParms=%RU32) from host, skipping\n", uMsg, pHostCtx->uNumParms);
        if (VbglR3GuestCtrlSupportsOptimizations(pHostCtx->uClientID))
            VbglR3GuestCtrlMsgSkip(pHostCtx->uClientID, VERR_NOT_SUPPORTED, uMsg);
        else
        {
            /*
             * !!! HACK ALERT BEGIN !!!
             * As peeking for the current message by VbglR3GuestCtrlMsgWaitFor() / GUEST_MSG_WAIT only gives us the message type and
             * the number of parameters, but *not* the actual context ID the message is bound to, try to retrieve it here.
             *
             * This is needed in order to reply to the host with the current context ID, without breaking existing clients.
             * Not doing this isn't fatal, but will make host clients wait longer (timing out) for not implemented messages. */
            /** @todo Get rid of this as soon as we have a protocol bump (v4). */
            struct HGCMMsgSkip
            {
                VBGLIOCHGCMCALL hdr;
                /** UInt32: Context ID. */
                HGCMFunctionParameter context;
            };

            HGCMMsgSkip Msg;
            VBGL_HGCM_HDR_INIT(&Msg.hdr, pHostCtx->uClientID, GUEST_MSG_WAIT, pHostCtx->uNumParms);
            Msg.context.SetUInt32(0);

            /* Retrieve the context ID of the message which is not supported and put it in pHostCtx. */
            int rc2 = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
            if (RT_SUCCESS(rc2))
                Msg.context.GetUInt32(&pHostCtx->uContextID);

            /* Now fake a reply to the message. */
            rc2 = VbglR3GuestCtrlMsgReply(pHostCtx, VERR_NOT_SUPPORTED);
            AssertRC(rc2);

            /*  !!!                !!!
             *  !!! HACK ALERT END !!!
             *  !!!                !!! */

            /* Tell the host service to skip the message. */
            VbglR3GuestCtrlMsgSkipOld(pHostCtx->uClientID);

            rc = VINF_SUCCESS;
        }
    }

    if (RT_FAILURE(rc))
        VGSvcError("Error while handling message (uMsg=%RU32, cParms=%RU32), rc=%Rrc\n", uMsg, pHostCtx->uNumParms, rc);

    return rc;
}


/**
 * Thread main routine for a spawned guest session process.
 *
 * This thread runs in the main executable to control the spawned session process.
 *
 * @returns VBox status code.
 * @param   hThreadSelf     Thread handle.
 * @param   pvUser          Pointer to a VBOXSERVICECTRLSESSIONTHREAD structure.
 *
 */
static DECLCALLBACK(int) vgsvcGstCtrlSessionThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLSESSIONTHREAD pThread = (PVBOXSERVICECTRLSESSIONTHREAD)pvUser;
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    uint32_t const idSession = pThread->StartupInfo.uSessionID;
    uint32_t const idClient  = g_idControlSvcClient;
    VGSvcVerbose(3, "Session ID=%RU32 thread running\n", idSession);

    /* Let caller know that we're done initializing, regardless of the result. */
    int rc2 = RTThreadUserSignal(hThreadSelf);
    AssertRC(rc2);

    /*
     * Wait for the child process to stop or the shutdown flag to be signalled.
     */
    RTPROCSTATUS    ProcessStatus       = { 0, RTPROCEXITREASON_NORMAL };
    bool            fProcessAlive       = true;
    bool            fSessionCancelled   = VbglR3GuestCtrlSupportsOptimizations(g_idControlSvcClient);
    uint32_t        cMsShutdownTimeout  = 30 * 1000; /** @todo Make this configurable. Later. */
    uint64_t        msShutdownStart     = 0;
    uint64_t const  msStart             = RTTimeMilliTS();
    size_t          offSecretKey        = 0;
    int             rcWait;
    for (;;)
    {
        /* Secret key feeding. */
        if (offSecretKey < sizeof(pThread->abKey))
        {
            size_t cbWritten = 0;
            rc2 = RTPipeWrite(pThread->hKeyPipe, &pThread->abKey[offSecretKey], sizeof(pThread->abKey) - offSecretKey, &cbWritten);
            if (RT_SUCCESS(rc2))
                offSecretKey += cbWritten;
        }

        /* Poll child process status. */
        rcWait = RTProcWaitNoResume(pThread->hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
        if (   rcWait == VINF_SUCCESS
            || rcWait == VERR_PROCESS_NOT_FOUND)
        {
            fProcessAlive = false;
            break;
        }
        AssertMsgBreak(rcWait == VERR_PROCESS_RUNNING || rcWait == VERR_INTERRUPTED,
                       ("Got unexpected rc=%Rrc while waiting for session process termination\n", rcWait));

        /* Shutting down? */
        if (ASMAtomicReadBool(&pThread->fShutdown))
        {
            if (!msShutdownStart)
            {
                VGSvcVerbose(3, "Notifying guest session process (PID=%RU32, session ID=%RU32) ...\n",
                             pThread->hProcess, idSession);

                VBGLR3GUESTCTRLCMDCTX hostCtx =
                {
                    /* .idClient  = */  idClient,
                    /* .idContext = */  VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(idSession),
                    /* .uProtocol = */  pThread->StartupInfo.uProtocol,
                    /* .cParams   = */  2
                };
                rc2 = VbglR3GuestCtrlSessionClose(&hostCtx, 0 /* fFlags */);
                if (RT_FAILURE(rc2))
                {
                    VGSvcError("Unable to notify guest session process (PID=%RU32, session ID=%RU32), rc=%Rrc\n",
                               pThread->hProcess, idSession, rc2);

                    if (rc2 == VERR_NOT_SUPPORTED)
                    {
                        /* Terminate guest session process in case it's not supported by a too old host. */
                        rc2 = RTProcTerminate(pThread->hProcess);
                        VGSvcVerbose(3, "Terminating guest session process (PID=%RU32) ended with rc=%Rrc\n",
                                     pThread->hProcess, rc2);
                    }
                    break;
                }

                VGSvcVerbose(3, "Guest session ID=%RU32 thread was asked to terminate, waiting for session process to exit (%RU32 ms timeout) ...\n",
                             idSession, cMsShutdownTimeout);
                msShutdownStart = RTTimeMilliTS();
                continue; /* Don't waste time on waiting. */
            }
            if (RTTimeMilliTS() - msShutdownStart > cMsShutdownTimeout)
            {
                 VGSvcVerbose(3, "Guest session ID=%RU32 process did not shut down within time\n", idSession);
                 break;
            }
        }

        /* Cancel the prepared session stuff after 30 seconds. */
        if (  !fSessionCancelled
            && RTTimeMilliTS() - msStart >= 30000)
        {
            VbglR3GuestCtrlSessionCancelPrepared(g_idControlSvcClient, idSession);
            fSessionCancelled = true;
        }

/** @todo r=bird: This 100ms sleep is _extremely_ sucky! */
        RTThreadSleep(100); /* Wait a bit. */
    }

    if (!fSessionCancelled)
        VbglR3GuestCtrlSessionCancelPrepared(g_idControlSvcClient, idSession);

    if (!fProcessAlive)
    {
        VGSvcVerbose(2, "Guest session process (ID=%RU32) terminated with rc=%Rrc, reason=%d, status=%d\n",
                     idSession, rcWait, ProcessStatus.enmReason, ProcessStatus.iStatus);
        if (ProcessStatus.iStatus == RTEXITCODE_INIT)
        {
            VGSvcError("Guest session process (ID=%RU32) failed to initialize. Here some hints:\n", idSession);
            VGSvcError("- Is logging enabled and the output directory is read-only by the guest session user?\n");
            /** @todo Add more here. */
        }
    }

    uint32_t uSessionStatus = GUEST_SESSION_NOTIFYTYPE_UNDEFINED;
    uint32_t uSessionRc = VINF_SUCCESS; /** uint32_t vs. int. */

    if (fProcessAlive)
    {
        for (int i = 0; i < 3; i++)
        {
            VGSvcVerbose(2, "Guest session ID=%RU32 process still alive, killing attempt %d/3\n", idSession, i + 1);

            rc2 = RTProcTerminate(pThread->hProcess);
            if (RT_SUCCESS(rc2))
                break;
            /** @todo r=bird: What's the point of sleeping 3 second after the last attempt? */
            RTThreadSleep(3000);
        }

        VGSvcVerbose(2, "Guest session ID=%RU32 process termination resulted in rc=%Rrc\n", idSession, rc2);
        uSessionStatus = RT_SUCCESS(rc2) ? GUEST_SESSION_NOTIFYTYPE_TOK : GUEST_SESSION_NOTIFYTYPE_TOA;
    }
    else if (RT_SUCCESS(rcWait))
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
                AssertMsgFailed(("Unhandled process termination reason (%d)\n", ProcessStatus.enmReason));
                uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEA;
                break;
        }
    }
    else
    {
        /* If we didn't find the guest process anymore, just assume it terminated normally. */
        uSessionStatus = GUEST_SESSION_NOTIFYTYPE_TEN;
    }

    VGSvcVerbose(3, "Guest session ID=%RU32 thread ended with sessionStatus=%RU32, sessionRc=%Rrc\n",
                 idSession, uSessionStatus, uSessionRc);

    /*
     * Report final status.
     */
    Assert(uSessionStatus != GUEST_SESSION_NOTIFYTYPE_UNDEFINED);
    VBGLR3GUESTCTRLCMDCTX ctx = { idClient, VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(idSession) };
    rc2 = VbglR3GuestCtrlSessionNotify(&ctx, uSessionStatus, uSessionRc);
    if (RT_FAILURE(rc2))
        VGSvcError("Reporting session ID=%RU32 final status failed with rc=%Rrc\n", idSession, rc2);

    VGSvcVerbose(3, "Session ID=%RU32 thread ending\n", idSession);
    return VINF_SUCCESS;
}

/**
 * Reads the secret key the parent VBoxService instance passed us and pass it
 * along as a authentication token to the host service.
 *
 * For older hosts, this sets up the message filtering.
 *
 * @returns VBox status code.
 * @param   idClient        The HGCM client ID.
 * @param   idSession       The session ID.
 */
static int vgsvcGstCtrlSessionReadKeyAndAccept(uint32_t idClient, uint32_t idSession)
{
    /*
     * Read it.
     */
    RTHANDLE Handle;
    int rc = RTHandleGetStandard(RTHANDLESTD_INPUT, &Handle);
    if (RT_SUCCESS(rc))
    {
        if (Handle.enmType == RTHANDLETYPE_PIPE)
        {
            uint8_t abSecretKey[RT_SIZEOFMEMB(VBOXSERVICECTRLSESSIONTHREAD, abKey)];
            rc = RTPipeReadBlocking(Handle.u.hPipe, abSecretKey, sizeof(abSecretKey), NULL);
            if (RT_SUCCESS(rc))
            {
                VGSvcVerbose(3, "Got secret key from standard input.\n");

                /*
                 * Do the accepting, if appropriate.
                 */
                if (g_fControlSupportsOptimizations)
                {
                    rc = VbglR3GuestCtrlSessionAccept(idClient, idSession, abSecretKey, sizeof(abSecretKey));
                    if (RT_SUCCESS(rc))
                        VGSvcVerbose(3, "Session %u accepted\n");
                    else
                        VGSvcError("Failed to accept session: %Rrc\n", rc);
                }
                else
                {
                    /* For legacy hosts, we do the filtering thingy. */
                    rc = VbglR3GuestCtrlMsgFilterSet(idClient, VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(idSession),
                                                     VBOX_GUESTCTRL_FILTER_BY_SESSION(idSession), 0);
                    if (RT_SUCCESS(rc))
                        VGSvcVerbose(3, "Session %u filtering successfully enabled\n");
                    else
                        VGSvcError("Failed to set session filter: %Rrc\n", rc);
                }
            }
            else
                VGSvcError("Error reading secret key from standard input: %Rrc\n", rc);
        }
        else
        {
            VGSvcError("Standard input is not a pipe!\n");
            rc = VERR_INVALID_HANDLE;
        }
        RTHandleClose(&Handle);
    }
    else
        VGSvcError("RTHandleGetStandard failed on standard input: %Rrc\n", rc);
    return rc;
}

/**
 * Main message handler for the guest control session process.
 *
 * @returns exit code.
 * @param   pSession    Pointer to g_Session.
 * @thread  main.
 */
static RTEXITCODE vgsvcGstCtrlSessionSpawnWorker(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, RTEXITCODE_FAILURE);
    VGSvcVerbose(0, "Hi, this is guest session ID=%RU32\n", pSession->StartupInfo.uSessionID);

    /*
     * Connect to the host service.
     */
    uint32_t idClient;
    int rc = VbglR3GuestCtrlConnect(&idClient);
    if (RT_FAILURE(rc))
        return VGSvcError("Error connecting to guest control service, rc=%Rrc\n", rc);
    g_fControlSupportsOptimizations = VbglR3GuestCtrlSupportsOptimizations(idClient);

    rc = vgsvcGstCtrlSessionReadKeyAndAccept(idClient, pSession->StartupInfo.uSessionID);
    if (RT_SUCCESS(rc))
    {
        VGSvcVerbose(1, "Using client ID=%RU32\n", idClient);

        /*
         * Report started status.
         * If session status cannot be posted to the host for some reason, bail out.
         */
        VBGLR3GUESTCTRLCMDCTX ctx = { idClient, VBOX_GUESTCTRL_CONTEXTID_MAKE_SESSION(pSession->StartupInfo.uSessionID) };
        rc = VbglR3GuestCtrlSessionNotify(&ctx, GUEST_SESSION_NOTIFYTYPE_STARTED, VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            /*
             * Allocate a scratch buffer for commands which also send payload data with them.
             * This buffer may grow if the host sends us larger chunks of data.
             */
            uint32_t cbScratchBuf = _64K;
            void    *pvScratchBuf = RTMemAlloc(cbScratchBuf);
            if (pvScratchBuf)
            {
                /*
                 * Message processing loop.
                 */
                VBGLR3GUESTCTRLCMDCTX CtxHost = { idClient, 0 /* Context ID */, pSession->StartupInfo.uProtocol, 0 };
                for (;;)
                {
                    VGSvcVerbose(3, "Waiting for host msg ...\n");
                    uint32_t uMsg = 0;
                    rc = VbglR3GuestCtrlMsgPeekWait(idClient, &uMsg, &CtxHost.uNumParms, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        VGSvcVerbose(4, "Msg=%RU32 (%RU32 parms) retrieved (%Rrc)\n", uMsg, CtxHost.uNumParms, rc);

                        /*
                         * Pass it on to the session handler.
                         * Note! Only when handling HOST_SESSION_CLOSE is the rc used.
                         */
                        bool fShutdown = false;
                        rc = VGSvcGstCtrlSessionHandler(pSession, uMsg, &CtxHost, &pvScratchBuf, &cbScratchBuf, &fShutdown);
                        if (fShutdown)
                            break;
                    }
                    else /** @todo Shouldn't we have a plan for handling connection loss and such?  Now, we'll just spin like crazy. */
                        VGSvcVerbose(3, "Getting host message failed with %Rrc\n", rc); /* VERR_GEN_IO_FAILURE seems to be normal if ran into timeout. */

                    /* Let others run (guests are often single CPU) ... */
                    RTThreadYield();
                }

                /*
                 * Shutdown.
                 */
                RTMemFree(pvScratchBuf);
            }
            else
                rc = VERR_NO_MEMORY;

            VGSvcVerbose(0, "Session %RU32 ended\n", pSession->StartupInfo.uSessionID);
        }
        else
            VGSvcError("Reporting session ID=%RU32 started status failed with rc=%Rrc\n", pSession->StartupInfo.uSessionID, rc);
    }
    else
        VGSvcError("Setting message filterAdd=0x%x failed with rc=%Rrc\n", pSession->StartupInfo.uSessionID, rc);

    VGSvcVerbose(3, "Disconnecting client ID=%RU32 ...\n", idClient);
    VbglR3GuestCtrlDisconnect(idClient);

    VGSvcVerbose(3, "Session worker returned with rc=%Rrc\n", rc);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Finds a (formerly) started guest process given by its PID and increases its
 * reference count.
 *
 * Must be decreased by the caller with VGSvcGstCtrlProcessRelease().
 *
 * @returns Guest process if found, otherwise NULL.
 * @param   pSession    Pointer to guest session where to search process in.
 * @param   uPID        PID to search for.
 *
 * @note    This does *not lock the process!
 */
PVBOXSERVICECTRLPROCESS VGSvcGstCtrlSessionRetainProcess(PVBOXSERVICECTRLSESSION pSession, uint32_t uPID)
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


int VGSvcGstCtrlSessionClose(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    VGSvcVerbose(0, "Session %RU32 is about to close ...\n", pSession->StartupInfo.uSessionID);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        /*
         * Close all guest processes.
         */
        VGSvcVerbose(0, "Stopping all guest processes ...\n");

        /* Signal all guest processes in the active list that we want to shutdown. */
        size_t cProcesses = 0;
        PVBOXSERVICECTRLPROCESS pProcess;
        RTListForEach(&pSession->lstProcesses, pProcess, VBOXSERVICECTRLPROCESS, Node)
        {
            VGSvcGstCtrlProcessStop(pProcess);
            cProcesses++;
        }

        VGSvcVerbose(1, "%zu guest processes were signalled to stop\n", cProcesses);

        /* Wait for all active threads to shutdown and destroy the active thread list. */
        pProcess = RTListGetFirst(&pSession->lstProcesses, VBOXSERVICECTRLPROCESS, Node);
        while (pProcess)
        {
            PVBOXSERVICECTRLPROCESS pNext = RTListNodeGetNext(&pProcess->Node, VBOXSERVICECTRLPROCESS, Node);
            bool fLast = RTListNodeIsLast(&pSession->lstProcesses, &pProcess->Node);

            int rc2 = RTCritSectLeave(&pSession->CritSect);
            AssertRC(rc2);

            rc2 = VGSvcGstCtrlProcessWait(pProcess, 30 * 1000 /* Wait 30 seconds max. */, NULL /* rc */);

            int rc3 = RTCritSectEnter(&pSession->CritSect);
            AssertRC(rc3);

            if (RT_SUCCESS(rc2))
                VGSvcGstCtrlProcessFree(pProcess);

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

            VGSvcVerbose(1, "Process %p (PID %RU32) still in list\n", pProcess, pProcess->uPID);
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
        VGSvcVerbose(0, "Closing all guest files ...\n");

        PVBOXSERVICECTRLFILE pFile;
        pFile = RTListGetFirst(&pSession->lstFiles, VBOXSERVICECTRLFILE, Node);
        while (pFile)
        {
            PVBOXSERVICECTRLFILE pNext = RTListNodeGetNext(&pFile->Node, VBOXSERVICECTRLFILE, Node);
            bool fLast = RTListNodeIsLast(&pSession->lstFiles, &pFile->Node);

            int rc2 = vgsvcGstCtrlSessionFileDestroy(pFile);
            if (RT_FAILURE(rc2))
            {
                VGSvcError("Unable to close file '%s'; rc=%Rrc\n", pFile->szName, rc2);
                if (RT_SUCCESS(rc))
                    rc = rc2;
                /* Keep going. */
            }

            if (fLast)
                break;

            pFile = pNext;
        }

        AssertMsg(RTListIsEmpty(&pSession->lstFiles), ("Guest file list still contains entries when it should not\n"));

        int rc2 = RTCritSectLeave(&pSession->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


int VGSvcGstCtrlSessionDestroy(PVBOXSERVICECTRLSESSION pSession)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    int rc = VGSvcGstCtrlSessionClose(pSession);

    /* Destroy critical section. */
    RTCritSectDelete(&pSession->CritSect);

    return rc;
}


int VGSvcGstCtrlSessionInit(PVBOXSERVICECTRLSESSION pSession, uint32_t fFlags)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    RTListInit(&pSession->lstProcesses);
    RTListInit(&pSession->lstFiles);

    pSession->fFlags = fFlags;

    /* Init critical section for protecting the thread lists. */
    int rc = RTCritSectInit(&pSession->CritSect);
    AssertRC(rc);

    return rc;
}


/**
 * Adds a guest process to a session's process list.
 *
 * @return  VBox status code.
 * @param   pSession                Guest session to add process to.
 * @param   pProcess                Guest process to add.
 */
int VGSvcGstCtrlSessionProcessAdd(PVBOXSERVICECTRLSESSION pSession, PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        VGSvcVerbose( 3, "Adding process (PID %RU32) to session ID=%RU32\n", pProcess->uPID, pSession->StartupInfo.uSessionID);

        /* Add process to session list. */
        RTListAppend(&pSession->lstProcesses, &pProcess->Node);

        int rc2 = RTCritSectLeave(&pSession->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return VINF_SUCCESS;
}


/**
 * Removes a guest process from a session's process list.
 *
 * @return  VBox status code.
 * @param   pSession                Guest session to remove process from.
 * @param   pProcess                Guest process to remove.
 */
int VGSvcGstCtrlSessionProcessRemove(PVBOXSERVICECTRLSESSION pSession, PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        VGSvcVerbose(3, "Removing process (PID %RU32) from session ID=%RU32\n", pProcess->uPID, pSession->StartupInfo.uSessionID);
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
 * @return  VBox status code.
 * @param   pSession            The guest session.
 * @param   pbAllowed           True if starting (another) guest process
 *                              is allowed, false if not.
 */
int VGSvcGstCtrlSessionProcessStartAllowed(const PVBOXSERVICECTRLSESSION pSession, bool *pbAllowed)
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

            VGSvcVerbose(3, "Maximum served guest processes set to %u, running=%u\n", pSession->uProcsMaxKept, uProcsRunning);

            int32_t iProcsLeft = (pSession->uProcsMaxKept - uProcsRunning - 1);
            if (iProcsLeft < 0)
            {
                VGSvcVerbose(3, "Maximum running guest processes reached (%u)\n", pSession->uProcsMaxKept);
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
 * Creates the process for a guest session.
 *
 * @return  VBox status code.
 * @param   pSessionStartupInfo     Session startup info.
 * @param   pSessionThread          The session thread under construction.
 * @param   uCtrlSessionThread      The session thread debug ordinal.
 */
static int vgsvcVGSvcGstCtrlSessionThreadCreateProcess(const PVBOXSERVICECTRLSESSIONSTARTUPINFO pSessionStartupInfo,
                                                       PVBOXSERVICECTRLSESSIONTHREAD pSessionThread, uint32_t uCtrlSessionThread)
{
    RT_NOREF(uCtrlSessionThread);

    /*
     * Is this an anonymous session?  Anonymous sessions run with the same
     * privileges as the main VBoxService executable.
     */
    bool const fAnonymous = pSessionThread->StartupInfo.szUser[0] == '\0';
    if (fAnonymous)
    {
        Assert(!strlen(pSessionThread->StartupInfo.szPassword));
        Assert(!strlen(pSessionThread->StartupInfo.szDomain));

        VGSvcVerbose(3, "New anonymous guest session ID=%RU32 created, fFlags=%x, using protocol %RU32\n",
                     pSessionStartupInfo->uSessionID,
                     pSessionStartupInfo->fFlags,
                     pSessionStartupInfo->uProtocol);
    }
    else
    {
        VGSvcVerbose(3, "Spawning new guest session ID=%RU32, szUser=%s, szPassword=%s, szDomain=%s, fFlags=%x, using protocol %RU32\n",
                     pSessionStartupInfo->uSessionID,
                     pSessionStartupInfo->szUser,
#ifdef DEBUG
                     pSessionStartupInfo->szPassword,
#else
                     "XXX", /* Never show passwords in release mode. */
#endif
                     pSessionStartupInfo->szDomain,
                     pSessionStartupInfo->fFlags,
                     pSessionStartupInfo->uProtocol);
    }

    /*
     * Spawn a child process for doing the actual session handling.
     * Start by assembling the argument list.
     */
    char szExeName[RTPATH_MAX];
    char *pszExeName = RTProcGetExecutablePath(szExeName, sizeof(szExeName));
    AssertReturn(pszExeName, VERR_FILENAME_TOO_LONG);

    char szParmSessionID[32];
    RTStrPrintf(szParmSessionID, sizeof(szParmSessionID), "--session-id=%RU32", pSessionThread->StartupInfo.uSessionID);

    char szParmSessionProto[32];
    RTStrPrintf(szParmSessionProto, sizeof(szParmSessionProto), "--session-proto=%RU32",
                pSessionThread->StartupInfo.uProtocol);
#ifdef DEBUG
    char szParmThreadId[32];
    RTStrPrintf(szParmThreadId, sizeof(szParmThreadId), "--thread-id=%RU32", uCtrlSessionThread);
#endif
    unsigned    idxArg = 0; /* Next index in argument vector. */
    char const *apszArgs[24];

    apszArgs[idxArg++] = pszExeName;
    apszArgs[idxArg++] = "guestsession";
    apszArgs[idxArg++] = szParmSessionID;
    apszArgs[idxArg++] = szParmSessionProto;
#ifdef DEBUG
    apszArgs[idxArg++] = szParmThreadId;
#endif
    if (!fAnonymous) /* Do we need to pass a user name? */
    {
        apszArgs[idxArg++] = "--user";
        apszArgs[idxArg++] = pSessionThread->StartupInfo.szUser;

        if (strlen(pSessionThread->StartupInfo.szDomain))
        {
            apszArgs[idxArg++] = "--domain";
            apszArgs[idxArg++] = pSessionThread->StartupInfo.szDomain;
        }
    }

    /* Add same verbose flags as parent process. */
    char szParmVerbose[32];
    if (g_cVerbosity > 0)
    {
        unsigned cVs = RT_MIN(g_cVerbosity, RT_ELEMENTS(szParmVerbose) - 2);
        szParmVerbose[0] = '-';
        memset(&szParmVerbose[1], 'v', cVs);
        szParmVerbose[1 + cVs] = '\0';
        apszArgs[idxArg++] = szParmVerbose;
    }

    /* Add log file handling. Each session will have an own
     * log file, naming based on the parent log file. */
    char szParmLogFile[sizeof(g_szLogFile) + 128];
    if (g_szLogFile[0])
    {
        const char *pszSuffix = RTPathSuffix(g_szLogFile);
        if (!pszSuffix)
            pszSuffix = strchr(g_szLogFile, '\0');
        size_t cchBase = pszSuffix - g_szLogFile;
#ifndef DEBUG
        RTStrPrintf(szParmLogFile, sizeof(szParmLogFile), "%.*s-%RU32-%s%s",
                    cchBase, g_szLogFile, pSessionStartupInfo->uSessionID, pSessionStartupInfo->szUser, pszSuffix);
#else
        RTStrPrintf(szParmLogFile, sizeof(szParmLogFile), "%.*s-%RU32-%RU32-%s%s",
                    cchBase, g_szLogFile, pSessionStartupInfo->uSessionID, uCtrlSessionThread,
                    pSessionStartupInfo->szUser, pszSuffix);
#endif
        apszArgs[idxArg++] = "--logfile";
        apszArgs[idxArg++] = szParmLogFile;
    }

#ifdef DEBUG
    if (g_Session.fFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT)
        apszArgs[idxArg++] = "--dump-stdout";
    if (g_Session.fFlags & VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR)
        apszArgs[idxArg++] = "--dump-stderr";
#endif
    apszArgs[idxArg] = NULL;
    Assert(idxArg < RT_ELEMENTS(apszArgs));

    if (g_cVerbosity > 3)
    {
        VGSvcVerbose(4, "Spawning parameters:\n");
        for (idxArg = 0; apszArgs[idxArg]; idxArg++)
            VGSvcVerbose(4, "\t%s\n", apszArgs[idxArg]);
    }

    /*
     * Flags.
     */
    uint32_t const fProcCreate = RTPROC_FLAGS_PROFILE
#ifdef RT_OS_WINDOWS
                               | RTPROC_FLAGS_SERVICE
                               | RTPROC_FLAGS_HIDDEN
#endif
                               ;

    /*
     * Configure standard handles.
     */
    RTHANDLE hStdIn;
    int rc = RTPipeCreate(&hStdIn.u.hPipe, &pSessionThread->hKeyPipe, RTPIPE_C_INHERIT_READ);
    if (RT_SUCCESS(rc))
    {
        hStdIn.enmType = RTHANDLETYPE_PIPE;

        RTHANDLE hStdOutAndErr;
        rc = RTFileOpenBitBucket(&hStdOutAndErr.u.hFile, RTFILE_O_WRITE);
        if (RT_SUCCESS(rc))
        {
            hStdOutAndErr.enmType = RTHANDLETYPE_FILE;

            /*
             * Windows: If a domain name is given, construct an UPN (User Principle Name)
             *          with the domain name built-in, e.g. "joedoe@example.com".
             */
            const char *pszUser    = pSessionThread->StartupInfo.szUser;
#ifdef RT_OS_WINDOWS
            char       *pszUserUPN = NULL;
            if (pSessionThread->StartupInfo.szDomain[0])
            {
                int cchbUserUPN = RTStrAPrintf(&pszUserUPN, "%s@%s",
                                               pSessionThread->StartupInfo.szUser,
                                               pSessionThread->StartupInfo.szDomain);
                if (cchbUserUPN > 0)
                {
                    pszUser = pszUserUPN;
                    VGSvcVerbose(3, "Using UPN: %s\n", pszUserUPN);
                }
                else
                    rc = VERR_NO_STR_MEMORY;
            }
            if (RT_SUCCESS(rc))
#endif
            {
                /*
                 * Finally, create the process.
                 */
                rc = RTProcCreateEx(pszExeName, apszArgs, RTENV_DEFAULT, fProcCreate,
                                    &hStdIn, &hStdOutAndErr, &hStdOutAndErr,
                                    !fAnonymous ? pszUser : NULL,
                                    !fAnonymous ? pSessionThread->StartupInfo.szPassword : NULL,
                                    &pSessionThread->hProcess);
            }
#ifdef RT_OS_WINDOWS
            RTStrFree(pszUserUPN);
#endif
            RTFileClose(hStdOutAndErr.u.hFile);
        }

        RTPipeClose(hStdIn.u.hPipe);
    }
    return rc;
}


/**
 * Creates a guest session.
 *
 * This will spawn a new VBoxService.exe instance under behalf of the given user
 * which then will act as a session host. On successful open, the session will
 * be added to the given session thread list.
 *
 * @return  VBox status code.
 * @param   pList                   Which list to use to store the session thread in.
 * @param   pSessionStartupInfo     Session startup info.
 * @param   ppSessionThread         Returns newly created session thread on success.
 *                                  Optional.
 */
int VGSvcGstCtrlSessionThreadCreate(PRTLISTANCHOR pList, const PVBOXSERVICECTRLSESSIONSTARTUPINFO pSessionStartupInfo,
                                    PVBOXSERVICECTRLSESSIONTHREAD *ppSessionThread)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pSessionStartupInfo, VERR_INVALID_POINTER);
    /* ppSessionThread is optional. */

#ifdef VBOX_STRICT
    /* Check for existing session in debug mode. Should never happen because of
     * Main consistency. */
    PVBOXSERVICECTRLSESSIONTHREAD pSessionCur;
    RTListForEach(pList, pSessionCur, VBOXSERVICECTRLSESSIONTHREAD, Node)
    {
        AssertMsgReturn(pSessionCur->StartupInfo.uSessionID != pSessionStartupInfo->uSessionID,
                        ("Guest session thread ID=%RU32 (%p) already exists when it should not\n",
                         pSessionCur->StartupInfo.uSessionID, pSessionCur), VERR_ALREADY_EXISTS);
    }
#endif

    /* Static counter to help tracking session thread <-> process relations. */
    static uint32_t s_uCtrlSessionThread = 0;
#if 1
    if (++s_uCtrlSessionThread == 100000)
#else /* This must be some joke, right? ;-) */
    if (s_uCtrlSessionThread++ == UINT32_MAX)
#endif
        s_uCtrlSessionThread = 0; /* Wrap around to not let IPRT freak out. */

    /*
     * Allocate and initialize the session thread structure.
     */
    int rc;
    PVBOXSERVICECTRLSESSIONTHREAD pSessionThread = (PVBOXSERVICECTRLSESSIONTHREAD)RTMemAllocZ(sizeof(*pSessionThread));
    if (pSessionThread)
    {
        //pSessionThread->fShutdown = false;
        //pSessionThread->fStarted  = false;
        //pSessionThread->fStopped  = false;
        pSessionThread->hKeyPipe  = NIL_RTPIPE;
        pSessionThread->Thread    = NIL_RTTHREAD;
        pSessionThread->hProcess  = NIL_RTPROCESS;

        /* Copy over session startup info. */
        memcpy(&pSessionThread->StartupInfo, pSessionStartupInfo, sizeof(VBOXSERVICECTRLSESSIONSTARTUPINFO));

        /* Generate the secret key. */
        RTRandBytes(pSessionThread->abKey, sizeof(pSessionThread->abKey));

        rc = RTCritSectInit(&pSessionThread->CritSect);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            /*
             * Give the session key to the host so it can validate the client.
             */
            if (VbglR3GuestCtrlSupportsOptimizations(g_idControlSvcClient))
            {
                for (uint32_t i = 0; i < 10; i++)
                {
                    rc = VbglR3GuestCtrlSessionPrepare(g_idControlSvcClient, pSessionStartupInfo->uSessionID,
                                                       pSessionThread->abKey, sizeof(pSessionThread->abKey));
                    if (rc != VERR_OUT_OF_RESOURCES)
                        break;
                    RTThreadSleep(100);
                }
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Start the session child process.
                 */
                rc = vgsvcVGSvcGstCtrlSessionThreadCreateProcess(pSessionStartupInfo, pSessionThread, s_uCtrlSessionThread);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Start the session thread.
                     */
                    rc = RTThreadCreateF(&pSessionThread->Thread, vgsvcGstCtrlSessionThread, pSessionThread /*pvUser*/, 0 /*cbStack*/,
                                         RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "CtrlSess%u", s_uCtrlSessionThread);
                    if (RT_SUCCESS(rc))
                    {
                        /* Wait for the thread to initialize. */
                        rc = RTThreadUserWait(pSessionThread->Thread, RT_MS_1MIN);
                        if (   RT_SUCCESS(rc)
                            && !ASMAtomicReadBool(&pSessionThread->fShutdown))
                        {
                            VGSvcVerbose(2, "Thread for session ID=%RU32 started\n", pSessionThread->StartupInfo.uSessionID);

                            ASMAtomicXchgBool(&pSessionThread->fStarted, true);

                            /* Add session to list. */
                            RTListAppend(pList, &pSessionThread->Node);
                            if (ppSessionThread) /* Return session if wanted. */
                                *ppSessionThread = pSessionThread;
                            return VINF_SUCCESS;
                        }

                        /*
                         * Bail out.
                         */
                        VGSvcError("Thread for session ID=%RU32 failed to start, rc=%Rrc\n",
                                   pSessionThread->StartupInfo.uSessionID, rc);
                        if (RT_SUCCESS_NP(rc))
                            rc = VERR_CANT_CREATE; /** @todo Find a better rc. */
                    }
                    else
                        VGSvcError("Creating session thread failed, rc=%Rrc\n", rc);

                    RTProcTerminate(pSessionThread->hProcess);
                    uint32_t cMsWait = 1;
                    while (   RTProcWait(pSessionThread->hProcess, RTPROCWAIT_FLAGS_NOBLOCK, NULL) == VERR_PROCESS_RUNNING
                           && cMsWait <= 9) /* 1023 ms */
                    {
                        RTThreadSleep(cMsWait);
                        cMsWait <<= 1;
                    }
                }

                if (VbglR3GuestCtrlSupportsOptimizations(g_idControlSvcClient))
                    VbglR3GuestCtrlSessionCancelPrepared(g_idControlSvcClient, pSessionStartupInfo->uSessionID);
            }
            else
                VGSvcVerbose(3, "VbglR3GuestCtrlSessionPrepare failed: %Rrc\n", rc);
            RTPipeClose(pSessionThread->hKeyPipe);
            pSessionThread->hKeyPipe = NIL_RTPIPE;
            RTCritSectDelete(&pSessionThread->CritSect);
        }
        RTMemFree(pSessionThread);
    }
    else
        rc = VERR_NO_MEMORY;

    VGSvcVerbose(3, "Spawning session thread returned returned rc=%Rrc\n", rc);
    return rc;
}


/**
 * Waits for a formerly opened guest session process to close.
 *
 * @return  VBox status code.
 * @param   pThread                 Guest session thread to wait for.
 * @param   uTimeoutMS              Waiting timeout (in ms).
 * @param   fFlags                  Closing flags.
 */
int VGSvcGstCtrlSessionThreadWait(PVBOXSERVICECTRLSESSIONTHREAD pThread, uint32_t uTimeoutMS, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    /** @todo Validate closing flags. */

    AssertMsgReturn(pThread->Thread != NIL_RTTHREAD,
                    ("Guest session thread of session %p does not exist when it should\n", pThread),
                    VERR_NOT_FOUND);

    int rc = VINF_SUCCESS;

    /*
     * The spawned session process should have received the same closing request,
     * so just wait for the process to close.
     */
    if (ASMAtomicReadBool(&pThread->fStarted))
    {
        /* Ask the thread to shutdown. */
        ASMAtomicXchgBool(&pThread->fShutdown, true);

        VGSvcVerbose(3, "Waiting for session thread ID=%RU32 to close (%RU32ms) ...\n",
                     pThread->StartupInfo.uSessionID, uTimeoutMS);

        int rcThread;
        rc = RTThreadWait(pThread->Thread, uTimeoutMS, &rcThread);
        if (RT_SUCCESS(rc))
            VGSvcVerbose(3, "Session thread ID=%RU32 ended with rc=%Rrc\n", pThread->StartupInfo.uSessionID, rcThread);
        else
            VGSvcError("Waiting for session thread ID=%RU32 to close failed with rc=%Rrc\n", pThread->StartupInfo.uSessionID, rc);
    }

    return rc;
}

/**
 * Waits for the specified session thread to end and remove
 * it from the session thread list.
 *
 * @return  VBox status code.
 * @param   pThread                 Session thread to destroy.
 * @param   fFlags                  Closing flags.
 */
int VGSvcGstCtrlSessionThreadDestroy(PVBOXSERVICECTRLSESSIONTHREAD pThread, uint32_t fFlags)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    int rc = VGSvcGstCtrlSessionThreadWait(pThread, 5 * 60 * 1000 /* 5 minutes timeout */, fFlags);

    /* Remove session from list and destroy object. */
    RTListNodeRemove(&pThread->Node);

    RTMemFree(pThread);
    pThread = NULL;

    return rc;
}

/**
 * Close all open guest session threads.
 *
 * @note    Caller is responsible for locking!
 *
 * @return  VBox status code.
 * @param   pList                   Which list to close the session threads for.
 * @param   fFlags                  Closing flags.
 */
int VGSvcGstCtrlSessionThreadDestroyAll(PRTLISTANCHOR pList, uint32_t fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    /*int rc = VbglR3GuestCtrlClose
        if (RT_FAILURE(rc))
            VGSvcError("Cancelling pending waits failed; rc=%Rrc\n", rc);*/

    PVBOXSERVICECTRLSESSIONTHREAD pSessIt;
    PVBOXSERVICECTRLSESSIONTHREAD pSessItNext;
    RTListForEachSafe(pList, pSessIt, pSessItNext, VBOXSERVICECTRLSESSIONTHREAD, Node)
    {
        int rc2 = VGSvcGstCtrlSessionThreadDestroy(pSessIt, fFlags);
        if (RT_FAILURE(rc2))
        {
            VGSvcError("Closing session thread '%s' failed with rc=%Rrc\n", RTThreadGetName(pSessIt->Thread), rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
            /* Keep going. */
        }
    }

    VGSvcVerbose(4, "Destroying guest session threads ended with %Rrc\n", rc);
    return rc;
}


/**
 * Main function for the session process.
 *
 * @returns exit code.
 * @param   argc        Argument count.
 * @param   argv        Argument vector (UTF-8).
 */
RTEXITCODE VGSvcGstCtrlSessionSpawnInit(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--domain",          VBOXSERVICESESSIONOPT_DOMAIN,          RTGETOPT_REQ_STRING },
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

    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    uint32_t fSession = VBOXSERVICECTRLSESSION_FLAG_SPAWN;

    /* Protocol and session ID must be specified explicitly. */
    g_Session.StartupInfo.uProtocol  = UINT32_MAX;
    g_Session.StartupInfo.uSessionID = UINT32_MAX;

    int ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case VBOXSERVICESESSIONOPT_DOMAIN:
                /* Information not needed right now, skip. */
                break;
#ifdef DEBUG
            case VBOXSERVICESESSIONOPT_DUMP_STDOUT:
                fSession |= VBOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT;
                break;

            case VBOXSERVICESESSIONOPT_DUMP_STDERR:
                fSession |= VBOXSERVICECTRLSESSION_FLAG_DUMPSTDERR;
                break;
#endif
            case VBOXSERVICESESSIONOPT_SESSION_ID:
                g_Session.StartupInfo.uSessionID = ValueUnion.u32;
                break;

            case VBOXSERVICESESSIONOPT_SESSION_PROTO:
                g_Session.StartupInfo.uProtocol = ValueUnion.u32;
                break;
#ifdef DEBUG
            case VBOXSERVICESESSIONOPT_THREAD_ID:
                /* Not handled. Mainly for processs listing. */
                break;
#endif
            case VBOXSERVICESESSIONOPT_LOG_FILE:
            {
                int rc = RTStrCopy(g_szLogFile, sizeof(g_szLogFile), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error copying log file name: %Rrc", rc);
                break;
            }

            case VBOXSERVICESESSIONOPT_USERNAME:
                /* Information not needed right now, skip. */
                break;

            /** @todo Implement help? */

            case 'v':
                g_cVerbosity++;
                break;

            case VINF_GETOPT_NOT_OPTION:
                /* Ignore; might be "guestsession" main command. */
                /** @todo r=bird: We DO NOT ignore stuff on the command line! */
                break;

            default:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Unknown command '%s'", ValueUnion.psz);
        }
    }

    /* Check that we've got all the required options. */
    if (g_Session.StartupInfo.uProtocol == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No protocol version specified");

    if (g_Session.StartupInfo.uSessionID == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No session ID specified");

    /* Init the session object. */
    int rc = VGSvcGstCtrlSessionInit(&g_Session, fSession);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_INIT, "Failed to initialize session object, rc=%Rrc\n", rc);

    rc = VGSvcLogCreate(g_szLogFile[0] ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_INIT, "Failed to create log file '%s', rc=%Rrc\n",
                              g_szLogFile[0] ? g_szLogFile : "<None>", rc);

    RTEXITCODE rcExit = vgsvcGstCtrlSessionSpawnWorker(&g_Session);

    VGSvcLogDestroy();
    return rcExit;
}

