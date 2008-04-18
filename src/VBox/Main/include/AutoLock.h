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

#ifdef VBOX_MAIN_USE_SEMRW
# include <iprt/semaphore.h>
#endif

namespace util
{

/**
 * Abstract lock operations. See LockHandle and AutoWriteLock for details.
 */
class LockOps
{
public:

    virtual ~LockOps() {}

    virtual void lock() = 0;
    virtual void unlock() = 0;
};

/**
 * Read lock operations. See LockHandle and AutoWriteLock for details.
 */
class ReadLockOps : public LockOps
{
public:

    /**
     * Requests a read (shared) lock.
     */
    virtual void lockRead() = 0;

    /**
     * Releases a read (shared) lock ackquired by lockRead().
     */
    virtual void unlockRead() = 0;

    // LockOps interface
    void lock() { lockRead(); }
    void unlock() { unlockRead(); }
};

/**
 * Write lock operations. See LockHandle and AutoWriteLock for details.
 */
class WriteLockOps : public LockOps
{
public:

    /**
     * Requests a write (exclusive) lock.
     */
    virtual void lockWrite() = 0;

    /**
     * Releases a write (exclusive) lock ackquired by lockWrite().
     */
    virtual void unlockWrite() = 0;

    // LockOps interface
    void lock() { lockWrite(); }
    void unlock() { unlockWrite(); }
};

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
class LockHandle : protected ReadLockOps, protected WriteLockOps
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

    /**
     * Returns an interface to read lock operations of this semaphore.
     * Used by constructors of AutoMultiLockN classes.
     */
    LockOps *rlock() { return (ReadLockOps *) this; }

    /**
     * Returns an interface to write lock operations of this semaphore.
     * Used by constructors of AutoMultiLockN classes.
     */
    LockOps *wlock() { return (WriteLockOps *) this; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (LockHandle)

    friend class AutoWriteLock;
    friend class AutoReadLock;
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

    bool isWriteLockOnCurrentThread() const;

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (RWLockHandle)

    void lockWrite();
    void unlockWrite();
    void lockRead();
    void unlockRead();

    uint32_t writeLockLevel() const;

#ifdef VBOX_MAIN_USE_SEMRW

    RTSEMRW mSemRW;

#else /* VBOX_MAIN_USE_SEMRW */

    mutable RTCRITSECT mCritSect;
    RTSEMEVENT mGoWriteSem;
    RTSEMEVENTMULTI mGoReadSem;

    RTTHREAD mWriteLockThread;

    uint32_t mReadLockCount;
    uint32_t mWriteLockLevel;
    uint32_t mWriteLockPending;

#endif /* VBOX_MAIN_USE_SEMRW */
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

    WriteLockHandle()
    {
        RTCritSectInit (&mCritSect);
    }

    virtual ~WriteLockHandle()
    {
        RTCritSectDelete (&mCritSect);
    }

    bool isWriteLockOnCurrentThread() const
    {
        return RTCritSectIsOwner (&mCritSect);
    }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (WriteLockHandle)

    void lockWrite()
    {
#if defined(DEBUG)
        RTCritSectEnterDebug (&mCritSect,
                              "WriteLockHandle::lockWrite() return address >>>",
                              0, (RTUINTPTR) ASMReturnAddress());
#else
        RTCritSectEnter (&mCritSect);
#endif
    }

    void unlockWrite()
    {
        RTCritSectLeave (&mCritSect);
    }

    void lockRead() { lockWrite(); }
    void unlockRead() { unlockWrite(); }

    uint32_t writeLockLevel() const
    {
        return RTCritSectGetRecursion (&mCritSect);
    }

    mutable RTCRITSECT mCritSect;
};

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

    /**
     * Equivalent to <tt>#lockHandle()->rlock()</tt>.
     * Returns @c NULL false if lockHandle() returns @c NULL.
     */
    LockOps *rlock()
    {
        LockHandle *h = lockHandle();
        return h ? h->rlock() : NULL;
    }

    /**
     * Equivalent to <tt>#lockHandle()->wlock()</tt>. Returns @c NULL false if
     * lockHandle() returns @c NULL.
     */
    LockOps *wlock()
    {
        LockHandle *h = lockHandle();
        return h ? h->wlock() : NULL;
    }
};

/**
 * Provides safe management of read/write semaphores in write mode.
 *
 * A read/write semaphore is represented by the LockHandle class. This semaphore
 * can be requested ("locked") in two different modes: for reading and for
 * writing. A write lock is exclusive and acts like a mutex: only one thread can
 * acquire a write lock on the given semaphore at a time; all other threads
 * trying to request a write lock or a read lock (see below) on the same
 * semaphore will be indefinitely blocked until the owning thread releases the
 * write lock.
 *
 * A read lock is shared. This means that several threads can acquire a read
 * lock on the same semaphore at the same time provided that there is no thread
 * that holds a write lock on that semaphore. Note that when there are one or
 * more threads holding read locks, a request for a write lock on another thread
 * will be indefinitely blocked until all threads holding read locks release
 * them.
 *
 * Note that write locks can be nested -- the same thread can request a write
 * lock on the same semaphore several times. In this case, the corresponding
 * number of release calls must be done in order to completely release all
 * nested write locks and make the semaphore available for locking by other
 * threads.
 *
 * Read locks can be nested too in which case the same rule of the equal number
 * of the release calls applies. Read locks can be also nested into write
 * locks which means that the same thread can successfully request a read lock
 * if it already holds a write lock. However, please note that the opposite is
 * <b>not possible</b>: if a thread tries to request a write lock on the same
 * semaphore it is already holding a read lock, it will definitely produce a
 * <b>deadlock</b> (i.e. it will block forever waiting for itself).
 *
 * Note that instances of the AutoWriteLock class manage write locks of
 * read/write semaphores only. In order to manage read locks, please use the
 * AutoReadLock class.
 *
 * Safe semaphore management consists of the following:
 * <ul>
 *   <li>When an instance of the AutoWriteLock class is constructed given a
 *   valid semaphore handle, it will automatically request a write lock on that
 *   semaphore.
 *   </li>
 *   <li>When an instance of the AutoWriteLock class constructed given a valid
 *   semaphore handle is destroyed (e.g. goes out of scope), it will
 *   automatically release the write lock that was requested upon construction
 *   and also all nested write locks requested later using the #lock() call
 *   (note that the latter is considered to be a program logic error, see the
 *   #~AutoWriteLock() description for details).
 *   </li>
 * </ul>
 *
 * Note that the LockHandle class taken by AutoWriteLock constructors is an
 * abstract base of the read/write semaphore. You should choose one of the
 * existing subclasses of this abstract class or create your own subclass that
 * implements necessary read and write lock semantics. The most suitable choice
 * is the RWLockHandle class which provides full support for both read and write
 * locks as describerd above. Alternatively, you can use the WriteLockHandle
 * class if you only need write (exclusive) locking (WriteLockHandle requires
 * less system resources and works faster).
 *
 * A typical usage pattern of the AutoWriteLock class is as follows:
 * <code>
 *  struct Struct : public RWLockHandle
 *  {
 *      ...
 *  };
 *
 *  void foo (Struct &aStruct)
 *  {
 *      {
 *          // acquire a write lock of aStruct
 *          AutoWriteLock alock (aStruct);
 *
 *          // now we can modify aStruct in a thread-safe manner
 *          aStruct.foo = ...;
 *
 *          // note that the write lock will be automatically released upon
 *          // execution of the return statement below
 *          if (!aStruct.bar)
 *              return;
 *
 *          ...
 *      }
 *
 *      // note that the write lock is automatically released here
 *  }
 * </code>
 *
 * <b>Locking policy</b>
 *
 * When there are multiple threads and multiple objects to lock, there is always
 * a potential possibility to produce a deadlock if the lock order is mixed up.
 * Here is a classical example of a deadlock when two threads need to lock the
 * same two objects in a row but do it in different order:
 * <code>
 *  Thread 1:
 *    #1: AutoWriteLock (mFoo);
 *        ...
 *    #2: AutoWriteLock (mBar);
 *        ...
 *  Thread 2:
 *    #3: AutoWriteLock (mBar);
 *        ...
 *    #4: AutoWriteLock (mFoo);
 *        ...
 * </code>
 *
 * If the threads happen to be scheduled so that #3 completes after #1 has
 * completed but before #2 got control, the threads will hit a deadlock: Thread
 * 2 will be holding mBar and waiting for mFoo at #4 forever because Thread 1 is
 * holding mFoo and won't release it until it acquires mBar at #2 that will
 * never happen because mBar is held by Thread 2.
 *
 * One of ways to avoid the described behavior is to never lock more than one
 * obhect in a row. While it is definitely a good and safe practice, it's not
 * always possible: the application logic may require several simultaneous locks
 * in order to provide data integrity.
 *
 * One of the possibilities to solve the deadlock problem is to make sure that
 * the locking order is always the same across the application. In the above
 * example, it would mean that <b>both</b> threads should first requiest a lock
 * of mFoo and then mBar (or vice versa). One of the methods to guarantee the
 * locking order consistent is to introduce a set of locking rules. The
 * advantage of this method is that it doesn't require any special semaphore
 * implementation or additional control structures. The disadvantage is that
 * it's the programmer who must make sure these rules are obeyed across the
 * whole application so the human factor applies. Taking the simplicity of this
 * method into account, it is chosen to solve potential deadlock problems when
 * using AutoWriteLock and AutoReadLock classes. Here are the locking rules
 * that must be obeyed by <b>all</b> users of these classes. Note that if more
 * than one rule matches the given group of objects to lock, all of these rules
 * must be met:
 * <ol>
 *     <li>If there is a parent-child (or master-slave) relationship between the
 *     locked objects, parent (master) objects must be locked before child
 *     (slave) objects.
 *     </li>
 *     <li>When a group of equal objects (in terms of parent-child or
 *     master-slave relationsip) needs to be locked in a raw, the lock order
 *     must match the sort order (which must be consistent for the given group).
 * </ol>
 * Note that if there is no pragrammatically expressed sort order (e.g.
 * the objects are not part of the sorted vector or list but instead are
 * separate data members of a class), object class names sorted in alphabetical
 * order must be used to determine the lock order. If there is more than one
 * object of the given class, the object variable names' alphabetical order must
 * be used as a lock order. When objects are not represented as individual
 * variables, as in case of unsorted arrays/lists, the list of alphabetically
 * sorted object UUIDs must be used to determine the sort order.
 *
 * All non-standard locking order must be avoided by all means, but when
 * absolutely necessary, it must be clearly documented at relevant places so it
 * is well seen by other developers. For example, if a set of instances of some
 * class needs to be locked but these instances are not part of the sorted list
 * and don't have UUIDs, then the class description must state what to use to
 * determine the lock order (maybe some property that returns an unique value
 * per every object).
 */
class AutoWriteLock
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
    AutoWriteLock() : mHandle (NULL), mLockLevel (0), mGlobalLockLevel (0) {}

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock (LockHandle *aHandle)
        : mHandle (aHandle), mLockLevel (0), mGlobalLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock (LockHandle &aHandle)
        : mHandle (&aHandle), mLockLevel (0), mGlobalLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock (const Lockable &aLockable)
        : mHandle (aLockable.lockHandle()), mLockLevel (0), mGlobalLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock (const Lockable *aLockable)
        : mHandle (aLockable ? aLockable->lockHandle() : NULL)
        , mLockLevel (0), mGlobalLockLevel (0)
    { lock(); }

    /**
     * Release all write locks acquired by this instance through the #lock()
     * call and destroys the instance.
     *
     * Note that if there there are nested #lock() calls without the
     * corresponding number of #unlock() calls when the destructor is called, it
     * will assert. This is because having an unbalanced number of nested locks
     * is a program logic error which must be fixed.
     */
    ~AutoWriteLock()
    {
        if (mHandle)
        {
            if (mGlobalLockLevel)
            {
                mGlobalLockLevel -= mLockLevel;
                mLockLevel = 0;
                for (; mGlobalLockLevel; -- mGlobalLockLevel)
                    mHandle->lockWrite();
            }

            AssertMsg (mLockLevel <= 1, ("Lock level > 1: %d\n", mLockLevel));
            for (; mLockLevel; -- mLockLevel)
                mHandle->unlockWrite();
        }
    }

    /**
     * Requests a write (exclusive) lock. If a write lock is already owned by
     * this thread, increases the lock level (allowing for nested write locks on
     * the same thread). Blocks indefinitely if a write lock or a read lock is
     * already owned by another thread until that tread releases the locks,
     * otherwise returns immediately.
     */
    void lock()
    {
        if (mHandle)
        {
            mHandle->lockWrite();
            ++ mLockLevel;
            Assert (mLockLevel != 0 /* overflow? */);
        }
    }

    /**
     * Decreases the write lock level increased by #lock(). If the level drops
     * to zero (e.g. the number of nested #unlock() calls matches the number of
     * nested #lock() calls), releases the lock making the managed semaphore
     * available for locking by other threads.
     */
    void unlock()
    {
        if (mHandle)
        {
            AssertReturnVoid (mLockLevel != 0 /* unlock() w/o preceding lock()? */);
            mHandle->unlockWrite();
            -- mLockLevel;
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
    void leave()
    {
        if (mHandle)
        {
            AssertReturnVoid (mLockLevel != 0 /* leave() w/o preceding lock()? */);
            AssertReturnVoid (mGlobalLockLevel == 0 /* second leave() in a row? */);

            mGlobalLockLevel = mHandle->writeLockLevel();
            AssertReturnVoid (mGlobalLockLevel >= mLockLevel /* logic error! */);

            for (uint32_t left = mGlobalLockLevel; left; -- left)
                mHandle->unlockWrite();
        }
    }

    /**
     * Causes the current thread to restore the write lock level after the
     * #leave() call. This call will indefinitely block if another thread has
     * successfully acquired a write or a read lock on the same semaphore in
     * between.
     */
    void enter()
    {
        if (mHandle)
        {
            AssertReturnVoid (mLockLevel != 0 /* enter() w/o preceding lock()+leave()? */);
            AssertReturnVoid (mGlobalLockLevel != 0 /* enter() w/o preceding leave()? */);

            for (; mGlobalLockLevel; -- mGlobalLockLevel)
                mHandle->lockWrite();
        }
    }

    /** Returns @c true if this instance manages a null semaphore handle. */
    bool isNull() const { return mHandle == NULL; }
    bool operator !() const { return isNull(); }

    /**
     * Returns @c true if the current thread holds a write lock on the managed
     * read/write semaphore. Returns @c false if the managed semaphore is @c
     * NULL.
     *
     * @note Intended for debugging only.
     */
    bool isWriteLockOnCurrentThread() const
    {
        return mHandle ? mHandle->isWriteLockOnCurrentThread() : false;
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
    uint32_t writeLockLevel() const
    {
        return mHandle ? mHandle->writeLockLevel() : 0;
    }

    /**
     * Returns @c true if this instance manages the given semaphore handle.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const LockHandle &aHandle) const { return mHandle == &aHandle; }

    /**
     * Returns @c true if this instance manages the given semaphore handle.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const LockHandle *aHandle) const { return mHandle == aHandle; }

    /**
     * Returns @c true if this instance manages the given lockable object.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const Lockable &aLockable)
    {
         return belongsTo (aLockable.lockHandle());
    }

    /**
     * Returns @c true if this instance manages the given lockable object.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const Lockable *aLockable)
    {
         return aLockable && belongsTo (aLockable->lockHandle());
    }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoWriteLock)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoWriteLock)

    LockHandle *mHandle;
    uint32_t mLockLevel;
    uint32_t mGlobalLockLevel;

    template <size_t> friend class AutoMultiWriteLockBase;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Provides safe management of read/write semaphores in read mode.
 *
 * This class differs from the AutoWriteLock class is so that it's #lock() and
 * #unlock() methods requests and release read (shared) locks on the managed
 * read/write semaphore instead of write (exclusive) locks. See the
 * AutoWriteLock class description for more information about read and write
 * locks.
 *
 * Safe semaphore management consists of the following:
 * <ul>
 *   <li>When an instance of the AutoReadLock class is constructed given a
 *   valid semaphore handle, it will automatically request a read lock on that
 *   semaphore.
 *   </li>
 *   <li>When an instance of the AutoReadLock class constructed given a valid
 *   semaphore handle is destroyed (e.g. goes out of scope), it will
 *   automatically release the read lock that was requested upon construction
 *   and also all nested read locks requested later using the #lock() call (note
 *   that the latter is considered to be a program logic error, see the
 *   #~AutoReadLock() description for details).
 *   </li>
 * </ul>
 *
 * Note that the LockHandle class taken by AutoReadLock constructors is an
 * abstract base of the read/write semaphore. You should choose one of the
 * existing subclasses of this abstract class or create your own subclass that
 * implements necessary read and write lock semantics. The most suitable choice
 * is the RWLockHandle class which provides full support for both read and write
 * locks as describerd in AutoWriteLock docs. Alternatively, you can use the
 * WriteLockHandle class if you only need write (exclusive) locking
 * (WriteLockHandle requires less system resources and works faster).
 *
 * However, please note that it absolutely does not make sense to manage
 * WriteLockHandle semaphores with AutoReadLock instances because
 * AutoReadLock instances will behave like AutoWriteLock instances in this
 * case since WriteLockHandle provides only exclusive write locking. You have
 * been warned.

 * A typical usage pattern of the AutoReadLock class is as follows:
 * <code>
 *  struct Struct : public RWLockHandle
 *  {
 *      ...
 *  };
 *
 *  void foo (Struct &aStruct)
 *  {
 *      {
 *          // acquire a read lock of aStruct (note that two foo() calls may be
 *          executed on separate threads simultaneously w/o blocking each other)
 *          AutoReadLock alock (aStruct);
 *
 *          // now we can read aStruct in a thread-safe manner
 *          if (aStruct.foo)
 *              ...;
 *
 *          // note that the read lock will be automatically released upon
 *          // execution of the return statement below
 *          if (!aStruct.bar)
 *              return;
 *
 *          ...
 *      }
 *
 *      // note that the read lock is automatically released here
 *  }
 * </code>
 */
class AutoReadLock
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
    AutoReadLock() : mHandle (NULL), mLockLevel (0) {}

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock (LockHandle *aHandle)
        : mHandle (aHandle), mLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock (LockHandle &aHandle)
        : mHandle (&aHandle), mLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock (const Lockable &aLockable)
        : mHandle (aLockable.lockHandle()), mLockLevel (0)
    { lock(); }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock (const Lockable *aLockable)
        : mHandle (aLockable ? aLockable->lockHandle() : NULL)
        , mLockLevel (0)
    { lock(); }

    /**
     * Release all read locks acquired by this instance through the #lock()
     * call and destroys the instance.
     *
     * Note that if there there are nested #lock() calls without the
     * corresponding number of #unlock() calls when the destructor is called, it
     * will assert. This is because having an unbalanced number of nested locks
     * is a program logic error which must be fixed.
     */
    ~AutoReadLock()
    {
        if (mHandle)
        {
            AssertMsg (mLockLevel <= 1, ("Lock level > 1: %d\n", mLockLevel));
            for (; mLockLevel; -- mLockLevel)
                mHandle->unlockRead();
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
    void lock()
    {
        if (mHandle)
        {
            mHandle->lockRead();
            ++ mLockLevel;
            Assert (mLockLevel != 0 /* overflow? */);
        }
    }

    /**
     * Decreases the read lock level increased by #lock(). If the level drops to
     * zero (e.g. the number of nested #unlock() calls matches the number of
     * nested #lock() calls), releases the lock making the managed semaphore
     * available for locking by other threads.
     */
    void unlock()
    {
        if (mHandle)
        {
            AssertReturnVoid (mLockLevel != 0 /* unlock() w/o preceding lock()? */);
            mHandle->unlockRead();
            -- mLockLevel;
        }
    }

    /** Returns @c true if this instance manages a null semaphore handle. */
    bool isNull() const { return mHandle == NULL; }
    bool operator !() const { return isNull(); }

    /**
     * Returns @c true if this instance manages the given semaphore handle.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const LockHandle &aHandle) const { return mHandle == &aHandle; }

    /**
     * Returns @c true if this instance manages the given semaphore handle.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const LockHandle *aHandle) const { return mHandle == aHandle; }

    /**
     * Returns @c true if this instance manages the given lockable object.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const Lockable &aLockable)
    {
         return belongsTo (aLockable.lockHandle());
    }

    /**
     * Returns @c true if this instance manages the given lockable object.
     *
     * @note Intended for debugging only.
     */
    bool belongsTo (const Lockable *aLockable)
    {
         return aLockable && belongsTo (aLockable->lockHandle());
    }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoReadLock)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoReadLock)

    LockHandle *mHandle;
    uint32_t mLockLevel;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Helper template class for AutoMultiLockN classes.
 *
 * @param Cnt number of read/write semaphores to manage.
 */
template <size_t Cnt>
class AutoMultiLockBase
{
public:

    /**
     * Releases all locks if not yet released by #unlock() and destroys the
     * instance.
     */
    ~AutoMultiLockBase()
    {
        if (mIsLocked)
            unlock();
    }

    /**
     * Calls LockOps::lock() methods of all managed semaphore handles
     * in order they were passed to the constructor.
     *
     * Note that as opposed to LockHandle::lock(), this call cannot be nested
     * and will assert if so.
     */
    void lock()
    {
        AssertReturnVoid (!mIsLocked);

        size_t i = 0;
        while (i < ELEMENTS (mOps))
            if (mOps [i])
                mOps [i ++]->lock();
        mIsLocked = true;
    }

    /**
     * Calls LockOps::unlock() methods of all managed semaphore handles in
     * reverse to the order they were passed to the constructor.
     *
     * Note that as opposed to LockHandle::unlock(), this call cannot be nested
     * and will assert if so.
     */
    void unlock()
    {
        AssertReturnVoid (mIsLocked);

        AssertReturnVoid (ELEMENTS (mOps) > 0);
        size_t i = ELEMENTS (mOps);
        do
            if (mOps [-- i])
                mOps [i]->unlock();
        while (i != 0);
        mIsLocked = false;
    }

protected:

    AutoMultiLockBase() : mIsLocked (false) {}

    LockOps *mOps [Cnt];
    bool mIsLocked;

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMultiLockBase)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoMultiLockBase)
};

/** AutoMultiLockBase <0> is meaningless and forbidden. */
template<>
class AutoMultiLockBase <0> { private : AutoMultiLockBase(); };

/** AutoMultiLockBase <1> is meaningless and forbidden. */
template<>
class AutoMultiLockBase <1> { private : AutoMultiLockBase(); };

////////////////////////////////////////////////////////////////////////////////

/* AutoMultiLockN class definitions */

#define A(n) LockOps *l##n
#define B(n) mOps [n] = l##n

/**
 * AutoMultiLock for 2 locks.
 *
 * The AutoMultiLockN family of classes provides a possibility to manage several
 * read/write semaphores at once. This is handy if all managed semaphores need
 * to be locked and unlocked synchronously and will also help to avoid locking
 * order errors.
 *
 * Instances of AutoMultiLockN classes are constructed from a list of LockOps
 * arguments. The AutoMultiLockBase::lock() method will make sure that the given
 * list of semaphores represented by LockOps pointers will be locked in order
 * they are passed to the constructor. The AutoMultiLockBase::unlock() method
 * will make sure that they will be unlocked in reverse order.
 *
 * The type of the lock to request is specified for each semaphore individually
 * using the corresponding LockOps getter of a LockHandle or Lockable object:
 * LockHandle::wlock() in order to request a write lock or LockHandle::rlock()
 * in order to request a read lock.
 *
 * Here is a typical usage pattern:
 * <code>
 *  ...
 *  LockHandle data1, data2;
 *  ...
 *  {
 *      AutoMultiLock2 multiLock (data1.wlock(), data2.rlock());
 *      // both locks are held here:
 *      // - data1 is locked in write mode (like AutoWriteLock)
 *      // - data2 is locked in read mode (like AutoReadLock)
 *  }
 *  // both locks are released here
 * </code>
 */
class AutoMultiLock2 : public AutoMultiLockBase <2>
{
public:
    AutoMultiLock2 (A(0), A(1))
    { B(0); B(1); lock(); }
};

/** AutoMultiLock for 3 locks. See AutoMultiLock2 for more information. */
class AutoMultiLock3 : public AutoMultiLockBase <3>
{
public:
    AutoMultiLock3 (A(0), A(1), A(2))
    { B(0); B(1); B(2); lock(); }
};

/** AutoMultiLock for 4 locks. See AutoMultiLock2 for more information. */
class AutoMultiLock4 : public AutoMultiLockBase <4>
{
public:
    AutoMultiLock4 (A(0), A(1), A(2), A(3))
    { B(0); B(1); B(2); B(3); lock(); }
};

#undef B
#undef A

////////////////////////////////////////////////////////////////////////////////

/**
 * Helper template class for AutoMultiWriteLockN classes.
 *
 * @param Cnt number of write semaphores to manage.
 */
template <size_t Cnt>
class AutoMultiWriteLockBase
{
public:

    /**
     * Calls AutoWriteLock::lock() methods for all managed semaphore handles in
     * order they were passed to the constructor.
     */
    void lock()
    {
        size_t i = 0;
        while (i < ELEMENTS (mLocks))
            mLocks [i ++].lock();
    }

    /**
     * Calls AutoWriteLock::unlock() methods for all managed semaphore handles
     * in reverse to the order they were passed to the constructor.
     */
    void unlock()
    {
        AssertReturnVoid (ELEMENTS (mLocks) > 0);
        size_t i = ELEMENTS (mLocks);
        do
            mLocks [-- i].unlock();
        while (i != 0);
    }

    /**
     * Calls AutoWriteLock::leave() methods for all managed semaphore handles in
     * reverse to the order they were passed to the constructor.
     */
    void leave()
    {
        AssertReturnVoid (ELEMENTS (mLocks) > 0);
        size_t i = ELEMENTS (mLocks);
        do
            mLocks [-- i].leave();
        while (i != 0);
    }

    /**
     * Calls AutoWriteLock::enter() methods for all managed semaphore handles in
     * order they were passed to the constructor.
     */
    void enter()
    {
        size_t i = 0;
        while (i < ELEMENTS (mLocks))
            mLocks [i ++].enter();
    }

protected:

    AutoMultiWriteLockBase() {}

    void setLockHandle (size_t aIdx, LockHandle *aHandle)
    { mLocks [aIdx].mHandle = aHandle; }

private:

    AutoWriteLock mLocks [Cnt];

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMultiWriteLockBase)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoMultiWriteLockBase)
};

/** AutoMultiWriteLockBase <0> is meaningless and forbidden. */
template<>
class AutoMultiWriteLockBase <0> { private : AutoMultiWriteLockBase(); };

/** AutoMultiWriteLockBase <1> is meaningless and forbidden. */
template<>
class AutoMultiWriteLockBase <1> { private : AutoMultiWriteLockBase(); };

////////////////////////////////////////////////////////////////////////////////

/* AutoMultiLockN class definitions */

#define A(n) LockHandle *l##n
#define B(n) setLockHandle (n, l##n)

#define C(n) Lockable *l##n
#define D(n) setLockHandle (n, l##n ? l##n->lockHandle() : NULL)

/**
 * AutoMultiWriteLock for 2 locks.
 *
 * The AutoMultiWriteLockN family of classes provides a possibility to manage
 * several read/write semaphores at once. This is handy if all managed
 * semaphores need to be locked and unlocked synchronously and will also help to
 * avoid locking order errors.
 *
 * The functionality of the AutoMultiWriteLockN class family is similar to the
 * functionality of the AutoMultiLockN class family (see the AutoMultiLock2
 * class for details) with two important differences:
 * <ol>
 *     <li>Instances of AutoMultiWriteLockN classes are constructed from a list
 *     of LockHandle or Lockable arguments directly instead of getting
 *     intermediate LockOps interface pointers.
 *     </li>
 *     <li>All locks are requested in <b>write</b> mode.
 *     </li>
 *     <li>Since all locks are requested in write mode, bulk
 *     AutoMultiWriteLockBase::leave() and AutoMultiWriteLockBase::enter()
 *     operations are also available, that will leave and enter all managed
 *     semaphores at once in the proper order (similarly to
 *     AutoMultiWriteLockBase::lock() and AutoMultiWriteLockBase::unlock()).
 *     </li>
 * </ol>
 *
 * Here is a typical usage pattern:
 * <code>
 *  ...
 *  LockHandle data1, data2;
 *  ...
 *  {
 *      AutoMultiWriteLock2 multiLock (&data1, &data2);
 *      // both locks are held in write mode here
 *  }
 *  // both locks are released here
 * </code>
 */
class AutoMultiWriteLock2 : public AutoMultiWriteLockBase <2>
{
public:
    AutoMultiWriteLock2 (A(0), A(1))
    { B(0); B(1); lock(); }
    AutoMultiWriteLock2 (C(0), C(1))
    { D(0); D(1); lock(); }
};

/** AutoMultiWriteLock for 3 locks. See AutoMultiWriteLock2 for more details. */
class AutoMultiWriteLock3 : public AutoMultiWriteLockBase <3>
{
public:
    AutoMultiWriteLock3 (A(0), A(1), A(2))
    { B(0); B(1); B(2); lock(); }
    AutoMultiWriteLock3 (C(0), C(1), C(2))
    { D(0); D(1); D(2); lock(); }
};

/** AutoMultiWriteLock for 4 locks. See AutoMultiWriteLock2 for more details. */
class AutoMultiWriteLock4 : public AutoMultiWriteLockBase <4>
{
public:
    AutoMultiWriteLock4 (A(0), A(1), A(2), A(3))
    { B(0); B(1); B(2); B(3); lock(); }
    AutoMultiWriteLock4 (C(0), C(1), C(2), C(3))
    { D(0); D(1); D(2); D(3); lock(); }
};

#undef D
#undef C
#undef B
#undef A

} /* namespace util */

#endif // ____H_AUTOLOCK

