/* $Id$ */
/** @file
 * VBoxManage - The 'guestproperty' command.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#include "VBoxManage.h"

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/EventQueue.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>

#ifdef USE_XPCOM_QUEUE
# include <sys/select.h>
# include <errno.h>
#endif

#ifdef RT_OS_DARWIN
# include <CoreFoundation/CFRunLoop.h>
#endif

using namespace com;

/**
 * IVirtualBoxCallback implementation for handling the GuestPropertyCallback in
 * relation to the "guestproperty wait" command.
 */
class GuestPropertyCallback :
  VBOX_SCRIPTABLE_IMPL(IVirtualBoxCallback)
{
public:
    GuestPropertyCallback(const char *pszPatterns, Guid aUuid)
        : mSignalled(false), mPatterns(pszPatterns), mUuid(aUuid)
    {
#ifndef VBOX_WITH_XPCOM
        refcnt = 0;
#endif
    }

    virtual ~GuestPropertyCallback()
    {
    }

#ifndef VBOX_WITH_XPCOM
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
#endif /* !VBOX_WITH_XPCOM */
    VBOX_SCRIPTABLE_DISPATCH_IMPL(IVirtualBoxCallback)

    NS_DECL_ISUPPORTS

    STDMETHOD(OnMachineStateChange)(IN_BSTR machineId,
                                    MachineState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnMachineDataChange)(IN_BSTR machineId)
    {
        return S_OK;
    }

    STDMETHOD(OnExtraDataCanChange)(IN_BSTR machineId, IN_BSTR key,
                                    IN_BSTR value, BSTR *error,
                                    BOOL *changeAllowed)
    {
        /* we never disagree */
        if (!changeAllowed)
            return E_INVALIDARG;
        *changeAllowed = TRUE;
        return S_OK;
    }

    STDMETHOD(OnExtraDataChange)(IN_BSTR machineId, IN_BSTR key,
                                 IN_BSTR value)
    {
        return S_OK;
    }

    STDMETHOD(OnMediumRegistered)(IN_BSTR mediaId,
                                  DeviceType_T mediaType, BOOL registered)
    {
        NOREF(mediaId);
        NOREF(mediaType);
        NOREF(registered);
        return S_OK;
    }

    STDMETHOD(OnMachineRegistered)(IN_BSTR machineId, BOOL registered)
    {
        return S_OK;
    }

     STDMETHOD(OnSessionStateChange)(IN_BSTR machineId,
                                    SessionState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotTaken)(IN_BSTR aMachineId,
                               IN_BSTR aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotDiscarded)(IN_BSTR aMachineId,
                                   IN_BSTR aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotChange)(IN_BSTR aMachineId,
                                IN_BSTR aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnGuestPropertyChange)(IN_BSTR machineId,
                                     IN_BSTR name, IN_BSTR value,
                                     IN_BSTR flags)
    {
        Utf8Str utf8Name(name);
        Guid uuid(machineId);
        if (   uuid == mUuid
            && RTStrSimplePatternMultiMatch(mPatterns, RTSTR_MAX,
                                            utf8Name.raw(), RTSTR_MAX, NULL))
        {
            RTPrintf("Name: %lS, value: %lS, flags: %lS\n", name, value, flags);
            ASMAtomicWriteBool(&mSignalled, true);
            com::EventQueue::getMainEventQueue()->interruptEventQueueProcessing();
        }
        return S_OK;
    }

    bool Signalled(void) const
    {
        return mSignalled;
    }

private:
    bool volatile mSignalled;
    const char *mPatterns;
    Guid mUuid;
#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(GuestPropertyCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestPropertyCallback, IVirtualBoxCallback)
#endif /* VBOX_WITH_XPCOM */

void usageGuestProperty(void)
{
    RTPrintf("VBoxManage guestproperty    get <vmname>|<uuid>\n"
             "                            <property> [--verbose]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    set <vmname>|<uuid>\n"
             "                            <property> [<value> [--flags <flags>]]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    enumerate <vmname>|<uuid>\n"
             "                            [--patterns <patterns>]\n"
             "\n");
    RTPrintf("VBoxManage guestproperty    wait <vmname>|<uuid> <patterns>\n"
             "                            [--timeout <milliseconds>] [--fail-on-timeout]\n"
             "\n");
}

static int handleGetGuestProperty(HandlerArg *a)
{
    HRESULT rc = S_OK;

    bool verbose = false;
    if (    a->argc == 3
        &&  (   !strcmp(a->argv[2], "--verbose")
             || !strcmp(a->argv[2], "-verbose")))
        verbose = true;
    else if (a->argc != 2)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED (a->virtualBox->OpenSession(a->session, uuid)))
            CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, uuid), 1);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        Bstr value;
        ULONG64 u64Timestamp;
        Bstr flags;
        CHECK_ERROR(machine, GetGuestProperty(Bstr(a->argv[1]), value.asOutParam(),
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

static int handleSetGuestProperty(HandlerArg *a)
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
    if (a->argc == 3)
        pszValue = a->argv[2];
    else if (a->argc == 4)
        usageOK = false;
    else if (a->argc == 5)
    {
        pszValue = a->argv[2];
        if (   strcmp(a->argv[3], "--flags")
            && strcmp(a->argv[3], "-flags"))
            usageOK = false;
        pszFlags = a->argv[4];
    }
    else if (a->argc != 2)
        usageOK = false;
    if (!usageOK)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");
    /* This is always needed. */
    pszName = a->argv[1];

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED (a->virtualBox->OpenSession(a->session, uuid)))
            CHECK_ERROR_RET (a->virtualBox, OpenExistingSession(a->session, uuid), 1);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        if (!pszValue && !pszFlags)
            CHECK_ERROR(machine, SetGuestPropertyValue(Bstr(pszName), NULL));
        else if (!pszFlags)
            CHECK_ERROR(machine, SetGuestPropertyValue(Bstr(pszName), Bstr(pszValue)));
        else
            CHECK_ERROR(machine, SetGuestProperty(Bstr(pszName), Bstr(pszValue), Bstr(pszFlags)));

        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, SaveSettings());

        a->session->Close();
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Enumerates the properties in the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int handleEnumGuestProperty(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (    a->argc < 1
        ||  a->argc == 2
        ||  (   a->argc > 3
             && strcmp(a->argv[1], "--patterns")
             && strcmp(a->argv[1], "-patterns")))
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

    /*
     * Pack the patterns
     */
    Utf8Str Utf8Patterns(a->argc > 2 ? a->argv[2] : "*");
    for (int i = 3; i < a->argc; ++i)
        Utf8Patterns = Utf8StrFmt ("%s,%s", Utf8Patterns.raw(), a->argv[i]);

    /*
     * Make the actual call to Main.
     */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    HRESULT rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* open a session for the VM - new or existing */
        if (FAILED(a->virtualBox->OpenSession(a->session, uuid)))
            CHECK_ERROR_RET (a->virtualBox, OpenExistingSession(a->session, uuid), 1);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

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
static int handleWaitGuestProperty(HandlerArg *a)
{
    /*
     * Handle arguments
     */
    bool        fFailOnTimeout = false;
    const char *pszPatterns    = NULL;
    uint32_t    cMsTimeout     = RT_INDEFINITE_WAIT;
    bool        usageOK        = true;
    if (a->argc < 2)
        usageOK = false;
    else
        pszPatterns = a->argv[1];
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    HRESULT rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (!machine)
        usageOK = false;
    for (int i = 2; usageOK && i < a->argc; ++i)
    {
        if (   !strcmp(a->argv[i], "--timeout")
            || !strcmp(a->argv[i], "-timeout"))
        {
            if (   i + 1 >= a->argc
                || RTStrToUInt32Full(a->argv[i + 1], 10, &cMsTimeout) != VINF_SUCCESS)
                usageOK = false;
            else
                ++i;
        }
        else if (!strcmp(a->argv[i], "--fail-on-timeout"))
            fFailOnTimeout = true;
        else
            usageOK = false;
    }
    if (!usageOK)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

    /*
     * Set up the callback and loop until signal or timeout.
     *
     * We do this in 1000 ms chunks to be on the safe side (there used to be
     * better reasons for it).
     */
    Bstr uuid;
    machine->COMGETTER(Id)(uuid.asOutParam());
    GuestPropertyCallback *cbImpl = new GuestPropertyCallback(pszPatterns, uuid);
    ComPtr<IVirtualBoxCallback> callback;
    rc = createCallbackWrapper((IVirtualBoxCallback *)cbImpl, callback.asOutParam());
    if (FAILED(rc))
    {
        RTPrintf("Error creating callback wrapper: %Rhrc\n", rc);
        return 1;
    }
    a->virtualBox->RegisterCallback(callback);
    uint64_t u64Started = RTTimeMilliTS();
    do
    {
        unsigned cMsWait;
        if (cMsTimeout == RT_INDEFINITE_WAIT)
            cMsWait = 1000;
        else
        {
            uint64_t cMsElapsed = RTTimeMilliTS() - u64Started;
            if (cMsElapsed >= cMsTimeout)
                break; /* timed out */
            cMsWait = RT_MIN(1000, cMsTimeout - (uint32_t)cMsElapsed);
        }
        int vrc = com::EventQueue::getMainEventQueue()->processEventQueue(cMsWait);
        if (    RT_FAILURE(vrc)
            &&  vrc != VERR_TIMEOUT)
        {
            RTPrintf("Error waiting for event: %Rrc\n", vrc);
            return 1;
        }
    } while (!cbImpl->Signalled());

    a->virtualBox->UnregisterCallback(callback);

    int rcRet = 0;
    if (!cbImpl->Signalled())
    {
        RTPrintf("Time out or interruption while waiting for a notification.\n");
        if (fFailOnTimeout)
            rcRet = 2;
    }
    return rcRet;
}

/**
 * Access the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
int handleGuestProperty(HandlerArg *a)
{
    HandlerArg arg = *a;
    arg.argc = a->argc - 1;
    arg.argv = a->argv + 1;

    if (a->argc == 0)
        return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");

    /* switch (cmd) */
    if (strcmp(a->argv[0], "get") == 0)
        return handleGetGuestProperty(&arg);
    if (strcmp(a->argv[0], "set") == 0)
        return handleSetGuestProperty(&arg);
    if (strcmp(a->argv[0], "enumerate") == 0)
        return handleEnumGuestProperty(&arg);
    if (strcmp(a->argv[0], "wait") == 0)
        return handleWaitGuestProperty(&arg);

    /* default: */
    return errorSyntax(USAGE_GUESTPROPERTY, "Incorrect parameters");
}
