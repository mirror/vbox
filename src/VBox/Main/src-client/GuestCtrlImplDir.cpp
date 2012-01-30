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
#include "GuestDirEntryImpl.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <iprt/path.h>

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
                                   ExecuteProcessFlag_None,
                                   NULL, NULL,
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
 * @param puHandle             Pointer where the handle gets stored to. Optional.
 * @param uPID                 PID of guest process running the associated "vbox_ls".
 * @param aDirectory           Directory the handle is assigned to.
 * @param aFilter              Directory filter.  Optional.
 * @param uFlags               Directory open flags.
 *
 */
int Guest::directoryCreateHandle(ULONG *puHandle, ULONG uPID,
                                 IN_BSTR aDirectory, IN_BSTR aFilter, ULONG uFlags)
{
    AssertReturn(uPID, VERR_INVALID_PARAMETER);
    CheckComArgStrNotEmptyOrNull(aDirectory);
    /* aFilter is optional. */
    /* uFlags are optional. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_TOO_MUCH_DATA;
    for (uint32_t i = 0; i < UINT32_MAX - 1; i++)
    {
        /* Create a new context ID ... */
        uint32_t uHandleTry = ASMAtomicIncU32(&mNextDirectoryID);
        GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandleTry);
        if (it == mGuestDirectoryMap.end()) /* We found a free slot ... */
        {
            mGuestDirectoryMap[uHandleTry].mDirectory = aDirectory;
            mGuestDirectoryMap[uHandleTry].mFilter = aFilter;
            mGuestDirectoryMap[uHandleTry].mPID = uPID;
            mGuestDirectoryMap[uHandleTry].mFlags = uFlags;
            Assert(mGuestDirectoryMap.size());

            rc = VINF_SUCCESS;

            if (puHandle)
                *puHandle = uHandleTry;
            break;
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
        /* Destroy raw guest stream buffer - not used
         * anymore. */
        it->second.mStream.Destroy();

        /* Remove callback context (not used anymore). */
        mGuestDirectoryMap.erase(it);
    }
}

#if 0
STDMETHODIMP Guest::DirectoryExists(IN_BSTR aDirectory, IN_BSTR aUsername, IN_BSTR aPassword, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUsername) == NULL || *(aUsername) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    return directoryExistsInternal(aDirectory,
                                   aUsername, aPassword, aExists);
#endif
}
#endif

#ifdef VBOX_WITH_GUEST_CONTROL
HRESULT Guest::directoryExistsInternal(IN_BSTR aDirectory, IN_BSTR aUsername, IN_BSTR aPassword, BOOL *aExists)
{
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aDirectory);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc;
    HRESULT hr = directoryQueryInfoInternal(aDirectory,
                                            aUsername, aPassword,
                                            NULL /* No RTFSOBJINFO needed */,
                                            RTFSOBJATTRADD_NOTHING, &rc);
    if (SUCCEEDED(hr))
    {
        switch (rc)
        {
            case VINF_SUCCESS:
                *aExists = TRUE;
                break;

            case VERR_PATH_NOT_FOUND:
                *aExists = FALSE;
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("Unable to query directory existence (%Rrc)"), rc);
                break;
        }
    }
    return hr;
}
#endif

/**
 * Gets the associated PID from a directory handle.
 *
 * @return  uint32_t                Associated PID, 0 if handle not found/invalid.
 * @param   uHandle                 Directory handle to get PID for.
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
 * Returns the next directory entry of an open guest directory.
 * Returns VERR_NO_DATA if no more entries available or VERR_NOT_FOUND
 * if directory handle is invalid.
 *
 * @return  IPRT status code.
 * @param   uHandle                 Directory handle to get entry for.
 * @param   streamBlock             Reference that receives the next stream block data.
 */
int Guest::directoryGetNextEntry(uint32_t uHandle, GuestProcessStreamBlock &streamBlock)
{
    // LOCK DOES NOT WORK HERE!?
    //AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestDirectoryMapIter it = mGuestDirectoryMap.find(uHandle);
    if (it != mGuestDirectoryMap.end())
    {
#ifdef DEBUG
        it->second.mStream.Dump("/tmp/stream.txt");
#endif
        return executeStreamGetNextBlock(it->second.mPID,
                                         ProcessOutputFlag_None /* StdOut */,
                                         it->second.mStream, streamBlock);
    }

    return VERR_NOT_FOUND;
}

/**
 * Checks whether a specified directory handle exists (is valid)
 * or not.
 *
 * @return  bool                    True if handle exists, false if not.
 * @param   uHandle                 Directory handle to check.
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

    HRESULT hr = S_OK;
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
        if (Utf8Filter.isEmpty())
            pszDirectoryFinal = RTStrDup(Utf8Directory.c_str());
        else
            pszDirectoryFinal = RTPathJoinA(Utf8Directory.c_str(), Utf8Filter.c_str());
        if (!pszDirectoryFinal)
            return setError(E_OUTOFMEMORY, tr("Out of memory while allocating final directory"));

        args.push_back(Bstr(pszDirectoryFinal).raw());  /* The directory we want to open. */
        RTStrFree(pszDirectoryFinal);

        ULONG uPID;
        /* We only start the directory listing and requesting stdout data but don't get its
         * data here; this is done in sequential IGuest::DirectoryRead calls then. */
        hr = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_LS).raw(), Bstr("Opening directory").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   ExecuteProcessFlag_WaitForStdOut,
                                   NULL, NULL,
                                   NULL /* Progress */, &uPID);
        if (SUCCEEDED(hr))
        {
            /* Assign new directory handle ID. */
            ULONG uHandleNew;
            int rc = directoryCreateHandle(&uHandleNew, uPID,
                                           aDirectory, aFilter, aFlags);
            if (RT_SUCCESS(rc))
            {
                *aHandle = uHandleNew;
            }
            else
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Unable to create guest directory handle (%Rrc)"), rc);
            if (pRC)
                *pRC = rc;
        }
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}

HRESULT Guest::directoryQueryInfoInternal(IN_BSTR aDirectory,
                                          IN_BSTR aUsername, IN_BSTR aPassword,
                                          PRTFSOBJINFO aObjInfo, RTFSOBJATTRADD enmAddAttribs,
                                          int *pRC)
{
    using namespace guestControl;

    /** @todo Search directory cache first? */

    CheckComArgStrNotEmptyOrNull(aDirectory);
    /* aUsername is optional. */
    /* aPassword is optional. */
    /* aObjInfo is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;
    try
    {
        Utf8Str Utf8Dir(aDirectory);
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
        args.push_back(Bstr(Utf8Dir).raw());

        /*
         * Execute guest process.
         */
        ULONG uPID;
        GuestCtrlStreamObjects stdOut;
        hr = executeAndWaitForTool(Bstr(VBOXSERVICE_TOOL_STAT).raw(), Bstr("Querying directory information").raw(),
                                   ComSafeArrayAsInParam(args),
                                   ComSafeArrayAsInParam(env),
                                   aUsername, aPassword,
                                   ExecuteProcessFlag_WaitForStdOut,
                                   &stdOut, NULL /* stdErr */,
                                   NULL /* Progress */, &uPID);
        if (SUCCEEDED(hr))
        {
            int rc = VINF_SUCCESS;
            if (stdOut.size())
            {
                const char *pszFsType = stdOut[0].GetString("ftype");
                if (!pszFsType) /* Attribute missing? */
                     rc = VERR_PATH_NOT_FOUND;
                if (   RT_SUCCESS(rc)
                    && strcmp(pszFsType, "d")) /* Directory? */
                {
                     rc = VERR_PATH_NOT_FOUND;
                     /* This is not critical for Main, so don't set hr --
                      * we will take care of rc then. */
                }
                if (   RT_SUCCESS(rc)
                    && aObjInfo) /* Do we want object details? */
                {
                    rc = executeStreamQueryFsObjInfo(aDirectory, stdOut[0],
                                                     aObjInfo, enmAddAttribs);
                }
            }
            else
                rc = VERR_NO_DATA;

            if (pRC)
                *pRC = rc;
        }
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::DirectoryRead(ULONG aHandle, IGuestDirEntry **aDirEntry)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgOutPointerValid(aDirEntry);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;
    try
    {
        GuestProcessStreamBlock streamBlock;
        int rc = directoryGetNextEntry(aHandle, streamBlock);
        if (RT_SUCCESS(rc))
        {
            if (streamBlock.GetCount())
            {
                ComObjPtr <GuestDirEntry> pDirEntry;
                hr = pDirEntry.createObject();
                ComAssertComRC(hr);

                hr = pDirEntry->init(this, streamBlock);
                if (SUCCEEDED(hr))
                {
                    pDirEntry.queryInterfaceTo(aDirEntry);
                }
                else
                {
#ifdef DEBUG
                    streamBlock.Dump();
#endif
                    hr = VBOX_E_FILE_ERROR;
                }
            }
            else
            {
                /* No more directory entries to read. That's fine. */
                hr = E_ABORT; /** @todo Find/define a better rc! */
            }
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Failed getting next directory entry (%Rrc)"), rc);
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
#endif
}

