/* $Id$ */

/** @file
 *
 * Implementation of IStorageController.
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

#include "StorageControllerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <algorithm>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (StorageController)

HRESULT StorageController::FinalConstruct()
{
    return S_OK;
}

void StorageController::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the storage controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aName         Name of the storage controller.
 */
HRESULT StorageController::init (Machine *aParent, IN_BSTR aName, 
                                 StorageBus_T aStorageBus)
{
    LogFlowThisFunc (("aParent=%p aName=\"%ls\"\n", aParent, aName));

    ComAssertRet (aParent && aName, E_INVALIDARG);
    if (   (aStorageBus <= StorageBus_Null)
        || (aStorageBus >  StorageBus_SCSI))
        return setError (E_INVALIDARG,
            tr ("Invalid storage connection type"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    mData.allocate();

    mData->mName = aName;
    mData->mStorageBus = aStorageBus;

    switch (aStorageBus)
    {
        case StorageBus_IDE:
            mData->mPortCount = 2;
            mData->mStorageControllerType = StorageControllerType_PIIX4;
            break;
        case StorageBus_SATA:
            mData->mPortCount = 30;
            mData->mStorageControllerType = StorageControllerType_IntelAhci;
            break;
        case StorageBus_SCSI:
            mData->mPortCount = 16;
            mData->mStorageControllerType = StorageControllerType_LsiLogic;
            break;
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @param  aReshare
 *      When false, the original object will remain a data owner.
 *      Otherwise, data ownership will be transferred from the original
 *      object to this one.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for writing if @a aReshare is @c true, or for
 *  reading if @a aReshare is false.
 */
HRESULT StorageController::init (Machine *aParent,
                                 StorageController *aThat,
                                 bool aReshare /* = false */)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p, aReshare=%RTbool\n",
                      aParent, aThat, aReshare));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    if (aReshare)
    {
        AutoWriteLock thatLock (aThat);

        unconst (aThat->mPeer) = this;
        mData.attach (aThat->mData);
    }
    else
    {
        unconst (mPeer) = aThat;

        AutoReadLock thatLock (aThat);
        mData.share (aThat->mData);
    }

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the storage controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT StorageController::initCopy (Machine *aParent, StorageController *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mParent->addDependentChild (this);

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReadLock thatlock (aThat);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void StorageController::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    mParent->removeDependentChild (this);

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}


// IStorageController properties
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP StorageController::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mName is constant during life time, no need to lock */
    mData.data()->mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(Bus) (StorageBus_T *aBus)
{
    CheckComArgOutPointerValid(aBus);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aBus = mData->mStorageBus;

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(ControllerType) (StorageControllerType_T *aControllerType)
{
    CheckComArgOutPointerValid(aControllerType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aControllerType = mData->mStorageControllerType;

    return S_OK;
}

STDMETHODIMP StorageController::COMSETTER(ControllerType) (StorageControllerType_T aControllerType)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    switch (mData->mStorageBus)
    {
        case StorageBus_IDE:
        {
            if (   (aControllerType != StorageControllerType_PIIX3)
                && (aControllerType != StorageControllerType_PIIX4)
                && (aControllerType != StorageControllerType_ICH6))
                rc = E_INVALIDARG;
            break;
        }
        case StorageBus_SATA:
        {
            if (aControllerType != StorageControllerType_IntelAhci)
                rc = E_INVALIDARG;
            break;
        }
        case StorageBus_SCSI:
        {
            if (   (aControllerType != StorageControllerType_LsiLogic)
                && (aControllerType != StorageControllerType_BusLogic))
                rc = E_INVALIDARG;
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", mData->mStorageBus));
    }

    if (!SUCCEEDED (rc))
        return setError(rc,
                        tr ("Invalid controller type %d"),
                        aControllerType);

    mData->mStorageControllerType = aControllerType;

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(MaxDevicesPerPortCount) (ULONG *aMaxDevices)
{
    CheckComArgOutPointerValid(aMaxDevices);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    switch (mData->mStorageBus)
    {
        case StorageBus_SATA:
        case StorageBus_SCSI:
        {
            /* SATA and both SCSI controllers only support one device per port. */
            *aMaxDevices = 1;
            break;
        }
        case StorageBus_IDE:
        {
            /* The IDE controllers support 2 devices. One as master and one as slave. */
            *aMaxDevices = 2;
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", mData->mStorageBus));
    }

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(MinPortCount) (ULONG *aMinPortCount)
{
    CheckComArgOutPointerValid(aMinPortCount);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    switch (mData->mStorageBus)
    {
        case StorageBus_SATA:
        {
            *aMinPortCount = 1;
            break;
        }
        case StorageBus_SCSI:
        {
            *aMinPortCount = 16;
            break;
        }
        case StorageBus_IDE:
        {
            *aMinPortCount = 2;
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", mData->mStorageBus));
    }

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(MaxPortCount) (ULONG *aMaxPortCount)
{
    CheckComArgOutPointerValid(aMaxPortCount);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    switch (mData->mStorageBus)
    {
        case StorageBus_SATA:
        {
            *aMaxPortCount = 30;
            break;
        }
        case StorageBus_SCSI:
        {
            *aMaxPortCount = 16;
            break;
        }
        case StorageBus_IDE:
        {
            *aMaxPortCount = 2;
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", mData->mStorageBus));
    }

    return S_OK;
}


STDMETHODIMP StorageController::COMGETTER(PortCount) (ULONG *aPortCount)
{
    CheckComArgOutPointerValid(aPortCount);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aPortCount = mData->mPortCount;

    return S_OK;
}


STDMETHODIMP StorageController::COMSETTER(PortCount) (ULONG aPortCount)
{
    LogFlowThisFunc (("aPortCount=%u\n", aPortCount));

    switch (mData->mStorageBus)
    {
        case StorageBus_SATA:
        {
            /* AHCI SATA supports a maximum of 30 ports. */
            if ((aPortCount < 1) || (aPortCount > 30))
                return setError (E_INVALIDARG,
                    tr ("Invalid port count: %lu (must be in range [%lu, %lu])"),
                        aPortCount, 1, 30);
            break;
        }
        case StorageBus_SCSI:
        {
            /*
             * SCSI does not support setting different ports.
             * (doesn't make sense here either).
             * The maximum and minimum is 16 and unless the callee
             * tries to set a different value we return an error.
             */
            if (aPortCount != 16)
                return setError (E_INVALIDARG,
                    tr ("Invalid port count: %lu (must be in range [%lu, %lu])"),
                        aPortCount, 16, 16);
            break;
        }
        case StorageBus_IDE:
        {
            /*
             * The port count is fixed to 2. 
             */
            if (aPortCount != 2)
                return setError (E_INVALIDARG,
                    tr ("Invalid port count: %lu (must be in range [%lu, %lu])"),
                        aPortCount, 2, 2);
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", mData->mStorageBus));
    }

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mPortCount != aPortCount)
    {
        mData.backup();
        mData->mPortCount = aPortCount;

        /* leave the lock for safety */
        alock.leave();

        mParent->onStorageControllerChange ();
    }

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(Instance) (ULONG *aInstance)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* The machine doesn't need to be mutable. */

    AutoReadLock alock (this);

    *aInstance = mInstance;

    return S_OK;
}

STDMETHODIMP StorageController::COMSETTER(Instance) (ULONG aInstance)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* The machine doesn't need to be mutable. */

    AutoWriteLock alock (this);

    mInstance = aInstance;

    return S_OK;
}

// IStorageController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP StorageController::GetIDEEmulationPort(LONG DevicePosition, LONG *aPortNumber)
{
    CheckComArgOutPointerValid(aPortNumber);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    if (mData->mStorageControllerType != StorageControllerType_IntelAhci)
        return setError (E_NOTIMPL,
            tr ("Invalid controller type"));

    switch (DevicePosition)
    {
        case 0:
            *aPortNumber = mData->mPortIde0Master;
            break;
        case 1:
            *aPortNumber = mData->mPortIde0Slave;
            break;
        case 2:
            *aPortNumber = mData->mPortIde1Master;
            break;
        case 3:
            *aPortNumber = mData->mPortIde1Slave;
            break;
        default:
            return E_INVALIDARG;
    }

    return S_OK;
}

STDMETHODIMP StorageController::SetIDEEmulationPort(LONG DevicePosition, LONG aPortNumber)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());
    AutoWriteLock alock (this);

    if (mData->mStorageControllerType != StorageControllerType_IntelAhci)
        return setError (E_NOTIMPL,
            tr ("Invalid controller type"));

    if ((aPortNumber < 0) || (aPortNumber >= 30))
        return setError (E_INVALIDARG,
            tr ("Invalid port number: %l (must be in range [%lu, %lu])"),
                aPortNumber, 0, 29);

    switch (DevicePosition)
    {
        case 0:
            mData->mPortIde0Master = aPortNumber;
            break;
        case 1:
            mData->mPortIde0Slave = aPortNumber;
            break;
        case 2:
            mData->mPortIde1Master = aPortNumber;
            break;
        case 3:
            mData->mPortIde1Slave = aPortNumber;
            break;
        default:
            return E_INVALIDARG;
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/** @note Locks objects for writing! */
bool StorageController::rollback()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoWriteLock alock (this);

    bool dataChanged = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        dataChanged = mData.hasActualChanges();
        mData.rollback();
    }

    return dataChanged;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void StorageController::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock (mPeer, this);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  Cancels sharing (if any) by making an independent copy of data.
 *  This operation also resets this object's peer to NULL.
 *
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void StorageController::unshare()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* peer is not modified, lock it for reading (mPeer is "master" so locked
     * first) */
    AutoMultiLock2 alock (mPeer->rlock(), this->wlock());

    if (mData.isShared())
    {
        if (!mData.isBackedUp())
            mData.backup();

        mData.commit();
    }

    unconst (mPeer).setNull();
}

// private methods
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
