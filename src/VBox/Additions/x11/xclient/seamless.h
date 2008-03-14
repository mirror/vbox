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

#ifndef __Additions_xclient_seamless_h
# define __Additions_xclient_seamless_h

#include <VBox/log.h>

#include "seamless-host.h"
#include "seamless-guest.h"
#include "seamless-glue.h"

/** Thread function class for VBoxGuestSeamlessGuest. */
class VBoxGuestSeamlessGuestThread: public VBoxGuestThreadFunction
{
private:
    /** The guest class "owning" us. */
    VBoxGuestSeamlessGuestImpl *mGuest;
    /** The guest observer monitoring the guest. */
    VBoxGuestSeamlessObserver *mObserver;
    /** Should we exit the thread? */
    bool mExit;

    // Copying or assigning a thread object is not sensible
    VBoxGuestSeamlessGuestThread(const VBoxGuestSeamlessGuestThread&);
    VBoxGuestSeamlessGuestThread& operator=(const VBoxGuestSeamlessGuestThread&);

public:
    VBoxGuestSeamlessGuestThread(VBoxGuestSeamlessGuestImpl *pGuest,
                                 VBoxGuestSeamlessObserver *pObserver)
    { mGuest = pGuest; mObserver = pObserver; mExit = false; }
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

        LogFlowThisFunc(("\n"));
        rc = mGuest->start();
        if (RT_SUCCESS(rc))
        {
            while (!pThread->isStopping())
            {
                mGuest->nextEvent();
            }
            mGuest->stop();
        }
        LogFlowThisFunc(("returning %Rrc\n", rc));
        return rc;
    }
    /**
     * Send a signal to the thread function that it should exit
     */
    virtual void stop(void) { mGuest->interruptEvent(); }
};

/** Observer for the host class - start and stop seamless reporting in the guest when the
    host requests. */
class VBoxGuestSeamlessHostObserver : public VBoxGuestSeamlessObserver
{
private:
    VBoxGuestSeamlessHost *mHost;
    VBoxGuestThread *mGuestThread;

public:
    VBoxGuestSeamlessHostObserver(VBoxGuestSeamlessHost *pHost,
                                  VBoxGuestThread *pGuestThread)
    {
        mHost = pHost;
        mGuestThread = pGuestThread;
    }

    virtual void notify(void)
    {
        switch (mHost->getState())
        {
        case VBoxGuestSeamlessHost::ENABLE:
             mGuestThread->start();
            break;
        case VBoxGuestSeamlessHost::DISABLE:
             mGuestThread->stop(RT_INDEFINITE_WAIT, 0);
            break;
        default:
            break;
        }
    }
};

/** Observer for the guest class - send the host updated seamless rectangle information when
    it becomes available. */
class VBoxGuestSeamlessGuestObserver : public VBoxGuestSeamlessObserver
{
private:
    VBoxGuestSeamlessHost *mHost;
    VBoxGuestSeamlessGuestImpl *mGuest;

public:
    VBoxGuestSeamlessGuestObserver(VBoxGuestSeamlessHost *pHost,
                                   VBoxGuestSeamlessGuestImpl *pGuest)
    {
        mHost = pHost;
        mGuest = pGuest;
    }

    virtual void notify(void)
    {
        mHost->updateRects(mGuest->getRects());
    }
};

class VBoxGuestSeamless
{
private:
    VBoxGuestSeamlessHost mHost;
    VBoxGuestSeamlessGuestImpl mGuest;
    VBoxGuestSeamlessGuestThread mGuestFunction;
    VBoxGuestThread mGuestThread;
    VBoxGuestSeamlessHostObserver mHostObs;
    VBoxGuestSeamlessGuestObserver mGuestObs;

    bool isInitialised;
public:
    int init(void)
    {
        int rc = VINF_SUCCESS;

        LogFlowThisFunc(("\n"));
        if (isInitialised)  /* Assertion */
        {
            LogRelFunc(("error: called a second time! (VBoxClient)\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        if (RT_SUCCESS(rc))
        {
            rc = mHost.init(&mHostObs);
        }
        if (RT_SUCCESS(rc))
        {
            rc = mGuest.init(&mGuestObs);
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
            LogFunc(("returning %Rrc (VBoxClient)\n", rc));
        }
        LogFlowThisFunc(("returning %Rrc\n", rc));
        return rc;
    }

    void uninit(unsigned cMillies = RT_INDEFINITE_WAIT)
    {
        LogFlowThisFunc(("\n"));
        if (isInitialised)
        {
            mHost.stop();
            mGuestThread.stop(cMillies, 0);
            mGuest.uninit();
            isInitialised = false;
        }
        LogFlowThisFunc(("returning\n"));
    }

    VBoxGuestSeamless() : mGuestFunction(&mGuest, &mGuestObs),
                          mGuestThread(&mGuestFunction, 0, RTTHREADTYPE_MSG_PUMP,
                                       RTTHREADFLAGS_WAITABLE, "Guest events"),
                          mHostObs(&mHost, &mGuestThread), mGuestObs(&mHost, &mGuest)
    {
        isInitialised = false;
    }
    ~VBoxGuestSeamless() { uninit(); }
};

#endif /* __Additions_xclient_seamless_h not defined */
