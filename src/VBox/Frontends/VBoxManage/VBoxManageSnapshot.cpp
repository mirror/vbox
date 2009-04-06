/* $Id$ */
/** @file
 * VBoxManage - The 'snapshot' command.
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
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>

#include <VBox/com/VirtualBox.h>

#include <iprt/stream.h>
#include <iprt/getopt.h>

#include "VBoxManage.h"
using namespace com;

int handleSnapshot(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a VM and a command */
    if (a->argc < 2)
        return errorSyntax(USAGE_SNAPSHOT, "Not enough parameters");

    /* the first argument must be the VM */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Guid guid;
    machine->COMGETTER(Id)(guid.asOutParam());

    do
    {
        /* we have to open a session for this task. First try an existing session */
        rc = a->virtualBox->OpenExistingSession(a->session, guid);
        if (FAILED(rc))
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));

        /* switch based on the command */
        if (!strcmp(a->argv[1], "take"))
        {
            /* there must be a name */
            if (a->argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }
            Bstr name(a->argv[2]);
            if ((a->argc > 3) && (   (a->argc != 5)
                                  || (   strcmp(a->argv[3], "--description")
                                      && strcmp(a->argv[3], "-description")
                                      && strcmp(a->argv[3], "-desc"))))
            {
                errorSyntax(USAGE_SNAPSHOT, "Incorrect description format");
                rc = E_FAIL;
                break;
            }
            Bstr desc;
            if (a->argc == 5)
                desc = a->argv[4];
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, TakeSnapshot(name, desc, progress.asOutParam()));

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to take snapshot. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to take snapshot. No error message available!\n");
            }
        }
        else if (!strcmp(a->argv[1], "discard"))
        {
            /* exactly one parameter: snapshot name */
            if (a->argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
            }

            snapshot->COMGETTER(Id)(guid.asOutParam());

            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, DiscardSnapshot(guid, progress.asOutParam()));

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to discard snapshot. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to discard snapshot. No error message available!\n");
            }
        }
        else if (!strcmp(a->argv[1], "discardcurrent"))
        {
            if (   (a->argc != 3)
                || (   strcmp(a->argv[2], "--state")
                    && strcmp(a->argv[2], "-state")
                    && strcmp(a->argv[2], "--all")
                    && strcmp(a->argv[2], "-all")))
            {
                errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[2]).raw());
                rc = E_FAIL;
                break;
            }
            bool fAll = false;
            if (   !strcmp(a->argv[2], "--all")
                || !strcmp(a->argv[2], "-all"))
                fAll = true;

            ComPtr<IProgress> progress;

            if (fAll)
            {
                CHECK_ERROR_BREAK(console, DiscardCurrentSnapshotAndState(progress.asOutParam()));
            }
            else
            {
                CHECK_ERROR_BREAK(console, DiscardCurrentState(progress.asOutParam()));
            }

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to discard. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to discard. No error message available!\n");
            }

        }
        else if (!strcmp(a->argv[1], "edit"))
        {
            if (a->argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            if (   !strcmp(a->argv[2], "--current")
                || !strcmp(a->argv[2], "-current"))
            {
                CHECK_ERROR_BREAK(machine, COMGETTER(CurrentSnapshot)(snapshot.asOutParam()));
            }
            else
            {
                /* assume it's a UUID */
                Guid guid(a->argv[2]);
                if (!guid.isEmpty())
                {
                    CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
                }
                else
                {
                    /* then it must be a name */
                    CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
                }
            }

            /* parse options */
            for (int i = 3; i < a->argc; i++)
            {
                if (   !strcmp(a->argv[i], "--name")
                    || !strcmp(a->argv[i], "-name")
                    || !strcmp(a->argv[i], "-newname"))
                {
                    if (a->argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", a->argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Name)(Bstr(a->argv[i]));
                }
                else if (   !strcmp(a->argv[i], "--description")
                         || !strcmp(a->argv[i], "-description")
                         || !strcmp(a->argv[i], "-newdesc"))
                {
                    if (a->argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", a->argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Description)(Bstr(a->argv[i]));
                }
                else
                {
                    errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
                    rc = E_FAIL;
                    break;
                }
            }

        }
        else if (!strcmp(a->argv[1], "showvminfo"))
        {
            /* exactly one parameter: snapshot name */
            if (a->argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
            }

            /* get the machine of the given snapshot */
            ComPtr<IMachine> machine;
            snapshot->COMGETTER(Machine)(machine.asOutParam());
            showVMInfo(a->virtualBox, machine, VMINFO_NONE, console);
        }
        else
        {
            errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[1]).raw());
            rc = E_FAIL;
        }
    } while (0);

    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

