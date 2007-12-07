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

#include "FloppyDriveImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (FloppyDrive)

HRESULT FloppyDrive::FinalConstruct()
{
    return S_OK;
}

void FloppyDrive::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Floppy drive object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT FloppyDrive::init (Machine *aParent)
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
 *  Initializes the Floppy drive object given another Floppy drive object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT FloppyDrive::init (Machine *aParent, FloppyDrive *aThat)
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
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT FloppyDrive::initCopy (Machine *aParent, FloppyDrive *aThat)
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
void FloppyDrive::uninit()
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

// IFloppyDrive properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::COMGETTER(Enabled) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP FloppyDrive::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::COMGETTER(State) (DriveState_T *aDriveState)
{
    if (!aDriveState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aDriveState = mData->mDriveState;

    return S_OK;
}

// IFloppyDrive methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::MountImage (INPTR GUIDPARAM aImageId)
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

    ComPtr <IFloppyImage> image;
    rc = mParent->virtualBox()->GetFloppyImage (aImageId, image.asOutParam());

    if (SUCCEEDED (rc))
    {
        if (mData->mDriveState != DriveState_ImageMounted ||
            !mData->mFloppyImage.equalsTo (image))
        {
            mData.backup();

            unmount();

            mData->mFloppyImage = image;
            mData->mDriveState = DriveState_ImageMounted;

            /* leave the lock before informing callbacks */
            alock.unlock();

            mParent->onFloppyDriveChange();
        }
    }

    return rc;
}

STDMETHODIMP FloppyDrive::CaptureHostDrive (IHostFloppyDrive *aHostFloppyDrive)
{
    if (!aHostFloppyDrive)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mDriveState != DriveState_HostDriveCaptured ||
        !mData->mHostDrive.equalsTo (aHostFloppyDrive))
    {
        mData.backup();

        unmount();

        mData->mHostDrive = aHostFloppyDrive;
        mData->mDriveState = DriveState_HostDriveCaptured;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::Unmount()
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

        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::GetImage (IFloppyImage **aFloppyImage)
{
    if (!aFloppyImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mFloppyImage.queryInterfaceTo (aFloppyImage);

    return S_OK;
}

STDMETHODIMP FloppyDrive::GetHostDrive (IHostFloppyDrive **aHostDrive)
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
/////////////////////////////////////////////////////////////////////////////

/** 
 *  @note Locks this object for writing.
 */
bool FloppyDrive::rollback()
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
void FloppyDrive::commit()
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
 *  @note Locks this object for writing, together with the peer object (locked
 *  for reading) if there is one.
 */
void FloppyDrive::copyFrom (FloppyDrive *aThat)
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeRlock (mPeer));

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to unmount a drive.
 *
 *  @return COM status code
 */
HRESULT FloppyDrive::unmount()
{
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    if (mData->mFloppyImage)
        mData->mFloppyImage.setNull();
    if (mData->mHostDrive)
        mData->mHostDrive.setNull();

    return S_OK;
}
