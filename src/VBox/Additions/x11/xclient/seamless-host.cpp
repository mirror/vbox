/** @file
 *
 * Seamless mode:
 * Linux guest.
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

/*****************************************************************************
*   Header files                                                             *
*****************************************************************************/

#include <iprt/log.h>
#include <iprt/err.h>

#include "seamless-host.h"

/**
 * Start the service.
 * @returns iprt status value
 */
int VBoxGuestSeamlessHost::start(void)
{
    int rc = VERR_NOT_SUPPORTED;

    if (mRunning)  /* Assertion */
    {
        LogRel(("VBoxService: seamless service started twice!\n"));
        return VERR_INTERNAL_ERROR;
    }
    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxService (seamless): failed to set the guest IRQ filter mask, rc=%Rrc\n", rc));
    }
    rc = VbglR3SeamlessSetCap(true);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxService: enabled seamless capability on host.\n"));
        rc = mThread.start();
        if (RT_SUCCESS(rc))
        {
            mRunning = true;
        }
        else
        {
            LogRel(("VBoxService: failed to start seamless event thread, rc=%Rrc.  Disabled seamless capability on host again.\n", rc));
            VbglR3SeamlessSetCap(false);
        }
    }
    if (RT_FAILURE(rc))
    {
        Log(("VBoxService (seamless): failed to enable seamless capability on host, rc=%Rrc\n", rc));
    }
    return rc;
}

/** Stops the service. */
void VBoxGuestSeamlessHost::stop(unsigned cMillies /* = RT_INDEFINITE_WAIT */)
{
    if (!mRunning)  /* Assertion */
    {
        LogRel(("VBoxService: tried to stop seamless service which is not running!\n"));
        return;
    }
    mThread.stop(cMillies, 0);
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST);
    VbglR3SeamlessSetCap(false);
    mRunning = false;
}

/**
 * Waits for a seamless state change events from the host and dispatch it.
 *
 * @returns        IRPT return code.
 */
int VBoxGuestSeamlessHost::nextEvent(void)
{
    VMMDevSeamlessMode newMode = VMMDev_Seamless_Disabled;

    int rc = VbglR3SeamlessWaitEvent(&newMode);
    if (RT_SUCCESS(rc))
    {
        switch(newMode)
        {
            case VMMDev_Seamless_Visible_Region:
            /* A simplified seamless mode, obtained by making the host VM window borderless and
              making the guest desktop transparent. */
    #ifdef DEBUG
                LogRelFunc(("VMMDev_Seamless_Visible_Region request received (VBoxService).\n"));
    #endif
                mState = ENABLE;
                mObserver->notify();
                break;
            case VMMDev_Seamless_Host_Window:
            /* One host window represents one guest window.  Not yet implemented. */
                LogRelFunc(("Warning: VMMDev_Seamless_Host_Window request received (VBoxService).\n"));
                /* fall through to default */
            default:
                LogRelFunc(("Warning: unsupported VMMDev_Seamless request %d received (VBoxService).\n", newMode));
                /* fall through to case VMMDev_Seamless_Disabled */
            case VMMDev_Seamless_Disabled:
    #ifdef DEBUG
                LogRelFunc(("VMMDev_Seamless_Disabled set (VBoxService).\n"));
    #endif
                mState = DISABLE;
                mObserver->notify();
        }
    }
    else
    {
        LogFunc(("VbglR3SeamlessWaitEvent returned %Rrc (VBoxService)\n", rc));
    }
    return rc;
}

/**
 * Update the set of visible rectangles in the host.
 */
void VBoxGuestSeamlessHost::updateRects(std::auto_ptr<std::vector<RTRECT> > pRects)
{
    if (0 == pRects.get())  /* Assertion */
    {
        LogRelThisFunc(("ERROR: called with null pointer!\n"));
        return;
    }
    VbglR3SeamlessSendRects(pRects.get()->size(), pRects.get()->empty() ? NULL : &pRects.get()->front());
}

/**
 * The actual thread function.
 *
 * @returns iprt status code as thread return value
 * @param pParent the VBoxGuestThread running this thread function
 */
int VBoxGuestSeamlessHostThread::threadFunction(VBoxGuestThread *pThread)
{
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
    return VINF_SUCCESS;
}

/**
 * Send a signal to the thread function that it should exit
 */
void VBoxGuestSeamlessHostThread::stop(void)
{
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
}
