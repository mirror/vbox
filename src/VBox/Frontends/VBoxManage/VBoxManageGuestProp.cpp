/* $Id$ */
/** @file
 * VBoxManage - Implementation of guestproperty command.
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxManage.h"

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

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

DECLARE_TRANSLATION_CONTEXT(GuestProp);


static RTEXITCODE handleGetGuestProperty(HandlerArg *a)
{
    HRESULT hrc = S_OK;

    setCurrentSubcommand(HELP_SCOPE_GUESTPROPERTY_GET);

    bool verbose = false;
    if (    a->argc == 3
        &&  (   !strcmp(a->argv[2], "--verbose")
             || !strcmp(a->argv[2], "-verbose")))
        verbose = true;
    else if (a->argc != 2)
        return errorSyntax(GuestProp::tr("Incorrect parameters"));

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        /* open a session for the VM - new or existing */
        CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        Bstr value;
        LONG64 i64Timestamp;
        Bstr flags;
        CHECK_ERROR(machine, GetGuestProperty(Bstr(a->argv[1]).raw(),
                                              value.asOutParam(),
                                              &i64Timestamp, flags.asOutParam()));
        if (value.isEmpty())
            RTPrintf(GuestProp::tr("No value set!\n"));
        else
            RTPrintf(GuestProp::tr("Value: %ls\n"), value.raw());
        if (!value.isEmpty() && verbose)
        {
            RTPrintf(GuestProp::tr("Timestamp: %lld\n"), i64Timestamp);
            RTPrintf(GuestProp::tr("Flags: %ls\n"), flags.raw());
        }
    }
    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE handleSetGuestProperty(HandlerArg *a)
{
    HRESULT hrc = S_OK;

    setCurrentSubcommand(HELP_SCOPE_GUESTPROPERTY_SET);

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
        return errorSyntax(GuestProp::tr("Incorrect parameters"));
    /* This is always needed. */
    pszName = a->argv[1];

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        /* open a session for the VM - new or existing */
        CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        if (!pszFlags)
            CHECK_ERROR(machine, SetGuestPropertyValue(Bstr(pszName).raw(),
                                                       Bstr(pszValue).raw()));
        else
            CHECK_ERROR(machine, SetGuestProperty(Bstr(pszName).raw(),
                                                  Bstr(pszValue).raw(),
                                                  Bstr(pszFlags).raw()));

        if (SUCCEEDED(hrc))
            CHECK_ERROR(machine, SaveSettings());

        a->session->UnlockMachine();
    }
    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE handleDeleteGuestProperty(HandlerArg *a)
{
    HRESULT hrc = S_OK;

    setCurrentSubcommand(HELP_SCOPE_GUESTPROPERTY_UNSET);

    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    bool usageOK = true;
    const char *pszName = NULL;
    if (a->argc != 2)
        usageOK = false;
    if (!usageOK)
        return errorSyntax(GuestProp::tr("Incorrect parameters"));
    /* This is always needed. */
    pszName = a->argv[1];

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        /* open a session for the VM - new or existing */
        CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        CHECK_ERROR(machine, DeleteGuestProperty(Bstr(pszName).raw()));

        if (SUCCEEDED(hrc))
            CHECK_ERROR(machine, SaveSettings());

        a->session->UnlockMachine();
    }
    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Enumerates the properties in the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static RTEXITCODE handleEnumGuestProperty(HandlerArg *a)
{
    setCurrentSubcommand(HELP_SCOPE_GUESTPROPERTY_ENUMERATE);

    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (    a->argc < 1
        ||  a->argc == 2
        ||  (   a->argc > 3
             && strcmp(a->argv[1], "--patterns")
             && strcmp(a->argv[1], "-patterns")))
        return errorSyntax(GuestProp::tr("Incorrect parameters"));

    /*
     * Pack the patterns
     */
    Utf8Str strPatterns(a->argc > 2 ? a->argv[2] : "");
    for (int i = 3; i < a->argc; ++i)
        strPatterns = Utf8StrFmt ("%s,%s", strPatterns.c_str(), a->argv[i]);

    /*
     * Make the actual call to Main.
     */
    ComPtr<IMachine> machine;
    HRESULT hrc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        /* open a session for the VM - new or existing */
        CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        com::SafeArray<BSTR> names;
        com::SafeArray<BSTR> values;
        com::SafeArray<LONG64> timestamps;
        com::SafeArray<BSTR> flags;
        CHECK_ERROR(machine, EnumerateGuestProperties(Bstr(strPatterns).raw(),
                                                      ComSafeArrayAsOutParam(names),
                                                      ComSafeArrayAsOutParam(values),
                                                      ComSafeArrayAsOutParam(timestamps),
                                                      ComSafeArrayAsOutParam(flags)));
        if (SUCCEEDED(hrc))
        {
            if (names.size() == 0)
                RTPrintf(GuestProp::tr("No properties found.\n"));
            for (unsigned i = 0; i < names.size(); ++i)
                RTPrintf(GuestProp::tr("Name: %ls, value: %ls, timestamp: %lld, flags: %ls\n"),
                         names[i], values[i], timestamps[i], flags[i]);
        }
    }
    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Enumerates the properties in the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static RTEXITCODE handleWaitGuestProperty(HandlerArg *a)
{
    setCurrentSubcommand(HELP_SCOPE_GUESTPROPERTY_WAIT);

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
    HRESULT hrc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
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
        return errorSyntax(GuestProp::tr("Incorrect parameters"));

    /*
     * Set up the event listener and wait until found match or timeout.
     */
    Bstr aMachStrGuid;
    machine->COMGETTER(Id)(aMachStrGuid.asOutParam());
    Guid aMachGuid(aMachStrGuid);
    ComPtr<IEventSource> es;
    CHECK_ERROR(a->virtualBox, COMGETTER(EventSource)(es.asOutParam()));
    ComPtr<IEventListener> listener;
    CHECK_ERROR(es, CreateListener(listener.asOutParam()));
    com::SafeArray <VBoxEventType_T> eventTypes(1);
    eventTypes.push_back(VBoxEventType_OnGuestPropertyChanged);
    CHECK_ERROR(es, RegisterListener(listener, ComSafeArrayAsInParam(eventTypes), false));

    uint64_t u64Started = RTTimeMilliTS();
    bool fSignalled = false;
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

        ComPtr<IEvent> ev;
        hrc = es->GetEvent(listener, cMsWait, ev.asOutParam());
        if (ev) /** @todo r=andy Why not using SUCCEEDED(hrc) here? */
        {
            VBoxEventType_T aType;
            hrc = ev->COMGETTER(Type)(&aType);
            switch (aType)
            {
                case VBoxEventType_OnGuestPropertyChanged:
                {
                    ComPtr<IGuestPropertyChangedEvent> gpcev = ev;
                    Assert(gpcev);
                    Bstr aNextStrGuid;
                    gpcev->COMGETTER(MachineId)(aNextStrGuid.asOutParam());
                    if (aMachGuid != Guid(aNextStrGuid))
                        continue;
                    Bstr aNextName;
                    gpcev->COMGETTER(Name)(aNextName.asOutParam());
                    if (RTStrSimplePatternMultiMatch(pszPatterns, RTSTR_MAX,
                                                     Utf8Str(aNextName).c_str(), RTSTR_MAX, NULL))
                    {
                        Bstr aNextValue, aNextFlags;
                        BOOL aNextWasDeleted;
                        gpcev->COMGETTER(Value)(aNextValue.asOutParam());
                        gpcev->COMGETTER(Flags)(aNextFlags.asOutParam());
                        gpcev->COMGETTER(FWasDeleted)(&aNextWasDeleted);
                        if (aNextWasDeleted)
                            RTPrintf(GuestProp::tr("Property %ls was deleted\n"), aNextName.raw());
                        else
                            RTPrintf(GuestProp::tr("Name: %ls, value: %ls, flags: %ls\n"),
                                     aNextName.raw(), aNextValue.raw(), aNextFlags.raw());
                        fSignalled = true;
                    }
                    break;
                }
                default:
                     AssertFailed();
            }
        }
    } while (!fSignalled);

    es->UnregisterListener(listener);

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (!fSignalled)
    {
        RTMsgError(GuestProp::tr("Time out or interruption while waiting for a notification."));
        if (fFailOnTimeout)
            /* Hysterical rasins: We always returned 2 here, which now translates to syntax error... Which is bad. */
            rcExit = RTEXITCODE_SYNTAX;
    }
    return rcExit;
}

/**
 * Access the guest property store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
RTEXITCODE handleGuestProperty(HandlerArg *a)
{
    HandlerArg arg = *a;
    arg.argc = a->argc - 1;
    arg.argv = a->argv + 1;

    /** @todo This command does not follow the syntax where the <uuid|vmname>
     * comes between the command and subcommand.  The commands controlvm,
     * snapshot and debugvm puts it between.
     */

    if (a->argc == 0)
        return errorSyntax(GuestProp::tr("Incorrect parameters"));

    /* switch (cmd) */
    if (strcmp(a->argv[0], "get") == 0)
        return handleGetGuestProperty(&arg);
    if (strcmp(a->argv[0], "set") == 0)
        return handleSetGuestProperty(&arg);
    if (strcmp(a->argv[0], "delete") == 0 || strcmp(a->argv[0], "unset") == 0)
        return handleDeleteGuestProperty(&arg);
    if (strcmp(a->argv[0], "enumerate") == 0)
        return handleEnumGuestProperty(&arg);
    if (strcmp(a->argv[0], "wait") == 0)
        return handleWaitGuestProperty(&arg);

    /* default: */
    return errorSyntax(GuestProp::tr("Incorrect parameters"));
}
