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

#include <list>

#include <VBox/settings.h>

////////////////////////////////////////////////////////////////////////////////
//
// Snapshot private data definition
//
////////////////////////////////////////////////////////////////////////////////

typedef std::list< ComObjPtr<Snapshot> > SnapshotsList;

struct Snapshot::Data
{
    Data()
    {
        RTTimeSpecSetMilli(&timeStamp, 0);
    };

    ~Data()
    {}

    Guid                        uuid;
    Utf8Str                     strName;
    Utf8Str                     strDescription;
    RTTIMESPEC                  timeStamp;
    ComObjPtr<SnapshotMachine>  pMachine;

    SnapshotsList               llChildren;             // protected by VirtualBox::snapshotTreeLockHandle()
};

////////////////////////////////////////////////////////////////////////////////
//
// Constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

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
                       const Utf8Str &aName,
                       const Utf8Str &aDescription,
                       const RTTIMESPEC &aTimeStamp,
                       SnapshotMachine *aMachine,
                       Snapshot *aParent)
{
    LogFlowMember(("Snapshot::init(uuid: %s, aParent->uuid=%s)\n", aId.toString().c_str(), (aParent) ? aParent->m->uuid.toString().c_str() : ""));

    ComAssertRet (!aId.isEmpty() && !aName.isEmpty() && aMachine, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data;

    /* share parent weakly */
    unconst(mVirtualBox) = aVirtualBox;

    mParent = aParent;

    m->uuid = aId;
    m->strName = aName;
    m->strDescription = aDescription;
    m->timeStamp = aTimeStamp;
    m->pMachine = aMachine;

    if (aParent)
        aParent->m->llChildren.push_back(this);

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
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    // uninit all children
    SnapshotsList::iterator it;
    for (it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        Snapshot *pChild = *it;
        pChild->mParent.setNull();
        pChild->uninit();
    }
    m->llChildren.clear();          // this unsets all the ComPtrs and probably calls delete

    if (mParent)
    {
        SnapshotsList &llParent = mParent->m->llChildren;
        for (it = llParent.begin();
            it != llParent.end();
            ++it)
        {
            Snapshot *pParentsChild = *it;
            if (this == pParentsChild)
            {
                llParent.erase(it);
                break;
            }
        }

        mParent.setNull();
    }

    if (m->pMachine)
    {
        m->pMachine->uninit();
        m->pMachine.setNull();
    }

    delete m;
    m = NULL;
}

/**
 *  Discards the current snapshot by removing it from the tree of snapshots
 *  and reparenting its children.
 *
 *  After this, the caller must call uninit() on the snapshot. We can't call
 *  that from here because if we do, the AutoUninitSpan waits forever for
 *  the number of callers to become 0 (it is 1 because of the AutoCaller in here).
 *
 *  NOTE: this does NOT lock the snapshot, it is assumed that the caller has
 *  locked a) the machine and b) the snapshots tree in write mode!
 */
void Snapshot::beginDiscard()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc()))
        return;

    /* for now, the snapshot must have only one child when discarded,
        * or no children at all */
    AssertReturnVoid(m->llChildren.size() <= 1);

    ComObjPtr<Snapshot> parentSnapshot = parent();

    /// @todo (dmik):
    //  when we introduce clones later, discarding the snapshot
    //  will affect the current and first snapshots of clones, if they are
    //  direct children of this snapshot. So we will need to lock machines
    //  associated with child snapshots as well and update mCurrentSnapshot
    //  and/or mFirstSnapshot fields.

    if (this == m->pMachine->mData->mCurrentSnapshot)
    {
        m->pMachine->mData->mCurrentSnapshot = parentSnapshot;

        /* we've changed the base of the current state so mark it as
            * modified as it no longer guaranteed to be its copy */
        m->pMachine->mData->mCurrentStateModified = TRUE;
    }

    if (this == m->pMachine->mData->mFirstSnapshot)
    {
        if (m->llChildren.size() == 1)
        {
            ComObjPtr<Snapshot> childSnapshot = m->llChildren.front();
            m->pMachine->mData->mFirstSnapshot = childSnapshot;
        }
        else
            m->pMachine->mData->mFirstSnapshot.setNull();
    }

    // reparent our children
    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        ComObjPtr<Snapshot> child = *it;
        AutoWriteLock childLock(child);

        child->mParent = mParent;
        if (mParent)
            mParent->m->llChildren.push_back(child);
    }

    // clear our own children list (since we reparented the children)
    m->llChildren.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// ISnapshot public methods
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Snapshot::COMGETTER(Id) (BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->uuid.toUtf16().cloneTo(aId);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->strName.cloneTo(aName);
    return S_OK;
}

/**
 *  @note Locks this object for writing, then calls Machine::onSnapshotChange()
 *  (see its lock requirements).
 */
STDMETHODIMP Snapshot::COMSETTER(Name)(IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Utf8Str strName(aName);

    AutoWriteLock alock(this);

    if (m->strName != strName)
    {
        m->strName = strName;

        alock.leave(); /* Important! (child->parent locks are forbidden) */

        return m->pMachine->onSnapshotChange(this);
    }

    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Description) (BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->strDescription.cloneTo(aDescription);
    return S_OK;
}

STDMETHODIMP Snapshot::COMSETTER(Description) (IN_BSTR aDescription)
{
    CheckComArgNotNull(aDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Utf8Str strDescription(aDescription);

    AutoWriteLock alock(this);

    if (m->strDescription != strDescription)
    {
        m->strDescription = strDescription;

        alock.leave(); /* Important! (child->parent locks are forbidden) */

        return m->pMachine->onSnapshotChange(this);
    }

    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(TimeStamp) (LONG64 *aTimeStamp)
{
    CheckComArgOutPointerValid(aTimeStamp);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aTimeStamp = RTTimeSpecGetMilli(&m->timeStamp);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Online)(BOOL *aOnline)
{
    CheckComArgOutPointerValid(aOnline);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aOnline = !stateFilePath().isNull();
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Machine) (IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->pMachine.queryInterfaceTo(aMachine);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Parent) (ISnapshot **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mParent.queryInterfaceTo(aParent);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Children) (ComSafeArrayOut(ISnapshot *, aChildren))
{
    CheckComArgOutSafeArrayPointerValid(aChildren);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(m->pMachine->snapshotsTreeLockHandle());
    AutoReadLock block(this->lockHandle());

    SafeIfaceArray<ISnapshot> collection(m->llChildren);
    collection.detachTo(ComSafeArrayOutArg(aChildren));

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note
 *      Must be called from under the object's lock!
 */
const Bstr& Snapshot::stateFilePath() const
{
    return m->pMachine->mSSData->mStateFilePath;
}

/**
 * Returns the number of direct child snapshots, without grandchildren.
 * Does not recurse.
 * @return
 */
ULONG Snapshot::getChildrenCount()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock treeLock(m->pMachine->snapshotsTreeLockHandle());
    return (ULONG)m->llChildren.size();
}

/**
 * Implementation method for getAllChildrenCount() so we request the
 * tree lock only once before recursing. Don't call directly.
 * @return
 */
ULONG Snapshot::getAllChildrenCountImpl()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    ULONG count = (ULONG)m->llChildren.size();
    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        count += (*it)->getAllChildrenCountImpl();
    }

    return count;
}

/**
 * Returns the number of child snapshots including all grandchildren.
 * Recurses into the snapshots tree.
 * @return
 */
ULONG Snapshot::getAllChildrenCount()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock treeLock(m->pMachine->snapshotsTreeLockHandle());
    return getAllChildrenCountImpl();
}

/**
 * Returns the SnapshotMachine that this snapshot belongs to.
 * Caller must hold the snapshot's object lock!
 * @return
 */
ComPtr<SnapshotMachine> Snapshot::getSnapshotMachine()
{
    return (SnapshotMachine*)m->pMachine;
}

/**
 * Returns the UUID of this snapshot.
 * Caller must hold the snapshot's object lock!
 * @return
 */
Guid Snapshot::getId() const
{
    return m->uuid;
}

/**
 * Returns the name of this snapshot.
 * Caller must hold the snapshot's object lock!
 * @return
 */
const Utf8Str& Snapshot::getName() const
{
    return m->strName;
}

/**
 * Returns the time stamp of this snapshot.
 * Caller must hold the snapshot's object lock!
 * @return
 */
RTTIMESPEC Snapshot::getTimeStamp() const
{
    return m->timeStamp;
}

/**
 *  Searches for a snapshot with the given ID among children, grand-children,
 *  etc. of this snapshot. This snapshot itself is also included in the search.
 *  Caller must hold the snapshots tree lock!
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(IN_GUID aId)
{
    ComObjPtr<Snapshot> child;

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (m->uuid == aId)
        child = this;
    else
    {
        alock.unlock();
        for (SnapshotsList::const_iterator it = m->llChildren.begin();
             it != m->llChildren.end();
             ++it)
        {
            if ((child = (*it)->findChildOrSelf(aId)))
                break;
        }
    }

    return child;
}

/**
 *  Searches for a first snapshot with the given name among children,
 *  grand-children, etc. of this snapshot. This snapshot itself is also included
 *  in the search.
 *  Caller must hold the snapshots tree lock!
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(const Utf8Str &aName)
{
    ComObjPtr<Snapshot> child;
    AssertReturn(!aName.isEmpty(), child);

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock (this);

    if (m->strName == aName)
        child = this;
    else
    {
        alock.unlock();
        for (SnapshotsList::const_iterator it = m->llChildren.begin();
             it != m->llChildren.end();
             ++it)
        {
            if ((child = (*it)->findChildOrSelf(aName)))
                break;
        }
    }

    return child;
}

/**
 * Internal implementation for Snapshot::updateSavedStatePaths (below).
 * @param aOldPath
 * @param aNewPath
 */
void Snapshot::updateSavedStatePathsImpl(const char *aOldPath, const char *aNewPath)
{
    AutoWriteLock alock(this);

    Utf8Str path = m->pMachine->mSSData->mStateFilePath;
    LogFlowThisFunc(("Snap[%s].statePath={%s}\n", m->strName.c_str(), path.c_str()));

    /* state file may be NULL (for offline snapshots) */
    if (    path.length()
         && RTPathStartsWith(path.c_str(), aOldPath)
       )
    {
        path = Utf8StrFmt ("%s%s", aNewPath, path.raw() + strlen (aOldPath));
        m->pMachine->mSSData->mStateFilePath = path;

        LogFlowThisFunc(("-> updated: {%s}\n", path.raw()));
    }

    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        Snapshot *pChild = *it;
        pChild->updateSavedStatePathsImpl(aOldPath, aNewPath);
    }
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
void Snapshot::updateSavedStatePaths(const char *aOldPath, const char *aNewPath)
{
    LogFlowThisFunc(("aOldPath={%s} aNewPath={%s}\n", aOldPath, aNewPath));

    AssertReturnVoid(aOldPath);
    AssertReturnVoid(aNewPath);

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoWriteLock chLock(m->pMachine->snapshotsTreeLockHandle());
    // call the implementation under the tree lock
    updateSavedStatePathsImpl(aOldPath, aNewPath);
}

/**
 * Internal implementation for Snapshot::saveSnapshot (below).
 * @param aNode
 * @param aAttrsOnly
 * @return
 */
HRESULT Snapshot::saveSnapshotImpl(settings::Snapshot &data, bool aAttrsOnly)
{
    AutoReadLock alock(this);

    data.uuid = m->uuid;
    data.strName = m->strName;
    data.timestamp = m->timeStamp;
    data.strDescription = m->strDescription;

    if (aAttrsOnly)
        return S_OK;

    /* stateFile (optional) */
    if (stateFilePath())
    {
        /* try to make the file name relative to the settings file dir */
        Utf8Str strStateFilePath = stateFilePath();
        m->pMachine->calculateRelativePath(strStateFilePath, strStateFilePath);
        data.strStateFile = strStateFilePath;
    }
    else
        data.strStateFile.setNull();

    HRESULT rc = m->pMachine->saveHardware(data.hardware);
    CheckComRCReturnRC (rc);

    rc = m->pMachine->saveStorageControllers(data.storage);
    CheckComRCReturnRC (rc);

    alock.unlock();

    data.llChildSnapshots.clear();

    if (m->llChildren.size())
    {
        for (SnapshotsList::const_iterator it = m->llChildren.begin();
             it != m->llChildren.end();
             ++it)
        {
            settings::Snapshot snap;
            rc = (*it)->saveSnapshotImpl(snap, aAttrsOnly);
            CheckComRCReturnRC (rc);

            data.llChildSnapshots.push_back(snap);
        }
    }

    return S_OK;
}

/**
 *  Saves the given snapshot and all its children (unless \a aAttrsOnly is true).
 *  It is assumed that the given node is empty (unless \a aAttrsOnly is true).
 *
 *  @param aNode        <Snapshot> node to save the snapshot to.
 *  @param aSnapshot    Snapshot to save.
 *  @param aAttrsOnly   If true, only updatge user-changeable attrs.
 */
HRESULT Snapshot::saveSnapshot(settings::Snapshot &data, bool aAttrsOnly)
{
    AutoWriteLock listLock(m->pMachine->snapshotsTreeLockHandle());

    return saveSnapshotImpl(data, aAttrsOnly);
}

