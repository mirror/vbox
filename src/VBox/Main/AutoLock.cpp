/** @file
 *
 * AutoLock: smart critical section wrapper
 */

/*
 * Copyright (C) 2006-2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AutoLock.h"

#include "Logging.h"

namespace util
{

RWLockHandle::RWLockHandle()
{
    RTCritSectInit (&mCritSect);
    RTSemEventCreate (&mGoWriteSem);
    RTSemEventMultiCreate (&mGoReadSem);

    mWriteLockThread = NIL_RTTHREAD;

    mReadLockCount = 0;
    mWriteLockLevel = 0;
    mWriteLockPending = 0;
}

RWLockHandle::~RWLockHandle()
{
    RTSemEventMultiDestroy (mGoReadSem);
    RTSemEventDestroy (mGoWriteSem);
    RTCritSectDelete (&mCritSect);
}

bool RWLockHandle::isLockedOnCurrentThread() const
{
    RTCritSectEnter (&mCritSect);
    bool locked = mWriteLockThread == RTThreadSelf();
    RTCritSectLeave (&mCritSect);
    return locked;
}

void RWLockHandle::lockWrite()
{
    RTCritSectEnter (&mCritSect);

    if (mWriteLockThread != RTThreadSelf())
    {
        if (mReadLockCount != 0 || mWriteLockThread != NIL_RTTHREAD ||
            mWriteLockPending != 0 /* respect other pending writers */)
        {
            /* wait until all read locks or another write lock is released */
            ++ mWriteLockPending;
            Assert (mWriteLockPending != 0 /* pending writer overflow? */);
            RTCritSectLeave (&mCritSect);
            RTSemEventWait (mGoWriteSem, RT_INDEFINITE_WAIT);
            RTCritSectEnter (&mCritSect);
            -- mWriteLockPending;
        }

        Assert (mWriteLockLevel == 0);
        Assert (mWriteLockThread == NIL_RTTHREAD);

        mWriteLockThread = RTThreadSelf();
    }

    ++ mWriteLockLevel;
    Assert (mWriteLockLevel != 0 /* overflow */);

    RTCritSectLeave (&mCritSect);
}

void RWLockHandle::unlockWrite()
{
    RTCritSectEnter (&mCritSect);

    Assert (mWriteLockLevel != 0 /* unlockWrite() w/o preceding lockWrite()? */);
    if (mWriteLockLevel != 0)
    {
        -- mWriteLockLevel;
        if (mWriteLockLevel == 0)
        {
            mWriteLockThread = NIL_RTTHREAD;

            /* no write locks, let writers go if there are any (top priority),
             * otherwise let readers go if there are any */
            if (mWriteLockPending != 0)
                RTSemEventSignal (mGoWriteSem);
            else if (mReadLockCount != 0)
                RTSemEventMultiSignal (mGoReadSem);
        }
    }

    RTCritSectLeave (&mCritSect);
}

void RWLockHandle::lockRead()
{
    RTCritSectEnter (&mCritSect);

    ++ mReadLockCount;
    Assert (mReadLockCount != 0 /* read lock overflow? */);

    bool isWriteLock = mWriteLockLevel != 0;
    bool isFirstReadLock = mReadLockCount == 1;

    if (isWriteLock && mWriteLockThread == RTThreadSelf())
    {
        /* read lock nested into the write lock, cause return immediately */
        isWriteLock = false;
    }
    else
    {
        if (!isWriteLock)
        {
            /* write locks are top priority, so let them go if they are
             * pending */
            if (mWriteLockPending != 0)
            {
                isWriteLock = true;
                /* the first postponed reader kicks pending writers */
                if (isFirstReadLock)
                    RTSemEventSignal (mGoWriteSem);
            }
        }

        /* the first waiting reader resets the semaphore before letting it be
         * posted (i.e. before leaving the critical section) */
        if (isWriteLock && isFirstReadLock)
            RTSemEventMultiReset (mGoReadSem);
    }

    RTCritSectLeave (&mCritSect);

    /* wait until the write lock is released */
    if (isWriteLock)
        RTSemEventMultiWait (mGoReadSem, RT_INDEFINITE_WAIT);
}

void RWLockHandle::unlockRead()
{
    RTCritSectEnter (&mCritSect);

    Assert (mReadLockCount != 0 /* unlockRead() w/o preceding lockRead()? */);
    if (mReadLockCount != 0)
    {
        if (mWriteLockLevel != 0)
        {
            /* read unlock nested into the write lock, just decrease the
             * counter */
            Assert (mWriteLockThread == RTThreadSelf()
                    /* unlockRead() after lockWrite()? */);
            if (mWriteLockThread == RTThreadSelf())
                -- mReadLockCount;
        }
        else
        {
            -- mReadLockCount;
            if (mReadLockCount == 0)
            {
                /* no read locks, let writers go if there are any */
                if (mWriteLockPending != 0)
                    RTSemEventSignal (mGoWriteSem);
            }
        }
    }

    RTCritSectLeave (&mCritSect);
}

uint32_t RWLockHandle::writeLockLevel() const
{
    Assert (mWriteLockLevel != 0);

    return mWriteLockLevel;
}

} /* namespace util */

