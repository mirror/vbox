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

#include "seamless-x11.h"

/**
 * Interface to the host
 */
class SeamlessMain : public SeamlessHostProxy
{
private:
    // We don't want a copy constructor or assignment operator
    SeamlessMain(const SeamlessMain&);
    SeamlessMain& operator=(const SeamlessMain&);

    /** X11 event monitor object */
    SeamlessX11 mX11Monitor;

    /** Thread to start and stop when we enter and leave seamless mode which
     *  monitors X11 windows in the guest. */
    RTTHREAD mX11MonitorThread;
    /** Should the X11 monitor thread be stopping? */
    volatile bool mX11MonitorThreadStopping;

    /**
     * Waits for a seamless state change events from the host and dispatch it.  This is
     * meant to be called by the host event monitor thread exclusively.
     *
     * @returns        IRPT return code.
     */
    int nextStateChangeEvent(void);

    /**
     * Interrupt an event wait and cause nextStateChangeEvent() to return immediately.
     */
    void cancelEvent(void) { VbglR3InterruptEventWaits(); }
    
    /** Thread function to monitor X11 window configuration changes. */
    static DECLCALLBACK(int) x11MonitorThread(RTTHREAD self, void *pvUser);

    /** Helper to start the X11 monitor thread. */
    int startX11MonitorThread(void);

    /** Helper to stop the X11 monitor thread again. */
    void stopX11MonitorThread(void);

public:
    SeamlessMain(void);
    virtual ~SeamlessMain();

    /**
      * Start the service.
      * @returns iprt status value
      */
    int start(void);

    /**
     * Stops the service.
     */
    void stop();

    /**
     * Update the set of visible rectangles in the host.
     */
    virtual void sendRegionUpdate(RTRECT *pRects, size_t cRects);
};

#endif /* __Additions_xclient_seamless_h not defined */
