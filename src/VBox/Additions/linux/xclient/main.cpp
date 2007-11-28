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
#include <sys/stat.h>     /* For umask */
#include <fcntl.h>        /* For open */
#include <unistd.h>
#include <getopt.h>

#include <sys/time.h>     /* For getrlimit */
#include <sys/resource.h> /* For getrlimit */

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "clipboard.h"

#ifdef SEAMLESS_LINUX
# include "seamless.h"
#endif

static bool gbDaemonise = true;

/**
 * Go through the long Un*x ritual required to become a daemon process.
 */
void vboxDaemonise(void)
{
    /** rlimit structure for finding out how many open files we may have. */
    struct rlimit rlim;

    /* To make sure that we are not currently a session leader, we must first fork and let
       the parent process exit, as a newly created child is never session leader.  This will
       allow us to call setsid() later. */
    if (fork() != 0)
    {
        exit(0);
    }
    /* Find the maximum number of files we can have open and close them all. */
    if (0 != getrlimit(RLIMIT_NOFILE, &rlim))
    {
        /* For some reason the call failed.  In that case we will just close the three
           standard files and hope. */
        rlim.rlim_cur = 3;
    }
    for (int i = 0; i < rlim.rlim_cur; ++i)
    {
        close(i);
    }
    /* Change to the root directory to avoid keeping the one we were started in open. */
    chdir("/");
    /* Set our umask to zero. */
    umask(0);
    /* And open /dev/null on stdin/out/err. */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    dup(1);
    /* Detach from the controlling terminal by creating our own session, to avoid receiving
       signals from the old session. */
    setsid();
    /* And fork again, letting the parent exit, to make us a child of init and avoid zombies. */
    if (fork() != 0)
    {
        exit(0);
    }
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
#ifdef SEAMLESS_LINUX
    /** Our instance of the seamless class. */
    VBoxGuestSeamless seamless;
#endif

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
    if (gbDaemonise)
    {
        vboxDaemonise();
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
    LogRel(("Starting clipboard Guest Additions...\n"));
    rc = vboxClipboardConnect();
    if (rc != VINF_SUCCESS)
    {
        LogRel(("vboxClipboardConnect failed with rc = %d\n", rc));
        cout << "Failed to connect to the host clipboard." << endl;
    }
#ifdef SEAMLESS_LINUX
    try
    {
        LogRel(("Starting seamless Guest Additions...\n"));
        rc = seamless.init();
        if (rc != VINF_SUCCESS)
        {
            LogRel(("Failed to initialise seamless Additions, rc = %d\n", rc));
            cout << "Failed to initialise seamless Additions." << endl;
        }
    }
    catch (std::exception e)
    {
        LogRel(("Failed to initialise seamless Additions - caught exception: %s\n", e.what()));
        cout << "Failed to initialise seamless Additions\n" << endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("Failed to initialise seamless Additions - caught unknown exception.\n"));
        cout << "Failed to initialise seamless Additions\n" << endl;
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_LINUX defined */
    vboxClipboardMain();
    vboxClipboardDisconnect();
#ifdef SEAMLESS_LINUX
    try
    {
        seamless.uninit();
    }
    catch (std::exception e)
    {
        LogRel(("Error shutting down seamless Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("Error shutting down seamless Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_LINUX defined */
    LogFlowFunc(("returning %Vrc\n", rc));
    return rc;
}
