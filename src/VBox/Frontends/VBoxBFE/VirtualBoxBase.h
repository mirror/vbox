/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declarations of the BFE base classes
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIRTUALBOXBASEIMPL
#define ____H_VIRTUALBOXBASEIMPL

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"  // Our wrapper for COM definitions left in the code
#else
# include <VBox/com/defs.h>
#endif

#include <VBox/com/assert.h>  // For the AssertComRC macro

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/string.h>
#include <iprt/asm.h> // for ASMReturnAddress

#include <list>
#include <map>

// macros and inlines
////////////////////////////////////////////////////////////////////////////////

/**
 * A lightweight replacement for the COM setError function.  I am
 * assuming that this is only called in circumstances justifying
 * an assertion.
 *
 * @returns error number
 * @param   iNum      error number - this is simply returned
 * @param   pszFormat formatted error message
 */
static inline int setError(int iNum, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    AssertMsgFailed((pszFormat, args));
    va_end(args);
    return iNum;
}

/**
 * Translate an error string.  We do not do translation.
 */
#define tr(a) a

/**
 *  A special version of the Assert macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  In the debug build, this macro is equivalent to Assert.
 *  In the release build, this macro uses |setError (E_FAIL, ...)| to set the
 *  error info from the asserted expression.
 *
 *  @see VirtualBoxSupportErrorInfoImpl::setError
 *
 *  @param   expr    Expression which should be true.
 */
#if defined (DEBUG)
#define ComAssert(expr)    Assert (expr)
#else
#define ComAssert(expr)    \
    do { } while (0)
#endif

/**
 *  A special version of the AssertMsg macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   expr    Expression which should be true.
 *  @param   a       printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsg(expr, a)  AssertMsg (expr, a)
#else
#define ComAssertMsg(expr, a)  \
    do { } while (0)
#endif

/**
 *  A special version of the AssertRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 * @param   vrc     VBox status code.
 */
#if defined (DEBUG)
#define ComAssertRC(vrc)    AssertRC (vrc)
#else
#define ComAssertRC(vrc)    ComAssertMsgRC (vrc, ("%Vra", vrc))
#endif

/**
 *  A special version of the AssertMsgRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   vrc    VBox status code.
 *  @param   msg    printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsgRC(vrc, msg)    AssertMsgRC (vrc, msg)
#else
#define ComAssertMsgRC(vrc, msg)    ComAssertMsg (VBOX_SUCCESS (vrc), msg)
#endif


/**
 *  A special version of the AssertFailed macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 */
#if defined (DEBUG)
#define ComAssertFailed()   AssertFailed()
#else
#define ComAssertFailed()   \
    do { } while (0)
#endif

/**
 *  A special version of the AssertMsgFailed macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   a   printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsgFailed(a)   AssertMsgFailed(a)
#else
#define ComAssertMsgFailed(a)   \
    do { } while (0)
#endif

/**
 *  A special version of the AssertComRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param rc   COM result code
 */
#if defined (DEBUG)
#define ComAssertComRC(rc)  AssertComRC (rc)
#else
#define ComAssertComRC(rc)  ComAssertMsg (SUCCEEDED (rc), ("COM RC = 0x%08X\n", rc))
#endif


/** Special version of ComAssert that returns ret if expr fails */
#define ComAssertRet(expr, ret)             \
    do { ComAssert (expr); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertMsg that returns ret if expr fails */
#define ComAssertMsgRet(expr, a, ret)       \
    do { ComAssertMsg (expr, a); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertRC that returns ret if vrc does not succeed */
#define ComAssertRCRet(vrc, ret)            \
    do { ComAssertRC (vrc); if (!VBOX_SUCCESS (vrc)) return (ret); } while (0)
/** Special version of ComAssertMsgRC that returns ret if vrc does not succeed */
#define ComAssertMsgRCRet(vrc, msg, ret)    \
    do { ComAssertMsgRC (vrc, msg); if (!VBOX_SUCCESS (vrc)) return (ret); } while (0)
/** Special version of ComAssertFailed that returns ret */
#define ComAssertFailedRet(ret)             \
    do { ComAssertFailed(); return (ret); } while (0)
/** Special version of ComAssertMsgFailed that returns ret */
#define ComAssertMsgFailedRet(msg, ret)     \
    do { ComAssertMsgFailed (msg); return (ret); } while (0)
/** Special version of ComAssertComRC that returns ret if rc does not succeed */
#define ComAssertComRCRet(rc, ret)          \
    do { ComAssertComRC (rc); if (!SUCCEEDED (rc)) return (ret); } while (0)


/** Special version of ComAssert that evaulates eval and breaks if expr fails */
#define ComAssertBreak(expr, eval)                \
    if (1) { ComAssert (expr); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsg that evaulates eval and breaks if expr fails */
#define ComAssertMsgBreak(expr, a, eval)          \
    if (1)  { ComAssertMsg (expr, a); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertRC that evaulates eval and breaks if vrc does not succeed */
#define ComAssertRCBreak(vrc, eval)               \
    if (1)  { ComAssertRC (vrc); if (!VBOX_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgRC that evaulates eval and breaks if vrc does not succeed */
#define ComAssertMsgRCBreak(vrc, msg, eval)       \
    if (1)  { ComAssertMsgRC (vrc, msg); if (!VBOX_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertFailed that vaulates eval and breaks */
#define ComAssertFailedBreak(eval)              \
    if (1)  { ComAssertFailed(); { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgFailed that vaulates eval and breaks */
#define ComAssertMsgFailedBreak(msg, eval)        \
    if (1)  { ComAssertMsgFailed (msg); { eval; break; } } else do {} while (0)
/** Special version of ComAssertComRC that vaulates eval and breaks if rc does not succeed */
#define ComAssertComRCBreak(rc, eval)             \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { eval; break; } } else do {} while (0)

/**
 *  Checks whether this object is ready or not. Objects are typically ready
 *  after they are successfully created by their parent objects and become
 *  not ready when the respective parent itsef becomes not ready or gets
 *  destroyed while a reference to the child is still held by the caller
 *  (which prevents it from destruction).
 *
 *  When this object is not ready, the macro sets error info and returns
 *  E_UNEXPECTED (the translatable error message is defined in null context).
 *  Otherwise, the macro does nothing.
 *
 *  This macro <b>must</b> be used at the beginning of all interface methods
 *  (right after entering the class lock) in classes derived from both
 *  VirtualBoxBase and VirtualBoxSupportErrorInfoImpl.
 */
#define CHECK_READY() \
    do { \
        if (!isReady()) \
            return setError (E_UNEXPECTED, tr ("The object is not ready")); \
    } while (0)

/**
 *  Declares an empty construtor and destructor for the given class.
 *  This is useful to prevent the compiler from generating the default
 *  ctor and dtor, which in turn allows to use forward class statements
 *  (instead of including their header files) when declaring data members of
 *  non-fundamental types with constructors (which are always called implicitly
 *  by constructors and by the destructor of the class).
 *
 *  This macro is to be palced within (the public section of) the class
 *  declaration. Its counterpart, DEFINE_EMPTY_CTOR_DTOR, must be placed
 *  somewhere in one of the translation units (usually .cpp source files).
 *
 *  @param      cls     class to declare a ctor and dtor for
 */
#define DECLARE_EMPTY_CTOR_DTOR(cls) cls(); ~cls();

/**
 *  Defines an empty construtor and destructor for the given class.
 *  See DECLARE_EMPTY_CTOR_DTOR for more info.
 */
#define DEFINE_EMPTY_CTOR_DTOR(cls) \
    cls::cls () {}; cls::~cls () {};

////////////////////////////////////////////////////////////////////////////////

namespace stdx
{
    /**
     *  A wrapper around the container that owns pointers it stores.
     *
     *  @note
     *      Ownership is recognized only when destructing the container!
     *      Pointers are not deleted when erased using erase() etc.
     *
     *  @param container
     *      class that meets Container requirements (for example, an instance of
     *      std::list<>, std::vector<> etc.). The given class must store
     *      pointers (for example, std::list <MyType *>).
     */
    template <typename container>
    class ptr_container : public container
    {
    public:
        ~ptr_container()
        {
            for (typename container::iterator it = container::begin();
                it != container::end();
                ++ it)
                delete (*it);
        }
    };
};

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE VirtualBoxBase
{

public:
    VirtualBoxBase()
    {
        mReady = false;
        RTCritSectInit(&mCritSec);
    }
    virtual ~VirtualBoxBase()
    {
        RTCritSectDelete(&mCritSec);
    }

    /**
     *  Virtual unintialization method. Called during parent object's
     *  uninitialization, if the given subclass instance is a dependent child of
     *  a class dervived from VirtualBoxBaseWithChildren (@sa
     *  VirtualBoxBaseWithChildren::addDependentChild). In this case, this
     *  method's impelemtation must call setReady (false),
     */
    virtual void uninit() {}

    // lock the object
    void lock()
    {
        RTCritSectEnter(&mCritSec);
    }
    // unlock the object
    void unlock()
    {
        RTCritSectLeave(&mCritSec);
    }

    /** Returns true when the current thread owns this object's lock. */
    bool isLockedOnCurrentThread()
    {
        return RTCritSectIsOwner (&mCritSec);
    }

    /**
     *  Helper class to make safe locking / unlocking.
     *  The constructor, given the VirtualBoxBase pointer, safely acquires the
     *  lock protecting its data. This lock will be released automatically
     *  when the instance goes out of scope (block, funciton etc.).
     *
     *  @note
     *      An instance of this class must be declared as a local variable,
     *      otherwise the optimizer will most likely destruct it right after
     *      creation (but not at the end of the block), so the lock will be
     *      released immediately.
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

        /** Internal lock handle */
        class Handle
        {
        public:
            Handle (RTCRITSECT &critSect) : lock (critSect) {}
        private:
            RTCRITSECT &lock;
            friend class AutoLock;
        };

        AutoLock() : mLock (NULL), mLevel (0), mLeftLevel (0) {}

        AutoLock (VirtualBoxBase *that)
            : mLock (that ? &that->mCritSec : NULL)
            , mLevel (0), mLeftLevel (0)
        {
            if (mLock)
            {
                ___CritSectEnter (mLock);
                ++ mLevel;
            }
        }

        AutoLock (RTCRITSECT &critSect)
            : mLock (&critSect), mLevel (0), mLeftLevel (0)
        {
            if (mLock)
            {
                ___CritSectEnter (mLock);
                ++ mLevel;
            }
        }

        AutoLock (const Handle &handle)
            : mLock (&handle.lock), mLevel (0), mLeftLevel (0)
        {
            if (mLock)
            {
                ___CritSectEnter (mLock);
                ++ mLevel;
            }
        }

        ~AutoLock()
        {
            if (mLock)
            {
                if (mLeftLevel)
                {
                    mLeftLevel -= mLevel;
                    mLevel = 0;
                    for (; mLeftLevel; -- mLeftLevel)
                        RTCritSectEnter (mLock);
                }
                AssertMsg (mLevel <= 1, ("Lock level > 1: %d\n", mLevel));
                for (; mLevel; -- mLevel)
                    RTCritSectLeave (mLock);
            }
        }

        /**
         *  Tries to acquire the lock or increases the lock level
         *  if the lock is already owned by this thread.
         */
        void lock()
        {
            if (mLock)
            {
                AssertMsgReturn (mLeftLevel == 0, ("lock() after leave()\n"), (void) 0);
                ___CritSectEnter (mLock);
                ++ mLevel;
            }
        }

        /**
         *  Decreases the lock level. If the level goes to zero, the lock
         *  is released by the current thread.
         */
        void unlock()
        {
            if (mLock)
            {
                AssertMsgReturn (mLevel > 0, ("Lock level is zero\n"), (void) 0);
                AssertMsgReturn (mLeftLevel == 0, ("lock() after leave()\n"), (void) 0);
                -- mLevel;
                RTCritSectLeave (mLock);
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
            if (mLock)
            {
                AssertMsg (mLevel > 0, ("Lock level is zero\n"));
                AssertMsgReturn (mLeftLevel == 0, ("leave() w/o enter()\n"), (void) 0);
                mLeftLevel = RTCritSectGetRecursion (mLock);
                for (uint32_t left = mLeftLevel; left; -- left)
                    RTCritSectLeave (mLock);
                Assert (mLeftLevel >= mLevel);
            }
        }

        /**
         *  Causes the current thread to acquire the lock again and restore
         *  all lock levels after calling #leave().
         */
        void enter()
        {
            if (mLock)
            {
                AssertMsg (mLevel > 0, ("Lock level is zero\n"));
                AssertMsgReturn (mLeftLevel > 0, ("enter() w/o leave()\n"), (void) 0);
                for (; mLeftLevel; -- mLeftLevel)
                    ___CritSectEnter (mLock);
            }
        }

        uint32_t level() const { return mLevel; }

        bool isNull() const { return mLock == NULL; }
        bool operator !() const { return isNull(); }

        bool belongsTo (VirtualBoxBase *that) const
        {
            return that && &that->mCritSec == mLock;
        }

    private:

        AutoLock (const AutoLock &that); // disabled
        AutoLock &operator = (const AutoLock &that); // disabled

        RTCRITSECT *mLock;
        uint32_t mLevel;
        uint32_t mLeftLevel;

        #undef ___CritSectEnter
    };

    // sets the ready state of the object
    void setReady(bool isReady)
    {
        mReady = isReady;
    }
    // get the ready state of the object
    bool isReady()
    {
        return mReady;
    }

    /**
     *  Translates the given text string according to the currently installed
     *  translation table and current context. The current context is determined
     *  by the context parameter. Additionally, a comment to the source text
     *  string text can be given. This comment (which is NULL by default)
     *  is helpful in sutuations where it is necessary to distinguish between
     *  two or more semantically different roles of the same source text in the
     *  same context.
     *
     *  @param context      the context of the the translation
     *  @param sourceText   the string to translate
     *  @param comment      the comment to the string (NULL means no comment)
     *
     *  @return
     *      the translated version of the source string in UTF-8 encoding,
     *      or the source string itself if the translation is not found
     *      in the given context.
     */
    static const char *translate (const char *context, const char *sourceText,
                                  const char *comment = 0);

private:

    // flag determining whether an object is ready
    // for usage, i.e. methods may be called
    bool mReady;
    // mutex semaphore to lock the object
    RTCRITSECT mCritSec;
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  Simple template that manages data structure allocation/deallocation
 *  and supports data pointer sharing (the instance that shares the pointer is
 *  not responsible for memory deallocation as opposed to the instance that
 *  owns it).
 */
template <class D>
class Shareable
{
public:

    Shareable() : mData (NULL), mIsShared (FALSE) {}
    ~Shareable() { free(); }

    void allocate() { attach (new D); }

    virtual void free() {
        if (mData) {
            if (!mIsShared)
                delete mData;
            mData = NULL;
            mIsShared = false;
        }
    }

    void attach (D *data) {
        AssertMsg (data, ("new data must not be NULL"));
        if (data && mData != data) {
            if (mData && !mIsShared)
                delete mData;
            mData = data;
            mIsShared = false;
        }
    }

    void attach (Shareable &data) {
        AssertMsg (
            data.mData == mData || !data.mIsShared,
            ("new data must not be shared")
        );
        if (this != &data && !data.mIsShared) {
            attach (data.mData);
            data.mIsShared = true;
        }
    }

    void share (D *data) {
        AssertMsg (data, ("new data must not be NULL"));
        if (mData != data) {
            if (mData && !mIsShared)
                delete mData;
            mData = data;
            mIsShared = true;
        }
    }

    void share (const Shareable &data) { share (data.mData); }

    void attachCopy (const D *data) {
        AssertMsg (data, ("data to copy must not be NULL"));
        if (data)
            attach (new D (*data));
    }

    void attachCopy (const Shareable &data) {
        attachCopy (data.mData);
    }

    virtual D *detach() {
        D *d = mData;
        mData = NULL;
        mIsShared = false;
        return d;
    }

    D *data() const {
        return mData;
    }

    D *operator->() const {
        AssertMsg (mData, ("data must not be NULL"));
        return mData;
    }

    bool isNull() const { return mData == NULL; }
    bool operator!() const { return isNull(); }

    bool isShared() const { return mIsShared; }

protected:

    D *mData;
    bool mIsShared;
};

/**
 *  Simple template that enhances Shareable<> and supports data
 *  backup/rollback/commit (using the copy constructor of the managed data
 *  structure).
 */
template <class D>
class Backupable : public Shareable <D>
{
public:

    Backupable() : Shareable <D> (), mBackupData (NULL) {}

    void free()
    {
        AssertMsg (this->mData || !mBackupData, ("backup must be NULL if data is NULL"));
        rollback();
        Shareable <D>::free();
    }

    D *detach()
    {
        AssertMsg (this->mData || !mBackupData, ("backup must be NULL if data is NULL"));
        rollback();
        return Shareable <D>::detach();
    }

    void share (const Backupable &data)
    {
        AssertMsg (!data.isBackedUp(), ("data to share must not be backed up"));
        if (!data.isBackedUp())
            Shareable <D>::share (data.mData);
    }

    /**
     *  Stores the current data pointer in the backup area, allocates new data
     *  using the copy constructor on current data and makes new data active.
     */
    void backup()
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        if (this->mData && !mBackupData)
        {
            mBackupData = this->mData;
            this->mData = new D (*mBackupData);
        }
    }

    /**
     *  Deletes new data created by #backup() and restores previous data pointer
     *  stored in the backup area, making it active again.
     */
    void rollback()
    {
        if (this->mData && mBackupData)
        {
            delete this->mData;
            this->mData = mBackupData;
            mBackupData = NULL;
        }
    }

    /**
     *  Commits current changes by deleting backed up data and clearing up the
     *  backup area. The new data pointer created by #backup() remains active
     *  and becomes the only managed pointer.
     *
     *  This method is much faster than #commitCopy() (just a single pointer
     *  assignment operation), but makes the previous data pointer invalid
     *  (because it is freed). For this reason, this method must not be
     *  used if it's possible that data managed by this instance is shared with
     *  some other Shareable instance. See #commitCopy().
     */
    void commit()
    {
        if (this->mData && mBackupData)
        {
            if (!this->mIsShared)
                delete mBackupData;
            mBackupData = NULL;
            this->mIsShared = false;
        }
    }

    /**
     *  Commits current changes by assigning new data to the previous data
     *  pointer stored in the backup area using the assignment operator.
     *  New data is deleted, the backup area is cleared and the previous data
     *  pointer becomes active and the only managed pointer.
     *
     *  This method is slower than #commit(), but it keeps the previous data
     *  pointer valid (i.e. new data is copied to the same memory location).
     *  For that reason it's safe to use this method on instances that share
     *  managed data with other Shareable instances.
     */
    void commitCopy()
    {
        if (this->mData && mBackupData)
        {
            *mBackupData = *(this->mData);
            delete this->mData;
            this->mData = mBackupData;
            mBackupData = NULL;
        }
    }

    void assignCopy (const D *data)
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        AssertMsg (data, ("data to copy must not be NULL"));
        if (this->mData && data)
        {
            if (!mBackupData)
            {
                mBackupData = this->mData;
                this->mData = new D (*data);
            }
            else
                *this->mData = *data;
        }
    }

    void assignCopy (const Backupable &data)
    {
        assignCopy (data.mData);
    }

    bool isBackedUp() const
    {
        return mBackupData != NULL;
    }

    bool hasActualChanges() const
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        return this->mData != NULL && mBackupData != NULL &&
               !(*this->mData == *mBackupData);
    }

    D *backedUpData() const
    {
        return mBackupData;
    }

protected:

    D *mBackupData;
};

#endif // ____H_VIRTUALBOXBASEIMPL
