/** @file
 * X11 Guest client - seamless mode: main logic, communication with the host and
 * wrapper interface for the main code of the VBoxClient deamon.  The
 * X11-specific parts are split out into their own file for ease of testing.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*****************************************************************************
*   Header files                                                             *
*****************************************************************************/

#include <X11/Xlib.h>

#include <VBox/log.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/err.h>

#include "VBoxClient.h"
#include "seamless.h"

SeamlessMain::SeamlessMain(void)
{
    LogRelFlowFunc(("\n"));
    mX11MonitorThread = NIL_RTTHREAD;
    mX11MonitorThreadStopping = false;
    mHostEventThread = NIL_RTTHREAD;
    mHostEventThreadRunning = false;
    mHostEventThreadStopping = false;
}

SeamlessMain::~SeamlessMain()
{
    LogRelFlowFunc(("\n"));
    stop();
}

/**
 * Start the main service thread which listens for host state change
 * notifications.
 * @returns iprt status value.  Service will be set to the stopped state on
 *          failure.
 */
int SeamlessMain::start(void)
{
    int rc;
    const char *pszStage;

    LogRelFlowFunc(("\n"));
    do {
        pszStage = "Checking that we are not already running";
        rc = VERR_INTERNAL_ERROR;
        if (mHostEventThread)  /* Assertion */
            break;
        pszStage = "Testing event loop cancellation";
        VbglR3InterruptEventWaits();
        if (RT_FAILURE(VbglR3WaitEvent(VMMDEV_EVENT_VALID_EVENT_MASK, 0, NULL)))
            break;
        if (   VbglR3WaitEvent(VMMDEV_EVENT_VALID_EVENT_MASK, 0, NULL)
            != VERR_TIMEOUT)
            break;
        pszStage = "Connecting to the X server";
        rc = mX11Monitor.init(this);
        if (RT_FAILURE(rc))
            break;
        /* Create a thread to wait for requests from the host.  This is currently
         * done on a separate thread as the main thread monitors the X11 server
         * for disconnections. */
        /** @todo Move the disconnection monitoring to its own thread (better, the
         *  VT monitor thread) and run this logic on the main service thread. */
        pszStage = "Starting host event thread";
        rc = startHostEventThread();
        if (RT_FAILURE(rc))
            break;
        pszStage = "Setting guest IRQ filter mask";
        rc = VbglR3CtlFilterMask(VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST, 0);
        if (RT_FAILURE(rc))
            break;
        pszStage = "Reporting support for seamless capability";
        rc = VbglR3SeamlessSetCap(true);
        if (RT_FAILURE(rc))
            break;
    } while(0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxClient (seamless): failed to start.  Stage: \"%s\"  Error: %Rrc\n",
                pszStage, rc));
        stop();
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/** Stops the service. */
void SeamlessMain::stop()
{
    LogRelFlowFunc(("\n"));
    VbglR3SeamlessSetCap(false);
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST);
    if (mHostEventThread)
        stopHostEventThread();
    stopX11MonitorThread();
    mX11Monitor.uninit();
    LogRelFlowFunc(("returning\n"));
}

/**
 * Waits for a seamless state change events from the host and dispatch it.
 *
 * @returns        IRPT return code.
 */
int SeamlessMain::nextStateChangeEvent(void)
{
    VMMDevSeamlessMode newMode = VMMDev_Seamless_Disabled;

    LogRelFlowFunc(("\n"));
    int rc = VbglR3SeamlessWaitEvent(&newMode);
    if (RT_SUCCESS(rc))
    {
        switch(newMode)
        {
            case VMMDev_Seamless_Visible_Region:
            /* A simplified seamless mode, obtained by making the host VM window borderless and
              making the guest desktop transparent. */
                LogRelFlowFunc(("VMMDev_Seamless_Visible_Region request received (VBoxClient).\n"));
                /** @todo Do something on failure, like bail out. */
                startX11MonitorThread();
                break;
            case VMMDev_Seamless_Host_Window:
            /* One host window represents one guest window.  Not yet implemented. */
                LogRelFunc(("Warning: VMMDev_Seamless_Host_Window request received (VBoxClient).\n"));
                /* fall through to default */
            default:
                LogRelFunc(("Warning: unsupported VMMDev_Seamless request %d received (VBoxClient).\n", newMode));
                /* fall through to case VMMDev_Seamless_Disabled */
            case VMMDev_Seamless_Disabled:
                LogRelFlowFunc(("VMMDev_Seamless_Disabled set (VBoxClient).\n"));
                stopX11MonitorThread();
        }
    }
    else
    {
        LogRelFunc(("VbglR3SeamlessWaitEvent returned %Rrc (VBoxClient)\n", rc));
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/**
 * Update the set of visible rectangles in the host.
 */
void SeamlessMain::sendRegionUpdate(RTRECT *pRects, size_t cRects)
{
    LogRelFlowFunc(("\n"));
    if (cRects && !pRects)  /* Assertion */
    {
        LogRelThisFunc(("ERROR: called with null pointer!\n"));
        return;
    }
    VbglR3SeamlessSendRects(cRects, pRects);
    LogRelFlowFunc(("returning\n"));
}


/**
 * Thread to listen for seamless state change notifications from the host.
 */
int SeamlessMain::hostEventThread(RTTHREAD self, void *pvUser)
{
    SeamlessMain *pHost = (SeamlessMain *)pvUser;

    LogRelFlowFunc(("\n"));
    pHost->mHostEventThreadRunning = true;
    if (0 != pHost)
    {
        while (!pHost->mHostEventThreadStopping)
        {
            /* This thread is stopped by setting @a mHostEventThreadStopping
             * and sending a cancel to the state change event wait, see below.
             */
            int rc = pHost->nextStateChangeEvent();
            if (RT_FAILURE(rc) && !pHost->mHostEventThreadStopping)
            {
                /* If we are not stopping, sleep for a bit to avoid using up too
                    much CPU while retrying. */
                RTThreadYield();
            }
        }
    }
    pHost->mHostEventThreadRunning = false;
    return VINF_SUCCESS;
}

/**
 * Start the seamless state change notification listener thread.
 */
int SeamlessMain::startHostEventThread()
{
    int rc;

    mHostEventThreadStopping = false;
    rc = RTThreadCreate(&mHostEventThread, hostEventThread, this, 0,
                        RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                        "Host events");
    if (RT_FAILURE(rc))
        LogRel(("VBoxClient: failed to start seamless event thread, rc=%Rrc.\n",
                rc));
    return rc;
}

/**
 * Send a signal to the host event thread that it should exit and poke it.
 */
void SeamlessMain::stopHostEventThread()
{
    int rc;

    LogRelFlowFunc(("\n"));
    mHostEventThreadStopping = true;
    cancelEvent();
    rc = RTThreadWait(mHostEventThread, RT_INDEFINITE_WAIT, NULL);
    if (RT_SUCCESS(rc))
        mHostEventThread = NIL_RTTHREAD;
    else
        LogRelThisFunc(("Failed to stop seamless event thread, rc=%Rrc!\n",
                        rc));
    LogRelFlowFunc(("returning\n"));
}

/**
 * The actual X11 window configuration change monitor thread function.
 */
int SeamlessMain::x11MonitorThread(RTTHREAD self, void *pvUser)
{
    SeamlessMain *pHost = (SeamlessMain *)pvUser;
    int rc = VINF_SUCCESS;

    LogRelFlowFunc(("\n"));
    rc = pHost->mX11Monitor.start();
    if (RT_SUCCESS(rc))
    {
        while (!pHost->mX11MonitorThreadStopping)
            pHost->mX11Monitor.nextConfigurationEvent();
        pHost->mX11Monitor.stop();
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/**
 * Start the X11 window configuration change monitor thread.
 */
int SeamlessMain::startX11MonitorThread(void)
{
    int rc;

    mX11MonitorThreadStopping = false;
    rc = RTThreadCreate(&mX11MonitorThread, x11MonitorThread, this, 0,
                        RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                        "X11 events");
    if (RT_FAILURE(rc))
        LogRelFunc(("Warning: failed to start X11 monitor thread (VBoxClient).\n"));
    return rc;
}

/**
 * Send a signal to the thread function that it should exit
 */
void SeamlessMain::stopX11MonitorThread(void)
{
    int rc;

    mX11MonitorThreadStopping = true;
    if (!mX11MonitorThread)
        return;
    mX11Monitor.interruptEventWait();
    rc = RTThreadWait(mX11MonitorThread, RT_INDEFINITE_WAIT, NULL);
    if (RT_SUCCESS(rc))
        mX11MonitorThread = NIL_RTTHREAD;
    else
        LogRelThisFunc(("Failed to stop X11 monitor thread, rc=%Rrc!\n",
                        rc));
}

/** VBoxClient service class wrapping the logic for the seamless service while
 *  the main VBoxClient code provides the daemon logic needed by all services.
 */
class SeamlessService : public VBoxClient::Service
{
private:
    SeamlessMain mSeamless;
    bool mIsInitialised;
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-seamless.pid";
    }
    virtual int run(bool fDaemonised /* = false */)
    {
        Display *pDisplay = NULL;
        const char *pszStage;
        XEvent ev;
        int rc;

        do {
            pszStage = "Checking that we are not already running";
            rc = VERR_INTERNAL_ERROR;
            if (mIsInitialised)  /* Assertion */
                break;
            pszStage = "Connecting to the X server";
            rc = VERR_INTERNAL_ERROR;
            pDisplay = XOpenDisplay(NULL);
            if (!pDisplay)
                break;
            pszStage = "Starting the service";
            rc = mSeamless.start();
            if (RT_FAILURE(rc))
                break;
        } while(0);
        if (RT_FAILURE(rc))
        {
            LogRelFunc(("VBoxClient seamless service control: failed at stage: \"%s\".  Error: %Rrc\n",
                        pszStage, rc));
            mSeamless.stop();
            if (pDisplay)
                XCloseDisplay(pDisplay);
            return rc;
        }
        mIsInitialised = true;
        /* Stay running as long as X does... */
        while (true)
            XNextEvent(pDisplay, &ev);
        return VERR_INTERRUPTED;
    }
    virtual void cleanup()
    {
        VbglR3SeamlessSetCap(false);
    }
};

VBoxClient::Service *VBoxClient::GetSeamlessService()
{
    return new SeamlessService;
}
