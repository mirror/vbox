/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "MediumImpl.h"

#include "VirtualBoxImpl.h"

#include "Logging.h"

#include <VBox/com/array.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/file.h>

////////////////////////////////////////////////////////////////////////////////
// MediumBase class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (MediumBase)

// protected initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

// IMedium properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumBase::COMGETTER(Id) (OUT_GUID aId)
{
    CheckComArgOutPointerValid (aId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m.id.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP MediumBase::COMGETTER(Description) (BSTR *aDescription)
{
    CheckComArgOutPointerValid (aDescription);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m.description.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP MediumBase::COMSETTER(Description) (IN_BSTR aDescription)
{
    CheckComArgNotNull (aDescription);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /// @todo update m.description and save the global registry (and local
    /// registries of portable VMs referring to this medium), this will also
    /// require to add the mRegistered flag to data

    ReturnComNotImplemented();
}

STDMETHODIMP MediumBase::COMGETTER(State) (MediaState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* queryInfo() locks this for writing. */
    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
        case MediaState_LockedRead:
        case MediaState_LockedWrite:
        {
            rc = queryInfo(true /* fWrite */);
            break;
        }
        default:
            break;
    }

    *aState = m.state;

    return rc;
}

STDMETHODIMP MediumBase::COMGETTER(Location) (BSTR *aLocation)
{
    CheckComArgOutPointerValid(aLocation);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m.locationFull.cloneTo (aLocation);

    return S_OK;
}

STDMETHODIMP MediumBase::COMSETTER(Location) (IN_BSTR aLocation)
{
    CheckComArgNotNull (aLocation);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /// @todo NEWMEDIA for file names, add the default extension if no extension
    /// is present (using the information from the VD backend which also implies
    /// that one more parameter should be passed to setLocation() requesting
    /// that functionality since it is only allwed when called from this method

    /// @todo NEWMEDIA rename the file and set m.location on success, then save
    /// the global registry (and local registries of portable VMs referring to
    /// this medium), this will also require to add the mRegistered flag to data

    ReturnComNotImplemented();
}

STDMETHODIMP MediumBase::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid (aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    name().cloneTo (aName);

    return S_OK;
}

STDMETHODIMP MediumBase::COMGETTER(Size) (ULONG64 *aSize)
{
    CheckComArgOutPointerValid (aSize);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSize = m.size;

    return S_OK;
}

STDMETHODIMP MediumBase::COMGETTER(LastAccessError) (BSTR *aLastAccessError)
{
    CheckComArgOutPointerValid (aLastAccessError);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m.lastAccessError.cloneTo (aLastAccessError);

    return S_OK;
}

STDMETHODIMP MediumBase::COMGETTER(MachineIds) (ComSafeGUIDArrayOut (aMachineIds))
{
    if (ComSafeGUIDArrayOutIsNull (aMachineIds))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    com::SafeGUIDArray machineIds;

    if (m.backRefs.size() != 0)
    {
        machineIds.reset (m.backRefs.size());

        size_t i = 0;
        for (BackRefList::const_iterator it = m.backRefs.begin();
             it != m.backRefs.end(); ++ it, ++ i)
        {
            machineIds [i] = it->machineId;
        }
    }

    machineIds.detachTo (ComSafeGUIDArrayOutArg (aMachineIds));

    return S_OK;
}

// IMedium methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumBase::GetSnapshotIds (IN_GUID aMachineId,
                                         ComSafeGUIDArrayOut (aSnapshotIds))
{
    CheckComArgExpr (aMachineId, Guid (aMachineId).isEmpty() == false);
    CheckComArgOutSafeArrayPointerValid (aSnapshotIds);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    com::SafeGUIDArray snapshotIds;

    for (BackRefList::const_iterator it = m.backRefs.begin();
         it != m.backRefs.end(); ++ it)
    {
        if (it->machineId == aMachineId)
        {
            size_t size = it->snapshotIds.size();

            /* if the medium is attached to the machine in the current state, we
             * return its ID as the first element of the array */
            if (it->inCurState)
                ++ size;

            if (size > 0)
            {
                snapshotIds.reset (size);

                size_t j = 0;
                if (it->inCurState)
                    snapshotIds [j ++] = it->machineId;

                for (BackRef::GuidList::const_iterator jt =
                        it->snapshotIds.begin();
                     jt != it->snapshotIds.end(); ++ jt, ++ j)
                {
                    snapshotIds [j] = *jt;
                }
            }

            break;
        }
    }

    snapshotIds.detachTo (ComSafeGUIDArrayOutArg (aSnapshotIds));

    return S_OK;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP MediumBase::LockRead (MediaState_T *aState)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* return the current state before */
    if (aState)
        *aState = m.state;

    HRESULT rc = S_OK;

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
        case MediaState_LockedRead:
        {
            ++ m.readers;

            ComAssertMsgBreak (m.readers != 0, ("Counter overflow"),
                               rc = E_FAIL);

            if (m.state == MediaState_Created)
                m.accessibleInLock = true;
            else if (m.state == MediaState_Inaccessible)
                m.accessibleInLock = false;

            m.state = MediaState_LockedRead;

            break;
        }
        default:
        {
            rc = setStateError();
            break;
        }
    }

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP MediumBase::UnlockRead (MediaState_T *aState)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    switch (m.state)
    {
        case MediaState_LockedRead:
        {
            if (m.queryInfoSem == NIL_RTSEMEVENTMULTI)
            {
                Assert (m.readers != 0);
                -- m.readers;

                /* Reset the state after the last reader */
                if (m.readers == 0)
                {
                    if (m.accessibleInLock)
                        m.state = MediaState_Created;
                    else
                        m.state = MediaState_Inaccessible;
                }

                break;
            }

            /* otherwise, queryInfo() is in progress; fall through */
        }
        default:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Medium '%ls' is not locked for reading"),
                m.locationFull.raw());
            break;
        }
    }

    /* return the current state after */
    if (aState)
        *aState = m.state;

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP MediumBase::LockWrite (MediaState_T *aState)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* return the current state before */
    if (aState)
        *aState = m.state;

    HRESULT rc = S_OK;

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
        {
            if (m.state == MediaState_Created)
                m.accessibleInLock = true;
            else if (m.state == MediaState_Inaccessible)
                m.accessibleInLock = false;

            m.state = MediaState_LockedWrite;
            break;
        }
        default:
        {
            rc = setStateError();
            break;
        }
    }

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP MediumBase::UnlockWrite (MediaState_T *aState)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    switch (m.state)
    {
        case MediaState_LockedWrite:
        {
            if (m.accessibleInLock)
                m.state = MediaState_Created;
            else
                m.state = MediaState_Inaccessible;
            break;
        }
        default:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Medium '%ls' is not locked for writing"),
                m.locationFull.raw());
            break;
        }
    }

    /* return the current state after */
    if (aState)
        *aState = m.state;

    return rc;
}

STDMETHODIMP MediumBase::Close()
{
    AutoMayUninitSpan mayUninitSpan (this);
    CheckComRCReturnRC (mayUninitSpan.rc());

    if (mayUninitSpan.alreadyInProgress())
        return S_OK;

    /* unregisterWithVirtualBox() is assumed to always need a write mVirtualBox
     * lock as it is intenede to modify its internal structires. Also, we want
     * to unregister ourselves atomically after detecting that closure is
     * possible to make sure that we don't do that after another thread has done
     * VirtualBox::find*() but before it starts using us (provided that it holds
     * a mVirtualBox lock of course). */

    AutoWriteLock vboxLock (mVirtualBox);

    bool wasCreated = true;

    switch (m.state)
    {
        case MediaState_NotCreated:
            wasCreated = false;
            break;
        case MediaState_Created:
        case MediaState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    if (m.backRefs.size() != 0)
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Medium '%ls' is attached to %d virtual machines"),
                m.locationFull.raw(), m.backRefs.size());

    /* perform extra media-dependent close checks */
    HRESULT rc = canClose();
    CheckComRCReturnRC (rc);

    if (wasCreated)
    {
        /* remove from the list of known media before performing actual
         * uninitialization (to keep the media registry consistent on
         * failure to do so) */
        rc = unregisterWithVirtualBox();
        CheckComRCReturnRC (rc);
    }

    /* cause uninit() to happen on success */
    mayUninitSpan.acceptUninit();

    return S_OK;
}

// public methods for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Checks if the given change of \a aOldPath to \a aNewPath affects the location
 * of this media and updates it if necessary to reflect the new location.
 *
 * @param aOldPath  Old path (full).
 * @param aNewPath  New path (full).
 *
 * @note Locks this object for writing.
 */
HRESULT MediumBase::updatePath (const char *aOldPath, const char *aNewPath)
{
    AssertReturn (aOldPath, E_FAIL);
    AssertReturn (aNewPath, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    LogFlowThisFunc (("locationFull.before='%s'\n", m.locationFull.raw()));

    Utf8Str path = m.locationFull;

    if (RTPathStartsWith (path, aOldPath))
    {
        Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                      path.raw() + strlen (aOldPath));
        path = newPath;

        mVirtualBox->calculateRelativePath (path, path);

        unconst (m.locationFull) = newPath;
        unconst (m.location) = path;

        LogFlowThisFunc (("locationFull.after='%s'\n", m.locationFull.raw()));
    }

    return S_OK;
}

/**
 * Adds the given machine and optionally the snapshot to the list of the objects
 * this image is attached to.
 *
 * @param aMachineId    Machine ID.
 * @param aSnapshotId   Snapshot ID; when non-empty, adds a snapshot attachment.
 */
HRESULT MediumBase::attachTo (const Guid &aMachineId,
                              const Guid &aSnapshotId /*= Guid::Empty*/)
{
    AssertReturn (!aMachineId.isEmpty(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
        case MediaState_LockedRead:
        case MediaState_LockedWrite:
            break;

        default:
            return setStateError();
    }

    HRESULT rc = canAttach (aMachineId, aSnapshotId);
    CheckComRCReturnRC (rc);

    BackRefList::iterator it =
        std::find_if (m.backRefs.begin(), m.backRefs.end(),
                      BackRef::EqualsTo (aMachineId));
    if (it == m.backRefs.end())
    {
        BackRef ref (aMachineId, aSnapshotId);
        m.backRefs.push_back (ref);

        return S_OK;
    }

    if (aSnapshotId.isEmpty())
    {
        /* sanity: no duplicate attachments */
        AssertReturn (!it->inCurState, E_FAIL);
        it->inCurState = true;

        return S_OK;
    }

    /* sanity: no duplicate attachments */
    BackRef::GuidList::const_iterator jt =
        std::find (it->snapshotIds.begin(), it->snapshotIds.end(), aSnapshotId);
    AssertReturn (jt == it->snapshotIds.end(), E_FAIL);

    it->snapshotIds.push_back (aSnapshotId);

    return S_OK;
}

/**
 * Removes the given machine and optionally the snapshot from the list of the
 * objects this image is attached to.
 *
 * @param aMachineId    Machine ID.
 * @param aSnapshotId   Snapshot ID; when non-empty, removes the snapshot
 *                      attachment.
 */
HRESULT MediumBase::detachFrom (const Guid &aMachineId,
                                const Guid &aSnapshotId /*= Guid::Empty*/)
{
    AssertReturn (!aMachineId.isEmpty(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    BackRefList::iterator it =
        std::find_if (m.backRefs.begin(), m.backRefs.end(),
                      BackRef::EqualsTo (aMachineId));
    AssertReturn (it != m.backRefs.end(), E_FAIL);

    if (aSnapshotId.isEmpty())
    {
        /* remove the current state attachment */
        it->inCurState = false;
    }
    else
    {
        /* remove the snapshot attachment */
        BackRef::GuidList::iterator jt =
            std::find (it->snapshotIds.begin(), it->snapshotIds.end(), aSnapshotId);

        AssertReturn (jt != it->snapshotIds.end(), E_FAIL);
        it->snapshotIds.erase (jt);
    }

    /* if the backref becomes empty, remove it */
    if (it->inCurState == false && it->snapshotIds.size() == 0)
        m.backRefs.erase (it);

    return S_OK;
}

// protected methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns a short version of the location attribute.
 *
 * @note Must be called from under this object's read or write lock.
 */
Utf8Str MediumBase::name()
{
    Utf8Str location (m.locationFull);

    Utf8Str name = RTPathFilename (location);
    return name;
}

/**
 * Sets the value of m.location and calculates the value of m.locationFull.
 *
 * @param aLocation Path to the image file (can be relative to the
 *                  VirtualBox home directory).
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT MediumBase::setLocation (CBSTR aLocation)
{
    /* get the full file name */
    Utf8Str locationFull;
    int vrc = mVirtualBox->calculateFullPath (Utf8Str (aLocation), locationFull);
    if (RT_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid image file location '%ls' (%Rrc)"),
            aLocation, vrc);

    m.location = aLocation;
    m.locationFull = locationFull;

    return S_OK;
}

/**
 * Queries information from the image file.
 *
 * As a result of this call, the accessibility state and data members such as
 * size and description will be updated with the current information.
 *
 * The fWrite parameter is ignored in this variant, since this always opens
 * the medium read-only; it is however acknowledged by HardDisk::queryInfo(),
 * which overrides this implementation for hard disk media.
 *
 * @note This method may block during a system I/O call that checks image file
 *       accessibility.
 *
 * @note Locks this object for writing.
 */
HRESULT MediumBase::queryInfo(bool fWrite)
{
    AutoWriteLock alock (this);

    AssertReturn (m.state == MediaState_Created ||
                  m.state == MediaState_Inaccessible ||
                  m.state == MediaState_LockedRead ||
                  m.state == MediaState_LockedWrite,
                  E_FAIL);

    int vrc = VINF_SUCCESS;

    /* check if a blocking queryInfo() call is in progress on some other thread,
     * and wait for it to finish if so instead of querying data ourselves */
    if (m.queryInfoSem != NIL_RTSEMEVENTMULTI)
    {
        Assert (m.state == MediaState_LockedRead);

        ++ m.queryInfoCallers;
        alock.leave();

        vrc = RTSemEventMultiWait (m.queryInfoSem, RT_INDEFINITE_WAIT);

        alock.enter();
        -- m.queryInfoCallers;

        if (m.queryInfoCallers == 0)
        {
            /* last waiting caller deletes the semaphore */
            RTSemEventMultiDestroy (m.queryInfoSem);
            m.queryInfoSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRC (vrc);

        return S_OK;
    }

    /* lazily create a semaphore for possible callers */
    vrc = RTSemEventMultiCreate (&m.queryInfoSem);
    ComAssertRCRet (vrc, E_FAIL);

    bool tempStateSet = false;
    if (m.state != MediaState_LockedRead &&
        m.state != MediaState_LockedWrite)
    {
        /* Cause other methods to prevent any modifications before leaving the
         * lock. Note that clients will never see this temporary state change
         * directly since any COMGETTER(State) is (or will be) blocked until we
         * finish and restore the actual state. This may be seen only through
         * error messages reported by other methods. */
        m.state = MediaState_LockedRead;
        tempStateSet = true;
    }

    /* leave the lock before a blocking operation */
    alock.leave();

    bool success = false;

    /* get image file info */
    {
        RTFILE file;
        vrc = RTFileOpen (&file, Utf8Str (m.locationFull), RTFILE_O_READ);
        if (RT_SUCCESS (vrc))
        {
            vrc = RTFileGetSize (file, &m.size);

            RTFileClose (file);
        }

        if (RT_FAILURE (vrc))
        {
            m.lastAccessError = Utf8StrFmt (
                tr ("Could not access the image file '%ls' (%Rrc)"),
                m.locationFull.raw(), vrc);
        }

        success = (RT_SUCCESS (vrc));
    }

    alock.enter();

    if (success)
        m.lastAccessError.setNull();
    else
        LogWarningFunc (("'%ls' is not accessible (error='%ls', vrc=%Rrc)\n",
                         m.locationFull.raw(), m.lastAccessError.raw(), vrc));

    /* inform other callers if there are any */
    if (m.queryInfoCallers > 0)
    {
        RTSemEventMultiSignal (m.queryInfoSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (m.queryInfoSem);
        m.queryInfoSem = NIL_RTSEMEVENTMULTI;
    }

    if (tempStateSet)
    {
        /* Set the proper state according to the result of the check */
        if (success)
            m.state = MediaState_Created;
        else
            m.state = MediaState_Inaccessible;
    }
    else
    {
        /* we're locked, use a special field to store the result */
        m.accessibleInLock = success;
    }

    return S_OK;
}

/**
 * Sets the extended error info according to the current media state.
 *
 * @note Must be called from under this object's write or read lock.
 */
HRESULT MediumBase::setStateError()
{
    HRESULT rc = E_FAIL;

    switch (m.state)
    {
        case MediaState_NotCreated:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Storage for the medium '%ls' is not created"),
                m.locationFull.raw());
            break;
        }
        case MediaState_Created:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Storage for the medium '%ls' is already created"),
                m.locationFull.raw());
            break;
        }
        case MediaState_LockedRead:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Medium '%ls' is locked for reading by another task"),
                m.locationFull.raw());
            break;
        }
        case MediaState_LockedWrite:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Medium '%ls' is locked for writing by another task"),
                m.locationFull.raw());
            break;
        }
        case MediaState_Inaccessible:
        {
            AssertMsg (!m.lastAccessError.isEmpty(),
                       ("There must always be a reason for Inaccessible"));

            /* be in sync with Console::powerUpThread() */
            if (!m.lastAccessError.isEmpty())
                rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                    tr ("Medium '%ls' is not accessible. %ls"),
                    m.locationFull.raw(), m.lastAccessError.raw());
            else
                rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                    tr ("Medium '%ls' is not accessible"),
                    m.locationFull.raw());
            break;
        }
        case MediaState_Creating:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Storage for the medium '%ls' is being created"),
                m.locationFull.raw(), m.lastAccessError.raw());
            break;
        }
        case MediaState_Deleting:
        {
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Storage for the medium '%ls' is being deleted"),
                m.locationFull.raw(), m.lastAccessError.raw());
            break;
        }
        default:
        {
            AssertFailed();
            break;
        }
    }

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// ImageMediumBase class
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the image medium object by opening an image file at the specified
 * location.
 *
 * @param aVirtualBox   Parent VirtualBox object.
 * @param aLocation     Path to the image file (can be relative to the
 *                      VirtualBox home directory).
 * @param aId           UUID of the image.
 */
HRESULT ImageMediumBase::protectedInit (VirtualBox *aVirtualBox, CBSTR aLocation,
                                        const Guid &aId)
{
    LogFlowThisFunc (("aLocation='%ls', aId={%RTuuid}\n", aLocation, aId.raw()));

    AssertReturn (aVirtualBox, E_INVALIDARG);
    AssertReturn (aLocation, E_INVALIDARG);
    AssertReturn (!aId.isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share parent weakly */
    unconst (mVirtualBox) = aVirtualBox;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mVirtualBox->addDependentChild (this);

    /* there must be a storage unit */
    m.state = MediaState_Created;

    unconst (m.id) = aId;
    rc = setLocation (aLocation);
    CheckComRCReturnRC (rc);

    LogFlowThisFunc (("m.locationFull='%ls'\n", m.locationFull.raw()));

    /* get all the information about the medium from the file */
    rc = queryInfo(true);
    if (SUCCEEDED (rc))
    {
        /* if the image file is not accessible, it's not acceptable for the
         * newly opened media so convert this into an error */
        if (!m.lastAccessError.isNull())
            rc = setError (E_FAIL, Utf8Str (m.lastAccessError));
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the image medium object by loading its data from the given
 * settings node.
 *
 * Note that it is assumed that this method is called only for registered media.
 *
 * @param aVirtualBox   Parent VirtualBox object.
 * @param aImageNode    Either <DVDImage> or <FloppyImage> settings node.
 */
HRESULT ImageMediumBase::protectedInit (VirtualBox *aVirtualBox,
                                        const settings::Key &aImageNode)
{
    AssertReturn (aVirtualBox, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share parent weakly */
    unconst (mVirtualBox) = aVirtualBox;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mVirtualBox->addDependentChild (this);

    /* see below why we don't call queryInfo() (and therefore treat the medium
     * as inaccessible for now */
    m.state = MediaState_Inaccessible;

    /* required */
    unconst (m.id) = aImageNode.value <Guid> ("uuid");
    /* required */
    Bstr location = aImageNode.stringValue ("location");
    rc = setLocation (location);
    CheckComRCReturnRC (rc);
    /* optional */
    {
        settings::Key descNode = aImageNode.findKey ("Description");
        if (!descNode.isNull())
            m.description = descNode.keyStringValue();
    }

    LogFlowThisFunc (("m.locationFull='%ls', m.id={%RTuuid}\n",
                      m.locationFull.raw(), m.id.raw()));

    /* Don't call queryInfo() for registered media to prevent the calling
     * thread (i.e. the VirtualBox server startup thread) from an unexpected
     * freeze but mark it as initially inaccessible instead. The vital UUID and
     * location properties are read from the registry file above; to get the
     * actual state and the rest of the data, the user will have to call
     * COMGETTER(State).*/

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Uninitializes the instance.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void ImageMediumBase::protectedUninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mVirtualBox->removeDependentChild (this);

    unconst (mVirtualBox).setNull();
}

// public methods for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Saves image data by appending a new <Image> child node to the
 * given <Images> parent node.
 *
 * @param aImagesNode    <Images> node.
 *
 * @note Locks this object for reading.
 */
HRESULT ImageMediumBase::saveSettings (settings::Key &aImagesNode)
{
    using namespace settings;

    AssertReturn (!aImagesNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Key imageNode = aImagesNode.appendKey ("Image");
    /* required */
    imageNode.setValue <Guid> ("uuid", m.id);
    /* required */
    imageNode.setValue <Bstr> ("location", m.locationFull);
    /* optional */
    if (!m.description.isNull())
    {
        Key descNode = aImagesNode.createKey ("Description");
        descNode.setKeyValue <Bstr> (m.description);
    }

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// DVDImage class
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (DVDImage)

/**
 * @note Called from within this object's AutoMayUninitSpan and from under
 *       mVirtualBox write lock.
 */
HRESULT DVDImage::unregisterWithVirtualBox()
{
    return mVirtualBox->unregisterDVDImage (this);
}

////////////////////////////////////////////////////////////////////////////////
// FloppyImage class
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (FloppyImage)

/**
 * @note Called from within this object's AutoMayUninitSpan and from under
 *       mVirtualBox write lock.
 */
HRESULT FloppyImage::unregisterWithVirtualBox()
{
    return mVirtualBox->unregisterFloppyImage (this);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
