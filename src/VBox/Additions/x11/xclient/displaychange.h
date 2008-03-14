/** @file
 *
 * Guest client: display auto-resize.
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

#ifndef __Additions_client_display_change_h
# define __Additions_client_display_change_h

#include <VBox/log.h>
#include <VBox/VBoxGuest.h>         /* for the R3 guest library functions  */

#include "thread.h"                 /* for VBoxGuestThread */

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD)

#include <X11/Xlib.h>

/**
 * Display change request monitor thread function
 */
class VBoxGuestDisplayChangeThreadX11 : public VBoxGuestThreadFunction
{
private:
    // Copying or assigning a thread object is not sensible
    VBoxGuestDisplayChangeThreadX11(const VBoxGuestDisplayChangeThreadX11&);
    VBoxGuestDisplayChangeThreadX11& operator=(const VBoxGuestDisplayChangeThreadX11&);

    // Private member variables
    /** Have we been initialised yet? */
    bool mInit;
    /** The thread object running us. */
    VBoxGuestThread *mThread;
public:
    VBoxGuestDisplayChangeThreadX11()
    {
        mInit = false;
    }
    ~VBoxGuestDisplayChangeThreadX11()
    {
        LogFlowThisFunc(("\n"));
        if (mInit)
        {
            try
            {
                uninit();
            }
            catch(...) {}
        }
        LogFlowThisFunc(("returning\n"));
    }
    /**
     * Initialise the class and check that the guest supports dynamic resizing.
     * @returns iprt status value
     */
    int init(void);
    /**
     * Uninitialise the class
     */
    void uninit(void);
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

typedef VBoxGuestDisplayChangeThreadX11 VBoxGuestDisplayChangeThread;
#else
/* Just in case anyone else ever uses this */
# error Port me!
#endif

/**
 * Monitor for and dispatch display change events
 */
class VBoxGuestDisplayChangeMonitor
{
private:
    // No copying or assignment
    VBoxGuestDisplayChangeMonitor(const VBoxGuestDisplayChangeMonitor&);
    VBoxGuestDisplayChangeMonitor& operator=(const VBoxGuestDisplayChangeMonitor&);

    // Private member variables
    /** Our monitor thread function */
    VBoxGuestDisplayChangeThread mThreadFunction;
    /** And the thread for the thread function */
    VBoxGuestThread mThread;
    /** Are we initialised? */
    bool mInit;
public:
    /**
     * Initialise the class.
     * @returns iprt status value
     */
    int init(void);
    /**
     * Uninitialise the class.
     * @param cMillies how long to wait for the thread to stop
     */
    void uninit(unsigned cMillies = RT_INDEFINITE_WAIT);
    VBoxGuestDisplayChangeMonitor() : mThread(&mThreadFunction, 0, RTTHREADTYPE_MSG_PUMP,
                                              RTTHREADFLAGS_WAITABLE, "Display change")
    { mInit = false; }
    ~VBoxGuestDisplayChangeMonitor()
    {
        LogFlowThisFunc(("\n"));
        try
        {
            uninit(2000);
        }
        catch(...) {}
        LogFlowThisFunc(("returning\n"));
    }
};

#endif /* __Additions_display_change_h not defined */
