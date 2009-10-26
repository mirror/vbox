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
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>

#include <iprt/stream.h>
#include <iprt/getopt.h>

#include "VBoxManage.h"
using namespace com;

/**
 * Helper function used with "VBoxManage snapshot ... dump".
 * @param pMedium
 * @param pThisSnapshot
 * @param pCurrentSnapshot
 * @param uMediumLevel
 * @param uSnapshotLevel
 * @return
 */
bool FindAndPrintSnapshotUsingMedium(ComPtr<IMedium> &pMedium,
                                     ComPtr<ISnapshot> &pThisSnapshot,
                                     ComPtr<ISnapshot> &pCurrentSnapshot,
                                     uint32_t uMediumLevel,
                                     uint32_t uSnapshotLevel)
{
    HRESULT rc;

    do
    {
        // get snapshot machine so we can figure out which diff image this created
        ComPtr<IMachine> pSnapshotMachine;
        CHECK_ERROR_BREAK(pThisSnapshot, COMGETTER(Machine)(pSnapshotMachine.asOutParam()));

        // get media attachments
        SafeIfaceArray<IMediumAttachment> aAttachments;
        CHECK_ERROR_BREAK(pSnapshotMachine, COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(aAttachments)));

        bool fFound = false;

        for (uint32_t i = 0;
             i < aAttachments.size();
             ++i)
        {
            ComPtr<IMediumAttachment> pAttach(aAttachments[i]);
            DeviceType_T type;
            CHECK_ERROR_BREAK(pAttach, COMGETTER(Type)(&type));
            if (type == DeviceType_HardDisk)
            {
                ComPtr<IMedium> pMediumInSnapshot;
                CHECK_ERROR_BREAK(pAttach, COMGETTER(Medium)(pMediumInSnapshot.asOutParam()));

                if (pMediumInSnapshot == pMedium)
                {
                    // get snapshot name
                    Bstr bstrSnapshotName;
                    CHECK_ERROR_BREAK(pThisSnapshot, COMGETTER(Name)(bstrSnapshotName.asOutParam()));

                    RTPrintf("%*s  \"%ls\"\n",
                             50 + uSnapshotLevel * 2, "",            // indent
                             bstrSnapshotName.raw());
                    return true;        // found
                }
            }
        }

        if (!fFound)
        {
            SafeIfaceArray<ISnapshot> aSnapshots;
            CHECK_ERROR_BREAK(pThisSnapshot, COMGETTER(Children)(ComSafeArrayAsOutParam(aSnapshots)));

            for (uint32_t i = 0;
                i < aSnapshots.size();
                ++i)
            {
                ComPtr<ISnapshot> pChild(aSnapshots[i]);
                if (FindAndPrintSnapshotUsingMedium(pMedium,
                                                    pChild,
                                                    pCurrentSnapshot,
                                                    uMediumLevel,
                                                    uSnapshotLevel + 1))
                    // found:
                    break;
            }
        }
    } while (0);

    return false;
}

/**
 * Helper function used with "VBoxManage snapshot ... dump". Called from DumpSnapshot()
 * for each hard disk attachment found in a virtual machine. This then writes out the
 * root (base) medium for that hard disk attachment and recurses into the children
 * tree of that medium, correlating it with the snapshots of the machine.
 * @param pCurrentStateMedium constant, the medium listed in the current machine data (latest diff image).
 * @param pMedium variant, initially the base medium, then a child of the base medium when recursing.
 * @param pRootSnapshot constant, the root snapshot of the machine, if any; this then looks into the child snapshots.
 * @param pCurrentSnapshot constant, the machine's current snapshot (so we can mark it in the output).
 * @param uLevel variant, the recursion level for output indentation.
 */
void DumpMediumWithChildren(ComPtr<IMedium> &pCurrentStateMedium,
                            ComPtr<IMedium> &pMedium,
                            ComPtr<ISnapshot> &pRootSnapshot,
                            ComPtr<ISnapshot> &pCurrentSnapshot,
                            uint32_t uLevel)
{
    HRESULT rc;
    do
    {
        // print this medium
        Bstr bstrMediumName;
        CHECK_ERROR_BREAK(pMedium, COMGETTER(Name)(bstrMediumName.asOutParam()));
        RTPrintf("%*s  \"%ls\"%s\n",
                 uLevel * 2, "",            // indent
                 bstrMediumName.raw(),
                 (pCurrentStateMedium == pMedium) ? " (CURRENT STATE)" : "");

        // find and print the snapshot that uses this particular medium (diff image)
        FindAndPrintSnapshotUsingMedium(pMedium, pRootSnapshot, pCurrentSnapshot, uLevel, 0);

        // recurse into children
        SafeIfaceArray<IMedium> aChildren;
        CHECK_ERROR_BREAK(pMedium, COMGETTER(Children)(ComSafeArrayAsOutParam(aChildren)));
        for (uint32_t i = 0;
             i < aChildren.size();
             ++i)
        {
            ComPtr<IMedium> pChild(aChildren[i]);
            DumpMediumWithChildren(pCurrentStateMedium, pChild, pRootSnapshot, pCurrentSnapshot, uLevel + 1);
        }
    } while (0);
}

/**
 * Implementation for "VBoxManage snapshot ... dump". This goes thru the machine's
 * medium attachments and calls DumpMediumWithChildren() for each hard disk medium found,
 * which then dumps the parent/child tree of that medium together with the corresponding
 * snapshots.
 * @param pMachine Machine to dump snapshots for.
 */
void DumpSnapshot(ComPtr<IMachine> &pMachine)
{
    HRESULT rc;

    do
    {
        // get root snapshot
        ComPtr<ISnapshot> pSnapshot;
        CHECK_ERROR_BREAK(pMachine, GetSnapshot(Bstr(""), pSnapshot.asOutParam()));

        // get current snapshot
        ComPtr<ISnapshot> pCurrentSnapshot;
        CHECK_ERROR_BREAK(pMachine, COMGETTER(CurrentSnapshot)(pCurrentSnapshot.asOutParam()));

        // get media attachments
        SafeIfaceArray<IMediumAttachment> aAttachments;
        CHECK_ERROR_BREAK(pMachine, COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(aAttachments)));
        for (uint32_t i = 0;
             i < aAttachments.size();
             ++i)
        {
            ComPtr<IMediumAttachment> pAttach(aAttachments[i]);
            DeviceType_T type;
            CHECK_ERROR_BREAK(pAttach, COMGETTER(Type)(&type));
            if (type == DeviceType_HardDisk)
            {
                ComPtr<IMedium> pCurrentStateMedium;
                CHECK_ERROR_BREAK(pAttach, COMGETTER(Medium)(pCurrentStateMedium.asOutParam()));

                ComPtr<IMedium> pBaseMedium;
                CHECK_ERROR_BREAK(pCurrentStateMedium, COMGETTER(Base)(pBaseMedium.asOutParam()));

                Bstr bstrBaseMediumName;
                CHECK_ERROR_BREAK(pBaseMedium, COMGETTER(Name)(bstrBaseMediumName.asOutParam()));

                RTPrintf("[%RI32] Images and snapshots for medium \"%ls\"\n", i, bstrBaseMediumName.raw());

                DumpMediumWithChildren(pCurrentStateMedium,
                                       pBaseMedium,
                                       pSnapshot,
                                       pCurrentSnapshot,
                                       0);
            }
        }
    } while (0);
}

/**
 * Implementation for all VBoxManage snapshot ... subcommands.
 * @param a
 * @return
 */
int handleSnapshot(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a VM and a command */
    if (a->argc < 2)
        return errorSyntax(USAGE_SNAPSHOT, "Not enough parameters");

    /* the first argument must be the VM */
    Bstr bstrMachine(a->argv[0]);
    ComPtr<IMachine> pMachine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(bstrMachine, pMachine.asOutParam());
    if (FAILED(rc) || !pMachine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(bstrMachine, pMachine.asOutParam()));
    }
    if (!pMachine)
        return 1;
    Bstr guid;
    pMachine->COMGETTER(Id)(guid.asOutParam());

    do
    {
        /* we have to open a session for this task. First try an existing session */
        rc = a->virtualBox->OpenExistingSession(a->session, guid);
        if (FAILED(rc))
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));

        /* switch based on the command */
        bool fDelete = false, fRestore = false;
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

            /* parse the optional arguments */
            Bstr desc;
            bool fPause = false;
            static const RTGETOPTDEF s_aTakeOptions[] =
            {
                { "--description", 'd', RTGETOPT_REQ_STRING },
                { "-description",  'd', RTGETOPT_REQ_STRING },
                { "-desc",         'd', RTGETOPT_REQ_STRING },
                { "--pause",       'p', RTGETOPT_REQ_NOTHING }
            };
            RTGETOPTSTATE GetOptState;
            RTGetOptInit(&GetOptState, a->argc, a->argv, s_aTakeOptions, RT_ELEMENTS(s_aTakeOptions), 3, 0 /*fFlags*/);
            int ch;
            RTGETOPTUNION Value;
            while (   SUCCEEDED(rc)
                   && (ch = RTGetOpt(&GetOptState, &Value)))
            {
                switch (ch)
                {
                    case 'p':
                        fPause = true;
                        break;

                    case 'd':
                        desc = Value.psz;
                        break;

                    default:
                        errorGetOpt(USAGE_SNAPSHOT, ch, &Value);
                        rc = E_FAIL;
                        break;
                }
            }
            if (FAILED(rc))
                break;

            if (fPause)
            {
                MachineState_T machineState;
                CHECK_ERROR_BREAK(console, COMGETTER(State)(&machineState));
                if (machineState == MachineState_Running)
                    CHECK_ERROR_BREAK(console, Pause());
                else
                    fPause = false;
            }

            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, TakeSnapshot(name, desc, progress.asOutParam()));

            showProgress(progress);
            LONG iRc;
            progress->COMGETTER(ResultCode)(&iRc);
            rc = iRc;
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to take snapshot. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to take snapshot. No error message available!\n");
            }

            if (fPause)
            {
                MachineState_T machineState;
                CHECK_ERROR_BREAK(console, COMGETTER(State)(&machineState));
                if (machineState == MachineState_Paused)
                {
                    if (SUCCEEDED(rc))
                        CHECK_ERROR_BREAK(console, Resume());
                    else
                        console->Resume();
                }
            }
        }
        else if (    (fDelete = !strcmp(a->argv[1], "delete"))
                  || (fRestore = !strcmp(a->argv[1], "restore"))
                )
        {
            /* exactly one parameter: snapshot name */
            if (a->argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> pSnapshot;

            /* assume it's a UUID */
            Bstr guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(pMachine, GetSnapshot(guid, pSnapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(pMachine, FindSnapshot(Bstr(a->argv[2]), pSnapshot.asOutParam()));
                pSnapshot->COMGETTER(Id)(guid.asOutParam());
            }

            ComPtr<IProgress> pProgress;
            if (fDelete)
            {
                CHECK_ERROR_BREAK(console, DeleteSnapshot(guid, pProgress.asOutParam()));
            }
            else
            {
                // must be restore
                CHECK_ERROR_BREAK(console, RestoreSnapshot(pSnapshot, pProgress.asOutParam()));
            }

            showProgress(pProgress);
            LONG iRc;
            pProgress->COMGETTER(ResultCode)(&iRc);
            rc = iRc;
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(pProgress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: snapshot operation failed. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: snapshot operation failed. No error message available!\n");
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
                CHECK_ERROR_BREAK(pMachine, COMGETTER(CurrentSnapshot)(snapshot.asOutParam()));
            }
            else
            {
                /* assume it's a UUID */
                Bstr guid(a->argv[2]);
                if (!guid.isEmpty())
                {
                    CHECK_ERROR_BREAK(pMachine, GetSnapshot(guid, snapshot.asOutParam()));
                }
                else
                {
                    /* then it must be a name */
                    CHECK_ERROR_BREAK(pMachine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
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
            Bstr guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(pMachine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(pMachine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
            }

            /* get the machine of the given snapshot */
            ComPtr<IMachine> pMachine2;
            snapshot->COMGETTER(Machine)(pMachine2.asOutParam());
            showVMInfo(a->virtualBox, pMachine2, VMINFO_NONE, console);
        }
        else if (!strcmp(a->argv[1], "dump"))          // undocumented parameter to debug snapshot info
        {
            DumpSnapshot(pMachine);
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

