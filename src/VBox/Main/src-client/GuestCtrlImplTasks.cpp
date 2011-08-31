/* $Id: */
/** @file
 * VirtualBox Guest Control - Threaded operations (tasks).
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <memory>

#include "GuestImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
#endif

#include <iprt/file.h>
#include <iprt/isofs.h>
#include <iprt/list.h>
#include <iprt/path.h>

GuestTask::GuestTask(TaskType aTaskType, Guest *aThat, Progress *aProgress)
    : taskType(aTaskType),
      pGuest(aThat),
      progress(aProgress),
      rc(S_OK)
{

}

GuestTask::~GuestTask()
{

}

int GuestTask::startThread()
{
    return RTThreadCreate(NULL, GuestTask::taskThread, this,
                          0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                          "GuestTask");
}

/* static */
DECLCALLBACK(int) GuestTask::taskThread(RTTHREAD /* aThread */, void *pvUser)
{
    std::auto_ptr<GuestTask> task(static_cast<GuestTask*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Guest *pGuest = task->pGuest;

    LogFlowFuncEnter();
    LogFlowFunc(("Guest %p\n", pGuest));

    HRESULT rc = S_OK;

    switch (task->taskType)
    {
#ifdef VBOX_WITH_GUEST_CONTROL
        case TaskType_CopyFileToGuest:
        {
            rc = pGuest->taskCopyFileToGuest(task.get());
            break;
        }
        case TaskType_CopyFileFromGuest:
        {
            rc = pGuest->taskCopyFileFromGuest(task.get());
            break;
        }
        case TaskType_UpdateGuestAdditions:
        {
            rc = pGuest->taskUpdateGuestAdditions(task.get());
            break;
        }
#endif
        default:
            AssertMsgFailed(("Invalid task type %u specified!\n", task->taskType));
            break;
    }

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

/* static */
int GuestTask::uploadProgress(unsigned uPercent, void *pvUser)
{
    GuestTask *pTask = *(GuestTask**)pvUser;

    if (    pTask
        && !pTask->progress.isNull())
    {
        BOOL fCanceled;
        pTask->progress->COMGETTER(Canceled)(&fCanceled);
        if (fCanceled)
            return -1;
        pTask->progress->SetCurrentOperationProgress(uPercent);
    }
    return VINF_SUCCESS;
}

/* static */
HRESULT GuestTask::setProgressErrorInfo(HRESULT hr, ComObjPtr<Progress> pProgress,
                                               const char *pszText, ...)
{
    BOOL fCanceled;
    BOOL fCompleted;
    if (   SUCCEEDED(pProgress->COMGETTER(Canceled(&fCanceled)))
        && !fCanceled
        && SUCCEEDED(pProgress->COMGETTER(Completed(&fCompleted)))
        && !fCompleted)
    {
        va_list va;
        va_start(va, pszText);
        HRESULT hr2 = pProgress->notifyCompleteV(hr,
                                                 COM_IIDOF(IGuest),
                                                 Guest::getStaticComponentName(),
                                                 pszText,
                                                 va);
        va_end(va);
        if (hr2 == S_OK) /* If unable to retrieve error, return input error. */
            hr2 = hr;
        return hr2;
    }
    return S_OK;
}

/* static */
HRESULT GuestTask::setProgressErrorInfo(HRESULT hr,
                                        ComObjPtr<Progress> pProgress, ComObjPtr<Guest> pGuest)
{
    return setProgressErrorInfo(hr, pProgress,
                                Utf8Str(com::ErrorInfo((IGuest*)pGuest, COM_IIDOF(IGuest)).getText()).c_str());
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::taskCopyFileToGuest(GuestTask *aTask)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /*
     * Do *not* take a write lock here since we don't (and won't)
     * touch any class-specific data (of IGuest) here - only the member functions
     * which get called here can do that.
     */

    HRESULT rc = S_OK;

    try
    {
        Guest *pGuest = aTask->pGuest;
        AssertPtr(pGuest);

        /* Does our source file exist? */
        if (!RTFileExists(aTask->strSource.c_str()))
        {
            rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                 Guest::tr("Source file \"%s\" does not exist, or is not a file"),
                                                 aTask->strSource.c_str());
        }
        else
        {
            RTFILE fileSource;
            int vrc = RTFileOpen(&fileSource, aTask->strSource.c_str(),
                                 RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
            if (RT_FAILURE(vrc))
            {
                rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                     Guest::tr("Could not open source file \"%s\" for reading (%Rrc)"),
                                                     aTask->strSource.c_str(),  vrc);
            }
            else
            {
                uint64_t cbSize;
                vrc = RTFileGetSize(fileSource, &cbSize);
                if (RT_FAILURE(vrc))
                {
                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                         Guest::tr("Could not query file size of \"%s\" (%Rrc)"),
                                                         aTask->strSource.c_str(), vrc);
                }
                else
                {
                    com::SafeArray<IN_BSTR> args;
                    com::SafeArray<IN_BSTR> env;

                    /*
                     * Prepare tool command line.
                     */
                    char szOutput[RTPATH_MAX];
                    if (RTStrPrintf(szOutput, sizeof(szOutput), "--output=%s", aTask->strDest.c_str()) <= sizeof(szOutput) - 1)
                    {
                        /*
                         * Normalize path slashes, based on the detected guest.
                         */
                        Utf8Str osType = mData.mOSTypeId;
                        if (   osType.contains("Microsoft", Utf8Str::CaseInsensitive)
                            || osType.contains("Windows", Utf8Str::CaseInsensitive))
                        {
                            /* We have a Windows guest. */
                            RTPathChangeToDosSlashes(szOutput, true /* Force conversion. */);
                        }
                        else /* ... or something which isn't from Redmond ... */
                        {
                            RTPathChangeToUnixSlashes(szOutput, true /* Force conversion. */);
                        }

                        args.push_back(Bstr(szOutput).raw());             /* We want to write a file ... */
                    }
                    else
                    {
                        rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                             Guest::tr("Error preparing command line"));
                    }

                    ComPtr<IProgress> execProgress;
                    ULONG uPID;
                    if (SUCCEEDED(rc))
                    {
                        LogRel(("Copying file \"%s\" to guest \"%s\" (%u bytes) ...\n",
                                aTask->strSource.c_str(), aTask->strDest.c_str(), cbSize));
                        /*
                         * Okay, since we gathered all stuff we need until now to start the
                         * actual copying, start the guest part now.
                         */
                        rc = pGuest->ExecuteProcess(Bstr(VBOXSERVICE_TOOL_CAT).raw(),
                                                      ExecuteProcessFlag_Hidden
                                                    | ExecuteProcessFlag_WaitForProcessStartOnly,
                                                    ComSafeArrayAsInParam(args),
                                                    ComSafeArrayAsInParam(env),
                                                    Bstr(aTask->strUserName).raw(),
                                                    Bstr(aTask->strPassword).raw(),
                                                    5 * 1000 /* Wait 5s for getting the process started. */,
                                                    &uPID, execProgress.asOutParam());
                        if (FAILED(rc))
                            rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                    }

                    if (SUCCEEDED(rc))
                    {
                        BOOL fCompleted = FALSE;
                        BOOL fCanceled = FALSE;

                        size_t cbToRead = cbSize;
                        size_t cbTransfered = 0;
                        size_t cbRead;
                        SafeArray<BYTE> aInputData(_64K);
                        while (   SUCCEEDED(execProgress->COMGETTER(Completed(&fCompleted)))
                               && !fCompleted)
                        {
                            if (!cbToRead)
                                cbRead = 0;
                            else
                            {
                                vrc = RTFileRead(fileSource, (uint8_t*)aInputData.raw(),
                                                 RT_MIN(cbToRead, _64K), &cbRead);
                                /*
                                 * Some other error occured? There might be a chance that RTFileRead
                                 * could not resolve/map the native error code to an IPRT code, so just
                                 * print a generic error.
                                 */
                                if (RT_FAILURE(vrc))
                                {
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Could not read from file \"%s\" (%Rrc)"),
                                                                         aTask->strSource.c_str(), vrc);
                                    break;
                                }
                            }

                            /* Resize buffer to reflect amount we just have read.
                             * Size 0 is allowed! */
                            aInputData.resize(cbRead);

                            ULONG uFlags = ProcessInputFlag_None;
                            /* Did we reach the end of the content we want to transfer (last chunk)? */
                            if (   (cbRead < _64K)
                                /* Did we reach the last block which is exactly _64K? */
                                || (cbToRead - cbRead == 0)
                                /* ... or does the user want to cancel? */
                                || (   SUCCEEDED(aTask->progress->COMGETTER(Canceled(&fCanceled)))
                                    && fCanceled)
                               )
                            {
                                uFlags |= ProcessInputFlag_EndOfFile;
                            }

                            /* Transfer the current chunk ... */
                            ULONG uBytesWritten;
                            rc = pGuest->SetProcessInput(uPID, uFlags,
                                                         10 * 1000 /* Wait 10s for getting the input data transfered. */,
                                                         ComSafeArrayAsInParam(aInputData), &uBytesWritten);
                            if (FAILED(rc))
                            {
                                rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                                break;
                            }

                            Assert(cbRead <= cbToRead);
                            Assert(cbToRead >= cbRead);
                            cbToRead -= cbRead;

                            cbTransfered += uBytesWritten;
                            Assert(cbTransfered <= cbSize);
                            aTask->progress->SetCurrentOperationProgress(cbTransfered / (cbSize / 100.0));

                            /* End of file reached? */
                            if (cbToRead == 0)
                                break;

                            /* Did the user cancel the operation above? */
                            if (fCanceled)
                                break;

                            /* Progress canceled by Main API? */
                            if (   SUCCEEDED(execProgress->COMGETTER(Canceled(&fCanceled)))
                                && fCanceled)
                            {
                                rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                     Guest::tr("Copy operation of file \"%s\" was canceled on guest side"),
                                                                     aTask->strSource.c_str());
                                break;
                            }
                        }

                        if (SUCCEEDED(rc))
                        {
                            /*
                             * If we got here this means the started process either was completed,
                             * canceled or we simply got all stuff transferred.
                             */
                            ExecuteProcessStatus_T retStatus;
                            ULONG uRetExitCode;
                            rc = pGuest->executeWaitForStatusChange(uPID, 10 * 1000 /* 10s timeout. */,
                                                                    &retStatus, &uRetExitCode);
                            if (FAILED(rc))
                            {
                                rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                            }
                            else
                            {
                                if (   uRetExitCode != 0
                                    || retStatus    != ExecuteProcessStatus_TerminatedNormally)
                                {
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Guest reported error %u while copying file \"%s\" to \"%s\""),
                                                                         uRetExitCode, aTask->strSource.c_str(), aTask->strDest.c_str());
                                }
                            }
                        }

                        if (SUCCEEDED(rc))
                        {
                            if (fCanceled)
                            {
                                /*
                                 * In order to make the progress object to behave nicely, we also have to
                                 * notify the object with a complete event when it's canceled.
                                 */
                                aTask->progress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                                COM_IIDOF(IGuest),
                                                                Guest::getStaticComponentName(),
                                                                Guest::tr("Copying file \"%s\" canceled"), aTask->strSource.c_str());
                            }
                            else
                            {
                                /*
                                 * Even if we succeeded until here make sure to check whether we really transfered
                                 * everything.
                                 */
                                if (   cbSize > 0
                                    && cbTransfered == 0)
                                {
                                    /* If nothing was transfered but the file size was > 0 then "vbox_cat" wasn't able to write
                                     * to the destination -> access denied. */
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Access denied when copying file \"%s\" to \"%s\""),
                                                                         aTask->strSource.c_str(), aTask->strDest.c_str());
                                }
                                else if (cbTransfered < cbSize)
                                {
                                    /* If we did not copy all let the user know. */
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Copying file \"%s\" failed (%u/%u bytes transfered)"),
                                                                         aTask->strSource.c_str(), cbTransfered, cbSize);
                                }
                                else /* Yay, all went fine! */
                                    aTask->progress->notifyComplete(S_OK);
                            }
                        }
                    }
                }
                RTFileClose(fileSource);
            }
        }
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    /* Clean up */
    aTask->rc = rc;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

HRESULT Guest::taskCopyFileFromGuest(GuestTask *aTask)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /*
     * Do *not* take a write lock here since we don't (and won't)
     * touch any class-specific data (of IGuest) here - only the member functions
     * which get called here can do that.
     */

    HRESULT rc = S_OK;

    try
    {
        Guest *pGuest = aTask->pGuest;
        AssertPtr(pGuest);

        /* Does our source file exist? */
        BOOL fFileExists;
        rc = pGuest->FileExists(Bstr(aTask->strSource).raw(),
                                Bstr(aTask->strUserName).raw(), Bstr(aTask->strPassword).raw(),
                                &fFileExists);
        if (SUCCEEDED(rc))
        {
            if (!fFileExists)
                rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                     Guest::tr("Source file \"%s\" does not exist, or is not a file"),
                                                     aTask->strSource.c_str());
        }

        /* Query file size to make an estimate for our progress object. */
        if (SUCCEEDED(rc))
        {
            LONG64 lFileSize;
            rc = pGuest->FileQuerySize(Bstr(aTask->strSource).raw(),
                                       Bstr(aTask->strUserName).raw(), Bstr(aTask->strPassword).raw(),
                                       &lFileSize);
            if (FAILED(rc))
                rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);

            com::SafeArray<IN_BSTR> args;
            com::SafeArray<IN_BSTR> env;

            if (SUCCEEDED(rc))
            {
                /*
                 * Prepare tool command line.
                 */
                char szSource[RTPATH_MAX];
                if (RTStrPrintf(szSource, sizeof(szSource), "%s", aTask->strSource.c_str()) <= sizeof(szSource) - 1)
                {
                    /*
                     * Normalize path slashes, based on the detected guest.
                     */
                    Utf8Str osType = mData.mOSTypeId;
                    if (   osType.contains("Microsoft", Utf8Str::CaseInsensitive)
                        || osType.contains("Windows", Utf8Str::CaseInsensitive))
                    {
                        /* We have a Windows guest. */
                        RTPathChangeToDosSlashes(szSource, true /* Force conversion. */);
                    }
                    else /* ... or something which isn't from Redmond ... */
                    {
                        RTPathChangeToUnixSlashes(szSource, true /* Force conversion. */);
                    }

                    args.push_back(Bstr(szSource).raw()); /* Tell our cat tool which file to output. */
                }
                else
                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                         Guest::tr("Error preparing command line"));
            }

            ComPtr<IProgress> execProgress;
            ULONG uPID;
            if (SUCCEEDED(rc))
            {
                LogRel(("Copying file \"%s\" to host \"%s\" (%u bytes) ...\n",
                        aTask->strSource.c_str(), aTask->strDest.c_str(), lFileSize));

                /*
                 * Okay, since we gathered all stuff we need until now to start the
                 * actual copying, start the guest part now.
                 */
                rc = pGuest->ExecuteProcess(Bstr(VBOXSERVICE_TOOL_CAT).raw(),
                                            ExecuteProcessFlag_Hidden,
                                            ComSafeArrayAsInParam(args),
                                            ComSafeArrayAsInParam(env),
                                            Bstr(aTask->strUserName).raw(),
                                            Bstr(aTask->strPassword).raw(),
                                            5 * 1000 /* Wait 5s for getting the process started. */,
                                            &uPID, execProgress.asOutParam());
                if (FAILED(rc))
                    rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
            }

            if (SUCCEEDED(rc))
            {
                BOOL fCompleted = FALSE;
                BOOL fCanceled = FALSE;

                RTFILE hFileDest;
                int vrc = RTFileOpen(&hFileDest, aTask->strDest.c_str(),
                                     RTFILE_O_WRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE);
                if (RT_FAILURE(vrc))
                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                         Guest::tr("Unable to create/open destination file \"%s\", rc=%Rrc"),
                                                         aTask->strDest.c_str(), vrc);
                else
                {
                    size_t cbToRead = lFileSize;
                    size_t cbTransfered = 0;
                    SafeArray<BYTE> aOutputData(_64K);
                    while (SUCCEEDED(execProgress->COMGETTER(Completed(&fCompleted))))
                    {
                        rc = this->GetProcessOutput(uPID, ProcessOutputFlag_None,
                                                    10 * 1000 /* Timeout in ms */,
                                                    _64K, ComSafeArrayAsOutParam(aOutputData));
                        if (SUCCEEDED(rc))
                        {
                            if (!aOutputData.size())
                            {
                                /*
                                 * Only bitch about an unexpected end of a file when there already
                                 * was data read from that file. If this was the very first read we can
                                 * be (almost) sure that this file is not meant to be read by the specified user.
                                 */
                                if (   cbTransfered
                                    && cbToRead)
                                {
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Unexpected end of file \"%s\" (%u bytes left, %u bytes written)"),
                                                                         aTask->strSource.c_str(), cbToRead, cbTransfered);
                                }
                                break;
                            }

                            vrc = RTFileWrite(hFileDest, aOutputData.raw(), aOutputData.size(), NULL /* No partial writes */);
                            if (RT_FAILURE(vrc))
                            {
                                rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                     Guest::tr("Error writing to file \"%s\" (%u bytes left), rc=%Rrc"),
                                                                     aTask->strSource.c_str(), cbToRead, vrc);
                                break;
                            }

                            cbToRead -= aOutputData.size();
                            cbTransfered += aOutputData.size();

                            aTask->progress->SetCurrentOperationProgress(cbTransfered / (lFileSize / 100.0));
                        }
                        else
                        {
                            rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                            break;
                        }
                    }

                    if (SUCCEEDED(rc))
                        aTask->progress->notifyComplete(S_OK);

                    RTFileClose(hFileDest);
                }
            }
        }
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    /* Clean up */
    aTask->rc = rc;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

HRESULT Guest::taskUpdateGuestAdditions(GuestTask *aTask)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /*
     * Do *not* take a write lock here since we don't (and won't)
     * touch any class-specific data (of IGuest) here - only the member functions
     * which get called here can do that.
     */

    HRESULT rc = S_OK;
    BOOL fCompleted;
    BOOL fCanceled;

    try
    {
        Guest *pGuest = aTask->pGuest;
        AssertPtr(pGuest);

        aTask->progress->SetCurrentOperationProgress(10);

        /*
         * Determine guest OS type and the required installer image.
         * At the moment only Windows guests are supported.
         */
        Utf8Str installerImage;
        Bstr osTypeId;
        if (   SUCCEEDED(pGuest->COMGETTER(OSTypeId(osTypeId.asOutParam())))
            && !osTypeId.isEmpty())
        {
            Utf8Str osTypeIdUtf8(osTypeId); /* Needed for .contains(). */
            if (   osTypeIdUtf8.contains("Microsoft", Utf8Str::CaseInsensitive)
                || osTypeIdUtf8.contains("Windows", Utf8Str::CaseInsensitive))
            {
                if (osTypeIdUtf8.contains("64", Utf8Str::CaseInsensitive))
                    installerImage = "VBOXWINDOWSADDITIONS_AMD64.EXE";
                else
                    installerImage = "VBOXWINDOWSADDITIONS_X86.EXE";
                /* Since the installers are located in the root directory,
                 * no further path processing needs to be done (yet). */
            }
            else /* Everything else is not supported (yet). */
                throw GuestTask::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                      Guest::tr("Detected guest OS (%s) does not support automatic Guest Additions updating, please update manually"),
                                                      osTypeIdUtf8.c_str());
        }
        else
            throw GuestTask::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                  Guest::tr("Could not detected guest OS type/version, please update manually"));
        Assert(!installerImage.isEmpty());

        /*
         * Try to open the .ISO file and locate the specified installer.
         */
        RTISOFSFILE iso;
        int vrc = RTIsoFsOpen(&iso, aTask->strSource.c_str());
        if (RT_FAILURE(vrc))
        {
            rc = GuestTask::setProgressErrorInfo(VBOX_E_FILE_ERROR, aTask->progress,
                                                 Guest::tr("Invalid installation medium detected: \"%s\""),
                                                 aTask->strSource.c_str());
        }
        else
        {
            uint32_t cbOffset;
            size_t cbLength;
            vrc = RTIsoFsGetFileInfo(&iso, installerImage.c_str(), &cbOffset, &cbLength);
            if (   RT_SUCCESS(vrc)
                && cbOffset
                && cbLength)
            {
                vrc = RTFileSeek(iso.file, cbOffset, RTFILE_SEEK_BEGIN, NULL);
                if (RT_FAILURE(vrc))
                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                         Guest::tr("Could not seek to setup file on installation medium \"%s\" (%Rrc)"),
                                                         aTask->strSource.c_str(), vrc);
            }
            else
            {
                switch (vrc)
                {
                    case VERR_FILE_NOT_FOUND:
                        rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                             Guest::tr("Setup file was not found on installation medium \"%s\""),
                                                             aTask->strSource.c_str());
                        break;

                    default:
                        rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                             Guest::tr("An unknown error (%Rrc) occured while retrieving information of setup file on installation medium \"%s\""),
                                                             vrc, aTask->strSource.c_str());
                        break;
                }
            }

            /* Specify the ouput path on the guest side. */
            Utf8Str strInstallerPath = "%TEMP%\\VBoxWindowsAdditions.exe";

            if (RT_SUCCESS(vrc))
            {
                /* Okay, we're ready to start our copy routine on the guest! */
                aTask->progress->SetCurrentOperationProgress(15);

                /* Prepare command line args. */
                com::SafeArray<IN_BSTR> args;
                com::SafeArray<IN_BSTR> env;

                args.push_back(Bstr("--output").raw());               /* We want to write a file ... */
                args.push_back(Bstr(strInstallerPath.c_str()).raw()); /* ... with this path. */

                if (SUCCEEDED(rc))
                {
                    ComPtr<IProgress> progressCat;
                    ULONG uPID;

                    /*
                     * Start built-in "vbox_cat" tool (inside VBoxService) to
                     * copy over/pipe the data into a file on the guest (with
                     * system rights, no username/password specified).
                     */
                    rc = pGuest->executeProcessInternal(Bstr(VBOXSERVICE_TOOL_CAT).raw(),
                                                          ExecuteProcessFlag_Hidden
                                                        | ExecuteProcessFlag_WaitForProcessStartOnly,
                                                        ComSafeArrayAsInParam(args),
                                                        ComSafeArrayAsInParam(env),
                                                        Bstr("").raw() /* Username. */,
                                                        Bstr("").raw() /* Password */,
                                                        5 * 1000 /* Wait 5s for getting the process started. */,
                                                        &uPID, progressCat.asOutParam(), &vrc);
                    if (FAILED(rc))
                    {
                        /* Errors which return VBOX_E_NOT_SUPPORTED can be safely skipped by the caller
                         * to silently fall back to "normal" (old) .ISO mounting. */

                        /* Due to a very limited COM error range we use vrc for a more detailed error
                         * lookup to figure out what went wrong. */
                        switch (vrc)
                        {
                            /* Guest execution service is not (yet) ready. This basically means that either VBoxService
                             * is not running (yet) or that the Guest Additions are too old (because VBoxService does not
                             * support the guest execution feature in this version). */
                            case VERR_NOT_FOUND:
                                LogRel(("Guest Additions seem not to be installed yet\n"));
                                rc = GuestTask::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                                     Guest::tr("Guest Additions seem not to be installed or are not ready to update yet"));
                                break;

                            /* Getting back a VERR_INVALID_PARAMETER indicates that the installed Guest Additions are supporting the guest
                             * execution but not the built-in "vbox_cat" tool of VBoxService (< 4.0). */
                            case VERR_INVALID_PARAMETER:
                                LogRel(("Guest Additions are installed but don't supported automatic updating\n"));
                                rc = GuestTask::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                                     Guest::tr("Installed Guest Additions do not support automatic updating"));
                                break;

                            case VERR_TIMEOUT:
                                LogRel(("Guest was unable to start copying the Guest Additions setup within time\n"));
                                rc = GuestTask::setProgressErrorInfo(E_FAIL, aTask->progress,
                                                                     Guest::tr("Guest was unable to start copying the Guest Additions setup within time"));
                                break;

                            default:
                                rc = GuestTask::setProgressErrorInfo(E_FAIL, aTask->progress,
                                                                     Guest::tr("Error copying Guest Additions setup file to guest path \"%s\" (%Rrc)"),
                                                                     strInstallerPath.c_str(), vrc);
                                break;
                        }
                    }
                    else
                    {
                        LogRel(("Automatic update of Guest Additions started, using \"%s\"\n", aTask->strSource.c_str()));
                        LogRel(("Copying Guest Additions installer \"%s\" to \"%s\" on guest ...\n",
                                installerImage.c_str(), strInstallerPath.c_str()));
                        aTask->progress->SetCurrentOperationProgress(20);

                        /* Wait for process to exit ... */
                        SafeArray<BYTE> aInputData(_64K);
                        while (   SUCCEEDED(progressCat->COMGETTER(Completed(&fCompleted)))
                               && !fCompleted)
                        {
                            size_t cbRead;
                            /* cbLength contains remaining bytes of our installer file
                             * opened above to read. */
                            size_t cbToRead = RT_MIN(cbLength, _64K);
                            if (cbToRead)
                            {
                                vrc = RTFileRead(iso.file, (uint8_t*)aInputData.raw(), cbToRead, &cbRead);
                                if (   cbRead
                                    && RT_SUCCESS(vrc))
                                {
                                    /* Resize buffer to reflect amount we just have read. */
                                    if (cbRead > 0)
                                        aInputData.resize(cbRead);

                                    /* Did we reach the end of the content we want to transfer (last chunk)? */
                                    ULONG uFlags = ProcessInputFlag_None;
                                    if (   (cbRead < _64K)
                                        /* Did we reach the last block which is exactly _64K? */
                                        || (cbToRead - cbRead == 0)
                                        /* ... or does the user want to cancel? */
                                        || (   SUCCEEDED(aTask->progress->COMGETTER(Canceled(&fCanceled)))
                                            && fCanceled)
                                       )
                                    {
                                        uFlags |= ProcessInputFlag_EndOfFile;
                                    }

                                    /* Transfer the current chunk ... */
                                #ifdef DEBUG_andy
                                    LogRel(("Copying Guest Additions (%u bytes left) ...\n", cbLength));
                                #endif
                                    ULONG uBytesWritten;
                                    rc = pGuest->SetProcessInput(uPID, uFlags,
                                                                 10 * 1000 /* Wait 10s for getting the input data transfered. */,
                                                                 ComSafeArrayAsInParam(aInputData), &uBytesWritten);
                                    if (FAILED(rc))
                                    {
                                        rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                                        break;
                                    }

                                    /* If task was canceled above also cancel the process execution. */
                                    if (fCanceled)
                                        progressCat->Cancel();

                                #ifdef DEBUG_andy
                                    LogRel(("Copying Guest Additions (%u bytes written) ...\n", uBytesWritten));
                                #endif
                                    Assert(cbLength >= uBytesWritten);
                                    cbLength -= uBytesWritten;
                                }
                                else if (RT_FAILURE(vrc))
                                {
                                    rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Error while reading setup file \"%s\" (To read: %u, Size: %u) from installation medium (%Rrc)"),
                                                                         installerImage.c_str(), cbToRead, cbLength, vrc);
                                }
                            }

                            /* Internal progress canceled? */
                            if (   SUCCEEDED(progressCat->COMGETTER(Canceled(&fCanceled)))
                                && fCanceled)
                            {
                                aTask->progress->Cancel();
                                break;
                            }
                        }
                    }
                }
            }
            RTIsoFsClose(&iso);

            if (   SUCCEEDED(rc)
                && (   SUCCEEDED(aTask->progress->COMGETTER(Canceled(&fCanceled)))
                    && !fCanceled
                   )
               )
            {
                /*
                 * Installer was transferred successfully, so let's start it
                 * (with system rights).
                 */
                LogRel(("Preparing to execute Guest Additions update ...\n"));
                aTask->progress->SetCurrentOperationProgress(66);

                /* Prepare command line args for installer. */
                com::SafeArray<IN_BSTR> installerArgs;
                com::SafeArray<IN_BSTR> installerEnv;

                /** @todo Only Windows! */
                installerArgs.push_back(Bstr(strInstallerPath).raw()); /* The actual (internal) installer image (as argv[0]). */
                /* Note that starting at Windows Vista the lovely session 0 separation applies:
                 * This means that if we run an application with the profile/security context
                 * of VBoxService (system rights!) we're not able to show any UI. */
                installerArgs.push_back(Bstr("/S").raw());      /* We want to install in silent mode. */
                installerArgs.push_back(Bstr("/l").raw());      /* ... and logging enabled. */
                /* Don't quit VBoxService during upgrade because it still is used for this
                 * piece of code we're in right now (that is, here!) ... */
                installerArgs.push_back(Bstr("/no_vboxservice_exit").raw());
                /* Tell the installer to report its current installation status
                 * using a running VBoxTray instance via balloon messages in the
                 * Windows taskbar. */
                installerArgs.push_back(Bstr("/post_installstatus").raw());

                /*
                 * Start the just copied over installer with system rights
                 * in silent mode on the guest. Don't use the hidden flag since there
                 * may be pop ups the user has to process.
                 */
                ComPtr<IProgress> progressInstaller;
                ULONG uPID;
                rc = pGuest->executeProcessInternal(Bstr(strInstallerPath).raw(),
                                                    ExecuteProcessFlag_WaitForProcessStartOnly,
                                                    ComSafeArrayAsInParam(installerArgs),
                                                    ComSafeArrayAsInParam(installerEnv),
                                                    Bstr("").raw() /* Username */,
                                                    Bstr("").raw() /* Password */,
                                                    10 * 1000 /* Wait 10s for getting the process started */,
                                                    &uPID, progressInstaller.asOutParam(), &vrc);
                if (SUCCEEDED(rc))
                {
                    LogRel(("Guest Additions update is running ...\n"));

                    /* If the caller does not want to wait for out guest update process to end,
                     * complete the progress object now so that the caller can do other work. */
                    if (aTask->uFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly)
                        aTask->progress->notifyComplete(S_OK);
                    else
                        aTask->progress->SetCurrentOperationProgress(70);

                    /* Wait until the Guest Additions installer finishes ... */
                    while (   SUCCEEDED(progressInstaller->COMGETTER(Completed(&fCompleted)))
                           && !fCompleted)
                    {
                        if (   SUCCEEDED(aTask->progress->COMGETTER(Canceled(&fCanceled)))
                            && fCanceled)
                        {
                            progressInstaller->Cancel();
                            break;
                        }
                        /* Progress canceled by Main API? */
                        if (   SUCCEEDED(progressInstaller->COMGETTER(Canceled(&fCanceled)))
                            && fCanceled)
                        {
                            break;
                        }
                        RTThreadSleep(100);
                    }

                    ExecuteProcessStatus_T retStatus;
                    ULONG uRetExitCode, uRetFlags;
                    rc = pGuest->GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                    if (SUCCEEDED(rc))
                    {
                        if (fCompleted)
                        {
                            if (uRetExitCode == 0)
                            {
                                LogRel(("Guest Additions update successful!\n"));
                                if (   SUCCEEDED(aTask->progress->COMGETTER(Completed(&fCompleted)))
                                    && !fCompleted)
                                    aTask->progress->notifyComplete(S_OK);
                            }
                            else
                            {
                                LogRel(("Guest Additions update failed (Exit code=%u, Status=%u, Flags=%u)\n",
                                        uRetExitCode, retStatus, uRetFlags));
                                rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                     Guest::tr("Guest Additions update failed with exit code=%u (status=%u, flags=%u)"),
                                                                     uRetExitCode, retStatus, uRetFlags);
                            }
                        }
                        else if (   SUCCEEDED(progressInstaller->COMGETTER(Canceled(&fCanceled)))
                                 && fCanceled)
                        {
                            LogRel(("Guest Additions update was canceled\n"));
                            rc = GuestTask::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                 Guest::tr("Guest Additions update was canceled by the guest with exit code=%u (status=%u, flags=%u)"),
                                                                 uRetExitCode, retStatus, uRetFlags);
                        }
                        else
                        {
                            LogRel(("Guest Additions update was canceled by the user\n"));
                        }
                    }
                    else
                        rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
                }
                else
                    rc = GuestTask::setProgressErrorInfo(rc, aTask->progress, pGuest);
            }
        }
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    /* Clean up */
    aTask->rc = rc;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}
#endif

