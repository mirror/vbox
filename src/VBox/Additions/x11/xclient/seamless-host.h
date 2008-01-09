/** @file
 *
 * Guest client: seamless mode.
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

#ifndef __Additions_client_seamless_host_h
# define __Additions_client_seamless_host_h

#include <memory>                   /* for auto_ptr */
#include <vector>                   /* for vector */

#include <VBox/VBoxGuest.h>         /* for the R3 guest library functions  */

#include "seamless-glue.h"          /* for VBoxGuestSeamlessObserver */
#include "thread.h"                 /* for VBoxGuestThread */

class VBoxGuestSeamlessHost;

/**
 * Host event (i.e. enter or leave seamless mode) thread function for the main
 * seamless class
 */
class VBoxGuestSeamlessHostThread : public VBoxGuestThreadFunction
{
private:
    // Copying or assigning a thread object is not sensible
    VBoxGuestSeamlessHostThread(const VBoxGuestSeamlessHostThread&);
    VBoxGuestSeamlessHostThread& operator=(const VBoxGuestSeamlessHostThread&);

    // Private member variables
    /** The host proxy object */
    VBoxGuestSeamlessHost *mHost;

    /** The thread object running us. */
    VBoxGuestThread *mThread;
public:
    VBoxGuestSeamlessHostThread(VBoxGuestSeamlessHost *pHost)
    {
        mHost = pHost;
    }
    virtual ~VBoxGuestSeamlessHostThread(void) {}
    /**
      * The actual thread function.
      *
      * @returns iprt status code as thread return value
      * @param pParent the VBoxGuestThread running this thread function
      */
    virtual int threadFunction(VBoxGuestThread *pThread);
    /**
     * Send a signal to the thread function that it should exit
     */
    virtual void stop(void);
};

/**
 * Interface to the host
 */
class VBoxGuestSeamlessHost
{
    friend class VBoxGuestSeamlessHostThread;
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

    /** Observer to connect guest and host and ferry events back and forth. */
    VBoxGuestSeamlessObserver *mObserver;
    /** Host seamless event (i.e. enter and leave) thread function. */
    VBoxGuestSeamlessHostThread mThreadFunction;
    /** Host seamless event thread. */
    VBoxGuestThread mThread;
    /** Is the service running? */
    bool mRunning;
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

public:
    /**
     * Initialise the guest and ensure that it is capable of handling seamless mode
     * @param   pObserver Observer class to connect host and guest interfaces
     *
     * @returns iprt status code
     */
    int init(VBoxGuestSeamlessObserver *pObserver)
    {
        if (mObserver != 0)  /* Assertion */
        {
            LogRel(("VBoxService: ERROR: attempt to initialise seamless host object twice!\n"));
            return VERR_INTERNAL_ERROR;
        }
        mObserver = pObserver;
        return VINF_SUCCESS;
    }

    /**
      * Start the service.
      * @returns iprt status value
      */
    int start(void);

    /** Stops the service. */
    void stop(void);

    /** Returns the current state of the host - i.e. requesting seamless or not. */
    meEvent getState(void) { return mState; }

    /**
     * Update the set of visible rectangles in the host.
     */
    void updateRects(std::auto_ptr<std::vector<RTRECT> > pRects);

    VBoxGuestSeamlessHost(void) : mThreadFunction(this),
                                  mThread(&mThreadFunction, 0, RTTHREADTYPE_MAIN_WORKER,
                                  RTTHREADFLAGS_WAITABLE, "Host events")
    {
        mObserver = 0;
        mRunning = false;
        mState = NONE;
    }

    ~VBoxGuestSeamlessHost()
    {
        if (mRunning)  /* Assertion */
        {
            LogRel(("VBoxService: seamless host object still running!  Stopping...\n"));
            stop();
        }
    }
};

#endif /* __Additions_xclient_seamless_h not defined */
