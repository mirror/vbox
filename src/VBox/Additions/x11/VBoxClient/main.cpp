/** @file
 *
 * VirtualBox Guest Service:
 * Linux guest.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <sys/types.h>
#include <stdlib.h>       /* For exit */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <iprt/critsect.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/types.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/log.h>

#include "VBoxClient.h"

static int (*gpfnOldIOErrorHandler)(Display *) = NULL;

/** Object representing the service we are running.  This has to be global
 * so that the cleanup routine can access it. */
struct VBCLSERVICE **g_pService;
/** The name of our pidfile.  It is global for the benefit of the cleanup
 * routine. */
static char g_szPidFile[RTPATH_MAX];
/** The file handle of our pidfile.  It is global for the benefit of the
 * cleanup routine. */
static RTFILE g_hPidFile;
/** Global critical section held during the clean-up routine (to prevent it
 * being called on multiple threads at once) or things which may not happen
 * during clean-up (e.g. pausing and resuming the service).
 */
RTCRITSECT g_critSect;
/** Counter of how often our deamon has been respawned. */
unsigned cRespawn = 0;

/** Exit with a fatal error. */
void vbclFatalError(char *pszMessage)
{
    char *pszCommand;
    if (pszMessage && cRespawn == 0)
    {
        pszCommand = RTStrAPrintf2("notify-send \"VBoxClient: %s\"", pszMessage);
        if (pszCommand)
            system(pszCommand);
    }
    _exit(1);
}

/** Clean up if we get a signal or something.  This is extern so that we
 * can call it from other compilation units. */
void VBClCleanUp()
{
    /* We never release this, as we end up with a call to exit(3) which is not
     * async-safe.  Unless we fix this application properly, we should be sure
     * never to exit from anywhere except from this method. */
    int rc = RTCritSectEnter(&g_critSect);
    if (RT_FAILURE(rc))
        VBClFatalError(("VBoxClient: Failure while acquiring the global critical section, rc=%Rrc\n", rc));
    if (g_pService)
        (*g_pService)->cleanup(g_pService);
    if (g_szPidFile[0] && g_hPidFile)
        VbglR3ClosePidFile(g_szPidFile, g_hPidFile);
    exit(0);
}

/**
 * A standard signal handler which cleans up and exits.
 */
static void vboxClientSignalHandler(int cSignal)
{
    LogRel(("VBoxClient: terminated with signal %d\n", cSignal));
    /** Disable seamless mode */
    RTPrintf(("VBoxClient: terminating...\n"));
    VBClCleanUp();
}

/**
 * Xlib error handler for certain errors that we can't avoid.
 */
static int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    LogRelFlow(("VBoxClient: an X Window protocol error occurred: %s (error code %d).  Request code: %d, minor code: %d, serial number: %d\n", errorText, pError->error_code, pError->request_code, pError->minor_code, pError->serial));
    return 0;  /* We should never reach this. */
}

/**
 * Xlib error handler for fatal errors.  This often means that the programme is still running
 * when X exits.
 */
static int vboxClientXLibIOErrorHandler(Display *pDisplay)
{
    LogRel(("VBoxClient: a fatal guest X Window error occurred.  This may just mean that the Window system was shut down while the client was still running.\n"));
    VBClCleanUp();
    return 0;  /* We should never reach this. */
}

/**
 * Reset all standard termination signals to call our signal handler, which
 * cleans up and exits.
 */
static void vboxClientSetSignalHandlers(void)
{
    struct sigaction sigAction;

    LogRelFlowFunc(("\n"));
    sigAction.sa_handler = vboxClientSignalHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction(SIGHUP, &sigAction, NULL);
    sigaction(SIGINT, &sigAction, NULL);
    sigaction(SIGQUIT, &sigAction, NULL);
    sigaction(SIGPIPE, &sigAction, NULL);
    sigaction(SIGALRM, &sigAction, NULL);
    sigaction(SIGTERM, &sigAction, NULL);
    sigaction(SIGUSR1, &sigAction, NULL);
    sigaction(SIGUSR2, &sigAction, NULL);
    LogRelFlowFunc(("returning\n"));
}

/** Check whether X.Org has acquired or lost the current virtual terminal and
 * call the service @a pause() or @a resume() call-back if appropriate.
 * The functionality is provided by the vboxvideo driver for pre-1.16 X servers
 * and by 1.16 and later series servers.
 * This can either be called directly from a service's event loop or the service
 * can call VBClStartVTMonitor() to start an event loop in a separate thread.
 * Property notification for the root window should be selected first.  Services
 * are not required to check VT changes if they do not need the information.
 * @param  pEvent an event received on a display connection which will be
 *                checked to see if it is change to the XFree86_has_VT property
 */
void VBClCheckXOrgVT(union _XEvent *pEvent)
{
    Atom actualType;
    int actualFormat;
    unsigned long cItems, cbLeft;
    bool fHasVT = false;
    unsigned long *pValue;
    int rc;
    Display *pDisplay = pEvent->xany.display;
    Atom hasVT = XInternAtom(pDisplay, "XFree86_has_VT", False);

    if (   pEvent->type != PropertyNotify
        || pEvent->xproperty.window != DefaultRootWindow(pDisplay)
        || pEvent->xproperty.atom != hasVT)
        return;
    XGetWindowProperty(pDisplay, DefaultRootWindow(pDisplay), hasVT, 0, 1,
                       False, XA_INTEGER, &actualType, &actualFormat, &cItems,
                       &cbLeft, (unsigned char **)&pValue);
    if (cItems && actualFormat == 32)
    {
        fHasVT = *pValue != 0;
        XFree(pValue);
    }
    else
        return;
    if (fHasVT)
    {
        rc = (*g_pService)->resume(g_pService);
        if (RT_FAILURE(rc))
            VBClFatalError(("Error resuming the service: %Rrc\n"));
    }
    if (!fHasVT)
    {
        rc = (*g_pService)->pause(g_pService);
        if (RT_FAILURE(rc))
            VBClFatalError(("Error pausing the service: %Rrc\n"));
    }
}

/**
 * Thread which notifies the service when we switch to a different VT or back
 * and cleans up when the X server exits.
 * @note runs until programme exit.
 */
static int pfnMonitorThread(RTTHREAD self, void *pvUser)
{
    Display *pDisplay;
    bool fHasVT = true;

    pDisplay = XOpenDisplay(NULL);
    if (!pDisplay)
        VBClFatalError(("Failed to open the X11 display\n"));
    XSelectInput(pDisplay, DefaultRootWindow(pDisplay), PropertyChangeMask);
    while (true)
    {
        XEvent event;

        XNextEvent(pDisplay, &event);
        VBClCheckXOrgVT(&event);
    }
    return VINF_SUCCESS;  /* Should never be reached. */
}

/**
 * Start a thread which notifies the service when we switch to a different
 * VT or back, and terminates us when the X server exits.  This should be called
 * by most services which do not regularly run an X11 event loop.
 */
int VBClStartVTMonitor()
{
    return RTThreadCreate(NULL, pfnMonitorThread, NULL, 0,
                          RTTHREADTYPE_INFREQUENT_POLLER, 0, "MONITOR");
}

/**
 * Print out a usage message and exit with success.
 */
void vboxClientUsage(const char *pcszFileName)
{
    RTPrintf("Usage: %s --clipboard|"
#ifdef VBOX_WITH_DRAG_AND_DROP
             "--draganddrop|"
#endif
             "--display|"
# ifdef VBOX_WITH_GUEST_PROPS
             "--checkhostversion|"
#endif
             "--seamless [-d|--nodaemon]\n", pcszFileName);
    RTPrintf("Start the VirtualBox X Window System guest services.\n\n");
    RTPrintf("Options:\n");
    RTPrintf("  --clipboard        start the shared clipboard service\n");
#ifdef VBOX_WITH_DRAG_AND_DROP
    RTPrintf("  --draganddrop      start the drag and drop service\n");
#endif
    RTPrintf("  --display          start the display management service\n");
#ifdef VBOX_WITH_GUEST_PROPS
    RTPrintf("  --checkhostversion start the host version notifier service\n");
#endif
    RTPrintf("  --seamless         start the seamless windows service\n");
    RTPrintf("  -d, --nodaemon     continue running as a system service\n");
    RTPrintf("\n");
    exit(0);
}

/**
 * The main loop for the VBoxClient daemon.
 * @todo Clean up for readability.
 */
int main(int argc, char *argv[])
{
    bool fDaemonise = true, fRespawn = true;
    int rc;
    const char *pcszFileName, *pcszStage;

    /* Initialise our runtime before all else. */
    rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);
    /* This should never be called twice in one process - in fact one Display
     * object should probably never be used from multiple threads anyway. */
    if (!XInitThreads())
        VBClFatalError(("Failed to initialize X11 threads\n"));
    /* Get our file name for error output. */
    pcszFileName = RTPathFilename(argv[0]);
    if (!pcszFileName)
        pcszFileName = "VBoxClient";

    /* Parse our option(s) */
    /** @todo Use RTGetOpt() if the arguments become more complex. */
    for (int i = 1; i < argc; ++i)
    {
        rc = VERR_INVALID_PARAMETER;
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--nodaemon"))
        {
            /* If the user is running in "no daemon" mode anyway, send critical
             * logging to stdout as well. */
            PRTLOGGER pReleaseLog = RTLogRelDefaultInstance();

            if (pReleaseLog)
                rc = RTLogDestinations(pReleaseLog, "stdout");
            if (pReleaseLog && RT_FAILURE(rc))
                RTPrintf("%s: failed to redivert error output, rc=%Rrc\n",
                         pcszFileName, rc);
            fDaemonise = false;
        }
        else if (!strcmp(argv[i], "--no-respawn"))
        {
            fRespawn = false;
        }
        else if (!strcmp(argv[i], "--clipboard"))
        {
            if (g_pService)
                break;
            g_pService = VBClGetClipboardService();
        }
        else if (!strcmp(argv[i], "--display"))
        {
            if (g_pService)
                break;
            g_pService = VBClGetDisplayService();
        }
        else if (!strcmp(argv[i], "--seamless"))
        {
            if (g_pService)
                break;
            g_pService = VBClGetSeamlessService();
        }
        else if (!strcmp(argv[i], "--checkhostversion"))
        {
            if (g_pService)
                break;
            g_pService = VBClGetHostVersionService();
        }
#ifdef VBOX_WITH_DRAG_AND_DROP
        else if (!strcmp(argv[i], "--draganddrop"))
        {
            if (g_pService)
                break;
            g_pService = VBClGetDragAndDropService();
        }
#endif /* VBOX_WITH_DRAG_AND_DROP */
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            vboxClientUsage(pcszFileName);
            return 0;
        }
        else
        {
            RTPrintf("%s: unrecognized option `%s'\n", pcszFileName, argv[i]);
            RTPrintf("Try `%s --help' for more information\n", pcszFileName);
            return 1;
        }
        rc = VINF_SUCCESS;
    }
    if (RT_FAILURE(rc) || !g_pService)
    {
        vboxClientUsage(pcszFileName);
        return 1;
    }

    rc = RTCritSectInit(&g_critSect);
    if (RT_FAILURE(rc))
        VBClFatalError(("Initialising critical section: %Rrc\n", rc));
    rc = RTPathUserHome(g_szPidFile, sizeof(g_szPidFile));
    if (RT_FAILURE(rc))
        VBClFatalError(("Getting home directory for pid-file: %Rrc\n", rc));
    rc = RTPathAppend(g_szPidFile, sizeof(g_szPidFile),
                      (*g_pService)->getPidFilePath());
    if (RT_FAILURE(rc))
        VBClFatalError(("Creating pid-file path: %Rrc\n", rc));
    if (fDaemonise)
        rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */, fRespawn, &cRespawn);
    if (RT_FAILURE(rc))
        VBClFatalError(("Daemonizing: %Rrc\n", rc));
    if (g_szPidFile[0])
        rc = VbglR3PidFile(g_szPidFile, &g_hPidFile);
    if (rc == VERR_FILE_LOCK_VIOLATION)  /* Already running. */
        return 0;
    if (RT_FAILURE(rc))
        VBClFatalError(("Creating pid-file: %Rrc\n", rc));
    /* Set signal handlers to clean up on exit. */
    vboxClientSetSignalHandlers();
#ifndef VBOXCLIENT_WITHOUT_X11
    /* Set an X11 error handler, so that we don't die when we get unavoidable
     * errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    /* Set an X11 I/O error handler, so that we can shutdown properly on
     * fatal errors. */
    XSetIOErrorHandler(vboxClientXLibIOErrorHandler);
#endif
    rc = (*g_pService)->init(g_pService);
    if (RT_FAILURE(rc))
        VBClFatalError(("Initialising service: %Rrc\n", rc));
    rc = (*g_pService)->run(g_pService, fDaemonise);
    if (RT_FAILURE(rc))
        VBClFatalError(("Service main loop failed: %Rrc\n", rc));
    VBClCleanUp();
    return 0;
}
