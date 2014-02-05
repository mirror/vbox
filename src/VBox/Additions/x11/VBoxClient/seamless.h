/** @file
 *
 * Guest client: seamless mode.
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

#ifndef __Additions_xclient_seamless_h
# define __Additions_xclient_seamless_h

#include <VBox/log.h>

#include "seamless-host.h"
#include "seamless-x11.h"

/** Thread function class for VBoxGuestSeamlessX11. */
class VBoxGuestSeamlessGuestThread: public VBoxGuestThreadFunction
{
private:
    /** The guest class "owning" us. */
    VBoxGuestSeamlessX11 *mGuest;
    /** Should we exit the thread? */
    bool mExit;

    // Copying or assigning a thread object is not sensible
    VBoxGuestSeamlessGuestThread(const VBoxGuestSeamlessGuestThread&);
    VBoxGuestSeamlessGuestThread& operator=(const VBoxGuestSeamlessGuestThread&);

public:
    VBoxGuestSeamlessGuestThread(VBoxGuestSeamlessX11 *pGuest)
    { mGuest = pGuest; mExit = false; }
    virtual ~VBoxGuestSeamlessGuestThread(void) {}
    /**
      * The actual thread function.
      *
      * @returns iprt status code as thread return value
      * @param pParent the VBoxGuestThread running this thread function
      */
    virtual int threadFunction(VBoxGuestThread *pThread)
    {
        int rc = VINF_SUCCESS;

        LogRelFlowFunc(("\n"));
        rc = mGuest->start();
        if (RT_SUCCESS(rc))
        {
            while (!pThread->isStopping())
            {
                mGuest->nextEvent();
            }
            mGuest->stop();
        }
        LogRelFlowFunc(("returning %Rrc\n", rc));
        return rc;
    }
    /**
     * Send a signal to the thread function that it should exit
     */
    virtual void stop(void) { mGuest->interruptEvent(); }
};

class VBoxGuestSeamless
{
private:
    VBoxGuestSeamlessHost mHost;
    VBoxGuestSeamlessX11 mGuest;
    VBoxGuestSeamlessGuestThread mGuestFunction;
    VBoxGuestThread mGuestThread;

    bool isInitialised;
public:
    int init(void)
    {
        int rc = VINF_SUCCESS;

        LogRelFlowFunc(("\n"));
        if (isInitialised)  /* Assertion */
        {
            LogRelFunc(("error: called a second time! (VBoxClient)\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        if (RT_SUCCESS(rc))
        {
            rc = mHost.init(&mGuestThread);
        }
        if (RT_SUCCESS(rc))
        {
            rc = mGuest.init(&mHost);
        }
        if (RT_SUCCESS(rc))
        {
            rc = mHost.start();
        }
        if (RT_SUCCESS(rc))
        {
            isInitialised = true;
        }
        if (RT_FAILURE(rc))
        {
            LogRelFunc(("returning %Rrc (VBoxClient)\n", rc));
        }
        LogRelFlowFunc(("returning %Rrc\n", rc));
        return rc;
    }

    void uninit(RTMSINTERVAL cMillies = RT_INDEFINITE_WAIT)
    {
        LogRelFlowFunc(("\n"));
        if (isInitialised)
        {
            mHost.stop(cMillies);
            mGuestThread.stop(cMillies, 0);
            mGuest.uninit();
            isInitialised = false;
        }
        LogRelFlowFunc(("returning\n"));
    }

    VBoxGuestSeamless() : mGuestFunction(&mGuest),
                          mGuestThread(&mGuestFunction, 0, RTTHREADTYPE_MSG_PUMP,
                                       RTTHREADFLAGS_WAITABLE, "Guest events")
    {
        isInitialised = false;
    }
    ~VBoxGuestSeamless() { uninit(); }
};

#endif /* __Additions_xclient_seamless_h not defined */
