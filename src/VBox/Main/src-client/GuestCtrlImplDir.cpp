/* $Id: */
/** @file
 * VirtualBox Guest Control - Guest directory handling.
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

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::directoryCreateInternal(IN_BSTR aDirectory,
                                       IN_BSTR aUsername, IN_BSTR aPassword,
                                       ULONG aMode, ULONG aFlags, int *pRC)
{
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

        rc = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_MKDIR).raw(), Bstr("Creating directory").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   NULL /* Progress */, NULL /* PID */);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}

/**
 * Creates a new directory handle ID and returns it. Returns VERR_TOO_MUCH_DATA
 * if no free handles left, otherwise VINF_SUCCESS (or some other IPRT error).
 *
 * @return IPRT status code.
 * @param puHandle             Pointer where the handle gets stored to.
 * @param uPID                 PID of guest process running the associated "vbox_ls".
 * @param pszDirectory         Directory the handle is assigned to.
 * @param pszFilter            Directory filter.  Optional.
 * @param uFlags               Directory open flags.
 *
 */
int Guest::directoryCreateHandle(ULONG *puHandle, ULONG uPID,
                                 const char *pszDirectory, const char *pszFilter, ULONG uFlags)
{
    AssertPtrReturn(puHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDirectory, VERR_INVALID_POINTER);
    /* pszFilter is optional. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_TOO_MUCH_DATA;
    for (uint32_t i = 0; i < UINT32_MAX - 1; i++)
    {
        /* Create a new context ID ... */
        uint32_t uHandleTry = ASMAtomicIncU32(&mNextDirectoryID);
        GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandleTry);
        if (it == mGuestDirectoryMap.end())
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
                    mGuestDirectoryMap[uHandleTry].mPID = uPID;
                    mGuestDirectoryMap[uHandleTry].mFlags = uFlags;
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

/**
 * Destroys a previously created directory handle and its
 * associated data.
 *
 * @return  IPRT status code.
 * @param   uHandle             Handle to destroy.
 */
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

/**
 * Gets the associated PID from a directory handle.
 *
 * @return  uint32_t            Associated PID, 0 if handle not found/invalid.
 * @param   uHandle             Directory handle to get PID for.
 */
uint32_t Guest::directoryGetPID(uint32_t uHandle)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
        return it->second.mPID;

    return 0;
}

/**
 * Checks whether a specified directory handle exists (is valid)
 * or not.
 *
 * @return  bool                True if handle exists, false if not.
 * @param   uHandle             Directory handle to check.
 */
bool Guest::directoryHandleExists(uint32_t uHandle)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
        return true;

    return false;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

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

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::directoryOpenInternal(IN_BSTR aDirectory, IN_BSTR aFilter,
                                     ULONG aFlags,
                                     IN_BSTR aUsername, IN_BSTR aPassword,
                                     ULONG *aHandle, int *pRC)
{
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

        ULONG uPID;
        rc = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_LS).raw(), Bstr("Opening directory").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   NULL /* Progress */, &uPID);
        if (SUCCEEDED(rc))
        {
            /* Assign new directory handle ID. */
            int vrc = directoryCreateHandle(aHandle, uPID,
                                            Utf8Directory.c_str(),
                                            Utf8Filter.isEmpty() ? NULL : Utf8Filter.c_str(),
                                            aFlags);
            if (RT_FAILURE(vrc))
                rc = setError(VBOX_E_IPRT_ERROR,
                              tr("Unable to create guest directory handle (%Rrc)"), vrc);
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

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

