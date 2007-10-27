/** @file
 * innotek Portable Runtime - RTLock Classes for Scope-based Locking.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___iprt_lock_h
#define ___iprt_lock_h

#include <iprt/critsect.h>

__BEGIN_DECLS

/** @defgroup grp_rt_lock       RTLock - Scope-based Locking (C++).
 * @ingroup grp_rt
 * @{
 */

class RTLock;

/**
 * The mutex lock.
 *
 * This is used as a object data member if the intention is to lock
 * a single object. This can also be used statically, initialized in
 * a global variable, for class wide purposes.
 *
 * This is best used together with RTLock.
 */
class RTLockMtx
{
    friend class RTLock;

    private:
        RTCRITSECT      mMtx;

    public:
        RTLockMtx()
        {
            RTCritSectInit(&mMtx);
        }

        ~RTLockMtx()
        {
            RTCritSectDelete(&mMtx);
        }

    // lock() and unlock() are private so that only
    // friend RTLock can access them
    private:
        inline void lock()
        {
            RTCritSectEnter(&mMtx);
        }

        inline void unlock()
        {
            RTCritSectLeave(&mMtx);
        }
};


/**
 * The stack object for automatic locking and unlocking.
 *
 * This is a helper class for automatic locks, to simplify
 * requesting a RTLockMtx and to not forget releasing it.
 * To request a RTLockMtx, simply create an instance of RTLock
 * on the stack and pass the mutex to it:
 *
 * @code
    extern RTLockMtx gMtx;     // wherever this is
    ...
    if (...)
    {
        RTLock lock(gMtx);
        ... // do stuff
        // when lock goes out of scope, destructor releases the mutex
    }
   @endcode
 *
 * You can also explicitly release the mutex by calling RTLock::release().
 * This might be helpful if the lock doesn't go out of scope early enough
 * for your mutex to be released.
 */
class RTLock
{
    private:
        RTLockMtx  &mMtx;
        bool        mfLocked;

    public:
        RTLock(RTLockMtx &aMtx)
            : mMtx(aMtx)
        {
            mMtx.lock();
            mfLocked = true;
        }

        ~RTLock()
        {
            if (mfLocked)
                mMtx.unlock();
        }

        inline void release()
        {
            if (mfLocked)
            {
                mMtx.unlock();
                mfLocked = false;
            }
        }
};


/** @} */

__END_DECLS

#endif

