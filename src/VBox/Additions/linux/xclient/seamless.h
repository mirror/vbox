/** @file
 *
 * Guest client: seamless mode.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#ifndef __Additions_xclient_seamless_h
# define __Additions_xclient_seamless_h

#include "seamless-host.h"
#include "seamless-guest.h"
#include "seamless-glue.h"

/** Thread function class for VBoxGuestSeamlessX11. */
class VBoxGuestSeamlessGuestThread: public VBoxGuestThreadFunction
{
private:
    /** The guest class "owning" us. */
    VBoxGuestSeamlessGuestImpl *mGuest;
    /** Should we exit the thread? */
    bool mExit;

    // Copying or assigning a thread object is not sensible
    VBoxGuestSeamlessGuestThread(const VBoxGuestSeamlessGuestThread&);
    VBoxGuestSeamlessGuestThread& operator=(const VBoxGuestSeamlessGuestThread&);

public:
    VBoxGuestSeamlessGuestThread(VBoxGuestSeamlessGuestImpl *pGuest)
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
        while (!pThread->isStopping)
        {
            mGuest->nextEvent();
        }
        return VINF_SUCCESS;
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
    VBoxGuestSeamlessGuestImpl *mGuest;

public:
    VBoxGuestSeamlessHostObserver(VBoxGuestSeamlessHost *pHost,
                                  VBoxGuestSeamlessGuestImpl *pGuest)
    {
        mHost = pHost;
        mGuest = pGuest;
    }

    void notify(void)
    {
        switch (mHost->getState())
        {
        case VBoxGuestSeamlessGuest::ENABLE:
             mGuest->start();
            break;
        case VBoxGuestSeamlessGuest::DISABLE:
             mGuest->stop();
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

    void notify(void)
    {
        mHost->updateRects(mGuest->getRects());
    }
};

class VBoxGuestSeamless
{
private:
    VBoxGuestSeamlessHost mHost;
    VBoxGuestSeamlessGuestImpl mGuest;
    VBoxGuestSeamlessHostObserver mHostObs;
    VBoxGuestSeamlessGuestObserver mGuestObs;
    VBoxGuestSeamlessGuestThread mGuestFunction;
    VBoxGuestThread mGuestThread;

    bool isInitialised;
public:
    int init(void)
    {
        int rc = VINF_SUCCESS;

        if (isInitialised)  /* Assertion */
        {
            LogRelFunc(("error: called a second time!\n"));
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
            rc = mGuestThread.start();
        }
        isInitialised = true;
        return rc;
    }

    void uninit(void)
    {
        if (isInitialised)
        {
            mGuestThread.stop();
            mHost.stop();
            mGuest.uninit();
            isInitialised = false;
        }
    }

    VBoxGuestSeamless() : mHostObs(&mHost, &mGuest), mHostObs(&mHost, &mGuest),
                          mGuestFunction(&mGuest), mGuestThread(&mGuestFunction)
    {
        isInitialised = false;
    }
    ~VBoxGuestSeamless() { uninit(); }
};

#endif /* __Additions_xclient_seamless_h not defined */
