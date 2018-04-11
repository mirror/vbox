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
}

GuestSessionTask::~GuestSessionTask(void)
{
}

HRESULT GuestSessionTask::createAndSetProgressObject()
{
    LogFlowThisFunc(("Task Description = %s, pTask=%p\n", mDesc.c_str(), this));

    ComObjPtr<Progress> pProgress;
    HRESULT hr = S_OK;
    /* Create the progress object. */
    hr = pProgress.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    hr = pProgress->init(static_cast<IGuestSession*>(mSession),
                         Bstr(mDesc).raw(),
                         TRUE /* aCancelable */);
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    mProgress = pProgress;

    LogFlowFuncLeave();

    return hr;
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

    BOOL fCanceled;
    BOOL fCompleted;
    if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
        && !fCanceled
        && SUCCEEDED(mProgress->COMGETTER(Completed(&fCompleted)))
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
 * @return VBox status code. VWRN_ALREADY_EXISTS if directory on the guest already exists.
 * @param  strPath                  Absolute path to directory on the guest (guest style path) to create.
 * @param  enmDirecotryCreateFlags  Directory creation flags.
 * @param  uMode                    Directory mode to use for creation.
 * @param  fFollowSymlinks          Whether to follow symlinks on the guest or not.
 */
int GuestSessionTask::directoryCreate(const com::Utf8Str &strPath,
                                      DirectoryCreateFlag_T enmDirecotryCreateFlags, uint32_t uMode, bool fFollowSymlinks)
{
    LogFlowFunc(("strPath=%s, fFlags=0x%x, uMode=%RU32, fFollowSymlinks=%RTbool\n",
                 strPath.c_str(), enmDirecotryCreateFlags, uMode, fFollowSymlinks));

    GuestFsObjData objData; int rcGuest;

    int rc = mSession->i_directoryQueryInfo(strPath, fFollowSymlinks, objData, &rcGuest);
    if (RT_SUCCESS(rc))
    {
        return VWRN_ALREADY_EXISTS;
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
                        rc = mSession->i_directoryCreate(strPath.c_str(), uMode, enmDirecotryCreateFlags, &rcGuest);
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
                                Utf8StrFmt(GuestSession::tr("Reading %RU32 bytes @ %RU64, failed: %Rrc"), cbChunk, cbWrittenTotal, rc));
            break;
        }

        rc = RTFileWrite(*phDstFile, byBuf, cbRead, NULL /* No partial writes */);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Writing to %RU32 bytes to file failed: %Rrc"), cbRead, rc));
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
    RT_ZERO(srcObjData);

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
            if (   strDest.endsWith("\\")
                || strDest.endsWith("/"))
            {
                /* Build the final file name with destination path (on the host). */
                char szDstPath[RTPATH_MAX];
                RTStrPrintf2(szDstPath, sizeof(szDstPath), "%s", strDest.c_str());

                RTPathAppend(szDstPath, sizeof(szDstPath), RTPathFilename(strSource.c_str()));

                pszDstFile = RTStrDup(szDstPath);
            }
            else
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" already exists"),
                                               strDest.c_str(), rc));
                rc = VERR_ALREADY_EXISTS;
            }
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
 * @param  phSrcFile          Pointer to host file handle (source) to copy from. Must be in opened and ready state already.
 * @param  dstFile            Guest file (destination) to copy to the guest. Must be in opened and ready state already.
 * @param  fFileCopyFlags     File copy flags.
 * @param  offCopy            Offset (in bytes) where to start copying the source file.
 * @param  cbSize             Size (in bytes) to copy from the source file.
 */
int GuestSessionTask::fileCopyToGuestInner(PRTFILE phSrcFile, ComObjPtr<GuestFile> &dstFile, FileCopyFlag_T fFileCopyFlags,
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
        rc = RTFileSeek(*phSrcFile, offCopy, RTFILE_SEEK_END, &offActual);
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
        rc = RTFileRead(*phSrcFile, byBuf, cbChunk, &cbRead);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Reading %RU32 bytes @ %RU64, failed: %Rrc"), cbChunk, cbWrittenTotal, rc));
            break;
        }

        rc = dstFile->i_writeData(uTimeoutMs, byBuf, (uint32_t)cbRead, NULL /* No partial writes */);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Writing to %zu bytes to file failed: %Rrc"), cbRead, rc));
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
    RT_ZERO(dstObjData);

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
                if (   strDest.endsWith("\\")
                    || strDest.endsWith("/"))
                {
                    /* Build the final file name with destination path (on the host). */
                    char szDstPath[RTPATH_MAX];
                    RTStrPrintf2(szDstPath, sizeof(szDstPath), "%s", strDest.c_str());

                    RTPathAppend(szDstPath, sizeof(szDstPath), RTPathFilename(strSource.c_str()));

                    strDestFinal = szDstPath;
                }
                else
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" already exists"),
                                                   strDest.c_str(), rc));
                    rc = VERR_ALREADY_EXISTS;
                }
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

    RTFSOBJINFO srcObjInfo;
    RT_ZERO(srcObjInfo);

    bool fSkip = false; /* Whether to skip handling the file. */

    if (RT_SUCCESS(rc))
    {
        rc = RTPathQueryInfo(strSource.c_str(), &srcObjInfo, RTFSOBJATTRADD_NOTHING);
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
                                           strSource.c_str(), rc));
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
        RTFILE hSrcFile;
        rc = RTFileOpen(&hSrcFile, strSource.c_str(),
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE); /** @todo Use the correct open modes! */
        if (RT_SUCCESS(rc))
        {
            LogFlowThisFunc(("Copying '%s' to '%s' (%RI64 bytes) ...\n",
                             strSource.c_str(), strDestFinal.c_str(), srcObjInfo.cbObject));

            rc = fileCopyToGuestInner(&hSrcFile, dstFile, fFileCopyFlags, 0 /* Offset, unused */, srcObjInfo.cbObject);

            int rc2 = RTFileClose(hSrcFile);
            AssertRC(rc2);
        }
        else
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Opening source file \"%s\" failed: %Rrc"),
                                           strSource.c_str(), rc));
    }

    int rc2 = dstFile->i_closeFile(&rcGuest);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Translates a source path to a destination path (can be both sides,
 * either host or guest). The source root is needed to determine the start
 * of the relative source path which also needs to present in the destination
 * path.
 *
 * @return  IPRT status code.
 * @param   strSourceRoot           Source root path. No trailing directory slash!
 * @param   strSource               Actual source to transform. Must begin with
 *                                  the source root path!
 * @param   strDest                 Destination path.
 * @param   strOut                  where to store the output path.
 */
int GuestSessionTask::pathConstructOnGuest(const Utf8Str &strSourceRoot, const Utf8Str &strSource,
                                           const Utf8Str &strDest, Utf8Str &strOut)
{
    RT_NOREF(strSourceRoot, strSource, strDest, strOut);
    return VINF_SUCCESS;
}

SessionTaskOpen::SessionTaskOpen(GuestSession *pSession,
                                 uint32_t uFlags,
                                 uint32_t uTimeoutMS)
                                 : GuestSessionTask(pSession),
                                   mFlags(uFlags),
                                   mTimeoutMS(uTimeoutMS)
{
    m_strTaskName = "gctlSesOpen";
}

SessionTaskOpen::~SessionTaskOpen(void)
{

}

int SessionTaskOpen::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int vrc = mSession->i_startSession(NULL /*pvrcGuest*/);
    /* Nothing to do here anymore. */

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

SessionTaskCopyDirFrom::SessionTaskCopyDirFrom(GuestSession *pSession, const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                                               DirectoryCopyFlag_T fDirCopyFlags)
                                               : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyDirFrm";

    mSource = strSource;
    mDest   = strDest;
    mFilter = strFilter;

    u.Dir.fCopyFlags = fDirCopyFlags;
}

SessionTaskCopyDirFrom::~SessionTaskCopyDirFrom(void)
{
}

/**
 * Copys a directory (tree) from guest to the host.
 *
 * @return  IPRT status code.
 * @param   strSource               Source directory on the guest to copy to the host.
 * @param   strFilter               DOS-style wildcard filter (?, *).  Optional.
 * @param   strDest                 Destination directory on the host.
 * @param   fRecursive              Whether to recursively copy the directory contents or not.
 * @param   fFollowSymlinks         Whether to follow symlinks or not.
 * @param   strSubDir               Current sub directory to handle. Needs to NULL and only
 *                                  is needed for recursion.
 */
int SessionTaskCopyDirFrom::directoryCopyToHost(const Utf8Str &strSource, const Utf8Str &strFilter,
                                                const Utf8Str &strDest, bool fRecursive, bool fFollowSymlinks,
                                                const Utf8Str &strSubDir /* For recursion. */)
{
    Utf8Str strSrcDir    = strSource;
    Utf8Str strDstDir    = strDest;
    Utf8Str strSrcSubDir = strSubDir;

    /* Validation and sanity. */
    if (   !strSrcDir.endsWith("/")
        && !strSrcDir.endsWith("\\"))
        strSrcDir += "/";

    if (   !strDstDir.endsWith("/")
        && !strDstDir.endsWith("\\"))
        strDstDir+= "/";

    if (    strSrcSubDir.isNotEmpty() /* Optional, for recursion. */
        && !strSrcSubDir.endsWith("/")
        && !strSrcSubDir.endsWith("\\"))
        strSrcSubDir += "/";

    Utf8Str strSrcCur = strSrcDir + strSrcSubDir;

    LogFlowFunc(("Entering '%s'\n", strSrcCur.c_str()));

    int rc;

    if (strSrcSubDir.isNotEmpty())
    {
        const uint32_t fMode = 0700; /** @todo */

        rc = RTDirCreate(Utf8Str(strDstDir + strSrcSubDir).c_str(), fMode, 0);
        if (RT_FAILURE(rc))
            return rc;
    }

    GuestDirectoryOpenInfo dirOpenInfo;
    dirOpenInfo.mFilter = strFilter;
    dirOpenInfo.mPath   = strSrcCur;
    dirOpenInfo.mFlags  = 0; /** @todo Handle flags? */

    ComObjPtr <GuestDirectory> pDir; int rcGuest;
    rc = mSession->i_directoryOpen(dirOpenInfo, pDir, &rcGuest);

    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Opening directory \"%s\" failed: Invalid parameter"), dirOpenInfo.mPath.c_str()));
               break;

            case VERR_GSTCTL_GUEST_ERROR:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR, GuestProcess::i_guestErrorToString(rcGuest));
                break;

            default:
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Opening directory \"%s\" failed: %Rrc"), dirOpenInfo.mPath.c_str(), rc));
                break;
        }

        return rc;
    }

    ComObjPtr<GuestFsObjInfo> fsObjInfo;
    while (RT_SUCCESS(rc = pDir->i_readInternal(fsObjInfo, &rcGuest)))
    {
        FsObjType_T enmObjType = FsObjType_Unknown; /* Shut up MSC. */
        HRESULT hr2 = fsObjInfo->COMGETTER(Type)(&enmObjType);
        AssertComRC(hr2);

        com::Bstr bstrName;
        hr2 = fsObjInfo->COMGETTER(Name)(bstrName.asOutParam());
        AssertComRC(hr2);

        Utf8Str strName(bstrName);

        switch (enmObjType)
        {
            case FsObjType_Directory:
            {
                bool fSkip = false;

                if (   strName.equals(".")
                    || strName.equals(".."))
                {
                    fSkip = true;
                }

                if (   strFilter.isNotEmpty()
                    && !RTStrSimplePatternMatch(strFilter.c_str(), strName.c_str()))
                    fSkip = true;

                if (   fRecursive
                    && !fSkip)
                    rc = directoryCopyToHost(strSrcDir, strFilter, strDstDir, fRecursive, fFollowSymlinks,
                                             strSrcSubDir + strName);
                break;
            }

            case FsObjType_Symlink:
            {
                if (fFollowSymlinks)
                { /* Fall through to next case is intentional. */ }
                else
                    break;
                RT_FALL_THRU();
            }

            case FsObjType_File:
            {
                if (   strFilter.isNotEmpty()
                    && !RTStrSimplePatternMatch(strFilter.c_str(), strName.c_str()))
                {
                    break; /* Filter does not match. */
                }

                if (RT_SUCCESS(rc))
                {
                    Utf8Str strSrcFile = strSrcDir + strSrcSubDir + strName;
                    Utf8Str strDstFile = strDstDir + strSrcSubDir + strName;
                    rc = fileCopyFromGuest(strSrcFile, strDstFile, FileCopyFlag_None);
                }
                break;
            }

            default:
                break;
        }
    }

    if (rc == VERR_NO_MORE_FILES) /* Reading done? */
        rc = VINF_SUCCESS;

    pDir->i_closeInternal(&rcGuest);

    LogFlowFunc(("Leaving '%s', rc=%Rrc\n", strSrcCur.c_str(), rc));
    return rc;
}

int SessionTaskCopyDirFrom::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    const bool fRecursive      = true; /** @todo Make this configurable. */
    const bool fFollowSymlinks = true; /** @todo Make this configurable. */
    const uint32_t fDirMode    = 0700; /* Play safe by default. */

    int rc = VINF_SUCCESS;

    /* Figure out if we need to copy the entire source directory or just its contents. */
    Utf8Str strSrcDir = mSource;
    Utf8Str strDstDir = mDest;
    if (   !strSrcDir.endsWith("/")
        && !strSrcDir.endsWith("\\"))
    {
        if (   !strDstDir.endsWith("/")
            && !strDstDir.endsWith("\\"))
            strDstDir += "/";

        strDstDir += Utf8StrFmt("%s", RTPathFilename(strSrcDir.c_str()));
    }

    /* Create the root target directory on the host.
     * The target directory might already exist on the host (based on u.Dir.fCopyFlags). */
    const bool fExists = RTDirExists(strDstDir.c_str());
    if (   fExists
        && !(u.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting))
    {
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" exists when it must not"), strDstDir.c_str()));
        rc = VERR_ALREADY_EXISTS;
    }
    else if (!fExists)
    {
        rc = RTDirCreate(strDstDir.c_str(), fDirMode, 0);
        if (RT_FAILURE(rc))
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Error creating destination directory \"%s\", rc=%Rrc"),
                                           strDstDir.c_str(), rc));
    }

    if (RT_FAILURE(rc))
        return rc;

    RTDIR hDir;
    rc = RTDirOpen(&hDir, strDstDir.c_str());
    if (RT_FAILURE(rc))
    {
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Error opening destination directory \"%s\", rc=%Rrc"),
                                       strDstDir.c_str(), rc));
        return rc;
    }

    /* At this point the directory on the host was created and (hopefully) is ready
     * to receive further content. */
    rc = directoryCopyToHost(strSrcDir, mFilter, strDstDir, fRecursive, fFollowSymlinks,
                             "" /* strSubDir; for recursion */);
    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    RTDirClose(hDir);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

SessionTaskCopyDirTo::SessionTaskCopyDirTo(GuestSession *pSession,
                                           const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                                           DirectoryCopyFlag_T fDirCopyFlags)
                                           : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyDirTo";

    mSource = strSource;
    mDest   = strDest;
    mFilter = strFilter;

    u.Dir.fCopyFlags = fDirCopyFlags;
}

SessionTaskCopyDirTo::~SessionTaskCopyDirTo(void)
{

}

/**
 * Copys a directory (tree) from host to the guest.
 *
 * @return  IPRT status code.
 * @param   strSource               Source directory on the host to copy to the guest.
 * @param   strFilter               DOS-style wildcard filter (?, *).  Optional.
 * @param   strDest                 Destination directory on the guest.
 * @param   fRecursive              Whether to recursively copy the directory contents or not.
 * @param   fFollowSymlinks         Whether to follow symlinks or not.
 * @param   strSubDir               Current sub directory to handle. Needs to NULL and only
 *                                  is needed for recursion.
 */
int SessionTaskCopyDirTo::directoryCopyToGuest(const Utf8Str &strSource, const Utf8Str &strFilter,
                                               const Utf8Str &strDest, bool fRecursive, bool fFollowSymlinks,
                                               const Utf8Str &strSubDir /* For recursion. */)
{
    Utf8Str strSrcDir    = strSource;
    Utf8Str strDstDir    = strDest;
    Utf8Str strSrcSubDir = strSubDir;

    /* Validation and sanity. */
    if (   !strSrcDir.endsWith("/")
        && !strSrcDir.endsWith("\\"))
        strSrcDir += "/";

    if (   !strDstDir.endsWith("/")
        && !strDstDir.endsWith("\\"))
        strDstDir+= "/";

    if (    strSrcSubDir.isNotEmpty() /* Optional, for recursion. */
        && !strSrcSubDir.endsWith("/")
        && !strSrcSubDir.endsWith("\\"))
        strSrcSubDir += "/";

    Utf8Str strSrcCur = strSrcDir + strSrcSubDir;

    LogFlowFunc(("Entering '%s'\n", strSrcCur.c_str()));

    int rc;

    if (strSrcSubDir.isNotEmpty())
    {
        const uint32_t uMode = 0700; /** @todo */
        rc = directoryCreate(strDstDir + strSrcSubDir, DirectoryCreateFlag_Parents, uMode, fFollowSymlinks);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Open directory without a filter - RTDirOpenFiltered unfortunately
     * cannot handle sub directories so we have to do the filtering ourselves.
     */
    RTDIR hDir;
    rc = RTDirOpen(&hDir, strSrcCur.c_str());
    if (RT_SUCCESS(rc))
    {
        /*
         * Enumerate the directory tree.
         */
        size_t        cbDirEntry = 0;
        PRTDIRENTRYEX pDirEntry  = NULL;
        while (RT_SUCCESS(rc))
        {
            rc = RTDirReadExA(hDir, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_NO_MORE_FILES)
                    rc = VINF_SUCCESS;
                break;
            }

#ifdef LOG_ENABLED
            Utf8Str strDbgCurEntry = strSrcCur + Utf8Str(pDirEntry->szName);
            LogFlowFunc(("Handling '%s' (fMode=0x%x)\n", strDbgCurEntry.c_str(), pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK));
#endif
            switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                {
                    /* Skip "." and ".." entries. */
                    if (RTDirEntryExIsStdDotLink(pDirEntry))
                        break;

                    bool fSkip = false;

                    if (   strFilter.isNotEmpty()
                        && !RTStrSimplePatternMatch(strFilter.c_str(), pDirEntry->szName))
                        fSkip = true;

                    if (   fRecursive
                        && !fSkip)
                        rc = directoryCopyToGuest(strSrcDir, strFilter, strDstDir, fRecursive, fFollowSymlinks,
                                                  strSrcSubDir + Utf8Str(pDirEntry->szName));
                    break;
                }

                case RTFS_TYPE_SYMLINK:
                    if (fFollowSymlinks)
                    { /* Fall through to next case is intentional. */ }
                    else
                        break;
                    RT_FALL_THRU();

                case RTFS_TYPE_FILE:
                {
                    if (   strFilter.isNotEmpty()
                        && !RTStrSimplePatternMatch(strFilter.c_str(), pDirEntry->szName))
                    {
                        break; /* Filter does not match. */
                    }

                    if (RT_SUCCESS(rc))
                    {
                        Utf8Str strSrcFile = strSrcDir + strSrcSubDir + Utf8Str(pDirEntry->szName);
                        Utf8Str strDstFile = strDstDir + strSrcSubDir + Utf8Str(pDirEntry->szName);
                        rc = fileCopyToGuest(strSrcFile, strDstFile, FileCopyFlag_None);
                    }
                    break;
                }

                default:
                    break;
            }
            if (RT_FAILURE(rc))
                break;
        }

        RTDirReadExAFree(&pDirEntry, &cbDirEntry);
        RTDirClose(hDir);
    }

    LogFlowFunc(("Leaving '%s', rc=%Rrc\n", strSrcCur.c_str(), rc));
    return rc;
}

int SessionTaskCopyDirTo::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    const bool fRecursive      = true; /** @todo Make this configurable. */
    const bool fFollowSymlinks = true; /** @todo Make this configurable. */
    const uint32_t uDirMode    = 0700; /* Play safe by default. */

    /* Figure out if we need to copy the entire source directory or just its contents. */
    Utf8Str strSrcDir = mSource;
    Utf8Str strDstDir = mDest;
    if (   !strSrcDir.endsWith("/")
        && !strSrcDir.endsWith("\\"))
    {
        if (   !strDstDir.endsWith("/")
            && !strDstDir.endsWith("\\"))
            strDstDir += "/";

        strDstDir += Utf8StrFmt("%s", RTPathFilename(strSrcDir.c_str()));
    }

    /* Create the root target directory on the guest.
     * The target directory might already exist on the guest (based on u.Dir.fCopyFlags). */
    int rc = directoryCreate(strDstDir, DirectoryCreateFlag_None, uDirMode, fFollowSymlinks);
    if (   rc == VWRN_ALREADY_EXISTS
        && !(u.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting))
    {
        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                            Utf8StrFmt(GuestSession::tr("Destination directory \"%s\" exists when it must not"), strDstDir.c_str()));
    }

    if (RT_FAILURE(rc))
        return rc;

    /* At this point the directory on the guest was created and (hopefully) is ready
     * to receive further content. */
    rc = directoryCopyToGuest(strSrcDir, mFilter, strDstDir, fRecursive, fFollowSymlinks,
                              "" /* strSubDir; for recursion */);
    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

SessionTaskCopyFileTo::SessionTaskCopyFileTo(GuestSession *pSession,
                                             const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags)
                                             : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyFileTo";

    mSource = strSource;
    mDest   = strDest;

    u.File.fCopyFlags = fFileCopyFlags;
}

SessionTaskCopyFileTo::~SessionTaskCopyFileTo(void)
{

}

int SessionTaskCopyFileTo::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = fileCopyToGuest(mSource, mDest, u.File.fCopyFlags);
    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

SessionTaskCopyFileFrom::SessionTaskCopyFileFrom(GuestSession *pSession,
                                                 const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags)
                                                 : GuestSessionCopyTask(pSession)
{
    m_strTaskName = "gctlCpyFileFrm";

    mSource = strSource;
    mDest   = strDest;

    u.File.fCopyFlags = fFileCopyFlags;
}

SessionTaskCopyFileFrom::~SessionTaskCopyFileFrom(void)
{

}

int SessionTaskCopyFileFrom::Run(void)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(mSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = fileCopyFromGuest(mSource, mDest, u.File.fCopyFlags);
    if (RT_SUCCESS(rc))
        rc = setProgressSuccess();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

SessionTaskUpdateAdditions::SessionTaskUpdateAdditions(GuestSession *pSession,
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

SessionTaskUpdateAdditions::~SessionTaskUpdateAdditions(void)
{

}

int SessionTaskUpdateAdditions::addProcessArguments(ProcessArguments &aArgumentsDest, const ProcessArguments &aArgumentsSource)
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

int SessionTaskUpdateAdditions::copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO,
                                                Utf8Str const &strFileSource, const Utf8Str &strFileDest,
                                                bool fOptional)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pISO, VERR_INVALID_POINTER);

    uint32_t cbSrcOffset;
    size_t cbSrcSize;

    int rc = RTIsoFsGetFileInfo(pISO, strFileSource.c_str(), &cbSrcOffset, &cbSrcSize);
    if (RT_FAILURE(rc))
    {
        if (fOptional)
            return VINF_SUCCESS;

        return rc;
    }

    Assert(cbSrcOffset);
    Assert(cbSrcSize);
    rc = RTFileSeek(pISO->file, cbSrcOffset, RTFILE_SEEK_BEGIN, NULL);
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
            rc = fileCopyToGuestInner(&pISO->file, dstFile, FileCopyFlag_None, cbSrcOffset, cbSrcSize);

            int rc2 = dstFile->i_closeFile(&rcGuest);
            AssertRC(rc2);
        }

        if (RT_FAILURE(rc))
            return rc;
    }

    return rc;
}

int SessionTaskUpdateAdditions::runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo)
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

int SessionTaskUpdateAdditions::Run(void)
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

    RTISOFSFILE iso;
    if (RT_SUCCESS(rc))
    {
        /*
         * Try to open the .ISO file to extract all needed files.
         */
        rc = RTIsoFsOpen(&iso, mSource.c_str());
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
                                uint32_t offIgn;
                                size_t   cbIgn;
                                rc = RTIsoFsGetFileInfo(&iso, s_aCertFiles[i].pszIso, &offIgn, &cbIgn);
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
                        rc = copyFileToGuest(pSession, &iso, itFiles->strSource, itFiles->strDest, fOptional);
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
            RTIsoFsClose(&iso);

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

