/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "FloppyDriveImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

#include <iprt/string.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT FloppyDrive::FinalConstruct()
{
    return S_OK;
}

void FloppyDrive::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the Floppy drive object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT FloppyDrive::init (Machine *parent)
{
    LogFlowMember (("FloppyDrive::init (%p)\n", parent));

    ComAssertRet (parent, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    mData.allocate();

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the Floppy drive object given another Floppy drive object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT FloppyDrive::init (Machine *parent, FloppyDrive *that)
{
    LogFlowMember (("FloppyDrive::init (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    mPeer = that;

    AutoLock thatlock (that);
    mData.share (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT FloppyDrive::initCopy (Machine *parent, FloppyDrive *that)
{
    LogFlowMember (("FloppyDrive::initCopy (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    AutoLock thatlock (that);
    mData.attachCopy (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void FloppyDrive::uninit()
{
    LogFlowMember (("FloppyDrive::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    setReady (false);
}

// IFloppyDrive properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::COMGETTER(Enabled) (BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *enabled = mData->mEnabled;
    return S_OK;
}

STDMETHODIMP FloppyDrive::COMSETTER(Enabled) (BOOL enabled)
{
    LogFlowMember(("FloppyDrive::SetEnabled: %s\n", enabled ? "enable" : "disable"));

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mEnabled != enabled)
    {
        mData.backup();
        mData->mEnabled = enabled;

        alock.unlock();
        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::COMGETTER(State) (DriveState_T *driveState)
{
    if (!driveState)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *driveState = mData->mDriveState;
    return S_OK;
}

// IFloppyDrive methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::MountImage(INPTR GUIDPARAM imageId)
{
    if (Guid::isEmpty(imageId))
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    HRESULT rc = E_FAIL;

    // find the image in the collection
    ComPtr <IVirtualBox> vbox;
    rc = mParent->COMGETTER(Parent) (vbox.asOutParam());
    AssertComRCReturn (rc, rc);

    ComPtr <IFloppyImage> image;
    rc = vbox->GetFloppyImage (imageId, image.asOutParam());
    if (SUCCEEDED (rc))
    {
        if (mData->mDriveState != DriveState_ImageMounted ||
            !mData->mFloppyImage.equalsTo (image))
        {
            mData.backup();
            unmount();
            mData->mFloppyImage = image;
            mData->mDriveState = DriveState_ImageMounted;

            alock.unlock();
            mParent->onFloppyDriveChange();
        }
    }

    return rc;
}

STDMETHODIMP FloppyDrive::CaptureHostDrive(IHostFloppyDrive *hostFloppyDrive)
{
    if (!hostFloppyDrive)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mDriveState != DriveState_HostDriveCaptured ||
        !mData->mHostDrive.equalsTo (hostFloppyDrive))
    {
        mData.backup();
        unmount();
        mData->mHostDrive = hostFloppyDrive;
        mData->mDriveState = DriveState_HostDriveCaptured;

        alock.unlock();
        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::Unmount()
{
    AutoLock alock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mDriveState != DriveState_NotMounted)
    {
        mData.backup();
        unmount();
        mData->mDriveState = DriveState_NotMounted;

        alock.unlock();
        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::GetImage(IFloppyImage **floppyImage)
{
    if (!floppyImage)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mFloppyImage.queryInterfaceTo (floppyImage);
    return S_OK;
}

STDMETHODIMP FloppyDrive::GetHostDrive(IHostFloppyDrive **hostDrive)
{
    if (!hostDrive)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mHostDrive.queryInterfaceTo (hostDrive);
    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

bool FloppyDrive::rollback()
{
    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        // we need to check all data to see whether anything will be changed
        // after rollback
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

void FloppyDrive::commit()
{
    AutoLock alock (this);
    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

void FloppyDrive::copyFrom (FloppyDrive *aThat)
{
    AutoLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Helper to unmount a drive
 *
 * @returns COM status code
 *
 */
HRESULT FloppyDrive::unmount()
{
    if (mData->mFloppyImage)
        mData->mFloppyImage.setNull();
    if (mData->mHostDrive)
        mData->mHostDrive.setNull();
    return S_OK;
}
