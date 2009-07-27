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

// private snapshot data
////////////////////////////////////////////////////////////////////////////////

typedef std::list< ComPtr<Snapshot> > SnapshotsList;

struct Snapshot::Data
{
    Data()
    {
        RTTimeSpecSetMilli(&timeStamp, 0);
    };

    ~Data()
    {}

    Guid                        id;
    Bstr                        name;
    Bstr                        description;
    RTTIMESPEC                  timeStamp;
    ComObjPtr<SnapshotMachine>  pMachine;

    SnapshotsList               llChildren;             // protected by VirtualBox::snapshotTreeLockHandle()
};

// constructor / destructor
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
                       IN_BSTR aName,
                       IN_BSTR aDescription,
                       RTTIMESPEC aTimeStamp,
                       SnapshotMachine *aMachine,
                       Snapshot *aParent)
{
    LogFlowMember (("Snapshot::init(aParent=%p)\n", aParent));

    ComAssertRet (!aId.isEmpty() && aName && aMachine, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data;

    /* share parent weakly */
    unconst(mVirtualBox) = aVirtualBox;

    mParent = aParent;

    m->id = aId;
    m->name = aName;
    m->description = aDescription;
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
    AutoUninitSpan autoUninitSpan (this);
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
            ++ it)
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
 *  locked a) the machine, b) the snapshots tree and c) the snapshot (this).
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
            ComPtr<Snapshot> childSnapshot = m->llChildren.front();
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
        ComPtr<Snapshot> child = *it;
        AutoWriteLock childLock(child);

        child->mParent = mParent;
        if (mParent)
            mParent->m->llChildren.push_back(child);
    }

    // clear our own children list (since we reparented the children)
    m->llChildren.clear();
}

// ISnapshot methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Snapshot::COMGETTER(Id) (BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    m->id.toUtf16().cloneTo(aId);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    m->name.cloneTo(aName);
    return S_OK;
}

/**
 *  @note Locks this object for writing, then calls Machine::onSnapshotChange()
 *  (see its lock requirements).
 */
STDMETHODIMP Snapshot::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (m->name != aName)
    {
        m->name = aName;

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

    AutoReadLock alock (this);

    m->description.cloneTo(aDescription);
    return S_OK;
}

STDMETHODIMP Snapshot::COMSETTER(Description) (IN_BSTR aDescription)
{
    CheckComArgNotNull(aDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (m->description != aDescription)
    {
        m->description = aDescription;

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

    AutoReadLock alock (this);

    *aTimeStamp = RTTimeSpecGetMilli(&m->timeStamp);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Online) (BOOL *aOnline)
{
    CheckComArgOutPointerValid(aOnline);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    *aOnline = !stateFilePath().isNull();
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Machine) (IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    m->pMachine.queryInterfaceTo(aMachine);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Parent) (ISnapshot **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock (this);

    mParent.queryInterfaceTo (aParent);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Children) (ComSafeArrayOut (ISnapshot *, aChildren))
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

ULONG Snapshot::getChildrenCount()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock chLock(m->pMachine->snapshotsTreeLockHandle());
    AutoReadLock alock(this);

    return (ULONG)m->llChildren.size();
}

ULONG Snapshot::getAllChildrenCountImpl()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock(this);
    ULONG count = (ULONG)m->llChildren.size();
    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        count += (*it)->getAllChildrenCountImpl();
    }

    return count;
}

ULONG Snapshot::getAllChildrenCount()
{
    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock chLock(m->pMachine->snapshotsTreeLockHandle());
    return getAllChildrenCountImpl();
}

ComPtr<SnapshotMachine> Snapshot::getSnapshotMachine()
{
    return (SnapshotMachine*)m->pMachine;
}

Guid Snapshot::getId() const
{
    return m->id;
}

Bstr Snapshot::getName() const
{
    return m->name;
}

RTTIMESPEC Snapshot::getTimeStamp() const
{
    return m->timeStamp;
}

/**
 *  Searches for a snapshot with the given ID among children, grand-children,
 *  etc. of this snapshot. This snapshot itself is also included in the search.
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(IN_GUID aId)
{
    ComObjPtr<Snapshot> child;

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (m->id == aId)
        child = this;
    else
    {
        alock.unlock();
        AutoReadLock chlock(m->pMachine->snapshotsTreeLockHandle());
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
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(IN_BSTR aName)
{
    ComObjPtr <Snapshot> child;
    AssertReturn (aName, child);

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock (this);

    if (m->name == aName)
        child = this;
    else
    {
        alock.unlock();
        AutoReadLock chlock(m->pMachine->snapshotsTreeLockHandle());
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
    LogFlowThisFunc(("Snap[%ls].statePath={%s}\n", m->name.raw(), path.raw()));

    /* state file may be NULL (for offline snapshots) */
    if (    path.length()
         && RTPathStartsWith(path, aOldPath)
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
        updateSavedStatePathsImpl(aOldPath, aNewPath);
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
HRESULT Snapshot::saveSnapshotImpl(settings::Key &aNode, bool aAttrsOnly)
{
    using namespace settings;

    /* uuid (required) */
    if (!aAttrsOnly)
        aNode.setValue<Guid>("uuid", m->id);

    /* name (required) */
    aNode.setValue<Bstr>("name", m->name);

    /* timeStamp (required) */
    aNode.setValue<RTTIMESPEC>("timeStamp", m->timeStamp);

    /* Description node (optional) */
    if (!m->description.isNull())
    {
        Key descNode = aNode.createKey("Description");
        descNode.setKeyValue<Bstr>(m->description);
    }
    else
    {
        Key descNode = aNode.findKey ("Description");
        if (!descNode.isNull())
            descNode.zap();
    }

    if (aAttrsOnly)
        return S_OK;

    /* stateFile (optional) */
    if (stateFilePath())
    {
        /* try to make the file name relative to the settings file dir */
        Utf8Str strStateFilePath = stateFilePath();
        m->pMachine->calculateRelativePath(strStateFilePath, strStateFilePath);
        aNode.setStringValue ("stateFile", strStateFilePath);
    }

    {
        ComObjPtr<SnapshotMachine> snapshotMachine = m->pMachine;

        /* save hardware */
        {
            Key hwNode = aNode.createKey ("Hardware");
            HRESULT rc = snapshotMachine->saveHardware (hwNode);
            CheckComRCReturnRC (rc);
        }

        /* save hard disks. */
        {
            Key storageNode = aNode.createKey ("StorageControllers");
            HRESULT rc = snapshotMachine->saveStorageControllers (storageNode);
            CheckComRCReturnRC (rc);
        }
    }

    if (m->llChildren.size())
    {
        Key snapshotsNode = aNode.createKey ("Snapshots");

        HRESULT rc = S_OK;

        for (SnapshotsList::const_iterator it = m->llChildren.begin();
             it != m->llChildren.end();
             ++it)
        {
            Key snapshotNode = snapshotsNode.createKey("Snapshot");
            rc = (*it)->saveSnapshotImpl(snapshotNode, aAttrsOnly);
            CheckComRCReturnRC (rc);
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
HRESULT Snapshot::saveSnapshot(settings::Key &aNode, bool aAttrsOnly)
{
    AssertReturn(!aNode.isNull(), E_INVALIDARG);

    AutoWriteLock listLock(m->pMachine->snapshotsTreeLockHandle());

    return saveSnapshotImpl(aNode, aAttrsOnly);
}

