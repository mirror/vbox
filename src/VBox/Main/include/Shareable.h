/** @file
 *
 * Data structure management templates (Shareable and friends)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/// @todo (dmik) This header is not yet used.
//  Templates defined here are to replace Shareable and Backupable
//  defined in VirtualBoxBase.h

#ifndef ____H_SHAREABLE
#define ____H_SHAREABLE

#include <iprt/asm.h>
#include <iprt/assert.h>

namespace util
{

namespace internal
{

class Dummy
{
};

class Shareable_base
{
protected:

    class Data
    {
    public:

        Data() : mRefCnt (0) {}
        virtual ~Data() {}

        uint32_t addRef() { return ASMAtomicIncU32 (&mRefCnt); }
        uint32_t release()
        {
            uint32_t refCnt = ASMAtomicDecU32 (&mRefCnt);
            if (refCnt == 0)
                delete this;
            return refCnt;
        }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (Data)

        uint32_t volatile mRefCnt;
    };
};

} /* namespace internal */

/**
 *  Template class to manage allocation/deallocation of data structures on the
 *  heap with the ability to share allocated data among several instances.
 *
 *  Data sharing is implemented using the concept of reference counting.
 *  When an instance allocates the managed data structure from scratch, it sets
 *  the reference counter to one. When it starts sharing the existing data
 *  structure, it simply increments the reference counter by one. When it stops
 *  sharing data, the reference counter is decremented. Once this counter drops
 *  to zero, the managed data structure is automatically deallocated (deleted).
 *
 *  Data managed by instances of this class can be either |NULL| (not allocated
 *  and not shared) or non-|NULL| (either allocated using #create() or shared
 *  with another instance using one of assignment operators or methods), as
 *  indicated by the #isNull() method and by the data pointer returned by
 *  the #data() method. Note that the #operator->() method will fail (hit an
 *  assertion) if the managed data pointer is |NULL|.
 *
 *  The managed data structure (passed as the first argument to the template)
 *  must meet the following requirements:
 *  <ul>
 *      <li>Must have a public default constructor (with no arguments)
 *      <li>Must define a public copy constructor and assignment operation
 *  </ul>
 *
 *  This template class is NOT thread-safe. If you need thread safefy, you can
 *  specify util::Lockable as the second argument to the template. In this
 *  case, you can explicitly lock instances of the template (using the
 *  AutoWriteLock and AutoReadLock classes) before accessing any of its
 *  members, as follows:
 *  <code>
 *      struct Data { ... };
 *      Shareable <Data, util::Lockable> mData;
 *      ...
 *      {
 *          // acquire the lock until the end of the block
 *          AutoWriteLock alock (mData);
 *          // share with another instance (thatData defined somewhere else)
 *          {
 *              AutoReadLock thatLock (thatData);
 *              mData = thatData;
 *          }
 *          // access managed data (through #operator->())
 *          mData->mSomeVield = someValue;
 *      }
 *  </code>
 *
 *  Making this class thread-safe usually assumes that the managed data
 *  structure must be also thread-safe and must share its own lock object with
 *  all other Shareable instances referring it (so locking it through one
 *  Shareable instance will prevent another one sharing the same data from
 *  accessing it). This can be done in a similar way by deriving the data
 *  structure to manage from util::Lockable and using the #data() method to
 *  lock it before accessing:
 *  <code>
 *      struct Data : public util::Lockable { ... };
 *      Shareable <Data, util::Lockable> mData;
 *      ...
 *      {
 *          // read-only data access
 *          AutoReadLock lock (mData); // protect Shareable members (read-only)
 *          AutoReadLock dataLock (mData.data()); // protect Data members (read-only)
 *          if (mData->mSomeVield) ...
 *      }
 *      ...
 *      {
 *          // data modification
 *          AutoReadLock lock (mData); // protect Shareable members (still read-only)
 *          AutoWriteLock dataLock (mData.data()); // protect Data members (exclusive)
 *          mData->mSomeVield = someValue;
 *      }
 *  </code>
 *
 *  Please note that if you want to access managed data through #data() or
 *  #operator->(), you have to enter both locks! If you only need to access
 *  Shareable members themselves (i.e. to assign one Shareable to another or to
 *  call #setNull()) it's enough to enter the Shareable lock only (as it's shown
 *  at the beginning of the first example).
 *
 *  @param D        data structure to manage
 *  @param Extra    extra class the template instantiation will be publicly
 *                  derived from (by default, a dummy empty class)
 */
template <class D, class Extra = internal::Dummy>
class Shareable : public internal::Shareable_base, public Extra
{
public:

    /** Creates a new instance with managed data set to |NULL|. */
    Shareable() : mData (NULL) {}

    /** Calls #setNull() and deletes this instance. */
    virtual ~Shareable() { setNull(); };

    /**
     *  Allocates the managed data structure on the heap using the default
     *  constructor. The current data, if not |NULL|, is dereferenced (see
     *  #setNull()).
     */
    void create()
    {
        Data data = new Data();
        AssertReturn (data != NULL, (void) 0);
        setData (data);
    }

    /**
     *  Allocates a copy of the managed data structure represented by the given
     *  instance using the copy constructor. The current data, if not |NULL|,
     *  is dereferenced (see #setNull()).
     *
     *  @note The newly allocated data is not shared with the given instance
     *  (they are fully independent of each other).
     */
    void createCopy (const Shareable &that)
    {
        Data data = NULL;
        if (that.mData)
        {
            data = new Data (*that.mData);
            AssertReturn (data != NULL, (void) 0);
        }
        setData (data);
    }

    /**
     *  Starts sharing the managed data structure represented by the given
     *  instance. The data reference counter is incremented by one.
     */
    Shareable &operator= (const Shareable &that)
    {
        setData (that->mData);
        return *this;
    }

    /**
     *  Dereferences the managed data (decrements the reference counter) to
     *  effectively stop sharing and sets the managed data pointer to |NULL|.
     *  If this instance is the last (the only) one that to refers non-NULL data,
     *  this data will be deallocated (deleted). Does nothing if data is |NULL|.
     */
    virtual void setNull() { setData (NULL); }

    /**
     *  Returns |true| if the managed data pointer is |NULL| (not allocated and
     *  not shared).
     */
    bool isNull() const { return mData == NULL; }
    /**
     *  Converts this instance to |bool| (returns |true| when #isNull() is
     *  |false|)
     */
    operator bool() const { return !isNull(); }

    /**
     *  Returns a pointer to the managed data structure (|NULL| when #isNull()
     *  returns |true|).
     */
    D *data() { return mData; }
    /**
     *  Returns a pointer to the managed data structure (|NULL| when #isNull()
     *  returns |true|).
     */
    const D *data() const { return mData; }

    /**
     *  A convenience shortcut to #data() (implements direct pointer semantics).
     *  @note This operator will fail if the managed data pointer is |NULL|.
     */
    D *operator->()
    {
        AssertMsg (mData != NULL, ("Managed data is NULL\n"));
        return mData;
    }
    /**
     *  A convenience shortcut to #data() (implements direct pointer semantics).
     *  @note This operator will fail if the managed data pointer is |NULL|.
     */
    const D *operator->() const
    {
        AssertMsg (mData != NULL, ("Managed data is NULL\n"));
        return mData;
    }

protected:

    typedef internal::Shareable_base::Data BD;
    class Data : public D, public BD
    {
    public:
        Data() : D(), BD() {}
        Data (const Data &that) : D (&that), BD() {}
    private:
        Data &operator= (const Data &that);
    };

    void setData (Data *aData)
    {
        // beware of self assignment
        if (aData)
            aData->addRef();
        if (mData)
            mData->release();
        mData = aData;
    }

private:

    Data *mData;
};

WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP_TPL (Shareable, <class D>, <D>)

/**
 *  Template class that adds support for data backup to the Shareable template.
 *
 *  All thread safety remarks mentioned in the descrition of the Shareable
 *  template are appliable to this class as well. In particular, all new methods
 *  of this template are not implicitly thread-safe, so if you add thread
 *  safety using the util::Lockable class, don't forget to lock the
 *  Backupable instance before doing #backup(), #commit() or #rollback().
 *
 *  The managed data structure (passed as the first argument to the template)
 *  besides requirements mentioned in Shareable, must also provide a comparison
 *  operation (|bool operator== (const D &that) const|).
 *
 *  @param D        data structure to manage
 *  @param Extra    extra class the template instantiation will be publicly
 *                  derived from (by default, a dummy empty class)
 */
template <class D, class Extra = internal::Dummy>
class Backupable : public Shareable <D, Extra>
{
public:

    /**
     *  Creates a new instance with both managed data and its backup copy
     *  set to |NULL|.
     */
    Backupable() : mBackupData (NULL) {}

    /**
     *  Calls #rollback() and then calls Shareable::setNull().
     */
    virtual void setNull()
    {
        rollback();
        Shareable::setNull();
    }

    /**
     *  Stores the current data pointer in the backup area and allocates a new
     *  data using the copy constructor. The new data becomes the active one
     *  (returned by #data() and #operator->()).
     *  The method does nothing if the managed data is already backed up.
     *  @note The managed data pointer must not be |NULL|, otherwise this method
     *  will fail.
     */
    void backup()
    {
        if (!mBackupData)
        {
            AssertMsgReturn (this->mData, ("Managed data must not be NULL\n"),
                             (void) 0);
            Data data = new Data (*this->mData);
            AssertReturn (data != NULL, (void) 0);
            mBackupData = this->mData;
            mBackupData->addRef();
            setData (data);
        }
    }

    /**
     *  Dereferences the new data allocated by #backup() and restores the
     *  previous data pointer from the backup area, making it active again.
     *  If this instance is the last (the only) one that refers to the new data,
     *  this data will be deallocated (deleted).
     *  @note The managed data must be backed up, otherwise this method will fail.
     */
    void rollback()
    {
        AssertMsgReturn (this->mData, ("Managed data must not be NULL\n"),
                         (void) 0);
        AssertMsgReturn (mBackupData, ("Backed up data must not be NULL\n"),
                         (void) 0);
        setData (mBackupData);
        mBackupData->release();
        mBackupData = NULL;
    }

    /**
     *  Dereferences the data backed up by #backup() (see #setNull()) keeping
     *  the new data active.
     *  If this instance is the last (the only) one that refers to the backed up
     *  data, this data will be deallocated (deleted).
     *  @note The managed data must be backed up, otherwise this method will fail.
     */
    void commit()
    {
        AssertMsgReturn (this->mData, ("Managed data must not be NULL\n"),
                         (void) 0);
        AssertMsgReturn (mBackupData, ("Backed up data must not be NULL\n"),
                         (void) 0);
        mBackupData->release();
        mBackupData = NULL;
    }

    /** Returns |true| if the managed data is currently backed up. */
    bool isBackedUp() const { return mBackupData != NULL; }

    /**
     *  Returns |true| if the managed data is currently backed up and the backed
     *  up copy differs from the current data. The comparison is done using
     *  the comparison operator provided by the managed data structure.
     *  @note |false| is returned if the data is not currently backed up.
     */
    bool hasActualChanges() const
    {
        AssertMsgReturn (this->mData, ("Managed data must not be NULL"), false);
        return mBackupData != NULL &&
               !(this->mData->D::operator== (*mBackupData));
    }

    /**
     *  Returns a pointer to the backed up copy of the managed data structure
     *  (|NULL| when #isBackedUp() returns |false|).
     */
    const D *backupData() const { return mBackupData; }

protected:

    typedef Shareable::Data Data;

private:

    Data *mBackupData;
};

} /* namespace util */

#endif // ____H_SHAREABLE

