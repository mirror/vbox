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

#include <iprt/string.h>


namespace util
{

////////////////////////////////////////////////////////////////////////////////
//
// RWLockHandle
//
////////////////////////////////////////////////////////////////////////////////

RWLockHandle::RWLockHandle()
{
    int vrc = RTSemRWCreate (&mSemRW);
    AssertRC (vrc);
}


RWLockHandle::~RWLockHandle()
{
    RTSemRWDestroy (mSemRW);
}


bool RWLockHandle::isWriteLockOnCurrentThread() const
{
    return RTSemRWIsWriteOwner (mSemRW);
}


void RWLockHandle::lockWrite()
{
    int vrc = RTSemRWRequestWrite (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);
}


void RWLockHandle::unlockWrite()
{
    int vrc = RTSemRWReleaseWrite (mSemRW);
    AssertRC (vrc);
}


void RWLockHandle::lockRead()
{
    int vrc = RTSemRWRequestRead (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);
}


void RWLockHandle::unlockRead()
{
    int vrc = RTSemRWReleaseRead (mSemRW);
    AssertRC (vrc);
}


uint32_t RWLockHandle::writeLockLevel() const
{
    return RTSemRWGetWriteRecursion (mSemRW);
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoLockBase
//
////////////////////////////////////////////////////////////////////////////////

struct AutoLockBase::Data
{
    Data(LockHandle *argpHandle)
        : pHandle(argpHandle),
          fIsLocked(false),
          cUnlockedInLeave(0)
    { }

    LockHandle      *pHandle;
    bool            fIsLocked;
    uint32_t        cUnlockedInLeave;   // how many times the handle was unlocked in leave(); otherwise 0
};

AutoLockBase::AutoLockBase(LockHandle *pHandle)
{
    m = new Data(pHandle);
}

AutoLockBase::~AutoLockBase()
{
    delete m;
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLock
//
////////////////////////////////////////////////////////////////////////////////

void AutoWriteLock::cleanup()
{
    if (m->pHandle)
    {
        if (m->cUnlockedInLeave)
        {
            // there was a leave() before the destruction: then restore the
            // lock level that might have been set by locks other than our own
            if (m->fIsLocked)
                --m->cUnlockedInLeave;       // no lock for our own
            m->fIsLocked = false;
            for (; m->cUnlockedInLeave; --m->cUnlockedInLeave)
                m->pHandle->lockWrite();

                // @todo r=dj is this really desirable behavior? maybe leave/enter should go altogether?
        }

        if (m->fIsLocked)
            m->pHandle->unlockWrite();
    }
}

void AutoWriteLock::acquire()
{
    if (m->pHandle)
    {
        AssertMsg(!m->fIsLocked, ("m->fIsLocked is true, attempting to lock twice!"));
        m->pHandle->lockWrite();
        m->fIsLocked = true;
    }
}

void AutoWriteLock::release()
{
    if (m->pHandle)
    {
        AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot release!"));
        m->pHandle->unlockWrite();
        m->fIsLocked = false;
    }
}

/**
 * Causes the current thread to completely release the write lock to make
 * the managed semaphore immediately available for locking by other threads.
 *
 * This implies that all nested write locks on the semaphore will be
 * released, even those that were acquired through the calls to #lock()
 * methods of all other AutoWriteLock/AutoReadLock instances managing the
 * <b>same</b> read/write semaphore.
 *
 * After calling this method, the only method you are allowed to call is
 * #enter(). It will acquire the write lock again and restore the same
 * level of nesting as it had before calling #leave().
 *
 * If this instance is destroyed without calling #enter(), the destructor
 * will try to restore the write lock level that existed when #leave() was
 * called minus the number of nested #lock() calls made on this instance
 * itself. This is done to preserve lock levels of other
 * AutoWriteLock/AutoReadLock instances managing the same semaphore (if
 * any). Tiis also means that the destructor may indefinitely block if a
 * write or a read lock is owned by some other thread by that time.
 */
void AutoWriteLock::leave()
{
    if (m->pHandle)
    {
        AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot leave()!"));
        AssertMsg(m->cUnlockedInLeave == 0, ("m->cUnlockedInLeave is %d, must be 0! Called leave() twice?", m->cUnlockedInLeave));

        m->cUnlockedInLeave = m->pHandle->writeLockLevel();
        AssertMsg(m->cUnlockedInLeave >= 1, ("m->cUnlockedInLeave is %d, must be >=1!", m->cUnlockedInLeave));

        for (uint32_t left = m->cUnlockedInLeave;
                left;
                --left)
            m->pHandle->unlockWrite();
    }
}

/**
 * Causes the current thread to restore the write lock level after the
 * #leave() call. This call will indefinitely block if another thread has
 * successfully acquired a write or a read lock on the same semaphore in
 * between.
 */
void AutoWriteLock::enter()
{
    if (m->pHandle)
    {
        AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot enter()!"));
        AssertMsg(m->cUnlockedInLeave != 0, ("m->cUnlockedInLeave is 0! enter() without leave()?"));

        for (; m->cUnlockedInLeave; --m->cUnlockedInLeave)
            m->pHandle->lockWrite();
    }
}

/**
 * Attaches another handle to this auto lock instance.
 *
 * The previous object's lock is completely released before the new one is
 * acquired. The lock level of the new handle will be the same. This
 * also means that if the lock was not acquired at all before #attach(), it
 * will not be acquired on the new handle too.
 *
 * @param aHandle   New handle to attach.
 */
void AutoWriteLock::attach(LockHandle *aHandle)
{
    /* detect simple self-reattachment */
    if (m->pHandle != aHandle)
    {
        bool fWasLocked = m->fIsLocked;

        cleanup();

        m->pHandle = aHandle;
        m->fIsLocked = fWasLocked;

        if (m->pHandle)
            if (fWasLocked)
                m->pHandle->lockWrite();
    }
}

void AutoWriteLock::attachRaw(LockHandle *ph)
{
    m->pHandle = ph;
}

/**
 * Returns @c true if the current thread holds a write lock on the managed
 * read/write semaphore. Returns @c false if the managed semaphore is @c
 * NULL.
 *
 * @note Intended for debugging only.
 */
bool AutoWriteLock::isWriteLockOnCurrentThread() const
{
    return m->pHandle ? m->pHandle->isWriteLockOnCurrentThread() : false;
}

 /**
 * Returns the current write lock level of the managed smaphore. The lock
 * level determines the number of nested #lock() calls on the given
 * semaphore handle. Returns @c 0 if the managed semaphore is @c
 * NULL.
 *
 * Note that this call is valid only when the current thread owns a write
 * lock on the given semaphore handle and will assert otherwise.
 *
 * @note Intended for debugging only.
 */
uint32_t AutoWriteLock::writeLockLevel() const
{
    return m->pHandle ? m->pHandle->writeLockLevel() : 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoReadLock
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Release all read locks acquired by this instance through the #lock()
 * call and destroys the instance.
 *
 * Note that if there there are nested #lock() calls without the
 * corresponding number of #unlock() calls when the destructor is called, it
 * will assert. This is because having an unbalanced number of nested locks
 * is a program logic error which must be fixed.
 */
/*virtual*/ AutoReadLock::~AutoReadLock()
{
    if (m->pHandle)
    {
        if (m->fIsLocked)
            m->pHandle->unlockRead();
    }
}

/**
 * Requests a read (shared) lock. If a read lock is already owned by
 * this thread, increases the lock level (allowing for nested read locks on
 * the same thread). Blocks indefinitely if a write lock is already owned by
 * another thread until that tread releases the write lock, otherwise
 * returns immediately.
 *
 * Note that this method returns immediately even if any number of other
 * threads owns read locks on the same semaphore. Also returns immediately
 * if a write lock on this semaphore is owned by the current thread which
 * allows for read locks nested into write locks on the same thread.
 */
void AutoReadLock::acquire()
{
    if (m->pHandle)
    {
        AssertMsg(!m->fIsLocked, ("m->fIsLocked is true, attempting to lock twice!"));
        m->pHandle->lockRead();
        m->fIsLocked = true;
    }
}

/**
 * Decreases the read lock level increased by #lock(). If the level drops to
 * zero (e.g. the number of nested #unlock() calls matches the number of
 * nested #lock() calls), releases the lock making the managed semaphore
 * available for locking by other threads.
 */
void AutoReadLock::release()
{
    if (m->pHandle)
    {
        AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot release!"));
        m->pHandle->unlockRead();
        m->fIsLocked = false;
    }
}


} /* namespace util */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
