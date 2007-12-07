/** @file
 *
 * Guest client: seamless mode
 * Abstract class for interacting with the guest system
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#ifndef __Additions_client_seamless_guest_h
# define __Additions_client_seamless_guest_h

#include <memory>            /* for auto_ptr */
#include <vector>            /* for vector */

#include <iprt/types.h>      /* for RTRECT */

#include "seamless-glue.h"

/**
 * Observable to monitor the state of the guest windows.  This abstract class definition
 * serves as a template (in the linguistic sense, not the C++ sense) for creating
 * platform-specific child classes.
 */
class VBoxGuestSeamlessGuest
{
public:
    /** Events which can be reported by this class */
    enum meEvent
    {
        /** Empty event */
        NONE,
        /** Seamless mode is now supported */
        CAPABLE,
        /** Seamless mode is no longer supported */
        INCAPABLE
    };

    /**
     * Initialise the guest and ensure that it is capable of handling seamless mode
     *
     * @param   pObserver An observer object to which to report changes in state and events
     *                    by calling its notify() method.  A state change to CAPABLE also
     *                    signals new seamless data.
     * @returns iprt status code
     */
    int init(VBoxGuestSeamlessObserver *pObserver);

    /**
     * Shutdown seamless event monitoring.
     */
    void uninit(void);

    /**
     * Initialise seamless event reporting in the guest.
     *
     * @returns IPRT status code
     */
    int start(void);
    /** Stop reporting seamless events. */
    void stop(void);
    /** Get the current state of the guest (capable or incapable of seamless mode). */
    // meEvent getState(void);
    /** Get the current list of visible rectangles. */
    std::auto_ptr<std::vector<RTRECT> > getRects(void);
};

#if defined(RT_OS_LINUX)
# include "seamless-x11.h"  /* for VBoxGuestSeamlessGuestImpl */
#else
# error Port me
#endif

#endif /* __Additions_client_seamless_guest_h not defined */
