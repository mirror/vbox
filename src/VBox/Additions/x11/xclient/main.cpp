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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// #define LOG_GROUP LOG_GROUP_DEV_VMM_BACKDOOR

#include <VBox/VBoxGuest.h>
#include <VBox/log.h>
#include <iprt/initterm.h>

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>     /* For umask */
#include <fcntl.h>        /* For open */
#include <stdlib.h>       /* For exit */
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
    for (unsigned int i = 0; i < rlim.rlim_cur; ++i)
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
 * Xlib error handler for certain errors that we can't avoid.
 */
int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    if (pError->error_code == BadAtom)
    {
        /* This can be triggered in debug builds if a guest application passes a bad atom
           in its list of supported clipboard formats.  As such it is harmless. */
        Log(("VBoxService: ignoring BadAtom error and returning\n"));
        return 0;
    }
    if (pError->error_code == BadWindow)
    {
        /* This can be triggered if a guest application destroys a window before we notice. */
        Log(("VBoxService: ignoring BadWindow error and returning\n"));
        return 0;
    }
#ifdef CLIPBOARD_LINUX
    vboxClipboardDisconnect();
#endif
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    LogRel(("VBoxService: an X Window protocol error occurred: %s.  Request code: %d, minor code: %d, serial number: %d\n",
         pError->error_code, pError->request_code, pError->minor_code, pError->serial));
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
                std::cout << "Unrecognized command line argument: " << argv[argc] << std::endl;
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
            std::cout << "Unrecognized command line option: " << static_cast<char>(cOpt)
                      << std::endl;
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
    rc = VbglR3Init();
    if (RT_FAILURE(rc))
    {
        std::cout << "Failed to connect to the VirtualBox kernel service" << std::endl;
        return 1;
    }
    LogRel(("VBoxService: starting...\n"));
    /* Initialise threading in X11 and in Xt. */
    if (!XInitThreads() || !XtToolkitThreadInitialize())
    {
        LogRel(("VBoxService: error initialising threads in X11, exiting."));
        return 1;
    }
    /* Set an X11 error handler, so that we don't die when we get unavoidable errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
#ifdef CLIPBOARD_LINUX
    /* Connect to the host clipboard. */
    LogRel(("VBoxService: starting clipboard Guest Additions...\n"));
    rc = vboxClipboardConnect();
    if (RT_SUCCESS(rc))
    {
        LogRel(("VBoxService: vboxClipboardConnect failed with rc = %Rrc\n", rc));
    }
#endif  /* CLIPBOARD_LINUX defined */
#ifdef SEAMLESS_LINUX
    try
    {
        LogRel(("VBoxService: starting seamless Guest Additions...\n"));
        rc = seamless.init();
        if (RT_FAILURE(rc))
        {
            LogRel(("VBoxService: failed to initialise seamless Additions, rc = %Rrc\n", rc));
        }
    }
    catch (std::exception e)
    {
        LogRel(("VBoxService: failed to initialise seamless Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxService: failed to initialise seamless Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_LINUX defined */
#ifdef CLIPBOARD_LINUX
    LogRel(("VBoxService: connecting to the shared clipboard service.\n"));
    vboxClipboardMain();
    vboxClipboardDisconnect();
#else  /* CLIPBOARD_LINUX not defined */
    LogRel(("VBoxService: sleeping...\n"));
    pause();
    LogRel(("VBoxService: exiting...\n"));
#endif  /* CLIPBOARD_LINUX not defined */
#ifdef SEAMLESS_LINUX
    try
    {
        seamless.uninit();
    }
    catch (std::exception e)
    {
        LogRel(("VBoxService: error shutting down seamless Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxService: error shutting down seamless Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_LINUX defined */
    return rc;
}
