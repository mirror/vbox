/* $Id: */
/** @file
 * VirtualBox Guest Control - Guest file handling.
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

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileExistsInternal(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, BOOL *aExists, int *pRC)
{
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
}
#endif /* VBOX_WITH_GUEST_CONTROL */

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

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileQuerySizeInternal(IN_BSTR aFile, IN_BSTR aUserName, IN_BSTR aPassword, LONG64 *aSize, int *pRC)
{
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
                                                            10 * 1000 /* Timeout in ms */,
                                                            _64K, ComSafeArrayAsOutParam(aOutputData));
                                /** @todo Do stream header validation! */
                                if (   SUCCEEDED(rc)
                                    && aOutputData.size())
                                {
                                    vrc = guestStream.AddData(aOutputData.raw(), aOutputData.size());
                                    if (RT_UNLIKELY(RT_FAILURE(vrc)))
                                        rc = setError(VBOX_E_IPRT_ERROR,
                                                      tr("Query file size: Error while adding guest output to stream buffer for file \"%s\" (%Rrc)"),
                                                      Utf8File.c_str(), vrc);
                                }
                                else /* No more output! */
                                    break;
                            }

                            if (SUCCEEDED(rc))
                            {
                                vrc = guestStream.ParseBlock(); /* We only need one (the first) block. */
                                if (   RT_SUCCESS(vrc)
                                    || vrc == VERR_MORE_DATA)
                                {
                                    int64_t iVal;
                                    vrc = guestStream.GetInt64Ex("st_size", &iVal);
                                    if (RT_SUCCESS(vrc))
                                        *aSize = iVal;
                                    else
                                        rc = setError(VBOX_E_IPRT_ERROR,
                                                      tr("Query file size: Unable to retrieve file size for file \"%s\" (%Rrc)"),
                                                      Utf8File.c_str(), vrc);
                                }
                                else
                                    rc = setError(VBOX_E_IPRT_ERROR,
                                                  tr("Query file size: Error while parsing guest output for file \"%s\" (%Rrc)"),
                                                  Utf8File.c_str(), vrc);
                            }
                        }
                        else
                            rc = setError(VBOX_E_IPRT_ERROR,
                                          tr("Query file size: Error querying file size for file \"%s\" (exit code %u)"),
                                          Utf8File.c_str(), uRetExitCode);
                    }
                }
            }
            else if (fCanceled)
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Query file size: Checking for file size was aborted"));
            else
                AssertReleaseMsgFailed(("Checking for file size neither completed nor canceled!?\n"));
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

