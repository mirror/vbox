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

STDMETHODIMP Guest::FileExists(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUsername) == NULL || *(aUsername) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return fileExistsInternal(aFile,
                              aUsername, aPassword, aExists,
                              NULL /* rc */);
#endif
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileExistsInternal(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, BOOL *aExists, int *pRC)
{
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8File(aFile);
        Utf8Str Utf8Username(aUsername);
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
        rc = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_STAT).raw(), Bstr("Checking for file existence").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   NULL /* Progress */, NULL /* PID */);

        /* If the call succeeded the file exists, otherwise it does not. */
        *aExists = SUCCEEDED(rc) ? TRUE : FALSE;
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::FileQuerySize(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, LONG64 *aSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUsername) == NULL || *(aUsername) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return fileQuerySizeInternal(aFile,
                                 aUsername, aPassword, aSize,
                                 NULL /* rc */);
#endif
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileQuerySizeInternal(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, LONG64 *aSize, int *pRC)
{
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    try
    {
        Utf8Str Utf8File(aFile);
        Utf8Str Utf8Username(aUsername);
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
        ComPtr<IProgress> progressFileSize;
        ULONG uPID;

        rc = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_STAT).raw(), Bstr("Querying file size").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   progressFileSize.asOutParam(), &uPID);
        if (SUCCEEDED(rc))
        {
            GuestCtrlStreamObjects streamObjs;
            rc = executeCollectOutput(uPID, streamObjs);
            if (SUCCEEDED(rc))
            {
                /** @todo */
                #if 0
                                                    int64_t iVal;
                                    vrc = guestStream.GetInt64Ex("st_size", &iVal);
                                    if (RT_SUCCESS(vrc))
                                        *aSize = iVal;
                                    else
                                        rc = setError(VBOX_E_IPRT_ERROR,
                                                      tr("Query file size: Unable to retrieve file size for file \"%s\" (%Rrc)"),
                                                      Utf8File.c_str(), vrc);
              #endif
            }
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

