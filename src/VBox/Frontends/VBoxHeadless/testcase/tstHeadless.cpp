/** @file
 *
 * VBox frontends: VBoxHeadless frontend:
 * Testcases
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint_legacy.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#include <VBox/log.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>


////////////////////////////////////////////////////////////////////////////////

/**
 *  Entry point.
 */
int main (int argc, char **argv)
{
    // initialize VBox Runtime
    RTR3Init();

    // the below cannot be Bstr because on Linux Bstr doesn't work
    // until XPCOM (nsMemory) is initialized
    const char *name = NULL;
    const char *operation = NULL;

    // parse the command line
    if (argc > 1)
        name = argv [1];
    if (argc > 2)
        operation = argv [2];

    if (!name || !operation)
    {
        RTPrintf ("\nUsage:\n\n"
                  "%s <machine_name> [on|off|pause|resume]\n\n",
                  argv [0]);
        return -1;
    }

    RTPrintf ("\n");
    RTPrintf ("tstHeadless STARTED.\n");

    RTPrintf ("VM name   : {%s}\n"
              "Operation : %s\n\n",
              name, operation);

    HRESULT rc;

    CHECK_RC_RET (com::Initialize());

    do
    {
        ComPtr <IVirtualBox> virtualBox;
        ComPtr <ISession> session;

        RTPrintf ("Creating VirtualBox object...\n");
        CHECK_ERROR_NI_BREAK (virtualBox.createLocalObject (CLSID_VirtualBox));

        RTPrintf ("Creating Session object...\n");
        CHECK_ERROR_NI_BREAK (session.createInprocObject (CLSID_Session));

        // create the event queue
        // (here it is necessary only to process remaining XPCOM/IPC events
        // after the session is closed)
        EventQueue eventQ;

        // find ID by name
        Bstr id;
        {
            ComPtr <IMachine> m;
            CHECK_ERROR_BREAK (virtualBox, FindMachine (Bstr (name), m.asOutParam()));
            CHECK_ERROR_BREAK (m, COMGETTER(Id) (id.asOutParam()));
        }

        if (!strcmp (operation, "on"))
        {
            ComPtr <IProgress> progress;
            RTPrintf ("Opening a new (remote) session...\n");
            CHECK_ERROR_BREAK (virtualBox,
                               OpenRemoteSession (session, id, Bstr ("vrdp"),
                                                  NULL, progress.asOutParam()));

            RTPrintf ("Waiting for the remote session to open...\n");
            CHECK_ERROR_BREAK (progress, WaitForCompletion (-1));

            BOOL completed;
            CHECK_ERROR_BREAK (progress, COMGETTER(Completed) (&completed));
            ASSERT (completed);

            HRESULT resultCode;
            CHECK_ERROR_BREAK (progress, COMGETTER(ResultCode) (&resultCode));
            if (FAILED (resultCode))
            {
                ComPtr <IVirtualBoxErrorInfo> errorInfo;
                CHECK_ERROR_BREAK (progress,
                                   COMGETTER(ErrorInfo) (errorInfo.asOutParam()));
                ErrorInfo info (errorInfo);
                PRINT_ERROR_INFO (info);
            }
            else
            {
                RTPrintf ("Remote session has been successfully opened.\n");
            }
        }
        else
        {
            RTPrintf ("Opening an existing session...\n");
            CHECK_ERROR_BREAK (virtualBox, OpenExistingSession (session, id));

            ComPtr <IConsole> console;
            CHECK_ERROR_BREAK (session, COMGETTER (Console) (console.asOutParam()));

            if (!strcmp (operation, "off"))
            {
                RTPrintf ("Powering the VM off...\n");
                CHECK_ERROR_BREAK (console, PowerDown());
            }
            else
            if (!strcmp (operation, "pause"))
            {
                RTPrintf ("Pausing the VM...\n");
                CHECK_ERROR_BREAK (console, Pause());
            }
            else
            if (!strcmp (operation, "resume"))
            {
                RTPrintf ("Resuming the VM...\n");
                CHECK_ERROR_BREAK (console, Resume());
            }
            else
            {
                RTPrintf ("Invalid operation!\n");
            }
        }

        RTPrintf ("Closing the session (may fail after power off)...\n");
        CHECK_ERROR (session, Close());
    }
    while (0);
    RTPrintf ("\n");

    com::Shutdown();

    RTPrintf ("tstHeadless FINISHED.\n");

    return rc;
}

