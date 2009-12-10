/* $Id$ */

/** @file
 *
 * Implementation of IStorageController.
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

struct BackupableStorageControllerData
{
    /* Constructor. */
    BackupableStorageControllerData()
        : mStorageBus(StorageBus_IDE),
          mStorageControllerType(StorageControllerType_PIIX4),
          mInstance(0),
          mPortCount(2),
          mPortIde0Master(0),
          mPortIde0Slave(1),
          mPortIde1Master(2),
          mPortIde1Slave(3)
    { }

    bool operator==(const BackupableStorageControllerData &that) const
    {
        return    this == &that
                || (    (mStorageControllerType == that.mStorageControllerType)
                    && (strName           == that.strName)
                    && (mPortCount   == that.mPortCount)
                    && (mPortIde0Master == that.mPortIde0Master)
                    && (mPortIde0Slave  == that.mPortIde0Slave)
                    && (mPortIde1Master == that.mPortIde1Master)
                    && (mPortIde1Slave  == that.mPortIde1Slave));
    }

    /** Unique name of the storage controller. */
    Utf8Str strName;
    /** The connection type of thestorage controller. */
    StorageBus_T mStorageBus;
    /** Type of the Storage controller. */
    StorageControllerType_T mStorageControllerType;
    /** Instance number of the storage controller. */
    ULONG mInstance;
    /** Number of usable ports. */
    ULONG mPortCount;

    /** The following is only for the SATA controller atm. */
    /** Port which acts as primary master for ide emulation. */
    ULONG mPortIde0Master;
    /** Port which acts as primary slave for ide emulation. */
    ULONG mPortIde0Slave;
    /** Port which acts as secondary master for ide emulation. */
    ULONG mPortIde1Master;
    /** Port which acts as secondary slave for ide emulation. */
    ULONG mPortIde1Slave;
};

struct StorageController::Data
{
    Data()
    { }

    const ComObjPtr<Machine, ComWeakRef>    pParent;
    const ComObjPtr<StorageController>      pPeer;

    Backupable<BackupableStorageControllerData> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

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
 * @param aInstance     Instance number of the storage controller.
 */
HRESULT StorageController::init(Machine *aParent,
                                const Utf8Str &aName,
                                StorageBus_T aStorageBus,
                                ULONG aInstance)
{
    LogFlowThisFunc(("aParent=%p aName=\"%s\" aInstance=%u\n",
                     aParent, aName.raw(), aInstance));

    ComAssertRet(aParent && !aName.isEmpty(), E_INVALIDARG);
    if (   (aStorageBus <= StorageBus_Null)
        || (aStorageBus >  StorageBus_Floppy))
        return setError (E_INVALIDARG,
            tr ("Invalid storage connection type"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    /* m->pPeer is left null */

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    m->pParent->addDependentChild (this);

    m->bd.allocate();

    m->bd->strName = aName;
    m->bd->mInstance = aInstance;
    m->bd->mStorageBus = aStorageBus;

    switch (aStorageBus)
    {
        case StorageBus_IDE:
            m->bd->mPortCount = 2;
            m->bd->mStorageControllerType = StorageControllerType_PIIX4;
            break;
        case StorageBus_SATA:
            m->bd->mPortCount = 30;
            m->bd->mStorageControllerType = StorageControllerType_IntelAhci;
            break;
        case StorageBus_SCSI:
            m->bd->mPortCount = 16;
            m->bd->mStorageControllerType = StorageControllerType_LsiLogic;
            break;
        case StorageBus_Floppy:
            /** @todo allow 2 floppies later */
            m->bd->mPortCount = 1;
            m->bd->mStorageControllerType = StorageControllerType_I82078;
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
HRESULT StorageController::init(Machine *aParent,
                                StorageController *aThat,
                                bool aReshare /* = false */)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p, aReshare=%RTbool\n",
                      aParent, aThat, aReshare));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    m->pParent->addDependentChild (this);

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    if (aReshare)
    {
        AutoWriteLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

        unconst(aThat->m->pPeer) = this;
        m->bd.attach (aThat->m->bd);
    }
    else
    {
        unconst(m->pPeer) = aThat;

        AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
        m->bd.share (aThat->m->bd);
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
HRESULT StorageController::initCopy(Machine *aParent, StorageController *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    /* m->pPeer is left null */

    m->pParent->addDependentChild (this);

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatlock(aThat COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy (aThat->m->bd);

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
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    m->pParent->removeDependentChild (this);

    unconst(m->pPeer).setNull();
    unconst(m->pParent).setNull();

    delete m;
    m = NULL;
}


// IStorageController properties
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP StorageController::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mName is constant during life time, no need to lock */
    m->bd.data()->strName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(Bus) (StorageBus_T *aBus)
{
    CheckComArgOutPointerValid(aBus);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aBus = m->bd->mStorageBus;

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(ControllerType) (StorageControllerType_T *aControllerType)
{
    CheckComArgOutPointerValid(aControllerType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aControllerType = m->bd->mStorageControllerType;

    return S_OK;
}

STDMETHODIMP StorageController::COMSETTER(ControllerType) (StorageControllerType_T aControllerType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    switch (m->bd->mStorageBus)
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
        case StorageBus_Floppy:
        {
            if (aControllerType != StorageControllerType_I82078)
                rc = E_INVALIDARG;
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", m->bd->mStorageBus));
    }

    if (!SUCCEEDED(rc))
        return setError(rc,
                        tr ("Invalid controller type %d"),
                        aControllerType);

    m->bd->mStorageControllerType = aControllerType;

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(MaxDevicesPerPortCount) (ULONG *aMaxDevices)
{
    CheckComArgOutPointerValid(aMaxDevices);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComPtr<IVirtualBox> VBox;
    HRESULT rc = m->pParent->COMGETTER(Parent)(VBox.asOutParam());
    if (FAILED(rc))
        return rc;

    ComPtr<ISystemProperties> sysProps;
    rc = VBox->COMGETTER(SystemProperties)(sysProps.asOutParam());
    if (FAILED(rc))
        return rc;

    rc = sysProps->GetMaxDevicesPerPortForStorageBus(m->bd->mStorageBus, aMaxDevices);
    return rc;
}

STDMETHODIMP StorageController::COMGETTER(MinPortCount) (ULONG *aMinPortCount)
{
    CheckComArgOutPointerValid(aMinPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComPtr<IVirtualBox> VBox;
    HRESULT rc = m->pParent->COMGETTER(Parent)(VBox.asOutParam());
    if (FAILED(rc))
        return rc;

    ComPtr<ISystemProperties> sysProps;
    rc = VBox->COMGETTER(SystemProperties)(sysProps.asOutParam());
    if (FAILED(rc))
        return rc;

    rc = sysProps->GetMinPortCountForStorageBus(m->bd->mStorageBus, aMinPortCount);
    return rc;
}

STDMETHODIMP StorageController::COMGETTER(MaxPortCount) (ULONG *aMaxPortCount)
{
    CheckComArgOutPointerValid(aMaxPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComPtr<IVirtualBox> VBox;
    HRESULT rc = m->pParent->COMGETTER(Parent)(VBox.asOutParam());
    if (FAILED(rc))
        return rc;

    ComPtr<ISystemProperties> sysProps;
    rc = VBox->COMGETTER(SystemProperties)(sysProps.asOutParam());
    if (FAILED(rc))
        return rc;

    rc = sysProps->GetMaxPortCountForStorageBus(m->bd->mStorageBus, aMaxPortCount);
    return rc;
}


STDMETHODIMP StorageController::COMGETTER(PortCount) (ULONG *aPortCount)
{
    CheckComArgOutPointerValid(aPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPortCount = m->bd->mPortCount;

    return S_OK;
}


STDMETHODIMP StorageController::COMSETTER(PortCount) (ULONG aPortCount)
{
    LogFlowThisFunc(("aPortCount=%u\n", aPortCount));

    switch (m->bd->mStorageBus)
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
        case StorageBus_Floppy:
        {
            /** @todo allow 2 floppies later */
            /*
             * The port count is fixed to 1.
             */
            if (aPortCount != 1)
                return setError (E_INVALIDARG,
                    tr ("Invalid port count: %lu (must be in range [%lu, %lu])"),
                        aPortCount, 1, 1);
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", m->bd->mStorageBus));
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->mPortCount != aPortCount)
    {
        m->bd.backup();
        m->bd->mPortCount = aPortCount;

        /* leave the lock for safety */
        alock.leave();

        m->pParent->onStorageControllerChange ();
    }

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(Instance) (ULONG *aInstance)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* The machine doesn't need to be mutable. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aInstance = m->bd->mInstance;

    return S_OK;
}

STDMETHODIMP StorageController::COMSETTER(Instance) (ULONG aInstance)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* The machine doesn't need to be mutable. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd->mInstance = aInstance;

    return S_OK;
}

// IStorageController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP StorageController::GetIDEEmulationPort(LONG DevicePosition, LONG *aPortNumber)
{
    CheckComArgOutPointerValid(aPortNumber);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->mStorageControllerType != StorageControllerType_IntelAhci)
        return setError (E_NOTIMPL,
            tr ("Invalid controller type"));

    switch (DevicePosition)
    {
        case 0:
            *aPortNumber = m->bd->mPortIde0Master;
            break;
        case 1:
            *aPortNumber = m->bd->mPortIde0Slave;
            break;
        case 2:
            *aPortNumber = m->bd->mPortIde1Master;
            break;
        case 3:
            *aPortNumber = m->bd->mPortIde1Slave;
            break;
        default:
            return E_INVALIDARG;
    }

    return S_OK;
}

STDMETHODIMP StorageController::SetIDEEmulationPort(LONG DevicePosition, LONG aPortNumber)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->mStorageControllerType != StorageControllerType_IntelAhci)
        return setError (E_NOTIMPL,
            tr ("Invalid controller type"));

    if ((aPortNumber < 0) || (aPortNumber >= 30))
        return setError (E_INVALIDARG,
            tr ("Invalid port number: %l (must be in range [%lu, %lu])"),
                aPortNumber, 0, 29);

    switch (DevicePosition)
    {
        case 0:
            m->bd->mPortIde0Master = aPortNumber;
            break;
        case 1:
            m->bd->mPortIde0Slave = aPortNumber;
            break;
        case 2:
            m->bd->mPortIde1Master = aPortNumber;
            break;
        case 3:
            m->bd->mPortIde1Slave = aPortNumber;
            break;
        default:
            return E_INVALIDARG;
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////


const Utf8Str& StorageController::getName() const
{
    return m->bd->strName;
}

StorageControllerType_T StorageController::getControllerType() const
{
    return m->bd->mStorageControllerType;
}

StorageBus_T StorageController::getStorageBus() const
{
    return m->bd->mStorageBus;
}

ULONG StorageController::getInstance() const
{
    return m->bd->mInstance;
}

bool StorageController::isModified()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    return m->bd.isBackedUp();
}

bool StorageController::isReallyModified()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    return m->bd.hasActualChanges();
}

/** @note Locks objects for writing! */
bool StorageController::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    bool dataChanged = false;

    if (m->bd.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        dataChanged = m->bd.hasActualChanges();
        m->bd.rollback();
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (m->pPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (m->pPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            // attach new data to the peer and reshare it
            m->pPeer->m->bd.attach (m->bd);
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (m->pPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* peer is not modified, lock it for reading (m->pPeer is "master" so locked
     * first) */
    AutoReadLock rl(m->pPeer COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isShared())
    {
        if (!m->bd.isBackedUp())
            m->bd.backup();

        m->bd.commit();
    }

    unconst(m->pPeer).setNull();
}

const ComObjPtr<Machine, ComWeakRef>& StorageController::getMachine()
{
    return m->pParent;
}

ComObjPtr<StorageController> StorageController::getPeer()
{
    return m->pPeer;
}

// private methods
/////////////////////////////////////////////////////////////////////////////


/* vi: set tabstop=4 shiftwidth=4 expandtab: */
