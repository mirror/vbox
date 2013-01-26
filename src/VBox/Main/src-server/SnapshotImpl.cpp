/* $Id$ */
/** @file
 *
 * COM class implementation for Snapshot and SnapshotMachine in VBoxSVC.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Logging.h"
#include "SnapshotImpl.h"

#include "MachineImpl.h"
#include "MediumImpl.h"
#include "MediumFormatImpl.h"
#include "Global.h"
#include "ProgressImpl.h"

// @todo these three includes are required for about one or two lines, try
// to remove them and put that code in shared code in MachineImplcpp
#include "SharedFolderImpl.h"
#include "USBControllerImpl.h"
#include "VirtualBoxImpl.h"

#include "AutoCaller.h"

#include <iprt/path.h>
#include <iprt/cpp/utils.h>

#include <VBox/param.h>
#include <VBox/err.h>

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
        : pVirtualBox(NULL)
    {
        RTTimeSpecSetMilli(&timeStamp, 0);
    };

    ~Data()
    {}

    const Guid                  uuid;
    Utf8Str                     strName;
    Utf8Str                     strDescription;
    RTTIMESPEC                  timeStamp;
    ComObjPtr<SnapshotMachine>  pMachine;

    /** weak VirtualBox parent */
    VirtualBox * const          pVirtualBox;

    // pParent and llChildren are protected by the machine lock
    ComObjPtr<Snapshot>         pParent;
    SnapshotsList               llChildren;
};

////////////////////////////////////////////////////////////////////////////////
//
// Constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Snapshot::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void Snapshot::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
    BaseFinalRelease();
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
    LogFlowThisFunc(("uuid=%s aParent->uuid=%s\n", aId.toString().c_str(), (aParent) ? aParent->m->uuid.toString().c_str() : ""));

    ComAssertRet(!aId.isZero() && aId.isValid() && !aName.isEmpty() && aMachine, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data;

    /* share parent weakly */
    unconst(m->pVirtualBox) = aVirtualBox;

    m->pParent = aParent;

    unconst(m->uuid) = aId;
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
 *
 *  Since this manipulates the snapshots tree, the caller must hold the
 *  machine lock in write mode (which protects the snapshots tree)!
 */
void Snapshot::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    Assert(m->pMachine->isWriteLockOnCurrentThread());

    // uninit all children
    SnapshotsList::iterator it;
    for (it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        Snapshot *pChild = *it;
        pChild->m->pParent.setNull();
        pChild->uninit();
    }
    m->llChildren.clear();          // this unsets all the ComPtrs and probably calls delete

    if (m->pParent)
        deparent();

    if (m->pMachine)
    {
        m->pMachine->uninit();
        m->pMachine.setNull();
    }

    delete m;
    m = NULL;
}

/**
 *  Delete the current snapshot by removing it from the tree of snapshots
 *  and reparenting its children.
 *
 *  After this, the caller must call uninit() on the snapshot. We can't call
 *  that from here because if we do, the AutoUninitSpan waits forever for
 *  the number of callers to become 0 (it is 1 because of the AutoCaller in here).
 *
 *  NOTE: this does NOT lock the snapshot, it is assumed that the machine state
 *  (and the snapshots tree) is protected by the caller having requested the machine
 *  lock in write mode AND the machine state must be DeletingSnapshot.
 */
void Snapshot::beginSnapshotDelete()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc()))
        return;

    // caller must have acquired the machine's write lock
    Assert(   m->pMachine->mData->mMachineState == MachineState_DeletingSnapshot
           || m->pMachine->mData->mMachineState == MachineState_DeletingSnapshotOnline
           || m->pMachine->mData->mMachineState == MachineState_DeletingSnapshotPaused);
    Assert(m->pMachine->isWriteLockOnCurrentThread());

    // the snapshot must have only one child when being deleted or no children at all
    AssertReturnVoid(m->llChildren.size() <= 1);

    ComObjPtr<Snapshot> parentSnapshot = m->pParent;

    /// @todo (dmik):
    //  when we introduce clones later, deleting the snapshot will affect
    //  the current and first snapshots of clones, if they are direct children
    //  of this snapshot. So we will need to lock machines associated with
    //  child snapshots as well and update mCurrentSnapshot and/or
    //  mFirstSnapshot fields.

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
        // no need to lock, snapshots tree is protected by machine lock
        child->m->pParent = m->pParent;
        if (m->pParent)
            m->pParent->m->llChildren.push_back(child);
    }

    // clear our own children list (since we reparented the children)
    m->llChildren.clear();
}

/**
 * Internal helper that removes "this" from the list of children of its
 * parent. Used in uninit() and other places when reparenting is necessary.
 *
 * The caller must hold the machine lock in write mode (which protects the snapshots tree)!
 */
void Snapshot::deparent()
{
    Assert(m->pMachine->isWriteLockOnCurrentThread());

    SnapshotsList &llParent = m->pParent->m->llChildren;
    for (SnapshotsList::iterator it = llParent.begin();
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

    m->pParent.setNull();
}

////////////////////////////////////////////////////////////////////////////////
//
// ISnapshot public methods
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Snapshot::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->uuid.toUtf16().cloneTo(aId);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strName.cloneTo(aName);
    return S_OK;
}

/**
 *  @note Locks this object for writing, then calls Machine::onSnapshotChange()
 *  (see its lock requirements).
 */
STDMETHODIMP Snapshot::COMSETTER(Name)(IN_BSTR aName)
{
    HRESULT rc = S_OK;
    CheckComArgStrNotEmptyOrNull(aName);

    // prohibit setting a UUID only as the machine name, or else it can
    // never be found by findMachine()
    Guid test(aName);

    if (!test.isZero() && test.isValid())
        return setError(E_INVALIDARG,  tr("A machine cannot have a UUID as its name"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Utf8Str strName(aName);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->strName != strName)
    {
        m->strName = strName;
        alock.release(); /* Important! (child->parent locks are forbidden) */
        rc = m->pMachine->onSnapshotChange(this);
    }

    return rc;
}

STDMETHODIMP Snapshot::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strDescription.cloneTo(aDescription);
    return S_OK;
}

STDMETHODIMP Snapshot::COMSETTER(Description)(IN_BSTR aDescription)
{
    HRESULT rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Utf8Str strDescription(aDescription);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->strDescription != strDescription)
    {
        m->strDescription = strDescription;
        alock.release(); /* Important! (child->parent locks are forbidden) */
        rc = m->pMachine->onSnapshotChange(this);
    }

    return rc;
}

STDMETHODIMP Snapshot::COMGETTER(TimeStamp)(LONG64 *aTimeStamp)
{
    CheckComArgOutPointerValid(aTimeStamp);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeStamp = RTTimeSpecGetMilli(&m->timeStamp);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Online)(BOOL *aOnline)
{
    CheckComArgOutPointerValid(aOnline);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOnline = getStateFilePath().isNotEmpty();
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Machine)(IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pMachine.queryInterfaceTo(aMachine);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Parent)(ISnapshot **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pParent.queryInterfaceTo(aParent);
    return S_OK;
}

STDMETHODIMP Snapshot::COMGETTER(Children)(ComSafeArrayOut(ISnapshot *, aChildren))
{
    CheckComArgOutSafeArrayPointerValid(aChildren);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // snapshots tree is protected by machine lock
    AutoReadLock alock(m->pMachine COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<ISnapshot> collection(m->llChildren);
    collection.detachTo(ComSafeArrayOutArg(aChildren));

    return S_OK;
}

STDMETHODIMP Snapshot::GetChildrenCount(ULONG* count)
{
    CheckComArgOutPointerValid(count);

    *count = getChildrenCount();

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
//
// Snapshot public internal methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the parent snapshot or NULL if there's none.  Must have caller + locking!
 * @return
 */
const ComObjPtr<Snapshot>& Snapshot::getParent() const
{
    return m->pParent;
}

/**
 * Returns the first child snapshot or NULL if there's none.  Must have caller + locking!
 * @return
 */
const ComObjPtr<Snapshot> Snapshot::getFirstChild() const
{
    if (!m->llChildren.size())
        return NULL;
    return m->llChildren.front();
}

/**
 *  @note
 *      Must be called from under the object's lock!
 */
const Utf8Str& Snapshot::getStateFilePath() const
{
    return m->pMachine->mSSData->strStateFilePath;
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

    // snapshots tree is protected by machine lock
    AutoReadLock alock(m->pMachine COMMA_LOCKVAL_SRC_POS);

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

    // snapshots tree is protected by machine lock
    AutoReadLock alock(m->pMachine COMMA_LOCKVAL_SRC_POS);

    return getAllChildrenCountImpl();
}

/**
 * Returns the SnapshotMachine that this snapshot belongs to.
 * Caller must hold the snapshot's object lock!
 * @return
 */
const ComObjPtr<SnapshotMachine>& Snapshot::getSnapshotMachine() const
{
    return m->pMachine;
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
 *
 *  Caller must hold the machine lock (which protects the snapshots tree!)
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(IN_GUID aId)
{
    ComObjPtr<Snapshot> child;

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    // no need to lock, uuid is const
    if (m->uuid == aId)
        child = this;
    else
    {
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
 *
 *  Caller must hold the machine lock (which protects the snapshots tree!)
 */
ComObjPtr<Snapshot> Snapshot::findChildOrSelf(const Utf8Str &aName)
{
    ComObjPtr<Snapshot> child;
    AssertReturn(!aName.isEmpty(), child);

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->strName == aName)
        child = this;
    else
    {
        alock.release();
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
void Snapshot::updateSavedStatePathsImpl(const Utf8Str &strOldPath,
                                         const Utf8Str &strNewPath)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const Utf8Str &path = m->pMachine->mSSData->strStateFilePath;
    LogFlowThisFunc(("Snap[%s].statePath={%s}\n", m->strName.c_str(), path.c_str()));

    /* state file may be NULL (for offline snapshots) */
    if (    path.length()
         && RTPathStartsWith(path.c_str(), strOldPath.c_str())
       )
    {
        m->pMachine->mSSData->strStateFilePath = Utf8StrFmt("%s%s",
                                                            strNewPath.c_str(),
                                                            path.c_str() + strOldPath.length());
        LogFlowThisFunc(("-> updated: {%s}\n", path.c_str()));
    }

    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        Snapshot *pChild = *it;
        pChild->updateSavedStatePathsImpl(strOldPath, strNewPath);
    }
}

/**
 * Returns true if this snapshot or one of its children uses the given file,
 * whose path must be fully qualified, as its saved state. When invoked on a
 * machine's first snapshot, this can be used to check if a saved state file
 * is shared with any snapshots.
 *
 * Caller must hold the machine lock, which protects the snapshots tree.
 *
 * @param strPath
 * @param pSnapshotToIgnore If != NULL, this snapshot is ignored during the checks.
 * @return
 */
bool Snapshot::sharesSavedStateFile(const Utf8Str &strPath,
                                    Snapshot *pSnapshotToIgnore)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    const Utf8Str &path = m->pMachine->mSSData->strStateFilePath;

    if (!pSnapshotToIgnore || pSnapshotToIgnore != this)
        if (path.isNotEmpty())
            if (path == strPath)
                return true;        // no need to recurse then

    // but otherwise we must check children
    for (SnapshotsList::const_iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        Snapshot *pChild = *it;
        if (!pSnapshotToIgnore || pSnapshotToIgnore != pChild)
            if (pChild->sharesSavedStateFile(strPath, pSnapshotToIgnore))
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
 *  @note Locks the machine (for the snapshots tree) +  this object + children for writing.
 */
void Snapshot::updateSavedStatePaths(const Utf8Str &strOldPath,
                                     const Utf8Str &strNewPath)
{
    LogFlowThisFunc(("aOldPath={%s} aNewPath={%s}\n", strOldPath.c_str(), strNewPath.c_str()));

    AutoCaller autoCaller(this);
    AssertComRC(autoCaller.rc());

    // snapshots tree is protected by machine lock
    AutoWriteLock alock(m->pMachine COMMA_LOCKVAL_SRC_POS);

    // call the implementation under the tree lock
    updateSavedStatePathsImpl(strOldPath, strNewPath);
}

/**
 * Internal implementation for Snapshot::saveSnapshot (below). Caller has
 * requested the snapshots tree (machine) lock.
 *
 * @param aNode
 * @param aAttrsOnly
 * @return
 */
HRESULT Snapshot::saveSnapshotImpl(settings::Snapshot &data, bool aAttrsOnly)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.uuid = m->uuid;
    data.strName = m->strName;
    data.timestamp = m->timeStamp;
    data.strDescription = m->strDescription;

    if (aAttrsOnly)
        return S_OK;

    // state file (only if this snapshot is online)
    if (getStateFilePath().isNotEmpty())
        m->pMachine->copyPathRelativeToMachine(getStateFilePath(), data.strStateFile);
    else
        data.strStateFile.setNull();

    HRESULT rc = m->pMachine->saveHardware(data.hardware, &data.debugging, &data.autostart);
    if (FAILED(rc)) return rc;

    rc = m->pMachine->saveStorageControllers(data.storage);
    if (FAILED(rc)) return rc;

    alock.release();

    data.llChildSnapshots.clear();

    if (m->llChildren.size())
    {
        for (SnapshotsList::const_iterator it = m->llChildren.begin();
             it != m->llChildren.end();
             ++it)
        {
            settings::Snapshot snap;
            rc = (*it)->saveSnapshotImpl(snap, aAttrsOnly);
            if (FAILED(rc)) return rc;

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
 *  @param aAttrsOnly   If true, only update user-changeable attrs.
 */
HRESULT Snapshot::saveSnapshot(settings::Snapshot &data, bool aAttrsOnly)
{
    // snapshots tree is protected by machine lock
    AutoReadLock alock(m->pMachine COMMA_LOCKVAL_SRC_POS);

    return saveSnapshotImpl(data, aAttrsOnly);
}

/**
 * Part of the cleanup engine of Machine::Unregister().
 *
 * This recursively removes all medium attachments from the snapshot's machine
 * and returns the snapshot's saved state file name, if any, and then calls
 * uninit() on "this" itself.
 *
 * This recurses into children first, so the given MediaList receives child
 * media first before their parents. If the caller wants to close all media,
 * they should go thru the list from the beginning to the end because media
 * cannot be closed if they have children.
 *
 * This calls uninit() on itself, so the snapshots tree (beginning with a machine's pFirstSnapshot) becomes invalid after this.
 * It does not alter the main machine's snapshot pointers (pFirstSnapshot, pCurrentSnapshot).
 *
 * Caller must hold the machine write lock (which protects the snapshots tree!)
 *
 * @param writeLock Machine write lock, which can get released temporarily here.
 * @param cleanupMode Cleanup mode; see Machine::detachAllMedia().
 * @param llMedia List of media returned to caller, depending on cleanupMode.
 * @param llFilenames
 * @return
 */
HRESULT Snapshot::uninitRecursively(AutoWriteLock &writeLock,
                                    CleanupMode_T cleanupMode,
                                    MediaList &llMedia,
                                    std::list<Utf8Str> &llFilenames)
{
    Assert(m->pMachine->isWriteLockOnCurrentThread());

    HRESULT rc = S_OK;

    // make a copy of the Guid for logging before we uninit ourselves
#ifdef LOG_ENABLED
    Guid uuid = getId();
    Utf8Str name = getName();
    LogFlowThisFunc(("Entering for snapshot '%s' {%RTuuid}\n", name.c_str(), uuid.raw()));
#endif

    // recurse into children first so that the child media appear on
    // the list first; this way caller can close the media from the
    // beginning to the end because parent media can't be closed if
    // they have children

    // make a copy of the children list since uninit() modifies it
    SnapshotsList llChildrenCopy(m->llChildren);
    for (SnapshotsList::iterator it = llChildrenCopy.begin();
         it != llChildrenCopy.end();
         ++it)
    {
        Snapshot *pChild = *it;
        rc = pChild->uninitRecursively(writeLock, cleanupMode, llMedia, llFilenames);
        if (FAILED(rc))
            return rc;
    }

    // now call detachAllMedia on the snapshot machine
    rc = m->pMachine->detachAllMedia(writeLock,
                                     this /* pSnapshot */,
                                     cleanupMode,
                                     llMedia);
    if (FAILED(rc))
        return rc;

    // report the saved state file if it's not on the list yet
    if (!m->pMachine->mSSData->strStateFilePath.isEmpty())
    {
        bool fFound = false;
        for (std::list<Utf8Str>::const_iterator it = llFilenames.begin();
             it != llFilenames.end();
             ++it)
        {
            const Utf8Str &str = *it;
            if (str == m->pMachine->mSSData->strStateFilePath)
            {
                fFound = true;
                break;
            }
        }
        if (!fFound)
            llFilenames.push_back(m->pMachine->mSSData->strStateFilePath);
    }

    this->beginSnapshotDelete();
    this->uninit();

#ifdef LOG_ENABLED
    LogFlowThisFunc(("Leaving for snapshot '%s' {%RTuuid}\n", name.c_str(), uuid.raw()));
#endif

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
//
// SnapshotMachine implementation
//
////////////////////////////////////////////////////////////////////////////////

SnapshotMachine::SnapshotMachine()
    : mMachine(NULL)
{}

SnapshotMachine::~SnapshotMachine()
{}

HRESULT SnapshotMachine::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    return BaseFinalConstruct();
}

void SnapshotMachine::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();

    BaseFinalRelease();
}

/**
 *  Initializes the SnapshotMachine object when taking a snapshot.
 *
 *  @param aSessionMachine  machine to take a snapshot from
 *  @param aSnapshotId      snapshot ID of this snapshot machine
 *  @param aStateFilePath   file where the execution state will be later saved
 *                          (or NULL for the offline snapshot)
 *
 *  @note The aSessionMachine must be locked for writing.
 */
HRESULT SnapshotMachine::init(SessionMachine *aSessionMachine,
                              IN_GUID aSnapshotId,
                              const Utf8Str &aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%s}\n", aSessionMachine->mUserData->s.strName.c_str()));

    Guid l_guid(aSnapshotId);
    AssertReturn(aSessionMachine && (!l_guid.isZero() && l_guid.isValid()), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    AssertReturn(aSessionMachine->isWriteLockOnCurrentThread(), E_FAIL);

    mSnapshotId = aSnapshotId;
    ComObjPtr<Machine> pMachine = aSessionMachine->mPeer;

    /* mPeer stays NULL */
    /* memorize the primary Machine instance (i.e. not SessionMachine!) */
    unconst(mMachine) = pMachine;
    /* share the parent pointer */
    unconst(mParent) = pMachine->mParent;

    /* take the pointer to Data to share */
    mData.share(pMachine->mData);

    /* take the pointer to UserData to share (our UserData must always be the
     * same as Machine's data) */
    mUserData.share(pMachine->mUserData);
    /* make a private copy of all other data (recent changes from SessionMachine) */
    mHWData.attachCopy(aSessionMachine->mHWData);
    mMediaData.attachCopy(aSessionMachine->mMediaData);

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->strStateFilePath = aStateFilePath;

    HRESULT rc = S_OK;

    /* create copies of all shared folders (mHWData after attaching a copy
     * contains just references to original objects) */
    for (HWData::SharedFolderList::iterator it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++it)
    {
        ComObjPtr<SharedFolder> folder;
        folder.createObject();
        rc = folder->initCopy(this, *it);
        if (FAILED(rc)) return rc;
        *it = folder;
    }

    /* associate hard disks with the snapshot
     * (Machine::uninitDataAndChildObjects() will deassociate at destruction) */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        MediumAttachment *pAtt = *it;
        Medium *pMedium = pAtt->getMedium();
        if (pMedium) // can be NULL for non-harddisk
        {
            rc = pMedium->addBackReference(mData->mUuid, mSnapshotId);
            AssertComRC(rc);
        }
    }

    /* create copies of all storage controllers (mStorageControllerData
     * after attaching a copy contains just references to original objects) */
    mStorageControllers.allocate();
    for (StorageControllerList::const_iterator
         it = aSessionMachine->mStorageControllers->begin();
         it != aSessionMachine->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctrl;
        ctrl.createObject();
        ctrl->initCopy(this, *it);
        mStorageControllers->push_back(ctrl);
    }

    /* create all other child objects that will be immutable private copies */

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->initCopy(this, pMachine->mBIOSSettings);

    unconst(mVRDEServer).createObject();
    mVRDEServer->initCopy(this, pMachine->mVRDEServer);

    unconst(mAudioAdapter).createObject();
    mAudioAdapter->initCopy(this, pMachine->mAudioAdapter);

    unconst(mUSBController).createObject();
    mUSBController->initCopy(this, pMachine->mUSBController);

    mNetworkAdapters.resize(pMachine->mNetworkAdapters.size());
    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->initCopy(this, pMachine->mNetworkAdapters[slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
    {
        unconst(mSerialPorts[slot]).createObject();
        mSerialPorts[slot]->initCopy(this, pMachine->mSerialPorts[slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
    {
        unconst(mParallelPorts[slot]).createObject();
        mParallelPorts[slot]->initCopy(this, pMachine->mParallelPorts[slot]);
    }

    unconst(mBandwidthControl).createObject();
    mBandwidthControl->initCopy(this, pMachine->mBandwidthControl);

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the SnapshotMachine object when loading from the settings file.
 *
 *  @param aMachine machine the snapshot belongs to
 *  @param aHWNode          <Hardware> node
 *  @param aHDAsNode        <HardDiskAttachments> node
 *  @param aSnapshotId      snapshot ID of this snapshot machine
 *  @param aStateFilePath   file where the execution state is saved
 *                          (or NULL for the offline snapshot)
 *
 *  @note Doesn't lock anything.
 */
HRESULT SnapshotMachine::initFromSettings(Machine *aMachine,
                                          const settings::Hardware &hardware,
                                          const settings::Debugging *pDbg,
                                          const settings::Autostart *pAutostart,
                                          const settings::Storage &storage,
                                          IN_GUID aSnapshotId,
                                          const Utf8Str &aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%s}\n", aMachine->mUserData->s.strName.c_str()));

    Guid l_guid(aSnapshotId);
    AssertReturn(aMachine && (!l_guid.isZero() && l_guid.isValid()), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Don't need to lock aMachine when VirtualBox is starting up */

    mSnapshotId = aSnapshotId;

    /* mPeer stays NULL */
    /* memorize the primary Machine instance (i.e. not SessionMachine!) */
    unconst(mMachine) = aMachine;
    /* share the parent pointer */
    unconst(mParent) = aMachine->mParent;

    /* take the pointer to Data to share */
    mData.share(aMachine->mData);
    /*
     *  take the pointer to UserData to share
     *  (our UserData must always be the same as Machine's data)
     */
    mUserData.share(aMachine->mUserData);
    /* allocate private copies of all other data (will be loaded from settings) */
    mHWData.allocate();
    mMediaData.allocate();
    mStorageControllers.allocate();

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->strStateFilePath = aStateFilePath;

    /* create all other child objects that will be immutable private copies */

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init(this);

    unconst(mVRDEServer).createObject();
    mVRDEServer->init(this);

    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init(this);

    unconst(mUSBController).createObject();
    mUSBController->init(this);

    mNetworkAdapters.resize(Global::getMaxNetworkAdapters(mHWData->mChipsetType));
    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->init(this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
    {
        unconst(mSerialPorts[slot]).createObject();
        mSerialPorts[slot]->init(this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
    {
        unconst(mParallelPorts[slot]).createObject();
        mParallelPorts[slot]->init(this, slot);
    }

    unconst(mBandwidthControl).createObject();
    mBandwidthControl->init(this);

    /* load hardware and harddisk settings */

    HRESULT rc = loadHardware(hardware, pDbg, pAutostart);
    if (SUCCEEDED(rc))
        rc = loadStorageControllers(storage,
                                    NULL, /* puuidRegistry */
                                    &mSnapshotId);

    if (SUCCEEDED(rc))
        /* commit all changes made during the initialization */
        commit();   /// @todo r=dj why do we need a commit in init?!? this is very expensive
        /// @todo r=klaus for some reason the settings loading logic backs up
        // the settings, and therefore a commit is needed. Should probably be changed.

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Uninitializes this SnapshotMachine object.
 */
void SnapshotMachine::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    unconst(mMachine) = NULL;
    unconst(mParent) = NULL;
    unconst(mPeer) = NULL;

    LogFlowThisFuncLeave();
}

/**
 *  Overrides VirtualBoxBase::lockHandle() in order to share the lock handle
 *  with the primary Machine instance (mMachine) if it exists.
 */
RWLockHandle *SnapshotMachine::lockHandle() const
{
    AssertReturn(mMachine != NULL, NULL);
    return mMachine->lockHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// SnapshotMachine public internal methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 *  Called by the snapshot object associated with this SnapshotMachine when
 *  snapshot data such as name or description is changed.
 *
 *  @warning Caller must hold no locks when calling this.
 */
HRESULT SnapshotMachine::onSnapshotChange(Snapshot *aSnapshot)
{
    AutoMultiWriteLock2 mlock(this, aSnapshot COMMA_LOCKVAL_SRC_POS);
    Guid uuidMachine(mData->mUuid),
         uuidSnapshot(aSnapshot->getId());
    bool fNeedsGlobalSaveSettings = false;

    /* Flag the machine as dirty or change won't get saved. We disable the
     * modification of the current state flag, cause this snapshot data isn't
     * related to the current state. */
    mMachine->setModified(Machine::IsModified_Snapshots, false /* fAllowStateModification */);
    HRESULT rc = mMachine->saveSettings(&fNeedsGlobalSaveSettings,
                                        SaveS_Force);        // we know we need saving, no need to check
    mlock.release();

    if (SUCCEEDED(rc) && fNeedsGlobalSaveSettings)
    {
        // save the global settings
        AutoWriteLock vboxlock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    /* inform callbacks */
    mParent->onSnapshotChange(uuidMachine, uuidSnapshot);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// SessionMachine task records
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Abstract base class for SessionMachine::RestoreSnapshotTask and
 * SessionMachine::DeleteSnapshotTask. This is necessary since
 * RTThreadCreate cannot call a method as its thread function, so
 * instead we have it call the static SessionMachine::taskHandler,
 * which can then call the handler() method in here (implemented
 * by the children).
 */
struct SessionMachine::SnapshotTask
{
    SnapshotTask(SessionMachine *m,
                 Progress *p,
                 Snapshot *s)
        : pMachine(m),
          pProgress(p),
          machineStateBackup(m->mData->mMachineState), // save the current machine state
          pSnapshot(s)
    {}

    void modifyBackedUpState(MachineState_T s)
    {
        *const_cast<MachineState_T*>(&machineStateBackup) = s;
    }

    virtual void handler() = 0;

    ComObjPtr<SessionMachine>       pMachine;
    ComObjPtr<Progress>             pProgress;
    const MachineState_T            machineStateBackup;
    ComObjPtr<Snapshot>             pSnapshot;
};

/** Restore snapshot state task */
struct SessionMachine::RestoreSnapshotTask
    : public SessionMachine::SnapshotTask
{
    RestoreSnapshotTask(SessionMachine *m,
                        Progress *p,
                        Snapshot *s)
        : SnapshotTask(m, p, s)
    {}

    void handler()
    {
        pMachine->restoreSnapshotHandler(*this);
    }
};

/** Delete snapshot task */
struct SessionMachine::DeleteSnapshotTask
    : public SessionMachine::SnapshotTask
{
    DeleteSnapshotTask(SessionMachine *m,
                       Progress *p,
                       bool fDeleteOnline,
                       Snapshot *s)
        : SnapshotTask(m, p, s),
          m_fDeleteOnline(fDeleteOnline)
    {}

    void handler()
    {
        pMachine->deleteSnapshotHandler(*this);
    }

    bool m_fDeleteOnline;
};

/**
 * Static SessionMachine method that can get passed to RTThreadCreate to
 * have a thread started for a SnapshotTask. See SnapshotTask above.
 *
 * This calls either RestoreSnapshotTask::handler() or DeleteSnapshotTask::handler().
 */

/* static */ DECLCALLBACK(int) SessionMachine::taskHandler(RTTHREAD /* thread */, void *pvUser)
{
    AssertReturn(pvUser, VERR_INVALID_POINTER);

    SnapshotTask *task = static_cast<SnapshotTask*>(pvUser);
    task->handler();

    // it's our responsibility to delete the task
    delete task;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// TakeSnapshot methods (SessionMachine and related tasks)
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation for IInternalMachineControl::beginTakingSnapshot().
 *
 * Gets called indirectly from Console::TakeSnapshot, which creates a
 * progress object in the client and then starts a thread
 * (Console::fntTakeSnapshotWorker) which then calls this.
 *
 * In other words, the asynchronous work for taking snapshots takes place
 * on the _client_ (in the Console). This is different from restoring
 * or deleting snapshots, which start threads on the server.
 *
 * This does the server-side work of taking a snapshot: it creates differencing
 * images for all hard disks attached to the machine and then creates a
 * Snapshot object with a corresponding SnapshotMachine to save the VM settings.
 *
 * The client's fntTakeSnapshotWorker() blocks while this takes place.
 * After this returns successfully, fntTakeSnapshotWorker() will begin
 * saving the machine state to the snapshot object and reconfigure the
 * hard disks.
 *
 * When the console is done, it calls SessionMachine::EndTakingSnapshot().
 *
 *  @note Locks mParent + this object for writing.
 *
 * @param aInitiator in: The console on which Console::TakeSnapshot was called.
 * @param aName  in: The name for the new snapshot.
 * @param aDescription  in: A description for the new snapshot.
 * @param aConsoleProgress  in: The console's (client's) progress object.
 * @param fTakingSnapshotOnline  in: True if an online snapshot is being taken (i.e. machine is running).
 * @param aStateFilePath out: name of file in snapshots folder to which the console should write the VM state.
 * @return
 */
STDMETHODIMP SessionMachine::BeginTakingSnapshot(IConsole *aInitiator,
                                                 IN_BSTR aName,
                                                 IN_BSTR aDescription,
                                                 IProgress *aConsoleProgress,
                                                 BOOL fTakingSnapshotOnline,
                                                 BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    AssertReturn(aInitiator && aName, E_INVALIDARG);
    AssertReturn(aStateFilePath, E_POINTER);

    LogFlowThisFunc(("aName='%ls' fTakingSnapshotOnline=%RTbool\n", aName, fTakingSnapshotOnline));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    !Global::IsOnlineOrTransient(mData->mMachineState)
                  || mData->mMachineState == MachineState_Running
                  || mData->mMachineState == MachineState_Paused, E_FAIL);
    AssertReturn(mConsoleTaskData.mLastState == MachineState_Null, E_FAIL);
    AssertReturn(mConsoleTaskData.mSnapshot.isNull(), E_FAIL);

    if (    !fTakingSnapshotOnline
         && mData->mMachineState != MachineState_Saved
       )
    {
        /* save all current settings to ensure current changes are committed and
         * hard disks are fixed up */
        HRESULT rc = saveSettings(NULL);
                // no need to check for whether VirtualBox.xml needs changing since
                // we can't have a machine XML rename pending at this point
        if (FAILED(rc)) return rc;
    }

    /* create an ID for the snapshot */
    Guid snapshotId;
    snapshotId.create();

    Utf8Str strStateFilePath;
    /* stateFilePath is null when the machine is not online nor saved */
    if (fTakingSnapshotOnline)
    {
        Bstr value;
        HRESULT rc = GetExtraData(Bstr("VBoxInternal2/ForceTakeSnapshotWithoutState").raw(),
                                  value.asOutParam());
        if (FAILED(rc) || value != "1")
        {
            // creating a new online snapshot: we need a fresh saved state file
            composeSavedStateFilename(strStateFilePath);
        }
    }
    else if (mData->mMachineState == MachineState_Saved)
        // taking an online snapshot from machine in "saved" state: then use existing state file
        strStateFilePath = mSSData->strStateFilePath;

    if (strStateFilePath.isNotEmpty())
    {
        // ensure the directory for the saved state file exists
        HRESULT rc = VirtualBox::ensureFilePathExists(strStateFilePath, true /* fCreate */);
        if (FAILED(rc)) return rc;
    }

    /* create a snapshot machine object */
    ComObjPtr<SnapshotMachine> snapshotMachine;
    snapshotMachine.createObject();
    HRESULT rc = snapshotMachine->init(this, snapshotId.ref(), strStateFilePath);
    AssertComRCReturn(rc, rc);

    /* create a snapshot object */
    RTTIMESPEC time;
    ComObjPtr<Snapshot> pSnapshot;
    pSnapshot.createObject();
    rc = pSnapshot->init(mParent,
                         snapshotId,
                         aName,
                         aDescription,
                         *RTTimeNow(&time),
                         snapshotMachine,
                         mData->mCurrentSnapshot);
    AssertComRCReturnRC(rc);

    /* fill in the snapshot data */
    mConsoleTaskData.mLastState = mData->mMachineState;
    mConsoleTaskData.mSnapshot = pSnapshot;
    /// @todo in the long run the progress object should be moved to
    // VBoxSVC to avoid trouble with monitoring the progress object state
    // when the process where it lives is terminating shortly after the
    // operation completed.

    try
    {
        LogFlowThisFunc(("Creating differencing hard disks (online=%d)...\n",
                         fTakingSnapshotOnline));

        // backup the media data so we can recover if things goes wrong along the day;
        // the matching commit() is in fixupMedia() during endSnapshot()
        setModified(IsModified_Storage);
        mMediaData.backup();

        /* Console::fntTakeSnapshotWorker and friends expects this. */
        if (mConsoleTaskData.mLastState == MachineState_Running)
            setMachineState(MachineState_LiveSnapshotting);
        else
            setMachineState(MachineState_Saving); /** @todo Confusing! Saving is used for both online and offline snapshots. */

        alock.release();
        /* create new differencing hard disks and attach them to this machine */
        rc = createImplicitDiffs(aConsoleProgress,
                                 1,            // operation weight; must be the same as in Console::TakeSnapshot()
                                 !!fTakingSnapshotOnline);
        if (FAILED(rc))
            throw rc;

        // MUST NOT save the settings or the media registry here, because
        // this causes trouble with rolling back settings if the user cancels
        // taking the snapshot after the diff images have been created.
    }
    catch (HRESULT hrc)
    {
        LogThisFunc(("Caught %Rhrc [%s]\n", hrc, Global::stringifyMachineState(mData->mMachineState) ));
        if (    mConsoleTaskData.mLastState != mData->mMachineState
             && (   mConsoleTaskData.mLastState == MachineState_Running
                  ? mData->mMachineState == MachineState_LiveSnapshotting
                  : mData->mMachineState == MachineState_Saving)
           )
            setMachineState(mConsoleTaskData.mLastState);

        pSnapshot->uninit();
        pSnapshot.setNull();
        mConsoleTaskData.mLastState = MachineState_Null;
        mConsoleTaskData.mSnapshot.setNull();

        rc = hrc;

        // @todo r=dj what with the implicit diff that we created above? this is never cleaned up
    }

    if (fTakingSnapshotOnline && SUCCEEDED(rc))
        strStateFilePath.cloneTo(aStateFilePath);
    else
        *aStateFilePath = NULL;

    LogFlowThisFunc(("LEAVE - %Rhrc [%s]\n", rc, Global::stringifyMachineState(mData->mMachineState) ));
    return rc;
}

/**
 * Implementation for IInternalMachineControl::endTakingSnapshot().
 *
 * Called by the Console when it's done saving the VM state into the snapshot
 * (if online) and reconfiguring the hard disks. See BeginTakingSnapshot() above.
 *
 * This also gets called if the console part of snapshotting failed after the
 * BeginTakingSnapshot() call, to clean up the server side.
 *
 * @note Locks VirtualBox and this object for writing.
 *
 * @param aSuccess Whether Console was successful with the client-side snapshot things.
 * @return
 */
STDMETHODIMP SessionMachine::EndTakingSnapshot(BOOL aSuccess)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock machineLock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(   !aSuccess
                 || (    (    mData->mMachineState == MachineState_Saving
                           || mData->mMachineState == MachineState_LiveSnapshotting)
                      && mConsoleTaskData.mLastState != MachineState_Null
                      && !mConsoleTaskData.mSnapshot.isNull()
                    )
                 , E_FAIL);

    /*
     * Restore the state we had when BeginTakingSnapshot() was called,
     * Console::fntTakeSnapshotWorker restores its local copy when we return.
     * If the state was Running, then let Console::fntTakeSnapshotWorker do it
     * all to avoid races.
     */
    if (    mData->mMachineState != mConsoleTaskData.mLastState
         && mConsoleTaskData.mLastState != MachineState_Running
       )
        setMachineState(mConsoleTaskData.mLastState);

    ComObjPtr<Snapshot> pOldFirstSnap = mData->mFirstSnapshot;
    ComObjPtr<Snapshot> pOldCurrentSnap = mData->mCurrentSnapshot;

    bool fOnline = Global::IsOnline(mConsoleTaskData.mLastState);

    HRESULT rc = S_OK;

    if (aSuccess)
    {
        // new snapshot becomes the current one
        mData->mCurrentSnapshot = mConsoleTaskData.mSnapshot;

        /* memorize the first snapshot if necessary */
        if (!mData->mFirstSnapshot)
            mData->mFirstSnapshot = mData->mCurrentSnapshot;

        int flSaveSettings = SaveS_Force;       // do not do a deep compare in machine settings,
                                                // snapshots change, so we know we need to save
        if (!fOnline)
            /* the machine was powered off or saved when taking a snapshot, so
             * reset the mCurrentStateModified flag */
            flSaveSettings |= SaveS_ResetCurStateModified;

        rc = saveSettings(NULL, flSaveSettings);
    }

    if (aSuccess && SUCCEEDED(rc))
    {
        /* associate old hard disks with the snapshot and do locking/unlocking*/
        commitMedia(fOnline);

        /* inform callbacks */
        mParent->onSnapshotTaken(mData->mUuid,
                                 mConsoleTaskData.mSnapshot->getId());
        machineLock.release();
    }
    else
    {
        /* delete all differencing hard disks created (this will also attach
         * their parents back by rolling back mMediaData) */
        machineLock.release();

        rollbackMedia();

        mData->mFirstSnapshot = pOldFirstSnap;      // might have been changed above
        mData->mCurrentSnapshot = pOldCurrentSnap;      // might have been changed above

        // delete the saved state file (it might have been already created)
        if (fOnline)
            // no need to test for whether the saved state file is shared: an online
            // snapshot means that a new saved state file was created, which we must
            // clean up now
            RTFileDelete(mConsoleTaskData.mSnapshot->getStateFilePath().c_str());
            machineLock.acquire();


        mConsoleTaskData.mSnapshot->uninit();
        machineLock.release();

    }

    /* clear out the snapshot data */
    mConsoleTaskData.mLastState = MachineState_Null;
    mConsoleTaskData.mSnapshot.setNull();

    /* machineLock has been released already */

    mParent->saveModifiedRegistries();

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// RestoreSnapshot methods (SessionMachine and related tasks)
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation for IInternalMachineControl::restoreSnapshot().
 *
 * Gets called from Console::RestoreSnapshot(), and that's basically the
 * only thing Console does. Restoring a snapshot happens entirely on the
 * server side since the machine cannot be running.
 *
 * This creates a new thread that does the work and returns a progress
 * object to the client which is then returned to the caller of
 * Console::RestoreSnapshot().
 *
 * Actual work then takes place in RestoreSnapshotTask::handler().
 *
 * @note Locks this + children objects for writing!
 *
 * @param aInitiator in: rhe console on which Console::RestoreSnapshot was called.
 * @param aSnapshot in: the snapshot to restore.
 * @param aMachineState in: client-side machine state.
 * @param aProgress out: progress object to monitor restore thread.
 * @return
 */
STDMETHODIMP SessionMachine::RestoreSnapshot(IConsole *aInitiator,
                                             ISnapshot *aSnapshot,
                                             MachineState_T *aMachineState,
                                             IProgress **aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn(aInitiator, E_INVALIDARG);
    AssertReturn(aSnapshot && aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // machine must not be running
    ComAssertRet(!Global::IsOnlineOrTransient(mData->mMachineState),
                 E_FAIL);

    ComObjPtr<Snapshot> pSnapshot(static_cast<Snapshot*>(aSnapshot));
    ComObjPtr<SnapshotMachine> pSnapMachine = pSnapshot->getSnapshotMachine();

    // create a progress object. The number of operations is:
    // 1 (preparing) + # of hard disks + 1 (if we need to copy the saved state file) */
    LogFlowThisFunc(("Going thru snapshot machine attachments to determine progress setup\n"));

    ULONG ulOpCount = 1;            // one for preparations
    ULONG ulTotalWeight = 1;        // one for preparations
    for (MediaData::AttachmentList::iterator it = pSnapMachine->mMediaData->mAttachments.begin();
         it != pSnapMachine->mMediaData->mAttachments.end();
         ++it)
    {
        ComObjPtr<MediumAttachment> &pAttach = *it;
        AutoReadLock attachLock(pAttach COMMA_LOCKVAL_SRC_POS);
        if (pAttach->getType() == DeviceType_HardDisk)
        {
            ++ulOpCount;
            ++ulTotalWeight;         // assume one MB weight for each differencing hard disk to manage
            Assert(pAttach->getMedium());
            LogFlowThisFunc(("op %d: considering hard disk attachment %s\n", ulOpCount, pAttach->getMedium()->getName().c_str()));
        }
    }

    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    pProgress->init(mParent, aInitiator,
                    BstrFmt(tr("Restoring snapshot '%s'"), pSnapshot->getName().c_str()).raw(),
                    FALSE /* aCancelable */,
                    ulOpCount,
                    ulTotalWeight,
                    Bstr(tr("Restoring machine settings")).raw(),
                    1);

    /* create and start the task on a separate thread (note that it will not
     * start working until we release alock) */
    RestoreSnapshotTask *task = new RestoreSnapshotTask(this,
                                                        pProgress,
                                                        pSnapshot);
    int vrc = RTThreadCreate(NULL,
                             taskHandler,
                             (void*)task,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             0,
                             "RestoreSnap");
    if (RT_FAILURE(vrc))
    {
        delete task;
        ComAssertRCRet(vrc, E_FAIL);
    }

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState(MachineState_RestoringSnapshot);

    /* return the progress to the caller */
    pProgress.queryInterfaceTo(aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * Worker method for the restore snapshot thread created by SessionMachine::RestoreSnapshot().
 * This method gets called indirectly through SessionMachine::taskHandler() which then
 * calls RestoreSnapshotTask::handler().
 *
 * The RestoreSnapshotTask contains the progress object returned to the console by
 * SessionMachine::RestoreSnapshot, through which progress and results are reported.
 *
 * @note Locks mParent + this object for writing.
 *
 * @param aTask Task data.
 */
void SessionMachine::restoreSnapshotHandler(RestoreSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);

    LogFlowThisFunc(("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.pProgress->notifyComplete(E_FAIL,
                                        COM_IIDOF(IMachine),
                                        getComponentName(),
                                        tr("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    HRESULT rc = S_OK;

    bool stateRestored = false;

    try
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Discard all current changes to mUserData (name, OSType etc.).
         * Note that the machine is powered off, so there is no need to inform
         * the direct session. */
        if (mData->flModifications)
            rollback(false /* aNotify */);

        /* Delete the saved state file if the machine was Saved prior to this
         * operation */
        if (aTask.machineStateBackup == MachineState_Saved)
        {
            Assert(!mSSData->strStateFilePath.isEmpty());

            // release the saved state file AFTER unsetting the member variable
            // so that releaseSavedStateFile() won't think it's still in use
            Utf8Str strStateFile(mSSData->strStateFilePath);
            mSSData->strStateFilePath.setNull();
            releaseSavedStateFile(strStateFile, NULL /* pSnapshotToIgnore */ );

            aTask.modifyBackedUpState(MachineState_PoweredOff);

            rc = saveStateSettings(SaveSTS_StateFilePath);
            if (FAILED(rc))
                throw rc;
        }

        RTTIMESPEC snapshotTimeStamp;
        RTTimeSpecSetMilli(&snapshotTimeStamp, 0);

        {
            AutoReadLock snapshotLock(aTask.pSnapshot COMMA_LOCKVAL_SRC_POS);

            /* remember the timestamp of the snapshot we're restoring from */
            snapshotTimeStamp = aTask.pSnapshot->getTimeStamp();

            ComPtr<SnapshotMachine> pSnapshotMachine(aTask.pSnapshot->getSnapshotMachine());

            /* copy all hardware data from the snapshot */
            copyFrom(pSnapshotMachine);

            LogFlowThisFunc(("Restoring hard disks from the snapshot...\n"));

            // restore the attachments from the snapshot
            setModified(IsModified_Storage);
            mMediaData.backup();
            mMediaData->mAttachments.clear();
            for (MediaData::AttachmentList::const_iterator it = pSnapshotMachine->mMediaData->mAttachments.begin();
                 it != pSnapshotMachine->mMediaData->mAttachments.end();
                 ++it)
            {
                ComObjPtr<MediumAttachment> pAttach;
                pAttach.createObject();
                pAttach->initCopy(this, *it);
                mMediaData->mAttachments.push_back(pAttach);
            }

            /* release the locks before the potentially lengthy operation */
            snapshotLock.release();
            alock.release();

            rc = createImplicitDiffs(aTask.pProgress,
                                     1,
                                     false /* aOnline */);
            if (FAILED(rc))
                throw rc;

            alock.acquire();
            snapshotLock.acquire();

            /* Note: on success, current (old) hard disks will be
             * deassociated/deleted on #commit() called from #saveSettings() at
             * the end. On failure, newly created implicit diffs will be
             * deleted by #rollback() at the end. */

            /* should not have a saved state file associated at this point */
            Assert(mSSData->strStateFilePath.isEmpty());

            const Utf8Str &strSnapshotStateFile = aTask.pSnapshot->getStateFilePath();

            if (strSnapshotStateFile.isNotEmpty())
                // online snapshot: then share the state file
                mSSData->strStateFilePath = strSnapshotStateFile;

            LogFlowThisFunc(("Setting new current snapshot {%RTuuid}\n", aTask.pSnapshot->getId().raw()));
            /* make the snapshot we restored from the current snapshot */
            mData->mCurrentSnapshot = aTask.pSnapshot;
        }

        /* grab differencing hard disks from the old attachments that will
         * become unused and need to be auto-deleted */
        std::list< ComObjPtr<MediumAttachment> > llDiffAttachmentsToDelete;

        for (MediaData::AttachmentList::const_iterator it = mMediaData.backedUpData()->mAttachments.begin();
             it != mMediaData.backedUpData()->mAttachments.end();
             ++it)
        {
            ComObjPtr<MediumAttachment> pAttach = *it;
            ComObjPtr<Medium> pMedium = pAttach->getMedium();

            /* while the hard disk is attached, the number of children or the
             * parent cannot change, so no lock */
            if (    !pMedium.isNull()
                 && pAttach->getType() == DeviceType_HardDisk
                 && !pMedium->getParent().isNull()
                 && pMedium->getChildren().size() == 0
               )
            {
                LogFlowThisFunc(("Picked differencing image '%s' for deletion\n", pMedium->getName().c_str()));

                llDiffAttachmentsToDelete.push_back(pAttach);
            }
        }

        /* we have already deleted the current state, so set the execution
         * state accordingly no matter of the delete snapshot result */
        if (mSSData->strStateFilePath.isNotEmpty())
            setMachineState(MachineState_Saved);
        else
            setMachineState(MachineState_PoweredOff);

        updateMachineStateOnClient();
        stateRestored = true;

        /* Paranoia: no one must have saved the settings in the mean time. If
         * it happens nevertheless we'll close our eyes and continue below. */
        Assert(mMediaData.isBackedUp());

        /* assign the timestamp from the snapshot */
        Assert(RTTimeSpecGetMilli(&snapshotTimeStamp) != 0);
        mData->mLastStateChange = snapshotTimeStamp;

        // detach the current-state diffs that we detected above and build a list of
        // image files to delete _after_ saveSettings()

        MediaList llDiffsToDelete;

        for (std::list< ComObjPtr<MediumAttachment> >::iterator it = llDiffAttachmentsToDelete.begin();
             it != llDiffAttachmentsToDelete.end();
             ++it)
        {
            ComObjPtr<MediumAttachment> pAttach = *it;        // guaranteed to have only attachments where medium != NULL
            ComObjPtr<Medium> pMedium = pAttach->getMedium();

            AutoWriteLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);

            LogFlowThisFunc(("Detaching old current state in differencing image '%s'\n", pMedium->getName().c_str()));

            // Normally we "detach" the medium by removing the attachment object
            // from the current machine data; saveSettings() below would then
            // compare the current machine data with the one in the backup
            // and actually call Medium::removeBackReference(). But that works only half
            // the time in our case so instead we force a detachment here:
            // remove from machine data
            mMediaData->mAttachments.remove(pAttach);
            // Remove it from the backup or else saveSettings will try to detach
            // it again and assert. The paranoia check avoids crashes (see
            // assert above) if this code is buggy and saves settings in the
            // wrong place.
            if (mMediaData.isBackedUp())
                mMediaData.backedUpData()->mAttachments.remove(pAttach);
            // then clean up backrefs
            pMedium->removeBackReference(mData->mUuid);

            llDiffsToDelete.push_back(pMedium);
        }

        // save machine settings, reset the modified flag and commit;
        bool fNeedsGlobalSaveSettings = false;
        rc = saveSettings(&fNeedsGlobalSaveSettings,
                          SaveS_ResetCurStateModified);
        if (FAILED(rc))
            throw rc;
        // unconditionally add the parent registry. We do similar in SessionMachine::EndTakingSnapshot
        // (mParent->saveSettings())

        // release the locks before updating registry and deleting image files
        alock.release();

        mParent->markRegistryModified(mParent->getGlobalRegistryId());

        // from here on we cannot roll back on failure any more

        for (MediaList::iterator it = llDiffsToDelete.begin();
             it != llDiffsToDelete.end();
             ++it)
        {
            ComObjPtr<Medium> &pMedium = *it;
            LogFlowThisFunc(("Deleting old current state in differencing image '%s'\n", pMedium->getName().c_str()));

            HRESULT rc2 = pMedium->deleteStorage(NULL /* aProgress */,
                                                 true /* aWait */);
            // ignore errors here because we cannot roll back after saveSettings() above
            if (SUCCEEDED(rc2))
                pMedium->uninit();
        }
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (FAILED(rc))
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        /* undo all changes on failure */
        rollback(false /* aNotify */);

        if (!stateRestored)
        {
            /* restore the machine state */
            setMachineState(aTask.machineStateBackup);
            updateMachineStateOnClient();
        }
    }

    mParent->saveModifiedRegistries();

    /* set the result (this will try to fetch current error info on failure) */
    aTask.pProgress->notifyComplete(rc);

    if (SUCCEEDED(rc))
        mParent->onSnapshotDeleted(mData->mUuid, Guid());

    LogFlowThisFunc(("Done restoring snapshot (rc=%08X)\n", rc));

    LogFlowThisFuncLeave();
}

////////////////////////////////////////////////////////////////////////////////
//
// DeleteSnapshot methods (SessionMachine and related tasks)
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation for IInternalMachineControl::deleteSnapshot().
 *
 * Gets called from Console::DeleteSnapshot(), and that's basically the
 * only thing Console does initially. Deleting a snapshot happens entirely on
 * the server side if the machine is not running, and if it is running then
 * the individual merges are done via internal session callbacks.
 *
 * This creates a new thread that does the work and returns a progress
 * object to the client which is then returned to the caller of
 * Console::DeleteSnapshot().
 *
 * Actual work then takes place in DeleteSnapshotTask::handler().
 *
 * @note Locks mParent + this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DeleteSnapshot(IConsole *aInitiator,
                                            IN_BSTR aStartId,
                                            IN_BSTR aEndId,
                                            BOOL fDeleteAllChildren,
                                            MachineState_T *aMachineState,
                                            IProgress **aProgress)
{
    LogFlowThisFuncEnter();

    Guid startId(aStartId);
    Guid endId(aEndId);

    AssertReturn(aInitiator && !startId.isZero() && !endId.isZero() && startId.isValid() && endId.isValid(), E_INVALIDARG);

    AssertReturn(aMachineState && aProgress, E_POINTER);

    /** @todo implement the "and all children" and "range" variants */
    if (fDeleteAllChildren || startId != endId)
        ReturnComNotImplemented();

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // be very picky about machine states
    if (   Global::IsOnlineOrTransient(mData->mMachineState)
        && mData->mMachineState != MachineState_PoweredOff
        && mData->mMachineState != MachineState_Saved
        && mData->mMachineState != MachineState_Teleported
        && mData->mMachineState != MachineState_Aborted
        && mData->mMachineState != MachineState_Running
        && mData->mMachineState != MachineState_Paused)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Invalid machine state: %s"),
                        Global::stringifyMachineState(mData->mMachineState));

    ComObjPtr<Snapshot> pSnapshot;
    HRESULT rc = findSnapshotById(startId, pSnapshot, true /* aSetError */);
    if (FAILED(rc)) return rc;

    AutoWriteLock snapshotLock(pSnapshot COMMA_LOCKVAL_SRC_POS);

    size_t childrenCount = pSnapshot->getChildrenCount();
    if (childrenCount > 1)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Snapshot '%s' of the machine '%s' cannot be deleted. because it has %d child snapshots, which is more than the one snapshot allowed for deletion"),
                        pSnapshot->getName().c_str(),
                        mUserData->s.strName.c_str(),
                        childrenCount);

    /* If the snapshot being deleted is the current one, ensure current
     * settings are committed and saved.
     */
    if (pSnapshot == mData->mCurrentSnapshot)
    {
        if (mData->flModifications)
        {
            rc = saveSettings(NULL);
                // no need to change for whether VirtualBox.xml needs saving since
                // we can't have a machine XML rename pending at this point
            if (FAILED(rc)) return rc;
        }
    }

    ComObjPtr<SnapshotMachine> pSnapMachine = pSnapshot->getSnapshotMachine();

    /* create a progress object. The number of operations is:
     *   1 (preparing) + 1 if the snapshot is online + # of normal hard disks
     */
    LogFlowThisFunc(("Going thru snapshot machine attachments to determine progress setup\n"));

    ULONG ulOpCount = 1;            // one for preparations
    ULONG ulTotalWeight = 1;        // one for preparations

    if (pSnapshot->getStateFilePath().length())
    {
        ++ulOpCount;
        ++ulTotalWeight;            // assume 1 MB for deleting the state file
    }

    // count normal hard disks and add their sizes to the weight
    for (MediaData::AttachmentList::iterator it = pSnapMachine->mMediaData->mAttachments.begin();
         it != pSnapMachine->mMediaData->mAttachments.end();
         ++it)
    {
        ComObjPtr<MediumAttachment> &pAttach = *it;
        AutoReadLock attachLock(pAttach COMMA_LOCKVAL_SRC_POS);
        if (pAttach->getType() == DeviceType_HardDisk)
        {
            ComObjPtr<Medium> pHD = pAttach->getMedium();
            Assert(pHD);
            AutoReadLock mlock(pHD COMMA_LOCKVAL_SRC_POS);

            MediumType_T type = pHD->getType();
            // writethrough and shareable images are unaffected by snapshots,
            // so do nothing for them
            if (   type != MediumType_Writethrough
                && type != MediumType_Shareable
                && type != MediumType_Readonly)
            {
                // normal or immutable media need attention
                ++ulOpCount;
                ulTotalWeight += (ULONG)(pHD->getSize() / _1M);
            }
            LogFlowThisFunc(("op %d: considering hard disk attachment %s\n", ulOpCount, pHD->getName().c_str()));
        }
    }

    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    pProgress->init(mParent, aInitiator,
                    BstrFmt(tr("Deleting snapshot '%s'"), pSnapshot->getName().c_str()).raw(),
                    FALSE /* aCancelable */,
                    ulOpCount,
                    ulTotalWeight,
                    Bstr(tr("Setting up")).raw(),
                    1);

    bool fDeleteOnline = (   (mData->mMachineState == MachineState_Running)
                          || (mData->mMachineState == MachineState_Paused));

    /* create and start the task on a separate thread */
    DeleteSnapshotTask *task = new DeleteSnapshotTask(this, pProgress,
                                                      fDeleteOnline, pSnapshot);
    int vrc = RTThreadCreate(NULL,
                             taskHandler,
                             (void*)task,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             0,
                             "DeleteSnapshot");
    if (RT_FAILURE(vrc))
    {
        delete task;
        return E_FAIL;
    }

    // the task might start running but will block on acquiring the machine's write lock
    // which we acquired above; once this function leaves, the task will be unblocked;
    // set the proper machine state here now (note: after creating a Task instance)
    if (mData->mMachineState == MachineState_Running)
        setMachineState(MachineState_DeletingSnapshotOnline);
    else if (mData->mMachineState == MachineState_Paused)
        setMachineState(MachineState_DeletingSnapshotPaused);
    else
        setMachineState(MachineState_DeletingSnapshot);

    /* return the progress to the caller */
    pProgress.queryInterfaceTo(aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * Helper struct for SessionMachine::deleteSnapshotHandler().
 */
struct MediumDeleteRec
{
    MediumDeleteRec()
        : mfNeedsOnlineMerge(false),
          mpMediumLockList(NULL)
    {}

    MediumDeleteRec(const ComObjPtr<Medium> &aHd,
                    const ComObjPtr<Medium> &aSource,
                    const ComObjPtr<Medium> &aTarget,
                    const ComObjPtr<MediumAttachment> &aOnlineMediumAttachment,
                    bool fMergeForward,
                    const ComObjPtr<Medium> &aParentForTarget,
                    const MediaList &aChildrenToReparent,
                    bool fNeedsOnlineMerge,
                    MediumLockList *aMediumLockList)
        : mpHD(aHd),
          mpSource(aSource),
          mpTarget(aTarget),
          mpOnlineMediumAttachment(aOnlineMediumAttachment),
          mfMergeForward(fMergeForward),
          mpParentForTarget(aParentForTarget),
          mChildrenToReparent(aChildrenToReparent),
          mfNeedsOnlineMerge(fNeedsOnlineMerge),
          mpMediumLockList(aMediumLockList)
    {}

    MediumDeleteRec(const ComObjPtr<Medium> &aHd,
                    const ComObjPtr<Medium> &aSource,
                    const ComObjPtr<Medium> &aTarget,
                    const ComObjPtr<MediumAttachment> &aOnlineMediumAttachment,
                    bool fMergeForward,
                    const ComObjPtr<Medium> &aParentForTarget,
                    const MediaList &aChildrenToReparent,
                    bool fNeedsOnlineMerge,
                    MediumLockList *aMediumLockList,
                    const Guid &aMachineId,
                    const Guid &aSnapshotId)
        : mpHD(aHd),
          mpSource(aSource),
          mpTarget(aTarget),
          mpOnlineMediumAttachment(aOnlineMediumAttachment),
          mfMergeForward(fMergeForward),
          mpParentForTarget(aParentForTarget),
          mChildrenToReparent(aChildrenToReparent),
          mfNeedsOnlineMerge(fNeedsOnlineMerge),
          mpMediumLockList(aMediumLockList),
          mMachineId(aMachineId),
          mSnapshotId(aSnapshotId)
    {}

    ComObjPtr<Medium> mpHD;
    ComObjPtr<Medium> mpSource;
    ComObjPtr<Medium> mpTarget;
    ComObjPtr<MediumAttachment> mpOnlineMediumAttachment;
    bool mfMergeForward;
    ComObjPtr<Medium> mpParentForTarget;
    MediaList mChildrenToReparent;
    bool mfNeedsOnlineMerge;
    MediumLockList *mpMediumLockList;
    /* these are for reattaching the hard disk in case of a failure: */
    Guid mMachineId;
    Guid mSnapshotId;
};

typedef std::list<MediumDeleteRec> MediumDeleteRecList;

/**
 * Worker method for the delete snapshot thread created by
 * SessionMachine::DeleteSnapshot().  This method gets called indirectly
 * through SessionMachine::taskHandler() which then calls
 * DeleteSnapshotTask::handler().
 *
 * The DeleteSnapshotTask contains the progress object returned to the console
 * by SessionMachine::DeleteSnapshot, through which progress and results are
 * reported.
 *
 * SessionMachine::DeleteSnapshot() has set the machine state to
 * MachineState_DeletingSnapshot right after creating this task. Since we block
 * on the machine write lock at the beginning, once that has been acquired, we
 * can assume that the machine state is indeed that.
 *
 * @note Locks the machine + the snapshot + the media tree for writing!
 *
 * @param aTask Task data.
 */

void SessionMachine::deleteSnapshotHandler(DeleteSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);

    LogFlowThisFunc(("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.pProgress->notifyComplete(E_FAIL,
                                        COM_IIDOF(IMachine),
                                        getComponentName(),
                                        tr("The session has been accidentally closed"));
        LogFlowThisFuncLeave();
        return;
    }

    HRESULT rc = S_OK;
    MediumDeleteRecList toDelete;
    Guid snapshotId;

    try
    {
        /* Locking order:  */
        AutoMultiWriteLock2 multiLock(this->lockHandle(),                   // machine
                                      aTask.pSnapshot->lockHandle()         // snapshot
                                      COMMA_LOCKVAL_SRC_POS);
        // once we have this lock, we know that SessionMachine::DeleteSnapshot()
        // has exited after setting the machine state to MachineState_DeletingSnapshot

        AutoWriteLock treeLock(mParent->getMediaTreeLockHandle()
                                      COMMA_LOCKVAL_SRC_POS);

        ComObjPtr<SnapshotMachine> pSnapMachine = aTask.pSnapshot->getSnapshotMachine();
        // no need to lock the snapshot machine since it is const by definition
        Guid machineId = pSnapMachine->getId();

        // save the snapshot ID (for callbacks)
        snapshotId = aTask.pSnapshot->getId();

        // first pass:
        LogFlowThisFunc(("1: Checking hard disk merge prerequisites...\n"));

        // Go thru the attachments of the snapshot machine (the media in here
        // point to the disk states _before_ the snapshot was taken, i.e. the
        // state we're restoring to; for each such medium, we will need to
        // merge it with its one and only child (the diff image holding the
        // changes written after the snapshot was taken).
        for (MediaData::AttachmentList::iterator it = pSnapMachine->mMediaData->mAttachments.begin();
             it != pSnapMachine->mMediaData->mAttachments.end();
             ++it)
        {
            ComObjPtr<MediumAttachment> &pAttach = *it;
            AutoReadLock attachLock(pAttach COMMA_LOCKVAL_SRC_POS);
            if (pAttach->getType() != DeviceType_HardDisk)
                continue;

            ComObjPtr<Medium> pHD = pAttach->getMedium();
            Assert(!pHD.isNull());

            {
                // writethrough, shareable and readonly images are
                // unaffected by snapshots, skip them
                AutoReadLock medlock(pHD COMMA_LOCKVAL_SRC_POS);
                MediumType_T type = pHD->getType();
                if (   type == MediumType_Writethrough
                    || type == MediumType_Shareable
                    || type == MediumType_Readonly)
                    continue;
            }

#ifdef DEBUG
            pHD->dumpBackRefs();
#endif

            // needs to be merged with child or deleted, check prerequisites
            ComObjPtr<Medium> pTarget;
            ComObjPtr<Medium> pSource;
            bool fMergeForward = false;
            ComObjPtr<Medium> pParentForTarget;
            MediaList childrenToReparent;
            bool fNeedsOnlineMerge = false;
            bool fOnlineMergePossible = aTask.m_fDeleteOnline;
            MediumLockList *pMediumLockList = NULL;
            MediumLockList *pVMMALockList = NULL;
            ComObjPtr<MediumAttachment> pOnlineMediumAttachment;
            if (fOnlineMergePossible)
            {
                // Look up the corresponding medium attachment in the currently
                // running VM. Any failure prevents a live merge. Could be made
                // a tad smarter by trying a few candidates, so that e.g. disks
                // which are simply moved to a different controller slot do not
                // prevent online merging in general.
                pOnlineMediumAttachment =
                    findAttachment(mMediaData->mAttachments,
                                   pAttach->getControllerName().raw(),
                                   pAttach->getPort(),
                                   pAttach->getDevice());
                if (pOnlineMediumAttachment)
                {
                    rc = mData->mSession.mLockedMedia.Get(pOnlineMediumAttachment,
                                                          pVMMALockList);
                    if (FAILED(rc))
                        fOnlineMergePossible = false;
                }
                else
                    fOnlineMergePossible = false;
            }

            // no need to hold the lock any longer
            attachLock.release();

            treeLock.release();
            rc = prepareDeleteSnapshotMedium(pHD, machineId, snapshotId,
                                             fOnlineMergePossible,
                                             pVMMALockList, pSource, pTarget,
                                             fMergeForward, pParentForTarget,
                                             childrenToReparent,
                                             fNeedsOnlineMerge,
                                             pMediumLockList);
            treeLock.acquire();
            if (FAILED(rc))
                throw rc;

            // For simplicity, prepareDeleteSnapshotMedium selects the merge
            // direction in the following way: we merge pHD onto its child
            // (forward merge), not the other way round, because that saves us
            // from unnecessarily shuffling around the attachments for the
            // machine that follows the snapshot (next snapshot or current
            // state), unless it's a base image. Backwards merges of the first
            // snapshot into the base image is essential, as it ensures that
            // when all snapshots are deleted the only remaining image is a
            // base image. Important e.g. for medium formats which do not have
            // a file representation such as iSCSI.

            // a couple paranoia checks for backward merges
            if (pMediumLockList != NULL && !fMergeForward)
            {
                // parent is null -> this disk is a base hard disk: we will
                // then do a backward merge, i.e. merge its only child onto the
                // base disk. Here we need then to update the attachment that
                // refers to the child and have it point to the parent instead
                Assert(pHD->getChildren().size() == 1);

                ComObjPtr<Medium> pReplaceHD = pHD->getChildren().front();

                ComAssertThrow(pReplaceHD == pSource, E_FAIL);
            }

            Guid replaceMachineId;
            Guid replaceSnapshotId;

            const Guid *pReplaceMachineId = pSource->getFirstMachineBackrefId();
            // minimal sanity checking
            Assert(!pReplaceMachineId || *pReplaceMachineId == mData->mUuid);
            if (pReplaceMachineId)
                replaceMachineId = *pReplaceMachineId;

            const Guid *pSnapshotId = pSource->getFirstMachineBackrefSnapshotId();
            if (pSnapshotId)
                replaceSnapshotId = *pSnapshotId;

            if (replaceMachineId.isValid() && !replaceMachineId.isZero())
            {
                // Adjust the backreferences, otherwise merging will assert.
                // Note that the medium attachment object stays associated
                // with the snapshot until the merge was successful.
                HRESULT rc2 = S_OK;
                rc2 = pSource->removeBackReference(replaceMachineId, replaceSnapshotId);
                AssertComRC(rc2);

                toDelete.push_back(MediumDeleteRec(pHD, pSource, pTarget,
                                                   pOnlineMediumAttachment,
                                                   fMergeForward,
                                                   pParentForTarget,
                                                   childrenToReparent,
                                                   fNeedsOnlineMerge,
                                                   pMediumLockList,
                                                   replaceMachineId,
                                                   replaceSnapshotId));
            }
            else
                toDelete.push_back(MediumDeleteRec(pHD, pSource, pTarget,
                                                   pOnlineMediumAttachment,
                                                   fMergeForward,
                                                   pParentForTarget,
                                                   childrenToReparent,
                                                   fNeedsOnlineMerge,
                                                   pMediumLockList));
        }

        {/* see @bugref{4386} */
            /*check available place on the storage*/
          RTFOFF pcbTotal = 0;
          RTFOFF pcbFree = 0;
          uint32_t pcbBlock = 0;
          uint32_t pcbSector = 0;
          std::multimap<uint32_t,uint64_t> neededStorageFreeSpace;
          std::map<uint32_t,const char*> serialMapToStoragePath;

            MediumDeleteRecList::const_iterator it_md = toDelete.begin();

            while (it_md != toDelete.end())
            {
                uint64_t diskSize = 0;
                uint32_t pu32Serial = 0;
                ComObjPtr<Medium> pSource_local = it_md->mpSource;
                ComObjPtr<Medium> pTarget_local = it_md->mpTarget;
                ComPtr<IMediumFormat> pTargetFormat;

                {
                    if ( pSource_local.isNull() ||
                         pSource_local == pTarget_local )
                    {
                        ++it_md;
                        continue;
                    }
                }

                rc = pTarget_local->COMGETTER(MediumFormat)(pTargetFormat.asOutParam());
                if (FAILED(rc))
                    throw rc;
                ULONG uTargetCaps = 0;
                rc = pTargetFormat->COMGETTER(Capabilities)(&uTargetCaps);
                if (FAILED(rc))
                    throw rc;

                if (uTargetCaps & MediumFormatCapabilities_File)
                {
                    int vrc = RTFsQuerySerial(pTarget_local->getLocationFull().c_str(), &pu32Serial);
                    if (RT_FAILURE(vrc))
                    {
                        rc = setError(E_FAIL,
                                      tr(" Unable to merge storage '%s'. Can't get storage UID "),
                                      pTarget_local->getLocationFull().c_str());
                        throw rc;
                    }

                    pSource_local->COMGETTER(Size)((LONG64*)&diskSize);

                    /* store needed free space in multimap */
                    neededStorageFreeSpace.insert(std::make_pair(pu32Serial,diskSize));
                    /* linking storage UID with snapshot path, it is a helper container (just for easy finding needed path) */
                    serialMapToStoragePath.insert(std::make_pair(pu32Serial,pTarget_local->getLocationFull().c_str()));
                }

                ++it_md;
            }

            while (!neededStorageFreeSpace.empty())
            {
                std::pair<std::multimap<uint32_t,uint64_t>::iterator,std::multimap<uint32_t,uint64_t>::iterator> ret;
                uint64_t commonSourceStoragesSize = 0;

                /* find all records in multimap with identical storage UID*/
                ret = neededStorageFreeSpace.equal_range(neededStorageFreeSpace.begin()->first);
                std::multimap<uint32_t,uint64_t>::const_iterator it_ns = ret.first;

                for (; it_ns != ret.second ; ++it_ns)
                {
                    commonSourceStoragesSize += it_ns->second;
                }

                /* find appropriate path by storage UID*/
                std::map<uint32_t,const char*>::const_iterator it_sm = serialMapToStoragePath.find(ret.first->first);
                /* get info about a storage */
                if (it_sm == serialMapToStoragePath.end())
                {
                    LogFlowThisFunc((" Path to the storage wasn't found...\n "));

                    rc = setError(E_INVALIDARG,
                                      tr(" Unable to merge storage '%s'. Path to the storage wasn't found. "),
                                      it_sm->second);
                    throw rc;
                }

                int vrc = RTFsQuerySizes(it_sm->second, &pcbTotal, &pcbFree,&pcbBlock, &pcbSector);
                if (RT_FAILURE(vrc))
                {
                    rc = setError(E_FAIL,
                                      tr(" Unable to merge storage '%s'. Can't get the storage size. "),
                                      it_sm->second);
                    throw rc;
                }

                if (commonSourceStoragesSize > (uint64_t)pcbFree)
                {
                    LogFlowThisFunc((" Not enough free space to merge...\n "));

                    rc = setError(E_OUTOFMEMORY,
                                      tr(" Unable to merge storage '%s' - not enough free storage space. "),
                                      it_sm->second);
                    throw rc;
                }

                neededStorageFreeSpace.erase(ret.first, ret.second);
            }

            serialMapToStoragePath.clear();
        }

        // we can release the locks now since the machine state is MachineState_DeletingSnapshot
        treeLock.release();
        multiLock.release();

        /* Now we checked that we can successfully merge all normal hard disks
         * (unless a runtime error like end-of-disc happens). Now get rid of
         * the saved state (if present), as that will free some disk space.
         * The snapshot itself will be deleted as late as possible, so that
         * the user can repeat the delete operation if he runs out of disk
         * space or cancels the delete operation. */

        /* second pass: */
        LogFlowThisFunc(("2: Deleting saved state...\n"));

        {
            // saveAllSnapshots() needs a machine lock, and the snapshots
            // tree is protected by the machine lock as well
            AutoWriteLock machineLock(this COMMA_LOCKVAL_SRC_POS);

            Utf8Str stateFilePath = aTask.pSnapshot->getStateFilePath();
            if (!stateFilePath.isEmpty())
            {
                aTask.pProgress->SetNextOperation(Bstr(tr("Deleting the execution state")).raw(),
                                                  1);        // weight

                releaseSavedStateFile(stateFilePath, aTask.pSnapshot /* pSnapshotToIgnore */);

                // machine will need saving now
                machineLock.release();
                mParent->markRegistryModified(getId());
            }
        }

        /* third pass: */
        LogFlowThisFunc(("3: Performing actual hard disk merging...\n"));

        /// @todo NEWMEDIA turn the following errors into warnings because the
        /// snapshot itself has been already deleted (and interpret these
        /// warnings properly on the GUI side)
        for (MediumDeleteRecList::iterator it = toDelete.begin();
             it != toDelete.end();)
        {
            const ComObjPtr<Medium> &pMedium(it->mpHD);
            ULONG ulWeight;

            {
                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);
                ulWeight = (ULONG)(pMedium->getSize() / _1M);
            }

            aTask.pProgress->SetNextOperation(BstrFmt(tr("Merging differencing image '%s'"),
                                              pMedium->getName().c_str()).raw(),
                                              ulWeight);

            bool fNeedSourceUninit = false;
            bool fReparentTarget = false;
            if (it->mpMediumLockList == NULL)
            {
                /* no real merge needed, just updating state and delete
                 * diff files if necessary */
                AutoMultiWriteLock2 mLock(&mParent->getMediaTreeLockHandle(), pMedium->lockHandle() COMMA_LOCKVAL_SRC_POS);

                Assert(   !it->mfMergeForward
                       || pMedium->getChildren().size() == 0);

                /* Delete the differencing hard disk (has no children). Two
                 * exceptions: if it's the last medium in the chain or if it's
                 * a backward merge we don't want to handle due to complexity.
                 * In both cases leave the image in place. If it's the first
                 * exception the user can delete it later if he wants. */
                if (!pMedium->getParent().isNull())
                {
                    Assert(pMedium->getState() == MediumState_Deleting);
                    /* No need to hold the lock any longer. */
                    mLock.release();
                    rc = pMedium->deleteStorage(&aTask.pProgress,
                                                true /* aWait */);
                    if (FAILED(rc))
                        throw rc;

                    // need to uninit the deleted medium
                    fNeedSourceUninit = true;
                }
            }
            else
            {
                bool fNeedsSave = false;
                if (it->mfNeedsOnlineMerge)
                {
                    // online medium merge, in the direction decided earlier
                    rc = onlineMergeMedium(it->mpOnlineMediumAttachment,
                                           it->mpSource,
                                           it->mpTarget,
                                           it->mfMergeForward,
                                           it->mpParentForTarget,
                                           it->mChildrenToReparent,
                                           it->mpMediumLockList,
                                           aTask.pProgress,
                                           &fNeedsSave);
                }
                else
                {
                    // normal medium merge, in the direction decided earlier
                    rc = it->mpSource->mergeTo(it->mpTarget,
                                               it->mfMergeForward,
                                               it->mpParentForTarget,
                                               it->mChildrenToReparent,
                                               it->mpMediumLockList,
                                               &aTask.pProgress,
                                               true /* aWait */);
                }

                // If the merge failed, we need to do our best to have a usable
                // VM configuration afterwards. The return code doesn't tell
                // whether the merge completed and so we have to check if the
                // source medium (diff images are always file based at the
                // moment) is still there or not. Be careful not to lose the
                // error code below, before the "Delayed failure exit".
                if (FAILED(rc))
                {
                    AutoReadLock mlock(it->mpSource COMMA_LOCKVAL_SRC_POS);
                    if (!it->mpSource->isMediumFormatFile())
                        // Diff medium not backed by a file - cannot get status so
                        // be pessimistic.
                        throw rc;
                    const Utf8Str &loc = it->mpSource->getLocationFull();
                    // Source medium is still there, so merge failed early.
                    if (RTFileExists(loc.c_str()))
                        throw rc;

                    // Source medium is gone. Assume the merge succeeded and
                    // thus it's safe to remove the attachment. We use the
                    // "Delayed failure exit" below.
                }

                // need to change the medium attachment for backward merges
                fReparentTarget = !it->mfMergeForward;

                if (!it->mfNeedsOnlineMerge)
                {
                    // need to uninit the medium deleted by the merge
                    fNeedSourceUninit = true;

                    // delete the no longer needed medium lock list, which
                    // implicitly handled the unlocking
                    delete it->mpMediumLockList;
                    it->mpMediumLockList = NULL;
                }
            }

            // Now that the medium is successfully merged/deleted/whatever,
            // remove the medium attachment from the snapshot. For a backwards
            // merge the target attachment needs to be removed from the
            // snapshot, as the VM will take it over. For forward merges the
            // source medium attachment needs to be removed.
            ComObjPtr<MediumAttachment> pAtt;
            if (fReparentTarget)
            {
                pAtt = findAttachment(pSnapMachine->mMediaData->mAttachments,
                                      it->mpTarget);
                it->mpTarget->removeBackReference(machineId, snapshotId);
            }
            else
                pAtt = findAttachment(pSnapMachine->mMediaData->mAttachments,
                                      it->mpSource);
            pSnapMachine->mMediaData->mAttachments.remove(pAtt);

            if (fReparentTarget)
            {
                // Search for old source attachment and replace with target.
                // There can be only one child snapshot in this case.
                ComObjPtr<Machine> pMachine = this;
                Guid childSnapshotId;
                ComObjPtr<Snapshot> pChildSnapshot = aTask.pSnapshot->getFirstChild();
                if (pChildSnapshot)
                {
                    pMachine = pChildSnapshot->getSnapshotMachine();
                    childSnapshotId = pChildSnapshot->getId();
                }
                pAtt = findAttachment(pMachine->mMediaData->mAttachments, it->mpSource);
                if (pAtt)
                {
                    AutoWriteLock attLock(pAtt COMMA_LOCKVAL_SRC_POS);
                    pAtt->updateMedium(it->mpTarget);
                    it->mpTarget->addBackReference(pMachine->mData->mUuid, childSnapshotId);
                }
                else
                {
                    // If no attachment is found do not change anything. Maybe
                    // the source medium was not attached to the snapshot.
                    // If this is an online deletion the attachment was updated
                    // already to allow the VM continue execution immediately.
                    // Needs a bit of special treatment due to this difference.
                    if (it->mfNeedsOnlineMerge)
                        it->mpTarget->addBackReference(pMachine->mData->mUuid, childSnapshotId);
                }
            }

            if (fNeedSourceUninit)
                it->mpSource->uninit();

            // One attachment is merged, must save the settings
            mParent->markRegistryModified(getId());

            // prevent calling cancelDeleteSnapshotMedium() for this attachment
            it = toDelete.erase(it);

            // Delayed failure exit when the merge cleanup failed but the
            // merge actually succeeded.
            if (FAILED(rc))
                throw rc;
        }

        {
            // beginSnapshotDelete() needs the machine lock, and the snapshots
            // tree is protected by the machine lock as well
            AutoWriteLock machineLock(this COMMA_LOCKVAL_SRC_POS);

            aTask.pSnapshot->beginSnapshotDelete();
            aTask.pSnapshot->uninit();

            machineLock.release();
            mParent->markRegistryModified(getId());
        }
    }
    catch (HRESULT aRC) {
        rc = aRC;
    }

    if (FAILED(rc))
    {
        // preserve existing error info so that the result can
        // be properly reported to the progress object below
        ErrorInfoKeeper eik;

        AutoMultiWriteLock2 multiLock(this->lockHandle(),                   // machine
                                      &mParent->getMediaTreeLockHandle()    // media tree
                                      COMMA_LOCKVAL_SRC_POS);

        // un-prepare the remaining hard disks
        for (MediumDeleteRecList::const_iterator it = toDelete.begin();
             it != toDelete.end();
             ++it)
        {
            cancelDeleteSnapshotMedium(it->mpHD, it->mpSource,
                                       it->mChildrenToReparent,
                                       it->mfNeedsOnlineMerge,
                                       it->mpMediumLockList, it->mMachineId,
                                       it->mSnapshotId);
        }
    }

    // whether we were successful or not, we need to set the machine
    // state and save the machine settings;
    {
        // preserve existing error info so that the result can
        // be properly reported to the progress object below
        ErrorInfoKeeper eik;

        // restore the machine state that was saved when the
        // task was started
        setMachineState(aTask.machineStateBackup);
        updateMachineStateOnClient();

        mParent->saveModifiedRegistries();
    }

    // report the result (this will try to fetch current error info on failure)
    aTask.pProgress->notifyComplete(rc);

    if (SUCCEEDED(rc))
        mParent->onSnapshotDeleted(mData->mUuid, snapshotId);

    LogFlowThisFunc(("Done deleting snapshot (rc=%08X)\n", rc));
    LogFlowThisFuncLeave();
}

/**
 * Checks that this hard disk (part of a snapshot) may be deleted/merged and
 * performs necessary state changes. Must not be called for writethrough disks
 * because there is nothing to delete/merge then.
 *
 * This method is to be called prior to calling #deleteSnapshotMedium().
 * If #deleteSnapshotMedium() is not called or fails, the state modifications
 * performed by this method must be undone by #cancelDeleteSnapshotMedium().
 *
 * @return COM status code
 * @param aHD           Hard disk which is connected to the snapshot.
 * @param aMachineId    UUID of machine this hard disk is attached to.
 * @param aSnapshotId   UUID of snapshot this hard disk is attached to. May
 *                      be a zero UUID if no snapshot is applicable.
 * @param fOnlineMergePossible Flag whether an online merge is possible.
 * @param aVMMALockList Medium lock list for the medium attachment of this VM.
 *                      Only used if @a fOnlineMergePossible is @c true, and
 *                      must be non-NULL in this case.
 * @param aSource       Source hard disk for merge (out).
 * @param aTarget       Target hard disk for merge (out).
 * @param aMergeForward Merge direction decision (out).
 * @param aParentForTarget New parent if target needs to be reparented (out).
 * @param aChildrenToReparent Children which have to be reparented to the
 *                      target (out).
 * @param fNeedsOnlineMerge Whether this merge needs to be done online (out).
 *                      If this is set to @a true then the @a aVMMALockList
 *                      parameter has been modified and is returned as
 *                      @a aMediumLockList.
 * @param aMediumLockList Where to store the created medium lock list (may
 *                      return NULL if no real merge is necessary).
 *
 * @note Caller must hold media tree lock for writing. This locks this object
 *       and every medium object on the merge chain for writing.
 */
HRESULT SessionMachine::prepareDeleteSnapshotMedium(const ComObjPtr<Medium> &aHD,
                                                    const Guid &aMachineId,
                                                    const Guid &aSnapshotId,
                                                    bool fOnlineMergePossible,
                                                    MediumLockList *aVMMALockList,
                                                    ComObjPtr<Medium> &aSource,
                                                    ComObjPtr<Medium> &aTarget,
                                                    bool &aMergeForward,
                                                    ComObjPtr<Medium> &aParentForTarget,
                                                    MediaList &aChildrenToReparent,
                                                    bool &fNeedsOnlineMerge,
                                                    MediumLockList * &aMediumLockList)
{
    Assert(!mParent->getMediaTreeLockHandle().isWriteLockOnCurrentThread());
    Assert(!fOnlineMergePossible || VALID_PTR(aVMMALockList));

    AutoWriteLock alock(aHD COMMA_LOCKVAL_SRC_POS);

    // Medium must not be writethrough/shareable/readonly at this point
    MediumType_T type = aHD->getType();
    AssertReturn(   type != MediumType_Writethrough
                 && type != MediumType_Shareable
                 && type != MediumType_Readonly, E_FAIL);

    aMediumLockList = NULL;
    fNeedsOnlineMerge = false;

    if (aHD->getChildren().size() == 0)
    {
        /* This technically is no merge, set those values nevertheless.
         * Helps with updating the medium attachments. */
        aSource = aHD;
        aTarget = aHD;

        /* special treatment of the last hard disk in the chain: */
        if (aHD->getParent().isNull())
        {
            /* lock only, to prevent any usage until the snapshot deletion
             * is completed */
            alock.release();
            return aHD->LockWrite(NULL);
        }

        /* the differencing hard disk w/o children will be deleted, protect it
         * from attaching to other VMs (this is why Deleting) */
        return aHD->markForDeletion();
   }

    /* not going multi-merge as it's too expensive */
    if (aHD->getChildren().size() > 1)
        return setError(E_FAIL,
                        tr("Hard disk '%s' has more than one child hard disk (%d)"),
                        aHD->getLocationFull().c_str(),
                        aHD->getChildren().size());

    ComObjPtr<Medium> pChild = aHD->getChildren().front();

    AutoWriteLock childLock(pChild COMMA_LOCKVAL_SRC_POS);

    /* the rest is a normal merge setup */
    if (aHD->getParent().isNull())
    {
        /* base hard disk, backward merge */
        const Guid *pMachineId1 = pChild->getFirstMachineBackrefId();
        const Guid *pMachineId2 = aHD->getFirstMachineBackrefId();
        if (pMachineId1 && pMachineId2 && *pMachineId1 != *pMachineId2)
        {
            /* backward merge is too tricky, we'll just detach on snapshot
             * deletion, so lock only, to prevent any usage */
            childLock.release();
            alock.release();
            return aHD->LockWrite(NULL);
        }

        aSource = pChild;
        aTarget = aHD;
    }
    else
    {
        /* Determine best merge direction. */
        bool fMergeForward = true;

        childLock.release();
        alock.release();
        HRESULT rc = aHD->queryPreferredMergeDirection(pChild, fMergeForward);
        alock.acquire();
        childLock.acquire();

        if (FAILED(rc) && rc != E_FAIL)
            return rc;

        if (fMergeForward)
        {
            aSource = aHD;
            aTarget = pChild;
            LogFlowFunc(("Forward merging selected\n"));
        }
        else
        {
            aSource = pChild;
            aTarget = aHD;
            LogFlowFunc(("Backward merging selected\n"));
        }
    }

    HRESULT rc;
    childLock.release();
    alock.release();
    rc = aSource->prepareMergeTo(aTarget, &aMachineId, &aSnapshotId,
                                 !fOnlineMergePossible /* fLockMedia */,
                                 aMergeForward, aParentForTarget,
                                 aChildrenToReparent, aMediumLockList);
    alock.acquire();
    childLock.acquire();
    if (SUCCEEDED(rc) && fOnlineMergePossible)
    {
        /* Try to lock the newly constructed medium lock list. If it succeeds
         * this can be handled as an offline merge, i.e. without the need of
         * asking the VM to do the merging. Only continue with the online
         * merging preparation if applicable. */
        childLock.release();
        alock.release();
        rc = aMediumLockList->Lock();
        alock.acquire();
        childLock.acquire();
        if (FAILED(rc) && fOnlineMergePossible)
        {
            /* Locking failed, this cannot be done as an offline merge. Try to
             * combine the locking information into the lock list of the medium
             * attachment in the running VM. If that fails or locking the
             * resulting lock list fails then the merge cannot be done online.
             * It can be repeated by the user when the VM is shut down. */
            MediumLockList::Base::iterator lockListVMMABegin =
                aVMMALockList->GetBegin();
            MediumLockList::Base::iterator lockListVMMAEnd =
                aVMMALockList->GetEnd();
            MediumLockList::Base::iterator lockListBegin =
                aMediumLockList->GetBegin();
            MediumLockList::Base::iterator lockListEnd =
                aMediumLockList->GetEnd();
            for (MediumLockList::Base::iterator it = lockListVMMABegin,
                 it2 = lockListBegin;
                 it2 != lockListEnd;
                 ++it, ++it2)
            {
                if (   it == lockListVMMAEnd
                    || it->GetMedium() != it2->GetMedium())
                {
                    fOnlineMergePossible = false;
                    break;
                }
                bool fLockReq = (it2->GetLockRequest() || it->GetLockRequest());
                childLock.release();
                alock.release();
                rc = it->UpdateLock(fLockReq);
                alock.acquire();
                childLock.acquire();
                if (FAILED(rc))
                {
                    // could not update the lock, trigger cleanup below
                    fOnlineMergePossible = false;
                    break;
                }
            }

            if (fOnlineMergePossible)
            {
                /* we will lock the children of the source for reparenting */
                for (MediaList::const_iterator it = aChildrenToReparent.begin();
                     it != aChildrenToReparent.end();
                     ++it)
                {
                    ComObjPtr<Medium> pMedium = *it;
                    AutoReadLock mediumLock(pMedium COMMA_LOCKVAL_SRC_POS);
                    if (pMedium->getState() == MediumState_Created)
                    {
                        mediumLock.release();
                        childLock.release();
                        alock.release();
                        rc = pMedium->LockWrite(NULL);
                        alock.acquire();
                        childLock.acquire();
                        mediumLock.acquire();
                        if (FAILED(rc))
                            throw rc;
                    }
                    else
                    {
                        mediumLock.release();
                        childLock.release();
                        alock.release();
                        rc = aVMMALockList->Update(pMedium, true);
                        alock.acquire();
                        childLock.acquire();
                        mediumLock.acquire();
                        if (FAILED(rc))
                        {
                            mediumLock.release();
                            childLock.release();
                            alock.release();
                            rc = pMedium->LockWrite(NULL);
                            alock.acquire();
                            childLock.acquire();
                            mediumLock.acquire();
                            if (FAILED(rc))
                                throw rc;
                        }
                    }
                }
            }

            if (fOnlineMergePossible)
            {
                childLock.release();
                alock.release();
                rc = aVMMALockList->Lock();
                alock.acquire();
                childLock.acquire();
                if (FAILED(rc))
                {
                    aSource->cancelMergeTo(aChildrenToReparent, aMediumLockList);
                    rc = setError(rc,
                                  tr("Cannot lock hard disk '%s' for a live merge"),
                                  aHD->getLocationFull().c_str());
                }
                else
                {
                    delete aMediumLockList;
                    aMediumLockList = aVMMALockList;
                    fNeedsOnlineMerge = true;
                }
            }
            else
            {
                aSource->cancelMergeTo(aChildrenToReparent, aMediumLockList);
                rc = setError(rc,
                              tr("Failed to construct lock list for a live merge of hard disk '%s'"),
                              aHD->getLocationFull().c_str());
            }

            // fix the VM's lock list if anything failed
            if (FAILED(rc))
            {
                lockListVMMABegin = aVMMALockList->GetBegin();
                lockListVMMAEnd = aVMMALockList->GetEnd();
                MediumLockList::Base::iterator lockListLast = lockListVMMAEnd;
                lockListLast--;
                for (MediumLockList::Base::iterator it = lockListVMMABegin;
                     it != lockListVMMAEnd;
                     ++it)
                {
                    childLock.release();
                    alock.release();
                    it->UpdateLock(it == lockListLast);
                    alock.acquire();
                    childLock.acquire();
                    ComObjPtr<Medium> pMedium = it->GetMedium();
                    AutoWriteLock mediumLock(pMedium COMMA_LOCKVAL_SRC_POS);
                    // blindly apply this, only needed for medium objects which
                    // would be deleted as part of the merge
                    pMedium->unmarkLockedForDeletion();
                }
            }

        }
        else
        {
            aSource->cancelMergeTo(aChildrenToReparent, aMediumLockList);
            rc = setError(rc,
                          tr("Cannot lock hard disk '%s' for an offline merge"),
                          aHD->getLocationFull().c_str());
        }
    }

    return rc;
}

/**
 * Cancels the deletion/merging of this hard disk (part of a snapshot). Undoes
 * what #prepareDeleteSnapshotMedium() did. Must be called if
 * #deleteSnapshotMedium() is not called or fails.
 *
 * @param aHD           Hard disk which is connected to the snapshot.
 * @param aSource       Source hard disk for merge.
 * @param aChildrenToReparent Children to unlock.
 * @param fNeedsOnlineMerge Whether this merge needs to be done online.
 * @param aMediumLockList Medium locks to cancel.
 * @param aMachineId    Machine id to attach the medium to.
 * @param aSnapshotId   Snapshot id to attach the medium to.
 *
 * @note Locks the medium tree and the hard disks in the chain for writing.
 */
void SessionMachine::cancelDeleteSnapshotMedium(const ComObjPtr<Medium> &aHD,
                                                const ComObjPtr<Medium> &aSource,
                                                const MediaList &aChildrenToReparent,
                                                bool fNeedsOnlineMerge,
                                                MediumLockList *aMediumLockList,
                                                const Guid &aMachineId,
                                                const Guid &aSnapshotId)
{
    if (aMediumLockList == NULL)
    {
        AutoMultiWriteLock2 mLock(&mParent->getMediaTreeLockHandle(), aHD->lockHandle() COMMA_LOCKVAL_SRC_POS);

        Assert(aHD->getChildren().size() == 0);

        if (aHD->getParent().isNull())
        {
            HRESULT rc = aHD->UnlockWrite(NULL);
            AssertComRC(rc);
        }
        else
        {
            HRESULT rc = aHD->unmarkForDeletion();
            AssertComRC(rc);
        }
    }
    else
    {
        if (fNeedsOnlineMerge)
        {
            // Online merge uses the medium lock list of the VM, so give
            // an empty list to cancelMergeTo so that it works as designed.
            aSource->cancelMergeTo(aChildrenToReparent, new MediumLockList());

            // clean up the VM medium lock list ourselves
            MediumLockList::Base::iterator lockListBegin =
                aMediumLockList->GetBegin();
            MediumLockList::Base::iterator lockListEnd =
                aMediumLockList->GetEnd();
            MediumLockList::Base::iterator lockListLast = lockListEnd;
            lockListLast--;
            for (MediumLockList::Base::iterator it = lockListBegin;
                 it != lockListEnd;
                 ++it)
            {
                ComObjPtr<Medium> pMedium = it->GetMedium();
                AutoWriteLock mediumLock(pMedium COMMA_LOCKVAL_SRC_POS);
                if (pMedium->getState() == MediumState_Deleting)
                    pMedium->unmarkForDeletion();
                else
                {
                    // blindly apply this, only needed for medium objects which
                    // would be deleted as part of the merge
                    pMedium->unmarkLockedForDeletion();
                }
                mediumLock.release();
                it->UpdateLock(it == lockListLast);
                mediumLock.acquire();
            }
        }
        else
        {
            aSource->cancelMergeTo(aChildrenToReparent, aMediumLockList);
        }
    }

    if (aMachineId.isValid() && !aMachineId.isZero())
    {
        // reattach the source media to the snapshot
        HRESULT rc = aSource->addBackReference(aMachineId, aSnapshotId);
        AssertComRC(rc);
    }
}

/**
 * Perform an online merge of a hard disk, i.e. the equivalent of
 * Medium::mergeTo(), just for running VMs. If this fails you need to call
 * #cancelDeleteSnapshotMedium().
 *
 * @return COM status code
 * @param aMediumAttachment Identify where the disk is attached in the VM.
 * @param aSource       Source hard disk for merge.
 * @param aTarget       Target hard disk for merge.
 * @param aMergeForward Merge direction.
 * @param aParentForTarget New parent if target needs to be reparented.
 * @param aChildrenToReparent Children which have to be reparented to the
 *                      target.
 * @param aMediumLockList Where to store the created medium lock list (may
 *                      return NULL if no real merge is necessary).
 * @param aProgress     Progress indicator.
 * @param pfNeedsMachineSaveSettings Whether the VM settings need to be saved (out).
 */
HRESULT SessionMachine::onlineMergeMedium(const ComObjPtr<MediumAttachment> &aMediumAttachment,
                                          const ComObjPtr<Medium> &aSource,
                                          const ComObjPtr<Medium> &aTarget,
                                          bool fMergeForward,
                                          const ComObjPtr<Medium> &aParentForTarget,
                                          const MediaList &aChildrenToReparent,
                                          MediumLockList *aMediumLockList,
                                          ComObjPtr<Progress> &aProgress,
                                          bool *pfNeedsMachineSaveSettings)
{
    AssertReturn(aSource != NULL, E_FAIL);
    AssertReturn(aTarget != NULL, E_FAIL);
    AssertReturn(aSource != aTarget, E_FAIL);
    AssertReturn(aMediumLockList != NULL, E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        // Similar code appears in Medium::taskMergeHandle, so
        // if you make any changes below check whether they are applicable
        // in that context as well.

        unsigned uTargetIdx = (unsigned)-1;
        unsigned uSourceIdx = (unsigned)-1;
        /* Sanity check all hard disks in the chain. */
        MediumLockList::Base::iterator lockListBegin =
            aMediumLockList->GetBegin();
        MediumLockList::Base::iterator lockListEnd =
            aMediumLockList->GetEnd();
        unsigned i = 0;
        for (MediumLockList::Base::iterator it = lockListBegin;
             it != lockListEnd;
             ++it)
        {
            MediumLock &mediumLock = *it;
            const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();

            if (pMedium == aSource)
                uSourceIdx = i;
            else if (pMedium == aTarget)
                uTargetIdx = i;

            // In Medium::taskMergeHandler there is lots of consistency
            // checking which we cannot do here, as the state details are
            // impossible to get outside the Medium class. The locking should
            // have done the checks already.

            i++;
        }

        ComAssertThrow(   uSourceIdx != (unsigned)-1
                       && uTargetIdx != (unsigned)-1, E_FAIL);

        // For forward merges, tell the VM what images need to have their
        // parent UUID updated. This cannot be done in VBoxSVC, as opening
        // the required parent images is not safe while the VM is running.
        // For backward merges this will be simply an array of size 0.
        com::SafeIfaceArray<IMedium> childrenToReparent(aChildrenToReparent);

        ComPtr<IInternalSessionControl> directControl;
        {
            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

            if (mData->mSession.mState != SessionState_Locked)
                throw setError(VBOX_E_INVALID_VM_STATE,
                                tr("Machine is not locked by a session (session state: %s)"),
                                Global::stringifySessionState(mData->mSession.mState));
            directControl = mData->mSession.mDirectControl;
        }

        // Must not hold any locks here, as this will call back to finish
        // updating the medium attachment, chain linking and state.
        rc = directControl->OnlineMergeMedium(aMediumAttachment,
                                              uSourceIdx, uTargetIdx,
                                              aSource, aTarget,
                                              fMergeForward, aParentForTarget,
                                              ComSafeArrayAsInParam(childrenToReparent),
                                              aProgress);
        if (FAILED(rc))
            throw rc;
    }
    catch (HRESULT aRC) { rc = aRC; }

    // The callback mentioned above takes care of update the medium state

    if (pfNeedsMachineSaveSettings)
        *pfNeedsMachineSaveSettings = true;

    return rc;
}

/**
 * Implementation for IInternalMachineControl::finishOnlineMergeMedium().
 *
 * Gets called after the successful completion of an online merge from
 * Console::onlineMergeMedium(), which gets invoked indirectly above in
 * the call to IInternalSessionControl::onlineMergeMedium.
 *
 * This updates the medium information and medium state so that the VM
 * can continue with the updated state of the medium chain.
 */
STDMETHODIMP SessionMachine::FinishOnlineMergeMedium(IMediumAttachment *aMediumAttachment,
                                                     IMedium *aSource,
                                                     IMedium *aTarget,
                                                     BOOL aMergeForward,
                                                     IMedium *aParentForTarget,
                                                     ComSafeArrayIn(IMedium *, aChildrenToReparent))
{
    HRESULT rc = S_OK;
    ComObjPtr<Medium> pSource(static_cast<Medium *>(aSource));
    ComObjPtr<Medium> pTarget(static_cast<Medium *>(aTarget));
    ComObjPtr<Medium> pParentForTarget(static_cast<Medium *>(aParentForTarget));
    bool fSourceHasChildren = false;

    // all hard disks but the target were successfully deleted by
    // the merge; reparent target if necessary and uninitialize media

    AutoWriteLock treeLock(mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    // Declare this here to make sure the object does not get uninitialized
    // before this method completes. Would normally happen as halfway through
    // we delete the last reference to the no longer existing medium object.
    ComObjPtr<Medium> targetChild;

    if (aMergeForward)
    {
        // first, unregister the target since it may become a base
        // hard disk which needs re-registration
        rc = mParent->unregisterMedium(pTarget);
        AssertComRC(rc);

        // then, reparent it and disconnect the deleted branch at
        // both ends (chain->parent() is source's parent)
        pTarget->deparent();
        pTarget->setParent(pParentForTarget);
        if (pParentForTarget)
            pSource->deparent();

        // then, register again
        rc = mParent->registerMedium(pTarget, &pTarget, DeviceType_HardDisk);
        AssertComRC(rc);
    }
    else
    {
        Assert(pTarget->getChildren().size() == 1);
        targetChild = pTarget->getChildren().front();

        // disconnect the deleted branch at the elder end
        targetChild->deparent();

        // Update parent UUIDs of the source's children, reparent them and
        // disconnect the deleted branch at the younger end
        com::SafeIfaceArray<IMedium> childrenToReparent(ComSafeArrayInArg(aChildrenToReparent));
        if (childrenToReparent.size() > 0)
        {
            fSourceHasChildren = true;
            // Fix the parent UUID of the images which needs to be moved to
            // underneath target. The running machine has the images opened,
            // but only for reading since the VM is paused. If anything fails
            // we must continue. The worst possible result is that the images
            // need manual fixing via VBoxManage to adjust the parent UUID.
            MediaList toReparent;
            for (size_t i = 0; i < childrenToReparent.size(); i++)
            {
                Medium *pMedium = static_cast<Medium *>(childrenToReparent[i]);
                toReparent.push_back(pMedium);
            }
            treeLock.release();
            pTarget->fixParentUuidOfChildren(toReparent);
            treeLock.acquire();

            // obey {parent,child} lock order
            AutoWriteLock sourceLock(pSource COMMA_LOCKVAL_SRC_POS);

            for (size_t i = 0; i < childrenToReparent.size(); i++)
            {
                Medium *pMedium = static_cast<Medium *>(childrenToReparent[i]);
                AutoWriteLock childLock(pMedium COMMA_LOCKVAL_SRC_POS);

                pMedium->deparent();  // removes pMedium from source
                pMedium->setParent(pTarget);
            }
        }
    }

    /* unregister and uninitialize all hard disks removed by the merge */
    MediumLockList *pMediumLockList = NULL;
    MediumAttachment *pMediumAttachment = static_cast<MediumAttachment *>(aMediumAttachment);
    rc = mData->mSession.mLockedMedia.Get(pMediumAttachment, pMediumLockList);
    const ComObjPtr<Medium> &pLast = aMergeForward ? pTarget : pSource;
    AssertReturn(SUCCEEDED(rc) && pMediumLockList, E_FAIL);
    MediumLockList::Base::iterator lockListBegin =
        pMediumLockList->GetBegin();
    MediumLockList::Base::iterator lockListEnd =
        pMediumLockList->GetEnd();
    for (MediumLockList::Base::iterator it = lockListBegin;
         it != lockListEnd;
         )
    {
        MediumLock &mediumLock = *it;
        /* Create a real copy of the medium pointer, as the medium
         * lock deletion below would invalidate the referenced object. */
        const ComObjPtr<Medium> pMedium = mediumLock.GetMedium();

        /* The target and all images not merged (readonly) are skipped */
        if (   pMedium == pTarget
            || pMedium->getState() == MediumState_LockedRead)
        {
            ++it;
        }
        else
        {
            rc = mParent->unregisterMedium(pMedium);
            AssertComRC(rc);

            /* now, uninitialize the deleted hard disk (note that
             * due to the Deleting state, uninit() will not touch
             * the parent-child relationship so we need to
             * uninitialize each disk individually) */

            /* note that the operation initiator hard disk (which is
             * normally also the source hard disk) is a special case
             * -- there is one more caller added by Task to it which
             * we must release. Also, if we are in sync mode, the
             * caller may still hold an AutoCaller instance for it
             * and therefore we cannot uninit() it (it's therefore
             * the caller's responsibility) */
            if (pMedium == aSource)
            {
                Assert(pSource->getChildren().size() == 0);
                Assert(pSource->getFirstMachineBackrefId() == NULL);
            }

            /* Delete the medium lock list entry, which also releases the
             * caller added by MergeChain before uninit() and updates the
             * iterator to point to the right place. */
            rc = pMediumLockList->RemoveByIterator(it);
            AssertComRC(rc);

            pMedium->uninit();
        }

        /* Stop as soon as we reached the last medium affected by the merge.
         * The remaining images must be kept unchanged. */
        if (pMedium == pLast)
            break;
    }

    /* Could be in principle folded into the previous loop, but let's keep
     * things simple. Update the medium locking to be the standard state:
     * all parent images locked for reading, just the last diff for writing. */
    lockListBegin = pMediumLockList->GetBegin();
    lockListEnd = pMediumLockList->GetEnd();
    MediumLockList::Base::iterator lockListLast = lockListEnd;
    lockListLast--;
    for (MediumLockList::Base::iterator it = lockListBegin;
         it != lockListEnd;
         ++it)
    {
         it->UpdateLock(it == lockListLast);
    }

    /* If this is a backwards merge of the only remaining snapshot (i.e. the
     * source has no children) then update the medium associated with the
     * attachment, as the previously associated one (source) is now deleted.
     * Without the immediate update the VM could not continue running. */
    if (!aMergeForward && !fSourceHasChildren)
    {
        AutoWriteLock attLock(pMediumAttachment COMMA_LOCKVAL_SRC_POS);
        pMediumAttachment->updateMedium(pTarget);
    }

    return S_OK;
}
