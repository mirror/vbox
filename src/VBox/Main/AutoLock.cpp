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

#include <vector>

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

typedef std::vector<LockHandle*> HandlesVector;
typedef std::vector<uint32_t> CountsVector;

struct AutoLockBase::Data
{
    Data(size_t cHandles)
        : fIsLocked(false),
          aHandles(cHandles),       // size of array
          acUnlockedInLeave(cHandles)
    {
        for (uint32_t i = 0; i < cHandles; ++i)
        {
            acUnlockedInLeave[i] = 0;
            aHandles[i] = NULL;
        }
    }

    bool            fIsLocked;          // if true, then all items in aHandles are locked by this AutoLock and
                                        // need to be unlocked in the destructor
    HandlesVector   aHandles;           // array (vector) of LockHandle instances; in the case of AutoWriteLock
                                        // and AutoReadLock, there will only be one item on the list; with the
                                        // AutoMulti* derivatives, there will be multiple
    CountsVector    acUnlockedInLeave;  // for each lock handle, how many times the handle was unlocked in leave(); otherwise 0
};

AutoLockBase::AutoLockBase(uint32_t cHandles)
{
    m = new Data(cHandles);
}

AutoLockBase::AutoLockBase(uint32_t cHandles, LockHandle *pHandle)
{
    Assert(cHandles == 1);
    m = new Data(1);
    m->aHandles[0] = pHandle;
}

AutoLockBase::~AutoLockBase()
{
    delete m;
}

/**
 * Requests ownership of all contained lock handles by calling
 * the pure virtual callLockImpl() function on each of them,
 * which must be implemented by the descendant class; in the
 * implementation, AutoWriteLock will request a write lock
 * whereas AutoReadLock will request a read lock.
 *
 * Does *not* modify the lock counts in the member variables.
 */
void AutoLockBase::callLockOnAllHandles()
{
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
            // call virtual function implemented in AutoWriteLock or AutoReadLock
            this->callLockImpl(*pHandle);
    }
}

/**
 * Releases ownership of all contained lock handles by calling
 * the pure virtual callUnlockImpl() function on each of them,
 * which must be implemented by the descendant class; in the
 * implementation, AutoWriteLock will release a write lock
 * whereas AutoReadLock will release a read lock.
 *
 * Does *not* modify the lock counts in the member variables.
 */
void AutoLockBase::callUnlockOnAllHandles()
{
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
            // call virtual function implemented in AutoWriteLock or AutoReadLock
            this->callUnlockImpl(*pHandle);
    }
}

/**
 * Destructor implementation that can also be called explicitly, if required.
 * Restores the exact state before the AutoLock was created; that is, unlocks
 * all contained semaphores and might actually lock them again if leave()
 * was called during the AutoLock's lifetime.
 */
void AutoLockBase::cleanup()
{
    bool fAnyUnlockedInLeave = false;

    uint32_t i = 0;
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
        {
            if (m->acUnlockedInLeave[i])
            {
                // there was a leave() before the destruction: then restore the
                // lock level that might have been set by locks other than our own
                if (m->fIsLocked)
                {
                    --m->acUnlockedInLeave[i];
                    fAnyUnlockedInLeave = true;
                }
                for (; m->acUnlockedInLeave[i]; --m->acUnlockedInLeave[i])
                    callLockImpl(*pHandle);
            }
        }
        ++i;
    }

    if (m->fIsLocked && !fAnyUnlockedInLeave)
        callUnlockOnAllHandles();
}

/**
 * Requests ownership of all contained semaphores. Public method that can
 * only be called once and that also gets called by the AutoLock constructors.
 */
void AutoLockBase::acquire()
{
    AssertMsg(!m->fIsLocked, ("m->fIsLocked is true, attempting to lock twice!"));
    callLockOnAllHandles();
    m->fIsLocked = true;
}

/**
 * Releases ownership of all contained semaphores. Public method.
 */
void AutoLockBase::release()
{
    AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot release!"));
    callUnlockOnAllHandles();
    m->fIsLocked = false;
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
    LockHandle *pHandle = m->aHandles[0];

    if (pHandle)
    {
        if (m->fIsLocked)
            pHandle->unlockRead();
    }
}

/**
 * Implementation of the pure virtual declared in AutoLockBase.
 * This gets called by AutoLockBase.acquire() to actually request
 * the semaphore; in the AutoReadLock implementation, we request
 * the semaphore in read mode.
 */
/*virtual*/ void AutoReadLock::callLockImpl(LockHandle &l)
{
    l.lockRead();
}

/**
 * Implementation of the pure virtual declared in AutoLockBase.
 * This gets called by AutoLockBase.release() to actually release
 * the semaphore; in the AutoReadLock implementation, we release
 * the semaphore in read mode.
 */
/*virtual*/ void AutoReadLock::callUnlockImpl(LockHandle &l)
{
    l.unlockRead();
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLockBase
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation of the pure virtual declared in AutoLockBase.
 * This gets called by AutoLockBase.acquire() to actually request
 * the semaphore; in the AutoWriteLock implementation, we request
 * the semaphore in write mode.
 */
/*virtual*/ void AutoWriteLockBase::callLockImpl(LockHandle &l)
{
    l.lockWrite();
}

/**
 * Implementation of the pure virtual declared in AutoLockBase.
 * This gets called by AutoLockBase.release() to actually release
 * the semaphore; in the AutoWriteLock implementation, we release
 * the semaphore in write mode.
 */
/*virtual*/ void AutoWriteLockBase::callUnlockImpl(LockHandle &l)
{
    l.unlockWrite();
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
void AutoWriteLockBase::leave()
{
    AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot leave()!"));

    uint32_t i = 0;
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
        {
            AssertMsg(m->acUnlockedInLeave[i] == 0, ("m->cUnlockedInLeave[%d] is %d, must be 0! Called leave() twice?", i, m->acUnlockedInLeave[i]));
            m->acUnlockedInLeave[i] = pHandle->writeLockLevel();
            AssertMsg(m->acUnlockedInLeave[i] >= 1, ("m->cUnlockedInLeave[%d] is %d, must be >=1!", i, m->acUnlockedInLeave[i]));

            for (uint32_t left = m->acUnlockedInLeave[i];
                 left;
                 --left)
                pHandle->unlockWrite();
        }
        ++i;
    }
}

/**
 * Causes the current thread to restore the write lock level after the
 * #leave() call. This call will indefinitely block if another thread has
 * successfully acquired a write or a read lock on the same semaphore in
 * between.
 */
void AutoWriteLockBase::enter()
{
    AssertMsg(m->fIsLocked, ("m->fIsLocked is false, cannot enter()!"));

    uint32_t i = 0;
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
        {
            AssertMsg(m->acUnlockedInLeave[i] != 0, ("m->cUnlockedInLeave[%d] is 0! enter() without leave()?", i));

            for (; m->acUnlockedInLeave[i]; --m->acUnlockedInLeave[i])
                pHandle->lockWrite();
        }
        ++i;
    }
}

/**
 * Same as #leave() but checks if the current thread actally owns the lock
 * and only proceeds in this case. As a result, as opposed to #leave(),
 * doesn't assert when called with no lock being held.
 */
void AutoWriteLockBase::maybeLeave()
{
    uint32_t i = 0;
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
        {
            if (pHandle->isWriteLockOnCurrentThread())
            {
                m->acUnlockedInLeave[i] = pHandle->writeLockLevel();
                AssertMsg(m->acUnlockedInLeave[i] >= 1, ("m->cUnlockedInLeave[%d] is %d, must be >=1!", i, m->acUnlockedInLeave[i]));

                for (uint32_t left = m->acUnlockedInLeave[i];
                     left;
                     --left)
                    pHandle->unlockWrite();
            }
        }
        ++i;
    }
}

/**
 * Same as #enter() but checks if the current thread actally owns the lock
 * and only proceeds if not. As a result, as opposed to #enter(), doesn't
 * assert when called with the lock already being held.
 */
void AutoWriteLockBase::maybeEnter()
{
    uint32_t i = 0;
    for (HandlesVector::iterator it = m->aHandles.begin();
         it != m->aHandles.end();
         ++it)
    {
        LockHandle *pHandle = *it;
        if (pHandle)
        {
            if (!pHandle->isWriteLockOnCurrentThread())
            {
                for (; m->acUnlockedInLeave[i]; --m->acUnlockedInLeave[i])
                    pHandle->lockWrite();
            }
        }
        ++i;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLock
//
////////////////////////////////////////////////////////////////////////////////

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
    LockHandle *pHandle = m->aHandles[0];

    /* detect simple self-reattachment */
    if (pHandle != aHandle)
    {
        bool fWasLocked = m->fIsLocked;

        cleanup();

        m->aHandles[0] = aHandle;
        m->fIsLocked = fWasLocked;

        if (aHandle)
            if (fWasLocked)
                aHandle->lockWrite();
    }
}

void AutoWriteLock::attachRaw(LockHandle *ph)
{
    m->aHandles[0] = ph;
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
    return m->aHandles[0] ? m->aHandles[0]->isWriteLockOnCurrentThread() : false;
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
    return m->aHandles[0] ? m->aHandles[0]->writeLockLevel() : 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// AutoMultiWriteLock*
//
////////////////////////////////////////////////////////////////////////////////

AutoMultiWriteLock2::AutoMultiWriteLock2(Lockable *pl1, Lockable *pl2)
    : AutoWriteLockBase(2)
{
    if (pl1)
        m->aHandles[0] = pl1->lockHandle();
    if (pl2)
        m->aHandles[1] = pl2->lockHandle();
    acquire();
}

AutoMultiWriteLock2::AutoMultiWriteLock2(LockHandle *pl1, LockHandle *pl2)
    : AutoWriteLockBase(2)
{
    m->aHandles[0] = pl1;
    m->aHandles[1] = pl2;
    acquire();
}

AutoMultiWriteLock3::AutoMultiWriteLock3(Lockable *pl1, Lockable *pl2, Lockable *pl3)
    : AutoWriteLockBase(3)
{
    if (pl1)
        m->aHandles[0] = pl1->lockHandle();
    if (pl2)
        m->aHandles[1] = pl2->lockHandle();
    if (pl3)
        m->aHandles[2] = pl3->lockHandle();
    acquire();
}

AutoMultiWriteLock3::AutoMultiWriteLock3(LockHandle *pl1, LockHandle *pl2, LockHandle *pl3)
    : AutoWriteLockBase(3)
{
    m->aHandles[0] = pl1;
    m->aHandles[1] = pl2;
    m->aHandles[2] = pl3;
    acquire();
}

} /* namespace util */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
