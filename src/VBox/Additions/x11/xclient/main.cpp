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
#include <stdlib.h>       /* For exit */
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "clipboard.h"

#ifdef SEAMLESS_X11
# include "seamless.h"
#endif

static bool gbDaemonise = true;
static int (*gpfnOldIOErrorHandler)(Display *) = NULL;

/**
 * Drop the programmes privileges to the caller's.
 * @returns IPRT status code
 * @todo move this into the R3 guest library
 */
int vboxClientDropPrivileges(void)
{
    int rc = VINF_SUCCESS;
    int rcSystem, rcErrno;

#ifdef _POSIX_SAVED_IDS
    rcSystem = setuid(getuid());
#else
    rcSystem = setreuid(-1, getuid());
#endif
    if (rcSystem < 0)
    {
        rcErrno = errno;
        rc = RTErrConvertFromErrno(rcErrno);
        LogRel(("VBoxClient: failed to drop privileges, error %Rrc.\n", rc));
    }
    return rc;
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
        Log(("VBoxClient: ignoring BadAtom error and returning\n"));
        return 0;
    }
    if (pError->error_code == BadWindow)
    {
        /* This can be triggered if a guest application destroys a window before we notice. */
        Log(("VBoxClient: ignoring BadWindow error and returning\n"));
        return 0;
    }
#ifdef VBOX_X11_CLIPBOARD
    vboxClipboardDisconnect();
#endif
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    LogRel(("VBoxClient: an X Window protocol error occurred: %s (error code %d).  Request code: %d, minor code: %d, serial number: %d\n", errorText, pError->error_code, pError->request_code, pError->minor_code, pError->serial));
    VbglR3Term();
    exit(1);
}

/**
 * Xlib error handler for fatal errors.  This often means that the programme is still running
 * when X exits.
 */
int vboxClientXLibIOErrorHandler(Display *pDisplay)
{
    Log(("VBoxClient: a fatal guest X Window error occurred.  This may just mean that the Window system was shut down while the client was still running.\n"));
    VbglR3Term();
    return gpfnOldIOErrorHandler(pDisplay);
}

int main(int argc, char *argv[])
{
    int rcClipboard, rc;
#ifdef SEAMLESS_X11
    /** Our instance of the seamless class. */
    VBoxGuestSeamless seamless;
#endif

    /* Parse our option(s) */
/** @todo r=bird: use RTGetOpt */
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
        rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */);
        if (RT_FAILURE(rc))
        {
            std::cout << "VBoxClient: failed to daemonize. exiting."<< std::endl;
            return 1;
        }
    }
    /* Initialise our runtime before all else. */
    RTR3Init(false);
    if (RT_FAILURE(VbglR3Init()))
    {
        std::cout << "Failed to connect to the VirtualBox kernel service" << std::endl;
        return 1;
    }
    if (RT_FAILURE(vboxClientDropPrivileges()))
        return 1;
    LogRel(("VBoxClient: starting...\n"));
    /* Initialise threading in X11 and in Xt. */
    if (!XInitThreads() || !XtToolkitThreadInitialize())
    {
        LogRel(("VBoxClient: error initialising threads in X11, exiting."));
        return 1;
    }
    /* Set an X11 error handler, so that we don't die when we get unavoidable errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    /* Set an X11 I/O error handler, so that we can shutdown properly on fatal errors. */
    gpfnOldIOErrorHandler = XSetIOErrorHandler(vboxClientXLibIOErrorHandler);
#ifdef VBOX_X11_CLIPBOARD
    /* Connect to the host clipboard. */
    LogRel(("VBoxClient: starting clipboard Guest Additions...\n"));
    rcClipboard = vboxClipboardConnect();
    if (RT_FAILURE(rcClipboard))
    {
        LogRel(("VBoxClient: vboxClipboardConnect failed with rc = %Rrc\n", rc));
    }
#endif  /* VBOX_X11_CLIPBOARD defined */
#ifdef SEAMLESS_X11
    try
    {
        LogRel(("VBoxClient: starting seamless Guest Additions...\n"));
        rc = seamless.init();
        if (RT_FAILURE(rc))
        {
            LogRel(("VBoxClient: failed to initialise seamless Additions, rc = %Rrc\n", rc));
        }
    }
    catch (std::exception e)
    {
        LogRel(("VBoxClient: failed to initialise seamless Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxClient: failed to initialise seamless Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_X11 defined */
#ifdef VBOX_X11_CLIPBOARD
    if (RT_SUCCESS(rcClipboard))
    {
        LogRel(("VBoxClient: connecting to the shared clipboard service.\n"));
        vboxClipboardMain();
        vboxClipboardDisconnect();
    }
#else  /* VBOX_X11_CLIPBOARD not defined */
    LogRel(("VBoxClient: sleeping...\n"));
    pause();
    LogRel(("VBoxClient: exiting...\n"));
#endif  /* VBOX_X11_CLIPBOARD not defined */
#ifdef SEAMLESS_X11
    try
    {
        seamless.uninit();
    }
    catch (std::exception e)
    {
        LogRel(("VBoxClient: error shutting down seamless Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxClient: error shutting down seamless Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
#endif /* SEAMLESS_X11 defined */
    VbglR3Term();
    return RT_SUCCESS(rc) ? 0 : 1;
}
