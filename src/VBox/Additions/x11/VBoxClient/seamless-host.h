/** @file
 * X11 Guest client - seamless mode, missing proper description while using the
 * potentially confusing word 'host'.
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

#ifndef __Additions_client_seamless_host_h
# define __Additions_client_seamless_host_h

#include <iprt/thread.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>      /* for the R3 guest library functions  */

class VBoxGuestSeamlessX11;

/**
 * Small virtual class which provides the interface for notifying the host of
 * changes to the X11 window configuration, mainly split out from
 * @a VBoxGuestSeamlessHost to simplify the unit test.
 */
class VBoxGuestSeamlessHostInt
{
public:
    virtual void notify(RTRECT *pRects, size_t cRects) = 0;
};

/**
 * Interface to the host
 */
class VBoxGuestSeamlessHost : public VBoxGuestSeamlessHostInt
{
public:
    /** Events which can be reported by this class */
    enum meEvent
    {
        /** Empty event */
        NONE,
        /** Request to enable seamless mode */
        ENABLE,
        /** Request to disable seamless mode */
        DISABLE
    };

private:
    // We don't want a copy constructor or assignment operator
    VBoxGuestSeamlessHost(const VBoxGuestSeamlessHost&);
    VBoxGuestSeamlessHost& operator=(const VBoxGuestSeamlessHost&);

    /** Thread to start and stop when we enter and leave seamless mode which
     *  monitors X11 windows in the guest. */
    RTTHREAD mX11MonitorRTThread;
    /** X11 event monitor class */
    VBoxGuestSeamlessX11 *mX11Monitor;
    /** Should the X11 monitor thread be stopping? */
    volatile bool mX11ThreadStopping;
    /** Host seamless event thread. */
    RTTHREAD mThread;
    /** Is the thread running? */
    volatile bool mThreadRunning;
    /** Should the thread be stopping? */
    volatile bool mThreadStopping;
    /** Last request issued by the host. */
    meEvent mState;

    /**
     * Waits for a seamless state change events from the host and dispatch it.  This is
     * meant to be called by the host event monitor thread exclusively.
     *
     * @returns        IRPT return code.
     */
    int nextEvent(void);

    /**
     * Interrupt an event wait and cause nextEvent() to return immediately.
     */
    void cancelEvent(void) { VbglR3InterruptEventWaits(); }
    
    /** Thread function to query seamless activation and deactivation events
     *  from the host. */
    static DECLCALLBACK(int) threadFunction(RTTHREAD self, void *pvUser);

    /** Helper to stop the event query thread again. */
    void stopThread(RTMSINTERVAL cMillies);

    /** Thread function to monitor X11 window configuration changes. */
    static DECLCALLBACK(int) x11ThreadFunction(RTTHREAD self, void *pvUser);

    /** Helper to stop the X11 monitor thread again. */
    void stopX11Thread(void);

public:
    /**
     * Initialise the guest and ensure that it is capable of handling seamless mode
     * @param   pX11Monitor Object to monitor X11 guest windows.
     *
     * @returns iprt status code
     */
    int init(VBoxGuestSeamlessX11 *pX11Monitor)
    {
        LogRelFlowFunc(("\n"));
        if (mX11Monitor != NULL)  /* Assertion */
        {
            LogRel(("VBoxClient: ERROR: attempt to initialise seamless host object twice!\n"));
            return VERR_INTERNAL_ERROR;
        }
        mX11Monitor = pX11Monitor;
        LogRelFlowFunc(("returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }

    /**
      * Start the service.
      * @returns iprt status value
      */
    int start(void);

    /**
     * Stops the service.
     * @param cMillies how long to wait for the thread to exit
     */
    void stop(RTMSINTERVAL cMillies = RT_INDEFINITE_WAIT);

    /** Returns the current state of the host - i.e. requesting seamless or not. */
    meEvent getState(void) { return mState; }

    /**
     * Update the set of visible rectangles in the host.
     */
    virtual void notify(RTRECT *pRects, size_t cRects);

    VBoxGuestSeamlessHost(void)
    {
        mX11MonitorRTThread = NIL_RTTHREAD;
        mX11Monitor = NULL;
        mX11ThreadStopping = false;
        mThread = NIL_RTTHREAD;
        mThreadRunning = false;
        mThreadStopping = false;
        mState = NONE;
    }

    ~VBoxGuestSeamlessHost()
    {
        LogRelFlowFunc(("\n"));
        if (mThread)  /* Assertion */
        {
            LogRel(("VBoxClient: seamless host object still running!  Stopping...\n"));
            stop(2000);
        }
        LogRelFlowFunc(("returning\n"));
    }
};

#endif /* __Additions_xclient_seamless_h not defined */
