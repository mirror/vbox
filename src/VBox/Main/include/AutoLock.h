/** @file
 *
 * AutoLock: smart critical section wrapper
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_AUTOLOCK
#define ____H_AUTOLOCK

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>

#if defined(DEBUG)
# include <iprt/asm.h> // for ASMReturnAddress
#endif

namespace util
{

template <size_t> class AutoMultiLock;
namespace internal { struct LockableTag; }

/**
 *  Smart class to safely manage critical sections. Also provides ecplicit
 *  lock, unlock leave and enter operations.
 *
 *  When constructing an instance, it enters the given critical
 *  section. This critical section will be exited automatically when the
 *  instance goes out of scope (i.e. gets destroyed).
 */
class AutoLock
{
public:

    #if defined(DEBUG)
    # define ___CritSectEnter(cs) \
        RTCritSectEnterDebug ((cs), \
            "AutoLock::lock()/enter() return address >>>", 0, \
            (RTUINTPTR) ASMReturnAddress())
    #else
    # define ___CritSectEnter(cs) RTCritSectEnter ((cs))
    #endif

    /**
     *  Lock (critical section) handle. An auxiliary base class for structures
     *  that need locking. Instances of classes inherited from it can be passed
     *  as arguments to the AutoLock constructor.
     */
    class Handle
    {
    public:

        Handle() { RTCritSectInit (&mCritSect); }
        virtual ~Handle() { RTCritSectDelete (&mCritSect); }

        /** Returns |true| if this handle is locked on the current thread. */
        bool isLockedOnCurrentThread() const
        {
            return RTCritSectIsOwner (&mCritSect);
        }

        /** Returns a tag to lock this handle for reading by AutoMultiLock */
        internal::LockableTag rlock() const;
        /** Returns a tag to lock this handle for writing by AutoMultiLock */
        internal::LockableTag wlock() const;

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (Handle)

        mutable RTCRITSECT mCritSect;

        friend class AutoLock;
        template <size_t> friend class AutoMultiLock;
    };

    /**
     *  Lockable interface. An abstract base for classes that need locking.
     *  Unlike Handle that makes the lock handle a part of class data, this
     *  class allows subclasses to decide which lock handle to use.
     */
    class Lockable
    {
    public:

        /**
         *  Returns a pointer to a Handle used by AutoLock for locking.
         *  Subclasses are allowed to return |NULL| -- in this case,
         *  the AutoLock object constructed using an instance of such
         *  subclass will simply turn into no-op.
         */
        virtual Handle *lockHandle() const = 0;

        /**
         *  Equivalent to |#lockHandle()->isLockedOnCurrentThread()|.
         *  Returns |false| if lockHandle() returns NULL.
         */
        bool isLockedOnCurrentThread()
        {
            Handle *h = lockHandle();
            return h ? h->isLockedOnCurrentThread() : false;
        }

        /**
         *  Returns a tag to lock this handle for reading by AutoMultiLock.
         *  Shortcut to |lockHandle()->rlock()|.
         *  The returned tag is a no-op, when lockHandle() returns |NULL|.
         */
        internal::LockableTag rlock() const;

        /**
         *  Returns a tag to lock this handle for writing by AutoMultiLock.
         *  Shortcut to |lockHandle()->wlock()|.
         *  The returned tag is a no-op, when lockHandle() returns |NULL|.
         */
        internal::LockableTag wlock() const;
    };

    AutoLock() : mCritSect (NULL), mLevel (0), mLeftLevel (0) {}

    AutoLock (RTCRITSECT &aCritSect)
        : mCritSect (&aCritSect), mLevel (0), mLeftLevel (0) { lock(); }

    AutoLock (RTCRITSECT *aCritSect)
        : mCritSect (aCritSect), mLevel (0), mLeftLevel (0) { lock(); }

    AutoLock (const Handle &aHandle)
        : mCritSect (&aHandle.mCritSect), mLevel (0), mLeftLevel (0) { lock(); }

    AutoLock (const Handle *aHandle)
        : mCritSect (aHandle ? &aHandle->mCritSect : NULL)
        , mLevel (0), mLeftLevel (0) { lock(); }

    AutoLock (const Lockable &aLockable)
        : mCritSect (critSect (&aLockable))
        , mLevel (0), mLeftLevel (0) { lock(); }

    AutoLock (const Lockable *aLockable)
        : mCritSect (aLockable ? critSect (aLockable) : NULL)
        , mLevel (0), mLeftLevel (0) { lock(); }

    ~AutoLock()
    {
        if (mCritSect)
        {
            if (mLeftLevel)
            {
                mLeftLevel -= mLevel;
                mLevel = 0;
                for (; mLeftLevel; -- mLeftLevel)
                    RTCritSectEnter (mCritSect);
            }
            AssertMsg (mLevel <= 1, ("Lock level > 1: %d\n", mLevel));
            for (; mLevel; -- mLevel)
                RTCritSectLeave (mCritSect);
        }
    }

    /**
     *  Tries to acquire the lock or increases the lock level
     *  if the lock is already owned by this thread.
     */
    void lock()
    {
        if (mCritSect)
        {
            AssertMsgReturn (mLeftLevel == 0, ("lock() after leave()\n"), (void) 0);
            ___CritSectEnter (mCritSect);
            ++ mLevel;
        }
    }

    /**
     *  Decreases the lock level. If the level goes to zero, the lock
     *  is released by the current thread.
     */
    void unlock()
    {
        if (mCritSect)
        {
            AssertMsgReturn (mLevel > 0, ("Lock level is zero\n"), (void) 0);
            AssertMsgReturn (mLeftLevel == 0, ("lock() after leave()\n"), (void) 0);
            -- mLevel;
            RTCritSectLeave (mCritSect);
        }
    }

    /**
     *  Causes the current thread to completely release the lock
     *  (including locks acquired by all other instances of this class
     *  referring to the same object or handle). #enter() must be called
     *  to acquire the lock back and restore all lock levels.
     */
    void leave()
    {
        if (mCritSect)
        {
            AssertMsg (mLevel > 0, ("Lock level is zero\n"));
            AssertMsgReturn (mLeftLevel == 0, ("leave() w/o enter()\n"), (void) 0);
            mLeftLevel = RTCritSectGetRecursion (mCritSect);
            for (uint32_t left = mLeftLevel; left; -- left)
                RTCritSectLeave (mCritSect);
            Assert (mLeftLevel >= mLevel);
        }
    }

    /**
     *  Causes the current thread to acquire the lock again and restore
     *  all lock levels after calling #leave().
     */
    void enter()
    {
        if (mCritSect)
        {
            AssertMsg (mLevel > 0, ("Lock level is zero\n"));
            AssertMsgReturn (mLeftLevel > 0, ("enter() w/o leave()\n"), (void) 0);
            for (; mLeftLevel; -- mLeftLevel)
                ___CritSectEnter (mCritSect);
        }
    }

    /**
     *  Current level of the nested lock. 1 means the lock is not currently
     *  nested.
     */
    uint32_t level() const { return RTCritSectGetRecursion (mCritSect); }

    bool isNull() const { return mCritSect == NULL; }
    bool operator !() const { return isNull(); }

    /** Returns |true| if this instance manages the given lock handle. */
    bool belongsTo (const Handle &aHandle)
    {
         return &aHandle.mCritSect == mCritSect;
    }

    /** Returns |true| if this instance manages the given lock handle. */
    bool belongsTo (const Handle *aHandle)
    {
         return aHandle && &aHandle->mCritSect == mCritSect;
    }

    /** Returns |true| if this instance manages the given lockable object. */
    bool belongsTo (const Lockable &aLockable)
    {
         return belongsTo (aLockable.lockHandle());
    }

    /** Returns |true| if this instance manages the given lockable object. */
    bool belongsTo (const Lockable *aLockable)
    {
         return aLockable && belongsTo (aLockable->lockHandle());
    }

    /**
     *  Returns a tag to lock the given Lockable for reading by AutoMultiLock.
     *  Shortcut to |aL->lockHandle()->rlock()|.
     *  The returned tag is a no-op when @a aL is |NULL|.
     */
    static internal::LockableTag maybeRlock (Lockable *aL);

    /**
     *  Returns a tag to lock the given Lockable for writing by AutoMultiLock.
     *  Shortcut to |aL->lockHandle()->wlock()|.
     *  The returned tag is a no-op when @a aL is |NULL|.
     */
    static internal::LockableTag maybeWlock (Lockable *aL);

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoLock)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoLock)

    inline static RTCRITSECT *critSect (const Lockable *l)
    {
        Assert (l);
        Handle *h = l->lockHandle();
        return h ? &h->mCritSect : NULL;
    }

    RTCRITSECT *mCritSect;
    uint32_t mLevel;
    uint32_t mLeftLevel;

    #undef ___CritSectEnter
};

/**
 *  Prototype. Later will be used to acquire a read-only lock
 *  (read-only locks differ from regular (write) locks so that more than one
 *  read lock can be acquired simultaneously provided that there are no
 *  active write locks).
 *  @todo Implement it!
 */
class AutoReaderLock : public AutoLock
{
public:

    AutoReaderLock (const Handle &aHandle) : AutoLock (aHandle) {}
    AutoReaderLock (const Handle *aHandle) : AutoLock (aHandle) {}

    AutoReaderLock (const Lockable &aLockable) : AutoLock (aLockable) {}
    AutoReaderLock (const Lockable *aLockable) : AutoLock (aLockable) {}

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoReaderLock)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoReaderLock)
};

namespace internal
{
    /**
     *  @internal
     *  Special struct to differentiate between read and write locks
     *  in AutoMultiLock constructors.
     */
    struct LockableTag
    {
        LockableTag (const AutoLock::Handle *h, char m)
            : handle (h), mode (m) {}
        const AutoLock::Handle * const handle;
        const char mode;
    };
}

inline internal::LockableTag AutoLock::Handle::rlock() const
{
    return internal::LockableTag (this, 'r');
}

inline internal::LockableTag AutoLock::Handle::wlock() const
{
    return internal::LockableTag (this, 'w');
}

inline internal::LockableTag AutoLock::Lockable::rlock() const
{
    return internal::LockableTag (lockHandle(), 'r');
}

inline internal::LockableTag AutoLock::Lockable::wlock() const
{
    return internal::LockableTag (lockHandle(), 'w');
}

/* static */
inline internal::LockableTag AutoLock::maybeRlock (AutoLock::Lockable *aL)
{
    return internal::LockableTag (aL ? aL->lockHandle() : NULL, 'r');
}

/* static */
inline internal::LockableTag AutoLock::maybeWlock (AutoLock::Lockable *aL)
{
    return internal::LockableTag (aL ? aL->lockHandle() : NULL, 'w');
}

/**
 *  Smart template class to safely enter and leave a list of critical sections.
 *
 *  When constructing an instance, it enters all given critical sections
 *  (in an "atomic", "all or none" fashion). These critical sections will be
 *  exited automatically when the instance goes out of scope (i.e. gets destroyed).
 *
 *  It is possible to lock different critical sections in two different modes:
 *  for writing (as AutoLock does) or for reading (as AutoReaderLock does).
 *  The lock mode is determined by the method called on an AutoLock::Handle or
 *  AutoLock::Lockable instance when passing it to the AutoMultiLock constructor:
 *  |rlock()| to lock for reading or |wlock()| to lock for writing.
 *
 *  Instances of this class are constructed as follows:
 *  <code>
 *      ...
 *      AutoLock::Handle data1, data2;
 *      ...
 *      {
 *          AutoMultiLock <2> multiLock (data1.wlock(), data2.rlock());
 *          // all locks are entered here:
 *          // data1 is entered in write mode (like AutoLock)
 *          // data2 is entered in read mode (like AutoReaderLock),
 *      }
 *      // all locks are exited here
 *  </code>
 *
 *  The number of critical sections passed to the constructor must exactly
 *  match the number specified as the @a tCnt parameter of the template.
 *
 *  @param tCnt number of critical sections to manage
 */
template <size_t tCnt>
class AutoMultiLock
{
public:

    #if defined(DEBUG)
    # define ___CritSectEnterMulti(n, acs) \
        RTCritSectEnterMultipleDebug ((n), (acs), \
            "AutoMultiLock instantiation address >>>", 0, \
            (RTUINTPTR) ASMReturnAddress())
    #else
    # define ___CritSectEnterMulti(n, acs) RTCritSectEnterMultiple ((n), (acs))
    #endif

    #define A(n) internal::LockableTag l##n
    #define B(n) if (l##n.handle) { /* skip NULL tags */ \
                     mS[i] = &l##n.handle->mCritSect; \
                     mM[i++] = l##n.mode; \
                 } else
    #define C(n) \
        mLocked = true; \
        mM [0] = 0; /* for safety in case of early return */ \
        AssertMsg (tCnt == n, \
            ("This AutoMultiLock is for %d locks, but %d were passed!\n", tCnt, n)); \
        if (tCnt != n) return; \
        int i = 0
    /// @todo (dmik) this will change when we switch to RTSemRW*
    #define D() mM [i] = 0; /* end of array */ \
                ___CritSectEnterMulti ((unsigned) strlen (mM), mS)

    AutoMultiLock (A(0), A(1))
    {
        C(2);
        B(0);
        B(1);
        D();
    }
    AutoMultiLock (A(0), A(1), A(2))
    { C(3); B(0); B(1); B(2); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3))
    { C(4); B(0); B(1); B(2); B(3); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4))
    { C(5); B(0); B(1); B(2); B(3); B(4); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4), A(5))
    { C(6); B(0); B(1); B(2); B(3); B(4); B(5); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4), A(5), A(6))
    { C(7); B(0); B(1); B(2); B(3); B(4); B(5); B(6); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7))
    { C(8); B(0); B(1); B(2); B(3); B(4); B(5); B(6); B(7); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8))
    { C(9); B(0); B(1); B(2); B(3); B(4); B(5); B(6); B(7); B(8); D(); }
    AutoMultiLock (A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9))
    { C(10); B(0); B(1); B(2); B(3); B(4); B(5); B(6); B(7); B(8); B(9); D(); }

    #undef D
    #undef C
    #undef B
    #undef A

    /**
     *  Releases all locks if not yet released by #leave() and
     *  destroys the instance.
     */
    ~AutoMultiLock()
    {
        /// @todo (dmik) this will change when we switch to RTSemRW*
        if (mLocked)
            RTCritSectLeaveMultiple ((unsigned) strlen (mM), mS);
    }

    /**
     *  Releases all locks temporarily in order to enter them again using
     *  #enter().
     *
     *  @note There is no need to call this method unless you want later call
     *  #enter(). The destructor calls it automatically when necessary.
     *
     *  @note Calling this method twice without calling #enter() in between will
     *  definitely fail.
     *
     *  @note Unlike AutoLock::leave(), this method doesn't cause a complete
     *  release of all involved locks; if any of the locks was entered on the
     *  current thread prior constructing the AutoMultiLock instanve, they will
     *  remain acquired after this call! For this reason, using this method in
     *  the custom code doesn't make any practical sense.
     *
     *  @todo Rename this method to unlock() and rename #enter() to lock()
     *  for similarity with AutoLock.
     */
    void leave()
    {
        AssertMsgReturn (mLocked, ("Already released all locks"), (void) 0);
        /// @todo (dmik) this will change when we switch to RTSemRW*
        RTCritSectLeaveMultiple ((unsigned) strlen (mM), mS);
        mLocked = false;
    }

    /**
     *  Tries to enter all locks temporarily released by #unlock().
     *
     *  @note This method succeeds only after #unlock() and always fails
     *  otherwise.
     */
    void enter()
    {
        AssertMsgReturn (!mLocked, ("Already entered all locks"), (void) 0);
        ___CritSectEnterMulti ((unsigned) strlen (mM), mS);
        mLocked = true;
    }

private:

    AutoMultiLock();

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMultiLock)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoMultiLock)

    RTCRITSECT *mS [tCnt];
    char mM [tCnt + 1];
    bool mLocked;

    #undef ___CritSectEnterMulti
};

/**
 *  Disable instantiations of AutoMultiLock for zero and one
 *  number of locks.
 */
template<>
class AutoMultiLock <0> { private : AutoMultiLock(); };

template<>
class AutoMultiLock <1> { private : AutoMultiLock(); };

} // namespace util

#endif // ____H_AUTOLOCK

