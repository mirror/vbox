
/* $Id$ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include "GuestImpl.h"
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ProgressImpl.h"

#include <memory> /* For auto_ptr. */

#include <iprt/env.h>
#include <iprt/file.h> /* For CopyTo/From. */

#include <VBox/com/array.h>
#include <VBox/version.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestSession)

HRESULT GuestSession::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestSession::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// session task classes
/////////////////////////////////////////////////////////////////////////////

GuestSessionTask::GuestSessionTask(GuestSession *pSession)
{
    mSession = pSession;
}

GuestSessionTask::~GuestSessionTask(void)
{
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
        && !fCompleted)
        return VINF_SUCCESS;
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
        HRESULT hr = mProgress->notifyComplete(S_OK);
        if (FAILED(hr))
            return VERR_COM_UNEXPECTED; /** @todo Find a better rc. */
    }

    return VINF_SUCCESS;
}

int GuestSessionTask::setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg)
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
        HRESULT hr2 = mProgress->notifyComplete(hr,
                                               COM_IIDOF(IGuestSession),
                                               GuestSession::getStaticComponentName(),
                                               strMsg.c_str());
        if (FAILED(hr2))
            return VERR_COM_UNEXPECTED;
    }
    return VINF_SUCCESS;
}

SessionTaskCopyTo::SessionTaskCopyTo(GuestSession *pSession,
                                     const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags)
                                     : GuestSessionTask(pSession)
{
    mSource = strSource;
    mDest   = strDest;
    mCopyFileFlags = uFlags;
}

SessionTaskCopyTo::~SessionTaskCopyTo(void)
{

}

int SessionTaskCopyTo::Run(void)
{
    LogFlowThisFuncEnter();

    ComObjPtr<GuestSession> pSession = mSession;
    Assert(!pSession.isNull());

    AutoCaller autoCaller(pSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (mCopyFileFlags)
        return setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                   Utf8StrFmt(GuestSession::tr("Copy flags (%#x) not implemented yet"),
                                              mCopyFileFlags));
    int rc;

    /** @todo Make use of exceptions (+ finally block) here! */

    try
    {
        /* Does our source file exist? */
        if (!RTFileExists(mSource.c_str()))
        {
            rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                     Utf8StrFmt(GuestSession::tr("Source file \"%s\" does not exist or is not a file"),
                                                mSource.c_str()));
        }
        else
        {
            RTFILE fileSource;
            rc = RTFileOpen(&fileSource, mSource.c_str(),
                            RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
            if (RT_FAILURE(rc))
            {
                rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                         Utf8StrFmt(GuestSession::tr("Could not open source file \"%s\" for reading (%Rrc)"),
                                                    mSource.c_str(), rc));
            }
            else
            {
                uint64_t cbFileSize;
                rc = RTFileGetSize(fileSource, &cbFileSize);
                if (RT_FAILURE(rc))
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Could not query file size of \"%s\": %Rrc"),
                                                   mSource.c_str(), rc));
                }
                else
                {
                    GuestProcessStartupInfo procInfo;
                    procInfo.mName    = Utf8StrFmt(GuestSession::tr("Copying file \"%s\" to the guest to \"%s\" (%RU64 bytes)"),
                                                   mSource.c_str(), mDest.c_str(), cbFileSize);
                    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_CAT);
                    procInfo.mFlags   = ProcessCreateFlag_Hidden;

                    /* Set arguments.*/
                    procInfo.mArguments.push_back(Utf8StrFmt("--output=%s", mDest.c_str())); /** @todo Do we need path conversion? */

                    /* Startup process. */
                    ComObjPtr<GuestProcess> pProcess;
                    rc = pSession->processCreateExInteral(procInfo, pProcess);
                    if (RT_SUCCESS(rc))
                        rc = pProcess->startProcess();
                    if (RT_FAILURE(rc))
                    {
                        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                            Utf8StrFmt(GuestSession::tr("Unable to start guest process: %Rrc"), rc));
                    }
                    else
                    {
                        GuestProcessWaitResult waitRes;
                        BYTE byBuf[_64K];

                        BOOL fCanceled = FALSE;
                        uint64_t cbWrittenTotal = 0;
                        uint64_t cbToRead = cbFileSize;

                        for (;;)
                        {
                            rc = pProcess->waitFor(ProcessWaitForFlag_StdIn,
                                                   30 * 1000 /* Timeout */, waitRes);
                            if (   RT_FAILURE(rc)
                                || waitRes.mResult == ProcessWaitResult_Terminate
                                || waitRes.mResult == ProcessWaitResult_Error
                                || waitRes.mResult == ProcessWaitResult_Timeout)
                            {
                                break;
                            }

                            size_t cbRead = 0;
                            if (cbFileSize) /* If we have nothing to write, take a shortcut. */
                            {
                                /** @todo Not very efficient, but works for now. */
                                rc = RTFileSeek(fileSource, cbWrittenTotal,
                                                RTFILE_SEEK_BEGIN, NULL /* poffActual */);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = RTFileRead(fileSource, (uint8_t*)byBuf,
                                                    RT_MIN(cbToRead, sizeof(byBuf)), &cbRead);
                                    /*
                                     * Some other error occured? There might be a chance that RTFileRead
                                     * could not resolve/map the native error code to an IPRT code, so just
                                     * print a generic error.
                                     */
                                    if (RT_FAILURE(rc))
                                    {
                                        setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                            Utf8StrFmt(GuestSession::tr("Could not read from file \"%s\" (%Rrc)"),
                                                                       mSource.c_str(), rc));
                                        break;
                                    }
                                }
                                else
                                {
                                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                        Utf8StrFmt(GuestSession::tr("Seeking file \"%s\" offset %RU64 failed: %Rrc"),
                                                                   mSource.c_str(), cbWrittenTotal, rc));
                                    break;
                                }
                            }

                            uint32_t fFlags = ProcessInputFlag_None;

                            /* Did we reach the end of the content we want to transfer (last chunk)? */
                            if (   (cbRead < sizeof(byBuf))
                                /* Did we reach the last block which is exactly _64K? */
                                || (cbToRead - cbRead == 0)
                                /* ... or does the user want to cancel? */
                                || (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
                                    && fCanceled)
                               )
                            {
                                fFlags |= ProcessInputFlag_EndOfFile;
                            }

                            uint32_t cbWritten;
                            Assert(sizeof(byBuf) >= cbRead);
                            rc = pProcess->writeData(0 /* StdIn */, fFlags,
                                                     byBuf, cbRead,
                                                     30 * 1000 /* Timeout */, &cbWritten);
                            if (RT_FAILURE(rc))
                            {
                                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                    Utf8StrFmt(GuestSession::tr("Writing to file \"%s\" (offset %RU64) failed: %Rrc"),
                                                               mSource.c_str(), cbWrittenTotal, rc));
                                break;
                            }
#ifdef DEBUG
                            LogFlowThisFunc(("cbWritten=%RU32, cbToRead=%RU64, cbWrittenTotal=%RU64, cbFileSize=%RU64\n",
                                             cbWritten, cbToRead - cbWritten, cbWrittenTotal + cbWritten, cbFileSize));
#endif
                            /* Only subtract bytes reported written by the guest. */
                            Assert(cbToRead >= cbWritten);
                            cbToRead -= cbWritten;

                            /* Update total bytes written to the guest. */
                            cbWrittenTotal += cbWritten;
                            Assert(cbWrittenTotal <= cbFileSize);

                            /* Did the user cancel the operation above? */
                            if (fCanceled)
                                break;

                            /* Update the progress.
                             * Watch out for division by zero. */
                            cbFileSize > 0
                                ? rc = setProgress((ULONG)(cbWrittenTotal * 100 / cbFileSize))
                                : rc = setProgress(100);
                            if (RT_FAILURE(rc))
                                break;

                            /* End of file reached? */
                            if (!cbToRead)
                                break;
                        } /* for */

                        if (   !fCanceled
                            || RT_SUCCESS(rc))
                        {
                            /*
                             * Even if we succeeded until here make sure to check whether we really transfered
                             * everything.
                             */
                            if (   cbFileSize > 0
                                && cbWrittenTotal == 0)
                            {
                                /* If nothing was transfered but the file size was > 0 then "vbox_cat" wasn't able to write
                                 * to the destination -> access denied. */
                                rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                         Utf8StrFmt(GuestSession::tr("Access denied when copying file \"%s\" to \"%s\""),
                                                                    mSource.c_str(), mDest.c_str()));
                            }
                            else if (cbWrittenTotal < cbFileSize)
                            {
                                /* If we did not copy all let the user know. */
                                rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                         Utf8StrFmt(GuestSession::tr("Copying file \"%s\" failed (%RU64/%RU64 bytes transfered)"),
                                                                    mSource.c_str(), cbWrittenTotal, cbFileSize));
                            }
                            else
                            {
                                rc = pProcess->waitFor(ProcessWaitForFlag_Terminate,
                                                       30 * 1000 /* Timeout */, waitRes);
                                if (   RT_FAILURE(rc)
                                    || waitRes.mResult != ProcessWaitResult_Terminate)
                                {
                                    if (RT_FAILURE(rc))
                                        rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                                 Utf8StrFmt(GuestSession::tr("Waiting on termination for copying file \"%s\" failed: %Rrc"),
                                                                            mSource.c_str(), rc));
                                    else
                                        rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                                 Utf8StrFmt(GuestSession::tr("Waiting on termination for copying file \"%s\" failed with wait result %ld"),
                                                                            mSource.c_str(), waitRes.mResult));
                                }

                                if (RT_SUCCESS(rc))
                                {
                                    ProcessStatus_T procStatus;
                                    LONG exitCode;
                                    if (   (   SUCCEEDED(pProcess->COMGETTER(Status(&procStatus)))
                                            && procStatus != ProcessStatus_TerminatedNormally)
                                        || (   SUCCEEDED(pProcess->COMGETTER(ExitCode(&exitCode)))
                                            && exitCode != 0)
                                       )
                                    {
                                        rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                                 Utf8StrFmt(GuestSession::tr("Copying file \"%s\" failed with status %ld, exit code %ld"),
                                                                            mSource.c_str(), procStatus, exitCode)); /**@todo Add stringify methods! */
                                    }
                                }

                                if (RT_SUCCESS(rc))
                                    rc = setProgressSuccess();
                            }
                        }

                        pProcess->close();
                    } /* processCreateExInteral */
                } /* RTFileGetSize */

                RTFileClose(fileSource);

            } /* RTFileOpen */
        } /* RTFileExists */
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SessionTaskCopyTo::RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress)
{
    LogFlowThisFunc(("strDesc=%s, strSource=%s, strDest=%s, mCopyFileFlags=%x\n",
                     strDesc.c_str(), mSource.c_str(), mDest.c_str(), mCopyFileFlags));

    mDesc = strDesc;
    mProgress = pProgress;

    int rc = RTThreadCreate(NULL, SessionTaskCopyTo::taskThread, this,
                            0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                            "gctlCpyTo");
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
int SessionTaskCopyTo::taskThread(RTTHREAD Thread, void *pvUser)
{
    std::auto_ptr<SessionTaskCopyTo> task(static_cast<SessionTaskCopyTo*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    LogFlowFunc(("pTask=%p\n", task.get()));
    return task->Run();
}

SessionTaskCopyFrom::SessionTaskCopyFrom(GuestSession *pSession,
                                         const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags)
                                         : GuestSessionTask(pSession)
{
    mSource = strSource;
    mDest   = strDest;
    mFlags  = uFlags;
}

SessionTaskCopyFrom::~SessionTaskCopyFrom(void)
{

}

int SessionTaskCopyFrom::Run(void)
{
    LogFlowThisFuncEnter();

    ComObjPtr<GuestSession> pSession = mSession;
    Assert(!pSession.isNull());

    AutoCaller autoCaller(pSession);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc;

    /** @todo Make use of exceptions (+ finally block) here! */

    try
    {
        /*
         * Note: There will be races between querying file size + reading the guest file's
         *       content because we currently *do not* lock down the guest file when doing the
         *       actual operations.
         ** @todo Implement guest file locking!
         */
        GuestFsObjData objData;
        rc = pSession->fileQueryInfoInternal(Utf8Str(mSource), objData);
        if (RT_FAILURE(rc))
        {
            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                Utf8StrFmt(GuestSession::tr("Querying guest file information for \"%s\" failed: %Rrc"),
                                mSource.c_str(), rc));
        }
        else if (objData.mType != FsObjType_File) /* Only single files are supported at the moment. */
        {
            rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                     Utf8StrFmt(GuestSession::tr("Guest file \"%s\" is not a file"), mSource.c_str()));
        }

        if (RT_SUCCESS(rc))
        {
            RTFILE fileDest;
            rc = RTFileOpen(&fileDest, mDest.c_str(),
                            RTFILE_O_WRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE); /** @todo Use the correct open modes! */
            if (RT_FAILURE(rc))
            {
                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                    Utf8StrFmt(GuestSession::tr("Error opening destination file \"%s\": %Rrc"),
                                               mDest.c_str(), rc));
            }
            else
            {
                GuestProcessStartupInfo procInfo;
                procInfo.mName    = Utf8StrFmt(GuestSession::tr("Copying file \"%s\" from guest to the host to \"%s\" (%RI64 bytes)"),
                                                                mSource.c_str(), mDest.c_str(), objData.mObjectSize);
                procInfo.mCommand   = Utf8Str(VBOXSERVICE_TOOL_CAT);
                procInfo.mFlags     = ProcessCreateFlag_Hidden | ProcessCreateFlag_WaitForStdOut;

                /* Set arguments.*/
                procInfo.mArguments.push_back(mSource); /* Which file to output? */

                /* Startup process. */
                ComObjPtr<GuestProcess> pProcess;
                rc = pSession->processCreateExInteral(procInfo, pProcess);
                if (RT_SUCCESS(rc))
                    rc = pProcess->startProcess();
                if (RT_FAILURE(rc))
                {
                    setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                        Utf8StrFmt(GuestSession::tr("Unable to start guest process: %Rrc"), rc));
                }
                else
                {
                    GuestProcessWaitResult waitRes;
                    BYTE byBuf[_64K];

                    BOOL fCanceled = FALSE;
                    uint64_t cbWrittenTotal = 0;
                    uint64_t cbToRead = objData.mObjectSize;

                    for (;;)
                    {
                        rc = pProcess->waitFor(ProcessWaitForFlag_StdOut,
                                               30 * 1000 /* Timeout */, waitRes);
                        if (   RT_FAILURE(rc)
                            || waitRes.mResult != ProcessWaitResult_StdOut)
                        {
                            break;
                        }

                        size_t cbRead;
                        rc = pProcess->readData(OUTPUT_HANDLE_ID_STDOUT, sizeof(byBuf),
                                                30 * 1000 /* Timeout */, byBuf, sizeof(byBuf),
                                                &cbRead);
                        if (RT_FAILURE(rc))
                        {
                            setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                Utf8StrFmt(GuestSession::tr("Reading from file \"%s\" (offset %RU64) failed: %Rrc"),
                                                mSource.c_str(), cbWrittenTotal, rc));
                            break;
                        }

                        if (cbRead)
                        {
                            rc = RTFileWrite(fileDest, byBuf, cbRead, NULL /* No partial writes */);
                            if (RT_FAILURE(rc))
                            {
                                setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                    Utf8StrFmt(GuestSession::tr("Error writing to file \"%s\" (%RU64 bytes left): %Rrc"),
                                                    mDest.c_str(), cbToRead, rc));
                                break;
                            }

                            /* Only subtract bytes reported written by the guest. */
                            Assert(cbToRead >= cbRead);
                            cbToRead -= cbRead;

                            /* Update total bytes written to the guest. */
                            cbWrittenTotal += cbRead;
                            Assert(cbWrittenTotal <= (uint64_t)objData.mObjectSize);

                            /* Did the user cancel the operation above? */
                            if (   SUCCEEDED(mProgress->COMGETTER(Canceled(&fCanceled)))
                                && fCanceled)
                                break;

                            rc = setProgress((ULONG)(cbWrittenTotal / ((uint64_t)objData.mObjectSize / 100.0)));
                            if (RT_FAILURE(rc))
                                break;

                            /* End of file reached? */
                            if (cbToRead == 0)
                                break;
                        }
                    } /* for */

                    if (   !fCanceled
                        || RT_SUCCESS(rc))
                    {
                        /*
                         * Even if we succeeded until here make sure to check whether we really transfered
                         * everything.
                         */
                        if (   objData.mObjectSize > 0
                            && cbWrittenTotal == 0)
                        {
                            /* If nothing was transfered but the file size was > 0 then "vbox_cat" wasn't able to write
                             * to the destination -> access denied. */
                            rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                     Utf8StrFmt(GuestSession::tr("Access denied when copying file \"%s\" to \"%s\""),
                                                     mSource.c_str(), mDest.c_str()));
                        }
                        else if (cbWrittenTotal < (uint64_t)objData.mObjectSize)
                        {
                            /* If we did not copy all let the user know. */
                            rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                     Utf8StrFmt(GuestSession::tr("Copying file \"%s\" failed (%RU64/%RU64 bytes transfered)"),
                                                     mSource.c_str(), cbWrittenTotal, objData.mObjectSize));
                        }
                        else
                        {
                            ProcessStatus_T procStatus;
                            LONG exitCode;
                            if (   (   SUCCEEDED(pProcess->COMGETTER(Status(&procStatus)))
                                    && procStatus != ProcessStatus_TerminatedNormally)
                                || (   SUCCEEDED(pProcess->COMGETTER(ExitCode(&exitCode)))
                                    && exitCode != 0)
                               )
                            {
                                rc = setProgressErrorMsg(VBOX_E_IPRT_ERROR,
                                                         Utf8StrFmt(GuestSession::tr("Copying file \"%s\" failed with status %ld, exit code %d"),
                                                                    mSource.c_str(), procStatus, exitCode)); /**@todo Add stringify methods! */
                            }
                            else /* Yay, success! */
                                rc = setProgressSuccess();
                        }
                    }

                    pProcess->close();
                }

                RTFileClose(fileDest);
            }
        }
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SessionTaskCopyFrom::RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress)
{
    LogFlowThisFunc(("strDesc=%s, strSource=%s, strDest=%s, uFlags=%x\n",
                     strDesc.c_str(), mSource.c_str(), mDest.c_str(), mFlags));

    mDesc = strDesc;
    mProgress = pProgress;

    int rc = RTThreadCreate(NULL, SessionTaskCopyFrom::taskThread, this,
                            0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                            "gctlCpyFrom");
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
int SessionTaskCopyFrom::taskThread(RTTHREAD Thread, void *pvUser)
{
    std::auto_ptr<SessionTaskCopyFrom> task(static_cast<SessionTaskCopyFrom*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    LogFlowFunc(("pTask=%p\n", task.get()));
    return task->Run();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestSession::init(Guest *aGuest, ULONG aSessionID,
                       Utf8Str aUser, Utf8Str aPassword, Utf8Str aDomain, Utf8Str aName)
{
    LogFlowThisFuncEnter();

    AssertPtrReturn(aGuest, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    mData.mTimeout = 30 * 60 * 1000; /* Session timeout is 30 mins by default. */
    mData.mParent = aGuest;
    mData.mId = aSessionID;

    mData.mCredentials.mUser = aUser;
    mData.mCredentials.mPassword = aPassword;
    mData.mCredentials.mDomain = aDomain;
    mData.mName = aName;

    /* Confirm a successful initialization when it's the case. */
    autoInitSpan.setSucceeded();

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestSession::uninit(void)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

#ifdef VBOX_WITH_GUEST_CONTROL
    for (SessionDirectories::iterator itDirs = mData.mDirectories.begin();
         itDirs != mData.mDirectories.end(); ++itDirs)
    {
        (*itDirs)->uninit();
        (*itDirs).setNull();
    }
    mData.mDirectories.clear();

    for (SessionFiles::iterator itFiles = mData.mFiles.begin();
         itFiles != mData.mFiles.end(); ++itFiles)
    {
        (*itFiles)->uninit();
        (*itFiles).setNull();
    }
    mData.mFiles.clear();

    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
        itProcs->second->close();
    }

    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
        itProcs->second->uninit();
        itProcs->second.setNull();
    }
    mData.mProcesses.clear();

    mData.mParent->sessionClose(this);

    LogFlowThisFuncLeave();
#endif
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestSession::COMGETTER(User)(BSTR *aUser)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aUser);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mCredentials.mUser.cloneTo(aUser);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Domain)(BSTR *aDomain)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aDomain);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mCredentials.mDomain.cloneTo(aDomain);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Name)(BSTR *aName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mName.cloneTo(aName);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Id)(ULONG *aId)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aId = mData.mId;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Timeout)(ULONG *aTimeout)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aTimeout);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = mData.mTimeout;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMSETTER(Timeout)(ULONG aTimeout)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mTimeout = aTimeout;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Environment)(ComSafeArrayOut(BSTR, aEnvironment))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aEnvironment);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    size_t cEnvVars = mData.mEnvironment.Size();
    LogFlowThisFunc(("%s cEnvVars=%RU32\n", mData.mName.c_str(), cEnvVars));
    com::SafeArray<BSTR> environment(cEnvVars);

    for (size_t i = 0; i < cEnvVars; i++)
    {
        Bstr strEnv(mData.mEnvironment.Get(i));
        strEnv.cloneTo(&environment[i]);
    }
    environment.detachTo(ComSafeArrayOutArg(aEnvironment));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMSETTER(Environment)(ComSafeArrayIn(IN_BSTR, aValues))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<IN_BSTR> environment(ComSafeArrayInArg(aValues));

    int rc = VINF_SUCCESS;
    for (size_t i = 0; i < environment.size() && RT_SUCCESS(rc); i++)
    {
        Utf8Str strEnv(environment[i]);
        if (!strEnv.isEmpty()) /* Silently skip empty entries. */
            rc = mData.mEnvironment.Set(strEnv);
    }

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
    LogFlowFuncLeaveRC(hr);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Processes)(ComSafeArrayOut(IGuestProcess *, aProcesses))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aProcesses);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestProcess> collection(mData.mProcesses);
    collection.detachTo(ComSafeArrayOutArg(aProcesses));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Directories)(ComSafeArrayOut(IGuestDirectory *, aDirectories))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aDirectories);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestDirectory> collection(mData.mDirectories);
    collection.detachTo(ComSafeArrayOutArg(aDirectories));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Files)(ComSafeArrayOut(IGuestFile *, aFiles))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aFiles);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestFile> collection(mData.mFiles);
    collection.detachTo(ComSafeArrayOutArg(aFiles));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// private methods
/////////////////////////////////////////////////////////////////////////////

int GuestSession::directoryClose(ComObjPtr<GuestDirectory> pDirectory)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (SessionDirectories::iterator itDirs = mData.mDirectories.begin();
         itDirs != mData.mDirectories.end(); ++itDirs)
    {
        if (pDirectory == (*itDirs))
        {
            mData.mDirectories.erase(itDirs);
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::directoryCreateInternal(const Utf8Str &strPath, uint32_t uMode, uint32_t uFlags, ComObjPtr<GuestDirectory> &pDirectory)
{
    LogFlowThisFunc(("strPath=%s, uMode=%x, uFlags=%x\n",
                     strPath.c_str(), uMode, uFlags));

    GuestProcessStartupInfo procInfo;
    procInfo.mName      = Utf8StrFmt(tr("Creating directory \"%s\"", strPath.c_str()));
    procInfo.mCommand   = Utf8Str(VBOXSERVICE_TOOL_MKDIR);
    procInfo.mFlags     = ProcessCreateFlag_Hidden;

    int rc = VINF_SUCCESS;

    /* Construct arguments. */
    if (uFlags & DirectoryCreateFlag_Parents)
        procInfo.mArguments.push_back(Utf8Str("--parents")); /* We also want to create the parent directories. */
    if (uMode)
    {
        procInfo.mArguments.push_back(Utf8Str("--mode")); /* Set the creation mode. */

        char szMode[16];
        if (RTStrPrintf(szMode, sizeof(szMode), "%o", uMode))
        {
            procInfo.mArguments.push_back(Utf8Str(szMode));
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }
    procInfo.mArguments.push_back(strPath); /* The directory we want to create. */

    ComObjPtr<GuestProcess> pProcess;
    rc = processCreateExInteral(procInfo, pProcess);
    if (RT_SUCCESS(rc))
        rc = pProcess->startProcess();
    if (RT_SUCCESS(rc))
    {
        GuestProcessWaitResult waitRes;
        rc = pProcess->waitFor(ProcessWaitForFlag_Terminate, 30 * 1000 /* Timeout */, waitRes);
        if (RT_SUCCESS(rc))
        {
            ProcessStatus_T procStatus;
            HRESULT hr = pProcess->COMGETTER(Status)(&procStatus);
            ComAssertComRC(hr);
            if (procStatus == ProcessStatus_TerminatedNormally)
            {
                LONG lExitCode;
                pProcess->COMGETTER(ExitCode)(&lExitCode);
                if (lExitCode != 0)
                    return VERR_CANT_CREATE;
            }
            else
                rc = VERR_BROKEN_PIPE; /** @todo Find a better rc. */
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    try
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Create the directory object. */
        HRESULT hr = pDirectory.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        /* Note: There will be a race between creating and getting/initing the directory
                 object here. */
        rc = pDirectory->init(this /* Parent */, strPath);
        if (RT_FAILURE(rc)) throw rc;

        /* Add the created directory to our vector. */
        mData.mDirectories.push_back(pDirectory);

        LogFlowFunc(("Added new directory \"%s\" (Session: %RU32)\n",
                     strPath.c_str(), mData.mId));
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::directoryOpenInternal(const Utf8Str &strPath, const Utf8Str &strFilter,
                                        uint32_t uFlags, ComObjPtr<GuestDirectory> &pDirectory)
{
    LogFlowThisFunc(("strPath=%s, strPath=%s, uFlags=%x\n",
                     strPath.c_str(), strFilter.c_str(), uFlags));
    int rc;

    try
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Create the directory object. */
        HRESULT hr = pDirectory.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        rc = pDirectory->init(this /* Parent */,
                              strPath, strFilter, uFlags);
        if (RT_FAILURE(rc)) throw rc;

        /* Add the created directory to our vector. */
        mData.mDirectories.push_back(pDirectory);

        LogFlowFunc(("Added new directory \"%s\" (Session: %RU32)\n",
                     strPath.c_str(), mData.mId));
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::dispatchToProcess(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData)
{
    LogFlowFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint32_t uProcessID = VBOX_GUESTCTRL_CONTEXTID_GET_PROCESS(uContextID);
#ifdef DEBUG
    LogFlowFunc(("uProcessID=%RU32 (%RU32 total)\n",
                 uProcessID, mData.mProcesses.size()));
#endif
    int rc;
    SessionProcesses::const_iterator itProc
        = mData.mProcesses.find(uProcessID);
    if (itProc != mData.mProcesses.end())
    {
        ComObjPtr<GuestProcess> pProcess(itProc->second);
        Assert(!pProcess.isNull());

        alock.release();
        rc = pProcess->callbackDispatcher(uContextID, uFunction, pvData, cbData);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::fileClose(ComObjPtr<GuestFile> pFile)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (SessionFiles::iterator itFiles = mData.mFiles.begin();
         itFiles != mData.mFiles.end(); ++itFiles)
    {
        if (pFile == (*itFiles))
        {
            mData.mFiles.erase(itFiles);
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::fileOpenInternal(const Utf8Str &strPath, const Utf8Str &strOpenMode, const Utf8Str &strDisposition,
                                   uint32_t uCreationMode, int64_t iOffset, ComObjPtr<GuestFile> &pFile)
{
    LogFlowThisFunc(("strPath=%s, strOpenMode=%s, strDisposition=%s, uCreationMode=%x, iOffset=%RI64\n",
                     strPath.c_str(), strOpenMode.c_str(), strDisposition.c_str(), uCreationMode, iOffset));
    int rc;

    try
    {
        /* Create the directory object. */
        HRESULT hr = pFile.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        /* Note: There will be a race between creating and getting/initing the directory
                 object here. */
        rc = pFile->init(this /* Parent */,
                         strPath, strOpenMode, strDisposition, uCreationMode, iOffset);
        if (RT_FAILURE(rc)) throw rc;

        /* Add the created directory to our vector. */
        mData.mFiles.push_back(pFile);

        LogFlowFunc(("Added new file \"%s\" (Session: %RU32\n",
                     strPath.c_str(), mData.mId));
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* Note: Will work on directories and others, too. */
int GuestSession::fileQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    GuestProcessStartupInfo procInfo;
    procInfo.mName    = Utf8StrFmt(tr("Querying info for \"%s\""), strPath.c_str());
    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_STAT);
    procInfo.mFlags   = ProcessCreateFlag_Hidden | ProcessCreateFlag_WaitForStdOut;

    /* Construct arguments. */
    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    procInfo.mArguments.push_back(strPath);

    GuestProcessStream streamOut;

    ComObjPtr<GuestProcess> pProcess;
    int rc = processCreateExInteral(procInfo, pProcess);
    if (RT_SUCCESS(rc))
        rc = pProcess->startProcess();
    if (RT_SUCCESS(rc))
    {
        GuestProcessWaitResult waitRes;
        BYTE byBuf[_64K];
        size_t cbRead = 0;

        /** @todo Merge with GuestDirectory::read. */
        for (;;)
        {
            rc = pProcess->waitFor(ProcessWaitForFlag_StdOut,
                                   30 * 1000 /* Timeout */, waitRes);
            if (   RT_FAILURE(rc)
                || waitRes.mResult == ProcessWaitResult_Terminate
                || waitRes.mResult == ProcessWaitResult_Error
                || waitRes.mResult == ProcessWaitResult_Timeout)
            {
                break;
            }

            rc = pProcess->readData(OUTPUT_HANDLE_ID_STDOUT, sizeof(byBuf),
                                    30 * 1000 /* Timeout */, byBuf, sizeof(byBuf),
                                    &cbRead);
            if (RT_FAILURE(rc))
                break;

            if (cbRead)
            {
                rc = streamOut.AddData(byBuf, cbRead);
                if (RT_FAILURE(rc))
                    break;
            }
        }

        LogFlowThisFunc(("rc=%Rrc, cbRead=%RU64, cbStreamOut=%RU32\n",
                         rc, cbRead, streamOut.GetSize()));
    }

    if (RT_SUCCESS(rc))
    {
        GuestProcessStreamBlock streamBlock;
        rc = streamOut.ParseBlock(streamBlock);
        if (RT_SUCCESS(rc))
        {
            rc = objData.FromStat(streamBlock);
        }
        else
            AssertMsgFailed(("Parsing stream block failed: %Rrc\n", rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::fileQuerySizeInternal(const Utf8Str &strPath, int64_t *pllSize)
{
    AssertPtrReturn(pllSize, VERR_INVALID_POINTER);

    GuestFsObjData objData;
    int rc = fileQueryInfoInternal(strPath, objData);
    if (RT_SUCCESS(rc))
    {
        if (objData.mType == FsObjType_File)
            *pllSize = objData.mObjectSize;
        else
            rc = VERR_NOT_A_FILE;
    }

    return rc;
}

const GuestCredentials& GuestSession::getCredentials(void)
{
    return mData.mCredentials;
}

const GuestEnvironment& GuestSession::getEnvironment(void)
{
    return mData.mEnvironment;
}

Utf8Str GuestSession::getName(void)
{
    return mData.mName;
}

int GuestSession::processClose(ComObjPtr<GuestProcess> pProcess)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
        if (pProcess == itProcs->second)
        {
            LogFlowFunc(("Removing process (Session: %RU32) with process ID=%RU32, guest PID=%RU32 (now total %ld processes)\n",
                         mData.mId, itProcs->second->getProcessID(), itProcs->second->getPID(), mData.mProcesses.size() - 1));

            mData.mProcesses.erase(itProcs);
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

/**
 * Creates but does *not* start the process yet. See GuestProcess::startProcess() or
 * GuestProcess::startProcessAsync() for that.
 *
 * @return  IPRT status code.
 * @param   procInfo
 * @param   pProcess
 */
int GuestSession::processCreateExInteral(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProcess)
{
    LogFlowFunc(("mCmd=%s, mFlags=%x, mTimeoutMS=%RU32\n",
                 procInfo.mCommand.c_str(), procInfo.mFlags, procInfo.mTimeoutMS));
#ifdef DEBUG
    if (procInfo.mArguments.size())
    {
        LogFlowFunc(("Arguments:"));
        ProcessArguments::const_iterator it = procInfo.mArguments.begin();
        while (it != procInfo.mArguments.end())
        {
            LogFlow((" %s", (*it).c_str()));
            it++;
        }
        LogFlow(("\n"));
    }
#endif

    /* Validate flags. */
    if (procInfo.mFlags)
    {
        if (   !(procInfo.mFlags & ProcessCreateFlag_IgnoreOrphanedProcesses)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForProcessStartOnly)
            && !(procInfo.mFlags & ProcessCreateFlag_Hidden)
            && !(procInfo.mFlags & ProcessCreateFlag_NoProfile)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdOut)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdErr))
        {
            return VERR_INVALID_PARAMETER;
        }
    }

    /* Adjust timeout. If set to 0, we define
     * an infinite timeout. */
    if (procInfo.mTimeoutMS == 0)
        procInfo.mTimeoutMS = UINT32_MAX;

    /** @tood Implement process priority + affinity. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_MAX_PROCS_REACHED;
    if (mData.mProcesses.size() >= VBOX_GUESTCTRL_MAX_PROCESSES)
        return rc;

    /* Create a new (host-based) process ID and assign it. */
    uint32_t uNewProcessID = 0;
    ULONG uTries = 0;

    for (;;)
    {
        /* Is the context ID already used? */
        if (!processExists(uNewProcessID, NULL /* pProgress */))
        {
            /* Callback with context ID was not found. This means
             * we can use this context ID for our new callback we want
             * to add below. */
            rc = VINF_SUCCESS;
            break;
        }
        uNewProcessID++;
        if (uNewProcessID == VBOX_GUESTCTRL_MAX_PROCESSES)
            uNewProcessID = 0;

        if (++uTries == UINT32_MAX)
            break; /* Don't try too hard. */
    }
    if (RT_FAILURE(rc)) return rc;

    try
    {
        /* Create the process object. */
        HRESULT hr = pProcess.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        rc = pProcess->init(mData.mParent->getConsole() /* Console */, this /* Session */,
                            uNewProcessID, procInfo);
        if (RT_FAILURE(rc)) throw rc;

        /* Add the created process to our map. */
        mData.mProcesses[uNewProcessID] = pProcess;

        LogFlowFunc(("Added new process (Session: %RU32) with process ID=%RU32 (now total %ld processes)\n",
                     mData.mId, uNewProcessID, mData.mProcesses.size()));
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    return rc;
}

inline bool GuestSession::processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess)
{
    SessionProcesses::const_iterator it = mData.mProcesses.find(uProcessID);
    if (it != mData.mProcesses.end())
    {
        if (pProcess)
            *pProcess = it->second;
        return true;
    }
    return false;
}

inline int GuestSession::processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess)
{
    AssertReturn(uPID, false);
    /* pProcess is optional. */

    SessionProcesses::iterator it = mData.mProcesses.begin();
    for (; it != mData.mProcesses.end(); it++)
    {
        ComObjPtr<GuestProcess> pCurProc = it->second;
        AutoCaller procCaller(pCurProc);
        if (procCaller.rc())
            return VERR_COM_INVALID_OBJECT_STATE;

        if (it->second->getPID() == uPID)
        {
            if (pProcess)
                *pProcess = pCurProc;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::startTaskAsync(const Utf8Str &strTaskDesc,
                                 GuestSessionTask *pTask, ComObjPtr<Progress> &pProgress)
{
    LogFlowThisFunc(("strTaskDesc=%s, pTask=%p\n", strTaskDesc.c_str(), pTask));

    AssertPtrReturn(pTask, VERR_INVALID_POINTER);

    int rc;

    try
    {
        /* Create the progress object. */
        HRESULT hr = pProgress.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        hr = pProgress->init(static_cast<IGuestSession*>(this),
                             Bstr(strTaskDesc).raw(),
                             TRUE /* aCancelable */);
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        /* Initialize our worker task. */
        std::auto_ptr<GuestSessionTask> task(pTask);

        rc = task->RunAsync(strTaskDesc, pProgress);
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Queries/collects information prior to establishing a guest session.
 * This is necessary to know which guest control protocol version to use,
 * among other things (later).
 *
 * @return  IPRT status code.
 */
int GuestSession::queryInfo(void)
{
#if 1
    /* Since the new functions were not implemented yet, force Main to use protocol ver 1. */
    mData.mProtocolVersion = 1;
#else
    /*
     * Try querying the guest control protocol version running on the guest.
     * This is done using the Guest Additions version
     */
    ComObjPtr<Guest> pGuest = mData.mParent;
    Assert(!pGuest.isNull());

    uint32_t uVerAdditions = pGuest->getAdditionsVersion();
    mData.mProtocolVersion = (   VBOX_FULL_VERSION_GET_MAJOR(uVerAdditions) >= 4
                              && VBOX_FULL_VERSION_GET_MINOR(uVerAdditions) >= 2) /** @todo What's about v5.0 ? */
                           ? 2  /* Guest control 2.0. */
                           : 1; /* Legacy guest control (VBox < 4.2). */
    /* Build revision is ignored. */

    /* Tell the user but don't bitch too often. */
    static short s_gctrlLegacyWarning = 0;
    if (s_gctrlLegacyWarning++ < 3) /** @todo Find a bit nicer text. */
        LogRel((tr("Warning: Guest Additions are older (%ld.%ld) than host capabilities for guest control, please upgrade them. Using protocol version %ld now\n"),
                VBOX_FULL_VERSION_GET_MAJOR(uVerAdditions), VBOX_FULL_VERSION_GET_MINOR(uVerAdditions), mData.mProtocolVersion));
#endif
    return VINF_SUCCESS;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestSession::Close(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    uninit();

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::CopyFrom(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgOutPointerValid(aProgress);

    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aSource) == NULL || *(aSource) == '\0'))
        return setError(E_INVALIDARG, tr("No source specified"));
    if (RT_UNLIKELY((aDest) == NULL || *(aDest) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = CopyFileFlag_None;
    if (aFlags)
    {
        com::SafeArray<CopyFileFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    ComObjPtr<Progress> pProgress;
    SessionTaskCopyFrom *pTask = new SessionTaskCopyFrom(this /* GuestSession */,
                                                         Utf8Str(aSource), Utf8Str(aDest), fFlags);
    AssertPtrReturn(pTask, VERR_NO_MEMORY);
    int rc = startTaskAsync(Utf8StrFmt(tr("Copying \"%ls\" from guest to \"%ls\" on the host"), aSource, aDest),
                            pTask, pProgress);
    if (RT_SUCCESS(rc))
    {
        /* Return progress to the caller. */
        hr = pProgress.queryInterfaceTo(aProgress);
    }
    else
        hr = setError(VBOX_E_IPRT_ERROR,
                      tr("Starting task for copying file \"%ls\" from guest to \"%ls\" on the host failed: %Rrc"), rc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::CopyTo(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgOutPointerValid(aProgress);

    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aSource) == NULL || *(aSource) == '\0'))
        return setError(E_INVALIDARG, tr("No source specified"));
    if (RT_UNLIKELY((aDest) == NULL || *(aDest) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = CopyFileFlag_None;
    if (aFlags)
    {
        com::SafeArray<CopyFileFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    ComObjPtr<Progress> pProgress;
    SessionTaskCopyTo *pTask = new SessionTaskCopyTo(this /* GuestSession */,
                                                     Utf8Str(aSource), Utf8Str(aDest), fFlags);
    AssertPtrReturn(pTask, VERR_NO_MEMORY);
    int rc = startTaskAsync(Utf8StrFmt(tr("Copying \"%ls\" from host to \"%ls\" on the guest"), aSource, aDest),
                            pTask, pProgress);
    if (RT_SUCCESS(rc))
    {
        /* Return progress to the caller. */
        hr = pProgress.queryInterfaceTo(aProgress);
    }
    else
        hr = setError(VBOX_E_IPRT_ERROR,
                      tr("Starting task for copying file \"%ls\" from host to \"%ls\" on the guest failed: %Rrc"), rc);
   return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryCreate(IN_BSTR aPath, ULONG aMode,
                                           ComSafeArrayIn(DirectoryCreateFlag_T, aFlags), IGuestDirectory **aDirectory)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to create specified"));
    /* aDirectory is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryCreateFlag_None;
    if (aFlags)
    {
        com::SafeArray<DirectoryCreateFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];

        if (fFlags)
        {
            if (!(fFlags & DirectoryCreateFlag_Parents))
                return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), fFlags);
        }
    }

    HRESULT hr = S_OK;

    ComObjPtr <GuestDirectory> pDirectory;
    int rc = directoryCreateInternal(Utf8Str(aPath), (uint32_t)aMode, fFlags, pDirectory);
    if (RT_SUCCESS(rc))
    {
        if (aDirectory)
        {
            /* Return directory object to the caller. */
            hr = pDirectory.queryInterfaceTo(aDirectory);
        }
        else
        {
            rc = directoryClose(pDirectory);
            if (RT_FAILURE(rc))
                hr = setError(VBOX_E_IPRT_ERROR, tr("Unable to close directory object, rc=%Rrc"), rc);
        }
    }
    else
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Invalid parameters given"));
               break;

            case VERR_BROKEN_PIPE:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Unexpectedly aborted"));
               break;

            case VERR_CANT_CREATE:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Could not create directory"));
               break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryCreateTemp(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aName, IGuestDirectory **aDirectory)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aTemplate) == NULL || *(aTemplate) == '\0'))
        return setError(E_INVALIDARG, tr("No file to remove specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestProcessStartupInfo procInfo;
    GuestProcessStream      streamOut;
    int rc = VINF_SUCCESS;

    try  /* Can this be done without exceptions? */
    {
        Utf8Str strTemplate(aTemplate);
        procInfo.mName    = Utf8StrFmt(tr("Creating temporary directory from template \"%s\"",
                                       strTemplate.c_str()));
        procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_MKTEMP);
        /* Construct arguments. */
        procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
        procInfo.mArguments.push_back(Bstr(aTemplate)); /* The template we want to use. */
    }
    catch (...)
    {
        return E_OUTOFMEMORY;
    }

    ComObjPtr<GuestProcess> pProcess;
    rc = processCreateExInteral(procInfo, pProcess);
    if (RT_SUCCESS(rc))
    {
        GuestProcessWaitResult waitRes;
        BYTE byBuf[_64K];
        size_t cbRead;

        for (;;)
        {
            rc = pProcess->waitFor(ProcessWaitForFlag_StdOut,
                                   30 * 1000 /* Timeout */, waitRes);
            if (   RT_FAILURE(rc)
                || waitRes.mResult != ProcessWaitResult_StdOut)
            {
                break;
            }

            rc = pProcess->readData(OUTPUT_HANDLE_ID_STDOUT, sizeof(byBuf),
                                    30 * 1000 /* Timeout */, byBuf, sizeof(byBuf),
                                    &cbRead);
            if (RT_FAILURE(rc))
                break;

            rc = streamOut.AddData(byBuf, cbRead);
            if (RT_FAILURE(rc))
                break;
        }

        LogFlowThisFunc(("rc=%Rrc, cbRead=%RU32, cbStreamOut=%RU32\n",
                         rc, cbRead, streamOut.GetSize()));
    } 

    if (RT_FAILURE(rc))
        return setError(E_FAIL, tr("Error while creating temporary directory: %Rrc"), rc);
    if (!streamOut.GetSize())
        return setError(E_FAIL, tr("No return code after creating temporary directory"));
    GuestProcessStreamBlock streamBlock;
    rc = streamOut.ParseBlock(streamBlock);
    if (RT_SUCCESS(rc))
    {
        streamBlock.GetString("name");
        int64_t i64rc;
        if (RT_FAILURE(streamBlock.GetInt64Ex("rc", &i64rc)))
            return setError(E_FAIL, tr("No return code after creating temporary directory"));
        // *aReturnCode = (LONG)i64rc;
    }
    else
        return setError(E_FAIL, tr("Error while getting return code from creating temporary directory: %Rrc"), rc);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryExists(IN_BSTR aPath, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to check existence for specified"));
    CheckComArgOutPointerValid(aExists);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestFsObjData objData;
    int rc = fileQueryInfoInternal(Utf8Str(aPath), objData);
    if (RT_SUCCESS(rc))
    {
        *aExists = objData.mType == FsObjType_Directory;
    }
    else
    {
        switch (rc)
        {
            /** @todo Add more errors here! */

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying directory existence failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryOpen(IN_BSTR aPath, IN_BSTR aFilter, ComSafeArrayIn(DirectoryOpenFlag_T, aFlags), IGuestDirectory **aDirectory)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to open specified"));
    if (RT_UNLIKELY((aFilter) != NULL && *(aFilter) != '\0'))
        return setError(E_INVALIDARG, tr("Directory filters not implemented yet"));

    CheckComArgOutPointerValid(aDirectory);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryOpenFlag_None;
    if (aFlags)
    {
        com::SafeArray<DirectoryOpenFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];

        if (fFlags)
            return setError(E_INVALIDARG, tr("Open flags (%#x) not implemented yet"), fFlags);
    }

    HRESULT hr = S_OK;

    ComObjPtr <GuestDirectory> pDirectory;
    int rc = directoryOpenInternal(Utf8Str(aPath), Utf8Str(aFilter), fFlags, pDirectory);
    if (RT_SUCCESS(rc))
    {
        if (aDirectory)
        {
            /* Return directory object to the caller. */
            hr = pDirectory.queryInterfaceTo(aDirectory);
        }
        else
        {
            rc = directoryClose(pDirectory);
            if (RT_FAILURE(rc))
                hr = setError(VBOX_E_IPRT_ERROR, tr("Unable to close directory object, rc=%Rrc"), rc);
        }
    }
    else
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Opening directory failed: Invalid parameters given"));
               break;

            case VERR_BROKEN_PIPE:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Opening directory failed: Unexpectedly aborted"));
               break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Opening directory failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryQueryInfo(IN_BSTR aPath, IGuestFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRemove(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRemoveRecursive(IN_BSTR aPath, ComSafeArrayIn(DirectoryRemoveRecFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRename(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectorySetACL(IN_BSTR aPath, IN_BSTR aACL)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentClear(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mEnvironment.Clear();

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentGet(IN_BSTR aName, BSTR *aValue)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aName) == NULL || *(aName) == '\0'))
        return setError(E_INVALIDARG, tr("No value name specified"));

    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Bstr strValue(mData.mEnvironment.Get(Utf8Str(aName)));
    strValue.cloneTo(aValue);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentSet(IN_BSTR aName, IN_BSTR aValue)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aName) == NULL || *(aName) == '\0'))
        return setError(E_INVALIDARG, tr("No value name specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = mData.mEnvironment.Set(Utf8Str(aName), Utf8Str(aValue));

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
    LogFlowFuncLeaveRC(hr);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentUnset(IN_BSTR aName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mEnvironment.Unset(Utf8Str(aName));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileCreateTemp(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aName, IGuestFile **aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileExists(IN_BSTR aPath, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to check existence for specified"));
    CheckComArgOutPointerValid(aExists);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestFsObjData objData;
    int rc = fileQueryInfoInternal(Utf8Str(aPath), objData);
    if (RT_SUCCESS(rc))
    {
        *aExists = objData.mType == FsObjType_File;
    }
    else
    {
        switch (rc)
        {
            /** @todo Add more errors here! */

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying file existence failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileRemove(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to remove specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestProcessStartupInfo procInfo;
    GuestProcessStream      streamOut;
    int rc = VINF_SUCCESS;

    try  /* Can this be done without exceptions? */
    {
        Utf8Str strPath(aPath);
        procInfo.mName    = Utf8StrFmt(tr("Removing file \"%s\"",
                                       strPath.c_str()));
        procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_RM);
        /* Construct arguments. */
        procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
        procInfo.mArguments.push_back(Bstr(aPath)); /* The directory we want to create. */
    }
    catch (...)
    {
        return E_OUTOFMEMORY;
    }

    ComObjPtr<GuestProcess> pProcess;
    rc = processCreateExInteral(procInfo, pProcess);
    if (RT_SUCCESS(rc))
    {
        GuestProcessWaitResult waitRes;
        BYTE byBuf[_64K];
        size_t cbRead;

        for (;;)
        {
            rc = pProcess->waitFor(ProcessWaitForFlag_StdOut,
                                   30 * 1000 /* Timeout */, waitRes);
            if (   RT_FAILURE(rc)
                || waitRes.mResult != ProcessWaitResult_StdOut)
            {
                break;
            }

            rc = pProcess->readData(OUTPUT_HANDLE_ID_STDOUT, sizeof(byBuf),
                                    30 * 1000 /* Timeout */, byBuf, sizeof(byBuf),
                                    &cbRead);
            if (RT_FAILURE(rc))
                break;

            rc = streamOut.AddData(byBuf, cbRead);
            if (RT_FAILURE(rc))
                break;
        }

        LogFlowThisFunc(("rc=%Rrc, cbRead=%RU32, cbStreamOut=%RU32\n",
                         rc, cbRead, streamOut.GetSize()));
    }

    if (RT_FAILURE(rc))
        return setError(E_FAIL, tr("Error while deleting file: %Rrc"), rc);
    if (!streamOut.GetSize())
        return setError(E_FAIL, tr("No return code after deleting file"));
    GuestProcessStreamBlock streamBlock;
    rc = streamOut.ParseBlock(streamBlock);
    if (RT_SUCCESS(rc))
    {
        streamBlock.GetString("fname");
        int64_t i64rc;
        if (RT_FAILURE(streamBlock.GetInt64Ex("rc", &i64rc)))
            return setError(E_FAIL, tr("No return code after deleting file"));
    }
    else
        return setError(E_FAIL, tr("Error while getting return code from deleting file: %Rrc"), rc);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileOpen(IN_BSTR aPath, IN_BSTR aOpenMode, IN_BSTR aDisposition, ULONG aCreationMode, LONG64 aOffset, IGuestFile **aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to open specified"));
    if (RT_UNLIKELY((aOpenMode) == NULL || *(aOpenMode) == '\0'))
        return setError(E_INVALIDARG, tr("No open mode specified"));
    if (RT_UNLIKELY((aDisposition) == NULL || *(aDisposition) == '\0'))
        return setError(E_INVALIDARG, tr("No disposition mode specified"));

    CheckComArgOutPointerValid(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /** @todo Validate open mode. */
    /** @todo Validate disposition mode. */

    /** @todo Validate creation mode. */
    uint32_t uCreationMode = 0;

    HRESULT hr = S_OK;

    ComObjPtr <GuestFile> pFile;
    int rc = fileOpenInternal(Utf8Str(aPath), Utf8Str(aOpenMode), Utf8Str(aDisposition),
                              aCreationMode, aOffset, pFile);
    if (RT_SUCCESS(rc))
    {
        /* Return directory object to the caller. */
        hr = pFile.queryInterfaceTo(aFile);
    }
    else
    {
        switch (rc)
        {
            /** @todo Add more error info! */

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileQueryInfo(IN_BSTR aPath, IGuestFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileQuerySize(IN_BSTR aPath, LONG64 *aSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to query size for specified"));
    CheckComArgOutPointerValid(aSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    int64_t llSize;
    int rc = fileQuerySizeInternal(Utf8Str(aPath), &llSize);
    if (RT_SUCCESS(rc))
    {
        *aSize = llSize;
    }
    else
    {
        switch (rc)
        {
            /** @todo Add more errors here! */

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying file size failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileRename(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileSetACL(IN_BSTR aPath, IN_BSTR aACL)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessCreate(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                         ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS, IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    com::SafeArray<LONG> affinity;

    HRESULT hr = ProcessCreateEx(aCommand, ComSafeArrayInArg(aArguments), ComSafeArrayInArg(aEnvironment),
                                 ComSafeArrayInArg(aFlags), aTimeoutMS, ProcessPriority_Default, ComSafeArrayAsInParam(affinity), aProcess);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessCreateEx(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                           ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS,
                                           ProcessPriority_T aPriority, ComSafeArrayIn(LONG, aAffinity),
                                           IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aCommand) == NULL || *(aCommand) == '\0'))
        return setError(E_INVALIDARG, tr("No command to execute specified"));
    CheckComArgOutPointerValid(aProcess);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestProcessStartupInfo procInfo;
    procInfo.mCommand = Utf8Str(aCommand);

    if (aArguments)
    {
        com::SafeArray<IN_BSTR> arguments(ComSafeArrayInArg(aArguments));
        for (size_t i = 0; i < arguments.size(); i++)
            procInfo.mArguments.push_back(Utf8Str(arguments[i]));
    }

    int rc = VINF_SUCCESS;

    /*
     * Create the process environment:
     * - Apply the session environment in a first step, and
     * - Apply environment variables specified by this call to
     *   have the chance of overwriting/deleting session entries.
     */
    procInfo.mEnvironment = mData.mEnvironment; /* Apply original session environment. */

    if (aEnvironment)
    {
        com::SafeArray<IN_BSTR> environment(ComSafeArrayInArg(aEnvironment));
        for (size_t i = 0; i < environment.size() && RT_SUCCESS(rc); i++)
            rc = procInfo.mEnvironment.Set(Utf8Str(environment[i]));
    }

    HRESULT hr = S_OK;

    if (RT_SUCCESS(rc))
    {
        if (aFlags)
        {
            com::SafeArray<ProcessCreateFlag_T> flags(ComSafeArrayInArg(aFlags));
            for (size_t i = 0; i < flags.size(); i++)
                procInfo.mFlags |= flags[i];
        }

        procInfo.mTimeoutMS = aTimeoutMS;

        if (aAffinity)
        {
            com::SafeArray<LONG> affinity(ComSafeArrayInArg(aAffinity));
            for (size_t i = 0; i < affinity.size(); i++)
                procInfo.mAffinity[i] = affinity[i]; /** @todo Really necessary? Later. */
        }

        procInfo.mPriority = aPriority;

        ComObjPtr<GuestProcess> pProcess;
        rc = processCreateExInteral(procInfo, pProcess);
        if (RT_SUCCESS(rc))
        {
            /* Return guest session to the caller. */
            HRESULT hr2 = pProcess.queryInterfaceTo(aProcess);
            if (FAILED(hr2))
                rc = VERR_COM_OBJECT_NOT_FOUND;

            if (RT_SUCCESS(rc))
                rc = pProcess->startProcessAsync();
        }
    }

    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Maximum number of guest processes per session (%ld) reached"),
                              VBOX_GUESTCTRL_MAX_PROCESSES);
                break;

            /** @todo Add more errors here. */

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Could not create guest process, rc=%Rrc"), rc);
                break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessGet(ULONG aPID, IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFunc(("aPID=%RU32\n", aPID));

    CheckComArgOutPointerValid(aProcess);
    if (aPID == 0)
        return setError(E_INVALIDARG, tr("No valid process ID (PID) specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    ComObjPtr<GuestProcess> pProcess;
    int rc = processGetByPID(aPID, &pProcess);
    if (RT_FAILURE(rc))
        hr = setError(E_INVALIDARG, tr("No process with PID %RU32 found"), aPID);

    /* This will set (*aProcess) to NULL if pProgress is NULL. */
    HRESULT hr2 = pProcess.queryInterfaceTo(aProcess);
    if (SUCCEEDED(hr))
        hr = hr2;

    LogFlowThisFunc(("aProcess=%p, hr=%Rhrc\n", *aProcess, hr));
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkCreate(IN_BSTR aSource, IN_BSTR aTarget, SymlinkType_T aType)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkExists(IN_BSTR aSymlink, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRead(IN_BSTR aSymlink, ComSafeArrayIn(SymlinkReadFlag_T, aFlags), BSTR *aTarget)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRemoveDirectory(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRemoveFile(IN_BSTR aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

