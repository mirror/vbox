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

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "clipboard.h"

#ifdef SEAMLESS_LINUX
# include "seamless.h"
#endif

static bool gbDaemonise = true;

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
#ifdef VBOX_X11_CLIPBOARD
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
    gbDaemonise = false; // ram
    if (gbDaemonise)
    {
        if (VbglR3Daemonize(0, 0) != 0)
        {
            LogRel(("VBoxService: failed to daemonize. exiting."));
            return 1;
        }
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
#ifdef VBOX_X11_CLIPBOARD
    /* Connect to the host clipboard. */
    LogRel(("VBoxService: starting clipboard Guest Additions...\n"));
    rc = vboxClipboardConnect();
    if (RT_SUCCESS(rc))
    {
        LogRel(("VBoxService: vboxClipboardConnect failed with rc = %Rrc\n", rc));
    }
#endif  /* VBOX_X11_CLIPBOARD defined */
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
#ifdef VBOX_X11_CLIPBOARD
    LogRel(("VBoxService: connecting to the shared clipboard service.\n"));
    vboxClipboardMain();
    vboxClipboardDisconnect();
#else  /* VBOX_X11_CLIPBOARD not defined */
    LogRel(("VBoxService: sleeping...\n"));
    pause();
    LogRel(("VBoxService: exiting...\n"));
#endif  /* VBOX_X11_CLIPBOARD not defined */
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
