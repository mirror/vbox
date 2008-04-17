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

#include "SnapshotImpl.h"

#include "MachineImpl.h"
#include "Logging.h"

#include <iprt/path.h>
#include <VBox/param.h>
#include <VBox/err.h>

#include <algorithm>

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

Snapshot::Data::Data()
{
    RTTimeSpecSetMilli (&mTimeStamp, 0);
};

Snapshot::Data::~Data()
{
};

HRESULT Snapshot::FinalConstruct()
{
    LogFlowMember (("Snapshot::FinalConstruct()\n"));
    return S_OK;
}

void Snapshot::FinalRelease()
{
    LogFlowMember (("Snapshot::FinalRelease()\n"));
    uninit();
}

/**
 *  Initializes the instance
 *
 *  @param  aId            id of the snapshot
 *  @param  aName          name of the snapshot
 *  @param  aDescription   name of the snapshot (NULL if no description)
 *  @param  aTimeStamp     timestamp of the snapshot, in ms since 1970-01-01 UTC
 *  @param  aMachine       machine associated with this snapshot
 *  @param  aParent        parent snapshot (NULL if no parent)
 */
HRESULT Snapshot::init (const Guid &aId, INPTR BSTR aName, INPTR BSTR aDescription,
                        RTTIMESPEC aTimeStamp, SnapshotMachine *aMachine,
                        Snapshot *aParent)
{
    LogFlowMember (("Snapshot::init(aParent=%p)\n", aParent));

    ComAssertRet (!aId.isEmpty() && aName && aMachine, E_INVALIDARG);

    AutoWriteLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    mData.mId = aId;
    mData.mName = aName;
    mData.mDescription = aDescription;
    mData.mTimeStamp = aTimeStamp;
    mData.mMachine = aMachine;

    if (aParent)
        aParent->addDependentChild (this);

    setReady (true);

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease(), by the parent when it gets destroyed,
 *  or by a third party when it decides this object is no more valid.
 */
void Snapshot::uninit()
{
    LogFlowMember (("Snapshot::uninit()\n"));

    AutoWriteLock alock (this);

    LogFlowMember (("Snapshot::uninit(): isReady=%d\n", isReady()));
    if (!isReady())
        return;

    // uninit all children
    uninitDependentChildren();

    setReady (false);

    if (mParent)
    {
        alock.leave();
        mParent->removeDependentChild (this);
        alock.enter();
        mParent.setNull();
    }

    if (mData.mMachine)
    {
        mData.mMachine->uninit();
        mData.mMachine.setNull();
    }
}

/**
 *  Discards the current snapshot by removing it from the tree of snapshots
 *  and reparenting its children.
 *  This method also calls #uninit() in case of success.
 */
void Snapshot::discard()
{
    LogFlowMember (("Snapshot::discard()\n"));

    AutoWriteLock alock (this);
    AssertReturn (isReady(), (void) 0);

    {
        AutoWriteLock chLock (childrenLock ());
        AssertReturn (!!mParent || children().size() <= 1, (void) 0);

        for (SnapshotList::const_iterator it = children().begin();
             it != children().end(); ++ it)
        {
            ComObjPtr <Snapshot> child = *it;
            AutoWriteLock childLock (child);
            // reparent the child
            child->mParent = mParent;
            if (mParent)
                mParent->addDependentChild (child);
        }
    }

    // detach all our children to avoid their uninit in #uninit()
    removeDependentChildren();

    // finalize uninitialization
    uninit();
}

// ISnapshot methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Snapshot::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mData.mId.cloneTo (aId);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mData.mName.cloneTo (aName);
    return S_OK;
}

/**
 *  @note Locks this object for writing, then calls Machine::onSnapshotChange()
 *  (see its lock requirements).
 */
STDMETHODIMP Snapshot::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    if (mData.mName != aName)
    {
        mData.mName = aName;

        alock.leave(); /* Important! (child->parent locks are forbidden) */

        return mData.mMachine->onSnapshotChange (this);
    }

    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mData.mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP Snapshot::COMSETTER(Description) (INPTR BSTR aDescription)
{
    if (!aDescription)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    if (mData.mDescription != aDescription)
    {
        mData.mDescription = aDescription;

        alock.leave(); /* Important! (child->parent locks are forbidden) */

        return mData.mMachine->onSnapshotChange (this);
    }

    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(TimeStamp) (LONG64 *aTimeStamp)
{
    if (!aTimeStamp)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    *aTimeStamp = RTTimeSpecGetMilli (&mData.mTimeStamp);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Online) (BOOL *aOnline)
{
    if (!aOnline)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    *aOnline = !stateFilePath().isNull();
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Machine) (IMachine **aMachine)
{
    if (!aMachine)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mData.mMachine.queryInterfaceTo (aMachine);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Parent) (ISnapshot **aParent)
{
    if (!aParent)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mParent.queryInterfaceTo (aParent);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Children) (ISnapshotCollection **aChildren)
{
    if (!aChildren)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    AutoWriteLock chLock (childrenLock ());

    ComObjPtr <SnapshotCollection> collection;
    collection.createObject();
    collection->init (children());
    collection.queryInterfaceTo (aChildren);

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note
 *      Must be called from under the object's lock!
 */
const Bstr &Snapshot::stateFilePath() const
{
    return mData.mMachine->mSSData->mStateFilePath;
}

/**
 *  Returns the number of children of this snapshot, including grand-children,
 *  etc.
 */
ULONG Snapshot::descendantCount()
{
    AutoWriteLock alock(this);
    AssertReturn (isReady(), 0);

    AutoWriteLock chLock (childrenLock ());

    ULONG count = children().size();

    for (SnapshotList::const_iterator it = children().begin();
         it != children().end(); ++ it)
    {
        count += (*it)->descendantCount();
    }

    return count;
}

/**
 *  Searches for a snapshot with the given ID among children, grand-children,
 *  etc. of this snapshot. This snapshot itself is also included in the search.
 */
ComObjPtr <Snapshot> Snapshot::findChildOrSelf (INPTR GUIDPARAM aId)
{
    ComObjPtr <Snapshot> child;

    AutoWriteLock alock (this);
    AssertReturn (isReady(), child);

    if (mData.mId == aId)
        child = this;
    else
    {
        AutoWriteLock chLock (childrenLock ());
        for (SnapshotList::const_iterator it = children().begin();
             !child && it != children().end(); ++ it)
        {
            child = (*it)->findChildOrSelf (aId);
        }
    }

    return child;
}

/**
 *  Searches for a first snapshot with the given name among children,
 *  grand-children, etc. of this snapshot. This snapshot itself is also included
 *  in the search.
 */
ComObjPtr <Snapshot> Snapshot::findChildOrSelf (INPTR BSTR aName)
{
    ComObjPtr <Snapshot> child;
    AssertReturn (aName, child);

    AutoWriteLock alock (this);
    AssertReturn (isReady(), child);

    if (mData.mName == aName)
        child = this;
    else
    {
        AutoWriteLock chLock (childrenLock ());
        for (SnapshotList::const_iterator it = children().begin();
             !child && it != children().end(); ++ it)
        {
            child = (*it)->findChildOrSelf (aName);
        }
    }

    return child;
}

/**
 *  Returns @c true if the given DVD image is attached to this snapshot or any
 *  of its children, recursively.
 *
 *  @param aId          Image ID to check.
 *
 *  @note Locks this object for reading.
 */
bool Snapshot::isDVDImageUsed (const Guid &aId)
{
    AutoReadLock alock (this);
    AssertReturn (isReady(), false);

    AssertReturn (!mData.mMachine.isNull(), false);
    AssertReturn (!mData.mMachine->mDVDDrive.isNull(), false);

    DVDDrive::Data *d = mData.mMachine->mDVDDrive->data().data();

    if (d &&
        d->mDriveState == DriveState_ImageMounted)
    {
        Guid id;
        HRESULT rc = d->mDVDImage->COMGETTER(Id) (id.asOutParam());
        AssertComRC (rc);
        if (id == aId)
            return true;
    }

    AutoReadLock chLock (childrenLock());
    for (SnapshotList::const_iterator it = children().begin();
         it != children().end(); ++ it)
    {
        if ((*it)->isDVDImageUsed (aId))
            return true;
    }

    return false;
}

/**
 *  Returns @c true if the given Floppy image is attached to this snapshot or any
 *  of its children, recursively.
 *
 *  @param aId          Image ID to check.
 *
 *  @note Locks this object for reading.
 */
bool Snapshot::isFloppyImageUsed (const Guid &aId)
{
    AutoReadLock alock (this);
    AssertReturn (isReady(), false);

    AssertReturn (!mData.mMachine.isNull(), false);
    AssertReturn (!mData.mMachine->mFloppyDrive.isNull(), false);

    FloppyDrive::Data *d = mData.mMachine->mFloppyDrive->data().data();

    if (d &&
        d->mDriveState == DriveState_ImageMounted)
    {
        Guid id;
        HRESULT rc = d->mFloppyImage->COMGETTER(Id) (id.asOutParam());
        AssertComRC (rc);
        if (id == aId)
            return true;
    }

    AutoReadLock chLock (childrenLock());
    for (SnapshotList::const_iterator it = children().begin();
         it != children().end(); ++ it)
    {
        if ((*it)->isFloppyImageUsed (aId))
            return true;
    }

    return false;
}

/**
 *  Checks if the specified path change affects the saved state file path of
 *  this snapshot or any of its (grand-)children and updates it accordingly.
 *
 *  Intended to be called by Machine::openConfigLoader() only.
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object + children for writing.
 */
void Snapshot::updateSavedStatePaths (const char *aOldPath, const char *aNewPath)
{
    LogFlowThisFunc (("aOldPath={%s} aNewPath={%s}\n", aOldPath, aNewPath));

    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoWriteLock alock (this);
    AssertReturnVoid (isReady());

    Utf8Str path = mData.mMachine->mSSData->mStateFilePath;
    LogFlowThisFunc (("Snap[%ls].statePath={%s}\n", mData.mName.raw(), path.raw()));

    /* state file may be NULL (for offline snapshots) */
    if (path && RTPathStartsWith (path, aOldPath))
    {
        path = Utf8StrFmt ("%s%s", aNewPath, path.raw() + strlen (aOldPath));
        mData.mMachine->mSSData->mStateFilePath = path;

        LogFlowThisFunc (("-> updated: {%s}\n", path.raw()));
    }

    AutoWriteLock chLock (childrenLock ());
    for (SnapshotList::const_iterator it = children().begin();
         it != children().end(); ++ it)
    {
        (*it)->updateSavedStatePaths (aOldPath, aNewPath);
    }
}

