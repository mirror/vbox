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

#ifdef VBOX_MAIN_AUTOLOCK_TRAP
// workaround for compile problems on gcc 4.1
# ifdef __GNUC__
#  pragma GCC visibility push(default)
# endif
#endif

#include "AutoLock.h"

#include "Logging.h"

#include <iprt/string.h>

#ifdef VBOX_MAIN_AUTOLOCK_TRAP
# if defined (RT_OS_LINUX)
#  include <signal.h>
#  include <execinfo.h>
/* get REG_EIP from ucontext.h */
#  ifndef __USE_GNU
#  define __USE_GNU
#  endif
#  include <ucontext.h>
#  ifdef RT_ARCH_AMD64
#   define REG_PC REG_RIP
#  else
#   define REG_PC REG_EIP
#  endif
# endif
#endif /* VBOX_MAIN_AUTOLOCK_TRAP */

namespace util
{

#ifdef VBOX_MAIN_AUTOLOCK_TRAP

namespace internal
{

struct TLS
{
    struct Uint32_t
    {
        Uint32_t() : raw (0) {}
        operator uint32_t &() { return raw; }
        uint32_t raw;
    };

    typedef std::map <RWLockHandle *, Uint32_t> HandleMap;
    HandleMap handles; /*< handle reference counter on the current thread */
};

/**
 * Global module initialization structure.
 *
 * The constructor and destructor of this structure are used to perform global
 * module initiaizaton and cleanup. Thee must be only one global variable of
 * this structure.
 */
static
class Global
{
public:

    Global() : tlsID (NIL_RTTLS)
    {
#if defined (RT_OS_LINUX)
        int vrc = RTTlsAllocEx (&tlsID, TLSDestructor);
        AssertRC (vrc);
#else
        tlsID = RTTlsAlloc();
        Assert (tlsID != NIL_RTTLS);
#endif
    }

    ~Global()
    {
        RTTlsFree (tlsID);
    }

    TLS *tls() const
    {
        TLS *tls = NULL;
        if (tlsID != NIL_RTTLS)
        {
            tls = static_cast <TLS *> (RTTlsGet (tlsID));
            if (tls == NULL)
            {
                tls = new TLS();
                RTTlsSet (tlsID, tls);
            }
        }

        return tls;
    }

    RTTLS tlsID;
}
gGlobal;

DECLCALLBACK(void) TLSDestructor (void *aValue)
{
    if (aValue != NULL)
    {
        TLS *tls = static_cast <TLS *> (aValue);
        RWLockHandle::TLSDestructor (tls);
        delete tls;
    }
}

} /* namespace internal */

#endif /* VBOX_MAIN_AUTOLOCK_TRAP */


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
# ifdef VBOX_MAIN_AUTOLOCK_TRAP
        if (mReadLockCount != 0)
        {
            using namespace internal;
            TLS *tls = gGlobal.tls();
            if (tls != NULL)
            {
                TLS::HandleMap::const_iterator it = tls->handles.find (this);
                if (it != tls->handles.end() && it->second.raw != 0)
                {
                    /* if there is a writer then the handle reference counter equals
                     * to the number of readers on the current thread plus 1 */

                    uint32_t readers = it->second.raw;
                    if (mWriteLockThread != NIL_RTNATIVETHREAD)
                        -- readers;

                    std::string info;
                    gatherInfo (info);

                    AssertReleaseMsgFailedReturnVoid ((
                        "DETECTED SELF DEADLOCK on Thread %08x: lockWrite() after "
                        "lockRead(): reader count = %d!\n%s\n",
                        threadSelf, readers, info.c_str()));
                }
            }
        }
# endif /* VBOX_MAIN_AUTOLOCK_TRAP */

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

# ifdef VBOX_MAIN_AUTOLOCK_TRAP
    logOp (LockWrite);
# endif

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

# ifdef VBOX_MAIN_AUTOLOCK_TRAP
    logOp (UnlockWrite);
# endif

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

# ifdef VBOX_MAIN_AUTOLOCK_TRAP
    logOp (LockRead);
# endif /* VBOX_MAIN_AUTOLOCK_TRAP */

    bool isWriteLock = mWriteLockLevel != 0;
    bool isFirstReadLock = mReadLockCount == 0;

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

# ifdef VBOX_MAIN_AUTOLOCK_TRAP
                logOp (UnlockRead);
# endif /* VBOX_MAIN_AUTOLOCK_TRAP */
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

# ifdef VBOX_MAIN_AUTOLOCK_TRAP
            logOp (UnlockRead);
# endif /* VBOX_MAIN_AUTOLOCK_TRAP */
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


#ifdef VBOX_MAIN_AUTOLOCK_TRAP

void RWLockHandle::logOp (Operation aOp)
{
    std::string info;

    char buf [256];
    RTStrPrintf (buf, sizeof (buf), "[%c] Thread %08x (%s)\n",
                 aOp == LockRead ? 'r' : aOp == LockWrite ? 'w' : '?',
                 RTThreadNativeSelf(), RTThreadGetName (RTThreadSelf()));
    info += buf;

# if defined (RT_OS_LINUX)

    void *trace [16];
    char **messages = (char **) NULL;
    int i, trace_size = 0;
    trace_size = backtrace (trace, 16);

    messages = backtrace_symbols (trace, trace_size);
    /* skip first stack frame (points here) and the second stack frame (points
     * to lockRead()/lockWrite() */
    for (i = 2; i < trace_size; ++i)
        (info += messages[i]) += "\n";

    free (messages);

# endif /* defined (RT_OS_LINUX) */

    internal::TLS *tls = internal::gGlobal.tls();
    if (tls != NULL)
    {

        switch (aOp)
        {
            case LockRead:
            {
                mReaderInfo.push_back (info);
                ++ tls->handles [this];
                break;
            }
            case UnlockRead:
            {
                mReaderInfo.pop_back();
                -- tls->handles [this];
                break;
            }
            case LockWrite:
            {
                mWriterInfo = info;
                ++ tls->handles [this];
                break;
            }
            case UnlockWrite:
            {
                mWriterInfo.clear();;
                -- tls->handles [this];
                break;
            }
        }
    }
}

void RWLockHandle::gatherInfo (std::string &aInfo)
{
    char buf [256];
    RTStrPrintf (buf, sizeof (buf),
                 "[*] RWLockHandle %x:\n", this,
                 RTThreadNativeSelf(), RTThreadGetName (RTThreadSelf()));
    aInfo += buf;

    /* add reader info */
    for (ReaderInfo::const_iterator it = mReaderInfo.begin();
         it != mReaderInfo.end(); ++ it)
    {
        aInfo += *it;
    }
    /* add writer info */
    if (!mWriterInfo.empty())
        aInfo += mWriterInfo;
}

/* static */
void RWLockHandle::TLSDestructor (internal::TLS *aTLS)
{
    using namespace internal;

    if (aTLS != NULL && aTLS->handles.size())
    {
        std::string info;
        size_t cnt = 0;

        for (TLS::HandleMap::const_iterator it = aTLS->handles.begin();
             it != aTLS->handles.end(); ++ it)
        {
            if (it->second.raw != 0)
            {
                it->first->gatherInfo (info);
                ++ cnt;
            }
        }

        if (cnt != 0)
        {
            AssertReleaseMsgFailed ((
                "DETECTED %d HELD RWLockHandle's on Thread %08x!\n%s\n",
                cnt, RTThreadNativeSelf(), info.c_str()));
        }
    }
}

#endif /* ifdef VBOX_MAIN_AUTOLOCK_TRAP */


} /* namespace util */

