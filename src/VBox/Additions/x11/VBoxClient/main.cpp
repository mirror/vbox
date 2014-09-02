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

/** Exit with a fatal error. */
void vbclFatalError(char *pszMessage)
{
    char *pszCommand;
    if (pszMessage)
    {
        pszCommand = RTStrAPrintf2("notify-send \"VBoxClient: %s\"",
                                   pszMessage);
        if (pszCommand)
            system(pszCommand);
    }
    exit(1);
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
    {
        LogRel(("VBoxClient: Failure while acquiring the global critical section, rc=%Rrc\n", rc));
        abort();
    }
    if (g_pService)
        (*g_pService)->cleanup(g_pService);
    if (g_szPidFile[0] && g_hPidFile)
        VbglR3ClosePidFile(g_szPidFile, g_hPidFile);
    VbglR3Term();
    exit(0);
}

/**
 * A standard signal handler which cleans up and exits.
 */
void vboxClientSignalHandler(int cSignal)
{
    LogRel(("VBoxClient: terminated with signal %d\n", cSignal));
    /** Disable seamless mode */
    RTPrintf(("VBoxClient: terminating...\n"));
    VBClCleanUp();
}

/**
 * Xlib error handler for certain errors that we can't avoid.
 */
int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
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
void vboxClientSetSignalHandlers(void)
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

/** Connect to the X server and return the "XFree86_VT" root window property,
 * or 0 on failure. */
static unsigned long getXOrgVT(Display *pDisplay)
{
    Atom actualType;
    int actualFormat;
    unsigned long cItems, cbLeft, cVT = 0;
    unsigned long *pValue;

    XGetWindowProperty(pDisplay, DefaultRootWindow(pDisplay),
                       XInternAtom(pDisplay, "XFree86_VT", False), 0, 1, False,
                       XA_INTEGER, &actualType, &actualFormat, &cItems, &cbLeft,
                       (unsigned char **)&pValue);
    if (cItems && actualFormat == 32)
    {
        cVT = *pValue;
        XFree(pValue);
    }
    return cVT;
}

/** Check whether the current virtual terminal is the one running the X server.
 */
static void checkVTSysfs(RTFILE hFile, uint32_t cVT)
{
    char szTTY[7] = "";
    uint32_t cTTY;
    size_t cbRead;
    int rc;
    const char *pcszStage;

    do {
        pcszStage = "reading /sys/class/tty/tty0/active";
        rc = RTFileReadAt(hFile, 0, (void *)szTTY, sizeof(szTTY), &cbRead);
        if (RT_FAILURE(rc))
            break;
        szTTY[cbRead - 1] = '\0';
        pcszStage = "getting VT number from sysfs file";
        rc = RTStrToUInt32Full(&szTTY[3], 10, &cTTY);
        if (RT_FAILURE(rc))
            break;
        pcszStage = "entering critical section";
        rc = RTCritSectEnter(&g_critSect);
        if (RT_FAILURE(rc))
            break;
        pcszStage = "asking service to pause or resume";
        if (cTTY == cVT)
            rc = (*g_pService)->resume(g_pService);
        else
            rc = (*g_pService)->pause(g_pService);
        if (RT_FAILURE(rc))
            break;
        pcszStage = "leaving critical section";
        rc = RTCritSectLeave(&g_critSect);
    } while(false);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("VBoxClient: failed at stage: \"%s\" rc: %Rrc cVT: %d szTTY: %s.\n",
                    pcszStage, rc, (int) cVT, szTTY));
        if (RTCritSectIsOwner(&g_critSect))
            RTCritSectLeave(&g_critSect);
        VBClCleanUp();
    }
}

/** Poll for TTY changes using sysfs and for X server disconnection.
 * Reading from the start of the pollable file "/sys/class/tty/tty0/active"
 * returns the currently active TTY as a string of the form "tty<n>", with n
 * greater than zero.  Polling for POLLPRI returns when the TTY changes.
 * @a cVT should be zero if we do not know the X server's VT. */
static void pollTTYAndXServer(Display *pDisplay, uint32_t cVT)
{
    RTFILE hFile = NIL_RTFILE;
    struct pollfd pollFD[2];
    unsigned cPollFD = 1;
    int rc;

    pollFD[1].fd = -1;
    pollFD[1].revents = 0;
    /* This block could be Linux-only, but keeping it on Solaris too, where it
     * should just fail gracefully, gives us more code path coverage. */
    if (cVT)
    {
        rc = RTFileOpen(&hFile, "/sys/class/tty/tty0/active",
                        RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN);
        if (RT_SUCCESS(rc))
        {
            pollFD[1].fd = RTFileToNative(hFile);
            pollFD[1].events = POLLPRI;
            cPollFD = 2;
        }
    }
    AssertRelease(pollFD[1].fd >= 0 || cPollFD == 1);
    pollFD[0].fd = ConnectionNumber(pDisplay);
    pollFD[0].events = POLLIN;
    while (true)
    {
        if (hFile != NIL_RTFILE)
            checkVTSysfs(hFile, cVT);
        /* The only point of this loop is to trigger the I/O error handler if
         * appropriate. */
        while (XPending(pDisplay))
        {
            XEvent ev;

            XNextEvent(pDisplay, &ev);
        }
        /* If we get caught in a tight loop for some reason try to limit the
         * damage. */
        if (poll(pollFD, cPollFD, 0) > 0)
        {
            LogRel(("Monitor thread: unexpectedly fast event, revents=0x%x, 0x%x.\n",
                    pollFD[0].revents, pollFD[1].revents));
            RTThreadYield();
        }
        if (   (poll(pollFD, cPollFD, -1) < 0 && errno != EINTR)
            || pollFD[0].revents & POLLNVAL
            || pollFD[1].revents & POLLNVAL)
        {
            LogRel(("Monitor thread: poll failed, stopping.\n"));
            VBClCleanUp();
        }
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
    unsigned long cVT;
    RTFILE hFile;
    
    pDisplay = XOpenDisplay(NULL);
    if (!pDisplay)
        return VINF_SUCCESS;
    cVT = getXOrgVT(pDisplay);
    /* Note: cVT will be 0 if we failed to get it.  This is valid. */
    pollTTYAndXServer(pDisplay, (uint32_t) cVT);
    /* Should never get here. */
    return VINF_SUCCESS;
}

/**
 * Start the thread which notifies the service when we switch to a different
 * VT or back, and terminates us when the X server exits.  The first is best
 * effort functionality: XFree86 4.3 and older do not report their VT via the
 * "XFree86_VT" root window property at all, and pre-2.6.38 Linux does not
 * provide the interface in "sysfs" which we use.  If there is a need for this
 * to work with pre-2.6.38 Linux we can send the VT_GETSTATE ioctl to
 * /dev/console at regular intervals.
 */
static int startMonitorThread()
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
    bool fDaemonise = true;
    int rc;
    const char *pcszFileName, *pcszStage;

    /* Initialise our runtime before all else. */
    rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);
    /* This should never be called twice in one process - in fact one Display
     * object should probably never be used from multiple threads anyway. */
    if (!XInitThreads())
        return 1;
    /* Get our file name for error output. */
    pcszFileName = RTPathFilename(argv[0]);
    if (!pcszFileName)
        pcszFileName = "VBoxClient";
    /* Initialise the guest library. */
    rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
    {
        RTPrintf("%s: failed to connect to the VirtualBox kernel service, rc=%Rrc\n",
                 pcszFileName, rc);
        return 1;
    }

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

    do {
        pcszStage = "Initialising critical section";
        rc = RTCritSectInit(&g_critSect);
        if (RT_FAILURE(rc))
            break;
        pcszStage = "Getting home directory for pid-file";
        rc = RTPathUserHome(g_szPidFile, sizeof(g_szPidFile));
        if (RT_FAILURE(rc))
            break;
        pcszStage = "Creating pid-file path";
        rc = RTPathAppend(g_szPidFile, sizeof(g_szPidFile),
                          (*g_pService)->getPidFilePath());
        if (RT_FAILURE(rc))
            break;
        pcszStage = "Daemonising";
        if (fDaemonise)
            rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */);
        if (RT_FAILURE(rc))
            break;
        pcszStage = "Creating pid-file";
        if (g_szPidFile[0])
            rc = VbglR3PidFile(g_szPidFile, &g_hPidFile);
        if (RT_FAILURE(rc))
            break;
        /* Set signal handlers to clean up on exit. */
        vboxClientSetSignalHandlers();
        /* Set an X11 error handler, so that we don't die when we get unavoidable
         * errors. */
        XSetErrorHandler(vboxClientXLibErrorHandler);
        /* Set an X11 I/O error handler, so that we can shutdown properly on
         * fatal errors. */
        XSetIOErrorHandler(vboxClientXLibIOErrorHandler);
        pcszStage = "Initialising service";
        rc = (*g_pService)->init(g_pService);
    } while (0);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("VBoxClient: failed at stage: \"%s\" rc: %Rrc\n",
                    pcszStage, rc));
        VbglR3Term();
        return 1;
    }

    rc = startMonitorThread();
    if (RT_FAILURE(rc))
        LogRel(("Failed to start the monitor thread (%Rrc).  Exiting.\n",
                 rc));
    else
        (*g_pService)->run(g_pService, fDaemonise);  /* Should never return. */
    VBClCleanUp();
    return 1;
}
