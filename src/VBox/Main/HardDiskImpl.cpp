/** @file
 *
 * VirtualBox COM class implementation
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

#include "HardDiskImpl.h"
#include "ProgressImpl.h"
#include "VirtualBoxImpl.h"
#include "SystemPropertiesImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/cpputils.h>
#include <VBox/VBoxHDD.h>
#include <VBox/err.h>

#include <algorithm>

#define CHECK_BUSY() \
    do { \
        if (isBusy()) \
            return setError (E_UNEXPECTED, \
                tr ("Hard disk '%ls' is being used by another task"), \
                toString().raw()); \
    } while (0)

#define CHECK_BUSY_AND_READERS() \
do { \
    if (readers() > 0 || isBusy()) \
        return setError (E_UNEXPECTED, \
            tr ("Hard disk '%ls' is being used by another task"), \
            toString().raw()); \
} while (0)

/** Task structure for asynchronous VDI operations */
struct VDITask
{
    enum Op { CreateDynamic, CreateStatic, CloneToImage };

    VDITask (Op op, HVirtualDiskImage *i, Progress *p)
        : operation (op)
        , vdi (i)
        , progress (p)
        {}

    Op operation;
    ComObjPtr <HVirtualDiskImage> vdi;
    ComObjPtr <Progress> progress;

    /* for CreateDynamic, CreateStatic */
    uint64_t size;

    /* for CloneToImage */
    ComObjPtr <HardDisk> source;
};

/**
 *  Progress callback handler for VDI operations.
 *
 *  @param uPercent    Completetion precentage (0-100).
 *  @param pvUser      Pointer to the Progress instance.
 */
static DECLCALLBACK(int) progressCallback (PVM /* pVM */, unsigned uPercent, void *pvUser)
{
    Progress *progress = static_cast <Progress *> (pvUser);

    /* update the progress object, capping it at 99% as the final percent
     * is used for additional operations like setting the UUIDs and similar. */
    if (progress)
        progress->notifyProgress (RT_MIN (uPercent, 99));

    return VINF_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// HardDisk class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

/** Shold be called by subclasses from #FinalConstruct() */
HRESULT HardDisk::FinalConstruct()
{
    mRegistered = FALSE;

    mStorageType = HardDiskStorageType_VirtualDiskImage;
    mType = HardDiskType_NormalHardDisk;

    mBusy = false;
    mReaders = 0;

    return S_OK;
}

/**
 *  Shold be called by subclasses from #FinalRelease().
 *  Uninitializes this object by calling #uninit() if it's not yet done.
 */
void HardDisk::FinalRelease()
{
    uninit();
}

// protected initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the hard disk object.
 *
 *  Subclasses should call this or any other #init() method from their
 *  init() implementations.
 *
 *  @note
 *      This method doesn't do |isReady()| check and doesn't call
 *      |setReady (true)| on success!
 *  @note
 *      This method must be called from under the object's lock!
 */
HRESULT HardDisk::protectedInit (VirtualBox *aVirtualBox, HardDisk *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aVirtualBox, E_INVALIDARG);

    mVirtualBox = aVirtualBox;
    mParent = aParent;

    if (!aParent)
        aVirtualBox->addDependentChild (this);
    else
        aParent->addDependentChild (this);

    return S_OK;
}

/**
 *  Uninitializes the instance.
 *  Subclasses should call this from their uninit() implementations.
 *  The readiness flag must be true on input and will be set to false
 *  on output.
 *
 *  @param alock this object's autolock
 *
 *  @note
 *  Using mParent and mVirtualBox members after this method returns
 *  is forbidden.
 */
void HardDisk::protectedUninit (AutoLock &alock)
{
    LogFlowThisFunc (("\n"));

    Assert (alock.belongsTo (this));
    Assert (isReady());

    /* uninit all children */
    uninitDependentChildren();

    setReady (false);

    if (mParent)
        mParent->removeDependentChild (this);
    else
    {
        alock.leave();
        mVirtualBox->removeDependentChild (this);
        alock.enter();
    }

    mParent.setNull();
    mVirtualBox.setNull();
}

// IHardDisk properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDisk::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mId.cloneTo (aId);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(StorageType) (HardDiskStorageType_T *aStorageType)
{
    if (!aStorageType)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aStorageType = mStorageType;
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Location) (BSTR *aLocation)
{
    if (!aLocation)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    toString (false /* aShort */).cloneTo (aLocation);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Type) (HardDiskType_T *aType)
{
    if (!aType)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aType = mType;
    return S_OK;
}

STDMETHODIMP HardDisk::COMSETTER(Type) (HardDiskType_T aType)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mRegistered)
        return setError (E_FAIL,
            tr ("You cannot change the type of the registered hard disk '%ls'"),
            toString().raw());

    /* return silently if nothing to do */
    if (mType == aType)
        return S_OK;

    if (mStorageType == HardDiskStorageType_VMDKImage)
        return setError (E_FAIL,
            tr ("Currently, changing the type of VMDK hard disks is not allowed"));

    if (mStorageType == HardDiskStorageType_ISCSIHardDisk)
        return setError (E_FAIL,
            tr ("Currently, changing the type of iSCSI hard disks is not allowed"));

    /// @todo (dmik) later: allow to change the type on any registered hard disk
    //  depending on whether it is attached or not, has children etc.
    //  Don't forget to save hdd configuration afterwards.

    mType = aType;
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Parent) (IHardDisk **aParent)
{
    if (!aParent)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mParent.queryInterfaceTo (aParent);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Children) (IHardDiskCollection **aChildren)
{
    if (!aChildren)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    AutoLock chLock (childrenLock());

    ComObjPtr <HardDiskCollection> collection;
    collection.createObject();
    collection->init (children());
    collection.queryInterfaceTo (aChildren);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Root) (IHardDisk **aRoot)
{
    if (!aRoot)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    root().queryInterfaceTo (aRoot);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Accessible) (BOOL *aAccessible)
{
    if (!aAccessible)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    HRESULT rc = getAccessible (mLastAccessError);
    if (FAILED (rc))
        return rc;

    *aAccessible = mLastAccessError.isNull();
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(AllAccessible) (BOOL *aAllAccessible)
{
    if (!aAllAccessible)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    if (mParent)
    {
        HRESULT rc = S_OK;

        /* check the accessibility state of all ancestors */
        ComObjPtr <HardDisk> parent = (HardDisk *) mParent;
        while (parent)
        {
            AutoLock parentLock (parent);
            HRESULT rc = parent->getAccessible (mLastAccessError);
            if (FAILED (rc))
                break;
            *aAllAccessible = mLastAccessError.isNull();
            if (!*aAllAccessible)
                break;
            parent = parent->mParent;
        }

        return rc;
    }

    HRESULT rc = getAccessible (mLastAccessError);
    if (FAILED (rc))
        return rc;

    *aAllAccessible = mLastAccessError.isNull();
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(LastAccessError) (BSTR *aLastAccessError)
{
    if (!aLastAccessError)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mLastAccessError.cloneTo (aLastAccessError);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(MachineId) (GUIDPARAMOUT aMachineId)
{
    if (!aMachineId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mMachineId.cloneTo (aMachineId);
    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(SnapshotId) (GUIDPARAMOUT aSnapshotId)
{
    if (!aSnapshotId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mSnapshotId.cloneTo (aSnapshotId);
    return S_OK;
}

STDMETHODIMP HardDisk::CloneToImage (INPTR BSTR aFilePath,
                                     IVirtualDiskImage **aImage,
                                     IProgress **aProgress)
{
    if (!aFilePath || !(*aFilePath))
        return E_INVALIDARG;
    if (!aImage || !aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();
    CHECK_BUSY();

    if (!mParent.isNull())
        return setError (E_FAIL,
            tr ("Cloning differencing VDI images is not yet supported ('%ls')"),
            toString().raw());

    HRESULT rc = S_OK;

    /* create a project object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    rc = progress->init (mVirtualBox, (IVirtualDiskImage *) this,
                         Bstr (tr ("Creating a hard disk clone")),
                         FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);

    /* create an imageless resulting object */
    ComObjPtr <HVirtualDiskImage> image;
    image.createObject();
    rc = image->init (mVirtualBox, NULL, NULL);
    CheckComRCReturnRC (rc);

    /* append the default path if only a name is given */
    Bstr path = aFilePath;
    {
        Utf8Str fp = aFilePath;
        if (!RTPathHavePath (fp))
        {
            AutoReaderLock propsLock (mVirtualBox->systemProperties());
            path = Utf8StrFmt ("%ls%c%s",
                mVirtualBox->systemProperties()->defaultVDIFolder().raw(),
                RTPATH_DELIMITER,
                fp.raw());
        }
    }

    /* set the desired path */
    rc = image->setFilePath (path);
    CheckComRCReturnRC (rc);

    /* ensure the directory exists */
    {
        Utf8Str imageDir = image->filePath();
        RTPathStripFilename (imageDir.mutableRaw());
        if (!RTDirExists (imageDir))
        {
            int vrc = RTDirCreateFullPath (imageDir, 0777);
            if (VBOX_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to store the image file (%Vrc)"),
                    imageDir.raw(), vrc);
            }
        }
    }

    /* mark as busy (being created)
     * (VDI task thread will unmark it) */
    image->setBusy();

    /* fill in a VDI task data */
    VDITask *task = new VDITask (VDITask::CloneToImage, image, progress);
    task->source = this;

    /* increase readers until finished
     * (VDI task thread will decrease them) */
    addReader();

    /* create the hard disk creation thread, pass operation data */
    int vrc = RTThreadCreate (NULL, HVirtualDiskImage::VDITaskThread,
                              (void *) task, 0, RTTHREADTYPE_MAIN_HEAVY_WORKER,
                              0, "VDITask");
    ComAssertMsgRC (vrc, ("Could not create a thread (%Vrc)", vrc));
    if (VBOX_FAILURE (vrc))
    {
        releaseReader();
        image->clearBusy();
        delete task;
        return E_FAIL;
    }

    /* return interfaces to the caller */
    image.queryInterfaceTo (aImage);
    progress.queryInterfaceTo (aProgress);

    return S_OK;
}

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Returns the very first (grand-) parent of this hard disk or the hard
 *  disk itself, if it doesn't have a parent.
 *
 *  @note
 *      Must be called from under the object's lock
 */
ComObjPtr <HardDisk> HardDisk::root() const
{
    ComObjPtr <HardDisk> root = const_cast <HardDisk *> (this);
    ComObjPtr <HardDisk> parent;
    while ((parent = root->parent()))
        root = parent;

    return root;
}

/**
 *  Attempts to mark the hard disk as registered.
 *  Must be always called by every reimplementation.
 *  Only VirtualBox can call this method.
 *
 *  @param aRegistered  true to set registered and false to set unregistered
 */
HRESULT HardDisk::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        ComAssertRet (mMachineId.isEmpty(), E_FAIL);
        ComAssertRet (mId && children().size() == 0, E_FAIL);

        if (mRegistered)
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is already registered"),
                toString().raw());

        CHECK_BUSY();
    }
    else
    {
        if (!mRegistered)
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is already unregistered"),
                toString().raw());

        if (!mMachineId.isEmpty())
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is attached to a virtual machine with UUID {%s}"),
                toString().raw(), mMachineId.toString().raw());

        if (children().size() > 0)
            return setError (E_FAIL,
                tr ("Hard disk '%ls' has %d differencing hard disks based on it"),
                toString().raw(), children().size());

        CHECK_BUSY_AND_READERS();
    }

    mRegistered = aRegistered;
    return S_OK;
}

/**
 *  Checks basic accessibility of this hard disk only (w/o parents).
 *  Must be always called by every HardDisk::getAccessible() reimplementation
 *  in the first place.
 *
 *  When @a aCheckBusy is true, this method checks that mBusy = false (and
 *  returns an appropriate error if not). This lets reimplementations
 *  successfully call addReader() after getBaseAccessible() succeeds to
 *  reference the disk and protect it from being modified or deleted before
 *  the remaining check steps are done. Note that in this case, the
 *  reimplementation must enter the object lock before calling this method and
 *  must not leave it before calling addReader() to avoid race condition.
 *
 *  When @a aCheckReaders is true, this method checks that mReaders = 0 (and
 *  returns an appropriate error if not). When set to true together with
 *  @a aCheckBusy, this lets reimplementations successfully call setBusy() after
 *  getBaseAccessible() succeeds to lock the disk and make sure nobody is
 *  referencing it until the remaining check steps are done. Note that in this
 *  case, the reimplementation must enter the object lock before calling this
 *  method and must not leave it before calling setBusy() to avoid race
 *  condition.
 *
 *  @param aAccessError     On output, a null string indicates the hard disk is
 *                          accessible, otherwise contains a message describing
 *                          the reason of inaccessibility.
 *  @param aCheckBusy       Whether to do the busy check or not.
 *  @param aCheckReaders    Whether to do readers check or not.
 */
HRESULT HardDisk::getBaseAccessible (Bstr &aAccessError,
                                     bool aCheckBusy /* = false */,
                                     bool aCheckReaders /* = false */)
{
    AutoLock alock (this);
    CHECK_READY();

    aAccessError.setNull();

    if (aCheckBusy)
    {
        if (mBusy)
        {
            aAccessError = Utf8StrFmt (
                tr ("Hard disk '%ls' is being exclusively used by another task"),
                toString().raw());
            return S_OK;
        }
    }

    if (aCheckReaders)
    {
        if (mReaders > 0)
        {
            aAccessError = Utf8StrFmt (
                tr ("Hard disk '%ls' is being used by another task (%d readers)"),
                toString().raw(), mReaders);
            return S_OK;
        }
    }

    return S_OK;
}

/**
 *  Returns true if the set of properties that makes this object unique
 *  is equal to the same set of properties in the given object.
 */
bool HardDisk::sameAs (HardDisk *that)
{
    AutoLock alock (this);
    if (!isReady())
        return false;

    /// @todo (dmik) besides UUID, we temporarily use toString() to uniquely
    //  identify objects. This is ok for VDIs but may be not good for iSCSI,
    //  so it will need a reimp of this method.

    return that->mId == mId ||
           toString (false /* aShort */) == that->toString (false /* aShort */);
}

/**
 *  Marks this hard disk as busy.
 *  A busy hard disk cannot have readers and its properties (UUID, description)
 *  cannot be externally modified.
 */
void HardDisk::setBusy()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    AssertMsgReturnVoid (mBusy == false, ("%ls", toString().raw()));
    AssertMsgReturnVoid (mReaders == 0, ("%ls", toString().raw()));

    mBusy = true;
}

/**
 *  Clears the busy flag previously set by #setBusy().
 */
void HardDisk::clearBusy()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    AssertMsgReturnVoid (mBusy == true, ("%ls", toString().raw()));

    mBusy = false;
}

/**
 *  Increases the number of readers of this hard disk.
 *  A hard disk that have readers cannot be marked as busy (and vice versa)
 *  and its properties (UUID, description) cannot be externally modified.
 */
void HardDisk::addReader()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    AssertMsgReturnVoid (mBusy == false, ("%ls", toString().raw()));

    ++ mReaders;
}

/**
 *  Decreases the number of readers of this hard disk.
 */
void HardDisk::releaseReader()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    AssertMsgReturnVoid (mBusy == false, ("%ls", toString().raw()));
    AssertMsgReturnVoid (mReaders > 0, ("%ls", toString().raw()));

    -- mReaders;
}

/**
 *  Increases the number of readers on all ancestors of this hard disk.
 */
void HardDisk::addReaderOnAncestors()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    if (mParent)
    {
        AutoLock alock (mParent);
        mParent->addReader();
        mParent->addReaderOnAncestors();
    }
}

/**
 *  Decreases the number of readers on all ancestors of this hard disk.
 */
void HardDisk::releaseReaderOnAncestors()
{
    AutoLock alock (this);
    AssertReturnVoid (isReady());

    if (mParent)
    {
        AutoLock alock (mParent);
        mParent->releaseReaderOnAncestors();
        mParent->releaseReader();
    }
}

/**
 *  Returns true if this hard disk has children not belonging to the same
 *  machine.
 */
bool HardDisk::hasForeignChildren()
{
    AutoLock alock (this);
    AssertReturn (isReady(), false);

    AssertReturn (!mMachineId.isEmpty(), false);

    /* check all children */
    AutoLock chLock (childrenLock());
    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        ComObjPtr <HardDisk> child = *it;
        AutoLock childLock (child);
        if (child->mMachineId != mMachineId)
            return true;
    }

    return false;
}

/**
 *   Marks this hard disk and all its children as busy.
 *   Used for merge operations.
 *   Returns a meaningful error info on failure.
 */
HRESULT HardDisk::setBusyWithChildren()
{
    AutoLock alock (this);
    AssertReturn (isReady(), E_FAIL);

    const char *errMsg = tr ("Hard disk '%ls' is being used by another task");

    if (mReaders > 0 || mBusy)
        return setError (E_FAIL, errMsg, toString().raw());

    AutoLock chLock (childrenLock());

    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        ComObjPtr <HardDisk> child = *it;
        AutoLock childLock (child);
        if (child->mReaders > 0 || child->mBusy)
        {
            /* reset the busy flag of all previous children */
            while (it != children().begin())
                (*(-- it))->clearBusy();
            return setError (E_FAIL, errMsg, child->toString().raw());
        }
        else
            child->mBusy = true;
    }

    mBusy = true;

    return S_OK;
}

/**
 *   Clears the busy flag of this hard disk and all its children.
 *   An opposite to #setBusyWithChildren.
 */
void HardDisk::clearBusyWithChildren()
{
    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    AssertReturn (mBusy == true, (void) 0);

    AutoLock chLock (childrenLock());

    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        ComObjPtr <HardDisk> child = *it;
        AutoLock childLock (child);
        Assert (child->mBusy == true);
        child->mBusy = false;
    }

    mBusy = false;
}

/**
 *  Checks that this hard disk and all its direct children are accessible.
 */
HRESULT HardDisk::getAccessibleWithChildren (Bstr &aAccessError)
{
    AutoLock alock (this);
    AssertReturn (isReady(), E_FAIL);

    HRESULT rc = getAccessible (aAccessError);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    AutoLock chLock (childrenLock());

    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        ComObjPtr <HardDisk> child = *it;
        rc = child->getAccessible (aAccessError);
        if (FAILED (rc) || !aAccessError.isNull())
            return rc;
    }

    return rc;
}

/**
 *  Checks that this hard disk and all its descendants are consistent.
 *  For now, the consistency means that:
 *
 *  1) every differencing image is associated with a registered machine
 *  2) every root image that has differencing children is associated with
 *     a registered machine.
 *
 *  This method is used by the VirtualBox constructor after loading all hard
 *  disks and all machines.
 */
HRESULT HardDisk::checkConsistency()
{
    AutoLock alock (this);
    AssertReturn (isReady(), E_FAIL);

    if (isDifferencing())
    {
        Assert (mVirtualBox->isMachineIdValid (mMachineId) ||
                mMachineId.isEmpty());

        if (mMachineId.isEmpty())
            return setError (E_FAIL,
                tr ("Differencing hard disk '%ls' is not associated with "
                    "any registered virtual machine or snapshot"),
                toString().raw());
    }

    HRESULT rc = S_OK;

    AutoLock chLock (childrenLock());

    if (mParent.isNull() && mType == HardDiskType_NormalHardDisk &&
        children().size() != 0)
    {
        if (mMachineId.isEmpty())
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is not associated with any registered "
                    "virtual machine or snapshot, but has differencing child "
                    "hard disks based on it"),
                toString().raw());
    }

    for (HardDiskList::const_iterator it = children().begin();
         it != children().end() && SUCCEEDED (rc);
         ++ it)
    {
        rc = (*it)->checkConsistency();
    }

    return rc;
}

/**
 *  Creates a differencing hard disk for this hard disk and returns the
 *  created hard disk object to the caller.
 *
 *  The created differencing hard disk is automatically added to the list of
 *  children of this hard disk object and registered within VirtualBox.

 *  The specified progress object (if not NULL) receives the percentage
 *  of the operation completion. However, it is responsibility of the caller to
 *  call Progress::notifyComplete() after this method returns.
 *
 *  @param aFolder      folder where to create the differencing disk
 *                      (must be a full path)
 *  @param aMachineId   machine ID the new hard disk will belong to
 *  @param aHardDisk    resulting hard disk object
 *  @param aProgress    progress object to run during copy operation
 *                      (may be NULL)
 *
 *  @note
 *      Must be NOT called from under locks of other objects that need external
 *      access dirung this method execurion!
 */
HRESULT HardDisk::createDiffHardDisk (const Bstr &aFolder, const Guid &aMachineId,
                                      ComObjPtr <HVirtualDiskImage> &aHardDisk,
                                      Progress *aProgress)
{
    AssertReturn (!aFolder.isEmpty() && !aMachineId.isEmpty(),
                  E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    ComAssertRet (isBusy() == false, E_FAIL);

    Guid id;
    id.create();

    Utf8Str filePathTo = Utf8StrFmt ("%ls%c{%Vuuid}.vdi",
                                     aFolder.raw(), RTPATH_DELIMITER, id.ptr());

    /* try to make the path relative to the vbox home dir */
    const char *filePathToRel = filePathTo;
    {
        const Utf8Str &homeDir = mVirtualBox->homeDir();
        if (!strncmp (filePathTo, homeDir, homeDir.length()))
            filePathToRel = (filePathToRel + homeDir.length() + 1);
    }

    /* first ensure the directory exists */
    {
        Utf8Str dir = aFolder;
        if (!RTDirExists (dir))
        {
            int vrc = RTDirCreateFullPath (dir, 0777);
            if (VBOX_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to store the image file (%Vrc)"),
                    dir.raw(), vrc);
            }
        }
    }

    alock.leave();

    /* call storage type specific diff creation method */
    HRESULT rc = createDiffImage (id, filePathTo, aProgress);

    alock.enter();

    CheckComRCReturnRC (rc);

    ComObjPtr <HVirtualDiskImage> vdi;
    vdi.createObject();
    rc = vdi->init (mVirtualBox, this, Bstr (filePathToRel),
                    TRUE /* aRegistered  */);
    CheckComRCReturnRC (rc);

    /* associate the created hard disk with the given machine */
    vdi->setMachineId (aMachineId);

    rc = mVirtualBox->registerHardDisk (vdi, VirtualBox::RHD_Internal);
    CheckComRCReturnRC (rc);

    aHardDisk = vdi;

    return S_OK;
}

/**
 *  Checks if the given change of \a aOldPath to \a aNewPath affects the path
 *  of this hard disk or any of its children and updates it if necessary (by
 *  calling #updatePath()). Intended to be called only by
 *  VirtualBox::updateSettings() if a machine's name change causes directory
 *  renaming that affects this image.
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object and all children for writing.
 */
void HardDisk::updatePaths (const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoLock alock (this);
    AssertReturnVoid (isReady());

    updatePath (aOldPath, aNewPath);

    /* update paths of all children */
    AutoLock chLock (childrenLock());
    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        (*it)->updatePaths (aOldPath, aNewPath);
    }
}

/**
 *  Helper method that deduces a hard disk object type to create from
 *  the location string format and from the contents of the resource
 *  pointed to by the location string.
 *
 *  Currently, the location string must be a file path which is
 *  passed to the HVirtualDiskImage or HVMDKImage initializer in
 *  attempt to create a hard disk object.
 *
 *  @param  aVirtualBox
 *  @param  aLocation
 *  @param  hardDisk
 *
 *  @return
 */
/* static */
HRESULT HardDisk::openHardDisk (VirtualBox *aVirtualBox, INPTR BSTR aLocation,
                                ComObjPtr <HardDisk> &hardDisk)
{
    LogFlowFunc (("aLocation=\"%ls\"\n", aLocation));

    AssertReturn (aVirtualBox, E_POINTER);

    /* null and empty strings are not allowed locations */
    AssertReturn (aLocation, E_INVALIDARG);
    AssertReturn (*aLocation, E_INVALIDARG);

    static const struct
    {
        HardDiskStorageType_T type;
        const char *ext;
    }
    storageTypes[] =
    {
        /* try the plugin format first if there is no extension match */
        { HardDiskStorageType_CustomHardDisk, NULL },
        /* then try the rest */
        { HardDiskStorageType_VMDKImage, ".vmdk" },
        { HardDiskStorageType_VirtualDiskImage, ".vdi" },
        { HardDiskStorageType_VHDImage, ".vhd" },
    };

    /* try to guess the probe order by extension */
    size_t first = 0;
    bool haveFirst = false;
    Utf8Str loc = aLocation;
    char *ext = RTPathExt (loc);

    for (size_t i = 0; i < ELEMENTS (storageTypes); ++ i)
    {
        if (storageTypes [i].ext &&
            RTPathCompare (ext, storageTypes [i].ext) == 0)
        {
            first = i;
            haveFirst = true;
            break;
        }
    }

    HRESULT rc = S_OK;

    HRESULT firstRC = S_OK;
    com::ErrorInfoKeeper firstErr (true /* aIsNull */);

    for (size_t i = 0; i < ELEMENTS (storageTypes); ++ i)
    {
        size_t j = !haveFirst ? i : i == 0 ? first : i == first ? 0 : i;
        switch (storageTypes [j].type)
        {
            case HardDiskStorageType_VirtualDiskImage:
            {
                ComObjPtr <HVirtualDiskImage> obj;
                obj.createObject();
                rc = obj->init (aVirtualBox, NULL, aLocation,
                                FALSE /* aRegistered */);
                if (SUCCEEDED (rc))
                {
                    hardDisk = obj;
                    return rc;
                }
                break;
            }
            case HardDiskStorageType_VMDKImage:
            {
                ComObjPtr <HVMDKImage> obj;
                obj.createObject();
                rc = obj->init (aVirtualBox, NULL, aLocation,
                                FALSE /* aRegistered */);
                if (SUCCEEDED (rc))
                {
                    hardDisk = obj;
                    return rc;
                }
                break;
            }
            case HardDiskStorageType_CustomHardDisk:
            {
                ComObjPtr <HCustomHardDisk> obj;
                obj.createObject();
                rc = obj->init (aVirtualBox, NULL, aLocation,
                                FALSE /* aRegistered */);
                if (SUCCEEDED (rc))
                {
                    hardDisk = obj;
                    return rc;
                }
                break;
            }
            case HardDiskStorageType_VHDImage:
            {
                ComObjPtr <HVHDImage> obj;
                obj.createObject();
                rc = obj->init (aVirtualBox, NULL, aLocation,
                                FALSE /* aRegistered */);
                if (SUCCEEDED (rc))
                {
                    hardDisk = obj;
                    return rc;
                }
                break;
            }
            default:
            {
                AssertComRCReturnRC (E_FAIL);
            }
        }

        Assert (FAILED (rc));

        /* remember the error of the matching class */
        if (haveFirst && j == first)
        {
            firstRC = rc;
            firstErr.fetch();
        }
    }

    if (haveFirst)
    {
        Assert (FAILED (firstRC));
        /* firstErr will restore the error info upon destruction */
        return firstRC;
    }

    /* There was no exact extension match; chances are high that an error we
     * got after probing is useless. Use a generic error message instead. */

    firstErr.forget();

    return setError (E_FAIL,
        tr ("Could not recognize the format of the hard disk '%ls'. "
            "Either the given format is not supported or hard disk data "
            "is corrupt"),
        aLocation);
}

// protected methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads the base settings of the hard disk from the given node, registers
 *  it and loads and registers all child hard disks as HVirtualDiskImage
 *  instances.
 *
 *  Subclasses must call this method in their init() or loadSettings() methods
 *  *after* they load specific parts of data (at least, necessary to let
 *  toString() function correctly), in order to be properly loaded from the
 *  settings file and registered.
 *
 *  @param aHDNode      <HardDisk> node when #isDifferencing() = false, or
 *                      <DiffHardDisk> node otherwise.
 *
 *  @note
 *      Must be called from under the object's lock
 */
HRESULT HardDisk::loadSettings (const settings::Key &aHDNode)
{
    using namespace settings;

    AssertReturn (!aHDNode.isNull(), E_FAIL);

    /* required */
    mId = aHDNode.value <Guid> ("uuid");

    if (!isDifferencing())
    {
        /* type required for <HardDisk> nodes only */
        const char *type = aHDNode.stringValue ("type");
        if (strcmp (type, "normal") == 0)
            mType = HardDiskType_NormalHardDisk;
        else if (strcmp (type, "immutable") == 0)
            mType = HardDiskType_ImmutableHardDisk;
        else if (strcmp (type, "writethrough") == 0)
            mType = HardDiskType_WritethroughHardDisk;
        else
            ComAssertMsgFailedRet (("Invalid hard disk type '%s'\n", type),
                                   E_FAIL);
    }
    else
        mType = HardDiskType_NormalHardDisk;

    HRESULT rc = mVirtualBox->registerHardDisk (this, VirtualBox::RHD_OnStartUp);
    CheckComRCReturnRC (rc);

    /* load all children */
    Key::List children = aHDNode.keys ("DiffHardDisk");
    for (Key::List::const_iterator it = children.begin();
         it != children.end(); ++ it)
    {
        Key vdiNode = (*it).key ("VirtualDiskImage");

        ComObjPtr <HVirtualDiskImage> vdi;
        vdi.createObject();
        rc = vdi->init (mVirtualBox, this, (*it), vdiNode);
        CheckComRCBreakRC (rc);
    }

    return rc;
}

/**
 *  Saves the base settings of the hard disk to the given node
 *  and saves all child hard disks as <DiffHardDisk> nodes.
 *
 *  Subclasses must call this method in their saveSettings() methods
 *  in order to be properly saved to the settings file.
 *
 *  @param aHDNode      <HardDisk> node when #isDifferencing() = false, or
 *                      <DiffHardDisk> node otherwise.
 *
 *  @note
 *      Must be called from under the object's lock
 */
HRESULT HardDisk::saveSettings (settings::Key &aHDNode)
{
    using namespace settings;

    AssertReturn (!aHDNode.isNull(), E_FAIL);

    /* uuid (required) */
    aHDNode.setValue <Guid> ("uuid", mId);

    if (!isDifferencing())
    {
        /* type (required) */
        const char *type = NULL;
        switch (mType)
        {
            case HardDiskType_NormalHardDisk:
                type = "normal";
                break;
            case HardDiskType_ImmutableHardDisk:
                type = "immutable";
                break;
            case HardDiskType_WritethroughHardDisk:
                type = "writethrough";
                break;
        }
        aHDNode.setStringValue ("type", type);
    }

    /* save all children */
    AutoLock chLock (childrenLock());
    for (HardDiskList::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        ComObjPtr <HardDisk> child = *it;
        AutoLock childLock (child);

        Key hdNode = aHDNode.appendKey ("DiffHardDisk");

        {
            Key vdiNode = hdNode.createKey ("VirtualDiskImage");
            HRESULT rc = child->saveSettings (hdNode, vdiNode);
            CheckComRCReturnRC (rc);
        }
    }

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HVirtualDiskImage class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HVirtualDiskImage::FinalConstruct()
{
    HRESULT rc = HardDisk::FinalConstruct();
    if (FAILED (rc))
        return rc;

    mState = NotCreated;

    mStateCheckSem = NIL_RTSEMEVENTMULTI;
    mStateCheckWaiters = 0;

    mSize = 0;
    mActualSize = 0;

    return S_OK;
}

void HVirtualDiskImage::FinalRelease()
{
    HardDisk::FinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VDI hard disk object by reading its properties from
 *  the given configuration node. The created hard disk will be marked as
 *  registered on success.
 *
 *  @param aHDNode      <HardDisk> or <DiffHardDisk> node.
 *  @param aVDINode     <VirtualDiskImage> node.
 */
HRESULT HVirtualDiskImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                                 const settings::Key &aHDNode,
                                 const settings::Key &aVDINode)
{
    using namespace settings;

    LogFlowThisFunc (("\n"));

    AssertReturn (!aHDNode.isNull() && !aVDINode.isNull(), E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VirtualDiskImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* filePath (required) */
        Bstr filePath = aVDINode.stringValue ("filePath");
        rc = setFilePath (filePath);
        CheckComRCBreakRC (rc);

        LogFlowThisFunc (("'%ls'\n", mFilePathFull.raw()));

        /* load basic settings and children */
        rc = loadSettings (aHDNode);
        CheckComRCBreakRC (rc);

        mState = Created;
        mRegistered = TRUE;

        /* Don't call queryInformation() for registered hard disks to
         * prevent the calling thread (i.e. the VirtualBox server startup
         * thread) from an unexpected freeze. The vital mId property (UUID)
         * is read from the registry file in loadSettings(). To get the rest,
         * the user will have to call COMGETTER(Accessible) manually. */
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Initializes the VDI hard disk object using the given image file name.
 *
 *  @param aVirtualBox  VirtualBox parent.
 *  @param aParent      Parent hard disk.
 *  @param aFilePath    Path to the image file, or @c NULL to create an
 *                      image-less object.
 *  @param aRegistered  Whether to mark this disk as registered or not
 *                      (ignored when @a aFilePath is @c NULL, assuming @c FALSE)
 */
HRESULT HVirtualDiskImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                                const BSTR aFilePath, BOOL aRegistered /* = FALSE */)
{
    LogFlowThisFunc (("aFilePath='%ls', aRegistered=%d\n",
                      aFilePath, aRegistered));

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VirtualDiskImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        rc = setFilePath (aFilePath);
        CheckComRCBreakRC (rc);

        Assert (mId.isEmpty());

        if (aFilePath && *aFilePath)
        {
            mRegistered = aRegistered;
            mState = Created;

            /* Call queryInformation() anyway (even if it will block), because
             * it is the only way to get the UUID of the existing VDI and
             * initialize the vital mId property. */
            Bstr errMsg;
            rc = queryInformation (&errMsg);
            if (SUCCEEDED (rc))
            {
                /* We are constructing a new HVirtualDiskImage object. If there
                 * is a fatal accessibility error (we cannot read image UUID),
                 * we have to fail. We do so even on non-fatal errors as well,
                 * because it's not worth to keep going with the inaccessible
                 * image from the very beginning (when nothing else depends on
                 * it yet). */
                if (!errMsg.isNull())
                    rc = setErrorBstr (E_FAIL, errMsg);
            }
        }
        else
        {
            mRegistered = FALSE;
            mState = NotCreated;
            mId.create();
        }
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void HVirtualDiskImage::uninit()
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    if (!isReady())
        return;

    HardDisk::protectedUninit (alock);
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVirtualDiskImage::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP HVirtualDiskImage::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mState >= Created)
    {
        int vrc = VDISetImageComment (Utf8Str (mFilePathFull), Utf8Str (aDescription));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Could not change the description of the VDI hard disk '%ls' "
                    "(%Vrc)"),
                toString().raw(), vrc);
    }

    mDescription = aDescription;
    return S_OK;
}

STDMETHODIMP HVirtualDiskImage::COMGETTER(Size) (ULONG64 *aSize)
{
    if (!aSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    /* only a non-differencing image knows the logical size */
    if (isDifferencing())
        return root()->COMGETTER(Size) (aSize);

    *aSize = mSize;
    return S_OK;
}

STDMETHODIMP HVirtualDiskImage::COMGETTER(ActualSize) (ULONG64 *aActualSize)
{
    if (!aActualSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActualSize = mActualSize;
    return S_OK;
}

// IVirtualDiskImage properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVirtualDiskImage::COMGETTER(FilePath) (BSTR *aFilePath)
{
    if (!aFilePath)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mFilePathFull.cloneTo (aFilePath);
    return S_OK;
}

STDMETHODIMP HVirtualDiskImage::COMSETTER(FilePath) (INPTR BSTR aFilePath)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mState != NotCreated)
        return setError (E_ACCESSDENIED,
            tr ("Cannot change the file path of the existing hard disk '%ls'"),
            toString().raw());

    CHECK_BUSY_AND_READERS();

    /* append the default path if only a name is given */
    Bstr path = aFilePath;
    if (aFilePath && *aFilePath)
    {
        Utf8Str fp = aFilePath;
        if (!RTPathHavePath (fp))
        {
            AutoReaderLock propsLock (mVirtualBox->systemProperties());
            path = Utf8StrFmt ("%ls%c%s",
                               mVirtualBox->systemProperties()->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    return setFilePath (path);
}

STDMETHODIMP HVirtualDiskImage::COMGETTER(Created) (BOOL *aCreated)
{
    if (!aCreated)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aCreated = mState >= Created;
    return S_OK;
}

// IVirtualDiskImage methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVirtualDiskImage::CreateDynamicImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, TRUE /* aDynamic */, aProgress);
}

STDMETHODIMP HVirtualDiskImage::CreateFixedImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, FALSE /* aDynamic */, aProgress);
}

STDMETHODIMP HVirtualDiskImage::DeleteImage()
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_BUSY_AND_READERS();

    if (mRegistered)
        return setError (E_ACCESSDENIED,
             tr ("Cannot delete an image of the registered hard disk image '%ls"),
             mFilePathFull.raw());
    if (mState == NotCreated)
        return setError (E_FAIL,
             tr ("Hard disk image has been already deleted or never created"));

    HRESULT rc = deleteImage();
    CheckComRCReturnRC (rc);

    mState = NotCreated;

    /* reset the fields */
    mSize = 0;
    mActualSize = 0;

    return S_OK;
}

// public/protected methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Attempts to mark the hard disk as registered.
 *  Only VirtualBox can call this method.
 */
HRESULT HVirtualDiskImage::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        if (mState == NotCreated)
            return setError (E_FAIL,
                tr ("Image file '%ls' is not yet created for this hard disk"),
                mFilePathFull.raw());
        if (isDifferencing())
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is differencing and cannot be unregistered "
                    "explicitly"),
                mFilePathFull.raw());
    }
    else
    {
        ComAssertRet (mState >= Created, E_FAIL);
    }

    return HardDisk::trySetRegistered (aRegistered);
}

/**
 *  Checks accessibility of this hard disk image only (w/o parents).
 *
 *  @param aAccessError on output, a null string indicates the hard disk is
 *                      accessible, otherwise contains a message describing
 *                      the reason of inaccessibility.
 */
HRESULT HVirtualDiskImage::getAccessible (Bstr &aAccessError)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mStateCheckSem != NIL_RTSEMEVENTMULTI)
    {
        /* An accessibility check in progress on some other thread,
         * wait for it to finish. */

        ComAssertRet (mStateCheckWaiters != (ULONG) ~0, E_FAIL);
        ++ mStateCheckWaiters;
        alock.leave();

        int vrc = RTSemEventMultiWait (mStateCheckSem, RT_INDEFINITE_WAIT);

        alock.enter();
        AssertReturn (mStateCheckWaiters != 0, E_FAIL);
        -- mStateCheckWaiters;
        if (mStateCheckWaiters == 0)
        {
            RTSemEventMultiDestroy (mStateCheckSem);
            mStateCheckSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRCReturn (vrc, E_FAIL);

        /* don't touch aAccessError, it has been already set */
        return S_OK;
    }

    /* check the basic accessibility */
    HRESULT rc = getBaseAccessible (aAccessError, true /* aCheckBusy */);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    if (mState >= Created)
    {
        return queryInformation (&aAccessError);
    }

    aAccessError = Utf8StrFmt ("Hard disk image '%ls' is not yet created",
                               mFilePathFull.raw());
    return S_OK;
}

/**
 *  Saves hard disk settings to the specified storage node and saves
 *  all children to the specified hard disk node
 *
 *  @param aHDNode      <HardDisk> or <DiffHardDisk> node.
 *  @param aStorageNode <VirtualDiskImage> node.
 */
HRESULT HVirtualDiskImage::saveSettings (settings::Key &aHDNode,
                                         settings::Key &aStorageNode)
{
    AssertReturn (!aHDNode.isNull() && !aStorageNode.isNull(), E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    /* filePath (required) */
    aStorageNode.setValue <Bstr> ("filePath", mFilePath);

    /* save basic settings and children */
    return HardDisk::saveSettings (aHDNode);
}

/**
 *  Checks if the given change of \a aOldPath to \a aNewPath affects the path
 *  of this hard disk and updates it if necessary to reflect the new location.
 *  Intended to be from HardDisk::updatePaths().
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object for writing.
 */
void HVirtualDiskImage::updatePath (const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoLock alock (this);
    AssertReturnVoid (isReady());

    size_t oldPathLen = strlen (aOldPath);

    Utf8Str path = mFilePathFull;
    LogFlowThisFunc (("VDI.fullPath={%s}\n", path.raw()));

    if (RTPathStartsWith (path, aOldPath))
    {
        Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                              path.raw() + oldPathLen);
        path = newPath;

        mVirtualBox->calculateRelativePath (path, path);

        unconst (mFilePathFull) = newPath;
        unconst (mFilePath) = path;

        LogFlowThisFunc (("-> updated: full={%s} short={%s}\n",
                          newPath.raw(), path.raw()));
    }
}

/**
 *  Returns the string representation of this hard disk.
 *  When \a aShort is false, returns the full image file path.
 *  Otherwise, returns the image file name only.
 *
 *  @param aShort       if true, a short representation is returned
 */
Bstr HVirtualDiskImage::toString (bool aShort /* = false */)
{
    AutoLock alock (this);

    if (!aShort)
        return mFilePathFull;
    else
    {
        Utf8Str fname = mFilePathFull;
        return RTPathFilename (fname.mutableRaw());
    }
}

/**
 *  Creates a clone of this hard disk by storing hard disk data in the given
 *  VDI file.
 *
 *  If the operation fails, @a aDeleteTarget will be set to @c true unless the
 *  failure happened because the target file already existed.
 *
 *  @param aId              UUID to assign to the created image.
 *  @param aTargetPath      VDI file where the cloned image is to be to stored.
 *  @param aProgress        progress object to run during operation.
 *  @param aDeleteTarget    Whether it is recommended to delete target on
 *                          failure or not.
 */
HRESULT
HVirtualDiskImage::cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                                 Progress *aProgress, bool &aDeleteTarget)
{
    /* normally, the target file should be deleted on error */
    aDeleteTarget = true;

    AssertReturn (!aId.isEmpty(), E_FAIL);
    AssertReturn (!aTargetPath.isNull(), E_FAIL);
    AssertReturn (aProgress, E_FAIL);

    AutoLock alock (this);
    AssertReturn (isReady(), E_FAIL);

    AssertReturn (isBusy() == false, E_FAIL);

    /// @todo (dmik) cloning of differencing images is not yet supported
    AssertReturn (mParent.isNull(), E_FAIL);

    Utf8Str filePathFull = mFilePathFull;

    if (mState == NotCreated)
        return setError (E_FAIL,
            tr ("Source hard disk image '%s' is not yet created"),
            filePathFull.raw());

    addReader();
    alock.leave();

    int vrc = VDICopyImage (aTargetPath, filePathFull, NULL,
                            progressCallback,
                            static_cast <Progress *> (aProgress));

    alock.enter();
    releaseReader();

    /* We don't want to delete existing user files */
    if (vrc == VERR_ALREADY_EXISTS)
        aDeleteTarget = false;

    if (VBOX_SUCCESS (vrc))
        vrc = VDISetImageUUIDs (aTargetPath, aId, NULL, NULL, NULL);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not copy the hard disk image '%s' to '%s' (%Vrc)"),
            filePathFull.raw(), aTargetPath.raw(), vrc);

    return S_OK;
}

/**
 *  Creates a new differencing image for this hard disk with the given
 *  VDI file name.
 *
 *  @param aId          UUID to assign to the created image
 *  @param aTargetPath  VDI file where to store the created differencing image
 *  @param aProgress    progress object to run during operation
 *                      (can be NULL)
 */
HRESULT
HVirtualDiskImage::createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                                    Progress *aProgress)
{
    AssertReturn (!aId.isEmpty(), E_FAIL);
    AssertReturn (!aTargetPath.isNull(), E_FAIL);

    AutoLock alock (this);
    AssertReturn (isReady(), E_FAIL);

    AssertReturn (isBusy() == false, E_FAIL);
    AssertReturn (mState >= Created, E_FAIL);

    addReader();
    alock.leave();

    int vrc = VDICreateDifferenceImage (aTargetPath, Utf8Str (mFilePathFull),
                                        NULL, aProgress ? progressCallback : NULL,
                                        static_cast <Progress *> (aProgress));
    alock.enter();
    releaseReader();

    /* update the UUID to correspond to the file name */
    if (VBOX_SUCCESS (vrc))
        vrc = VDISetImageUUIDs (aTargetPath, aId, NULL, NULL, NULL);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not create a differencing hard disk '%s' (%Vrc)"),
            aTargetPath.raw(), vrc);

    return S_OK;
}

/**
 *  Copies the image file of this hard disk to a separate VDI file (with an
 *  unique creation UUID) and creates a new hard disk object for the copied
 *  image. The copy will be created as a child of this hard disk's parent
 *  (so that this hard disk must be a differencing one).
 *
 *  The specified progress object (if not NULL) receives the percentage
 *  of the operation completion. However, it is responsibility of the caller to
 *  call Progress::notifyComplete() after this method returns.
 *
 *  @param aFolder      folder where to create a copy (must be a full path)
 *  @param aMachineId   machine ID the new hard disk will belong to
 *  @param aHardDisk    resulting hard disk object
 *  @param aProgress    progress object to run during copy operation
 *                      (may be NULL)
 *
 *  @note
 *      Must be NOT called from under locks of other objects that need external
 *      access dirung this method execurion!
 */
HRESULT
HVirtualDiskImage::cloneDiffImage (const Bstr &aFolder, const Guid &aMachineId,
                                   ComObjPtr <HVirtualDiskImage> &aHardDisk,
                                   Progress *aProgress)
{
    AssertReturn (!aFolder.isEmpty() && !aMachineId.isEmpty(),
                  E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    AssertReturn (!mParent.isNull(), E_FAIL);

    ComAssertRet (isBusy() == false, E_FAIL);
    ComAssertRet (mState >= Created, E_FAIL);

    Guid id;
    id.create();

    Utf8Str filePathTo = Utf8StrFmt ("%ls%c{%Vuuid}.vdi",
                                     aFolder.raw(), RTPATH_DELIMITER, id.ptr());

    /* try to make the path relative to the vbox home dir */
    const char *filePathToRel = filePathTo;
    {
        const Utf8Str &homeDir = mVirtualBox->homeDir();
        if (!strncmp (filePathTo, homeDir, homeDir.length()))
            filePathToRel = (filePathToRel + homeDir.length() + 1);
    }

    /* first ensure the directory exists */
    {
        Utf8Str dir = aFolder;
        if (!RTDirExists (dir))
        {
            int vrc = RTDirCreateFullPath (dir, 0777);
            if (VBOX_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to store the image file (%Vrc)"),
                    dir.raw(), vrc);
            }
        }
    }

    Utf8Str filePathFull = mFilePathFull;

    alock.leave();

    int vrc = VDICopyImage (filePathTo, filePathFull, NULL,
                            progressCallback,
                            static_cast <Progress *> (aProgress));

    alock.enter();

    /* get modification and parent UUIDs of this image */
    RTUUID modUuid, parentUuid, parentModUuid;
    if (VBOX_SUCCESS (vrc))
        vrc = VDIGetImageUUIDs (filePathFull, NULL, &modUuid,
                                &parentUuid, &parentModUuid);

    // update the UUID of the copy to correspond to the file name
    // and copy all other UUIDs from this image
    if (VBOX_SUCCESS (vrc))
        vrc = VDISetImageUUIDs (filePathTo, id, &modUuid,
                                &parentUuid, &parentModUuid);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not copy the hard disk image '%s' to '%s' (%Vrc)"),
            filePathFull.raw(), filePathTo.raw(), vrc);

    ComObjPtr <HVirtualDiskImage> vdi;
    vdi.createObject();
    HRESULT rc = vdi->init (mVirtualBox, mParent, Bstr (filePathToRel),
                            TRUE /* aRegistered  */);
    if (FAILED (rc))
        return rc;

    /* associate the created hard disk with the given machine */
    vdi->setMachineId (aMachineId);

    rc = mVirtualBox->registerHardDisk (vdi, VirtualBox::RHD_Internal);
    if (FAILED (rc))
        return rc;

    aHardDisk = vdi;

    return S_OK;
}

/**
 *  Merges this child image to its parent image and updates the parent UUID
 *  of all children of this image (to point to this image's parent).
 *  It's a responsibility of the caller to unregister and uninitialize
 *  the merged image on success.
 *
 *  This method is intended to be called on a worker thread (the operation
 *  can be time consuming).
 *
 *  The specified progress object (if not NULL) receives the percentage
 *  of the operation completion. However, it is responsibility of the caller to
 *  call Progress::notifyComplete() after this method returns.
 *
 *  @param aProgress    progress object to run during copy operation
 *                      (may be NULL)
 *
 *  @note
 *      This method expects that both this hard disk and the paret hard disk
 *      are marked as busy using #setBusyWithChildren() prior to calling it!
 *      Busy flags of both hard disks will be cleared by this method
 *      on a successful return. In case of failure, #clearBusyWithChildren()
 *      must be called on a parent.
 *
 *  @note
 *      Must be NOT called from under locks of other objects that need external
 *      access dirung this method execurion!
 */
HRESULT HVirtualDiskImage::mergeImageToParent (Progress *aProgress)
{
    LogFlowThisFunc (("mFilePathFull='%ls'\n", mFilePathFull.raw()));

    AutoLock alock (this);
    CHECK_READY();

    AssertReturn (!mParent.isNull(), E_FAIL);
    AutoLock parentLock (mParent);

    ComAssertRet (isBusy() == true, E_FAIL);
    ComAssertRet (mParent->isBusy() == true, E_FAIL);

    ComAssertRet (mState >= Created && mParent->asVDI()->mState >= Created, E_FAIL);

    ComAssertMsgRet (mParent->storageType() == HardDiskStorageType_VirtualDiskImage,
                     ("non VDI storage types are not yet supported!"), E_FAIL);

    parentLock.leave();
    alock.leave();

    int vrc = VDIMergeImage (Utf8Str (mFilePathFull),
                             Utf8Str (mParent->asVDI()->mFilePathFull),
                             progressCallback,
                             static_cast <Progress *> (aProgress));
    alock.enter();
    parentLock.enter();

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not merge the hard disk image '%ls' to "
                "its parent image '%ls' (%Vrc)"),
            mFilePathFull.raw(), mParent->asVDI()->mFilePathFull.raw(), vrc);

    {
        HRESULT rc = S_OK;

        AutoLock chLock (childrenLock());

        for (HardDiskList::const_iterator it = children().begin();
             it != children().end(); ++ it)
        {
            ComObjPtr <HVirtualDiskImage> child = (*it)->asVDI();
            AutoLock childLock (child);

            /* reparent the child */
            child->mParent = mParent;
            if (mParent)
                mParent->addDependentChild (child);

            /* change the parent UUID in the image as well */
            RTUUID parentUuid, parentModUuid;
            vrc = VDIGetImageUUIDs (Utf8Str (mParent->asVDI()->mFilePathFull),
                                    &parentUuid, &parentModUuid, NULL, NULL);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not access the hard disk image '%ls' (%Vrc)"),
                    mParent->asVDI()->mFilePathFull.raw(), vrc);
                break;
            }
            ComAssertBreak (mParent->id() == Guid (parentUuid), rc = E_FAIL);
            vrc = VDISetImageUUIDs (Utf8Str (child->mFilePathFull),
                                    NULL, NULL, &parentUuid, &parentModUuid);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not update parent UUID of the hard disk image "
                        "'%ls' (%Vrc)"),
                    child->mFilePathFull.raw(), vrc);
                break;
            }
        }

        if (FAILED (rc))
            return rc;
    }

    /* detach all our children to avoid their uninit in #uninit() */
    removeDependentChildren();

    mParent->clearBusy();
    clearBusy();

    return S_OK;
}

/**
 *  Merges this image to all its child images, updates the parent UUID
 *  of all children of this image (to point to this image's parent).
 *  It's a responsibility of the caller to unregister and uninitialize
 *  the merged image on success.
 *
 *  This method is intended to be called on a worker thread (the operation
 *  can be time consuming).
 *
 *  The specified progress object (if not NULL) receives the percentage
 *  of the operation completion. However, it is responsibility of the caller to
 *  call Progress::notifyComplete() after this method returns.
 *
 *  @param aProgress    progress object to run during copy operation
 *                      (may be NULL)
 *
 *  @note
 *      This method expects that both this hard disk and all children
 *      are marked as busy using setBusyWithChildren() prior to calling it!
 *      Busy flags of all affected hard disks will be cleared by this method
 *      on a successful return. In case of failure, #clearBusyWithChildren()
 *      must be called for this hard disk.
 *
 *  @note
 *      Must be NOT called from under locks of other objects that need external
 *      access dirung this method execurion!
 */
HRESULT HVirtualDiskImage::mergeImageToChildren (Progress *aProgress)
{
    LogFlowThisFunc (("mFilePathFull='%ls'\n", mFilePathFull.raw()));

    AutoLock alock (this);
    CHECK_READY();

    /* this must be a diff image */
    AssertReturn (isDifferencing(), E_FAIL);

    ComAssertRet (isBusy() == true, E_FAIL);
    ComAssertRet (mState >= Created, E_FAIL);

    ComAssertMsgRet (mParent->storageType() == HardDiskStorageType_VirtualDiskImage,
                     ("non VDI storage types are not yet supported!"), E_FAIL);

    {
        HRESULT rc = S_OK;

        AutoLock chLock (childrenLock());

        /* iterate over a copy since we will modify the list */
        HardDiskList list = children();

        for (HardDiskList::const_iterator it = list.begin();
             it != list.end(); ++ it)
        {
            ComObjPtr <HardDisk> hd = *it;
            ComObjPtr <HVirtualDiskImage> child = hd->asVDI();
            AutoLock childLock (child);

            ComAssertRet (child->isBusy() == true, E_FAIL);
            ComAssertBreak (child->mState >= Created, rc = E_FAIL);

            childLock.leave();
            alock.leave();

            int vrc = VDIMergeImage (Utf8Str (mFilePathFull),
                                     Utf8Str (child->mFilePathFull),
                                     progressCallback,
                                     static_cast <Progress *> (aProgress));
            alock.enter();
            childLock.enter();

            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not merge the hard disk image '%ls' to "
                        "its parent image '%ls' (%Vrc)"),
                    mFilePathFull.raw(), child->mFilePathFull.raw(), vrc);
                break;
            }

            /* reparent the child */
            child->mParent = mParent;
            if (mParent)
                mParent->addDependentChild (child);

            /* change the parent UUID in the image as well */
            RTUUID parentUuid, parentModUuid;
            vrc = VDIGetImageUUIDs (Utf8Str (mParent->asVDI()->mFilePathFull),
                                    &parentUuid, &parentModUuid, NULL, NULL);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not access the hard disk image '%ls' (%Vrc)"),
                    mParent->asVDI()->mFilePathFull.raw(), vrc);
                break;
            }
            ComAssertBreak (mParent->id() == Guid (parentUuid), rc = E_FAIL);
            vrc = VDISetImageUUIDs (Utf8Str (child->mFilePathFull),
                                    NULL, NULL, &parentUuid, &parentModUuid);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not update parent UUID of the hard disk image "
                        "'%ls' (%Vrc)"),
                    child->mFilePathFull.raw(), vrc);
                break;
            }

            /* detach child to avoid its uninit in #uninit() */
            removeDependentChild (child);

            /* remove the busy flag */
            child->clearBusy();
        }

        if (FAILED (rc))
            return rc;
    }

    clearBusy();

    return S_OK;
}

/**
 *  Deletes and recreates the differencing hard disk image from scratch.
 *  The file name and UUID remain the same.
 */
HRESULT HVirtualDiskImage::wipeOutImage()
{
    AutoLock alock (this);
    CHECK_READY();

    AssertReturn (isDifferencing(), E_FAIL);
    AssertReturn (mRegistered, E_FAIL);
    AssertReturn (mState >= Created, E_FAIL);

    ComAssertMsgRet (mParent->storageType() == HardDiskStorageType_VirtualDiskImage,
                     ("non-VDI storage types are not yet supported!"), E_FAIL);

    Utf8Str filePathFull = mFilePathFull;

    int vrc = RTFileDelete (filePathFull);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not delete the image file '%s' (%Vrc)"),
            filePathFull.raw(), vrc);

    vrc = VDICreateDifferenceImage (filePathFull,
                                    Utf8Str (mParent->asVDI()->mFilePathFull),
                                    NULL, NULL, NULL);
    /* update the UUID to correspond to the file name */
    if (VBOX_SUCCESS (vrc))
        vrc = VDISetImageUUIDs (filePathFull, mId, NULL, NULL, NULL);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not create a differencing hard disk '%s' (%Vrc)"),
            filePathFull.raw(), vrc);

    return S_OK;
}

HRESULT HVirtualDiskImage::deleteImage (bool aIgnoreErrors /* = false */)
{
    AutoLock alock (this);
    CHECK_READY();

    AssertReturn (!mRegistered, E_FAIL);
    AssertReturn (mState >= Created, E_FAIL);

    int vrc = RTFileDelete (Utf8Str (mFilePathFull));
    if (VBOX_FAILURE (vrc) && !aIgnoreErrors)
        return setError (E_FAIL,
            tr ("Could not delete the image file '%ls' (%Vrc)"),
            mFilePathFull.raw(), vrc);

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to set a new file path.
 *  Resolves a path relatively to the Virtual Box home directory.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT HVirtualDiskImage::setFilePath (const BSTR aFilePath)
{
    if (aFilePath && *aFilePath)
    {
        /* get the full file name */
        char filePathFull [RTPATH_MAX];
        int vrc = RTPathAbsEx (mVirtualBox->homeDir(), Utf8Str (aFilePath),
                               filePathFull, sizeof (filePathFull));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Invalid image file path '%ls' (%Vrc)"), aFilePath, vrc);

        mFilePath = aFilePath;
        mFilePathFull = filePathFull;
    }
    else
    {
        mFilePath.setNull();
        mFilePathFull.setNull();
    }

    return S_OK;
}

/**
 *  Helper to query information about the VDI hard disk.
 *
 *  @param aAccessError not used when NULL, otherwise see #getAccessible()
 *
 *  @note Must be called from under the object's lock, only after
 *        CHECK_BUSY_AND_READERS() succeeds.
 */
HRESULT HVirtualDiskImage::queryInformation (Bstr *aAccessError)
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    /* create a lock object to completely release it later */
    AutoLock alock (this);

    AssertReturn (mStateCheckWaiters == 0, E_FAIL);

    ComAssertRet (mState >= Created, E_FAIL);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* lazily create a semaphore */
    vrc = RTSemEventMultiCreate (&mStateCheckSem);
    ComAssertRCRet (vrc, E_FAIL);

    /* Reference the disk to prevent any concurrent modifications
     * after releasing the lock below (to unblock getters before
     * a lengthy operation). */
    addReader();

    alock.leave();

    /* VBoxVHDD management interface needs to be optimized: we're opening a
     * file three times in a raw to get three bits of information. */

    Utf8Str filePath = mFilePathFull;
    Bstr errMsg;

    do
    {
        /* check the image file */
        Guid id, parentId;
        vrc = VDICheckImage (filePath, NULL, NULL, NULL,
                             id.ptr(), parentId.ptr(), NULL, 0);

        if (VBOX_FAILURE (vrc))
            break;

        if (!mId.isEmpty())
        {
            /* check that the actual UUID of the image matches the stored UUID */
            if (mId != id)
            {
                errMsg = Utf8StrFmt (
                    tr ("Actual UUID {%Vuuid} of the hard disk image '%s' doesn't "
                        "match UUID {%Vuuid} stored in the registry"),
                    id.ptr(), filePath.raw(), mId.ptr());
                break;
            }
        }
        else
        {
            /* assgn an UUID read from the image file */
            mId = id;
        }

        if (mParent)
        {
            /* check parent UUID */
            AutoLock parentLock (mParent);
            if (mParent->id() != parentId)
            {
                errMsg = Utf8StrFmt (
                    tr ("UUID {%Vuuid} of the parent image '%ls' stored in "
                        "the hard disk image file '%s' doesn't match "
                        "UUID {%Vuuid} stored in the registry"),
                    parentId.raw(), mParent->toString().raw(),
                    filePath.raw(), mParent->id().raw());
                break;
            }
        }
        else if (!parentId.isEmpty())
        {
            errMsg = Utf8StrFmt (
                tr ("Hard disk image '%s' is a differencing image that is linked "
                    "to a hard disk with UUID {%Vuuid} and cannot be used "
                    "directly as a base hard disk"),
                filePath.raw(), parentId.raw());
            break;
        }

        {
            RTFILE file = NIL_RTFILE;
            vrc = RTFileOpen (&file, filePath, RTFILE_O_READ);
            if (VBOX_SUCCESS (vrc))
            {
                uint64_t size = 0;
                vrc = RTFileGetSize (file, &size);
                if (VBOX_SUCCESS (vrc))
                    mActualSize = size;
                RTFileClose (file);
            }
            if (VBOX_FAILURE (vrc))
                break;
        }

        if (!mParent)
        {
            /* query logical size only for non-differencing images */

            PVDIDISK disk = VDIDiskCreate();
            vrc = VDIDiskOpenImage (disk, Utf8Str (mFilePathFull),
                                    VDI_OPEN_FLAGS_READONLY);
            if (VBOX_SUCCESS (vrc))
            {
                uint64_t size = VDIDiskGetSize (disk);
                /* convert to MBytes */
                mSize = size / 1024 / 1024;
            }

            VDIDiskDestroy (disk);
            if (VBOX_FAILURE (vrc))
                break;
        }
    }
    while (0);

    /* enter the lock again */
    alock.enter();

    /* remove the reference */
    releaseReader();

    if (FAILED (rc) || VBOX_FAILURE (vrc) || !errMsg.isNull())
    {
        LogWarningFunc (("'%ls' is not accessible "
                         "(rc=%08X, vrc=%Vrc, errMsg='%ls')\n",
                         mFilePathFull.raw(), rc, vrc, errMsg.raw()));

        if (aAccessError)
        {
            if (!errMsg.isNull())
                *aAccessError = errMsg;
            else if (VBOX_FAILURE (vrc))
                *aAccessError = Utf8StrFmt (
                    tr ("Could not access hard disk image '%ls' (%Vrc)"),
                        mFilePathFull.raw(), vrc);
        }

        /* downgrade to not accessible */
        mState = Created;
    }
    else
    {
        if (aAccessError)
            aAccessError->setNull();

        mState = Accessible;
    }

    /* inform waiters if there are any */
    if (mStateCheckWaiters > 0)
    {
        RTSemEventMultiSignal (mStateCheckSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (mStateCheckSem);
        mStateCheckSem = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}

/**
 *  Helper to create hard disk images.
 *
 *  @param aSize        size in MB
 *  @param aDynamic     dynamic or fixed image
 *  @param aProgress    address of IProgress pointer to return
 */
HRESULT HVirtualDiskImage::createImage (ULONG64 aSize, BOOL aDynamic,
                                        IProgress **aProgress)
{
    AutoLock alock (this);

    CHECK_BUSY_AND_READERS();

    if (mState != NotCreated)
        return setError (E_ACCESSDENIED,
            tr ("Hard disk image '%ls' is already created"),
            mFilePathFull.raw());

    if (!mFilePathFull)
        return setError (E_ACCESSDENIED,
            tr ("Cannot create a hard disk image using an empty (null) file path"),
            mFilePathFull.raw());

    /* first ensure the directory exists */
    {
        Utf8Str imageDir = mFilePathFull;
        RTPathStripFilename (imageDir.mutableRaw());
        if (!RTDirExists (imageDir))
        {
            int vrc = RTDirCreateFullPath (imageDir, 0777);
            if (VBOX_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to store the image file (%Vrc)"),
                    imageDir.raw(), vrc);
            }
        }
    }

    /* check whether the given file exists or not */
    RTFILE file;
    int vrc = RTFileOpen (&file, Utf8Str (mFilePathFull),
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (vrc != VERR_FILE_NOT_FOUND)
    {
        if (VBOX_SUCCESS (vrc))
            RTFileClose (file);
        switch(vrc)
        {
            case VINF_SUCCESS:
                return setError (E_FAIL,
                    tr ("Image file '%ls' already exists"),
                    mFilePathFull.raw());

            default:
                return setError(E_FAIL,
                    tr ("Invalid image file path '%ls' (%Vrc)"),
                    mFilePathFull.raw(), vrc);
        }
    }

    /* check VDI size limits */
    {
        HRESULT rc;
        ULONG64 maxVDISize;
        ComPtr <ISystemProperties> props;
        rc = mVirtualBox->COMGETTER(SystemProperties) (props.asOutParam());
        ComAssertComRCRet (rc, E_FAIL);
        rc = props->COMGETTER(MaxVDISize) (&maxVDISize);
        ComAssertComRCRet (rc, E_FAIL);

        if (aSize < 1 || aSize > maxVDISize)
            return setError (E_INVALIDARG,
                tr ("Invalid VDI size: %llu MB (must be in range [1, %llu] MB)"),
                aSize, maxVDISize);
    }

    HRESULT rc;

    /* create a project object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    {
        Bstr desc = aDynamic ? tr ("Creating a dynamically expanding hard disk")
                             : tr ("Creating a fixed-size hard disk");
        rc = progress->init (mVirtualBox, (IVirtualDiskImage *) this, desc,
                             FALSE /* aCancelable */);
        CheckComRCReturnRC (rc);
    }

    /* mark as busy (being created)
     * (VDI task thread will unmark it) */
    setBusy();

    /* fill in VDI task data */
    VDITask *task = new VDITask (aDynamic ? VDITask::CreateDynamic
                                          : VDITask::CreateStatic,
                                 this, progress);
    task->size = aSize;
    task->size *= 1024 * 1024; /* convert to bytes */

    /* create the hard disk creation thread, pass operation data */
    vrc = RTThreadCreate (NULL, VDITaskThread, (void *) task, 0,
                          RTTHREADTYPE_MAIN_HEAVY_WORKER, 0, "VDITask");
    ComAssertMsgRC (vrc, ("Could not create a thread (%Vrc)", vrc));
    if (VBOX_FAILURE (vrc))
    {
        clearBusy();
        delete task;
        rc = E_FAIL;
    }
    else
    {
        /* get one interface for the caller */
        progress.queryInterfaceTo (aProgress);
    }

    return rc;
}

/* static */
DECLCALLBACK(int) HVirtualDiskImage::VDITaskThread (RTTHREAD thread, void *pvUser)
{
    VDITask *task = static_cast <VDITask *> (pvUser);
    AssertReturn (task, VERR_GENERAL_FAILURE);

    LogFlowFunc (("operation=%d, size=%llu\n", task->operation, task->size));

    VDIIMAGETYPE type = (VDIIMAGETYPE) 0;

    switch (task->operation)
    {
        case VDITask::CreateDynamic: type = VDI_IMAGE_TYPE_NORMAL; break;
        case VDITask::CreateStatic: type = VDI_IMAGE_TYPE_FIXED; break;
        case VDITask::CloneToImage: break;
        default: AssertFailedReturn (VERR_GENERAL_FAILURE); break;
    }

    HRESULT rc = S_OK;
    Utf8Str errorMsg;

    bool deleteTarget = true;

    if (task->operation == VDITask::CloneToImage)
    {
        Assert (!task->vdi->id().isEmpty());
        /// @todo (dmik) check locks
        AutoLock sourceLock (task->source);
        rc = task->source->cloneToImage (task->vdi->id(),
                                         Utf8Str (task->vdi->filePathFull()),
                                         task->progress, deleteTarget);

        /* release reader added in HardDisk::CloneToImage() */
        task->source->releaseReader();
    }
    else
    {
        int vrc = VDICreateBaseImage (Utf8Str (task->vdi->filePathFull()),
                                      type, task->size,
                                      Utf8Str (task->vdi->mDescription),
                                      progressCallback,
                                      static_cast <Progress *> (task->progress));

        /* We don't want to delete existing user files */
        if (vrc == VERR_ALREADY_EXISTS)
            deleteTarget = false;

        if (VBOX_SUCCESS (vrc) && task->vdi->id())
        {
            /* we have a non-null UUID, update the created image */
            vrc = VDISetImageUUIDs (Utf8Str (task->vdi->filePathFull()),
                                    task->vdi->id().raw(), NULL, NULL, NULL);
        }

        if (VBOX_FAILURE (vrc))
        {
            errorMsg = Utf8StrFmt (
                tr ("Falied to create a hard disk image '%ls' (%Vrc)"),
                task->vdi->filePathFull().raw(), vrc);
            rc = E_FAIL;
        }
    }

    LogFlowFunc (("rc=%08X\n", rc));

    AutoLock alock (task->vdi);

    /* clear busy set in in HardDisk::CloneToImage() or
     * in HVirtualDiskImage::createImage() */
    task->vdi->clearBusy();

    if (SUCCEEDED (rc))
    {
        task->vdi->mState = HVirtualDiskImage::Created;
        /* update VDI data fields */
        Bstr errMsg;
        rc = task->vdi->queryInformation (&errMsg);
        /* we want to deliver the access check result to the caller
         * immediately, before he calls HardDisk::GetAccssible() himself. */
        if (SUCCEEDED (rc) && !errMsg.isNull())
            task->progress->notifyCompleteBstr (
                E_FAIL, COM_IIDOF (IVirtualDiskImage), getComponentName(),
                errMsg);
        else
            task->progress->notifyComplete (rc);
    }
    else
    {
        /* delete the target file so we don't have orphaned files */
        if (deleteTarget)
            RTFileDelete(Utf8Str (task->vdi->filePathFull()));

        task->vdi->mState = HVirtualDiskImage::NotCreated;
        /* complete the progress object */
        if (errorMsg)
            task->progress->notifyComplete (
                E_FAIL, COM_IIDOF (IVirtualDiskImage), getComponentName(),
                errorMsg);
        else
            task->progress->notifyComplete (rc);
    }

    delete task;

    return VINF_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// HISCSIHardDisk class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HISCSIHardDisk::FinalConstruct()
{
    HRESULT rc = HardDisk::FinalConstruct();
    if (FAILED (rc))
        return rc;

    mSize = 0;
    mActualSize = 0;

    mPort = 0;
    mLun = 0;

    return S_OK;
}

void HISCSIHardDisk::FinalRelease()
{
    HardDisk::FinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the iSCSI hard disk object by reading its properties from
 *  the given configuration node. The created hard disk will be marked as
 *  registered on success.
 *
 *  @param aHDNode      <HardDisk> node.
 *  @param aVDINod      <ISCSIHardDisk> node.
 */
HRESULT HISCSIHardDisk::init (VirtualBox *aVirtualBox,
                              const settings::Key &aHDNode,
                              const settings::Key &aISCSINode)
{
    using namespace settings;

    LogFlowThisFunc (("\n"));

    AssertReturn (!aHDNode.isNull() && !aISCSINode.isNull(), E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_ISCSIHardDisk;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, NULL);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* server (required) */
        mServer = aISCSINode.stringValue ("server");
        /* target (required) */
        mTarget = aISCSINode.stringValue ("target");

        /* port (optional) */
        mPort = aISCSINode.value <USHORT> ("port");
        /* lun (optional) */
        mLun = aISCSINode.value <ULONG64> ("lun");
        /* userName (optional) */
        mUserName = aISCSINode.stringValue ("userName");
        /* password (optional) */
        mPassword = aISCSINode.stringValue ("password");

        LogFlowThisFunc (("'iscsi:%ls:%hu@%ls/%ls:%llu'\n",
                          mServer.raw(), mPort, mUserName.raw(), mTarget.raw(),
                          mLun));

        /* load basic settings and children */
        rc = loadSettings (aHDNode);
        CheckComRCBreakRC (rc);

        if (mType != HardDiskType_WritethroughHardDisk)
        {
            rc = setError (E_FAIL,
                tr ("Currently, non-Writethrough iSCSI hard disks are not "
                    "allowed ('%ls')"),
                toString().raw());
            break;
        }

        mRegistered = TRUE;
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Initializes the iSCSI hard disk object using default values for all
 *  properties. The created hard disk will NOT be marked as registered.
 */
HRESULT HISCSIHardDisk::init (VirtualBox *aVirtualBox)
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_ISCSIHardDisk;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, NULL);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* we have to generate a new UUID */
        mId.create();
        /* currently, all iSCSI hard disks are writethrough */
        mType = HardDiskType_WritethroughHardDisk;
        mRegistered = FALSE;
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void HISCSIHardDisk::uninit()
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    if (!isReady())
        return;

    HardDisk::protectedUninit (alock);
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HISCSIHardDisk::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mDescription != aDescription)
    {
        mDescription = aDescription;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(Size) (ULONG64 *aSize)
{
    if (!aSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aSize = mSize;
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(ActualSize) (ULONG64 *aActualSize)
{
    if (!aActualSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActualSize = mActualSize;
    return S_OK;
}

// IISCSIHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HISCSIHardDisk::COMGETTER(Server) (BSTR *aServer)
{
    if (!aServer)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mServer.cloneTo (aServer);
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Server) (INPTR BSTR aServer)
{
    if (!aServer || !*aServer)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mServer != aServer)
    {
        mServer = aServer;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(Port) (USHORT *aPort)
{
    if (!aPort)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aPort = mPort;
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Port) (USHORT aPort)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mPort != aPort)
    {
        mPort = aPort;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(Target) (BSTR *aTarget)
{
    if (!aTarget)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mTarget.cloneTo (aTarget);
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Target) (INPTR BSTR aTarget)
{
    if (!aTarget || !*aTarget)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mTarget != aTarget)
    {
        mTarget = aTarget;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(Lun) (ULONG64 *aLun)
{
    if (!aLun)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aLun = mLun;
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Lun) (ULONG64 aLun)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mLun != aLun)
    {
        mLun = aLun;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(UserName) (BSTR *aUserName)
{
    if (!aUserName)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mUserName.cloneTo (aUserName);
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(UserName) (INPTR BSTR aUserName)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mUserName != aUserName)
    {
        mUserName = aUserName;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMGETTER(Password) (BSTR *aPassword)
{
    if (!aPassword)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mPassword.cloneTo (aPassword);
    return S_OK;
}

STDMETHODIMP HISCSIHardDisk::COMSETTER(Password) (INPTR BSTR aPassword)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    if (mPassword != aPassword)
    {
        mPassword = aPassword;
        if (mRegistered)
            return mVirtualBox->saveSettings();
    }

    return S_OK;
}

// public/protected methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Attempts to mark the hard disk as registered.
 *  Only VirtualBox can call this method.
 */
HRESULT HISCSIHardDisk::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        if (mServer.isEmpty() || mTarget.isEmpty())
            return setError (E_FAIL,
                tr ("iSCSI Hard disk has no server or target defined"));
    }
    else
    {
    }

    return HardDisk::trySetRegistered (aRegistered);
}

/**
 *  Checks accessibility of this iSCSI hard disk.
 */
HRESULT HISCSIHardDisk::getAccessible (Bstr &aAccessError)
{
    AutoLock alock (this);
    CHECK_READY();

    /* check the basic accessibility */
    HRESULT rc = getBaseAccessible (aAccessError);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    return queryInformation (aAccessError);
}

/**
 *  Saves hard disk settings to the specified storage node and saves
 *  all children to the specified hard disk node
 *
 *  @param aHDNode      <HardDisk>.
 *  @param aStorageNode <ISCSIHardDisk> node.
 */
HRESULT HISCSIHardDisk::saveSettings (settings::Key &aHDNode,
                                      settings::Key &aStorageNode)
{
    AssertReturn (!aHDNode.isNull() && !aStorageNode.isNull(), E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    /* server (required) */
    aStorageNode.setValue <Bstr> ("server", mServer);
    /* target (required) */
    aStorageNode.setValue <Bstr> ("target", mTarget);

    /* port (optional, defaults to 0) */
    aStorageNode.setValueOr <USHORT> ("port", mPort, 0);
    /* lun (optional, force 0x format to conform to XML Schema!) */
    aStorageNode.setValueOr <ULONG64> ("lun", mLun, 0, 16);
    /* userName (optional) */
    aStorageNode.setValueOr <Bstr> ("userName", mUserName, Bstr::Null);
    /* password (optional) */
    aStorageNode.setValueOr <Bstr> ("password", mPassword, Bstr::Null);

    /* save basic settings and children */
    return HardDisk::saveSettings (aHDNode);
}

/**
 *  Returns the string representation of this hard disk.
 *  When \a aShort is false, returns the full image file path.
 *  Otherwise, returns the image file name only.
 *
 *  @param aShort       if true, a short representation is returned
 */
Bstr HISCSIHardDisk::toString (bool aShort /* = false */)
{
    AutoLock alock (this);

    Bstr str;
    if (!aShort)
    {
        /* the format is iscsi:[<user@>]<server>[:<port>]/<target>[:<lun>] */
        str = Utf8StrFmt ("iscsi:%s%ls%s/%ls%s",
            !mUserName.isNull() ? Utf8StrFmt ("%ls@", mUserName.raw()).raw() : "",
            mServer.raw(),
            mPort != 0 ? Utf8StrFmt (":%hu", mPort).raw() : "",
            mTarget.raw(),
            mLun != 0 ? Utf8StrFmt (":%llu", mLun).raw() : "");
    }
    else
    {
        str = Utf8StrFmt ("%ls%s",
            mTarget.raw(),
            mLun != 0 ? Utf8StrFmt (":%llu", mLun).raw() : "");
    }

    return str;
}

/**
 *  Creates a clone of this hard disk by storing hard disk data in the given
 *  VDI file.
 *
 *  If the operation fails, @a aDeleteTarget will be set to @c true unless the
 *  failure happened because the target file already existed.
 *
 *  @param aId              UUID to assign to the created image.
 *  @param aTargetPath      VDI file where the cloned image is to be to stored.
 *  @param aProgress        progress object to run during operation.
 *  @param aDeleteTarget    Whether it is recommended to delete target on
 *                          failure or not.
 */
HRESULT
HISCSIHardDisk::cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                              Progress *aProgress, bool &aDeleteTarget)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

//    AssertReturn (isBusy() == false, E_FAIL);
//    addReader();
//    releaseReader();
}

/**
 *  Creates a new differencing image for this hard disk with the given
 *  VDI file name.
 *
 *  @param aId          UUID to assign to the created image
 *  @param aTargetPath  VDI file where to store the created differencing image
 *  @param aProgress    progress object to run during operation
 *                      (can be NULL)
 */
HRESULT
HISCSIHardDisk::createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                                 Progress *aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

//    AssertReturn (isBusy() == false, E_FAIL);
//    addReader();
//    releaseReader();
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to query information about the iSCSI hard disk.
 *
 *  @param aAccessError see #getAccessible()
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT HISCSIHardDisk::queryInformation (Bstr &aAccessError)
{
    /// @todo (dmik) query info about this iSCSI disk,
    //  set mSize and mActualSize,
    //  or set aAccessError in case of failure

    aAccessError.setNull();
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HVMDKImage class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HVMDKImage::FinalConstruct()
{
    HRESULT rc = HardDisk::FinalConstruct();
    if (FAILED (rc))
        return rc;

    mState = NotCreated;

    mStateCheckSem = NIL_RTSEMEVENTMULTI;
    mStateCheckWaiters = 0;

    mSize = 0;
    mActualSize = 0;

    /* initialize the container */
    int vrc = VDCreate ("VMDK", VDError, this, &mContainer);
    ComAssertRCRet (vrc, E_FAIL);

    return S_OK;
}

void HVMDKImage::FinalRelease()
{
    if (mContainer != NULL)
        VDDestroy (mContainer);

    HardDisk::FinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VMDK hard disk object by reading its properties from
 *  the given configuration node. The created hard disk will be marked as
 *  registered on success.
 *
 *  @param aHDNode      <HardDisk> node.
 *  @param aVMDKNode    <VirtualDiskImage> node.
 */
HRESULT HVMDKImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                          const settings::Key &aHDNode, 
                          const settings::Key &aVMDKNode)
{
    using namespace settings;

    LogFlowThisFunc (("\n"));

    AssertReturn (!aHDNode.isNull() && !aVMDKNode.isNull(), E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VMDKImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* filePath (required) */
        Bstr filePath = aVMDKNode.stringValue ("filePath");
        rc = setFilePath (filePath);
        CheckComRCBreakRC (rc);

        LogFlowThisFunc (("'%ls'\n", mFilePathFull.raw()));

        /* load basic settings and children */
        rc = loadSettings (aHDNode);
        CheckComRCBreakRC (rc);

        if (mType != HardDiskType_WritethroughHardDisk)
        {
            rc = setError (E_FAIL,
                tr ("Currently, non-Writethrough VMDK images are not "
                    "allowed ('%ls')"),
                toString().raw());
            break;
        }

        mState = Created;
        mRegistered = TRUE;

        /* Don't call queryInformation() for registered hard disks to
         * prevent the calling thread (i.e. the VirtualBox server startup
         * thread) from an unexpected freeze. The vital mId property (UUID)
         * is read from the registry file in loadSettings(). To get the rest,
         * the user will have to call COMGETTER(Accessible) manually. */
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Initializes the VMDK hard disk object using the given image file name.
 *
 *  @param aVirtualBox  VirtualBox parent.
 *  @param aParent      Currently, must always be @c NULL.
 *  @param aFilePath    Path to the image file, or @c NULL to create an
 *                      image-less object.
 *  @param aRegistered  Whether to mark this disk as registered or not
 *                      (ignored when @a aFilePath is @c NULL, assuming @c FALSE)
 */
HRESULT HVMDKImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                          const BSTR aFilePath, BOOL aRegistered /* = FALSE */)
{
    LogFlowThisFunc (("aFilePath='%ls', aRegistered=%d\n", aFilePath, aRegistered));

    AssertReturn (aParent == NULL, E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VMDKImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        rc = setFilePath (aFilePath);
        CheckComRCBreakRC (rc);

        /* currently, all VMDK hard disks are writethrough */
        mType = HardDiskType_WritethroughHardDisk;

        Assert (mId.isEmpty());

        if (aFilePath && *aFilePath)
        {
            mRegistered = aRegistered;
            mState = Created;

            /* Call queryInformation() anyway (even if it will block), because
             * it is the only way to get the UUID of the existing VDI and
             * initialize the vital mId property. */
            Bstr errMsg;
            rc = queryInformation (&errMsg);
            if (SUCCEEDED (rc))
            {
                /* We are constructing a new HVirtualDiskImage object. If there
                 * is a fatal accessibility error (we cannot read image UUID),
                 * we have to fail. We do so even on non-fatal errors as well,
                 * because it's not worth to keep going with the inaccessible
                 * image from the very beginning (when nothing else depends on
                 * it yet). */
                if (!errMsg.isNull())
                    rc = setErrorBstr (E_FAIL, errMsg);
            }
        }
        else
        {
            mRegistered = FALSE;
            mState = NotCreated;
            mId.create();
        }
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void HVMDKImage::uninit()
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    if (!isReady())
        return;

    HardDisk::protectedUninit (alock);
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVMDKImage::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP HVMDKImage::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;

/// @todo (r=dmik) implement
//
//     if (mState >= Created)
//     {
//         int vrc = VDISetImageComment (Utf8Str (mFilePathFull), Utf8Str (aDescription));
//         if (VBOX_FAILURE (vrc))
//             return setError (E_FAIL,
//                 tr ("Could not change the description of the VDI hard disk '%ls' "
//                     "(%Vrc)"),
//                 toString().raw(), vrc);
//     }
//
//     mDescription = aDescription;
//     return S_OK;
}

STDMETHODIMP HVMDKImage::COMGETTER(Size) (ULONG64 *aSize)
{
    if (!aSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

/// @todo (r=dmik) will need this if we add support for differencing VMDKs
//
//     /* only a non-differencing image knows the logical size */
//     if (isDifferencing())
//         return root()->COMGETTER(Size) (aSize);

    *aSize = mSize;
    return S_OK;
}

STDMETHODIMP HVMDKImage::COMGETTER(ActualSize) (ULONG64 *aActualSize)
{
    if (!aActualSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActualSize = mActualSize;
    return S_OK;
}

// IVirtualDiskImage properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVMDKImage::COMGETTER(FilePath) (BSTR *aFilePath)
{
    if (!aFilePath)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mFilePathFull.cloneTo (aFilePath);
    return S_OK;
}

STDMETHODIMP HVMDKImage::COMSETTER(FilePath) (INPTR BSTR aFilePath)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mState != NotCreated)
        return setError (E_ACCESSDENIED,
            tr ("Cannot change the file path of the existing hard disk '%ls'"),
            toString().raw());

    CHECK_BUSY_AND_READERS();

    /* append the default path if only a name is given */
    Bstr path = aFilePath;
    if (aFilePath && *aFilePath)
    {
        Utf8Str fp = aFilePath;
        if (!RTPathHavePath (fp))
        {
            AutoReaderLock propsLock (mVirtualBox->systemProperties());
            path = Utf8StrFmt ("%ls%c%s",
                               mVirtualBox->systemProperties()->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    return setFilePath (path);
}

STDMETHODIMP HVMDKImage::COMGETTER(Created) (BOOL *aCreated)
{
    if (!aCreated)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aCreated = mState >= Created;
    return S_OK;
}

// IVMDKImage methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVMDKImage::CreateDynamicImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, TRUE /* aDynamic */, aProgress);
}

STDMETHODIMP HVMDKImage::CreateFixedImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, FALSE /* aDynamic */, aProgress);
}

STDMETHODIMP HVMDKImage::DeleteImage()
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;

/// @todo (r=dmik) later
// We will need to parse the file in order to delete all related delta and
// sparse images etc. We may also want to obey the .vmdk.lck file
// which is (as far as I understood) created when the VMware VM is
// running or saved etc.
//
//     if (mRegistered)
//         return setError (E_ACCESSDENIED,
//              tr ("Cannot delete an image of the registered hard disk image '%ls"),
//              mFilePathFull.raw());
//     if (mState == NotCreated)
//         return setError (E_FAIL,
//              tr ("Hard disk image has been already deleted or never created"));
//
//     int vrc = RTFileDelete (Utf8Str (mFilePathFull));
//     if (VBOX_FAILURE (vrc))
//         return setError (E_FAIL, tr ("Could not delete the image file '%ls' (%Vrc)"),
//                          mFilePathFull.raw(), vrc);
//
//     mState = NotCreated;
//
//     /* reset the fields */
//     mSize = 0;
//     mActualSize = 0;
//
//     return S_OK;
}

// public/protected methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Attempts to mark the hard disk as registered.
 *  Only VirtualBox can call this method.
 */
HRESULT HVMDKImage::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        if (mState == NotCreated)
            return setError (E_FAIL,
                tr ("Image file '%ls' is not yet created for this hard disk"),
                mFilePathFull.raw());

/// @todo (r=dmik) will need this if we add support for differencing VMDKs
//         if (isDifferencing())
//             return setError (E_FAIL,
//                 tr ("Hard disk '%ls' is differencing and cannot be unregistered "
//                     "explicitly"),
//                 mFilePathFull.raw());
    }
    else
    {
        ComAssertRet (mState >= Created, E_FAIL);
    }

    return HardDisk::trySetRegistered (aRegistered);
}

/**
 *  Checks accessibility of this hard disk image only (w/o parents).
 *
 *  @param aAccessError on output, a null string indicates the hard disk is
 *                      accessible, otherwise contains a message describing
 *                      the reason of inaccessibility.
 */
HRESULT HVMDKImage::getAccessible (Bstr &aAccessError)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mStateCheckSem != NIL_RTSEMEVENTMULTI)
    {
        /* An accessibility check in progress on some other thread,
         * wait for it to finish. */

        ComAssertRet (mStateCheckWaiters != (ULONG) ~0, E_FAIL);
        ++ mStateCheckWaiters;
        alock.leave();

        int vrc = RTSemEventMultiWait (mStateCheckSem, RT_INDEFINITE_WAIT);

        alock.enter();
        AssertReturn (mStateCheckWaiters != 0, E_FAIL);
        -- mStateCheckWaiters;
        if (mStateCheckWaiters == 0)
        {
            RTSemEventMultiDestroy (mStateCheckSem);
            mStateCheckSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRCReturn (vrc, E_FAIL);

        /* don't touch aAccessError, it has been already set */
        return S_OK;
    }

    /* check the basic accessibility */
    HRESULT rc = getBaseAccessible (aAccessError, true /* aCheckBusy */);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    if (mState >= Created)
    {
        return queryInformation (&aAccessError);
    }

    aAccessError = Utf8StrFmt ("Hard disk image '%ls' is not yet created",
                               mFilePathFull.raw());
    return S_OK;
}

/**
 *  Saves hard disk settings to the specified storage node and saves
 *  all children to the specified hard disk node
 *
 *  @param aHDNode      <HardDisk> or <DiffHardDisk> node.
 *  @param aStorageNode <VirtualDiskImage> node.
 */
HRESULT HVMDKImage::saveSettings (settings::Key &aHDNode,
                                  settings::Key &aStorageNode)
{
    AssertReturn (!aHDNode.isNull() && !aStorageNode.isNull(), E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    /* filePath (required) */
    aStorageNode.setValue <Bstr> ("filePath", mFilePath);

    /* save basic settings and children */
    return HardDisk::saveSettings (aHDNode);
}

/**
 *  Checks if the given change of \a aOldPath to \a aNewPath affects the path
 *  of this hard disk and updates it if necessary to reflect the new location.
 *  Intended to be from HardDisk::updatePaths().
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object for writing.
 */
void HVMDKImage::updatePath (const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoLock alock (this);
    AssertReturnVoid (isReady());

    size_t oldPathLen = strlen (aOldPath);

    Utf8Str path = mFilePathFull;
    LogFlowThisFunc (("VMDK.fullPath={%s}\n", path.raw()));

    if (RTPathStartsWith (path, aOldPath))
    {
        Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                              path.raw() + oldPathLen);
        path = newPath;

        mVirtualBox->calculateRelativePath (path, path);

        unconst (mFilePathFull) = newPath;
        unconst (mFilePath) = path;

        LogFlowThisFunc (("-> updated: full={%s} short={%s}\n",
                          newPath.raw(), path.raw()));
    }
}

/**
 *  Returns the string representation of this hard disk.
 *  When \a aShort is false, returns the full image file path.
 *  Otherwise, returns the image file name only.
 *
 *  @param aShort       if true, a short representation is returned
 */
Bstr HVMDKImage::toString (bool aShort /* = false */)
{
    AutoLock alock (this);

    if (!aShort)
        return mFilePathFull;
    else
    {
        Utf8Str fname = mFilePathFull;
        return RTPathFilename (fname.mutableRaw());
    }
}

/**
 *  Creates a clone of this hard disk by storing hard disk data in the given
 *  VDI file.
 *
 *  If the operation fails, @a aDeleteTarget will be set to @c true unless the
 *  failure happened because the target file already existed.
 *
 *  @param aId              UUID to assign to the created image.
 *  @param aTargetPath      VDI file where the cloned image is to be to stored.
 *  @param aProgress        progress object to run during operation.
 *  @param aDeleteTarget    Whether it is recommended to delete target on
 *                          failure or not.
 */
HRESULT
HVMDKImage::cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo (r=dmik) will need this if we add support for differencing VMDKs
//  Use code from HVirtualDiskImage::cloneToImage as an example.
}

/**
 *  Creates a new differencing image for this hard disk with the given
 *  VDI file name.
 *
 *  @param aId          UUID to assign to the created image
 *  @param aTargetPath  VDI file where to store the created differencing image
 *  @param aProgress    progress object to run during operation
 *                      (can be NULL)
 */
HRESULT
HVMDKImage::createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo (r=dmik) will need this if we add support for differencing VMDKs
//  Use code from HVirtualDiskImage::createDiffImage as an example.
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to set a new file path.
 *  Resolves a path relatively to the Virtual Box home directory.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT HVMDKImage::setFilePath (const BSTR aFilePath)
{
    if (aFilePath && *aFilePath)
    {
        /* get the full file name */
        char filePathFull [RTPATH_MAX];
        int vrc = RTPathAbsEx (mVirtualBox->homeDir(), Utf8Str (aFilePath),
                               filePathFull, sizeof (filePathFull));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Invalid image file path '%ls' (%Vrc)"), aFilePath, vrc);

        mFilePath = aFilePath;
        mFilePathFull = filePathFull;
    }
    else
    {
        mFilePath.setNull();
        mFilePathFull.setNull();
    }

    return S_OK;
}

/**
 *  Helper to query information about the VDI hard disk.
 *
 *  @param aAccessError not used when NULL, otherwise see #getAccessible()
 *
 *  @note Must be called from under the object's lock, only after
 *        CHECK_BUSY_AND_READERS() succeeds.
 */
HRESULT HVMDKImage::queryInformation (Bstr *aAccessError)
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    /* create a lock object to completely release it later */
    AutoLock alock (this);

    AssertReturn (mStateCheckWaiters == 0, E_FAIL);

    ComAssertRet (mState >= Created, E_FAIL);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* lazily create a semaphore */
    vrc = RTSemEventMultiCreate (&mStateCheckSem);
    ComAssertRCRet (vrc, E_FAIL);

    /* Reference the disk to prevent any concurrent modifications
     * after releasing the lock below (to unblock getters before
     * a lengthy operation). */
    addReader();

    alock.leave();

    /* VBoxVHDD management interface needs to be optimized: we're opening a
     * file three times in a raw to get three bits of information. */

    Utf8Str filePath = mFilePathFull;
    Bstr errMsg;

    /* reset any previous error report from VDError() */
    mLastVDError.setNull();

    do
    {
        Guid id, parentId;

        /// @todo changed from VD_OPEN_FLAGS_READONLY to VD_OPEN_FLAGS_NORMAL,
        /// because otherwise registering a VMDK which so far has no UUID will
        /// yield a null UUID. It cannot be added to a VMDK opened readonly,
        /// obviously. This of course changes locking behavior, but for now
        /// this is acceptable. A better solution needs to be found later.
        vrc = VDOpen (mContainer, filePath, VD_OPEN_FLAGS_NORMAL);
        if (VBOX_FAILURE (vrc))
            break;

        vrc = VDGetUuid (mContainer, 0, id.ptr());
        if (VBOX_FAILURE (vrc))
            break;
        vrc = VDGetParentUuid (mContainer, 0, parentId.ptr());
        if (VBOX_FAILURE (vrc))
            break;

        if (!mId.isEmpty())
        {
            /* check that the actual UUID of the image matches the stored UUID */
            if (mId != id)
            {
                errMsg = Utf8StrFmt (
                    tr ("Actual UUID {%Vuuid} of the hard disk image '%s' doesn't "
                        "match UUID {%Vuuid} stored in the registry"),
                    id.ptr(), filePath.raw(), mId.ptr());
                break;
            }
        }
        else
        {
            /* assgn an UUID read from the image file */
            mId = id;
        }

        if (mParent)
        {
            /* check parent UUID */
            AutoLock parentLock (mParent);
            if (mParent->id() != parentId)
            {
                errMsg = Utf8StrFmt (
                    tr ("UUID {%Vuuid} of the parent image '%ls' stored in "
                        "the hard disk image file '%s' doesn't match "
                        "UUID {%Vuuid} stored in the registry"),
                    parentId.raw(), mParent->toString().raw(),
                    filePath.raw(), mParent->id().raw());
                break;
            }
        }
        else if (!parentId.isEmpty())
        {
            errMsg = Utf8StrFmt (
                tr ("Hard disk image '%s' is a differencing image that is linked "
                    "to a hard disk with UUID {%Vuuid} and cannot be used "
                    "directly as a base hard disk"),
                filePath.raw(), parentId.raw());
            break;
        }

        /* get actual file size */
        /// @todo is there a direct method in RT?
        {
            RTFILE file = NIL_RTFILE;
            vrc = RTFileOpen (&file, filePath, RTFILE_O_READ);
            if (VBOX_SUCCESS (vrc))
            {
                uint64_t size = 0;
                vrc = RTFileGetSize (file, &size);
                if (VBOX_SUCCESS (vrc))
                    mActualSize = size;
                RTFileClose (file);
            }
            if (VBOX_FAILURE (vrc))
                break;
        }

        /* query logical size only for non-differencing images */
        if (!mParent)
        {
            uint64_t size = VDGetSize (mContainer, 0);
            /* convert to MBytes */
            mSize = size / 1024 / 1024;
        }
    }
    while (0);

    VDCloseAll (mContainer);

    /* enter the lock again */
    alock.enter();

    /* remove the reference */
    releaseReader();

    if (FAILED (rc) || VBOX_FAILURE (vrc) || !errMsg.isNull())
    {
        LogWarningFunc (("'%ls' is not accessible "
                         "(rc=%08X, vrc=%Vrc, errMsg='%ls', mLastVDError='%s')\n",
                         mFilePathFull.raw(), rc, vrc, errMsg.raw(), mLastVDError.raw()));

        if (aAccessError)
        {
            if (!errMsg.isNull())
                *aAccessError = errMsg;
            else if (!mLastVDError.isNull())
                *aAccessError = mLastVDError;
            else if (VBOX_FAILURE (vrc))
                *aAccessError = Utf8StrFmt (
                    tr ("Could not access hard disk image '%ls' (%Vrc)"),
                        mFilePathFull.raw(), vrc);
        }

        /* downgrade to not accessible */
        mState = Created;
    }
    else
    {
        if (aAccessError)
            aAccessError->setNull();

        mState = Accessible;
    }

    /* inform waiters if there are any */
    if (mStateCheckWaiters > 0)
    {
        RTSemEventMultiSignal (mStateCheckSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (mStateCheckSem);
        mStateCheckSem = NIL_RTSEMEVENTMULTI;
    }

    /* cleanup the last error report from VDError() */
    mLastVDError.setNull();

    return rc;
}

/**
 *  Helper to create hard disk images.
 *
 *  @param aSize        size in MB
 *  @param aDynamic     dynamic or fixed image
 *  @param aProgress    address of IProgress pointer to return
 */
HRESULT HVMDKImage::createImage (ULONG64 aSize, BOOL aDynamic,
                                 IProgress **aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo (r=dmik) later
//  Use code from HVirtualDiskImage::createImage as an example.
}

/* static */
DECLCALLBACK(int) HVMDKImage::VDITaskThread (RTTHREAD thread, void *pvUser)
{
    AssertMsgFailed (("Not implemented"));
    return VERR_GENERAL_FAILURE;

/// @todo (r=dmik) later
//  Use code from HVirtualDiskImage::VDITaskThread as an example.
}

/* static */
DECLCALLBACK(void) HVMDKImage::VDError (void *pvUser, int rc, RT_SRC_POS_DECL,
                                        const char *pszFormat, va_list va)
{
    HVMDKImage *that = static_cast <HVMDKImage *> (pvUser);
    AssertReturnVoid (that != NULL);

    /// @todo pass the error message to the operation initiator
    Utf8Str err = Utf8StrFmtVA (pszFormat, va);
    if (VBOX_FAILURE (rc))
        err = Utf8StrFmt ("%s (%Vrc)", err.raw(), rc);

    if (that->mLastVDError.isNull())
        that->mLastVDError = err;
    else
        that->mLastVDError = Utf8StrFmt
            ("%s.\n%s", that->mLastVDError.raw(), err.raw());
}

////////////////////////////////////////////////////////////////////////////////
// HCustomHardDisk class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HCustomHardDisk::FinalConstruct()
{
    HRESULT rc = HardDisk::FinalConstruct();
    if (FAILED (rc))
        return rc;

    mState = NotCreated;

    mStateCheckSem = NIL_RTSEMEVENTMULTI;
    mStateCheckWaiters = 0;

    mSize = 0;
    mActualSize = 0;
    mContainer = NULL;

    ComAssertRCRet (rc, E_FAIL);

    return S_OK;
}

void HCustomHardDisk::FinalRelease()
{
    if (mContainer != NULL)
        VDDestroy (mContainer);

    HardDisk::FinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the custom hard disk object by reading its properties from
 *  the given configuration node. The created hard disk will be marked as
 *  registered on success.
 *
 *  @param aHDNode      <HardDisk> node.
 *  @param aCustomNode  <VirtualDiskImage> node.
 */
HRESULT HCustomHardDisk::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                               const settings::Key &aHDNode,
                               const settings::Key &aCustomNode)
{
    using namespace settings;

    LogFlowThisFunc (("\n"));

    AssertReturn (!aHDNode.isNull() && !aCustomNode.isNull(), E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_CustomHardDisk;

    HRESULT rc = S_OK;
    int     vrc = VINF_SUCCESS;
    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* location (required) */
        Bstr location = aCustomNode.stringValue ("location");
        rc = setLocation (location);
        CheckComRCBreakRC (rc);

        LogFlowThisFunc (("'%ls'\n", mLocationFull.raw()));

        /* format (required) */
        mFormat = aCustomNode.stringValue ("format");

        /* initialize the container */
        vrc = VDCreate (Utf8Str (mFormat), VDError, this, &mContainer);
        if (VBOX_FAILURE (vrc))
        {
            AssertRC (vrc);
            if (mLastVDError.isEmpty())
                rc = setError (E_FAIL,
                    tr ("Unknown format '%ls' of the custom "
                        "hard disk '%ls' (%Vrc)"),
                    mFormat.raw(), toString().raw(), vrc);
            else
                rc = setErrorBstr (E_FAIL, mLastVDError);
            break;
        }

        /* load basic settings and children */
        rc = loadSettings (aHDNode);
        CheckComRCBreakRC (rc);

        if (mType != HardDiskType_WritethroughHardDisk)
        {
            rc = setError (E_FAIL,
                tr ("Currently, non-Writethrough custom hard disks "
                    "are not allowed ('%ls')"),
                toString().raw());
            break;
        }

        mState = Created;
        mRegistered = TRUE;

        /* Don't call queryInformation() for registered hard disks to
         * prevent the calling thread (i.e. the VirtualBox server startup
         * thread) from an unexpected freeze. The vital mId property (UUID)
         * is read from the registry file in loadSettings(). To get the rest,
         * the user will have to call COMGETTER(Accessible) manually. */
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Initializes the custom hard disk object using the given image file name.
 *
 *  @param aVirtualBox  VirtualBox parent.
 *  @param aParent      Currently, must always be @c NULL.
 *  @param aLocation    Location of the virtual disk, or @c NULL to create an
 *                      image-less object.
 *  @param aRegistered  Whether to mark this disk as registered or not
 *                      (ignored when @a aLocation is @c NULL, assuming @c FALSE)
 */
HRESULT HCustomHardDisk::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                               const BSTR aLocation, BOOL aRegistered /* = FALSE */)
{
    LogFlowThisFunc (("aLocation='%ls', aRegistered=%d\n", aLocation, aRegistered));

    AssertReturn (aParent == NULL, E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_CustomHardDisk;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        rc = setLocation (aLocation);
        CheckComRCBreakRC (rc);

        /* currently, all custom hard disks are writethrough */
        mType = HardDiskType_WritethroughHardDisk;

        Assert (mId.isEmpty());

        if (aLocation && *aLocation)
        {
            mRegistered = aRegistered;
            mState = Created;

            char *pszFormat = NULL;

            int vrc = VDGetFormat (Utf8Str (mLocation), &pszFormat);
            if (VBOX_FAILURE(vrc))
            {
                AssertRC (vrc);
                rc = setError (E_FAIL,
                    tr ("Cannot recognize the format of the custom "
                        "hard disk '%ls' (%Vrc)"),
                    toString().raw(), vrc);
                break;
            }

            /* Create the corresponding container. */
            vrc = VDCreate (pszFormat, VDError, this, &mContainer);

            if (VBOX_SUCCESS(vrc))
                mFormat = Bstr (pszFormat);

            RTStrFree (pszFormat);

            /* the format has been already checked for presence at this point */
            ComAssertRCBreak (vrc, rc = E_FAIL);

            /* Call queryInformation() anyway (even if it will block), because
             * it is the only way to get the UUID of the existing VDI and
             * initialize the vital mId property. */
            Bstr errMsg;
            rc = queryInformation (&errMsg);
            if (SUCCEEDED (rc))
            {
                /* We are constructing a new HVirtualDiskImage object. If there
                 * is a fatal accessibility error (we cannot read image UUID),
                 * we have to fail. We do so even on non-fatal errors as well,
                 * because it's not worth to keep going with the inaccessible
                 * image from the very beginning (when nothing else depends on
                 * it yet). */
                if (!errMsg.isNull())
                    rc = setErrorBstr (E_FAIL, errMsg);
            }
        }
        else
        {
            mRegistered = FALSE;
            mState = NotCreated;
            mId.create();
        }
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void HCustomHardDisk::uninit()
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    if (!isReady())
        return;

    HardDisk::protectedUninit (alock);
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HCustomHardDisk::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP HCustomHardDisk::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;
}

STDMETHODIMP HCustomHardDisk::COMGETTER(Size) (ULONG64 *aSize)
{
    if (!aSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aSize = mSize;
    return S_OK;
}

STDMETHODIMP HCustomHardDisk::COMGETTER(ActualSize) (ULONG64 *aActualSize)
{
    if (!aActualSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActualSize = mActualSize;
    return S_OK;
}

// ICustomHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HCustomHardDisk::COMGETTER(Location) (BSTR *aLocation)
{
    if (!aLocation)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mLocationFull.cloneTo (aLocation);
    return S_OK;
}

STDMETHODIMP HCustomHardDisk::COMSETTER(Location) (INPTR BSTR aLocation)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mState != NotCreated)
        return setError (E_ACCESSDENIED,
            tr ("Cannot change the file path of the existing hard disk '%ls'"),
            toString().raw());

    CHECK_BUSY_AND_READERS();

    /// @todo currently, we assume that location is always a file path for
    /// all custom hard disks. This is not generally correct, and needs to be
    /// parametrized in the VD plugin interface.

    /* append the default path if only a name is given */
    Bstr path = aLocation;
    if (aLocation && *aLocation)
    {
        Utf8Str fp = aLocation;
        if (!RTPathHavePath (fp))
        {
            AutoReaderLock propsLock (mVirtualBox->systemProperties());
            path = Utf8StrFmt ("%ls%c%s",
                               mVirtualBox->systemProperties()->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    return setLocation (path);
}

STDMETHODIMP HCustomHardDisk::COMGETTER(Created) (BOOL *aCreated)
{
    if (!aCreated)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aCreated = mState >= Created;
    return S_OK;
}

STDMETHODIMP HCustomHardDisk::COMGETTER(Format) (BSTR *aFormat)
{
    if (!aFormat)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mFormat.cloneTo (aFormat);
    return S_OK;
}

// ICustomHardDisk methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HCustomHardDisk::CreateDynamicImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, TRUE /* aDynamic */, aProgress);
}

STDMETHODIMP HCustomHardDisk::CreateFixedImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, FALSE /* aDynamic */, aProgress);
}

STDMETHODIMP HCustomHardDisk::DeleteImage()
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;

/// @todo later
}

// public/protected methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Attempts to mark the hard disk as registered.
 *  Only VirtualBox can call this method.
 */
HRESULT HCustomHardDisk::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        if (mState == NotCreated)
            return setError (E_FAIL,
                tr ("Storage location '%ls' is not yet created for this hard disk"),
                mLocationFull.raw());
    }
    else
    {
        ComAssertRet (mState >= Created, E_FAIL);
    }

    return HardDisk::trySetRegistered (aRegistered);
}

/**
 *  Checks accessibility of this hard disk image only (w/o parents).
 *
 *  @param aAccessError on output, a null string indicates the hard disk is
 *                      accessible, otherwise contains a message describing
 *                      the reason of inaccessibility.
 */
HRESULT HCustomHardDisk::getAccessible (Bstr &aAccessError)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mStateCheckSem != NIL_RTSEMEVENTMULTI)
    {
        /* An accessibility check in progress on some other thread,
         * wait for it to finish. */

        ComAssertRet (mStateCheckWaiters != (ULONG) ~0, E_FAIL);
        ++ mStateCheckWaiters;
        alock.leave();

        int vrc = RTSemEventMultiWait (mStateCheckSem, RT_INDEFINITE_WAIT);

        alock.enter();
        AssertReturn (mStateCheckWaiters != 0, E_FAIL);
        -- mStateCheckWaiters;
        if (mStateCheckWaiters == 0)
        {
            RTSemEventMultiDestroy (mStateCheckSem);
            mStateCheckSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRCReturn (vrc, E_FAIL);

        /* don't touch aAccessError, it has been already set */
        return S_OK;
    }

    /* check the basic accessibility */
    HRESULT rc = getBaseAccessible (aAccessError, true /* aCheckBusy */);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    if (mState >= Created)
    {
        return queryInformation (&aAccessError);
    }

    aAccessError = Utf8StrFmt ("Hard disk '%ls' is not yet created",
                               mLocationFull.raw());
    return S_OK;
}

/**
 *  Saves hard disk settings to the specified storage node and saves
 *  all children to the specified hard disk node
 *
 *  @param aHDNode      <HardDisk> or <DiffHardDisk> node.
 *  @param aStorageNode <VirtualDiskImage> node.
 */
HRESULT HCustomHardDisk::saveSettings (settings::Key &aHDNode,
                                       settings::Key &aStorageNode)
{
    AssertReturn (!aHDNode.isNull() && !aStorageNode.isNull(), E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    /* location (required) */
    aStorageNode.setValue <Bstr> ("location", mLocationFull);

    /* format (required) */
    aStorageNode.setValue <Bstr> ("format", mFormat);

    /* save basic settings and children */
    return HardDisk::saveSettings (aHDNode);
}

/**
 *  Returns the string representation of this hard disk.
 *  When \a aShort is false, returns the full image file path.
 *  Otherwise, returns the image file name only.
 *
 *  @param aShort       if true, a short representation is returned
 */
Bstr HCustomHardDisk::toString (bool aShort /* = false */)
{
    AutoLock alock (this);

    /// @todo currently, we assume that location is always a file path for
    /// all custom hard disks. This is not generally correct, and needs to be
    /// parametrized in the VD plugin interface.

    if (!aShort)
        return mLocationFull;
    else
    {
        Utf8Str fname = mLocationFull;
        return RTPathFilename (fname.mutableRaw());
    }
}

/**
 *  Creates a clone of this hard disk by storing hard disk data in the given
 *  VDI file.
 *
 *  If the operation fails, @a aDeleteTarget will be set to @c true unless the
 *  failure happened because the target file already existed.
 *
 *  @param aId              UUID to assign to the created image.
 *  @param aTargetPath      VDI file where the cloned image is to be to stored.
 *  @param aProgress        progress object to run during operation.
 *  @param aDeleteTarget    Whether it is recommended to delete target on
 *                          failure or not.
 */
HRESULT
HCustomHardDisk::cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;
}

/**
 *  Creates a new differencing image for this hard disk with the given
 *  VDI file name.
 *
 *  @param aId          UUID to assign to the created image
 *  @param aTargetPath  VDI file where to store the created differencing image
 *  @param aProgress    progress object to run during operation
 *                      (can be NULL)
 */
HRESULT
HCustomHardDisk::createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                                  Progress *aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to set a new location.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT HCustomHardDisk::setLocation (const BSTR aLocation)
{
    /// @todo currently, we assume that location is always a file path for
    /// all custom hard disks. This is not generally correct, and needs to be
    /// parametrized in the VD plugin interface.

    if (aLocation && *aLocation)
    {
        /* get the full file name */
        char locationFull [RTPATH_MAX];
        int vrc = RTPathAbsEx (mVirtualBox->homeDir(), Utf8Str (aLocation),
                               locationFull, sizeof (locationFull));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Invalid hard disk location '%ls' (%Vrc)"), aLocation, vrc);

        mLocation = aLocation;
        mLocationFull = locationFull;
    }
    else
    {
        mLocation.setNull();
        mLocationFull.setNull();
    }

    return S_OK;
}

/**
 *  Helper to query information about the custom hard disk.
 *
 *  @param aAccessError not used when NULL, otherwise see #getAccessible()
 *
 *  @note Must be called from under the object's lock, only after
 *        CHECK_BUSY_AND_READERS() succeeds.
 */
HRESULT HCustomHardDisk::queryInformation (Bstr *aAccessError)
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    /* create a lock object to completely release it later */
    AutoLock alock (this);

    AssertReturn (mStateCheckWaiters == 0, E_FAIL);

    ComAssertRet (mState >= Created, E_FAIL);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* lazily create a semaphore */
    vrc = RTSemEventMultiCreate (&mStateCheckSem);
    ComAssertRCRet (vrc, E_FAIL);

    /* Reference the disk to prevent any concurrent modifications
     * after releasing the lock below (to unblock getters before
     * a lengthy operation). */
    addReader();

    alock.leave();

    /* VBoxVHDD management interface needs to be optimized: we're opening a
     * file three times in a raw to get three bits of information. */

    Utf8Str location = mLocationFull;
    Bstr errMsg;

    /* reset any previous error report from VDError() */
    mLastVDError.setNull();

    do
    {
        Guid id, parentId;

        vrc = VDOpen (mContainer, location, VD_OPEN_FLAGS_INFO);
        if (VBOX_FAILURE (vrc))
            break;

        vrc = VDGetUuid (mContainer, 0, id.ptr());
        if (VBOX_FAILURE (vrc))
            break;
        vrc = VDGetParentUuid (mContainer, 0, parentId.ptr());
        if (VBOX_FAILURE (vrc))
            break;

        if (!mId.isEmpty())
        {
            /* check that the actual UUID of the image matches the stored UUID */
            if (mId != id)
            {
                errMsg = Utf8StrFmt (
                    tr ("Actual UUID {%Vuuid} of the hard disk image '%s' doesn't "
                        "match UUID {%Vuuid} stored in the registry"),
                        id.ptr(), location.raw(), mId.ptr());
                break;
            }
        }
        else
        {
            /* assgn an UUID read from the image file */
            mId = id;
        }

        if (mParent)
        {
            /* check parent UUID */
            AutoLock parentLock (mParent);
            if (mParent->id() != parentId)
            {
                errMsg = Utf8StrFmt (
                    tr ("UUID {%Vuuid} of the parent image '%ls' stored in "
                        "the hard disk image file '%s' doesn't match "
                        "UUID {%Vuuid} stored in the registry"),
                    parentId.raw(), mParent->toString().raw(),
                    location.raw(), mParent->id().raw());
                break;
            }
        }
        else if (!parentId.isEmpty())
        {
            errMsg = Utf8StrFmt (
                tr ("Hard disk image '%s' is a differencing image that is linked "
                    "to a hard disk with UUID {%Vuuid} and cannot be used "
                    "directly as a base hard disk"),
                location.raw(), parentId.raw());
            break;
        }

        /* get actual file size */
        /// @todo is there a direct method in RT?
        {
            RTFILE file = NIL_RTFILE;
            vrc = RTFileOpen (&file, location, RTFILE_O_READ);
            if (VBOX_SUCCESS (vrc))
            {
                uint64_t size = 0;
                vrc = RTFileGetSize (file, &size);
                if (VBOX_SUCCESS (vrc))
                    mActualSize = size;
                RTFileClose (file);
            }
            if (VBOX_FAILURE (vrc))
                break;
        }

        /* query logical size only for non-differencing images */
        if (!mParent)
        {
            uint64_t size = VDGetSize (mContainer, 0);
            /* convert to MBytes */
            mSize = size / 1024 / 1024;
        }
    }
    while (0);

    VDCloseAll (mContainer);

    /* enter the lock again */
    alock.enter();

    /* remove the reference */
    releaseReader();

    if (FAILED (rc) || VBOX_FAILURE (vrc) || !errMsg.isNull())
    {
        LogWarningFunc (("'%ls' is not accessible "
                         "(rc=%08X, vrc=%Vrc, errMsg='%ls')\n",
                         mLocationFull.raw(), rc, vrc, errMsg.raw()));

        if (aAccessError)
        {
            if (!errMsg.isNull())
                *aAccessError = errMsg;
            else if (!mLastVDError.isNull())
                *aAccessError = mLastVDError;
            else if (VBOX_FAILURE (vrc))
                *aAccessError = Utf8StrFmt (
                    tr ("Could not access hard disk '%ls' (%Vrc)"),
                        mLocationFull.raw(), vrc);
        }

        /* downgrade to not accessible */
        mState = Created;
    }
    else
    {
        if (aAccessError)
            aAccessError->setNull();

        mState = Accessible;
    }

    /* inform waiters if there are any */
    if (mStateCheckWaiters > 0)
    {
        RTSemEventMultiSignal (mStateCheckSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (mStateCheckSem);
        mStateCheckSem = NIL_RTSEMEVENTMULTI;
    }

    /* cleanup the last error report from VDError() */
    mLastVDError.setNull();

    return rc;
}

/**
 *  Helper to create hard disk images.
 *
 *  @param aSize        size in MB
 *  @param aDynamic     dynamic or fixed image
 *  @param aProgress    address of IProgress pointer to return
 */
HRESULT HCustomHardDisk::createImage (ULONG64 aSize, BOOL aDynamic,
                                      IProgress **aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;
}

/* static */
DECLCALLBACK(int) HCustomHardDisk::VDITaskThread (RTTHREAD thread, void *pvUser)
{
    AssertMsgFailed (("Not implemented"));
    return VERR_GENERAL_FAILURE;
}

/* static */
DECLCALLBACK(void) HCustomHardDisk::VDError (void *pvUser, int rc, RT_SRC_POS_DECL,
                                             const char *pszFormat, va_list va)
{
    HCustomHardDisk *that = static_cast <HCustomHardDisk *> (pvUser);
    AssertReturnVoid (that != NULL);

    /// @todo pass the error message to the operation initiator
    Utf8Str err = Utf8StrFmt (pszFormat, va);
    if (VBOX_FAILURE (rc))
        err = Utf8StrFmt ("%s (%Vrc)", err.raw(), rc);

    if (that->mLastVDError.isNull())
        that->mLastVDError = err;
    else
       that->mLastVDError = Utf8StrFmt
           ("%s.\n%s", that->mLastVDError.raw(), err.raw());
}

////////////////////////////////////////////////////////////////////////////////
// HVHDImage class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HVHDImage::FinalConstruct()
{
    HRESULT rc = HardDisk::FinalConstruct();
    if (FAILED (rc))
        return rc;

    mState = NotCreated;

    mStateCheckSem = NIL_RTSEMEVENTMULTI;
    mStateCheckWaiters = 0;

    mSize = 0;
    mActualSize = 0;

    /* initialize the container */
    int vrc = VDCreate ("VHD", VDError, this, &mContainer);
    ComAssertRCRet (vrc, E_FAIL);

    return S_OK;
}

void HVHDImage::FinalRelease()
{
    if (mContainer != NULL)
        VDDestroy (mContainer);

    HardDisk::FinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// public methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VHD hard disk object by reading its properties from
 *  the given configuration node. The created hard disk will be marked as
 *  registered on success.
 *
 *  @param aHDNode      <HardDisk> node
 *  @param aVHDNode    <VirtualDiskImage> node
 */
HRESULT HVHDImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                         const settings::Key &aHDNode, 
                         const settings::Key &aVHDNode)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (!aHDNode.isNull() && !aVHDNode.isNull(), E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VHDImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        /* filePath (required) */
        Bstr filePath = aVHDNode.stringValue("filePath");

        rc = setFilePath (filePath);
        CheckComRCBreakRC (rc);

        LogFlowThisFunc (("'%ls'\n", mFilePathFull.raw()));

        /* load basic settings and children */
        rc = loadSettings (aHDNode);
        CheckComRCBreakRC (rc);

        if (mType != HardDiskType_WritethroughHardDisk)
        {
            rc = setError (E_FAIL,
                tr ("Currently, non-Writethrough VHD images are not "
                    "allowed ('%ls')"),
                toString().raw());
            break;
        }

        mState = Created;
        mRegistered = TRUE;

        /* Don't call queryInformation() for registered hard disks to
         * prevent the calling thread (i.e. the VirtualBox server startup
         * thread) from an unexpected freeze. The vital mId property (UUID)
         * is read from the registry file in loadSettings(). To get the rest,
         * the user will have to call COMGETTER(Accessible) manually. */
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Initializes the VHD hard disk object using the given image file name.
 *
 *  @param aVirtualBox  VirtualBox parent.
 *  @param aParent      Currently, must always be @c NULL.
 *  @param aFilePath    Path to the image file, or @c NULL to create an
 *                      image-less object.
 *  @param aRegistered  Whether to mark this disk as registered or not
 *                      (ignored when @a aFilePath is @c NULL, assuming @c FALSE)
 */
HRESULT HVHDImage::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                          const BSTR aFilePath, BOOL aRegistered /* = FALSE */)
{
    LogFlowThisFunc (("aFilePath='%ls', aRegistered=%d\n", aFilePath, aRegistered));

    AssertReturn (aParent == NULL, E_FAIL);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mStorageType = HardDiskStorageType_VHDImage;

    HRESULT rc = S_OK;

    do
    {
        rc = protectedInit (aVirtualBox, aParent);
        CheckComRCBreakRC (rc);

        /* set ready to let protectedUninit() be called on failure */
        setReady (true);

        rc = setFilePath (aFilePath);
        CheckComRCBreakRC (rc);

        /* currently, all VHD hard disks are writethrough */
        mType = HardDiskType_WritethroughHardDisk;

        Assert (mId.isEmpty());

        if (aFilePath && *aFilePath)
        {
            mRegistered = aRegistered;
            mState = Created;

            /* Call queryInformation() anyway (even if it will block), because
             * it is the only way to get the UUID of the existing VDI and
             * initialize the vital mId property. */
            Bstr errMsg;
            rc = queryInformation (&errMsg);
            if (SUCCEEDED (rc))
            {
                /* We are constructing a new HVirtualDiskImage object. If there
                 * is a fatal accessibility error (we cannot read image UUID),
                 * we have to fail. We do so even on non-fatal errors as well,
                 * because it's not worth to keep going with the inaccessible
                 * image from the very beginning (when nothing else depends on
                 * it yet). */
                if (!errMsg.isNull())
                    rc = setErrorBstr (E_FAIL, errMsg);
            }
        }
        else
        {
            mRegistered = FALSE;
            mState = NotCreated;
            mId.create();
        }
    }
    while (0);

    if (FAILED (rc))
        uninit();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void HVHDImage::uninit()
{
    LogFlowThisFunc (("\n"));

    AutoLock alock (this);
    if (!isReady())
        return;

    HardDisk::protectedUninit (alock);
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVHDImage::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP HVHDImage::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;

/// @todo implement
//
//     if (mState >= Created)
//     {
//         int vrc = VDISetImageComment (Utf8Str (mFilePathFull), Utf8Str (aDescription));
//         if (VBOX_FAILURE (vrc))
//             return setError (E_FAIL,
//                 tr ("Could not change the description of the VDI hard disk '%ls' "
//                     "(%Vrc)"),
//                 toString().raw(), vrc);
//     }
//
//     mDescription = aDescription;
//     return S_OK;
}

STDMETHODIMP HVHDImage::COMGETTER(Size) (ULONG64 *aSize)
{
    if (!aSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

/// @todo will need this if we add suppord for differencing VMDKs
//
//     /* only a non-differencing image knows the logical size */
//     if (isDifferencing())
//         return root()->COMGETTER(Size) (aSize);

    *aSize = mSize;
    return S_OK;
}

STDMETHODIMP HVHDImage::COMGETTER(ActualSize) (ULONG64 *aActualSize)
{
    if (!aActualSize)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActualSize = mActualSize;
    return S_OK;
}

// IVirtualDiskImage properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVHDImage::COMGETTER(FilePath) (BSTR *aFilePath)
{
    if (!aFilePath)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mFilePathFull.cloneTo (aFilePath);
    return S_OK;
}

STDMETHODIMP HVHDImage::COMSETTER(FilePath) (INPTR BSTR aFilePath)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mState != NotCreated)
        return setError (E_ACCESSDENIED,
            tr ("Cannot change the file path of the existing hard disk '%ls'"),
            toString().raw());

    CHECK_BUSY_AND_READERS();

    /* append the default path if only a name is given */
    Bstr path = aFilePath;
    if (aFilePath && *aFilePath)
    {
        Utf8Str fp = aFilePath;
        if (!RTPathHavePath (fp))
        {
            AutoReaderLock propsLock (mVirtualBox->systemProperties());
            path = Utf8StrFmt ("%ls%c%s",
                               mVirtualBox->systemProperties()->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    return setFilePath (path);
}

STDMETHODIMP HVHDImage::COMGETTER(Created) (BOOL *aCreated)
{
    if (!aCreated)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aCreated = mState >= Created;
    return S_OK;
}

// IVHDImage methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HVHDImage::CreateDynamicImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, TRUE /* aDynamic */, aProgress);
}

STDMETHODIMP HVHDImage::CreateFixedImage (ULONG64 aSize, IProgress **aProgress)
{
    if (!aProgress)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    return createImage (aSize, FALSE /* aDynamic */, aProgress);
}

STDMETHODIMP HVHDImage::DeleteImage()
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_BUSY_AND_READERS();

    return E_NOTIMPL;

/// @todo later
// We will need to parse the file in order to delete all related delta and
// sparse images etc. We may also want to obey the .vmdk.lck file
// which is (as far as I understood) created when the VMware VM is
// running or saved etc.
//
//     if (mRegistered)
//         return setError (E_ACCESSDENIED,
//              tr ("Cannot delete an image of the registered hard disk image '%ls"),
//              mFilePathFull.raw());
//     if (mState == NotCreated)
//         return setError (E_FAIL,
//              tr ("Hard disk image has been already deleted or never created"));
//
//     int vrc = RTFileDelete (Utf8Str (mFilePathFull));
//     if (VBOX_FAILURE (vrc))
//         return setError (E_FAIL, tr ("Could not delete the image file '%ls' (%Vrc)"),
//                          mFilePathFull.raw(), vrc);
//
//     mState = NotCreated;
//
//     /* reset the fields */
//     mSize = 0;
//     mActualSize = 0;
//
//     return S_OK;
}

// public/protected methods for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Attempts to mark the hard disk as registered.
 *  Only VirtualBox can call this method.
 */
HRESULT HVHDImage::trySetRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aRegistered)
    {
        if (mState == NotCreated)
            return setError (E_FAIL,
                tr ("Image file '%ls' is not yet created for this hard disk"),
                mFilePathFull.raw());

/// @todo will need this if we add suppord for differencing VHDs
//         if (isDifferencing())
//             return setError (E_FAIL,
//                 tr ("Hard disk '%ls' is differencing and cannot be unregistered "
//                     "explicitly"),
//                 mFilePathFull.raw());
    }
    else
    {
        ComAssertRet (mState >= Created, E_FAIL);
    }

    return HardDisk::trySetRegistered (aRegistered);
}

/**
 *  Checks accessibility of this hard disk image only (w/o parents).
 *
 *  @param aAccessError on output, a null string indicates the hard disk is
 *                      accessible, otherwise contains a message describing
 *                      the reason of inaccessibility.
 */
HRESULT HVHDImage::getAccessible (Bstr &aAccessError)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mStateCheckSem != NIL_RTSEMEVENTMULTI)
    {
        /* An accessibility check in progress on some other thread,
         * wait for it to finish. */

        ComAssertRet (mStateCheckWaiters != (ULONG) ~0, E_FAIL);
        ++ mStateCheckWaiters;
        alock.leave();

        int vrc = RTSemEventMultiWait (mStateCheckSem, RT_INDEFINITE_WAIT);

        alock.enter();
        AssertReturn (mStateCheckWaiters != 0, E_FAIL);
        -- mStateCheckWaiters;
        if (mStateCheckWaiters == 0)
        {
            RTSemEventMultiDestroy (mStateCheckSem);
            mStateCheckSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRCReturn (vrc, E_FAIL);

        /* don't touch aAccessError, it has been already set */
        return S_OK;
    }

    /* check the basic accessibility */
    HRESULT rc = getBaseAccessible (aAccessError, true /* aCheckBusy */);
    if (FAILED (rc) || !aAccessError.isNull())
        return rc;

    if (mState >= Created)
    {
        return queryInformation (&aAccessError);
    }

    aAccessError = Utf8StrFmt ("Hard disk image '%ls' is not yet created",
                               mFilePathFull.raw());
    return S_OK;
}

/**
 *  Saves hard disk settings to the specified storage node and saves
 *  all children to the specified hard disk node
 *
 *  @param aHDNode      <HardDisk> or <DiffHardDisk> node
 *  @param aStorageNode <VirtualDiskImage> node
 */
HRESULT HVHDImage::saveSettings (settings::Key &aHDNode, settings::Key &aStorageNode)
{
    AssertReturn (!aHDNode.isNull() && !aStorageNode.isNull(), E_FAIL);

    AutoLock alock (this);
    CHECK_READY();

    /* filePath (required) */
    aStorageNode.setValue <Bstr> ("filePath", mFilePath);

    /* save basic settings and children */
    return HardDisk::saveSettings (aHDNode);
}

/**
 *  Checks if the given change of \a aOldPath to \a aNewPath affects the path
 *  of this hard disk and updates it if necessary to reflect the new location.
 *  Intended to be from HardDisk::updatePaths().
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object for writing.
 */
void HVHDImage::updatePath (const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoLock alock (this);
    AssertReturnVoid (isReady());

    size_t oldPathLen = strlen (aOldPath);

    Utf8Str path = mFilePathFull;
    LogFlowThisFunc (("VHD.fullPath={%s}\n", path.raw()));

    if (RTPathStartsWith (path, aOldPath))
    {
        Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                              path.raw() + oldPathLen);
        path = newPath;

        mVirtualBox->calculateRelativePath (path, path);

        unconst (mFilePathFull) = newPath;
        unconst (mFilePath) = path;

        LogFlowThisFunc (("-> updated: full={%s} short={%s}\n",
                          newPath.raw(), path.raw()));
    }
}

/**
 *  Returns the string representation of this hard disk.
 *  When \a aShort is false, returns the full image file path.
 *  Otherwise, returns the image file name only.
 *
 *  @param aShort       if true, a short representation is returned
 */
Bstr HVHDImage::toString (bool aShort /* = false */)
{
    AutoLock alock (this);

    if (!aShort)
        return mFilePathFull;
    else
    {
        Utf8Str fname = mFilePathFull;
        return RTPathFilename (fname.mutableRaw());
    }
}

/**
 *  Creates a clone of this hard disk by storing hard disk data in the given
 *  VDI file.
 *
 *  If the operation fails, @a aDeleteTarget will be set to @c true unless the
 *  failure happened because the target file already existed.
 *
 *  @param aId              UUID to assign to the created image.
 *  @param aTargetPath      VDI file where the cloned image is to be to stored.
 *  @param aProgress        progress object to run during operation.
 *  @param aDeleteTarget    Whether it is recommended to delete target on
 *                          failure or not.
 */
HRESULT
HVHDImage::cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo will need this if we add suppord for differencing VHDs
//  Use code from HVirtualDiskImage::cloneToImage as an example.
}

/**
 *  Creates a new differencing image for this hard disk with the given
 *  VDI file name.
 *
 *  @param aId          UUID to assign to the created image
 *  @param aTargetPath  VDI file where to store the created differencing image
 *  @param aProgress    progress object to run during operation
 *                      (can be NULL)
 */
HRESULT
HVHDImage::createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo will need this if we add suppord for differencing VHDs
//  Use code from HVirtualDiskImage::createDiffImage as an example.
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to set a new file path.
 *  Resolves a path relatively to the Virtual Box home directory.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT HVHDImage::setFilePath (const BSTR aFilePath)
{
    if (aFilePath && *aFilePath)
    {
        /* get the full file name */
        char filePathFull [RTPATH_MAX];
        int vrc = RTPathAbsEx (mVirtualBox->homeDir(), Utf8Str (aFilePath),
                               filePathFull, sizeof (filePathFull));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Invalid image file path '%ls' (%Vrc)"), aFilePath, vrc);

        mFilePath = aFilePath;
        mFilePathFull = filePathFull;
    }
    else
    {
        mFilePath.setNull();
        mFilePathFull.setNull();
    }

    return S_OK;
}

/**
 *  Helper to query information about the VDI hard disk.
 *
 *  @param aAccessError not used when NULL, otherwise see #getAccessible()
 *
 *  @note Must be called from under the object's lock, only after
 *        CHECK_BUSY_AND_READERS() succeeds.
 */
HRESULT HVHDImage::queryInformation (Bstr *aAccessError)
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    /* create a lock object to completely release it later */
    AutoLock alock (this);

    AssertReturn (mStateCheckWaiters == 0, E_FAIL);

    ComAssertRet (mState >= Created, E_FAIL);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* lazily create a semaphore */
    vrc = RTSemEventMultiCreate (&mStateCheckSem);
    ComAssertRCRet (vrc, E_FAIL);

    /* Reference the disk to prevent any concurrent modifications
     * after releasing the lock below (to unblock getters before
     * a lengthy operation). */
    addReader();

    alock.leave();

    /* VBoxVHDD management interface needs to be optimized: we're opening a
     * file three times in a raw to get three bits of information. */

    Utf8Str filePath = mFilePathFull;
    Bstr errMsg;

    /* reset any previous error report from VDError() */
    mLastVDError.setNull();

    do
    {
        Guid id, parentId;

        /// @todo changed from VD_OPEN_FLAGS_READONLY to VD_OPEN_FLAGS_NORMAL,
        /// because otherwise registering a VHD which so far has no UUID will
        /// yield a null UUID. It cannot be added to a VHD opened readonly,
        /// obviously. This of course changes locking behavior, but for now
        /// this is acceptable. A better solution needs to be found later.
        vrc = VDOpen (mContainer, filePath, VD_OPEN_FLAGS_NORMAL);
        if (VBOX_FAILURE (vrc))
            break;

        vrc = VDGetUuid (mContainer, 0, id.ptr());
        if (VBOX_FAILURE (vrc))
            break;
        vrc = VDGetParentUuid (mContainer, 0, parentId.ptr());
        if (VBOX_FAILURE (vrc))
            break;

        if (!mId.isEmpty())
        {
            /* check that the actual UUID of the image matches the stored UUID */
            if (mId != id)
            {
                errMsg = Utf8StrFmt (
                    tr ("Actual UUID {%Vuuid} of the hard disk image '%s' doesn't "
                        "match UUID {%Vuuid} stored in the registry"),
                    id.ptr(), filePath.raw(), mId.ptr());
                break;
            }
        }
        else
        {
            /* assgn an UUID read from the image file */
            mId = id;
        }

        if (mParent)
        {
            /* check parent UUID */
            AutoLock parentLock (mParent);
            if (mParent->id() != parentId)
            {
                errMsg = Utf8StrFmt (
                    tr ("UUID {%Vuuid} of the parent image '%ls' stored in "
                        "the hard disk image file '%s' doesn't match "
                        "UUID {%Vuuid} stored in the registry"),
                    parentId.raw(), mParent->toString().raw(),
                    filePath.raw(), mParent->id().raw());
                break;
            }
        }
        else if (!parentId.isEmpty())
        {
            errMsg = Utf8StrFmt (
                tr ("Hard disk image '%s' is a differencing image that is linked "
                    "to a hard disk with UUID {%Vuuid} and cannot be used "
                    "directly as a base hard disk"),
                filePath.raw(), parentId.raw());
            break;
        }

        /* get actual file size */
        /// @todo is there a direct method in RT?
        {
            RTFILE file = NIL_RTFILE;
            vrc = RTFileOpen (&file, filePath, RTFILE_O_READ);
            if (VBOX_SUCCESS (vrc))
            {
                uint64_t size = 0;
                vrc = RTFileGetSize (file, &size);
                if (VBOX_SUCCESS (vrc))
                    mActualSize = size;
                RTFileClose (file);
            }
            if (VBOX_FAILURE (vrc))
                break;
        }

        /* query logical size only for non-differencing images */
        if (!mParent)
        {
            uint64_t size = VDGetSize (mContainer, 0);
            /* convert to MBytes */
            mSize = size / 1024 / 1024;
        }
    }
    while (0);

    VDCloseAll (mContainer);

    /* enter the lock again */
    alock.enter();

    /* remove the reference */
    releaseReader();

    if (FAILED (rc) || VBOX_FAILURE (vrc) || !errMsg.isNull())
    {
        LogWarningFunc (("'%ls' is not accessible "
                         "(rc=%08X, vrc=%Vrc, errMsg='%ls')\n",
                         mFilePathFull.raw(), rc, vrc, errMsg.raw()));

        if (aAccessError)
        {
            if (!errMsg.isNull())
                *aAccessError = errMsg;
            else if (!mLastVDError.isNull())
                *aAccessError = mLastVDError;
            else if (VBOX_FAILURE (vrc))
                *aAccessError = Utf8StrFmt (
                    tr ("Could not access hard disk image '%ls' (%Vrc)"),
                        mFilePathFull.raw(), vrc);
        }

        /* downgrade to not accessible */
        mState = Created;
    }
    else
    {
        if (aAccessError)
            aAccessError->setNull();

        mState = Accessible;
    }

    /* inform waiters if there are any */
    if (mStateCheckWaiters > 0)
    {
        RTSemEventMultiSignal (mStateCheckSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (mStateCheckSem);
        mStateCheckSem = NIL_RTSEMEVENTMULTI;
    }

    /* cleanup the last error report from VDError() */
    mLastVDError.setNull();

    return rc;
}

/**
 *  Helper to create hard disk images.
 *
 *  @param aSize        size in MB
 *  @param aDynamic     dynamic or fixed image
 *  @param aProgress    address of IProgress pointer to return
 */
HRESULT HVHDImage::createImage (ULONG64 aSize, BOOL aDynamic,
                                 IProgress **aProgress)
{
    ComAssertMsgFailed (("Not implemented"));
    return E_NOTIMPL;

/// @todo later
//  Use code from HVirtualDiskImage::createImage as an example.
}

/* static */
DECLCALLBACK(int) HVHDImage::VDITaskThread (RTTHREAD thread, void *pvUser)
{
    AssertMsgFailed (("Not implemented"));
    return VERR_GENERAL_FAILURE;

/// @todo later
//  Use code from HVirtualDiskImage::VDITaskThread as an example.
}

/* static */
DECLCALLBACK(void) HVHDImage::VDError (void *pvUser, int rc, RT_SRC_POS_DECL,
                                        const char *pszFormat, va_list va)
{
    HVHDImage *that = static_cast <HVHDImage *> (pvUser);
    AssertReturnVoid (that != NULL);

    /// @todo pass the error message to the operation initiator
    Utf8Str err = Utf8StrFmt (pszFormat, va);
    if (VBOX_FAILURE (rc))
        err = Utf8StrFmt ("%s (%Vrc)", err.raw(), rc);

    if (that->mLastVDError.isNull())
        that->mLastVDError = err;
    else
        that->mLastVDError = Utf8StrFmt
            ("%s.\n%s", that->mLastVDError.raw(), err.raw());
}

