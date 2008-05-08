/** @file
 *
 * VirtualBox Guest Service:
 * Linux guest.
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

#include <VBox/VBoxGuest.h>
#include <VBox/log.h>
#include <iprt/initterm.h>
#include <iprt/path.h>

#include <iostream>
#include <cstdio>

#include <sys/types.h>
#include <stdlib.h>       /* For exit */
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "clipboard.h"

#ifdef DYNAMIC_RESIZE
# include "displaychange.h"
# ifdef SEAMLESS_GUEST
#  include "seamless.h"
# endif
#endif

#define TRACE printf("%s: %d\n", __PRETTY_FUNCTION__, __LINE__); Log(("%s: %d\n", __PRETTY_FUNCTION__, __LINE__))

static int (*gpfnOldIOErrorHandler)(Display *) = NULL;

/* Make these global so that the destructors are called if we make an "emergency exit",
   i.e. a (handled) signal or an X11 error. */
#ifdef DYNAMIC_RESIZE
VBoxGuestDisplayChangeMonitor gDisplayChange;
# ifdef SEAMLESS_GUEST
    /** Our instance of the seamless class.  This only makes sense if dynamic resizing
        is enabled. */
    VBoxGuestSeamless gSeamless;
# endif /* SEAMLESS_GUEST defined */
#endif /* DYNAMIC_RESIZE */
#ifdef VBOX_X11_CLIPBOARD
    VBoxGuestClipboard gClipboard;
#endif

/**
 * Drop the programmes privileges to the caller's.
 * @returns IPRT status code
 * @todo move this into the R3 guest library
 */
int vboxClientDropPrivileges(void)
{
    int rc = VINF_SUCCESS;
    int rcSystem, rcErrno;

    LogFlowFunc(("\n"));
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
    LogFlowFunc(("returning %Rrc\n", rc));
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
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    LogRel(("VBoxClient: an X Window protocol error occurred: %s (error code %d).  Request code: %d, minor code: %d, serial number: %d\n", errorText, pError->error_code, pError->request_code, pError->minor_code, pError->serial));
    /** Disable seamless mode */
    VbglR3SeamlessSetCap(false);
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
    /** Disable seamless mode */
    VbglR3SeamlessSetCap(false);
    VbglR3Term();
    return gpfnOldIOErrorHandler(pDisplay);
}

/**
 * A standard signal handler which cleans up and exits.  Our global static objects will
 * be cleaned up properly as we exit using "exit".
 */
void vboxClientSignalHandler(int cSignal)
{
    Log(("VBoxClient: terminated with signal %d\n", cSignal));
    /** Disable seamless mode */
    VbglR3SeamlessSetCap(false);
    printf(("VBoxClient: terminating...\n"));
    /* don't call VbglR3Term() here otherwise the /dev/vboxadd filehandle is closed */
    /* Our pause() call will now return and exit. */
}

/**
 * Reset all standard termination signals to call our signal handler, which cleans up
 * and exits.
 */
void vboxClientSetSignalHandlers(void)
{
    struct sigaction sigAction;

    LogFlowFunc(("\n"));
    sigAction.sa_handler = vboxClientSignalHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction(SIGHUP, &sigAction, NULL);
    sigaction(SIGINT, &sigAction, NULL);
    sigaction(SIGQUIT, &sigAction, NULL);
    sigaction(SIGABRT, &sigAction, NULL);
    sigaction(SIGPIPE, &sigAction, NULL);
    sigaction(SIGALRM, &sigAction, NULL);
    sigaction(SIGTERM, &sigAction, NULL);
    sigaction(SIGUSR1, &sigAction, NULL);
    sigaction(SIGUSR2, &sigAction, NULL);
    LogFlowFunc(("returning\n"));
}

/**
 * Print out a usage message and exit with success.
 */
void vboxClientUsage(const char *pcszFileName)
{
    /* printf is better for i18n than iostream. */
    printf("Usage: %s [-d|--nodaemon]\n", pcszFileName);
    printf("Start the VirtualBox X Window System guest services.\n\n");
    printf("Options:\n");
    printf("  -d, --nodaemon   do not lower privileges and continue running as a system\n");
    printf("                   service\n");
    exit(0);
}

/**
 * The main loop for the VBoxClient daemon.
 */
int main(int argc, char *argv[])
{
    int rcClipboard, rc = VINF_SUCCESS;
    const char *pszFileName = RTPathFilename(argv[0]);
    bool fDaemonise = true;

    if (NULL == pszFileName)
        pszFileName = "VBoxClient";

    /* Parse our option(s) */
    /** @todo Use RTGetOpt() if the arguments become more complex. */
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--nodaemon"))
            fDaemonise = false;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            vboxClientUsage(pszFileName);
            exit(0);
        }
        else
        {
            /* printf is better than iostream for i18n. */
            printf("%s: unrecognized option `%s'\n", pszFileName, argv[i]);
            printf("Try `%s --help' for more information\n", pszFileName);
            exit(1);
        }
    }
    if (fDaemonise)
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
    if (fDaemonise && RT_FAILURE(vboxClientDropPrivileges()))
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
    vboxClientSetSignalHandlers();
    try
    {
#ifdef VBOX_X11_CLIPBOARD
        /* Connect to the host clipboard. */
        LogRel(("VBoxClient: starting clipboard Guest Additions...\n"));
        rcClipboard = gClipboard.init();
        if (RT_FAILURE(rcClipboard))
        {
            LogRel(("VBoxClient: vboxClipboardConnect failed with rc = %Rrc\n", rc));
        }
#endif  /* VBOX_X11_CLIPBOARD defined */
#ifdef DYNAMIC_RESIZE
        LogRel(("VBoxClient: starting dynamic guest resizing...\n"));
        rc = gDisplayChange.init();
        if (RT_FAILURE(rc))
        {
            LogRel(("VBoxClient: failed to start dynamic guest resizing, rc = %Rrc\n", rc));
        }
# ifdef SEAMLESS_GUEST
        if (RT_SUCCESS(rc))
        {
            LogRel(("VBoxClient: starting seamless Guest Additions...\n"));
            rc = gSeamless.init();
            if (RT_FAILURE(rc))
            {
                LogRel(("VBoxClient: failed to start seamless Additions, rc = %Rrc\n", rc));
            }
        }
# endif /* SEAMLESS_GUEST defined */
#endif /* DYNAMIC_RESIZE defined */
    }
    catch (std::exception e)
    {
        LogRel(("VBoxClient: failed to initialise Guest Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxClient: failed to initialise Guest Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
    LogRel(("VBoxClient: sleeping...\n"));
    pause();
    LogRel(("VBoxClient: exiting...\n"));
    try
    {
        /* r=frank: Why all these 2s delays? What are we waiting for? */
#ifdef DYNAMIC_RESIZE
# ifdef SEAMLESS_GUEST
        LogRel(("VBoxClient: shutting down seamless Guest Additions...\n"));
        gSeamless.uninit(2000);
# endif /* SEAMLESS_GUEST defined */
        LogRel(("VBoxClient: shutting down dynamic guest resizing...\n"));
        gDisplayChange.uninit(2000);
#endif /* DYNAMIC_RESIZE defined */
#ifdef VBOX_X11_CLIPBOARD
        /* Connect to the host clipboard. */
        LogRel(("VBoxClient: shutting down clipboard Guest Additions...\n"));
        gClipboard.uninit(2000);
#endif  /* VBOX_X11_CLIPBOARD defined */
    }
    catch (std::exception e)
    {
        LogRel(("VBoxClient: failed to shut down Guest Additions - caught exception: %s\n", e.what()));
        rc = VERR_UNRESOLVED_ERROR;
    }
    catch (...)
    {
        LogRel(("VBoxClient: failed to shut down Guest Additions - caught unknown exception.\n"));
        rc = VERR_UNRESOLVED_ERROR;
    }
    VbglR3Term();
    return RT_SUCCESS(rc) ? 0 : 1;
}
