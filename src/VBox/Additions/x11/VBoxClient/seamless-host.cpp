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

/*****************************************************************************
*   Header files                                                             *
*****************************************************************************/
#include <VBox/log.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/err.h>

#include "seamless-host.h"

/**
 * Start the service.
 * @returns iprt status value
 */
int VBoxGuestSeamlessHost::start(void)
{
    int rc = VERR_NOT_SUPPORTED;

    LogRelFlowFunc(("\n"));
    if (mRunning)  /* Assertion */
    {
        LogRel(("VBoxClient: seamless service started twice!\n"));
        return VERR_INTERNAL_ERROR;
    }
    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxClient (seamless): failed to set the guest IRQ filter mask, rc=%Rrc\n", rc));
    }
    rc = VbglR3SeamlessSetCap(true);
    if (RT_SUCCESS(rc))
    {
        LogRel(("VBoxClient: enabled seamless capability on host.\n"));
        rc = mThread.start();
        if (RT_SUCCESS(rc))
        {
            mRunning = true;
        }
        else
        {
            LogRel(("VBoxClient: failed to start seamless event thread, rc=%Rrc.  Disabled seamless capability on host again.\n", rc));
            VbglR3SeamlessSetCap(false);
        }
    }
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxClient (seamless): failed to enable seamless capability on host, rc=%Rrc\n", rc));
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/** Stops the service. */
void VBoxGuestSeamlessHost::stop(RTMSINTERVAL cMillies /* = RT_INDEFINITE_WAIT */)
{
    LogRelFlowFunc(("returning\n"));
    if (!mRunning)  /* Assertion */
    {
        LogRel(("VBoxClient: tried to stop seamless service which is not running!\n"));
        return;
    }
    mThread.stop(cMillies, 0);
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST);
    VbglR3SeamlessSetCap(false);
    mRunning = false;
    LogRelFlowFunc(("returning\n"));
}

/**
 * Waits for a seamless state change events from the host and dispatch it.
 *
 * @returns        IRPT return code.
 */
int VBoxGuestSeamlessHost::nextEvent(void)
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
#ifdef DEBUG
                LogRelFunc(("VMMDev_Seamless_Visible_Region request received (VBoxClient).\n"));
#endif
                mState = ENABLE;
                mObserver->notify();
                break;
            case VMMDev_Seamless_Host_Window:
            /* One host window represents one guest window.  Not yet implemented. */
                LogRelFunc(("Warning: VMMDev_Seamless_Host_Window request received (VBoxClient).\n"));
                /* fall through to default */
            default:
                LogRelFunc(("Warning: unsupported VMMDev_Seamless request %d received (VBoxClient).\n", newMode));
                /* fall through to case VMMDev_Seamless_Disabled */
            case VMMDev_Seamless_Disabled:
#ifdef DEBUG
                LogRelFunc(("VMMDev_Seamless_Disabled set (VBoxClient).\n"));
#endif
                mState = DISABLE;
                mObserver->notify();
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
void VBoxGuestSeamlessHost::updateRects(RTRECT *pRects, size_t cRects)
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
 * The actual thread function.
 *
 * @returns iprt status code as thread return value
 * @param pParent the VBoxGuestThread running this thread function
 */
int VBoxGuestSeamlessHostThread::threadFunction(VBoxGuestThread *pThread)
{
    LogRelFlowFunc(("\n"));
    if (0 != mHost)
    {
        mThread = pThread;
        while (!mThread->isStopping())
        {
            if (RT_FAILURE(mHost->nextEvent()) && !mThread->isStopping())
            {
                /* If we are not stopping, sleep for a bit to avoid using up too
                    much CPU while retrying. */
                mThread->yield();
            }
        }
    }
    LogRelFlowFunc(("returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Send a signal to the thread function that it should exit
 */
void VBoxGuestSeamlessHostThread::stop(void)
{
    LogRelFlowFunc(("\n"));
    if (0 != mHost)
    {
        /**
         * @todo is this reasonable?  If the thread is in the event loop then the cancelEvent()
         *       will cause it to exit.  If it enters or exits the event loop it will also
         *       notice that we wish it to exit.  And if it is somewhere in-between, the
         *       yield() should give it time to get to one of places mentioned above.
         */
        for (int i = 0; (i < 5) && mThread->isRunning(); ++i)
        {
            mHost->cancelEvent();
            mThread->yield();
        }
    }
    LogRelFlowFunc(("returning\n"));
}
