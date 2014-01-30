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
#if 0
#include <sys/vt.h>
#endif
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

#define TRACE RTPrintf("%s: %d\n", __PRETTY_FUNCTION__, __LINE__); LogRel(("%s: %d\n", __PRETTY_FUNCTION__, __LINE__))

static int (*gpfnOldIOErrorHandler)(Display *) = NULL;

/** Object representing the service we are running.  This has to be global
 * so that the cleanup routine can access it. */
VBoxClient::Service *g_pService;
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
RTCRITSECT g_cleanupCritSect;

/** Clean up if we get a signal or something.  This is extern so that we
 * can call it from other compilation units. */
void VBoxClient::CleanUp()
{
    /* We never release this, as we end up with a call to exit(3) which is not
     * async-safe.  Until we fix this application properly, we should be sure
     * never to exit from anywhere except from this method. */
    int rc = RTCritSectEnter(&g_cleanupCritSect);
    if (RT_FAILURE(rc))
    {
        RTPrintf("VBoxClient: Failure while acquiring the global critical section, rc=%Rrc\n", rc);
        abort();
    }
    if (g_pService)
        g_pService->cleanup();
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
    VBoxClient::CleanUp();
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
    VBoxClient::CleanUp();
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
static unsigned long getXOrgVT(void)
{
    Display *pDisplay;
    Atom actualType;
    int actualFormat;
    unsigned long cItems, cbLeft, cVT = 0;
    unsigned long *pValue;

    pDisplay = XOpenDisplay(NULL);
    if (!pDisplay)
        return VINF_SUCCESS;
    XGetWindowProperty(pDisplay, DefaultRootWindow(pDisplay),
                       XInternAtom(pDisplay, "XFree86_VT", False), 0, 1, False,
                       XA_INTEGER, &actualType, &actualFormat, &cItems, &cbLeft,
                       (unsigned char **)&pValue);
    if (cItems && actualFormat == 32)
    {
        cVT = *pValue;
        XFree(pValue);
    }
    XCloseDisplay(pDisplay);
    return cVT;
}

#ifdef RT_OS_LINUX
/** Poll for TTY changes using sysfs. Reading from the start of the pollable
 * file "/sys/class/tty/tty0/active" returns the currently active TTY as a
 * string of the form "tty<n>", with n greater than zero.  Polling for POLLPRI
 * returns when the TTY changes. */
static bool pollTTYSysfs(uint32_t cVT)
{
    RTFILE hFile;
    struct pollfd pollFD;

    if (RT_SUCCESS(RTFileOpen(&hFile, "/sys/class/tty/tty0/active",
                          RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN)))
    {
        pollFD.fd = RTFileToNative(hFile);
        pollFD.events = POLLPRI;
        while (true)
        {
            char szTTY[7];
            uint32_t cTTY;
            size_t cbRead;

            if (RT_FAILURE(RTFileReadAt(hFile, 0, (void *)szTTY, sizeof(szTTY),
                           &cbRead)))
            {
                LogRel(("VT monitor thread: read failed, stopping.\n"));
                break;
            }
            szTTY[6] = '\0';
            cTTY = RTStrToUInt32(&szTTY[3]);
            if (!cTTY)
            {
                LogRel(("VT monitor thread: failed to read the TTY, stopping.\n"));
                break;
            }
            if (cTTY == cVT)
                g_pService->resume();
            else
                g_pService->pause();
            /* If we get caught in a tight loop for some reason try to limit the
             * damage. */
            if (poll(&pollFD, 1, 0) > 0)
            {
                LogRel(("VT monitor thread: unexpectedly fast event, revents=0x%x.\n",
                        pollFD.revents));
                RTThreadYield();
            }
            if (   (poll(&pollFD, 1, -1) < 0 && errno != EINVAL)
                || pollFD.revents & POLLNVAL)
            {
                LogRel(("VT monitor thread: poll failed, stopping.\n"));
                break;
            }
        }
        RTFileClose(hFile);
    }
    else
        return false;
    return true;
}
#endif /* defined RT_OS_LINUX */

#if 0
/** @note This is disabled because I believe that our existing code works well
 *        enough on the pre-Linux 2.6.38 systems where it would be needed. 
 *        I will leave the code in place in case that proves to be wrong. */
/** Here we monitor the active VT by performing a VT_STATE ioctl on
 * /dev/console at regular intervals.  Unfortunately we can only monitor the
 * first sixteen virtual terminals this way, so if the X server is running on
 * a higher one we do not even try. */
static bool pollTTYDevConsole(unsigned long cVT)
{
    RTFILE hFile;

    if (cVT >= 16)
        return false;
    if (RT_SUCCESS(RTFileOpen(&hFile, "/dev/console",
                      RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN)))
    {
        while (true)
        {
            struct vt_stat vtStat;
            int rc;

            if (   RT_FAILURE(RTFileIoCtl(hFile, VT_GETSTATE, &vtStat,
                              sizeof(vtStat), &rc))
                || rc)
            {
                LogRel(("VT monitor thread: ioctl failed, stopping.\n"));
                break;
            }
            if (vtStat.v_active == cVT)
                g_pService->resume();
            else
                g_pService->pause();
            RTThreadSleep(1000);
        }
        RTFileClose(hFile);
    }
    else
        return false;
    return true;
}
#endif

#ifdef RT_OS_LINUX
/**
 * Thread which notifies the service when we switch to a different VT or back.
 * @note runs until programme exit.
 * @todo take "g_cleanupCritSect" while pausing/resuming the service to ensure
 *       that they do not happen after clean-up starts.  We actually want to
 *       prevent racing with the static destructor for the service object, as
 *       there is (I think) no guarantee that the VT thread will stop before the
 *       destructor is called on programme exit.
 */
static int pfnVTMonitorThread(RTTHREAD self, void *pvUser)
{
    unsigned long cVT;
    RTFILE hFile;
    
    cVT = getXOrgVT();
    if (!cVT)
        return VINF_SUCCESS;
    if (!pollTTYSysfs((uint32_t) cVT))
#if 0  /* Probably not needed, see comment before function. */
        pollTTYDevConsole(cVT);
#else
        true;
#endif
    return VINF_SUCCESS;
}

/**
 * Start the thread which notifies the service when we switch to a different
 * VT or back.  This is best effort functionality (XFree86 4.3 and older do not
 * report their VT via the "XFree86_VT" root window property at all), so we
 * only fail if actual thread creation fails.
 */
static int startVTMonitorThread()
{
    return RTThreadCreate(NULL, pfnVTMonitorThread, NULL, 0,
                          RTTHREADTYPE_INFREQUENT_POLLER, 0, "VT_MONITOR");
}
#endif /* defined RT_OS_LINUX */

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
 */
int main(int argc, char *argv[])
{
    if (!XInitThreads())
        return 1;
    /* Initialise our runtime before all else. */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    int rcClipboard;
    const char *pszFileName = RTPathFilename(argv[0]);
    bool fDaemonise = true;
    /* Have any fatal errors occurred yet? */
    bool fSuccess = true;
    /* Do we know which service we wish to run? */
    bool fHaveService = false;

    if (NULL == pszFileName)
        pszFileName = "VBoxClient";

    /* Initialise our global clean-up critical section */
    rc = RTCritSectInit(&g_cleanupCritSect);
    if (RT_FAILURE(rc))
    {
        /* Of course, this should never happen. */
        RTPrintf("%s: Failed to initialise the global critical section, rc=%Rrc\n", pszFileName, rc);
        return 1;
    }

    /* Parse our option(s) */
    /** @todo Use RTGetOpt() if the arguments become more complex. */
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--nodaemon"))
            fDaemonise = false;
        else if (!strcmp(argv[i], "--clipboard"))
        {
            if (g_pService == NULL)
                g_pService = VBoxClient::GetClipboardService();
            else
                fSuccess = false;
        }
        else if (!strcmp(argv[i], "--display"))
        {
            if (g_pService == NULL)
                g_pService = VBoxClient::GetDisplayService();
            else
                fSuccess = false;
        }
        else if (!strcmp(argv[i], "--seamless"))
        {
            if (g_pService == NULL)
                g_pService = VBoxClient::GetSeamlessService();
            else
                fSuccess = false;
        }
        else if (!strcmp(argv[i], "--checkhostversion"))
        {
            if (g_pService == NULL)
                g_pService = VBoxClient::GetHostVersionService();
            else
                fSuccess = false;
        }
#ifdef VBOX_WITH_DRAG_AND_DROP
        else if (!strcmp(argv[i], "--draganddrop"))
        {
            if (g_pService == NULL)
                g_pService = VBoxClient::GetDragAndDropService();
            else
                fSuccess = false;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP */
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            vboxClientUsage(pszFileName);
            return 0;
        }
        else
        {
            RTPrintf("%s: unrecognized option `%s'\n", pszFileName, argv[i]);
            RTPrintf("Try `%s --help' for more information\n", pszFileName);
            return 1;
        }
    }
    if (!fSuccess || !g_pService)
    {
        vboxClientUsage(pszFileName);
        return 1;
    }
    /* Get the path for the pidfiles */
    rc = RTPathUserHome(g_szPidFile, sizeof(g_szPidFile));
    if (RT_FAILURE(rc))
    {
        RTPrintf("VBoxClient: failed to get home directory, rc=%Rrc.  Exiting.\n", rc);
        LogRel(("VBoxClient: failed to get home directory, rc=%Rrc.  Exiting.\n", rc));
        return 1;
    }
    rc = RTPathAppend(g_szPidFile, sizeof(g_szPidFile), g_pService->getPidFilePath());
    if (RT_FAILURE(rc))
    {
        RTPrintf("VBoxClient: RTPathAppend failed with rc=%Rrc.  Exiting.\n", rc);
        LogRel(("VBoxClient: RTPathAppend failed with rc=%Rrc.  Exiting.\n", rc));
        return 1;
    }

    /* Initialise the guest library. */
    if (RT_FAILURE(VbglR3InitUser()))
    {
        RTPrintf("Failed to connect to the VirtualBox kernel service\n");
        LogRel(("Failed to connect to the VirtualBox kernel service\n"));
        return 1;
    }
    if (fDaemonise)
    {
        rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */);
        if (RT_FAILURE(rc))
        {
            RTPrintf("VBoxClient: failed to daemonize.  Exiting.\n");
            LogRel(("VBoxClient: failed to daemonize.  Exiting.\n"));
# ifdef DEBUG
            RTPrintf("Error %Rrc\n", rc);
# endif
            return 1;
        }
    }
    if (g_szPidFile[0] && RT_FAILURE(VbglR3PidFile(g_szPidFile, &g_hPidFile)))
    {
        RTPrintf("Failed to create a pidfile.  Exiting.\n");
        LogRel(("Failed to create a pidfile.  Exiting.\n"));
        VbglR3Term();
        return 1;
    }
    /* Set signal handlers to clean up on exit. */
    vboxClientSetSignalHandlers();
    /* Set an X11 error handler, so that we don't die when we get unavoidable errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    /* Set an X11 I/O error handler, so that we can shutdown properly on fatal errors. */
    XSetIOErrorHandler(vboxClientXLibIOErrorHandler);
#ifdef RT_OS_LINUX
    rc = startVTMonitorThread();
    if (RT_FAILURE(rc))
    {
        RTPrintf("Failed to start the VT monitor thread (%Rrc).  Exiting.\n",
                 rc);
        LogRel(("Failed to start the VT monitor thread (%Rrc).  Exiting.\n",
                 rc));
        VbglR3Term();
        return 1;
    }
#endif /* defined RT_OS_LINUX */
    g_pService->run(fDaemonise);
    VBoxClient::CleanUp();
    return 1;  /* We should never get here. */
}
