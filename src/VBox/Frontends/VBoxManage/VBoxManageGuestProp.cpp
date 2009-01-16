/* $Id$ */
/** @file
 * VBoxManage - The 'guestproperty' command.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#if defined(VBOX_WITH_XPCOM) && !defined(RT_OS_DARWIN) && !defined(RT_OS_OS2)
# define USE_XPCOM_QUEUE
#endif

#include "VBoxManage.h"

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/log.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#ifdef USE_XPCOM_QUEUE
# include <sys/select.h>
#endif

using namespace com;

class GuestPropertyCallback : public IVirtualBoxCallback
{
public:
    GuestPropertyCallback(const char *pszPatterns, Guid aUuid)
        : mSignalled(false), mPatterns(pszPatterns), mUuid(aUuid)
    {
#if defined (RT_OS_WINDOWS)
        refcnt = 0;
#endif
    }

    virtual ~GuestPropertyCallback() {}

#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement(&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface)(REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IVirtualBoxCallback)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    STDMETHOD(OnMachineStateChange)(IN_GUID machineId,
                                    MachineState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnMachineDataChange)(IN_GUID machineId)
    {
        return S_OK;
    }

    STDMETHOD(OnExtraDataCanChange)(IN_GUID machineId, IN_BSTR key,
                                    IN_BSTR value, BSTR *error,
                                    BOOL *changeAllowed)
    {
        /* we never disagree */
        if (!changeAllowed)
            return E_INVALIDARG;
        *changeAllowed = TRUE;
        return S_OK;
    }

    STDMETHOD(OnExtraDataChange)(IN_GUID machineId, IN_BSTR key,
                                 IN_BSTR value)
    {
        return S_OK;
    }

    STDMETHOD(OnMediaRegistered) (IN_GUID mediaId,
                                  DeviceType_T mediaType, BOOL registered)
    {
        NOREF (mediaId);
        NOREF (mediaType);
        NOREF (registered);
        return S_OK;
    }

    STDMETHOD(OnMachineRegistered)(IN_GUID machineId, BOOL registered)
    {
        return S_OK;
    }

     STDMETHOD(OnSessionStateChange)(IN_GUID machineId,
                                    SessionState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotTaken) (IN_GUID aMachineId,
                                IN_GUID aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotDiscarded) (IN_GUID aMachineId,
                                    IN_GUID aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotChange) (IN_GUID aMachineId,
                                 IN_GUID aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnGuestPropertyChange)(IN_GUID machineId,
                                     IN_BSTR name, IN_BSTR value,
                                     IN_BSTR flags)
    {
        int rc = S_OK;
        Utf8Str utf8Name (name);
        Guid uuid(machineId);
        if (utf8Name.isNull())
            rc = E_OUTOFMEMORY;
        if (   SUCCEEDED (rc)
            && uuid == mUuid
            && RTStrSimplePatternMultiMatch (mPatterns, RTSTR_MAX,
                                             utf8Name.raw(), RTSTR_MAX, NULL))
        {
            RTPrintf ("Name: %lS, value: %lS, flags: %lS\n", name, value,
                      flags);
            mSignalled = true;
        }
        return rc;
    }

    bool Signalled(void) { return mSignalled; }

private:
    bool mSignalled;
    const char *mPatterns;
    Guid mUuid;
#ifdef RT_OS_WINDOWS
    long refcnt;
#endif

};

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(GuestPropertyCallback)
NS_IMPL_ISUPPORTS1_CI(GuestPropertyCallback, IVirtualBoxCallback)
#endif /* VBOX_WITH_XPCOM */

void usageGuestProperty(void)
{
    RTPrintf("VBoxManage guestproperty    get <vmname>|<uuid>\n"
             "                            <property> [-verbose]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    set <vmname>|<uuid>\n"
             "                            <property> [<value> [-flags <flags>]]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    enumerate <vmname>|<uuid>\n"
             "                            [-patterns <patterns>]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    wait <vmname>|<uuid> <patterns>\n"
             "                            [--timeout <timeout>]\n"
             "\n");
}

static int handleGetGuestProperty(int argc, char *argv[],
                                  ComPtr<IVirtualBox> aVirtualBox,
                                  ComPtr<ISession> aSession)
{
    HRESULT rc = S_OK;

    bool verbose = false;
    if ((3 == argc) && (0 == strcmp(argv[2], "-verbose")))
        verbose = true;
    else if (argc != 2)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = aVirtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED (aVirtualBox->OpenSession(aSession, uuid)))
            CHECK_ERROR_RET (aVirtualBox, OpenExistingSession(aSession, uuid), 1);

        /* get the mutable session machine */
        aSession->COMGETTER(Machine)(machine.asOutParam());

        Bstr value;
        uint64_t u64Timestamp;
        Bstr flags;
        CHECK_ERROR(machine, GetGuestProperty(Bstr(argv[1]), value.asOutParam(),
                    &u64Timestamp, flags.asOutParam()));
        if (!value)
            RTPrintf("No value set!\n");
        if (value)
            RTPrintf("Value: %lS\n", value.raw());
        if (value && verbose)
        {
            RTPrintf("Timestamp: %lld\n", u64Timestamp);
            RTPrintf("Flags: %lS\n", flags.raw());
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetGuestProperty(int argc, char *argv[],
                                  ComPtr<IVirtualBox> aVirtualBox,
                                  ComPtr<ISession> aSession)
{
    HRESULT rc = S_OK;

/*
 * Check the syntax.  We can deduce the correct syntax from the number of
 * arguments.
 */
    bool usageOK = true;
    const char *pszName = NULL;
    const char *pszValue = NULL;
    const char *pszFlags = NULL;
    if (3 == argc)
    {
        pszValue = argv[2];
    }
    else if (4 == argc)
        usageOK = false;
    else if (5 == argc)
    {
        pszValue = argv[2];
        if (strcmp(argv[3], "-flags") != 0)
            usageOK = false;
        pszFlags = argv[4];
    }
    else if (argc != 2)
        usageOK = false;
    if (!usageOK)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");
    /* This is always needed. */
    pszName = argv[1];

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = aVirtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED (aVirtualBox->OpenSession(aSession, uuid)))
            CHECK_ERROR_RET (aVirtualBox, OpenExistingSession(aSession, uuid), 1);

        /* get the mutable session machine */
        aSession->COMGETTER(Machine)(machine.asOutParam());

        if ((NULL == pszValue) && (NULL == pszFlags))
            CHECK_ERROR(machine, SetGuestPropertyValue(Bstr(pszName), NULL));
        else if (NULL == pszFlags)
            CHECK_ERROR(machine, SetGuestPropertyValue(Bstr(pszName), Bstr(pszValue)));
        else
            CHECK_ERROR(machine, SetGuestProperty(Bstr(pszName), Bstr(pszValue), Bstr(pszFlags)));

        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, SaveSettings());

        aSession->Close();
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Enumerates the properties in the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int handleEnumGuestProperty(int argc, char *argv[],
                                   ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
/*
 * Check the syntax.  We can deduce the correct syntax from the number of
 * arguments.
 */
    if ((argc < 1) || (2 == argc) ||
        ((argc > 3) && strcmp(argv[1], "-patterns") != 0))
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

/*
 * Pack the patterns
 */
    Utf8Str Utf8Patterns(argc > 2 ? argv[2] : "*");
    for (ssize_t i = 3; i < argc; ++i)
        Utf8Patterns = Utf8StrFmt ("%s,%s", Utf8Patterns.raw(), argv[i]);

/*
 * Make the actual call to Main.
 */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    HRESULT rc = aVirtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED (aVirtualBox->OpenSession(aSession, uuid)))
            CHECK_ERROR_RET (aVirtualBox, OpenExistingSession(aSession, uuid), 1);

        /* get the mutable session machine */
        aSession->COMGETTER(Machine)(machine.asOutParam());

        com::SafeArray <BSTR> names;
        com::SafeArray <BSTR> values;
        com::SafeArray <ULONG64> timestamps;
        com::SafeArray <BSTR> flags;
        CHECK_ERROR(machine, EnumerateGuestProperties(Bstr(Utf8Patterns),
                                                      ComSafeArrayAsOutParam(names),
                                                      ComSafeArrayAsOutParam(values),
                                                      ComSafeArrayAsOutParam(timestamps),
                                                      ComSafeArrayAsOutParam(flags)));
        if (SUCCEEDED(rc))
        {
            if (names.size() == 0)
                RTPrintf("No properties found.\n");
            for (unsigned i = 0; i < names.size(); ++i)
                RTPrintf("Name: %lS, value: %lS, timestamp: %lld, flags: %lS\n",
                         names[i], values[i], timestamps[i], flags[i]);
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Enumerates the properties in the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int handleWaitGuestProperty(int argc, char *argv[],
                                   ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{

/*
 * Handle arguments
 */
    const char *pszPatterns = NULL;
    uint32_t u32Timeout = RT_INDEFINITE_WAIT;
    bool usageOK = true;
    if (argc < 2)
        usageOK = false;
    else
        pszPatterns = argv[1];
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    HRESULT rc = aVirtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (!machine)
        usageOK = false;
    for (int i = 2; usageOK && i < argc; ++i)
    {
        if (strcmp(argv[i], "-timeout") == 0)
        {
            if (   i + 1 >= argc
                || RTStrToUInt32Full(argv[i + 1], 10, &u32Timeout)
                       != VINF_SUCCESS
               )
                usageOK = false;
            else
                ++i;
        }
        else
            usageOK = false;
    }
    if (!usageOK)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

/*
 * Set up the callback and wait.
 */
    Guid uuid;
    machine->COMGETTER(Id)(uuid.asOutParam());
    GuestPropertyCallback *callback = new GuestPropertyCallback(pszPatterns, uuid);
    callback->AddRef();
    aVirtualBox->RegisterCallback (callback);
    bool stop = false;
#ifdef USE_XPCOM_QUEUE
    int max_fd = g_pEventQ->GetEventQueueSelectFD();
#endif
    for (; !stop && u32Timeout > 0; u32Timeout -= RT_MIN(u32Timeout, 1000))
    {
#ifdef USE_XPCOM_QUEUE
        int prc;
        fd_set fdset;
        struct timeval tv;
        FD_ZERO (&fdset);
        FD_SET(max_fd, &fdset);
        tv.tv_sec = RT_MIN(u32Timeout, 1000);
        tv.tv_usec = u32Timeout > 1000 ? 0 : u32Timeout * 1000;
        RTTIMESPEC TimeNow;
        uint64_t u64Time = RTTimeSpecGetMilli(RTTimeNow(&TimeNow));
        prc = select(max_fd + 1, &fdset, NULL, NULL, &tv);
        if (prc == -1)
        {
            RTPrintf("Error waiting for event.\n");
            stop = true;
        }
        else if (prc != 0)
        {
            uint64_t u64NextTime = RTTimeSpecGetMilli(RTTimeNow(&TimeNow));
            u32Timeout += (uint32_t)(u64Time + 1000 - u64NextTime);
            g_pEventQ->ProcessPendingEvents();
            if (callback->Signalled())
                stop = true;
        }
#else
        /** @todo Use a semaphore.  But I currently don't have a Windows system
         * running to test on. */
        RTThreadSleep(RT_MIN(1000, u32Timeout));
        if (callback->Signalled())
            stop = true;
#endif /* USE_XPCOM_QUEUE */
    }

/*
 * Clean up the callback.
 */
    aVirtualBox->UnregisterCallback (callback);
    if (!callback->Signalled())
        RTPrintf("Time out or interruption while waiting for a notification.\n");
    callback->Release ();

/*
 * Done.
 */
    return 0;
}

/**
 * Access the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
int handleGuestProperty(int argc, char *argv[],
                        ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    if (0 == argc)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");
    if (0 == strcmp(argv[0], "get"))
        return handleGetGuestProperty(argc - 1, argv + 1, aVirtualBox, aSession);
    else if (0 == strcmp(argv[0], "set"))
        return handleSetGuestProperty(argc - 1, argv + 1, aVirtualBox, aSession);
    else if (0 == strcmp(argv[0], "enumerate"))
        return handleEnumGuestProperty(argc - 1, argv + 1, aVirtualBox, aSession);
    else if (0 == strcmp(argv[0], "wait"))
        return handleWaitGuestProperty(argc - 1, argv + 1, aVirtualBox, aSession);
    /* else */
    return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");
}

