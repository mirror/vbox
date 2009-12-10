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

#ifndef ____H_AUTOLOCK
#define ____H_AUTOLOCK

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

#include <iprt/err.h>
#include <iprt/assert.h>

#if defined(DEBUG)
# include <iprt/asm.h> // for ASMReturnAddress
#endif

#include <iprt/semaphore.h>

namespace util
{

////////////////////////////////////////////////////////////////////////////////
//
// LockHandle and friends
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Abstract read/write semaphore handle.
 *
 * This is a base class to implement semaphores that provide read/write locking.
 * Subclasses must implement all pure virtual methods of this class together
 * with pure methods of ReadLockOps and WriteLockOps classes.
 *
 * See the AutoWriteLock class documentation for the detailed description of
 * read and write locks.
 */
class LockHandle
{
public:
    LockHandle() {}
    virtual ~LockHandle() {}

    /**
     * Returns @c true if the current thread holds a write lock on this
     * read/write semaphore. Intended for debugging only.
     */
    virtual bool isWriteLockOnCurrentThread() const = 0;

    /**
     * Returns the current write lock level of this semaphore. The lock level
     * determines the number of nested #lock() calls on the given semaphore
     * handle.
     *
     * Note that this call is valid only when the current thread owns a write
     * lock on the given semaphore handle and will assert otherwise.
     */
    virtual uint32_t writeLockLevel() const = 0;

    virtual void lockWrite() = 0;
    virtual void unlockWrite() = 0;
    virtual void lockRead() = 0;
    virtual void unlockRead() = 0;

private:
    // prohibit copy + assignment
    LockHandle(const LockHandle&);
    LockHandle& operator=(const LockHandle&);
};

/**
 * Full-featured read/write semaphore handle implementation.
 *
 * This is an auxiliary base class for classes that need full-featured
 * read/write locking as described in the AutoWriteLock class documentation.
 * Instances of classes inherited from this class can be passed as arguments to
 * the AutoWriteLock and AutoReadLock constructors.
 */
class RWLockHandle : public LockHandle
{
public:
    RWLockHandle();
    virtual ~RWLockHandle();

    virtual bool isWriteLockOnCurrentThread() const;

    virtual void lockWrite();
    virtual void unlockWrite();
    virtual void lockRead();
    virtual void unlockRead();

    virtual uint32_t writeLockLevel() const;

private:
    RTSEMRW mSemRW;
};

/**
 * Write-only semaphore handle implementation.
 *
 * This is an auxiliary base class for classes that need write-only (exclusive)
 * locking and do not need read (shared) locking. This implementation uses a
 * cheap and fast critical section for both lockWrite() and lockRead() methods
 * which makes a lockRead() call fully equivalent to the lockWrite() call and
 * therefore makes it pointless to use instahces of this class with
 * AutoReadLock instances -- shared locking will not be possible anyway and
 * any call to lock() will block if there are lock owners on other threads.
 *
 * Use with care only when absolutely sure that shared locks are not necessary.
 */
class WriteLockHandle : public LockHandle
{
public:
    WriteLockHandle();
    virtual ~WriteLockHandle();
    virtual bool isWriteLockOnCurrentThread() const;

    virtual void lockWrite();
    virtual void unlockWrite();
    virtual void lockRead();
    virtual void unlockRead();
    virtual uint32_t writeLockLevel() const;

private:
    mutable RTCRITSECT mCritSect;
};

////////////////////////////////////////////////////////////////////////////////
//
// Lockable
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Lockable interface.
 *
 * This is an abstract base for classes that need read/write locking. Unlike
 * RWLockHandle and other classes that makes the read/write semaphore a part of
 * class data, this class allows subclasses to decide which semaphore handle to
 * use.
 */
class Lockable
{
public:

    /**
     * Returns a pointer to a LockHandle used by AutoWriteLock/AutoReadLock
     * for locking. Subclasses are allowed to return @c NULL -- in this case,
     * the AutoWriteLock/AutoReadLock object constructed using an instance of
     * such subclass will simply turn into no-op.
     */
    virtual LockHandle *lockHandle() const = 0;

    /**
     * Equivalent to <tt>#lockHandle()->isWriteLockOnCurrentThread()</tt>.
     * Returns @c false if lockHandle() returns @c NULL.
     */
    bool isWriteLockOnCurrentThread()
    {
        LockHandle *h = lockHandle();
        return h ? h->isWriteLockOnCurrentThread() : false;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoLockBase
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Abstract base class for all autolocks.
 *
 * This cannot be used directly. Use AutoReadLock or AutoWriteLock or AutoMultiWriteLock2/3
 * which directly and indirectly derive from this.
 *
 * In the implementation, the instance data contains a list of lock handles.
 * The class provides some utility functions to help locking and unlocking
 * them.
 */

class AutoLockBase
{
protected:
    AutoLockBase(uint32_t cHandles);
    AutoLockBase(uint32_t cHandles, LockHandle *pHandle);
    virtual ~AutoLockBase();

    struct Data;
    Data *m;

    virtual void callLockImpl(LockHandle &l) = 0;
    virtual void callUnlockImpl(LockHandle &l) = 0;

    void callLockOnAllHandles();
    void callUnlockOnAllHandles();

    void cleanup();

public:
    void acquire();
    void release();

private:
    // prohibit copy + assignment
    AutoLockBase(const AutoLockBase&);
    AutoLockBase& operator=(const AutoLockBase&);
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoReadLock
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Automatic read lock. Use this with a RWLockHandle to request a read/write
 * semaphore in read mode. You can also use this with a WriteLockHandle but
 * that makes little sense since they know no read mode.
 *
 * If constructed with a RWLockHandle or an instance of Lockable (which in
 * practice means any VirtualBoxBase derivative), it autoamtically requests
 * the lock in read mode and releases the read lock in the destructor.
 */
class AutoReadLock : public AutoLockBase
{
public:

    /**
     * Constructs a null instance that does not manage any read/write
     * semaphore.
     *
     * Note that all method calls on a null instance are no-ops. This allows to
     * have the code where lock protection can be selected (or omitted) at
     * runtime.
     */
    AutoReadLock()
        : AutoLockBase(1, NULL)
    { }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(LockHandle *aHandle)
        : AutoLockBase(1, aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(LockHandle &aHandle)
        : AutoLockBase(1, &aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(const Lockable &aLockable)
        : AutoLockBase(1, aLockable.lockHandle())
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(const Lockable *aLockable)
        : AutoLockBase(1, aLockable ? aLockable->lockHandle() : NULL)
    {
        acquire();
    }

    virtual ~AutoReadLock();

    virtual void callLockImpl(LockHandle &l);
    virtual void callUnlockImpl(LockHandle &l);
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLockBase
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Base class for all auto write locks.
 *
 * This cannot be used directly. Use AutoWriteLock or AutoMultiWriteLock2/3
 * which directly and indirectly derive from this.
 *
 * In addition to utility methods for subclasses, this implements the public
 * leave/enter/maybeLeave/maybeEnter methods, which are common to all
 * write locks.
 */
class AutoWriteLockBase : public AutoLockBase
{
protected:
    AutoWriteLockBase(uint32_t cHandles)
        : AutoLockBase(cHandles)
    { }

    AutoWriteLockBase(uint32_t cHandles, LockHandle *pHandle)
        : AutoLockBase(cHandles, pHandle)
    { }

    virtual ~AutoWriteLockBase()
    { }

    virtual void callLockImpl(LockHandle &l);
    virtual void callUnlockImpl(LockHandle &l);

public:
    void leave();
    void enter();
    void maybeLeave();
    void maybeEnter();
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLock
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Automatic write lock. Use this with a RWLockHandle to request a read/write
 * semaphore in write mode. There can only ever be one writer of a read/write
 * semaphore: while the lock is held in write mode, no other writer or reader
 * can request the semaphore and will block.
 *
 * If constructed with a RWLockHandle or an instance of Lockable (which in
 * practice means any VirtualBoxBase derivative), it autoamtically requests
 * the lock in write mode and releases the write lock in the destructor.
 *
 * When used with a WriteLockHandle, it requests the semaphore contained therein
 * exclusively.
 */
class AutoWriteLock : public AutoWriteLockBase
{
public:

    /**
     * Constructs a null instance that does not manage any read/write
     * semaphore.
     *
     * Note that all method calls on a null instance are no-ops. This allows to
     * have the code where lock protection can be selected (or omitted) at
     * runtime.
     */
    AutoWriteLock()
        : AutoWriteLockBase(1, NULL)
    { }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(LockHandle *aHandle)
        : AutoWriteLockBase(1, aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(LockHandle &aHandle)
        : AutoWriteLockBase(1, &aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(const Lockable &aLockable)
        : AutoWriteLockBase(1, aLockable.lockHandle())
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(const Lockable *aLockable)
        : AutoWriteLockBase(1, aLockable ? aLockable->lockHandle() : NULL)
    {
        acquire();
    }

    /**
     * Release all write locks acquired by this instance through the #lock()
     * call and destroys the instance.
     *
     * Note that if there there are nested #lock() calls without the
     * corresponding number of #unlock() calls when the destructor is called, it
     * will assert. This is because having an unbalanced number of nested locks
     * is a program logic error which must be fixed.
     */
    virtual ~AutoWriteLock()
    {
        cleanup();
    }

    void attach(LockHandle *aHandle);

    /** @see attach (LockHandle *) */
    void attach(LockHandle &aHandle)
    {
        attach(&aHandle);
    }

    /** @see attach (LockHandle *) */
    void attach(const Lockable &aLockable)
    {
        attach(aLockable.lockHandle());
    }

    /** @see attach (LockHandle *) */
    void attach(const Lockable *aLockable)
    {
        attach(aLockable ? aLockable->lockHandle() : NULL);
    }

    void attachRaw(LockHandle *ph);

    bool isWriteLockOnCurrentThread() const;
    uint32_t writeLockLevel() const;
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoMultiWriteLock*
//
////////////////////////////////////////////////////////////////////////////////

/**
 * A multi-write-lock containing two other write locks.
 *
 */
class AutoMultiWriteLock2 : public AutoWriteLockBase
{
public:
    AutoMultiWriteLock2(Lockable *pl1, Lockable *pl2);
    AutoMultiWriteLock2(LockHandle *pl1, LockHandle *pl2);

    virtual ~AutoMultiWriteLock2()
    {
        cleanup();
    }
};

/**
 * A multi-write-lock containing three other write locks.
 *
 */
class AutoMultiWriteLock3 : public AutoWriteLockBase
{
public:
    AutoMultiWriteLock3(Lockable *pl1, Lockable *pl2, Lockable *pl3);
    AutoMultiWriteLock3(LockHandle *pl1, LockHandle *pl2, LockHandle *pl3);

    virtual ~AutoMultiWriteLock3()
    {
        cleanup();
    }
};

} /* namespace util */

#endif // ____H_AUTOLOCK

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
