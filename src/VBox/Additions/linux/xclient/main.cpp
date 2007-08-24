/** @file
 *
 * VirtualBox Guest Service:
 * Linux guest.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_VMM_BACKDOOR

#include <VBox/VBoxGuest.h>
#include <VBox/log.h>
#include <iprt/initterm.h>

#include <iostream>

using std::cout;
using std::endl;

#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "clipboard.h"

static bool gbDaemonise = true;

/**
 * Become a daemon process
 */
void vboxDaemonise(void)
{
    /* First fork and exit the parent process, so that we are sure we are not session leader. */
    if (fork() != 0)
    {
        exit(0);
    }
    /* Detach from the controlling terminal by creating our own session. */
    setsid();
    /* And change to the root directory to avoid holding the one we were started in open. */
    chdir("/");
    /* Close the standard files. */
    close(0);
    close(1);
    close(2);
}

/**
 * Xlib error handler, so that we don't abort when we get a BadAtom error.
 */
int vboxClipboardXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    LogFlowFunc(("\n"));
    if (pError->error_code == BadAtom)
    {
        /* This can be triggered in debug builds if a guest application passes a bad atom
           in its list of supported clipboard formats.  As such it is harmless. */
        LogFlowFunc(("ignoring BadAtom error and returning\n"));
        return 0;
    }
    vboxClipboardDisconnect();
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    if (!gbDaemonise)
    {
        cout << "An X Window protocol error occurred: " << errorText << endl
            << "  Request code: " << int(pError->request_code) << endl
            << "  Minor code: " << int(pError->minor_code) << endl
            << "  Serial number of the failed request: " << int(pError->serial) << endl;
    }
    Log(("%s: an X Window protocol error occurred: %s.  Request code: %d, minor code: %d, serial number: %d\n",
         __PRETTY_FUNCTION__, pError->error_code, pError->request_code, pError->minor_code,
         pError->serial));
    LogFlowFunc(("exiting\n"));
    exit(1);
}

int main(int argc, char *argv[])
{
    int rc;

    /* Parse our option(s) */
    while (1)
    {
        static struct option sOpts[] =
        {
            {"nodaemon", 0, 0, 'd'},
            {0, 0, 0, 0}
        };
        int cOpt = getopt_long(argc, argv, "", sOpts, 0);
        if (cOpt == -1)
        {
            if (optind < argc)
            {
                cout << "Unrecognized command line argument: " << argv[argc] << endl;
                exit(1);
            }
            break;
        }
        switch(cOpt)
        {
        case 'd':
            gbDaemonise = false;
            break;
        default:
            cout << "Unrecognized command line option: " << static_cast<char>(cOpt) << endl;
        case '?':
            exit(1);
        }
    }
    /* Initialise our runtime before all else. */
    RTR3Init(false);
    LogFlowFunc(("\n"));
    /* Initialise threading in X11 and in Xt. */
    if (!XInitThreads() || !XtToolkitThreadInitialize())
    {
        LogRelFunc(("Error initialising threads in X11, returning 1."));
        cout << "Your guest system appears to be using an old, single-threaded version of the X Window System libraries.  This program cannot continue." << endl;
        return 1;
    }
    /* Set an X11 error handler, so that we don't die when we get BadAtom errors. */
    XSetErrorHandler(vboxClipboardXLibErrorHandler);
    /* Connect to the host clipboard. */
    rc = vboxClipboardConnect();
    if (rc != VINF_SUCCESS)
    {
        Log(("vboxClipboardConnect failed with rc = %d\n", rc));
        cout << "Failed to connect to the host clipboard." << endl;
        LogFlowFunc(("returning 1\n"));
        return 1;
    }
    if (gbDaemonise)
    {
        vboxDaemonise();
    }
    vboxClipboardMain();
    vboxClipboardDisconnect();
    LogFlowFunc(("returning 0\n"));
    return 0;
}
