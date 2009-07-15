/** @file
 *
 * VirtualBox COM class implementation
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
HRESULT Snapshot::init(VirtualBox *aVirtualBox,
                       const Guid &aId,
                       IN_BSTR aName,
                       IN_BSTR aDescription,
                       RTTIMESPEC aTimeStamp,
                       SnapshotMachine *aMachine,
                       Snapshot *aParent)
{
    LogFlowMember (("Snapshot::init(aParent=%p)\n", aParent));

    ComAssertRet (!aId.isEmpty() && aName && aMachine, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    /* share parent weakly */
    unconst (mVirtualBox) = aVirtualBox;

    mParent = aParent;

    mData.mId = aId;
    mData.mName = aName;
    mData.mDescription = aDescription;
    mData.mTimeStamp = aTimeStamp;
    mData.mMachine = aMachine;

    if (aParent)
        aParent->addDependentChild (this);

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

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

    /* Enclose the state transition Ready->InUninit->NotReady */
//     AutoUninitSpan autoUninitSpan (this);         @todo this creates a deadlock, investigate what this does actually
//     if (autoUninitSpan.uninitDone())
//         return;

    // uninit all children
    uninitDependentChildren();

    if (mParent)
    {
        mParent->removeDependentChild (this);
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
    AutoCaller autoCaller (this);
    if (FAILED(autoCaller.rc()))
        return;

    AutoWriteLock alock (this);

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

STDMETHODIMP Snapshot::COMGETTER(Id) (BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData.mId.toUtf16().cloneTo (aId);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData.mName.cloneTo (aName);
    return S_OK;
}

/**
 *  @note Locks this object for writing, then calls Machine::onSnapshotChange()
 *  (see its lock requirements).
 */
STDMETHODIMP Snapshot::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData.mDescription.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP Snapshot::COMSETTER(Description) (IN_BSTR aDescription)
{
    CheckComArgNotNull(aDescription);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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
    CheckComArgOutPointerValid(aTimeStamp);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aTimeStamp = RTTimeSpecGetMilli (&mData.mTimeStamp);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Online) (BOOL *aOnline)
{
    CheckComArgOutPointerValid(aOnline);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aOnline = !stateFilePath().isNull();
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Machine) (IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData.mMachine.queryInterfaceTo (aMachine);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Parent) (ISnapshot **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mParent.queryInterfaceTo (aParent);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Children) (ComSafeArrayOut (ISnapshot *, aChildren))
{
    CheckComArgOutSafeArrayPointerValid(aChildren);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);
    AutoReadLock chLock (childrenLock ());

    SafeIfaceArray <ISnapshot> collection (children());
    collection.detachTo (ComSafeArrayOutArg (aChildren));

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
    AutoCaller autoCaller (this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock (this);

    AutoReadLock chLock (childrenLock ());

    ULONG count = (ULONG)children().size();

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
ComObjPtr<Snapshot> Snapshot::findChildOrSelf (IN_GUID aId)
{
    ComObjPtr <Snapshot> child;

    AutoCaller autoCaller (this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock (this);

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
ComObjPtr <Snapshot> Snapshot::findChildOrSelf (IN_BSTR aName)
{
    ComObjPtr <Snapshot> child;
    AssertReturn (aName, child);

    AutoCaller autoCaller (this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock (this);

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

    AutoCaller autoCaller (this);
    AssertComRC(autoCaller.rc());

    AutoWriteLock alock (this);

    Utf8Str path = mData.mMachine->mSSData->mStateFilePath;
    LogFlowThisFunc (("Snap[%ls].statePath={%s}\n", mData.mName.raw(), path.raw()));

    /* state file may be NULL (for offline snapshots) */
    if (    path.length()
         && RTPathStartsWith(path, aOldPath)
       )
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

/**
    * Returns VirtualBox::mSnapshotsTreeLockHandle(), for convenience. Don't forget
    * to follow these locking rules:
    *
    * 1. The write lock on this handle must be either held alone on the thread
    *    or requested *after* the VirtualBox object lock. Mixing with other
    *    locks is prohibited.
    *
    * 2. The read lock on this handle may be intermixed with any other lock
    *    with the exception that it must be requested *after* the VirtualBox
    *    object lock.
    */
RWLockHandle* Snapshot::treeLock()
{
    return mVirtualBox->snapshotTreeLockHandle();
}

/** Reimplements VirtualBoxWithTypedChildren::childrenLock() to return
    *  treeLock(). */
RWLockHandle* Snapshot::childrenLock()
{
    return treeLock();
}


