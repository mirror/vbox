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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "DVDDriveImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

#include <iprt/string.h>

// defines
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT DVDDrive::FinalConstruct()
{
    return S_OK;
}

void DVDDrive::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the DVD drive object.
 *
 * @param   parent  handle of our parent object
 * @return  COM result indicator
 */
HRESULT DVDDrive::init (Machine *parent)
{
    LogFlowMember (("DVDDrive::init (%p)\n", parent));

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
 *  Initializes the DVD drive object given another DVD drive object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT DVDDrive::init (Machine *parent, DVDDrive *that)
{
    LogFlowMember (("DVDDrive::init (%p, %p)\n", parent, that));

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
HRESULT DVDDrive::initCopy (Machine *parent, DVDDrive *that)
{
    LogFlowMember (("DVDDrive::initCopy (%p, %p)\n", parent, that));

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
void DVDDrive::uninit()
{
    LogFlowMember (("DVDDrive::uninit()\n"));

    AutoLock alock (this);

    AssertReturn (isReady(), (void) 0);

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    setReady (false);
}

// IDVDDrive properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DVDDrive::COMGETTER(State) (DriveState_T *driveState)
{
    if (!driveState)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *driveState = mData->mDriveState;
    return S_OK;
}

STDMETHODIMP DVDDrive::COMGETTER(Passthrough) (BOOL *passthrough)
{
    if (!passthrough)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *passthrough = mData->mPassthrough;
    return S_OK;
}

STDMETHODIMP DVDDrive::COMSETTER(Passthrough) (BOOL passthrough)
{
    AutoLock alock(this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mPassthrough != passthrough)
    {
        mData.backup();
        mData->mPassthrough = passthrough;
    }
    return S_OK;
}

// IDVDDrive methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DVDDrive::MountImage(INPTR GUIDPARAM imageId)
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

    ComPtr <IDVDImage> image;
    rc = vbox->GetDVDImage (imageId, image.asOutParam());
    if (SUCCEEDED (rc))
    {
        if (mData->mDriveState != DriveState_ImageMounted ||
            !mData->mDVDImage.equalsTo (image))
        {
            mData.backup();
            unmount();
            mData->mDVDImage = image;
            mData->mDriveState = DriveState_ImageMounted;

            alock.unlock();
            mParent->onDVDDriveChange();
        }
    }

    return rc;
}

STDMETHODIMP DVDDrive::CaptureHostDrive(IHostDVDDrive *hostDVDDrive)
{
    if (!hostDVDDrive)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mDriveState != DriveState_HostDriveCaptured ||
        !mData->mHostDrive.equalsTo (hostDVDDrive))
    {
        mData.backup();
        unmount();
        mData->mHostDrive = hostDVDDrive;
        mData->mDriveState = DriveState_HostDriveCaptured;

        alock.unlock();
        mParent->onDVDDriveChange();
    }

    return S_OK;
}

STDMETHODIMP DVDDrive::Unmount()
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
        mParent->onDVDDriveChange();
    }

    return S_OK;
}

STDMETHODIMP DVDDrive::GetImage(IDVDImage **dvdImage)
{
    if (!dvdImage)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mDVDImage.queryInterfaceTo (dvdImage);
    return S_OK;
}

STDMETHODIMP DVDDrive::GetHostDrive(IHostDVDDrive **hostDrive)
{
    if (!hostDrive)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mHostDrive.queryInterfaceTo (hostDrive);
    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

bool DVDDrive::rollback()
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

void DVDDrive::commit()
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

void DVDDrive::copyFrom (DVDDrive *aThat)
{
    AutoLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Helper to unmount a drive
 *
 * @returns COM status code
 *
 */
HRESULT DVDDrive::unmount()
{
    if (mData->mDVDImage)
        mData->mDVDImage.setNull();
    if (mData->mHostDrive)
        mData->mHostDrive.setNull();
    return S_OK;
}
