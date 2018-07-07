/* $Id$ */
/** @file
 * VirtualBox Main - Guest session tasks.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_MAIN_GUESTSESSION
#include "LoggingNew.h"

#include "GuestImpl.h"
#ifndef VBOX_WITH_GUEST_CONTROL
# error "VBOX_WITH_GUEST_CONTROL must defined in this file"
#endif
#include "GuestSessionImpl.h"
#include "GuestSessionImplTasks.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"

#include <memory> /* For auto_ptr. */

#include <iprt/env.h>
#include <iprt/file.h> /* For CopyTo/From. */
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/fsvfs.h>


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/

/**
 * (Guest Additions) ISO file flags.
 * Needed for handling Guest Additions updates.
 */
#define ISOFILE_FLAG_NONE                0
/** Copy over the file from host to the
 *  guest. */
#define ISOFILE_FLAG_COPY_FROM_ISO       RT_BIT(0)
/** Execute file on the guest after it has
 *  been successfully transfered. */
#define ISOFILE_FLAG_EXECUTE             RT_BIT(7)
/** File is optional, does not have to be
 *  existent on the .ISO. */
#define ISOFILE_FLAG_OPTIONAL            RT_BIT(8)


// session task classes
/////////////////////////////////////////////////////////////////////////////

GuestSessionTask::GuestSessionTask(GuestSession *pSession)
    : ThreadTask("GenericGuestSessionTask")
{
    mSession = pSession;

    switch (mSession->i_getPathStyle())
    {
        case PathStyle_DOS:
            mfPathStyle = RTPATH_STR_F_STYLE_DOS;
            mPathStyle  = "\\";
            break;

        default:
            mfPathStyle = RTPATH_STR_F_STYLE_UNIX;
            mPathStyle  = "/";
            break;
    }
}

GuestSessionTask::~GuestSessionTask(void)
{
}

int GuestSessionTask::createAndSetProgressObject(ULONG cOperations /* = 1 */)
{
    LogFlowThisFunc(("cOperations=%ld\n", cOperations));

    /* Create the progress object. */
    ComObjPtr<Progress> pProgress;
    HRESULT hr = pProgress.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    hr = pProgress->init(static_cast<IGuestSession*>(mSession),
                         Bstr(mDesc).raw(),
                         TRUE /* aCancelable */, cOperations, Bstr(mDesc).raw());
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    mProgress = pProgress;

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

int GuestSessionTask::RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress)
{
    LogFlowThisFunc(("strDesc=%s\n", strDesc.c_str()));

    mDesc = strDesc;
    mProgress = pProgress;
    HRESULT hrc = createThreadWithType(RTTHREADTYPE_MAIN_HEAVY_WORKER);

    LogFlowThisFunc(("Returning hrc=%Rhrc\n", hrc));
    return Global::vboxStatusCodeToCOM(hrc);
}

int GuestSessionTask::getGuestProperty(const ComObjPtr<Guest> &pGuest,
                                       const Utf8Str &strPath, Utf8Str &strValue)
{
    ComObjPtr<Console> pConsole = pGuest->i_getConsole();
    const ComPtr<IMachine> pMachine = pConsole->i_machine();

    Assert(!pMachine.isNull());
    Bstr strTemp, strFlags;
    LONG64 i64Timestamp;
    HRESULT hr = pMachine->GetGuestProperty(Bstr(strPath).raw(),
                                            strTemp.asOutParam(),
                                            &i64Timestamp, strFlags.asOutParam());
    if (SUCCEEDED(hr))
    {
        strValue = strTemp;
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}

int GuestSessionTask::setProgress(ULONG uPercent)
{
    if (mProgress.isNull()) /* Progress is optional. */
        return VINF_SUCCESS;

    BOOL fCanceled;
    if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
        && fCanceled)
        return VERR_CANCELLED;
    BOOL fCompleted;
    if (   SUCCEEDED(mProgress->COMGETTER(Completed(&fCompleted)))
        && fCompleted)
    {
        AssertMsgFailed(("Setting value of an already completed progress\n"));
        return VINF_SUCCESS;
    }
    HRESULT hr = mProgress->SetCurrentOperationProgress(uPercent);
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    return VINF_SUCCESS;
}

int GuestSessionTask::setProgressSuccess(void)
{
    if (mProgress.isNull()) /* Progress is optional. */
        return VINF_SUCCESS;

    BOOL fCompleted;
    if (   SUCCEEDED(mProgress->COMGETTER(Completed(&fCompleted)))
        && !fCompleted)
    {
        HRESULT hr = mProgress->i_notifyComplete(S_OK);
        if (FAILED(hr))
            return VERR_COM_UNEXPECTED; /** @todo Find a better rc. */
    }

    return VINF_SUCCESS;
}

HRESULT GuestSessionTask::setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg)
{
    LogFlowFunc(("hr=%Rhrc, strMsg=%s\n",
                 hr, strMsg.c_str()));

    if (mProgress.isNull()) /* Progress is optional. */
        return hr; /* Return original rc. */

    BOOL fCanceled;
    BOOL fCompleted;
    if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
        && !fCanceled
        && SUCCEEDED(mProgress->COMGETTER(Completed(&fCompleted)))
        && !fCompleted)
    {
        HRESULT hr2 = mProgress->i_notifyComplete(hr,
                                                  COM_IIDOF(IGuestSession),
                                                  GuestSession::getStaticComponentName(),
                                                  strMsg.c_str());
        if (FAILED(hr2))
            return hr2;
    }
    return hr; /* Return original rc. */
}

/**
 * Creates a directory on the guest.
 *
 * @return VBox status code. VERR_ALREADY_EXISTS if directory on the guest already exists.
 * @param  strPath                  Absolute path to directory on the guest (guest style path) to create.
 * @param  enmDirectoryCreateFlags  Directory creation flags.
 * @param  uMode                    Directory mode to use for creation.
 * @param  fFollowSymlinks          Whether to follow symlinks on the guest or not.
 */
int GuestSessionTask::directoryCreate(const com::Utf8Str &strPath,
                                      DirectoryCreateFlag_T enmDirectoryCreateFlags, uint32_t uMode, bool fFollowSymlinks)
{
    LogFlowFunc(("strPath=%s, fFlags=0x%x, uMode=%RU32, fFollowSymlinks=%RTbool\n",
                 strPath.c_str(), enmDirectoryCreateFlags, uMode, fFollowSymlinks));

    GuestFsObjData objData; int rcGuest;
    int rc = mSession->i_directoryQueryInfo(strPath, fFollowSymlinks, objData, &rcGuest);
    if (RT_SUCCESS(rc))
    {
        return VERR_ALREADY_EXISTS;
    }
    else
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                switch (rcGuest)
                {
                    case VERR_FILE_NOT_FOUND:
                    case VERR_PATH_NOT_FOUND:
                        rc = mSession->i_directoryCreate(strPath.c_str(), uMode, enmDirectoryCreateFlags, &rcGuest);
                        break;
                    default:
                        break;
                }
                break;
            }

            default:
                break;
        }
    }

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_GSTCTL_GUEST_ERROR)
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestProcess::i_guestErrorToString(rcGuest));
        }
        else
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Error creating directory on the guest: %Rrc"), strPath.c_str(), rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function for copying a file from guest to the host.
 *
 * @return VBox status code.
 * @param  srcFile            Guest file (source) to copy to the host. Must be in opened and ready state already.
 * @param  phDstFile          Pointer to host file handle (destination) to copy to. Must be in opened and ready state already.
 * @param  fFileCopyFlags     File copy flags.
 * @param  offCopy            Offset (in bytes) where to start copying the source file.
 * @param  cbSize             Size (in bytes) to copy from the source file.
 */
int GuestSessionTask::fileCopyFromGuestInner(ComObjPtr<GuestFile> &srcFile, PRTFILE phDstFile, FileCopyFlag_T fFileCopyFlags,
                                            uint64_t offCopy, uint64_t cbSize)
{
    RT_NOREF(fFileCopyFlags);

    BOOL fCanceled = FALSE;
    uint64_t cbWrittenTotal = 0;
    uint64_t cbToRead       = cbSize;

    uint32_t uTimeoutMs = 30 * 1000; /* 30s timeout. */

    int rc = VINF_SUCCESS;

    if (offCopy)
    {
        uint64_t offActual;
        rc = srcFile->i_seekAt(offCopy, GUEST_FILE_SEEKTYPE_BEGIN, uTimeoutMs, &offActual);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Seeking to offset %RU64 failed: %Rrc"), offCopy, rc));
            return rc;
        }
    }

    BYTE byBuf[_64K];
    while (cbToRead)
    {
        uint32_t cbRead;
        const uint32_t cbChunk = RT_MIN(cbToRead, sizeof(byBuf));
        rc = srcFile->i_readData(cbChunk, uTimeoutMs, byBuf, sizeof(byBuf), &cbRead);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Reading %RU32 bytes @ %RU64 from guest failed: %Rrc"), cbChunk, cbWrittenTotal, rc));
            break;
        }

        rc = RTFileWrite(*phDstFile, byBuf, cbRead, NULL /* No partial writes */);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Writing %RU32 bytes to file on host failed: %Rrc"), cbRead, rc));
            break;
        }

        Assert(cbToRead >= cbRead);
        cbToRead -= cbRead;

        /* Update total bytes written to the guest. */
        cbWrittenTotal += cbRead;
        Assert(cbWrittenTotal <= cbSize);

        /* Did the user cancel the operation above? */
        if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
            && fCanceled)
            break;

        rc = setProgress((ULONG)(cbWrittenTotal / ((uint64_t)cbSize / 100.0)));
        if (RT_FAILURE(rc))
            break;
    }

    if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
        && fCanceled)
        return VINF_SUCCESS;

    if (RT_FAILURE(rc))
        return rc;

    /*
     * Even if we succeeded until here make sure to check whether we really transfered
     * everything.
     */
    if (   cbSize > 0
        && cbWrittenTotal == 0)
    {
        /* If nothing was transfered but the file size was > 0 then "vbox_cat" wasn't able to write
         * to the destination -> access denied. */
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Writing guest file to host failed: Access denied")));
        rc = VERR_ACCESS_DENIED;
    }
    else if (cbWrittenTotal < cbSize)
    {
        /* If we did not copy all let the user know. */
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Copying guest file to host to failed (%RU64/%RU64 bytes transfered)"),
                                       cbWrittenTotal, cbSize));
        rc = VERR_INTERRUPTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Copies a file from the guest to the host.
 *
 * @return VBox status code. VINF_NO_CHANGE if file was skipped.
 * @param  strSource            Full path of source file on the guest to copy.
 * @param  strDest              Full destination path and file name (host style) to copy file to.
 * @param  fFileCopyFlags       File copy flags.
 */
int GuestSessionTask::fileCopyFromGuest(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags)
{
    LogFlowThisFunc(("strSource=%s, strDest=%s, enmFileCopyFlags=0x%x\n", strSource.c_str(), strDest.c_str(), fFileCopyFlags));

    GuestFileOpenInfo srcOpenInfo;
    RT_ZERO(srcOpenInfo);
    srcOpenInfo.mFileName     = strSource;
    srcOpenInfo.mOpenAction   = FileOpenAction_OpenExisting;
    srcOpenInfo.mAccessMode   = FileAccessMode_ReadOnly;
    srcOpenInfo.mSharingMode  = FileSharingMode_All; /** @todo Use _Read when implemented. */

    ComObjPtr<GuestFile> srcFile;

    GuestFsObjData srcObjData;
    int rcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int rc = mSession->i_fsQueryInfo(strSource, TRUE /* fFollowSymlinks */, srcObjData, &rcGuest);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestFile::i_guestErrorToString(rcGuest));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Source file lookup for \"%s\" failed: %Rrc"),
                                               strSource.c_str(), rc));
                break;
        }
    }
    else
    {
        switch (srcObjData.mType)
        {
            case FsObjType_File:
                break;

            case FsObjType_Symlink:
                if (!(fFileCopyFlags & FileCopyFlag_FollowLinks))
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Source file \"%s\" is a symbolic link"),
                                                   strSource.c_str(), rc));
                    rc = VERR_IS_A_SYMLINK;
                }
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Source element \"%s\" is not a file"), strSource.c_str()));
                rc = VERR_NOT_A_FILE;
                break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = mSession->i_fileOpen(srcOpenInfo, srcFile, &rcGuest);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestFile::i_guestErrorToString(rcGuest));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Source file \"%s\" could not be opened: %Rrc"),
                                               strSource.c_str(), rc));
                break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    char *pszDstFile = NULL;
    RTFSOBJINFO dstObjInfo;
    RT_ZERO(dstObjInfo);

    bool fSkip = false; /* Whether to skip handling the file. */

    if (RT_SUCCESS(rc))
    {
        rc = RTPathQueryInfo(strDest.c_str(), &dstObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            if (fFileCopyFlags & FileCopyFlag_NoReplace)
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file \"%s\" already exists"), strDest.c_str()));
                rc = VERR_ALREADY_EXISTS;
            }

            if (fFileCopyFlags & FileCopyFlag_Update)
            {
                RTTIMESPEC srcModificationTimeTS;
                RTTimeSpecSetSeconds(&srcModificationTimeTS, srcObjData.mModificationTime);
                if (RTTimeSpecCompare(&srcModificationTimeTS, &dstObjInfo.ModificationTime) <= 0)
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Destination file \"%s\" has same or newer modification date"),
                                                   strDest.c_str()));
                    fSkip = true;
                }
            }
        }
        else
        {
            if (rc != VERR_FILE_NOT_FOUND) /* Destination file does not exist (yet)? */
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file lookup for \"%s\" failed: %Rrc"),
                                               strDest.c_str(), rc));
        }
    }

    if (fSkip)
    {
        int rc2 = srcFile->i_closeFile(&rcGuest);
        AssertRC(rc2);
        return VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_FILE(dstObjInfo.Attr.fMode))
        {
            if (fFileCopyFlags & FileCopyFlag_NoReplace)
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file \"%s\" already exists"),
                                               strDest.c_str(), rc));
                rc = VERR_ALREADY_EXISTS;
            }
            else
                pszDstFile = RTStrDup(strDest.c_str());
        }
        else if (RTFS_IS_DIRECTORY(dstObjInfo.Attr.fMode))
        {
            /* Build the final file name with destination path (on the host). */
            char szDstPath[RTPATH_MAX];
            RTStrPrintf2(szDstPath, sizeof(szDstPath), "%s", strDest.c_str());

            if (   !strDest.endsWith("\\")
                && !strDest.endsWith("/"))
                RTPathAppend(szDstPath, sizeof(szDstPath), "/"); /* IPRT can handle / on all hosts. */

            RTPathAppend(szDstPath, sizeof(szDstPath), RTPathFilenameEx(strSource.c_str(), mfPathStyle));

            pszDstFile = RTStrDup(szDstPath);
        }
        else if (RTFS_IS_SYMLINK(dstObjInfo.Attr.fMode))
        {
            if (!(fFileCopyFlags & FileCopyFlag_FollowLinks))
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file \"%s\" is a symbolic link"),
                                               strDest.c_str(), rc));
                rc = VERR_IS_A_SYMLINK;
            }
            else
                pszDstFile = RTStrDup(strDest.c_str());
        }
        else
        {
            LogFlowThisFunc(("Object type %RU32 not implemented yet\n", dstObjInfo.Attr.fMode));
            rc = VERR_NOT_IMPLEMENTED;
        }
    }
    else if (rc == VERR_FILE_NOT_FOUND)
        pszDstFile = RTStrDup(strDest.c_str());

    if (   RT_SUCCESS(rc)
        || rc == VERR_FILE_NOT_FOUND)
    {
        if (!pszDstFile)
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR, Utf8StrFmt(GuestSession::tr("No memory to allocate destination file path")));
            rc = VERR_NO_MEMORY;
        }
        else
        {
            RTFILE hDstFile;
            rc = RTFileOpen(&hDstFile, pszDstFile,
                            RTFILE_O_WRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE); /** @todo Use the correct open modes! */
            if (RT_SUCCESS(rc))
            {
                LogFlowThisFunc(("Copying '%s' to '%s' (%RI64 bytes) ...\n", strSource.c_str(), pszDstFile, srcObjData.mObjectSize));

                rc = fileCopyFromGuestInner(srcFile, &hDstFile, fFileCopyFlags, 0 /* Offset, unused */, (uint64_t)srcObjData.mObjectSize);

                int rc2 = RTFileClose(hDstFile);
                AssertRC(rc2);
            }
            else
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Opening/creating destination file \"%s\" failed: %Rrc"),
                                               pszDstFile, rc));
        }
    }

    RTStrFree(pszDstFile);

    int rc2 = srcFile->i_closeFile(&rcGuest);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function for copying a file from host to the guest.
 *
 * @return VBox status code.
 * @param  hVfsFIle           The VFS file handle to read from.
 * @param  dstFile            Guest file (destination) to copy to the guest. Must be in opened and ready state already.
 * @param  fFileCopyFlags     File copy flags.
 * @param  offCopy            Offset (in bytes) where to start copying the source file.
 * @param  cbSize             Size (in bytes) to copy from the source file.
 */
int GuestSessionTask::fileCopyToGuestInner(RTVFSFILE hVfsFile, ComObjPtr<GuestFile> &dstFile, FileCopyFlag_T fFileCopyFlags,
                                           uint64_t offCopy, uint64_t cbSize)
{
    RT_NOREF(fFileCopyFlags);

    BOOL fCanceled = FALSE;
    uint64_t cbWrittenTotal = 0;
    uint64_t cbToRead       = cbSize;

    uint32_t uTimeoutMs = 30 * 1000; /* 30s timeout. */

    int rc = VINF_SUCCESS;

    if (offCopy)
    {
        uint64_t offActual;
        rc = RTVfsFileSeek(hVfsFile, offCopy, RTFILE_SEEK_END, &offActual);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Seeking to offset %RU64 failed: %Rrc"), offCopy, rc));
            return rc;
        }
    }

    BYTE byBuf[_64K];
    while (cbToRead)
    {
        size_t cbRead;
        const uint32_t cbChunk = RT_MIN(cbToRead, sizeof(byBuf));
        rc = RTVfsFileRead(hVfsFile, byBuf, cbChunk, &cbRead);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Reading %RU32 bytes @ %RU64 from host failed: %Rrc"), cbChunk, cbWrittenTotal, rc));
            break;
        }

        rc = dstFile->i_writeData(uTimeoutMs, byBuf, (uint32_t)cbRead, NULL /* No partial writes */);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Writing %zu bytes to file on guest failed: %Rrc"), cbRead, rc));
            break;
        }

        Assert(cbToRead >= cbRead);
        cbToRead -= cbRead;

        /* Update total bytes written to the guest. */
        cbWrittenTotal += cbRead;
        Assert(cbWrittenTotal <= cbSize);

        /* Did the user cancel the operation above? */
        if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
            && fCanceled)
            break;

        rc = setProgress((ULONG)(cbWrittenTotal / ((uint64_t)cbSize / 100.0)));
        if (RT_FAILURE(rc))
            break;
    }

    if (RT_FAILURE(rc))
        return rc;

    /*
     * Even if we succeeded until here make sure to check whether we really transfered
     * everything.
     */
    if (   cbSize > 0
        && cbWrittenTotal == 0)
    {
        /* If nothing was transfered but the file size was > 0 then "vbox_cat" wasn't able to write
         * to the destination -> access denied. */
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Writing to destination file failed: Access denied")));
        rc = VERR_ACCESS_DENIED;
    }
    else if (cbWrittenTotal < cbSize)
    {
        /* If we did not copy all let the user know. */
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Copying to destination failed (%RU64/%RU64 bytes transfered)"),
                                       cbWrittenTotal, cbSize));
        rc = VERR_INTERRUPTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Copies a file from the guest to the host.
 *
 * @return VBox status code. VINF_NO_CHANGE if file was skipped.
 * @param  strSource            Full path of source file on the host to copy.
 * @param  strDest              Full destination path and file name (guest style) to copy file to.
 * @param  fFileCopyFlags       File copy flags.
 */
int GuestSessionTask::fileCopyToGuest(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags)
{
    LogFlowThisFunc(("strSource=%s, strDest=%s, fFileCopyFlags=0x%x\n", strSource.c_str(), strDest.c_str(), fFileCopyFlags));

    Utf8Str strDestFinal = strDest;

    GuestFsObjData dstObjData;
    int rcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int rc = mSession->i_fsQueryInfo(strDest, TRUE /* fFollowSymlinks */, dstObjData, &rcGuest);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                if (rcGuest == VERR_FILE_NOT_FOUND) /* File might not exist on the guest yet. */
                {
                    rc = VINF_SUCCESS;
                }
                else
                   setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestFile::i_guestErrorToString(rcGuest));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file lookup for \"%s\" failed: %Rrc"),
                                               strDest.c_str(), rc));
                break;
        }
    }
    else
    {
        switch (dstObjData.mType)
        {
            case FsObjType_Directory:
            {
                /* Build the final file name with destination path (on the host). */
                char szDstPath[RTPATH_MAX];
                RTStrPrintf2(szDstPath, sizeof(szDstPath), "%s", strDest.c_str());

                if (   !strDest.endsWith("\\")
                    && !strDest.endsWith("/"))
                    RTStrCat(szDstPath, sizeof(szDstPath), "/");

                RTStrCat(szDstPath, sizeof(szDstPath), RTPathFilename(strSource.c_str()));

                strDestFinal = szDstPath;
                break;
            }

            case FsObjType_File:
                if (fFileCopyFlags & FileCopyFlag_NoReplace)
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Destination file \"%s\" already exists"),
                                                   strDest.c_str(), rc));
                    rc = VERR_ALREADY_EXISTS;
                }
                break;

            case FsObjType_Symlink:
                if (!(fFileCopyFlags & FileCopyFlag_FollowLinks))
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Destination file \"%s\" is a symbolic link"),
                                                   strDest.c_str(), rc));
                    rc = VERR_IS_A_SYMLINK;
                }
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination element \"%s\" not supported"), strDest.c_str()));
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    GuestFileOpenInfo dstOpenInfo;
    RT_ZERO(dstOpenInfo);
    dstOpenInfo.mFileName        = strDestFinal;
    if (fFileCopyFlags & FileCopyFlag_NoReplace)
        dstOpenInfo.mOpenAction  = FileOpenAction_CreateNew;
    else
        dstOpenInfo.mOpenAction  = FileOpenAction_CreateOrReplace;
    dstOpenInfo.mAccessMode      = FileAccessMode_WriteOnly;
    dstOpenInfo.mSharingMode     = FileSharingMode_All; /** @todo Use _Read when implemented. */

    ComObjPtr<GuestFile> dstFile;
    rc = mSession->i_fileOpen(dstOpenInfo, dstFile, &rcGuest);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestFile::i_guestErrorToString(rcGuest));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination file \"%s\" could not be opened: %Rrc"),
                                               strDestFinal.c_str(), rc));
                break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    char szSrcReal[RTPATH_MAX];

    RTFSOBJINFO srcObjInfo;
    RT_ZERO(srcObjInfo);

    bool fSkip = false; /* Whether to skip handling the file. */

    if (RT_SUCCESS(rc))
    {
        rc = RTPathReal(strSource.c_str(), szSrcReal, sizeof(szSrcReal));
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Source path lookup for \"%s\" failed: %Rrc"),
                                           strSource.c_str(), rc));
        }
        else
        {
            rc = RTPathQueryInfo(szSrcReal, &srcObjInfo, RTFSOBJATTRADD_NOTHING);
            if (RT_SUCCESS(rc))
            {
                if (fFileCopyFlags & FileCopyFlag_Update)
                {
                    RTTIMESPEC dstModificationTimeTS;
                    RTTimeSpecSetSeconds(&dstModificationTimeTS, dstObjData.mModificationTime);
                    if (RTTimeSpecCompare(&dstModificationTimeTS, &srcObjInfo.ModificationTime) <= 0)
                    {
                        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                            Utf8StrFmt(GuestSession::tr("Destination file \"%s\" has same or newer modification date"),
                                                       strDestFinal.c_str()));
                        fSkip = true;
                    }
                }
            }
            else
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Source file lookup for \"%s\" failed: %Rrc"),
                                               szSrcReal, rc));
            }
        }
    }

    if (fSkip)
    {
        int rc2 = dstFile->i_closeFile(&rcGuest);
        AssertRC(rc2);
        return VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
        RTVFSFILE hSrcFile;
        rc = RTVfsFileOpenNormal(szSrcReal, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hSrcFile); /** @todo Use the correct open modes! */
        if (RT_SUCCESS(rc))
        {
            LogFlowThisFunc(("Copying '%s' to '%s' (%RI64 bytes) ...\n",
                             szSrcReal, strDestFinal.c_str(), srcObjInfo.cbObject));

            rc = fileCopyToGuestInner(hSrcFile, dstFile, fFileCopyFlags, 0 /* Offset, unused */, srcObjInfo.cbObject);

            int rc2 = RTVfsFileRelease(hSrcFile);
            AssertRC(rc2);
        }
        else
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Opening source file \"%s\" failed: %Rrc"),
                                           szSrcReal, rc));
    }

    int rc2 = dstFile->i_closeFile(&rcGuest);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds a guest file system entry to a given list.
 *
 * @return VBox status code.
 * @param  strFile              Path to file system entry to add.
 * @param  fsObjData            Guest file system information of entry to add.
 */
int FsList::AddEntryFromGuest(const Utf8Str &strFile, const GuestFsObjData &fsObjData)
{
    LogFlowFunc(("Adding '%s'\n", strFile.c_str()));

    FsEntry *pEntry = NULL;
    try
    {
        pEntry = new FsEntry();
        pEntry->fMode = fsObjData.GetFileMode();
        pEntry->strPath = strFile;

        mVecEntries.push_back(pEntry);
    }
    catch (...)
    {
        if (pEntry)
            delete pEntry;
        return VERR_NO_MEMORY;
    }

    return VINF_SUCCESS;
}

/**
 * Adds a host file system entry to a given list.
 *
 * @return VBox status code.
 * @param  strFile              Path to file system entry to add.
 * @param  pcObjInfo            File system information of entry to add.
 */
int FsList::AddEntryFromHost(const Utf8Str &strFile, PCRTFSOBJINFO pcObjInfo)
{
    LogFlowFunc(("Adding '%s'\n", strFile.c_str()));

    FsEntry *pEntry = NULL;
    try
    {
        pEntry = new FsEntry();
        pEntry->fMode = pcObjInfo->Attr.fMode & RTFS_TYPE_MASK;
        pEntry->strPath = strFile;

        mVecEntries.push_back(pEntry);
    }
    catch (...)
    {
        if (pEntry)
            delete pEntry;
        return VERR_NO_MEMORY;
    }

    return VINF_SUCCESS;
}

FsList::FsList(const GuestSessionTask &Task)
    : mTask(Task)
{
}

FsList::~FsList()
{
    Destroy();
}

/**
 * Initializes a file list.
 *
 * @return VBox status code.
 * @param  strSrcRootAbs        Source root path (absolute) for this file list.
 * @param  strDstRootAbs        Destination root path (absolute) for this file list.
 * @param  SourceSpec           Source specification to use.
 */
int FsList::Init(const Utf8Str &strSrcRootAbs, const Utf8Str &strDstRootAbs,
                 const GuestSessionFsSourceSpec &SourceSpec)
{
    mSrcRootAbs = strSrcRootAbs;
    mDstRootAbs = strDstRootAbs;
    mSourceSpec = SourceSpec;

    /* If the source is a directory, make sure the path is properly terminated already. */
    if (mSourceSpec.enmType == FsObjType_Directory)
    {
        if (   !mSrcRootAbs.endsWith("/")
            && !mSrcRootAbs.endsWith("\\"))
            mSrcRootAbs += "/";

        if (   !mDstRootAbs.endsWith("/")
            && !mDstRootAbs.endsWith("\\"))
            mDstRootAbs += "/";
    }

    return VINF_SUCCESS;
}

/**
 * Destroys a file list.
 */
void FsList::Destroy(void)
{
    LogFlowFuncEnter();

    FsEntries::iterator itEntry = mVecEntries.begin();
    while (itEntry != mVecEntries.end())
    {
        FsEntry *pEntry = *itEntry;
        delete pEntry;
        mVecEntries.erase(itEntry);
        itEntry = mVecEntries.begin();
    }

    Assert(mVecEntries.empty());

    LogFlowFuncLeave();
}

/**
 * Builds a guest file list from a given path (and optional filter).
 *
 * @return VBox status code.
 * @param  strPath              Directory on the guest to build list from.
 * @param  strSubDir            Current sub directory path; needed for recursion.
 *                              Set to an empty path.
 */
int FsList::AddDirFromGuest(const Utf8Str &strPath, const Utf8Str &strSubDir /* = "" */)
{
    Utf8Str strPathAbs = strPath;
    if (   !strPathAbs.endsWith("/")
        && !strPathAbs.endsWith("\\"))
        strPathAbs += "/";

    Utf8Str strPathSub = strSubDir;
    if (   strPathSub.isNotEmpty()
        && !strPathSub.endsWith("/")
        && !strPathSub.endsWith("\\"))
        strPathSub += "/";

    strPathAbs += strPathSub;

    LogFlowFunc(("Entering '%s' (sub '%s')\n", strPathAbs.c_str(), strPathSub.c_str()));

    GuestDirectoryOpenInfo dirOpenInfo;
    dirOpenInfo.mFilter = "";
    dirOpenInfo.mPath   = strPathAbs;
    dirOpenInfo.mFlags  = 0; /** @todo Handle flags? */

    const ComObjPtr<GuestSession> &pSession = mTask.GetSession();

    ComObjPtr <GuestDirectory> pDir; int rcGuest;
    int rc = pSession->i_directoryOpen(dirOpenInfo, pDir, &rcGuest);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               break;

            case VERR_GSTCTL_GUEST_ERROR:
                break;

            default:
                break;
        }

        return rc;
    }

    if (strPathSub.isNotEmpty())
    {
        GuestFsObjData fsObjData;
        fsObjData.mType = FsObjType_Directory;

        rc = AddEntryFromGuest(strPathSub, fsObjData);
    }

    if (RT_SUCCESS(rc))
    {
        ComObjPtr<GuestFsObjInfo> fsObjInfo;
        while (RT_SUCCESS(rc = pDir->i_readInternal(fsObjInfo, &rcGuest)))
        {
            FsObjType_T enmObjType = FsObjType_Unknown; /* Shut up MSC. */
            HRESULT hr2 = fsObjInfo->COMGETTER(Type)(&enmObjType);
            AssertComRC(hr2);

            com::Bstr bstrName;
            hr2 = fsObjInfo->COMGETTER(Name)(bstrName.asOutParam());
            AssertComRC(hr2);

            Utf8Str strEntry = strPathSub + Utf8Str(bstrName);

            LogFlowFunc(("Entry '%s'\n", strEntry.c_str()));

            switch (enmObjType)
            {
                case FsObjType_Directory:
                {
                    if (   bstrName.equals(".")
                        || bstrName.equals(".."))
                    {
                        break;
                    }

                    if (!(mSourceSpec.Type.Dir.fRecursive))
                        break;

                    rc = AddDirFromGuest(strPath, strEntry);
                    break;
                }

                case FsObjType_Symlink:
                {
                    if (mSourceSpec.fFollowSymlinks)
                    {
                        /** @todo Symlink handling from guest is not imlemented yet.
                         *        See IGuestSession::symlinkRead(). */
                    }
                    break;
                }

                case FsObjType_File:
                {
                    rc = AddEntryFromGuest(strEntry, fsObjInfo->i_getData());
                    break;
                }

                default:
                    break;
            }
        }

        if (rc == VERR_NO_MORE_FILES) /* End of listing reached? */
            rc = VINF_SUCCESS;
    }

    int rc2 = pDir->i_closeInternal(&rcGuest);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

/**
 * Builds a host file list from a given path (and optional filter).
 *
 * @return VBox status code.
 * @param  strPath              Directory on the host to build list from.
 * @param  strSubDir            Current sub directory path; needed for recursion.
 *                              Set to an empty path.
 */
int FsList::AddDirFromHost(const Utf8Str &strPath, const Utf8Str &strSubDir)
{
    Utf8Str strPathAbs = strPath;
    if (   !strPathAbs.endsWith("/")
        && !strPathAbs.endsWith("\\"))
        strPathAbs += "/";

    Utf8Str strPathSub = strSubDir;
    if (   strPathSub.isNotEmpty()
        && !strPathSub.endsWith("/")
        && !strPathSub.endsWith("\\"))
        strPathSub += "/";

    strPathAbs += strPathSub;

    LogFlowFunc(("Entering '%s' (sub '%s')\n", strPathAbs.c_str(), strPathSub.c_str()));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(strPathAbs.c_str(), &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            if (strPathSub.isNotEmpty())
                rc = AddEntryFromHost(strPathSub, &objInfo);

            if (RT_SUCCESS(rc))
            {
                RTDIR hDir;
                rc = RTDirOpen(&hDir, strPathAbs.c_str());
                if (RT_SUCCESS(rc))
                {
                    do
                    {
                        /* Retrieve the next directory entry. */
                        RTDIRENTRYEX Entry;
                        rc = RTDirReadEx(hDir, &Entry, NULL, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                        if (RT_FAILURE(rc))
                        {
                            if (rc == VERR_NO_MORE_FILES)
                                rc = VINF_SUCCESS;
                            break;
                        }

                        Utf8Str strEntry = strPathSub + Utf8Str(Entry.szName);

                        LogFlowFunc(("Entry '%s'\n", strEntry.c_str()));

                        switch (Entry.Info.Attr.fMode & RTFS_TYPE_MASK)
                        {
                            case RTFS_TYPE_DIRECTORY:
                            {
                                /* Skip "." and ".." entries. */
                                if (RTDirEntryExIsStdDotLink(&Entry))
                                    break;

                                if (!(mSourceSpec.Type.Dir.fRecursive))
                                    break;

                                rc = AddDirFromHost(strPath, strEntry);
                                break;
                            }

                            case RTFS_TYPE_FILE:
                            {
                                rc = AddEntryFromHost(strEntry, &Entry.Info);
                                break;
                            }

                            case RTFS_TYPE_SYMLINK:
                            {
                                if (mSourceSpec.fFollowSymlinks)
                                {
                                    Utf8Str strEntryAbs = strPathAbs + Utf8Str(Entry.szName);

                                    char szPathReal[RTPATH_MAX];
                                    rc = RTPathReal(strEntryAbs.c_str(), szPathReal, sizeof(szPathReal));
                                    if (RT_SUCCESS(rc))
                                    {
                                        rc = RTPathQueryInfo(szPathReal, &objInfo, RTFSOBJATTRADD_NOTHING);
                                        if (RT_SUCCESS(rc))
                                        {
                                            LogFlowFunc(("Symlink '%s' -> '%s'\n", strEntryAbs.c_str(), szPathReal));

                                            if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                                            {
                                                LogFlowFunc(("Symlink to directory\n"));
                                                rc = AddDirFromHost(strPath, strEntry);
                                            }
                                            else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                                            {
                                                LogFlowFunc(("Symlink to file\n"));
                                                rc = AddEntryFromHost(strEntry, &objInfo);
                                            }
                                            else
                                                rc = VERR_NOT_SUPPORTED;
                                        }
                                       else
                                           LogFlowFunc(("Unable to query symlink info for '%s', rc=%Rrc\n", szPathReal, rc));
                                    }
                                    else
                                    {
                                        LogFlowFunc(("Unable to resolve symlink for '%s', rc=%Rrc\n", strPathAbs.c_str(), rc));
                                        if (rc == VERR_FILE_NOT_FOUND) /* Broken symlink, skip. */
                                            rc = VINF_SUCCESS;
                                    }
                                }
                                break;
                            }

                            default:
                                break;
                        }

                    } while (RT_SUCCESS(rc));

                    RTDirClose(hDir);
                }
            }
        }
        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
        {
            rc = VERR_IS_A_FILE;
        }
        else if (RTFS_IS_SYMLINK(objInfo.Attr.fMode))
        {
            rc = VERR_IS_A_SYMLINK;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        LogFlowFunc(("Unable to query '%s', rc=%Rrc\n", strPathAbs.c_str(), rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

GuestSessionTaskOpen::GuestSessionTaskOpen(GuestSession *pSession, uint32_t uFlags, uint32_t uTimeoutMS)
                                           : GuestSessionTask(pSession)
                                           , mFlags(uFlags)
                                           , mTimeoutMS(uTimeoutMS)
{
    m_strTaskName = "gctlSesOpen";
}

GuestSessionTaskOpen::~GuestSessionTaskOpen(void)
{

}

int GuestSessionTaskOpen::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int vrc = mSession->i_startSession(NULL /*pvrcGuest*/);
    /* Nothing to do here anymore. */

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

GuestSessionCopyTask::GuestSessionCopyTask(GuestSession *pSession)
                                           : GuestSessionTask(pSession)
{
}

GuestSessionCopyTask::~GuestSessionCopyTask()
{
    FsLists::iterator itList = mVecLists.begin();
    while (itList != mVecLists.end())
    {
        FsList *pFsList = (*itList);
        pFsList->Destroy();
        delete pFsList;
        mVecLists.erase(itList);
        itList = mVecLists.begin();
    }

    Assert(mVecLists.empty());
}

GuestSessionTaskCopyFrom::GuestSessionTaskCopyFrom(GuestSession *pSession, GuestSessionFsSourceSet vecSrc, const Utf8Str &strDest)
                                                   : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyFrm";

    mSources = vecSrc;
    mDest    = strDest;
}

GuestSessionTaskCopyFrom::~GuestSessionTaskCopyFrom(void)
{
}

HRESULT GuestSessionTaskCopyFrom::Init(const Utf8Str &strTaskDesc)
{
    setTaskDesc(strTaskDesc);

    /* Create the progress object. */
    ComObjPtr<Progress> pProgress;
    HRESULT hr = pProgress.createObject();
    if (FAILED(hr))
        return hr;

    mProgress = pProgress;

    int rc = VINF_SUCCESS;

    ULONG cOperations = 0;
    Utf8Str strErrorInfo;

    /**
     * Note: We need to build up the file/directory here instead of GuestSessionTaskCopyFrom::Run
     *       because the caller expects a ready-for-operation progress object on return.
     *       The progress object will have a variable operation count, based on the elements to
     *       be processed.
     */

    GuestSessionFsSourceSet::iterator itSrc = mSources.begin();
    while (itSrc != mSources.end())
    {
        Utf8Str strSrc = itSrc->strSource;
        Utf8Str strDst = mDest;

        if (itSrc->enmType == FsObjType_Directory)
        {
            /* If the source does not end with a slash, copy over the entire directory
             * (and not just its contents). */
            if (   !strSrc.endsWith("/")
                && !strSrc.endsWith("\\"))
            {
                if (   !strDst.endsWith("/")
                    && !strDst.endsWith("\\"))
                    strDst += "/";

                strDst += Utf8StrFmt("%s", RTPathFilenameEx(strSrc.c_str(), mfPathStyle));
            }
        }

        LogFlowFunc(("strSrc=%s, strDst=%s\n", strSrc.c_str(), strDst.c_str()));

        GuestFsObjData srcObjData; int rcGuest;
        rc = mSession->i_fsQueryInfo(strSrc, itSrc->fFollowSymlinks ? TRUE : FALSE,
                                     srcObjData, &rcGuest);
        if (RT_FAILURE(rc))
        {
            strErrorInfo = Utf8StrFmt(GuestSession::tr("No such source file/directory: %s"), strSrc.c_str());
            break;
        }

        if (srcObjData.mType == FsObjType_Directory)
        {
            if (itSrc->enmType != FsObjType_Directory)
            {
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Source is not a file: %s"), strSrc.c_str());
                rc = VERR_NOT_A_FILE;
                break;
            }
        }
        else
        {
            if (itSrc->enmType != FsObjType_File)
            {
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Source is not a directory: %s"), strSrc.c_str());
                rc = VERR_NOT_A_DIRECTORY;
                break;
            }
        }

        FsList *pFsList = NULL;
        try
        {
            pFsList = new FsList(*this);
            rc = pFsList->Init(strSrc, strDst, *itSrc);
            if (RT_SUCCESS(rc))
            {
                if (itSrc->enmType == FsObjType_Directory)
                {
                    rc = pFsList->AddDirFromGuest(strSrc);
                }
                else
                    rc = pFsList->AddEntryFromGuest(RTPathFilename(strSrc.c_str()), srcObjData);
            }

            if (RT_FAILURE(rc))
            {
                delete pFsList;
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Error adding source '%s' to list: %Rrc"), strSrc.c_str(), rc);
                break;
            }

            mVecLists.push_back(pFsList);
        }
        catch (...)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        AssertPtr(pFsList);
        cOperations += (ULONG)pFsList->mVecEntries.size();

        itSrc++;
    }

    if (cOperations) /* Use the first element as description (if available). */
    {
        Assert(mVecLists.size());
        Assert(mVecLists[0]->mVecEntries.size());

        Utf8Str strFirstOp = mDest + mVecLists[0]->mVecEntries[0]->strPath;
        hr = pProgress->init(static_cast<IGuestSession*>(mSession), Bstr(mDesc).raw(),
                             TRUE /* aCancelable */, cOperations + 1 /* Number of operations */,
                             Bstr(strFirstOp).raw());
    }
    else /* If no operations have been defined, go with an "empty" progress object when will be used for error handling. */
        hr = pProgress->init(static_cast<IGuestSession*>(mSession), Bstr(mDesc).raw(),
                             TRUE /* aCancelable */, 1 /* cOperations */, Bstr(mDesc).raw());

    if (RT_FAILURE(rc))
    {
        Assert(strErrorInfo.isNotEmpty());
        setProgressErrorMsg(VBOX_E_IPRT_ERROR, strErrorInfo);
    }

    LogFlowFunc(("Returning %Rhrc (%Rrc)\n", hr, rc));
    return rc;
}

int GuestSessionTaskCopyFrom::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = VINF_SUCCESS;

    FsLists::const_iterator itList = mVecLists.begin();
    while (itList != mVecLists.end())
    {
        FsList *pList = *itList;
        AssertPtr(pList);

        const bool     fCopyIntoExisting = pList->mSourceSpec.Type.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting;
        const uint32_t fDirMode          = 0700; /** @todo Play safe by default; implement ACLs. */

        LogFlowFunc(("List: srcRootAbs=%s, dstRootAbs=%s\n", pList->mSrcRootAbs.c_str(), pList->mDstRootAbs.c_str()));

        /* Create the root directory. */
        if (pList->mSourceSpec.enmType == FsObjType_Directory)
        {
            rc = RTDirCreate(pList->mDstRootAbs.c_str(), fDirMode, 0 /* fCreate */);
            if (   rc == VWRN_ALREADY_EXISTS
                && !fCopyIntoExisting)
            {
                break;
            }
        }

        FsEntries::const_iterator itEntry = pList->mVecEntries.begin();
        while (itEntry != pList->mVecEntries.end())
        {
            FsEntry *pEntry = *itEntry;
            AssertPtr(pEntry);

            Utf8Str strSrcAbs = pList->mSrcRootAbs;
            Utf8Str strDstAbs = pList->mDstRootAbs;
            if (pList->mSourceSpec.enmType == FsObjType_Directory)
            {
                strSrcAbs += pEntry->strPath;
                strDstAbs += pEntry->strPath;

                if (pList->mSourceSpec.enmPathStyle == PathStyle_DOS)
                    strDstAbs.findReplace('\\', '/');
            }

            switch (pEntry->fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                    LogFlowFunc(("Directory '%s': %s -> %s\n", pEntry->strPath.c_str(), strSrcAbs.c_str(), strDstAbs.c_str()));
                    if (!pList->mSourceSpec.fDryRun)
                    {
                        rc = RTDirCreate(strDstAbs.c_str(), fDirMode, 0 /* fCreate */);
                        if (rc == VERR_ALREADY_EXISTS)
                        {
                            if (!fCopyIntoExisting)
                            {
                                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                    Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" already exists"),
                                                               strDstAbs.c_str()));
                                break;
                            }

                            rc = VINF_SUCCESS;
                        }

                        if (RT_FAILURE(rc))
                            break;
                    }
                    break;

                case RTFS_TYPE_FILE:
                    LogFlowFunc(("File '%s': %s -> %s\n", pEntry->strPath.c_str(), strSrcAbs.c_str(), strDstAbs.c_str()));
                    if (!pList->mSourceSpec.fDryRun)
                        rc = fileCopyFromGuest(strSrcAbs, strDstAbs, FileCopyFlag_None);
                    break;

                default:
                    LogFlowFunc(("Warning: Type %d for '%s' is not supported\n",
                                 pEntry->fMode & RTFS_TYPE_MASK, strSrcAbs.c_str()));
                    break;
            }

            if (RT_FAILURE(rc))
                break;

            mProgress->SetNextOperation(Bstr(strSrcAbs).raw(), 1);

            ++itEntry;
        }

        if (RT_FAILURE(rc))
            break;

        ++itList;
    }

    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

GuestSessionTaskCopyTo::GuestSessionTaskCopyTo(GuestSession *pSession, GuestSessionFsSourceSet vecSrc, const Utf8Str &strDest)
                                               : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyTo";

    mSources = vecSrc;
    mDest    = strDest;
}

GuestSessionTaskCopyTo::~GuestSessionTaskCopyTo(void)
{
}

HRESULT GuestSessionTaskCopyTo::Init(const Utf8Str &strTaskDesc)
{
    LogFlowFuncEnter();

    setTaskDesc(strTaskDesc);

    /* Create the progress object. */
    ComObjPtr<Progress> pProgress;
    HRESULT hr = pProgress.createObject();
    if (FAILED(hr))
        return hr;

    mProgress = pProgress;

    int rc = VINF_SUCCESS;

    ULONG cOperations = 0;
    Utf8Str strErrorInfo;

    /**
     * Note: We need to build up the file/directory here instead of GuestSessionTaskCopyTo::Run
     *       because the caller expects a ready-for-operation progress object on return.
     *       The progress object will have a variable operation count, based on the elements to
     *       be processed.
     */

    GuestSessionFsSourceSet::iterator itSrc = mSources.begin();
    while (itSrc != mSources.end())
    {
        Utf8Str strSrc = itSrc->strSource;
        Utf8Str strDst = mDest;

        if (itSrc->enmType == FsObjType_Directory)
        {
            /* If the source does not end with a slash, copy over the entire directory
             * (and not just its contents). */
            if (   !strSrc.endsWith("/")
                && !strSrc.endsWith("\\"))
            {
                if (   !strDst.endsWith("/")
                    && !strDst.endsWith("\\"))
                    strDst += "/";

                strDst += Utf8StrFmt("%s", RTPathFilenameEx(strSrc.c_str(), mfPathStyle));
            }
        }

        LogFlowFunc(("strSrc=%s, strDst=%s\n", strSrc.c_str(), strDst.c_str()));

        RTFSOBJINFO srcFsObjInfo;
        rc = RTPathQueryInfo(strSrc.c_str(), &srcFsObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
        {
            strErrorInfo = Utf8StrFmt(GuestSession::tr("No such source file/directory: %s"), strSrc.c_str());
            break;
        }

        if (RTFS_IS_DIRECTORY(srcFsObjInfo.Attr.fMode))
        {
            if (itSrc->enmType != FsObjType_Directory)
            {
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Source is not a file: %s"), strSrc.c_str());
                rc = VERR_NOT_A_FILE;
                break;
            }
        }
        else
        {
            if (itSrc->enmType == FsObjType_Directory)
            {
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Source is not a directory: %s"), strSrc.c_str());
                rc = VERR_NOT_A_DIRECTORY;
                break;
            }
        }

        FsList *pFsList = NULL;
        try
        {
            pFsList = new FsList(*this);
            rc = pFsList->Init(strSrc, strDst, *itSrc);
            if (RT_SUCCESS(rc))
            {
                if (itSrc->enmType == FsObjType_Directory)
                {
                    rc = pFsList->AddDirFromHost(strSrc);
                }
                else
                    rc = pFsList->AddEntryFromHost(RTPathFilename(strSrc.c_str()), &srcFsObjInfo);
            }

            if (RT_FAILURE(rc))
            {
                delete pFsList;
                strErrorInfo = Utf8StrFmt(GuestSession::tr("Error adding source '%s' to list: %Rrc"), strSrc.c_str(), rc);
                break;
            }

            mVecLists.push_back(pFsList);
        }
        catch (...)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        AssertPtr(pFsList);
        cOperations += (ULONG)pFsList->mVecEntries.size();

        itSrc++;
    }

    if (cOperations) /* Use the first element as description (if available). */
    {
        Assert(mVecLists.size());
        Assert(mVecLists[0]->mVecEntries.size());

        Utf8Str strFirstOp = mDest + mVecLists[0]->mVecEntries[0]->strPath;

        hr = pProgress->init(static_cast<IGuestSession*>(mSession), Bstr(mDesc).raw(),
                             TRUE /* aCancelable */, cOperations + 1 /* Number of operations */,
                             Bstr(strFirstOp).raw());
    }
    else /* If no operations have been defined, go with an "empty" progress object when will be used for error handling. */
        hr = pProgress->init(static_cast<IGuestSession*>(mSession), Bstr(mDesc).raw(),
                             TRUE /* aCancelable */, 1 /* cOperations */, Bstr(mDesc).raw());

    if (RT_FAILURE(rc))
    {
        Assert(strErrorInfo.isNotEmpty());
        setProgressErrorMsg(VBOX_E_IPRT_ERROR, strErrorInfo);
    }

    LogFlowFunc(("Returning %Rhrc (%Rrc)\n", hr, rc));
    return hr;
}

int GuestSessionTaskCopyTo::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = VINF_SUCCESS;

    FsLists::const_iterator itList = mVecLists.begin();
    while (itList != mVecLists.end())
    {
        FsList *pList = *itList;
        AssertPtr(pList);

        const bool     fFollowSymlinks   = pList->mSourceSpec.fFollowSymlinks;
        const bool     fCopyIntoExisting = pList->mSourceSpec.Type.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting;
        const uint32_t fDirMode          = 0700; /** @todo Play safe by default; implement ACLs. */

        LogFlowFunc(("List: srcRootAbs=%s, dstRootAbs=%s\n", pList->mSrcRootAbs.c_str(), pList->mDstRootAbs.c_str()));

        /* Create the root directory. */
        if (pList->mSourceSpec.enmType == FsObjType_Directory)
        {
            rc = directoryCreate(pList->mDstRootAbs.c_str(), DirectoryCreateFlag_None, fDirMode, fFollowSymlinks);
            if (   rc == VWRN_ALREADY_EXISTS
                && !fCopyIntoExisting)
            {
                break;
            }
        }

        FsEntries::const_iterator itEntry = pList->mVecEntries.begin();
        while (itEntry != pList->mVecEntries.end())
        {
            FsEntry *pEntry = *itEntry;
            AssertPtr(pEntry);

            Utf8Str strSrcAbs = pList->mSrcRootAbs;
            Utf8Str strDstAbs = pList->mDstRootAbs;
            if (pList->mSourceSpec.enmType == FsObjType_Directory)
            {
                strSrcAbs += pEntry->strPath;
                strDstAbs += pEntry->strPath;
            }

            switch (pEntry->fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                    LogFlowFunc(("Directory '%s': %s -> %s\n", pEntry->strPath.c_str(), strSrcAbs.c_str(), strDstAbs.c_str()));
                    if (!pList->mSourceSpec.fDryRun)
                    {
                        rc = directoryCreate(strDstAbs.c_str(), DirectoryCreateFlag_None, fDirMode, fFollowSymlinks);
                        if (   rc == VERR_ALREADY_EXISTS
                            && !fCopyIntoExisting)
                        {
                            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" already exists"),
                                                           strDstAbs.c_str()));
                            break;
                        }
                    }
                    break;

                case RTFS_TYPE_FILE:
                    LogFlowFunc(("File '%s': %s -> %s\n", pEntry->strPath.c_str(), strSrcAbs.c_str(), strDstAbs.c_str()));
                    if (!pList->mSourceSpec.fDryRun)
                        rc = fileCopyToGuest(strSrcAbs, strDstAbs, FileCopyFlag_None);
                    break;

                default:
                    LogFlowFunc(("Warning: Type %d for '%s' is not supported\n",
                                 pEntry->fMode & RTFS_TYPE_MASK, strSrcAbs.c_str()));
                    break;
            }

            if (RT_FAILURE(rc))
                break;

            mProgress->SetNextOperation(Bstr(strSrcAbs).raw(), 1);

            ++itEntry;
        }

        if (RT_FAILURE(rc))
            break;

        ++itList;
    }

    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

GuestSessionTaskUpdateAdditions::GuestSessionTaskUpdateAdditions(GuestSession *pSession,
                                                                 const Utf8Str &strSource,
                                                                 const ProcessArguments &aArguments,
                                                                 uint32_t fFlags)
                                                                 : GuestSessionTask(pSession)
{
    m_strTaskName = "gctlUpGA";

    mSource    = strSource;
    mArguments = aArguments;
    mFlags     = fFlags;
}

GuestSessionTaskUpdateAdditions::~GuestSessionTaskUpdateAdditions(void)
{

}

int GuestSessionTaskUpdateAdditions::addProcessArguments(ProcessArguments &aArgumentsDest, const ProcessArguments &aArgumentsSource)
{
    int rc = VINF_SUCCESS;

    try
    {
        /* Filter out arguments which already are in the destination to
         * not end up having them specified twice. Not the fastest method on the
         * planet but does the job. */
        ProcessArguments::const_iterator itSource = aArgumentsSource.begin();
        while (itSource != aArgumentsSource.end())
        {
            bool fFound = false;
            ProcessArguments::iterator itDest = aArgumentsDest.begin();
            while (itDest != aArgumentsDest.end())
            {
                if ((*itDest).equalsIgnoreCase((*itSource)))
                {
                    fFound = true;
                    break;
                }
                ++itDest;
            }

            if (!fFound)
                aArgumentsDest.push_back((*itSource));

            ++itSource;
        }
    }
    catch(std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }

    return rc;
}

int GuestSessionTaskUpdateAdditions::copyFileToGuest(GuestSession *pSession, RTVFS hVfsIso,
                                                Utf8Str const &strFileSource, const Utf8Str &strFileDest,
                                                bool fOptional)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertReturn(hVfsIso != NIL_RTVFS, VERR_INVALID_POINTER);

    RTVFSFILE hVfsFile = NIL_RTVFSFILE;
    int rc = RTVfsFileOpen(hVfsIso, strFileSource.c_str(), RTFILE_O_OPEN | RTFILE_O_READ, &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        uint64_t cbSrcSize = 0;
        rc = RTVfsFileGetSize(hVfsFile, &cbSrcSize);
        if (RT_SUCCESS(rc))
        {
            LogRel(("Copying Guest Additions installer file \"%s\" to \"%s\" on guest ...\n",
                    strFileSource.c_str(), strFileDest.c_str()));

            GuestFileOpenInfo dstOpenInfo;
            RT_ZERO(dstOpenInfo);
            dstOpenInfo.mFileName    = strFileDest;
            dstOpenInfo.mOpenAction  = FileOpenAction_CreateOrReplace;
            dstOpenInfo.mAccessMode  = FileAccessMode_WriteOnly;
            dstOpenInfo.mSharingMode = FileSharingMode_All; /** @todo Use _Read when implemented. */

            ComObjPtr<GuestFile> dstFile; int rcGuest;
            rc = mSession->i_fileOpen(dstOpenInfo, dstFile, &rcGuest);
            if (RT_FAILURE(rc))
            {
                switch (rc)
                {
                    case VERR_GSTCTL_GUEST_ERROR:
                        setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestFile::i_guestErrorToString(rcGuest));
                        break;

                    default:
                        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                            Utf8StrFmt(GuestSession::tr("Destination file \"%s\" could not be opened: %Rrc"),
                                                       strFileDest.c_str(), rc));
                        break;
                }
            }
            else
            {
                rc = fileCopyToGuestInner(hVfsFile, dstFile, FileCopyFlag_None, 0 /*cbOffset*/, cbSrcSize);

                int rc2 = dstFile->i_closeFile(&rcGuest);
                AssertRC(rc2);
            }
        }

        RTVfsFileRelease(hVfsFile);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        if (fOptional)
            return VINF_SUCCESS;

        return rc;
    }

    return rc;
}

int GuestSessionTaskUpdateAdditions::runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    LogRel(("Running %s ...\n", procInfo.mName.c_str()));

    GuestProcessTool procTool;
    int rcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int vrc = procTool.init(pSession, procInfo, false /* Async */, &rcGuest);
    if (RT_SUCCESS(vrc))
    {
        if (RT_SUCCESS(rcGuest))
            vrc = procTool.wait(GUESTPROCESSTOOL_WAIT_FLAG_NONE, &rcGuest);
        if (RT_SUCCESS(vrc))
            vrc = procTool.terminatedOk();
    }

    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VWRN_GSTCTL_PROCESS_EXIT_CODE:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Running update file \"%s\" on guest failed: %Rrc"),
                                               procInfo.mExecutable.c_str(), procTool.getRc()));
                break;

            case VERR_GSTCTL_GUEST_ERROR:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestProcess::i_guestErrorToString(rcGuest));
                break;

            case VERR_INVALID_STATE: /** @todo Special guest control rc needed! */
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Update file \"%s\" reported invalid running state"),
                                               procInfo.mExecutable.c_str()));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Error while running update file \"%s\" on guest: %Rrc"),
                                               procInfo.mExecutable.c_str(), vrc));
                break;
        }
    }

    return vrc;
}

int GuestSessionTaskUpdateAdditions::Run(void)
{
    LogFlowThisFuncEnter();

    ComObjPtr<GuestSession> pSession = mSession;
    Assert(!pSession.isNull());

    AutoCaller autoCaller(pSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = setProgress(10);
    if (RT_FAILURE(rc))
        return rc;

    HRESULT hr = S_OK;

    LogRel(("Automatic update of Guest Additions started, using \"%s\"\n", mSource.c_str()));

    ComObjPtr<Guest> pGuest(mSession->i_getParent());
#if 0
    /*
     * Wait for the guest being ready within 30 seconds.
     */
    AdditionsRunLevelType_T addsRunLevel;
    uint64_t tsStart = RTTimeSystemMilliTS();
    while (   SUCCEEDED(hr = pGuest->COMGETTER(AdditionsRunLevel)(&addsRunLevel))
           && (    addsRunLevel != AdditionsRunLevelType_Userland
                && addsRunLevel != AdditionsRunLevelType_Desktop))
    {
        if ((RTTimeSystemMilliTS() - tsStart) > 30 * 1000)
        {
            rc = VERR_TIMEOUT;
            break;
        }

        RTThreadSleep(100); /* Wait a bit. */
    }

    if (FAILED(hr)) rc = VERR_TIMEOUT;
    if (rc == VERR_TIMEOUT)
        hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                 Utf8StrFmt(GuestSession::tr("Guest Additions were not ready within time, giving up")));
#else
    /*
     * For use with the GUI we don't want to wait, just return so that the manual .ISO mounting
     * can continue.
     */
    AdditionsRunLevelType_T addsRunLevel;
    if (   FAILED(hr = pGuest->COMGETTER(AdditionsRunLevel)(&addsRunLevel))
        || (   addsRunLevel != AdditionsRunLevelType_Userland
            && addsRunLevel != AdditionsRunLevelType_Desktop))
    {
        if (addsRunLevel == AdditionsRunLevelType_System)
            hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                     Utf8StrFmt(GuestSession::tr("Guest Additions are installed but not fully loaded yet, aborting automatic update")));
        else
            hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                     Utf8StrFmt(GuestSession::tr("Guest Additions not installed or ready, aborting automatic update")));
        rc = VERR_NOT_SUPPORTED;
    }
#endif

    if (RT_SUCCESS(rc))
    {
        /*
         * Determine if we are able to update automatically. This only works
         * if there are recent Guest Additions installed already.
         */
        Utf8Str strAddsVer;
        rc = getGuestProperty(pGuest, "/VirtualBox/GuestAdd/Version", strAddsVer);
        if (   RT_SUCCESS(rc)
            && RTStrVersionCompare(strAddsVer.c_str(), "4.1") < 0)
        {
            hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                     Utf8StrFmt(GuestSession::tr("Guest has too old Guest Additions (%s) installed for automatic updating, please update manually"),
                                                strAddsVer.c_str()));
            rc = VERR_NOT_SUPPORTED;
        }
    }

    Utf8Str strOSVer;
    eOSType osType = eOSType_Unknown;
    if (RT_SUCCESS(rc))
    {
        /*
         * Determine guest OS type and the required installer image.
         */
        Utf8Str strOSType;
        rc = getGuestProperty(pGuest, "/VirtualBox/GuestInfo/OS/Product", strOSType);
        if (RT_SUCCESS(rc))
        {
            if (   strOSType.contains("Microsoft", Utf8Str::CaseInsensitive)
                || strOSType.contains("Windows", Utf8Str::CaseInsensitive))
            {
                osType = eOSType_Windows;

                /*
                 * Determine guest OS version.
                 */
                rc = getGuestProperty(pGuest, "/VirtualBox/GuestInfo/OS/Release", strOSVer);
                if (RT_FAILURE(rc))
                {
                    hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                             Utf8StrFmt(GuestSession::tr("Unable to detected guest OS version, please update manually")));
                    rc = VERR_NOT_SUPPORTED;
                }

                /* Because Windows 2000 + XP and is bitching with WHQL popups even if we have signed drivers we
                 * can't do automated updates here. */
                /* Windows XP 64-bit (5.2) is a Windows 2003 Server actually, so skip this here. */
                if (   RT_SUCCESS(rc)
                    && RTStrVersionCompare(strOSVer.c_str(), "5.0") >= 0)
                {
                    if (   strOSVer.startsWith("5.0") /* Exclude the build number. */
                        || strOSVer.startsWith("5.1") /* Exclude the build number. */)
                    {
                        /* If we don't have AdditionsUpdateFlag_WaitForUpdateStartOnly set we can't continue
                         * because the Windows Guest Additions installer will fail because of WHQL popups. If the
                         * flag is set this update routine ends successfully as soon as the installer was started
                         * (and the user has to deal with it in the guest). */
                        if (!(mFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly))
                        {
                            hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                                     Utf8StrFmt(GuestSession::tr("Windows 2000 and XP are not supported for automatic updating due to WHQL interaction, please update manually")));
                            rc = VERR_NOT_SUPPORTED;
                        }
                    }
                }
                else
                {
                    hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                             Utf8StrFmt(GuestSession::tr("%s (%s) not supported for automatic updating, please update manually"),
                                                        strOSType.c_str(), strOSVer.c_str()));
                    rc = VERR_NOT_SUPPORTED;
                }
            }
            else if (strOSType.contains("Solaris", Utf8Str::CaseInsensitive))
            {
                osType = eOSType_Solaris;
            }
            else /* Everything else hopefully means Linux :-). */
                osType = eOSType_Linux;

#if 1 /* Only Windows is supported (and tested) at the moment. */
            if (   RT_SUCCESS(rc)
                && osType != eOSType_Windows)
            {
                hr = setProgressErrorMsg(VBOX_E_NOT_SUPPORTED,
                                         Utf8StrFmt(GuestSession::tr("Detected guest OS (%s) does not support automatic Guest Additions updating, please update manually"),
                                                    strOSType.c_str()));
                rc = VERR_NOT_SUPPORTED;
            }
#endif
        }
    }

    if (RT_SUCCESS(rc))
    {
        /*
         * Try to open the .ISO file to extract all needed files.
         */
        RTVFSFILE hVfsFileIso;
        rc = RTVfsFileOpenNormal(mSource.c_str(), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, &hVfsFileIso);
        if (RT_SUCCESS(rc))
        {
            RTVFS hVfsIso;
            rc = RTFsIso9660VolOpen(hVfsFileIso, 0 /*fFlags*/, &hVfsIso, NULL);
            if (RT_FAILURE(rc))
            {
                hr = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                         Utf8StrFmt(GuestSession::tr("Unable to open Guest Additions .ISO file \"%s\": %Rrc"),
                                         mSource.c_str(), rc));
            }
            else
            {
                /* Set default installation directories. */
                Utf8Str strUpdateDir = "/tmp/";
                if (osType == eOSType_Windows)
                     strUpdateDir = "C:\\Temp\\";

                rc = setProgress(5);

                /* Try looking up the Guest Additions installation directory. */
                if (RT_SUCCESS(rc))
                {
                    /* Try getting the installed Guest Additions version to know whether we
                     * can install our temporary Guest Addition data into the original installation
                     * directory.
                     *
                     * Because versions prior to 4.2 had bugs wrt spaces in paths we have to choose
                     * a different location then.
                     */
                    bool fUseInstallDir = false;

                    Utf8Str strAddsVer;
                    rc = getGuestProperty(pGuest, "/VirtualBox/GuestAdd/Version", strAddsVer);
                    if (   RT_SUCCESS(rc)
                        && RTStrVersionCompare(strAddsVer.c_str(), "4.2r80329") > 0)
                    {
                        fUseInstallDir = true;
                    }

                    if (fUseInstallDir)
                    {
                        if (RT_SUCCESS(rc))
                            rc = getGuestProperty(pGuest, "/VirtualBox/GuestAdd/InstallDir", strUpdateDir);
                        if (RT_SUCCESS(rc))
                        {
                            if (osType == eOSType_Windows)
                            {
                                strUpdateDir.findReplace('/', '\\');
                                strUpdateDir.append("\\Update\\");
                            }
                            else
                                strUpdateDir.append("/update/");
                        }
                    }
                }

                if (RT_SUCCESS(rc))
                    LogRel(("Guest Additions update directory is: %s\n",
                            strUpdateDir.c_str()));

                /* Create the installation directory. */
                int rcGuest = VERR_IPE_UNINITIALIZED_STATUS;
                rc = pSession->i_directoryCreate(strUpdateDir, 755 /* Mode */, DirectoryCreateFlag_Parents, &rcGuest);
                if (RT_FAILURE(rc))
                {
                    switch (rc)
                    {
                        case VERR_GSTCTL_GUEST_ERROR:
                            hr = setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestProcess::i_guestErrorToString(rcGuest));
                            break;

                        default:
                            hr = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                     Utf8StrFmt(GuestSession::tr("Error creating installation directory \"%s\" on the guest: %Rrc"),
                                                                strUpdateDir.c_str(), rc));
                            break;
                    }
                }
                if (RT_SUCCESS(rc))
                    rc = setProgress(10);

                if (RT_SUCCESS(rc))
                {
                    /* Prepare the file(s) we want to copy over to the guest and
                     * (maybe) want to run. */
                    switch (osType)
                    {
                        case eOSType_Windows:
                        {
                            /* Do we need to install our certificates? We do this for W2K and up. */
                            bool fInstallCert = false;

                            /* Only Windows 2000 and up need certificates to be installed. */
                            if (RTStrVersionCompare(strOSVer.c_str(), "5.0") >= 0)
                            {
                                fInstallCert = true;
                                LogRel(("Certificates for auto updating WHQL drivers will be installed\n"));
                            }
                            else
                                LogRel(("Skipping installation of certificates for WHQL drivers\n"));

                            if (fInstallCert)
                            {
                                static struct { const char *pszDst, *pszIso; } const s_aCertFiles[] =
                                {
                                    { "vbox.cer",           "CERT/VBOX.CER" },
                                    { "vbox-sha1.cer",      "CERT/VBOX_SHA1.CER" },
                                    { "vbox-sha256.cer",    "CERT/VBOX_SHA256.CER" },
                                    { "vbox-sha256-r3.cer", "CERT/VBOX_SHA256_R3.CER" },
                                    { "oracle-vbox.cer",    "CERT/ORACLE_VBOX.CER" },
                                };
                                uint32_t fCopyCertUtil = ISOFILE_FLAG_COPY_FROM_ISO;
                                for (uint32_t i = 0; i < RT_ELEMENTS(s_aCertFiles); i++)
                                {
                                    /* Skip if not present on the ISO. */
                                    RTFSOBJINFO ObjInfo;
                                    rc = RTVfsQueryPathInfo(hVfsIso, s_aCertFiles[i].pszIso, &ObjInfo, RTFSOBJATTRADD_NOTHING,
                                                            RTPATH_F_ON_LINK);
                                    if (RT_FAILURE(rc))
                                        continue;

                                    /* Copy the certificate certificate. */
                                    Utf8Str const strDstCert(strUpdateDir + s_aCertFiles[i].pszDst);
                                    mFiles.push_back(ISOFile(s_aCertFiles[i].pszIso,
                                                             strDstCert,
                                                             ISOFILE_FLAG_COPY_FROM_ISO | ISOFILE_FLAG_OPTIONAL));

                                    /* Out certificate installation utility. */
                                    /* First pass: Copy over the file (first time only) + execute it to remove any
                                     *             existing VBox certificates. */
                                    GuestProcessStartupInfo siCertUtilRem;
                                    siCertUtilRem.mName = "VirtualBox Certificate Utility, removing old VirtualBox certificates";
                                    siCertUtilRem.mArguments.push_back(Utf8Str("remove-trusted-publisher"));
                                    siCertUtilRem.mArguments.push_back(Utf8Str("--root")); /* Add root certificate as well. */
                                    siCertUtilRem.mArguments.push_back(strDstCert);
                                    siCertUtilRem.mArguments.push_back(strDstCert);
                                    mFiles.push_back(ISOFile("CERT/VBOXCERTUTIL.EXE",
                                                             strUpdateDir + "VBoxCertUtil.exe",
                                                             fCopyCertUtil | ISOFILE_FLAG_EXECUTE | ISOFILE_FLAG_OPTIONAL,
                                                             siCertUtilRem));
                                    fCopyCertUtil = 0;
                                    /* Second pass: Only execute (but don't copy) again, this time installng the
                                     *              recent certificates just copied over. */
                                    GuestProcessStartupInfo siCertUtilAdd;
                                    siCertUtilAdd.mName = "VirtualBox Certificate Utility, installing VirtualBox certificates";
                                    siCertUtilAdd.mArguments.push_back(Utf8Str("add-trusted-publisher"));
                                    siCertUtilAdd.mArguments.push_back(Utf8Str("--root")); /* Add root certificate as well. */
                                    siCertUtilAdd.mArguments.push_back(strDstCert);
                                    siCertUtilAdd.mArguments.push_back(strDstCert);
                                    mFiles.push_back(ISOFile("CERT/VBOXCERTUTIL.EXE",
                                                             strUpdateDir + "VBoxCertUtil.exe",
                                                             ISOFILE_FLAG_EXECUTE | ISOFILE_FLAG_OPTIONAL,
                                                             siCertUtilAdd));
                                }
                            }
                            /* The installers in different flavors, as we don't know (and can't assume)
                             * the guest's bitness. */
                            mFiles.push_back(ISOFile("VBOXWINDOWSADDITIONS_X86.EXE",
                                                     strUpdateDir + "VBoxWindowsAdditions-x86.exe",
                                                     ISOFILE_FLAG_COPY_FROM_ISO));
                            mFiles.push_back(ISOFile("VBOXWINDOWSADDITIONS_AMD64.EXE",
                                                     strUpdateDir + "VBoxWindowsAdditions-amd64.exe",
                                                     ISOFILE_FLAG_COPY_FROM_ISO));
                            /* The stub loader which decides which flavor to run. */
                            GuestProcessStartupInfo siInstaller;
                            siInstaller.mName = "VirtualBox Windows Guest Additions Installer";
                            /* Set a running timeout of 5 minutes -- the Windows Guest Additions
                             * setup can take quite a while, so be on the safe side. */
                            siInstaller.mTimeoutMS = 5 * 60 * 1000;
                            siInstaller.mArguments.push_back(Utf8Str("/S")); /* We want to install in silent mode. */
                            siInstaller.mArguments.push_back(Utf8Str("/l")); /* ... and logging enabled. */
                            /* Don't quit VBoxService during upgrade because it still is used for this
                             * piece of code we're in right now (that is, here!) ... */
                            siInstaller.mArguments.push_back(Utf8Str("/no_vboxservice_exit"));
                            /* Tell the installer to report its current installation status
                             * using a running VBoxTray instance via balloon messages in the
                             * Windows taskbar. */
                            siInstaller.mArguments.push_back(Utf8Str("/post_installstatus"));
                            /* Add optional installer command line arguments from the API to the
                             * installer's startup info. */
                            rc = addProcessArguments(siInstaller.mArguments, mArguments);
                            AssertRC(rc);
                            /* If the caller does not want to wait for out guest update process to end,
                             * complete the progress object now so that the caller can do other work. */
                            if (mFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly)
                                siInstaller.mFlags |= ProcessCreateFlag_WaitForProcessStartOnly;
                            mFiles.push_back(ISOFile("VBOXWINDOWSADDITIONS.EXE",
                                                     strUpdateDir + "VBoxWindowsAdditions.exe",
                                                     ISOFILE_FLAG_COPY_FROM_ISO | ISOFILE_FLAG_EXECUTE, siInstaller));
                            break;
                        }
                        case eOSType_Linux:
                            /** @todo Add Linux support. */
                            break;
                        case eOSType_Solaris:
                            /** @todo Add Solaris support. */
                            break;
                        default:
                            AssertReleaseMsgFailed(("Unsupported guest type: %d\n", osType));
                            break;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    /* We want to spend 40% total for all copying operations. So roughly
                     * calculate the specific percentage step of each copied file. */
                    uint8_t uOffset = 20; /* Start at 20%. */
                    uint8_t uStep = 40 / (uint8_t)mFiles.size(); Assert(mFiles.size() <= 10);

                    LogRel(("Copying over Guest Additions update files to the guest ...\n"));

                    std::vector<ISOFile>::const_iterator itFiles = mFiles.begin();
                    while (itFiles != mFiles.end())
                    {
                        if (itFiles->fFlags & ISOFILE_FLAG_COPY_FROM_ISO)
                        {
                            bool fOptional = false;
                            if (itFiles->fFlags & ISOFILE_FLAG_OPTIONAL)
                                fOptional = true;
                            rc = copyFileToGuest(pSession, hVfsIso, itFiles->strSource, itFiles->strDest, fOptional);
                            if (RT_FAILURE(rc))
                            {
                                hr = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                         Utf8StrFmt(GuestSession::tr("Error while copying file \"%s\" to \"%s\" on the guest: %Rrc"),
                                                                    itFiles->strSource.c_str(), itFiles->strDest.c_str(), rc));
                                break;
                            }
                        }

                        rc = setProgress(uOffset);
                        if (RT_FAILURE(rc))
                            break;
                        uOffset += uStep;

                        ++itFiles;
                    }
                }

                /* Done copying, close .ISO file. */
                RTVfsRelease(hVfsIso);

                if (RT_SUCCESS(rc))
                {
                    /* We want to spend 35% total for all copying operations. So roughly
                     * calculate the specific percentage step of each copied file. */
                    uint8_t uOffset = 60; /* Start at 60%. */
                    uint8_t uStep = 35 / (uint8_t)mFiles.size(); Assert(mFiles.size() <= 10);

                    LogRel(("Executing Guest Additions update files ...\n"));

                    std::vector<ISOFile>::iterator itFiles = mFiles.begin();
                    while (itFiles != mFiles.end())
                    {
                        if (itFiles->fFlags & ISOFILE_FLAG_EXECUTE)
                        {
                            rc = runFileOnGuest(pSession, itFiles->mProcInfo);
                            if (RT_FAILURE(rc))
                                break;
                        }

                        rc = setProgress(uOffset);
                        if (RT_FAILURE(rc))
                            break;
                        uOffset += uStep;

                        ++itFiles;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    LogRel(("Automatic update of Guest Additions succeeded\n"));
                    rc = setProgressSuccess();
                }
            }

            RTVfsFileRelease(hVfsFileIso);
        }
    }

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_CANCELLED)
        {
            LogRel(("Automatic update of Guest Additions was canceled\n"));

            hr = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                     Utf8StrFmt(GuestSession::tr("Installation was canceled")));
        }
        else
        {
            Utf8Str strError = Utf8StrFmt("No further error information available (%Rrc)", rc);
            if (!mProgress.isNull()) /* Progress object is optional. */
            {
                com::ProgressErrorInfo errorInfo(mProgress);
                if (   errorInfo.isFullAvailable()
                    || errorInfo.isBasicAvailable())
                {
                    strError = errorInfo.getText();
                }
            }

            LogRel(("Automatic update of Guest Additions failed: %s (%Rhrc)\n",
                    strError.c_str(), hr));
        }

        LogRel(("Please install Guest Additions manually\n"));
    }

    /** @todo Clean up copied / left over installation files. */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

