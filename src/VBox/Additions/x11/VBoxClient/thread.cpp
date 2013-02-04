/** @file
 *
 * VirtualBox additions client application: thread class.
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

#include <VBox/log.h>

#include "thread.h"

/** Stop the thread using its stop method and get the exit value. */
int VBoxGuestThread::stop(RTMSINTERVAL cMillies, int *prc)
{
    int rc = VINF_SUCCESS;

    LogRelFlowFunc(("\n"));
    if (NIL_RTTHREAD == mSelf)  /* Assertion */
    {
        LogRelThisFunc(("Attempted to stop thread %s which is not running!\n", mName));
        return VERR_INTERNAL_ERROR;
    }
    mExit = true;
    mFunction->stop();
    if (0 != (mFlags & RTTHREADFLAGS_WAITABLE))
    {
        rc = RTThreadWait(mSelf, cMillies, prc);
        if (RT_SUCCESS(rc))
        {
            mSelf = NIL_RTTHREAD;
        }
        else
        {
            LogRelThisFunc(("Failed to stop thread %s!\n", mName));
        }
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/** Destroy the class, stopping the thread if necessary. */
VBoxGuestThread::~VBoxGuestThread(void)
{
    LogRelFlowFunc(("\n"));
    if (NIL_RTTHREAD != mSelf)
    {
        LogRelThisFunc(("Warning!  Stopping thread %s, as it is still running!\n", mName));
        stop(2000, 0);
    }
    LogRelFlowFunc(("returning\n"));
}

/** Start the thread. */
int VBoxGuestThread::start(void)
{
    int rc = VINF_SUCCESS;

    LogRelFlowFunc(("returning\n"));
    if (NIL_RTTHREAD != mSelf)  /* Assertion */
    {
        LogRelThisFunc(("Attempted to start thread %s twice!\n", mName));
        return VERR_INTERNAL_ERROR;
    }
    mExit = false;
    rc = RTThreadCreate(&mSelf, threadFunction, reinterpret_cast<void *>(this),
                          mStack, mType, mFlags, mName);
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/** Yield the CPU */
bool VBoxGuestThread::yield(void)
{
    return RTThreadYield();
}

/** The "real" thread function for the VBox runtime. */
int VBoxGuestThread::threadFunction(RTTHREAD self, void *pvUser)
{
    int rc = VINF_SUCCESS;

    LogRelFlowFunc(("\n"));
    PSELF pSelf = reinterpret_cast<PSELF>(pvUser);
    pSelf->mRunning = true;
    rc = pSelf->mFunction->threadFunction(pSelf);
    pSelf->mRunning = false;
    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}
