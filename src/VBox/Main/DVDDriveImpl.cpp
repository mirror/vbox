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

#include "DVDDriveImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (DVDDrive)

HRESULT DVDDrive::FinalConstruct()
{
    return S_OK;
}

void DVDDrive::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the DVD drive object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT DVDDrive::init (Machine *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the DVD drive object given another DVD drive object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT DVDDrive::init (Machine *aParent, DVDDrive *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the DVD drive object given another DVD drive object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT DVDDrive::initCopy (Machine *aParent, DVDDrive *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void DVDDrive::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// IDVDDrive properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DVDDrive::COMGETTER(State) (DriveState_T *aDriveState)
{
    if (!aDriveState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aDriveState = mData->mDriveState;

    return S_OK;
}

STDMETHODIMP DVDDrive::COMGETTER(Passthrough) (BOOL *aPassthrough)
{
    if (!aPassthrough)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aPassthrough = mData->mPassthrough;

    return S_OK;
}

STDMETHODIMP DVDDrive::COMSETTER(Passthrough) (BOOL aPassthrough)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mPassthrough != aPassthrough)
    {
        mData.backup();
        mData->mPassthrough = aPassthrough;
    }

    return S_OK;
}

// IDVDDrive methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DVDDrive::MountImage (INPTR GUIDPARAM aImageId)
{
    if (Guid::isEmpty (aImageId))
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = E_FAIL;

    /* Our lifetime is bound to mParent's lifetime, so we don't add caller.
     * We also don't lock mParent since its mParent field is const. */

    ComPtr <IDVDImage> image;
    rc = mParent->virtualBox()->GetDVDImage (aImageId, image.asOutParam());

    if (SUCCEEDED (rc))
    {
        if (mData->mDriveState != DriveState_ImageMounted ||
            !mData->mDVDImage.equalsTo (image))
        {
            mData.backup();

            unmount();

            mData->mDVDImage = image;
            mData->mDriveState = DriveState_ImageMounted;

            /* leave the lock before informing callbacks */
            alock.unlock();

            mParent->onDVDDriveChange();
        }
    }

    return rc;
}

STDMETHODIMP DVDDrive::CaptureHostDrive (IHostDVDDrive *aHostDVDDrive)
{
    if (!aHostDVDDrive)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mDriveState != DriveState_HostDriveCaptured ||
        !mData->mHostDrive.equalsTo (aHostDVDDrive))
    {
        mData.backup();

        unmount();

        mData->mHostDrive = aHostDVDDrive;
        mData->mDriveState = DriveState_HostDriveCaptured;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onDVDDriveChange();
    }

    return S_OK;
}

STDMETHODIMP DVDDrive::Unmount()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mDriveState != DriveState_NotMounted)
    {
        mData.backup();

        unmount();

        mData->mDriveState = DriveState_NotMounted;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onDVDDriveChange();
    }

    return S_OK;
}

STDMETHODIMP DVDDrive::GetImage (IDVDImage **aDVDImage)
{
    if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mDVDImage.queryInterfaceTo (aDVDImage);

    return S_OK;
}

STDMETHODIMP DVDDrive::GetHostDrive(IHostDVDDrive **aHostDrive)
{
    if (!aHostDrive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mHostDrive.queryInterfaceTo (aHostDrive);

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/** 
 *  @note Locks this object for writing.
 */
bool DVDDrive::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/** 
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void DVDDrive::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* lock both for writing since we modify both */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeWlock (mPeer));

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/** 
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void DVDDrive::copyFrom (DVDDrive *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), aThat->rlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to unmount a drive.
 *
 *  @return COM status code
 *
 */
HRESULT DVDDrive::unmount()
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    if (mData->mDVDImage)
        mData->mDVDImage.setNull();
    if (mData->mHostDrive)
        mData->mHostDrive.setNull();

    return S_OK;
}
