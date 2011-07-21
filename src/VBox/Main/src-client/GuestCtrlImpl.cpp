/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Guest
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

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
#include <iprt/cpp/utils.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/isofs.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <VBox/vmm/pgm.h>

#include <memory>

struct Guest::TaskGuest
{
    enum TaskType
    {
        /** Copies a file from host to the guest. */
        CopyFileToGuest   = 50,
        /** Copies a file from guest to the host. */
        CopyFileFromGuest = 55,

        /** Update Guest Additions by directly copying the required installer
         *  off the .ISO file, transfer it to the guest and execute the installer
         *  with system privileges. */
        UpdateGuestAdditions = 100
    };

    TaskGuest(TaskType aTaskType, Guest *aThat, Progress *aProgress)
        : taskType(aTaskType),
          pGuest(aThat),
          progress(aProgress),
          rc(S_OK)
    {}
    virtual ~TaskGuest() {}

    int startThread();
    static int taskThread(RTTHREAD aThread, void *pvUser);
    static int uploadProgress(unsigned uPercent, void *pvUser);

    static HRESULT setProgressErrorInfo(HRESULT hr, ComObjPtr<Progress> pProgress, const char * pszText, ...);
    static HRESULT setProgressErrorInfo(HRESULT hr, ComObjPtr<Progress> pProgress, ComObjPtr<Guest> pGuest);

    TaskType taskType;
    Guest *pGuest;
    ComObjPtr<Progress> progress;
    HRESULT rc;

    /* Task data. */
    Utf8Str strSource;
    Utf8Str strDest;
    Utf8Str strUserName;
    Utf8Str strPassword;
    ULONG   uFlags;
};

int Guest::TaskGuest::startThread()
{
    int vrc = RTThreadCreate(NULL, Guest::TaskGuest::taskThread, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Guest::Task");

    if (RT_FAILURE(vrc))
        return Guest::setErrorStatic(E_FAIL, Utf8StrFmt("Could not create taskThreadGuest (%Rrc)\n", vrc));

    return vrc;
}

/* static */
DECLCALLBACK(int) Guest::TaskGuest::taskThread(RTTHREAD /* aThread */, void *pvUser)
{
    std::auto_ptr<TaskGuest> task(static_cast<TaskGuest*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Guest *pGuest = task->pGuest;

    LogFlowFuncEnter();
    LogFlowFunc(("Guest %p\n", pGuest));

    HRESULT rc = S_OK;

    switch (task->taskType)
    {
#ifdef VBOX_WITH_GUEST_CONTROL
        case TaskGuest::CopyFileToGuest:
        {
            rc = pGuest->taskCopyFileToGuest(task.get());
            break;
        }
        case TaskGuest::CopyFileFromGuest:
        {
            rc = pGuest->taskCopyFileFromGuest(task.get());
            break;
        }
        case TaskGuest::UpdateGuestAdditions:
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
int Guest::TaskGuest::uploadProgress(unsigned uPercent, void *pvUser)
{
    Guest::TaskGuest *pTask = *(Guest::TaskGuest**)pvUser;

    if (pTask &&
        !pTask->progress.isNull())
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
HRESULT Guest::TaskGuest::setProgressErrorInfo(HRESULT hr, ComObjPtr<Progress> pProgress, const char *pszText, ...)
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
HRESULT Guest::TaskGuest::setProgressErrorInfo(HRESULT hr, ComObjPtr<Progress> pProgress, ComObjPtr<Guest> pGuest)
{
    return setProgressErrorInfo(hr, pProgress,
                                Utf8Str(com::ErrorInfo((IGuest*)pGuest, COM_IIDOF(IGuest)).getText()).c_str());
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::taskCopyFileToGuest(TaskGuest *aTask)
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
            rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                     Guest::tr("Could not open source file \"%s\" for reading (%Rrc)"),
                                                     aTask->strSource.c_str(),  vrc);
            }
            else
            {
                uint64_t cbSize;
                vrc = RTFileGetSize(fileSource, &cbSize);
                if (RT_FAILURE(vrc))
                {
                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                        rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                            rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
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
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                                rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
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
                                rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                            rc = pGuest->waitForProcessStatusChange(uPID, &retStatus, &uRetExitCode, 10 * 1000 /* 10s timeout. */);
                            if (FAILED(rc))
                            {
                                rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
                            }
                            else
                            {
                                if (   uRetExitCode != 0
                                    || retStatus    != ExecuteProcessStatus_TerminatedNormally)
                                {
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Access denied when copying file \"%s\" to \"%s\""),
                                                                         aTask->strSource.c_str(), aTask->strDest.c_str());
                                }
                                else if (cbTransfered < cbSize)
                                {
                                    /* If we did not copy all let the user know. */
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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

HRESULT Guest::taskCopyFileFromGuest(TaskGuest *aTask)
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
                rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);

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
                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                    rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
            }

            if (SUCCEEDED(rc))
            {
                BOOL fCompleted = FALSE;
                BOOL fCanceled = FALSE;

                RTFILE hFileDest;
                int vrc = RTFileOpen(&hFileDest, aTask->strDest.c_str(),
                                     RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE);
                if (RT_FAILURE(vrc))
                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                                if (cbToRead)
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                         Guest::tr("Unexpected end of file \"%s\" (%u bytes left)"),
                                                                         aTask->strSource.c_str(), cbToRead);
                                break;
                            }

                            vrc = RTFileWrite(hFileDest, aOutputData.raw(), aOutputData.size(), NULL /* No partial writes */);
                            if (RT_FAILURE(vrc))
                            {
                                rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                            rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
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

HRESULT Guest::taskUpdateGuestAdditions(TaskGuest *aTask)
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
                throw TaskGuest::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                      Guest::tr("Detected guest OS (%s) does not support automatic Guest Additions updating, please update manually"),
                                                      osTypeIdUtf8.c_str());
        }
        else
            throw TaskGuest::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                  Guest::tr("Could not detected guest OS type/version, please update manually"));
        Assert(!installerImage.isEmpty());

        /*
         * Try to open the .ISO file and locate the specified installer.
         */
        RTISOFSFILE iso;
        int vrc = RTIsoFsOpen(&iso, aTask->strSource.c_str());
        if (RT_FAILURE(vrc))
        {
            rc = TaskGuest::setProgressErrorInfo(VBOX_E_FILE_ERROR, aTask->progress,
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
                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                         Guest::tr("Could not seek to setup file on installation medium \"%s\" (%Rrc)"),
                                                         aTask->strSource.c_str(), vrc);
            }
            else
            {
                switch (vrc)
                {
                    case VERR_FILE_NOT_FOUND:
                        rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                             Guest::tr("Setup file was not found on installation medium \"%s\""),
                                                             aTask->strSource.c_str());
                        break;

                    default:
                        rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                                rc = TaskGuest::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                                     Guest::tr("Guest Additions seem not to be installed or are not ready to update yet"));
                                break;

                            /* Getting back a VERR_INVALID_PARAMETER indicates that the installed Guest Additions are supporting the guest
                             * execution but not the built-in "vbox_cat" tool of VBoxService (< 4.0). */
                            case VERR_INVALID_PARAMETER:
                                LogRel(("Guest Additions are installed but don't supported automatic updating\n"));
                                rc = TaskGuest::setProgressErrorInfo(VBOX_E_NOT_SUPPORTED, aTask->progress,
                                                                     Guest::tr("Installed Guest Additions do not support automatic updating"));
                                break;

                            case VERR_TIMEOUT:
                                LogRel(("Guest was unable to start copying the Guest Additions setup within time\n"));
                                rc = TaskGuest::setProgressErrorInfo(E_FAIL, aTask->progress,
                                                                     Guest::tr("Guest was unable to start copying the Guest Additions setup within time"));
                                break;

                            default:
                                rc = TaskGuest::setProgressErrorInfo(E_FAIL, aTask->progress,
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
                                        rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
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
                                    rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
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
                                rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                     Guest::tr("Guest Additions update failed with exit code=%u (status=%u, flags=%u)"),
                                                                     uRetExitCode, retStatus, uRetFlags);
                            }
                        }
                        else if (   SUCCEEDED(progressInstaller->COMGETTER(Canceled(&fCanceled)))
                                 && fCanceled)
                        {
                            LogRel(("Guest Additions update was canceled\n"));
                            rc = TaskGuest::setProgressErrorInfo(VBOX_E_IPRT_ERROR, aTask->progress,
                                                                 Guest::tr("Guest Additions update was canceled by the guest with exit code=%u (status=%u, flags=%u)"),
                                                                 uRetExitCode, retStatus, uRetFlags);
                        }
                        else
                        {
                            LogRel(("Guest Additions update was canceled by the user\n"));
                        }
                    }
                    else
                        rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
                }
                else
                    rc = TaskGuest::setProgressErrorInfo(rc, aTask->progress, pGuest);
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

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_WITH_GUEST_CONTROL
/**
 * Appends environment variables to the environment block.
 *
 * Each var=value pair is separated by the null character ('\\0').  The whole
 * block will be stored in one blob and disassembled on the guest side later to
 * fit into the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @param   pszEnvVar       The environment variable=value to append to the
 *                          environment block.
 * @param   ppvList         This is actually a pointer to a char pointer
 *                          variable which keeps track of the environment block
 *                          that we're constructing.
 * @param   pcbList         Pointer to the variable holding the current size of
 *                          the environment block.  (List is a misnomer, go
 *                          ahead a be confused.)
 * @param   pcEnvVars       Pointer to the variable holding count of variables
 *                          stored in the environment block.
 */
int Guest::prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnvVars)
{
    int rc = VINF_SUCCESS;
    uint32_t cchEnv = strlen(pszEnv); Assert(cchEnv >= 2);
    if (*ppvList)
    {
        uint32_t cbNewLen = *pcbList + cchEnv + 1; /* Include zero termination. */
        char *pvTmp = (char *)RTMemRealloc(*ppvList, cbNewLen);
        if (pvTmp == NULL)
            rc = VERR_NO_MEMORY;
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cchEnv);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void **)pvTmp;
        }
    }
    else
    {
        char *pszTmp;
        if (RTStrAPrintf(&pszTmp, "%s", pszEnv) >= 0)
        {
            *ppvList = (void **)pszTmp;
            /* Reset counters. */
            *pcEnvVars = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += cchEnv + 1; /* Include zero termination. */
        *pcEnvVars += 1;        /* Increase env variable count. */
    }
    return rc;
}

/**
 * Adds a callback with a user provided data block and an optional progress object
 * to the callback map. A callback is identified by a unique context ID which is used
 * to identify a callback from the guest side.
 *
 * @return  IPRT status code.
 * @param   puContextID
 * @param   pCallbackData
 */
int Guest::callbackAdd(const PVBOXGUESTCTRL_CALLBACK pCallbackData, uint32_t *puContextID)
{
    AssertPtrReturn(pCallbackData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puContextID, VERR_INVALID_PARAMETER);

    int rc;

    /* Create a new context ID and assign it. */
    uint32_t uNewContextID = 0;
    for (;;)
    {
        /* Create a new context ID ... */
        uNewContextID = ASMAtomicIncU32(&mNextContextID);
        if (uNewContextID == UINT32_MAX)
            ASMAtomicUoWriteU32(&mNextContextID, 1000);
        /* Is the context ID already used?  Try next ID ... */
        if (!callbackExists(uNewContextID))
        {
            /* Callback with context ID was not found. This means
             * we can use this context ID for our new callback we want
             * to add below. */
            rc = VINF_SUCCESS;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Add callback with new context ID to our callback map. */
        mCallbackMap[uNewContextID] = *pCallbackData;
        Assert(mCallbackMap.size());

        /* Report back new context ID. */
        *puContextID = uNewContextID;
    }

    return rc;
}

/**
 * Does not do locking!
 *
 * @param   uContextID
 */
void Guest::callbackDestroy(uint32_t uContextID)
{
    AssertReturnVoid(uContextID);

    LogFlowFunc(("Destroying callback with CID=%u ...\n", uContextID));

    /* Notify callback (if necessary). */
    int rc = callbackNotifyEx(uContextID, VERR_CANCELLED,
                              Guest::tr("VM is shutting down, canceling uncompleted guest requests ..."));
    AssertRC(rc);

    CallbackMapIter it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (it->second.pvData)
        {
            callbackFreeUserData(it->second.pvData);
            it->second.cbData = 0;
        }

        /* Remove callback context (not used anymore). */
        mCallbackMap.erase(it);
    }
}

bool Guest::callbackExists(uint32_t uContextID)
{
    AssertReturn(uContextID, false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIter it = mCallbackMap.find(uContextID);
    return (it == mCallbackMap.end()) ? false : true;
}

void Guest::callbackFreeUserData(void *pvData)
{
    if (pvData)
    {
        RTMemFree(pvData);
        pvData = NULL;
    }
}

int Guest::callbackGetUserData(uint32_t uContextID, eVBoxGuestCtrlCallbackType *pEnmType,
                               void **ppvData, size_t *pcbData)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);
    /* pEnmType is optional. */
    AssertPtrReturn(ppvData, VERR_INVALID_PARAMETER);
    /* pcbData is optional. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIterConst it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (pEnmType)
            *pEnmType = it->second.mType;

        void *pvData = RTMemAlloc(it->second.cbData);
        AssertPtrReturn(pvData, VERR_NO_MEMORY);
        memcpy(pvData, it->second.pvData, it->second.cbData);
        *ppvData = pvData;

        if (pcbData)
            *pcbData = it->second.cbData;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

/* Does not do locking! Caller has to take care of it because the caller needs to
 * modify the data ...*/
void* Guest::callbackGetUserDataMutableRaw(uint32_t uContextID, size_t *pcbData)
{
    AssertReturn(uContextID, NULL);
    /* pcbData is optional. */

    CallbackMapIterConst it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (pcbData)
            *pcbData = it->second.cbData;
        return it->second.pvData;
    }

    return NULL;
}

bool Guest::callbackIsCanceled(uint32_t uContextID)
{
    AssertReturn(uContextID, true);

    Progress *pProgress = NULL;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        BOOL fCanceled = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Canceled)(&fCanceled);
        if (   SUCCEEDED(hRC)
            && !fCanceled)
        {
            return false;
        }
    }

    return true; /* No progress / error means canceled. */
}

bool Guest::callbackIsComplete(uint32_t uContextID)
{
    AssertReturn(uContextID, true);

    Progress *pProgress = NULL;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        BOOL fCompleted = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Completed)(&fCompleted);
        if (   SUCCEEDED(hRC)
            && fCompleted)
        {
            return true;
        }
    }

    return false;
}

int Guest::callbackMoveForward(uint32_t uContextID, const char *pszMessage)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszMessage, VERR_INVALID_PARAMETER);

    Progress *pProgress = NULL;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        HRESULT hr = pProgress->SetNextOperation(Bstr(pszMessage).raw(), 1 /* Weight */);
        if (FAILED(hr))
            return VERR_CANCELLED;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

/**
 * TODO
 *
 * @return  IPRT status code.
 * @param   uContextID
 * @param   iRC
 * @param   pszMessage
 */
int Guest::callbackNotifyEx(uint32_t uContextID, int iRC, const char *pszMessage)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Notifying callback with CID=%u, iRC=%d, pszMsg=%s ...\n",
                 uContextID, iRC, pszMessage ? pszMessage : "<No message given>"));

    Progress *pProgress = NULL;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

#if 0
    BOOL fCanceled = FALSE;
    HRESULT hRC = pProgress->COMGETTER(Canceled)(&fCanceled);
    if (   SUCCEEDED(hRC)
        && fCanceled)
    {
        /* If progress already canceled do nothing here. */
        return VINF_SUCCESS;
    }
#endif

    if (pProgress)
    {
        /*
         * Assume we didn't complete to make sure we clean up even if the
         * following call fails.
         */
        BOOL fCompleted = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Completed)(&fCompleted);
        if (   SUCCEEDED(hRC)
            && !fCompleted)
        {
            /*
             * To get waitForCompletion completed (unblocked) we have to notify it if necessary (only
             * cancel won't work!). This could happen if the client thread (e.g. VBoxService, thread of a spawned process)
             * is disconnecting without having the chance to sending a status message before, so we
             * have to abort here to make sure the host never hangs/gets stuck while waiting for the
             * progress object to become signalled.
             */
            if (   RT_SUCCESS(iRC)
                && !pszMessage)
            {
                hRC = pProgress->notifyComplete(S_OK);
            }
            else
            {
                AssertPtrReturn(pszMessage, VERR_INVALID_PARAMETER);
                hRC = pProgress->notifyComplete(VBOX_E_IPRT_ERROR /* Must not be S_OK. */,
                                                COM_IIDOF(IGuest),
                                                Guest::getStaticComponentName(),
                                                pszMessage);
            }
        }
        ComAssertComRC(hRC);

        /*
         * Do *not* NULL pProgress here, because waiting function like executeProcess()
         * will still rely on this object for checking whether they have to give up!
         */
    }
    /* If pProgress is not found (anymore) that's fine.
     * Might be destroyed already. */
    return S_OK;
}

/**
 * TODO
 *
 * @return  IPRT status code.
 * @param   uPID
 * @param   iRC
 * @param   pszMessage
 */
int Guest::callbackNotifyAllForPID(uint32_t uPID, int iRC, const char *pszMessage)
{
    AssertReturn(uPID, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIter it;
    for (it = mCallbackMap.begin(); it != mCallbackMap.end(); it++)
    {
        switch (it->second.mType)
        {
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_START:
                break;

            /* When waiting for process output while the process is destroyed,
             * make sure we also destroy the actual waiting operation (internal progress object)
             * in order to not block the caller. */
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT:
            {
                PCALLBACKDATAEXECOUT pItData = (PCALLBACKDATAEXECOUT)it->second.pvData;
                AssertPtr(pItData);
                if (pItData->u32PID == uPID)
                    vrc = callbackNotifyEx(it->first, iRC, pszMessage);
                break;
            }

            /* When waiting for injecting process input while the process is destroyed,
             * make sure we also destroy the actual waiting operation (internal progress object)
             * in order to not block the caller. */
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS:
            {
                PCALLBACKDATAEXECINSTATUS pItData = (PCALLBACKDATAEXECINSTATUS)it->second.pvData;
                AssertPtr(pItData);
                if (pItData->u32PID == uPID)
                    vrc = callbackNotifyEx(it->first, iRC, pszMessage);
                break;
            }

            default:
                vrc = VERR_INVALID_PARAMETER;
                AssertMsgFailed(("Unknown callback type %d, iRC=%d, message=%s\n",
                                 it->second.mType, iRC, pszMessage ? pszMessage : "<No message given>"));
                break;
        }

        if (RT_FAILURE(vrc))
            break;
    }

    return vrc;
}

int Guest::callbackNotifyComplete(uint32_t uContextID)
{
    return callbackNotifyEx(uContextID, S_OK, NULL /* No message */);
}

int Guest::callbackWaitForCompletion(uint32_t uContextID, LONG lStage, LONG lTimeout)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);

    /*
     * Wait for the HGCM low level callback until the process
     * has been started (or something went wrong). This is necessary to
     * get the PID.
     */

    int vrc = VINF_SUCCESS;
    Progress *pProgress = NULL;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
        else
            vrc = VERR_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
    {
        HRESULT rc;
        if (lStage < 0)
            rc = pProgress->WaitForCompletion(lTimeout);
        else
            rc = pProgress->WaitForOperationCompletion((ULONG)lStage, lTimeout);
        if (SUCCEEDED(rc))
        {
            if (!callbackIsComplete(uContextID))
                vrc = callbackIsCanceled(uContextID)
                    ? VERR_CANCELLED : VINF_SUCCESS;
        }
        else
            vrc = VERR_TIMEOUT;
    }

    return vrc;
}

/**
 * Static callback function for receiving updates on guest control commands
 * from the guest. Acts as a dispatcher for the actual class instance.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 */
DECLCALLBACK(int) Guest::notifyCtrlDispatcher(void    *pvExtension,
                                              uint32_t u32Function,
                                              void    *pvParms,
                                              uint32_t cbParms)
{
    using namespace guestControl;

    /*
     * No locking, as this is purely a notification which does not make any
     * changes to the object state.
     */
#ifdef DEBUG_andy
    LogFlowFunc(("pvExtension = %p, u32Function = %d, pvParms = %p, cbParms = %d\n",
                 pvExtension, u32Function, pvParms, cbParms));
#endif
    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);

    int rc = VINF_SUCCESS;
    switch (u32Function)
    {
        case GUEST_DISCONNECTED:
        {
            //LogFlowFunc(("GUEST_DISCONNECTED\n"));

            PCALLBACKDATACLIENTDISCONNECTED pCBData = reinterpret_cast<PCALLBACKDATACLIENTDISCONNECTED>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATACLIENTDISCONNECTED) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_CLIENT_DISCONNECTED == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlClientDisconnected(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_STATUS\n"));

            PCALLBACKDATAEXECSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_STATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecStatus(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_OUTPUT:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_OUTPUT\n"));

            PCALLBACKDATAEXECOUT pCBData = reinterpret_cast<PCALLBACKDATAEXECOUT>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECOUT) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_OUT == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecOut(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_INPUT_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_INPUT_STATUS\n"));

            PCALLBACKDATAEXECINSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECINSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECINSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_IN_STATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecInStatus(u32Function, pCBData);
            break;
        }

        default:
            AssertMsgFailed(("Unknown guest control notification received, u32Function=%u\n", u32Function));
            rc = VERR_INVALID_PARAMETER;
            break;
    }
    return rc;
}

/* Function for handling the execution start/termination notification. */
/* Callback can be called several times. */
int Guest::notifyCtrlExecStatus(uint32_t                u32Function,
                                PCALLBACKDATAEXECSTATUS pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        PCALLBACKDATAEXECSTATUS pCallbackData =
            (PCALLBACKDATAEXECSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            pCallbackData->u32PID = pData->u32PID;
            pCallbackData->u32Status = pData->u32Status;
            pCallbackData->u32Flags = pData->u32Flags;
            /** @todo Copy void* buffer contents? */
        }
        else
            AssertReleaseMsgFailed(("Process status (PID=%u) does not have allocated callback data!\n",
                                    pData->u32PID));
    }

    int vrc = VINF_SUCCESS;
    Utf8Str errMsg;

    /* Was progress canceled before? */
    bool fCbCanceled = callbackIsCanceled(uContextID);
    if (!fCbCanceled)
    {
        /* Do progress handling. */
        switch (pData->u32Status)
        {
            case PROC_STS_STARTED:
                vrc = callbackMoveForward(uContextID, Guest::tr("Waiting for process to exit ..."));
                LogRel(("Guest process (PID %u) started\n", pData->u32PID)); /** @todo Add process name */
                break;

            case PROC_STS_TEN: /* Terminated normally. */
                vrc = callbackNotifyComplete(uContextID);
                LogRel(("Guest process (PID %u) exited normally\n", pData->u32PID)); /** @todo Add process name */
                break;

            case PROC_STS_TEA: /* Terminated abnormally. */
                LogRel(("Guest process (PID %u) terminated abnormally with exit code = %u\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated abnormally with status '%u'"),
                                    pData->u32Flags);
                break;

            case PROC_STS_TES: /* Terminated through signal. */
                LogRel(("Guest process (PID %u) terminated through signal with exit code = %u\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated via signal with status '%u'"),
                                    pData->u32Flags);
                break;

            case PROC_STS_TOK:
                LogRel(("Guest process (PID %u) timed out and was killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and was killed"));
                break;

            case PROC_STS_TOA:
                LogRel(("Guest process (PID %u) timed out and could not be killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and could not be killed"));
                break;

            case PROC_STS_DWN:
                LogRel(("Guest process (PID %u) killed because system is shutting down\n", pData->u32PID)); /** @todo Add process name */
                /*
                 * If u32Flags has ExecuteProcessFlag_IgnoreOrphanedProcesses set, we don't report an error to
                 * our progress object. This is helpful for waiters which rely on the success of our progress object
                 * even if the executed process was killed because the system/VBoxService is shutting down.
                 *
                 * In this case u32Flags contains the actual execution flags reached in via Guest::ExecuteProcess().
                 */
                if (pData->u32Flags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
                {
                    vrc = callbackNotifyComplete(uContextID);
                }
                else
                    errMsg = Utf8StrFmt(Guest::tr("Process killed because system is shutting down"));
                break;

            case PROC_STS_ERROR:
                LogRel(("Guest process (PID %u) could not be started because of rc=%Rrc\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process execution failed with rc=%Rrc"), pData->u32Flags);
                break;

            default:
                vrc = VERR_INVALID_PARAMETER;
                break;
        }

        /* Handle process map. */
        /** @todo What happens on/deal with PID reuse? */
        /** @todo How to deal with multiple updates at once? */
        if (pData->u32PID)
        {
            VBOXGUESTCTRL_PROCESS process;
            vrc = processGetByPID(pData->u32PID, &process);
            if (vrc == VERR_NOT_FOUND)
            {
                /* Not found, add to map. */
                vrc = processAdd(pData->u32PID,
                                 (ExecuteProcessStatus_T)pData->u32Status,
                                 pData->u32Flags /* Contains exit code. */,
                                 0 /*Flags. */);
                AssertRC(vrc);
            }
            else if (RT_SUCCESS(vrc))
            {
                /* Process found, update process map. */
                vrc = processSetStatus(pData->u32PID,
                                       (ExecuteProcessStatus_T)pData->u32Status,
                                       pData->u32Flags /* Contains exit code. */,
                                       0 /*Flags. */);
                AssertRC(vrc);
            }
            else
                AssertReleaseMsgFailed(("Process was neither found nor absent!?\n"));
        }
    }
    else
        errMsg = Utf8StrFmt(Guest::tr("Process execution canceled"));

    if (!callbackIsComplete(uContextID))
    {
        if (   errMsg.length()
            || fCbCanceled) /* If canceled we have to report E_FAIL! */
        {
            /* Notify all callbacks which are still waiting on something
             * which is related to the current PID. */
            if (pData->u32PID)
            {
                vrc = callbackNotifyAllForPID(pData->u32PID, VERR_GENERAL_FAILURE, errMsg.c_str());
                if (RT_FAILURE(vrc))
                    LogFlowFunc(("Failed to notify other callbacks for PID=%u\n",
                                 pData->u32PID));
            }

            /* Let the caller know what went wrong ... */
            int rc2 = callbackNotifyEx(uContextID, VERR_GENERAL_FAILURE, errMsg.c_str());
            if (RT_FAILURE(rc2))
            {
                LogFlowFunc(("Failed to notify callback CID=%u for PID=%u\n",
                             uContextID, pData->u32PID));

                if (RT_SUCCESS(vrc))
                    vrc = rc2;
            }
            LogFlowFunc(("Process (CID=%u, status=%u) reported error: %s\n",
                         uContextID, pData->u32Status, errMsg.c_str()));
        }
    }
    LogFlowFunc(("Returned with rc=%Rrc\n", vrc));
    return vrc;
}

/* Function for handling the execution output notification. */
int Guest::notifyCtrlExecOut(uint32_t             u32Function,
                             PCALLBACKDATAEXECOUT pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        PCALLBACKDATAEXECOUT pCallbackData =
            (PCALLBACKDATAEXECOUT)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            pCallbackData->u32PID = pData->u32PID;
            pCallbackData->u32HandleId = pData->u32HandleId;
            pCallbackData->u32Flags = pData->u32Flags;

            /* Make sure we really got something! */
            if (   pData->cbData
                && pData->pvData)
            {
                callbackFreeUserData(pCallbackData->pvData);

                /* Allocate data buffer and copy it */
                pCallbackData->pvData = RTMemAlloc(pData->cbData);
                pCallbackData->cbData = pData->cbData;

                AssertReturn(pCallbackData->pvData, VERR_NO_MEMORY);
                memcpy(pCallbackData->pvData, pData->pvData, pData->cbData);
            }
            else /* Nothing received ... */
            {
                pCallbackData->pvData = NULL;
                pCallbackData->cbData = 0;
            }
        }
        else
            AssertReleaseMsgFailed(("Process output status (PID=%u) does not have allocated callback data!\n",
                                    pData->u32PID));
    }

    int vrc;
    if (callbackIsCanceled(pData->u32PID))
    {
        vrc = callbackNotifyEx(uContextID, VERR_CANCELLED,
                               Guest::tr("The output operation was canceled"));
    }
    else
        vrc = callbackNotifyComplete(uContextID);

    return vrc;
}

/* Function for handling the execution input status notification. */
int Guest::notifyCtrlExecInStatus(uint32_t                  u32Function,
                                  PCALLBACKDATAEXECINSTATUS pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        PCALLBACKDATAEXECINSTATUS pCallbackData =
            (PCALLBACKDATAEXECINSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            /* Save bytes processed. */
            pCallbackData->cbProcessed = pData->cbProcessed;
            pCallbackData->u32Status = pData->u32Status;
            pCallbackData->u32Flags = pData->u32Flags;
            pCallbackData->u32PID = pData->u32PID;
        }
        else
            AssertReleaseMsgFailed(("Process input status (PID=%u) does not have allocated callback data!\n",
                                    pData->u32PID));
    }

    return callbackNotifyComplete(uContextID);
}

int Guest::notifyCtrlClientDisconnected(uint32_t                        u32Function,
                                        PCALLBACKDATACLIENTDISCONNECTED pData)
{
    /* u32Function is 0. */
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    return callbackNotifyEx(uContextID, S_OK,
                            Guest::tr("Client disconnected"));
}

int Guest::processAdd(uint32_t u32PID, ExecuteProcessStatus_T enmStatus,
                      uint32_t uExitCode, uint32_t uFlags)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIterConst it = mGuestProcessMap.find(u32PID);
    if (it == mGuestProcessMap.end())
    {
        VBOXGUESTCTRL_PROCESS process;

        process.mStatus = enmStatus;
        process.mExitCode = uExitCode;
        process.mFlags = uFlags;

        mGuestProcessMap[u32PID] = process;

        return VINF_SUCCESS;
    }

    return VERR_ALREADY_EXISTS;
}

int Guest::processGetByPID(uint32_t u32PID, PVBOXGUESTCTRL_PROCESS pProcess)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pProcess, VERR_INVALID_PARAMETER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIterConst it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        pProcess->mStatus = it->second.mStatus;
        pProcess->mExitCode = it->second.mExitCode;
        pProcess->mFlags = it->second.mFlags;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

int Guest::processSetStatus(uint32_t u32PID, ExecuteProcessStatus_T enmStatus, uint32_t uExitCode, uint32_t uFlags)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIter it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        it->second.mStatus = enmStatus;
        it->second.mExitCode = uExitCode;
        it->second.mFlags = uFlags;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

HRESULT Guest::handleErrorCompletion(int rc)
{
    HRESULT hRC;
    if (rc == VERR_NOT_FOUND)
        hRC = setErrorNoLog(VBOX_E_VM_ERROR,
                            tr("VMM device is not available (is the VM running?)"));
    else if (rc == VERR_CANCELLED)
        hRC = setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("Process execution has been canceled"));
    else if (rc == VERR_TIMEOUT)
        hRC= setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest did not respond within time"));
    else
        hRC = setErrorNoLog(E_UNEXPECTED,
                            tr("Waiting for completion failed with error %Rrc"), rc);
    return hRC;
}

HRESULT Guest::handleErrorHGCM(int rc)
{
    HRESULT hRC;
    if (rc == VERR_INVALID_VM_HANDLE)
        hRC = setErrorNoLog(VBOX_E_VM_ERROR,
                            tr("VMM device is not available (is the VM running?)"));
    else if (rc == VERR_NOT_FOUND)
        hRC = setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest execution service is not ready (yet)"));
    else if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        hRC= setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest execution service is not available"));
    else /* HGCM call went wrong. */
        hRC = setErrorNoLog(E_UNEXPECTED,
                            tr("The HGCM call failed with error %Rrc"), rc);
    return hRC;
}

HRESULT Guest::waitForProcessStatusChange(ULONG uPID, ExecuteProcessStatus_T *pRetStatus, ULONG *puRetExitCode, ULONG uTimeoutMS)
{
    AssertPtr(pRetStatus);
    AssertPtr(puRetExitCode);

    if (uTimeoutMS == 0)
        uTimeoutMS = UINT32_MAX;

    uint64_t u64StartMS = RTTimeMilliTS();

    HRESULT hRC;
    ULONG uRetFlagsIgnored;
    do
    {
        /*
         * Do some busy waiting within the specified time period (if any).
         */
        if (   uTimeoutMS != UINT32_MAX
            && RTTimeMilliTS() - u64StartMS > uTimeoutMS)
        {
            hRC = setError(VBOX_E_IPRT_ERROR,
                           tr("The process (PID %u) did not change its status within time (%ums)"),
                           uPID, uTimeoutMS);
            break;
        }
        hRC = GetProcessStatus(uPID, puRetExitCode, &uRetFlagsIgnored, pRetStatus);
        if (FAILED(hRC))
            break;
        RTThreadSleep(100);
    } while(*pRetStatus == ExecuteProcessStatus_Started && SUCCEEDED(hRC));
    return hRC;
}

HRESULT Guest::executeProcessResult(const char *pszCommand, const char *pszUser, ULONG ulTimeout,
                                    PCALLBACKDATAEXECSTATUS pExecStatus, ULONG *puPID)
{
    AssertPtrReturn(pExecStatus, E_INVALIDARG);
    AssertPtrReturn(puPID, E_INVALIDARG);

    HRESULT rc = S_OK;

    /* Did we get some status? */
    switch (pExecStatus->u32Status)
    {
        case PROC_STS_STARTED:
            /* Process is (still) running; get PID. */
            *puPID = pExecStatus->u32PID;
            break;

        /* In any other case the process either already
         * terminated or something else went wrong, so no PID ... */
        case PROC_STS_TEN: /* Terminated normally. */
        case PROC_STS_TEA: /* Terminated abnormally. */
        case PROC_STS_TES: /* Terminated through signal. */
        case PROC_STS_TOK:
        case PROC_STS_TOA:
        case PROC_STS_DWN:
            /*
             * Process (already) ended, but we want to get the
             * PID anyway to retrieve the output in a later call.
             */
            *puPID = pExecStatus->u32PID;
            break;

        case PROC_STS_ERROR:
            {
                int vrc = pExecStatus->u32Flags; /* u32Flags member contains IPRT error code. */
                if (vrc == VERR_FILE_NOT_FOUND) /* This is the most likely error. */
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The file '%s' was not found on guest"), pszCommand);
                else if (vrc == VERR_PATH_NOT_FOUND)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The path to file '%s' was not found on guest"), pszCommand);
                else if (vrc == VERR_BAD_EXE_FORMAT)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The file '%s' is not an executable format on guest"), pszCommand);
                else if (vrc == VERR_AUTHENTICATION_FAILURE)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The specified user '%s' was not able to logon on guest"), pszUser);
                else if (vrc == VERR_TIMEOUT)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The guest did not respond within time (%ums)"), ulTimeout);
                else if (vrc == VERR_CANCELLED)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The execution operation was canceled"));
                else if (vrc == VERR_PERMISSION_DENIED)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("Invalid user/password credentials"));
                else
                {
                    if (pExecStatus && pExecStatus->u32Status == PROC_STS_ERROR)
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Process could not be started: %Rrc"), pExecStatus->u32Flags);
                    else
                        rc = setErrorNoLog(E_UNEXPECTED,
                                           tr("The service call failed with error %Rrc"), vrc);
                }
            }
            break;

        case PROC_STS_UNDEFINED: /* . */
            rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                               tr("The operation did not complete within time"));
            break;

        default:
            AssertReleaseMsgFailed(("Process (PID %u) reported back an undefined state!\n",
                                    pExecStatus->u32PID));
            rc = E_UNEXPECTED;
            break;
    }

    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::ExecuteProcess(IN_BSTR aCommand, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aUserName, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aCommand);
    CheckComArgOutPointerValid(aPID);
    CheckComArgOutPointerValid(aProgress);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    LogRel(("Executing guest process \"%s\" as user \"%s\" ...\n",
            Utf8Str(aCommand).c_str(), Utf8Str(aUserName).c_str()));

    return executeProcessInternal(aCommand, aFlags, ComSafeArrayInArg(aArguments),
                                  ComSafeArrayInArg(aEnvironment),
                                  aUserName, aPassword, aTimeoutMS, aPID, aProgress, NULL /* rc */);
#endif
}

HRESULT Guest::executeProcessInternal(IN_BSTR aCommand, ULONG aFlags,
                                      ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                      IN_BSTR aUserName, IN_BSTR aPassword,
                                      ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress, int *pRC)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags !=  ExecuteProcessFlag_None)
    {
        if (   !(aFlags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
            && !(aFlags & ExecuteProcessFlag_WaitForProcessStartOnly)
            && !(aFlags & ExecuteProcessFlag_Hidden)
            && !(aFlags & ExecuteProcessFlag_NoProfile))
        {
            if (pRC)
                *pRC = VERR_INVALID_PARAMETER;
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.  Note that this is a multi operation
         * object to perform the following steps:
         * - Operation 1 (0): Create/start process.
         * - Operation 2 (1): Wait for process to exit.
         * If this progress completed successfully (S_OK), the process
         * started and exited normally. In any other case an error/exception
         * occurred.
         */
        ComObjPtr <Progress> pProgress;
        rc = pProgress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = pProgress->init(static_cast<IGuest*>(this),
                                 Bstr(tr("Executing process")).raw(),
                                 TRUE,
                                 2,                                          /* Number of operations. */
                                 Bstr(tr("Starting process ...")).raw());    /* Description of first stage. */
        }
        ComAssertComRC(rc);

        /*
         * Prepare process execution.
         */
        int vrc = VINF_SUCCESS;
        Utf8Str Utf8Command(aCommand);

        /* Adjust timeout. If set to 0, we define
         * an infinite timeout. */
        if (aTimeoutMS == 0)
            aTimeoutMS = UINT32_MAX;

        /* Prepare arguments. */
        char **papszArgv = NULL;
        uint32_t uNumArgs = 0;
        if (aArguments)
        {
            com::SafeArray<IN_BSTR> args(ComSafeArrayInArg(aArguments));
            uNumArgs = args.size();
            papszArgv = (char**)RTMemAlloc(sizeof(char*) * (uNumArgs + 1));
            AssertReturn(papszArgv, E_OUTOFMEMORY);
            for (unsigned i = 0; RT_SUCCESS(vrc) && i < uNumArgs; i++)
                vrc = RTUtf16ToUtf8(args[i], &papszArgv[i]);
            papszArgv[uNumArgs] = NULL;
        }

        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);
        if (RT_SUCCESS(vrc))
        {
            uint32_t uContextID = 0;

            char *pszArgs = NULL;
            if (uNumArgs > 0)
                vrc = RTGetOptArgvToString(&pszArgs, papszArgv, RTGETOPTARGV_CNV_QUOTE_MS_CRT);
            if (RT_SUCCESS(vrc))
            {
                uint32_t cbArgs = pszArgs ? strlen(pszArgs) + 1 : 0; /* Include terminating zero. */

                /* Prepare environment. */
                void *pvEnv = NULL;
                uint32_t uNumEnv = 0;
                uint32_t cbEnv = 0;
                if (aEnvironment)
                {
                    com::SafeArray<IN_BSTR> env(ComSafeArrayInArg(aEnvironment));

                    for (unsigned i = 0; i < env.size(); i++)
                    {
                        vrc = prepareExecuteEnv(Utf8Str(env[i]).c_str(), &pvEnv, &cbEnv, &uNumEnv);
                        if (RT_FAILURE(vrc))
                            break;
                    }
                }

                if (RT_SUCCESS(vrc))
                {
                    /* Allocate payload. */
                    PCALLBACKDATAEXECSTATUS pStatus = (PCALLBACKDATAEXECSTATUS)RTMemAlloc(sizeof(CALLBACKDATAEXECSTATUS));
                    AssertReturn(pStatus, VBOX_E_IPRT_ERROR);
                    RT_ZERO(*pStatus);

                    /* Create callback. */
                    VBOXGUESTCTRL_CALLBACK callback;
                    callback.mType  = VBOXGUESTCTRLCALLBACKTYPE_EXEC_START;
                    callback.cbData = sizeof(CALLBACKDATAEXECSTATUS);
                    callback.pvData = pStatus;
                    callback.pProgress = pProgress;

                    vrc = callbackAdd(&callback, &uContextID);
                    if (RT_SUCCESS(vrc))
                    {
                        VBOXHGCMSVCPARM paParms[15];
                        int i = 0;
                        paParms[i++].setUInt32(uContextID);
                        paParms[i++].setPointer((void*)Utf8Command.c_str(), (uint32_t)Utf8Command.length() + 1);
                        paParms[i++].setUInt32(aFlags);
                        paParms[i++].setUInt32(uNumArgs);
                        paParms[i++].setPointer((void*)pszArgs, cbArgs);
                        paParms[i++].setUInt32(uNumEnv);
                        paParms[i++].setUInt32(cbEnv);
                        paParms[i++].setPointer((void*)pvEnv, cbEnv);
                        paParms[i++].setPointer((void*)Utf8UserName.c_str(), (uint32_t)Utf8UserName.length() + 1);
                        paParms[i++].setPointer((void*)Utf8Password.c_str(), (uint32_t)Utf8Password.length() + 1);

                        /*
                         * If the WaitForProcessStartOnly flag is set, we only want to define and wait for a timeout
                         * until the process was started - the process itself then gets an infinite timeout for execution.
                         * This is handy when we want to start a process inside a worker thread within a certain timeout
                         * but let the started process perform lengthly operations then.
                         */
                        if (aFlags & ExecuteProcessFlag_WaitForProcessStartOnly)
                            paParms[i++].setUInt32(UINT32_MAX /* Infinite timeout */);
                        else
                            paParms[i++].setUInt32(aTimeoutMS);

                        VMMDev *pVMMDev = NULL;
                        {
                            /* Make sure mParent is valid, so set the read lock while using.
                             * Do not keep this lock while doing the actual call, because in the meanwhile
                             * another thread could request a write lock which would be a bad idea ... */
                            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                            /* Forward the information to the VMM device. */
                            AssertPtr(mParent);
                            pVMMDev = mParent->getVMMDev();
                        }

                        if (pVMMDev)
                        {
                            LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                            vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_CMD,
                                                       i, paParms);
                        }
                        else
                            vrc = VERR_INVALID_VM_HANDLE;
                    }
                    RTMemFree(pvEnv);
                }
                RTStrFree(pszArgs);
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback (timeout=%dms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECSTATUS pExecStatus = NULL;

                 /*
                 * Wait for the first stage (=0) to complete (that is starting the process).
                 */
                vrc = callbackWaitForCompletion(uContextID, 0 /* Stage */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecStatus, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        rc = executeProcessResult(Utf8Command.c_str(), Utf8UserName.c_str(), aTimeoutMS,
                                                  pExecStatus, aPID);
                        callbackFreeUserData(pExecStatus);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process execution status data"));
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);

                /*
                 * Do *not* remove the callback yet - we might wait with the IProgress object on something
                 * else (like end of process) ...
                 */
            }
            else
                rc = handleErrorHGCM(vrc);

            for (unsigned i = 0; i < uNumArgs; i++)
                RTMemFree(papszArgv[i]);
            RTMemFree(papszArgv);
        }

        if (SUCCEEDED(rc))
        {
            /* Return the progress to the caller. */
            pProgress.queryInterfaceTo(aProgress);
        }
        else
        {
            if (!pRC) /* Skip logging internal calls. */
                LogRel(("Executing guest process \"%s\" as user \"%s\" failed with %Rrc\n",
                        Utf8Command.c_str(), Utf8UserName.c_str(), vrc));
        }

        if (pRC)
            *pRC = vrc;
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::SetProcessInput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, ComSafeArrayIn(BYTE, aData), ULONG *aBytesWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    CheckComArgOutPointerValid(aBytesWritten);

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & ProcessInputFlag_EndOfFile))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetByPID(aPID, &process);
        if (RT_SUCCESS(vrc))
        {
            /* PID exists; check if process is still running. */
            if (process.mStatus != ExecuteProcessStatus_Started)
                rc = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("Cannot inject input to not running process (PID %u)"), aPID);
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Cannot inject input to non-existent process (PID %u)"), aPID);

        if (SUCCEEDED(rc))
        {
            uint32_t uContextID = 0;

            /*
             * Create progress object.
             * This progress object, compared to the one in executeProgress() above,
             * is only single-stage local and is used to determine whether the operation
             * finished or got canceled.
             */
            ComObjPtr <Progress> pProgress;
            rc = pProgress.createObject();
            if (SUCCEEDED(rc))
            {
                rc = pProgress->init(static_cast<IGuest*>(this),
                                     Bstr(tr("Setting input for process")).raw(),
                                     TRUE /* Cancelable */);
            }
            if (FAILED(rc)) throw rc;
            ComAssert(!pProgress.isNull());

            /* Adjust timeout. */
            if (aTimeoutMS == 0)
                aTimeoutMS = UINT32_MAX;

            /* Construct callback data. */
            VBOXGUESTCTRL_CALLBACK callback;
            callback.mType  = VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS;
            callback.cbData = sizeof(CALLBACKDATAEXECINSTATUS);

            PCALLBACKDATAEXECINSTATUS pStatus = (PCALLBACKDATAEXECINSTATUS)RTMemAlloc(callback.cbData);
            AssertReturn(pStatus, VBOX_E_IPRT_ERROR);
            RT_ZERO(*pStatus);

            /* Save PID + output flags for later use. */
            pStatus->u32PID = aPID;
            pStatus->u32Flags = aFlags;

            callback.pvData = pStatus;
            callback.pProgress = pProgress;

            /* Add the callback. */
            vrc = callbackAdd(&callback, &uContextID);
            if (RT_SUCCESS(vrc))
            {
                com::SafeArray<BYTE> sfaData(ComSafeArrayInArg(aData));
                uint32_t cbSize = sfaData.size();

                VBOXHGCMSVCPARM paParms[6];
                int i = 0;
                paParms[i++].setUInt32(uContextID);
                paParms[i++].setUInt32(aPID);
                paParms[i++].setUInt32(aFlags);
                paParms[i++].setPointer(sfaData.raw(), cbSize);
                paParms[i++].setUInt32(cbSize);

                {
                    VMMDev *pVMMDev = NULL;
                    {
                        /* Make sure mParent is valid, so set the read lock while using.
                         * Do not keep this lock while doing the actual call, because in the meanwhile
                         * another thread could request a write lock which would be a bad idea ... */
                        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                        /* Forward the information to the VMM device. */
                        AssertPtr(mParent);
                        pVMMDev = mParent->getVMMDev();
                    }

                    if (pVMMDev)
                    {
                        LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                        vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_SET_INPUT,
                                                   i, paParms);
                    }
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback ...\n"));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECINSTATUS pExecStatusIn = NULL;

                 /*
                 * Wait for the first stage (=0) to complete (that is starting the process).
                 */
                vrc = callbackWaitForCompletion(uContextID, 0 /* Stage */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecStatusIn, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        switch (pExecStatusIn->u32Status)
                        {
                            case INPUT_STS_WRITTEN:
                                *aBytesWritten = pExecStatusIn->cbProcessed;
                                break;

                            default:
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Client error %u while processing input data"), pExecStatusIn->u32Status);
                                break;
                        }

                        callbackFreeUserData(pExecStatusIn);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process input status data"));
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);
            }
            else
                rc = handleErrorHGCM(vrc);

            if (SUCCEEDED(rc))
            {
                /* Nothing to do here yet. */
            }

            /* The callback isn't needed anymore -- just was kept locally. */
            callbackDestroy(uContextID);

            /* Cleanup. */
            if (!pProgress.isNull())
                pProgress->uninit();
            pProgress.setNull();
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessOutput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, LONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    if (aSize < 0)
        return setError(E_INVALIDARG, tr("The size argument (%lld) is negative"), aSize);
    if (aSize == 0)
        return setError(E_INVALIDARG, tr("The size (%lld) is zero"), aSize);
    if (aFlags)
    {
        if (!(aFlags & ProcessOutputFlag_StdErr))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetByPID(aPID, &process);
        if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Cannot get output from non-existent process (PID %u)"), aPID);

        if (SUCCEEDED(rc))
        {
            uint32_t uContextID = 0;

            /*
             * Create progress object.
             * This progress object, compared to the one in executeProgress() above,
             * is only single-stage local and is used to determine whether the operation
             * finished or got canceled.
             */
            ComObjPtr <Progress> pProgress;
            rc = pProgress.createObject();
            if (SUCCEEDED(rc))
            {
                rc = pProgress->init(static_cast<IGuest*>(this),
                                     Bstr(tr("Setting input for process")).raw(),
                                     TRUE /* Cancelable */);
            }
            if (FAILED(rc)) throw rc;
            ComAssert(!pProgress.isNull());

            /* Adjust timeout. */
            if (aTimeoutMS == 0)
                aTimeoutMS = UINT32_MAX;

            /* Set handle ID. */
            uint32_t uHandleID = OUTPUT_HANDLE_ID_STDOUT; /* Default */
            if (aFlags & ProcessOutputFlag_StdErr)
                uHandleID = OUTPUT_HANDLE_ID_STDERR;

            /* Construct callback data. */
            VBOXGUESTCTRL_CALLBACK callback;
            callback.mType  = VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT;
            callback.cbData = sizeof(CALLBACKDATAEXECOUT);

            PCALLBACKDATAEXECOUT pStatus = (PCALLBACKDATAEXECOUT)RTMemAlloc(callback.cbData);
            AssertReturn(pStatus, VBOX_E_IPRT_ERROR);
            RT_ZERO(*pStatus);

            /* Save PID + output flags for later use. */
            pStatus->u32PID = aPID;
            pStatus->u32Flags = aFlags;

            callback.pvData = pStatus;
            callback.pProgress = pProgress;

            /* Add the callback. */
            vrc = callbackAdd(&callback, &uContextID);
            if (RT_SUCCESS(vrc))
            {
                VBOXHGCMSVCPARM paParms[5];
                int i = 0;
                paParms[i++].setUInt32(uContextID);
                paParms[i++].setUInt32(aPID);
                paParms[i++].setUInt32(uHandleID);
                paParms[i++].setUInt32(0 /* Flags, none set yet */);

                VMMDev *pVMMDev = NULL;
                {
                    /* Make sure mParent is valid, so set the read lock while using.
                     * Do not keep this lock while doing the actual call, because in the meanwhile
                     * another thread could request a write lock which would be a bad idea ... */
                    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                    /* Forward the information to the VMM device. */
                    AssertPtr(mParent);
                    pVMMDev = mParent->getVMMDev();
                }

                if (pVMMDev)
                {
                    LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                    vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_GET_OUTPUT,
                                               i, paParms);
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback (timeout=%dms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECOUT pExecOut = NULL;

                 /*
                 * Wait for the first stage (=0) to complete (that is starting the process).
                 */
                vrc = callbackWaitForCompletion(uContextID, 0 /* Stage */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecOut, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        com::SafeArray<BYTE> outputData((size_t)aSize);

                        if (pExecOut->cbData)
                        {
                            /* Do we need to resize the array? */
                            if (pExecOut->cbData > aSize)
                                outputData.resize(pExecOut->cbData);

                            /* Fill output in supplied out buffer. */
                            memcpy(outputData.raw(), pExecOut->pvData, pExecOut->cbData);
                            outputData.resize(pExecOut->cbData); /* Shrink to fit actual buffer size. */
                        }
                        else
                        {
                            /* No data within specified timeout available. */
                            outputData.resize(0);
                        }

                        /* Detach output buffer to output argument. */
                        outputData.detachTo(ComSafeArrayOutArg(aData));

                        callbackFreeUserData(pExecOut);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process output data"));
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);
            }
            else
                rc = handleErrorHGCM(vrc);

            if (SUCCEEDED(rc))
            {

            }

            /* The callback isn't needed anymore -- just was kept locally. */
            callbackDestroy(uContextID);

            /* Cleanup. */
            if (!pProgress.isNull())
                pProgress->uninit();
            pProgress.setNull();
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessStatus(ULONG aPID, ULONG *aExitCode, ULONG *aFlags, ExecuteProcessStatus_T *aStatus)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgNotNull(aExitCode);
    CheckComArgNotNull(aFlags);
    CheckComArgNotNull(aStatus);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetByPID(aPID, &process);
        if (RT_SUCCESS(vrc))
        {
            *aExitCode = process.mExitCode;
            *aFlags = process.mFlags;
            *aStatus = process.mStatus;
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Process (PID %u) not found!"), aPID);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::CopyFromGuest(IN_BSTR aSource, IN_BSTR aDest,
                                  IN_BSTR aUserName, IN_BSTR aPassword,
                                  ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgStrNotEmptyOrNull(aUserName);
    CheckComArgStrNotEmptyOrNull(aPassword);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != CopyFileFlag_None)
    {
        if (   !(aFlags & CopyFileFlag_Recursive)
            && !(aFlags & CopyFileFlag_Update)
            && !(aFlags & CopyFileFlag_FollowLinks))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Copying file from guest to host")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        TaskGuest *pTask = new TaskGuest(TaskGuest::CopyFileFromGuest, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<TaskGuest> task(pTask);

        /* Assign data - aSource is the source file on the host,
         * aDest reflects the full path on the guest. */
        task->strSource   = (Utf8Str(aSource));
        task->strDest     = (Utf8Str(aDest));
        task->strUserName = (Utf8Str(aUserName));
        task->strPassword = (Utf8Str(aPassword));
        task->uFlags      = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::CopyToGuest(IN_BSTR aSource, IN_BSTR aDest,
                                IN_BSTR aUserName, IN_BSTR aPassword,
                                ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgStrNotEmptyOrNull(aUserName);
    CheckComArgStrNotEmptyOrNull(aPassword);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != CopyFileFlag_None)
    {
        if (   !(aFlags & CopyFileFlag_Recursive)
            && !(aFlags & CopyFileFlag_Update)
            && !(aFlags & CopyFileFlag_FollowLinks))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Copying file from host to guest")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        TaskGuest *pTask = new TaskGuest(TaskGuest::CopyFileToGuest, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<TaskGuest> task(pTask);

        /* Assign data - aSource is the source file on the host,
         * aDest reflects the full path on the guest. */
        task->strSource   = (Utf8Str(aSource));
        task->strDest     = (Utf8Str(aDest));
        task->strUserName = (Utf8Str(aUserName));
        task->strPassword = (Utf8Str(aPassword));
        task->uFlags      = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::DirectoryClose(ULONG aHandle)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    if (directoryHandleExists(aHandle))
    {
        directoryDestroyHandle(aHandle);
        return S_OK;
    }

    return setError(VBOX_E_IPRT_ERROR,
                    Guest::tr("Directory handle is invalid"));
#endif
}

STDMETHODIMP Guest::DirectoryCreate(IN_BSTR aDirectory,
                                    IN_BSTR aUserName, IN_BSTR aPassword,
                                    ULONG aMode, ULONG aFlags)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    LogRel(("Creating guest directory \"%s\" as  user \"%s\" ...\n",
            Utf8Str(aDirectory).c_str(), Utf8Str(aUserName).c_str()));

    return directoryCreateInternal(aDirectory,
                                   aUserName, aPassword,
                                   aMode, aFlags, NULL /* rc */);
#endif
}

HRESULT Guest::directoryCreateInternal(IN_BSTR aDirectory,
                                       IN_BSTR aUserName, IN_BSTR aPassword,
                                       ULONG aMode, ULONG aFlags, int *pRC)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != DirectoryCreateFlag_None)
    {
        if (!(aFlags & DirectoryCreateFlag_Parents))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8Directory(aDirectory);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);

        com::SafeArray<IN_BSTR> args;
        com::SafeArray<IN_BSTR> env;

        /*
         * Prepare tool command line.
         */
        if (aFlags & DirectoryCreateFlag_Parents)
            args.push_back(Bstr("--parents").raw());        /* We also want to create the parent directories. */
        if (aMode > 0)
        {
            args.push_back(Bstr("--mode").raw());           /* Set the creation mode. */

            char szMode[16];
            RTStrPrintf(szMode, sizeof(szMode), "%o", aMode);
            args.push_back(Bstr(szMode).raw());
        }
        args.push_back(Bstr(Utf8Directory).raw());  /* The directory we want to create. */

        /*
         * Execute guest process.
         */
        ComPtr<IProgress> progressExec;
        ULONG uPID;
        if (SUCCEEDED(rc))
        {
            rc = ExecuteProcess(Bstr(VBOXSERVICE_TOOL_MKDIR).raw(),
                                ExecuteProcessFlag_Hidden,
                                ComSafeArrayAsInParam(args),
                                ComSafeArrayAsInParam(env),
                                Bstr(Utf8UserName).raw(),
                                Bstr(Utf8Password).raw(),
                                5 * 1000 /* Wait 5s for getting the process started. */,
                                &uPID, progressExec.asOutParam());
        }

        if (SUCCEEDED(rc))
        {
            /* Wait for process to exit ... */
            rc = progressExec->WaitForCompletion(-1);
            if (FAILED(rc)) return rc;

            BOOL fCompleted = FALSE;
            BOOL fCanceled = FALSE;
            progressExec->COMGETTER(Completed)(&fCompleted);
            if (!fCompleted)
                progressExec->COMGETTER(Canceled)(&fCanceled);

            if (fCompleted)
            {
                ExecuteProcessStatus_T retStatus;
                ULONG uRetExitCode, uRetFlags;
                if (SUCCEEDED(rc))
                {
                    rc = GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                    if (SUCCEEDED(rc) && uRetExitCode != 0)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("Error %u while creating guest directory"), uRetExitCode);
                    }
                }
            }
            else if (fCanceled)
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Guest directory creation was aborted"));
            else
                AssertReleaseMsgFailed(("Guest directory creation neither completed nor canceled!?\n"));
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

/**
 * Creates a new directory handle ID and returns it.
 *
 * @return IPRT status code.
 * @param puHandle             Pointer where the handle gets stored to.
 * @param pszDirectory         Directory the handle is assigned to.
 * @param pszFilter            Directory filter.  Optional.
 * @param uFlags               Directory open flags.
 *
 */
int Guest::directoryCreateHandle(ULONG *puHandle, const char *pszDirectory, const char *pszFilter, ULONG uFlags)
{
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDirectory, VERR_INVALID_POINTER);
    /* pszFilter is optional. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_TOO_MUCH_DATA;
    for (uint32_t i = 0; i < UINT32_MAX; i++)
    {
        /* Create a new context ID ... */
        uint32_t uHandleTry = ASMAtomicIncU32(&mNextDirectoryID);
        GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandleTry);
        if (mGuestDirectoryMap.end() == it)
        {
            rc = VINF_SUCCESS;
            if (!RTStrAPrintf(&mGuestDirectoryMap[uHandleTry].mpszDirectory, pszDirectory))
                rc = VERR_NO_MEMORY;
            else
            {
                /* Filter is optional. */
                if (pszFilter)
                {
                    if (!RTStrAPrintf(&mGuestDirectoryMap[uHandleTry].mpszFilter, pszFilter))
                        rc = VERR_NO_MEMORY;
                }

                if (RT_SUCCESS(rc))
                {
                    mGuestDirectoryMap[uHandleTry].uFlags = uFlags;
                    *puHandle = uHandleTry;

                    break;
                }
            }

            if (RT_FAILURE(rc))
                break;

            Assert(mGuestDirectoryMap.size());
        }
    }

    return rc;
}

void Guest::directoryDestroyHandle(uint32_t uHandle)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
    {
        RTStrFree(it->second.mpszDirectory);
        RTStrFree(it->second.mpszFilter);

        /* Remove callback context (not used anymore). */
        mGuestDirectoryMap.erase(it);
    }
}

uint32_t Guest::directoryGetPID(uint32_t uHandle)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
        return it->second.mPID;

    return 0;
}

bool Guest::directoryHandleExists(uint32_t uHandle)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
        return true;

    return false;
}

STDMETHODIMP Guest::DirectoryOpen(IN_BSTR aDirectory, IN_BSTR aFilter,
                                  ULONG aFlags, IN_BSTR aUserName, IN_BSTR aPassword,
                                  ULONG *aHandle)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);
    CheckComArgNotNull(aHandle);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return directoryOpenInternal(aDirectory, aFilter,
                                 aFlags,
                                 aUserName, aPassword,
                                 aHandle, NULL /* rc */);
#endif
}

HRESULT Guest::directoryOpenInternal(IN_BSTR aDirectory, IN_BSTR aFilter,
                                     ULONG aFlags,
                                     IN_BSTR aUserName, IN_BSTR aPassword,
                                     ULONG *aHandle, int *pRC)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);
    CheckComArgNotNull(aHandle);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. No flags supported yet. */
    if (aFlags != DirectoryOpenFlag_None)
        return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8Directory(aDirectory);
        Utf8Str Utf8Filter(aFilter);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);

        com::SafeArray<IN_BSTR> args;
        com::SafeArray<IN_BSTR> env;

        /*
         * Prepare tool command line.
         */

        /* We need to get output which is machine-readable in form
         * of "key=value\0..key=value\0\0". */
        args.push_back(Bstr("--machinereadable").raw());

        /* We want the long output format. Handy for getting a lot of
         * details we could (should?) use (later). */
        args.push_back(Bstr("-l").raw());

        /* As we want to keep this stuff simple we don't do recursive (-R)
         * or dereferencing (--dereference) lookups here. This has to be done by
         * the user. */

        /* Construct and hand in actual directory name + filter we want to open. */
        char *pszDirectoryFinal;
        int cbRet;
        if (Utf8Filter.isEmpty())
            cbRet = RTStrAPrintf(&pszDirectoryFinal, "%s", Utf8Directory.c_str());
        else
            cbRet = RTStrAPrintf(&pszDirectoryFinal, "%s/%s",
                                 Utf8Directory.c_str(), Utf8Filter.c_str());
        if (!cbRet)
            return setError(E_OUTOFMEMORY, tr("Out of memory while allocating final directory"));

        args.push_back(Bstr(pszDirectoryFinal).raw());  /* The directory we want to open. */

        /*
         * Execute guest process.
         */
        ComPtr<IProgress> progressExec;
        ULONG uPID;

        rc = ExecuteProcess(Bstr(VBOXSERVICE_TOOL_LS).raw(),
                            ExecuteProcessFlag_Hidden,
                            ComSafeArrayAsInParam(args),
                            ComSafeArrayAsInParam(env),
                            Bstr(Utf8UserName).raw(),
                            Bstr(Utf8Password).raw(),
                            30 * 1000 /* Wait 30s for getting the process started. */,
                            &uPID, progressExec.asOutParam());

        RTStrFree(pszDirectoryFinal);

        if (SUCCEEDED(rc))
        {
            /* Wait for process to exit ... */
            rc = progressExec->WaitForCompletion(-1);
            if (FAILED(rc)) return rc;

            BOOL fCompleted = FALSE;
            BOOL fCanceled = FALSE;
            progressExec->COMGETTER(Completed)(&fCompleted);
            if (!fCompleted)
                progressExec->COMGETTER(Canceled)(&fCanceled);

            if (fCompleted)
            {
                ExecuteProcessStatus_T retStatus;
                ULONG uRetExitCode, uRetFlags;
                if (SUCCEEDED(rc))
                {
                    rc = GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                    if (SUCCEEDED(rc) && uRetExitCode != 0)
                    {
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("Error %u while opening guest directory"), uRetExitCode);
                    }
                }
            }
            else if (fCanceled)
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Guest directory opening was aborted"));
            else
                AssertReleaseMsgFailed(("Guest directory opening neither completed nor canceled!?\n"));

            if (SUCCEEDED(rc))
            {
                /* Assign new directory handle ID. */
                int vrc = directoryCreateHandle(aHandle,
                                                Utf8Directory.c_str(),
                                                Utf8Filter.isEmpty() ? NULL : Utf8Filter.c_str(),
                                                aFlags);
                if (RT_FAILURE(vrc))
                {
                    rc = setError(VBOX_E_IPRT_ERROR,
                                  tr("Unable to create guest directory handle (%Rrc)"), vrc);
                }
            }
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::DirectoryRead(ULONG aHandle, IGuestDirEntry **aDirEntry)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    uint32_t uPID = directoryGetPID(aHandle);
    if (uPID)
    {
        SafeArray<BYTE> aOutputData;
        ULONG cbOutputData = 0;

        HRESULT rc = this->GetProcessOutput(uPID, ProcessOutputFlag_None,
                                            30 * 1000 /* Timeout in ms */,
                                            _64K, ComSafeArrayAsOutParam(aOutputData));
        if (SUCCEEDED(rc))
        {

        }

        return rc;
    }

    return setError(VBOX_E_IPRT_ERROR,
                    Guest::tr("Directory handle is invalid"));
#endif
}

STDMETHODIMP Guest::FileExists(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return fileExistsInternal(aFile,
                              aUserName, aPassword, aExists,
                              NULL /* rc */);
#endif
}

HRESULT Guest::fileExistsInternal(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, BOOL *aExists, int *pRC)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8File(aFile);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);

        com::SafeArray<IN_BSTR> args;
        com::SafeArray<IN_BSTR> env;

        /*
         * Prepare tool command line.
         */

        /* Only the actual file name to chekc is needed for now. */
        args.push_back(Bstr(Utf8File).raw());

        /*
         * Execute guest process.
         */
        ComPtr<IProgress> progressExec;
        ULONG uPID;

        rc = ExecuteProcess(Bstr(VBOXSERVICE_TOOL_STAT).raw(),
                            ExecuteProcessFlag_Hidden,
                            ComSafeArrayAsInParam(args),
                            ComSafeArrayAsInParam(env),
                            Bstr(Utf8UserName).raw(),
                            Bstr(Utf8Password).raw(),
                            30 * 1000 /* Wait 30s for getting the process started. */,
                            &uPID, progressExec.asOutParam());

        if (SUCCEEDED(rc))
        {
            /* Wait for process to exit ... */
            rc = progressExec->WaitForCompletion(-1);
            if (FAILED(rc)) return rc;

            BOOL fCompleted = FALSE;
            BOOL fCanceled = FALSE;
            progressExec->COMGETTER(Completed)(&fCompleted);
            if (!fCompleted)
                progressExec->COMGETTER(Canceled)(&fCanceled);

            if (fCompleted)
            {
                ExecuteProcessStatus_T retStatus;
                ULONG uRetExitCode, uRetFlags;
                if (SUCCEEDED(rc))
                {
                    rc = GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                    if (SUCCEEDED(rc))
                    {
                        *aExists = uRetExitCode == 0 ? TRUE : FALSE;
                    }
                    else
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("Error %u while checking for existence of file \"%s\""),
                                      uRetExitCode, Utf8File.c_str());
                }
            }
            else if (fCanceled)
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Checking for file existence was aborted"));
            else
                AssertReleaseMsgFailed(("Checking for file existence neither completed nor canceled!?\n"));
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::FileQuerySize(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, LONG64 *aSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUserName) == NULL || *(aUserName) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return fileQuerySizeInternal(aFile,
                                 aUserName, aPassword, aSize,
                                 NULL /* rc */);
#endif
}

HRESULT Guest::fileQuerySizeInternal(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, LONG64 *aSize, int *pRC)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8File(aFile);
        Utf8Str Utf8UserName(aUserName);
        Utf8Str Utf8Password(aPassword);

        com::SafeArray<IN_BSTR> args;
        com::SafeArray<IN_BSTR> env;

        /*
         * Prepare tool command line.
         */

        /* We need to get output which is machine-readable in form
         * of "key=value\0..key=value\0\0". */
        args.push_back(Bstr("--machinereadable").raw());

        /* Only the actual file name to chekc is needed for now. */
        args.push_back(Bstr(Utf8File).raw());

        /*
         * Execute guest process.
         */
        ComPtr<IProgress> progressExec;
        ULONG uPID;

        rc = ExecuteProcess(Bstr(VBOXSERVICE_TOOL_STAT).raw(),
                            ExecuteProcessFlag_Hidden,
                            ComSafeArrayAsInParam(args),
                            ComSafeArrayAsInParam(env),
                            Bstr(Utf8UserName).raw(),
                            Bstr(Utf8Password).raw(),
                            30 * 1000 /* Wait 30s for getting the process started. */,
                            &uPID, progressExec.asOutParam());

        if (SUCCEEDED(rc))
        {
            /* Wait for process to exit ... */
            rc = progressExec->WaitForCompletion(-1);
            if (FAILED(rc)) return rc;

            BOOL fCompleted = FALSE;
            BOOL fCanceled = FALSE;
            progressExec->COMGETTER(Completed)(&fCompleted);
            if (!fCompleted)
                progressExec->COMGETTER(Canceled)(&fCanceled);

            if (fCompleted)
            {
                ExecuteProcessStatus_T retStatus;
                ULONG uRetExitCode, uRetFlags;
                if (SUCCEEDED(rc))
                {
                    rc = GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                    if (SUCCEEDED(rc))
                    {
                        if (uRetExitCode == 0)
                        {
                            /* Get file size from output stream. */
                            SafeArray<BYTE> aOutputData;
                            ULONG cbOutputData = 0;

                            GuestProcessStream guestStream;
                            int vrc = VINF_SUCCESS;
                            for (;;)
                            {
                                rc = this->GetProcessOutput(uPID, ProcessOutputFlag_None,
                                                            30 * 1000 /* Timeout in ms */,
                                                            _64K, ComSafeArrayAsOutParam(aOutputData));
                                /** @todo Do stream header validation! */
                                if (   SUCCEEDED(rc)
                                    && aOutputData.size())
                                {
                                    vrc = guestStream.AddData(aOutputData.raw(), aOutputData.size());
                                    if (RT_UNLIKELY(RT_FAILURE(vrc)))
                                        rc = setError(VBOX_E_IPRT_ERROR,
                                                      tr("Error while adding guest output to stream buffer (%Rrc)"), vrc);
                                }
                                else
                                    break;
                            }

                            if (SUCCEEDED(rc))
                            {
                                vrc = guestStream.Parse();
                                if (   RT_SUCCESS(vrc)
                                    || vrc == VERR_MORE_DATA)
                                {
                                    int64_t iVal;
                                    vrc = guestStream.GetInt64Ex("st_size", &iVal);
                                    if (RT_SUCCESS(vrc))
                                        *aSize = iVal;
                                    else
                                        rc = setError(VBOX_E_IPRT_ERROR,
                                                      tr("Unable to retrieve file size (%Rrc)"), vrc);
                                }
                                else
                                    rc = setError(VBOX_E_IPRT_ERROR,
                                                  tr("Error while parsing guest output (%Rrc)"), vrc);
                            }
                        }
                        else
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("Error querying file size for file \"%s\" (exit code %u)"),
                                          Utf8File.c_str(), uRetExitCode);
                    }
                }
            }
            else if (fCanceled)
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Checking for file existence was aborted"));
            else
                AssertReleaseMsgFailed(("Checking for file existence neither completed nor canceled!?\n"));
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::UpdateGuestAdditions(IN_BSTR aSource, ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Updating Guest Additions")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        TaskGuest *pTask = new TaskGuest(TaskGuest::UpdateGuestAdditions, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<TaskGuest> task(pTask);

        /* Assign data - in that case aSource is the full path
         * to the Guest Additions .ISO we want to mount. */
        task->strSource = (Utf8Str(aSource));
        task->uFlags = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

