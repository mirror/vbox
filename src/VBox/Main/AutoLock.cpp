/** @file
 *
 * AutoWriteLock/AutoReadLock: smart R/W semaphore wrappers
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "AutoLock.h"

#include "Logging.h"

namespace util
{

RWLockHandle::RWLockHandle()
{
#ifdef VBOX_MAIN_USE_SEMRW

    int vrc = RTSemRWCreate (&mSemRW);
    AssertRC (vrc);

#else /* VBOX_MAIN_USE_SEMRW */

    int vrc = RTCritSectInit (&mCritSect);
    AssertRC (vrc);
    vrc = RTSemEventCreate (&mGoWriteSem);
    AssertRC (vrc);
    vrc = RTSemEventMultiCreate (&mGoReadSem);
    AssertRC (vrc);

    mWriteLockThread = NIL_RTNATIVETHREAD;

    mReadLockCount = 0;
    mSelfReadLockCount = 0;

    mWriteLockLevel = 0;
    mWriteLockPending = 0;

#endif /* VBOX_MAIN_USE_SEMRW */
}

RWLockHandle::~RWLockHandle()
{
#ifdef VBOX_MAIN_USE_SEMRW

    RTSemRWDestroy (mSemRW);

#else /* VBOX_MAIN_USE_SEMRW */

    RTSemEventMultiDestroy (mGoReadSem);
    RTSemEventDestroy (mGoWriteSem);
    RTCritSectDelete (&mCritSect);

#endif /* VBOX_MAIN_USE_SEMRW */
}

bool RWLockHandle::isWriteLockOnCurrentThread() const
{
#ifdef VBOX_MAIN_USE_SEMRW

    return RTSemRWIsWriteOwner (mSemRW);

#else /* VBOX_MAIN_USE_SEMRW */

    RTCritSectEnter (&mCritSect);
    bool locked = mWriteLockThread == RTThreadNativeSelf();
    RTCritSectLeave (&mCritSect);
    return locked;

#endif /* VBOX_MAIN_USE_SEMRW */
}

void RWLockHandle::lockWrite()
{
#ifdef VBOX_MAIN_USE_SEMRW

    int vrc = RTSemRWRequestWrite (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);

#else /* VBOX_MAIN_USE_SEMRW */

    RTCritSectEnter (&mCritSect);

    RTNATIVETHREAD threadSelf = RTThreadNativeSelf();

    if (mWriteLockThread != threadSelf)
    {
        if (mReadLockCount != 0 || mWriteLockThread != NIL_RTNATIVETHREAD ||
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
        Assert (mWriteLockThread == NIL_RTNATIVETHREAD);
        Assert (mSelfReadLockCount == 0 /* missing unlockRead()? */);

        mWriteLockThread = threadSelf;
    }

    ++ mWriteLockLevel;
    Assert (mWriteLockLevel != 0 /* overflow */);

    RTCritSectLeave (&mCritSect);

#endif /* VBOX_MAIN_USE_SEMRW */
}

void RWLockHandle::unlockWrite()
{
#ifdef VBOX_MAIN_USE_SEMRW

    int vrc = RTSemRWReleaseWrite (mSemRW);
    AssertRC (vrc);

#else /* VBOX_MAIN_USE_SEMRW */

    RTCritSectEnter (&mCritSect);

    Assert (mWriteLockLevel != 0 /* unlockWrite() w/o preceding lockWrite()? */);
    if (mWriteLockLevel != 0)
    {
        -- mWriteLockLevel;
        if (mWriteLockLevel == 0)
        {
            Assert (mSelfReadLockCount == 0
                    /* mixed unlockWrite()/unlockRead() order? */);

            mWriteLockThread = NIL_RTNATIVETHREAD;

            /* no write locks, let writers go if there are any (top priority),
             * otherwise let readers go if there are any */
            if (mWriteLockPending != 0)
                RTSemEventSignal (mGoWriteSem);
            else if (mReadLockCount != 0)
                RTSemEventMultiSignal (mGoReadSem);
        }
    }

    RTCritSectLeave (&mCritSect);

#endif /* VBOX_MAIN_USE_SEMRW */
}

void RWLockHandle::lockRead()
{
#ifdef VBOX_MAIN_USE_SEMRW

    int vrc = RTSemRWRequestRead (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);

#else /* VBOX_MAIN_USE_SEMRW */

    RTCritSectEnter (&mCritSect);

    RTNATIVETHREAD threadSelf = RTThreadNativeSelf();

    bool isWriteLock = mWriteLockLevel != 0;
    bool isFirstReadLock = mReadLockCount == 1;

    if (isWriteLock && mWriteLockThread == threadSelf)
    {
        /* read lock nested into the write lock */
        ++ mSelfReadLockCount;
        Assert (mSelfReadLockCount != 0 /* self read lock overflow? */);

        /* cause to return immediately */
        isWriteLock = false;
    }
    else
    {
        ++ mReadLockCount;
        Assert (mReadLockCount != 0 /* read lock overflow? */);

        if (!isWriteLock)
        {
            Assert (mSelfReadLockCount == 0 /* missing unlockRead()? */);

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

#endif /* VBOX_MAIN_USE_SEMRW */
}

void RWLockHandle::unlockRead()
{
#ifdef VBOX_MAIN_USE_SEMRW

    int vrc = RTSemRWReleaseRead (mSemRW);
    AssertRC (vrc);

#else /* VBOX_MAIN_USE_SEMRW */

    RTCritSectEnter (&mCritSect);

    RTNATIVETHREAD threadSelf = RTThreadNativeSelf();

    if (mWriteLockLevel != 0)
    {
        /* read unlock nested into the write lock */
        Assert (mWriteLockThread == threadSelf
                /* unlockRead() after lockWrite()? */);
        if (mWriteLockThread == threadSelf)
        {
            Assert (mSelfReadLockCount != 0
                    /* unlockRead() w/o preceding lockRead()? */);
            if (mSelfReadLockCount != 0)
            {
                -- mSelfReadLockCount;
            }
        }
    }
    else
    {
        Assert (mReadLockCount != 0
                /* unlockRead() w/o preceding lockRead()? */);
        if (mReadLockCount != 0)
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

#endif /* VBOX_MAIN_USE_SEMRW */
}

uint32_t RWLockHandle::writeLockLevel() const
{
#ifdef VBOX_MAIN_USE_SEMRW

    return RTSemRWGetWriteRecursion (mSemRW);

#else /* VBOX_MAIN_USE_SEMRW */

    Assert (mWriteLockLevel != 0);

    return mWriteLockLevel;

#endif /* VBOX_MAIN_USE_SEMRW */
}

} /* namespace util */

