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
                              aUsername, aPassword, aExists);
#endif
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileExistsInternal(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, BOOL *aExists)
{
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    RTFSOBJINFO objInfo;
    int rc;
    HRESULT hr = fileQueryInfoInternal(aFile,
                                       aUsername, aPassword,
                                       &objInfo, RTFSOBJATTRADD_NOTHING, &rc);
    if (SUCCEEDED(hr))
    {
        switch (rc)
        {
            case VINF_SUCCESS:
                *aExists = TRUE;
                break;

            case VERR_FILE_NOT_FOUND:
                *aExists = FALSE;
                break;

            case VERR_NOT_FOUND:
                rc = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("Unable to query file existence"));
                break;

            default:
                AssertReleaseMsgFailed(("fileExistsInternal: Unknown return value (%Rrc)\n", rc));
                break;
        }
    }
    return hr;
}

HRESULT Guest::fileQueryInfoInternal(IN_BSTR aFile,
                                     IN_BSTR aUsername, IN_BSTR aPassword,
                                     PRTFSOBJINFO aObjInfo, RTFSOBJATTRADD enmAddAttribs,
                                     int *pRC)
{
    using namespace guestControl;

    /* aUsername is optional. */
    /* aPassword is optional. */
    /* aObjInfo is optional. */

    HRESULT hr;
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
        ULONG uPID;
        hr = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_STAT).raw(), Bstr("Querying file information").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   NULL /* Progress */, &uPID);
        if (SUCCEEDED(hr))
        {
            GuestCtrlStreamObjects streamObjs;
            hr = executeStreamParse(uPID, streamObjs);
            if (SUCCEEDED(hr))
            {
                int rc = VINF_SUCCESS;

                GuestProcessStreamBlock *pBlock = streamObjs[0];
                AssertPtr(pBlock);
                const char *pszFsType = pBlock->GetString("ftype");
                if (!pszFsType) /* Attribute missing? */
                     rc = VERR_NOT_FOUND;
                if (   RT_SUCCESS(rc)
                    && strcmp(pszFsType, "-")) /* Regular file? */
                {
                     rc = VERR_FILE_NOT_FOUND;
                }
                if (   RT_SUCCESS(rc)
                    && aObjInfo) /* Do we want object details? */
                {
                    hr = executeStreamQueryFsObjInfo(aFile, pBlock,
                                                     aObjInfo, enmAddAttribs);
                }

                executeStreamFree(streamObjs);

                if (pRC)
                    *pRC = rc;
            }
        }
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
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

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    return fileQuerySizeInternal(aFile,
                                 aUsername, aPassword, aSize);
#endif
}

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::fileQuerySizeInternal(IN_BSTR aFile, IN_BSTR aUsername, IN_BSTR aPassword, LONG64 *aSize)
{
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aFile);

    int rc;
    RTFSOBJINFO objInfo;
    HRESULT hr = fileQueryInfoInternal(aFile,
                                       aUsername, aPassword,
                                       &objInfo, RTFSOBJATTRADD_NOTHING, &rc);
    if (SUCCEEDED(hr))
    {
        switch (rc)
        {
            case VINF_SUCCESS:
                *aSize = objInfo.cbObject;
                break;

            case VERR_FILE_NOT_FOUND:
                rc = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("File not found"));
                break;

            case VERR_NOT_FOUND:
                rc = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("Unable to query file size"));
                break;

            default:
                AssertReleaseMsgFailed(("fileExistsInternal: Unknown return value (%Rrc)\n", rc));
                break;
        }
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

