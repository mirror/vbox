/* $Id$ */

/** @file
 *
 * Implementation of IStorageController.
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "StorageControllerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "SystemPropertiesImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <algorithm>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

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
          fUseHostIOCache(true),
          fBootable(false),
          mPortIde0Master(0),
          mPortIde0Slave(1),
          mPortIde1Master(2),
          mPortIde1Slave(3)
    { }

    /** Unique name of the storage controller. */
    Utf8Str strName;
    /** The connection type of the storage controller. */
    StorageBus_T mStorageBus;
    /** Type of the Storage controller. */
    StorageControllerType_T mStorageControllerType;
    /** Instance number of the storage controller. */
    ULONG mInstance;
    /** Number of usable ports. */
    ULONG mPortCount;
    /** Whether to use the host IO caches. */
    BOOL fUseHostIOCache;
    /** Whether it is possible to boot from disks attached to this controller. */
    BOOL fBootable;

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
    Data(Machine * const aMachine)
        : pVirtualBox(NULL),
          pSystemProperties(NULL),
          pParent(aMachine)
    {
        unconst(pVirtualBox) = aMachine->getVirtualBox();
        unconst(pSystemProperties) = pVirtualBox->getSystemProperties();
    }

    VirtualBox * const                  pVirtualBox;
    SystemProperties * const            pSystemProperties;

    Machine * const                     pParent;
    const ComObjPtr<StorageController>  pPeer;

    Backupable<BackupableStorageControllerData> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT StorageController::FinalConstruct()
{
    return BaseFinalConstruct();
}

void StorageController::FinalRelease()
{
    uninit();
    BaseFinalRelease();
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
                                ULONG aInstance, bool fBootable)
{
    LogFlowThisFunc(("aParent=%p aName=\"%s\" aInstance=%u\n",
                     aParent, aName.c_str(), aInstance));

    ComAssertRet(aParent && !aName.isEmpty(), E_INVALIDARG);
    if (   (aStorageBus <= StorageBus_Null)
        || (aStorageBus >  StorageBus_SAS))
        return setError(E_INVALIDARG,
                        tr("Invalid storage connection type"));

    ULONG maxInstances;
    ChipsetType_T chipsetType;
    HRESULT rc = aParent->COMGETTER(ChipsetType)(&chipsetType);
    if (FAILED(rc))
        return rc;
    rc = aParent->getVirtualBox()->getSystemProperties()->GetMaxInstancesOfStorageBus(chipsetType, aStorageBus, &maxInstances);
    if (FAILED(rc))
        return rc;
    if (aInstance >= maxInstances)
        return setError(E_INVALIDARG,
                        tr("Too many storage controllers of this type"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* m->pPeer is left null */

    m->bd.allocate();

    m->bd->strName = aName;
    m->bd->mInstance = aInstance;
    m->bd->fBootable = fBootable;
    m->bd->mStorageBus = aStorageBus;
    if (   aStorageBus != StorageBus_IDE
        && aStorageBus != StorageBus_Floppy)
        m->bd->fUseHostIOCache = false;
    else
        m->bd->fUseHostIOCache = true;

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
            m->bd->mPortCount = 1;
            m->bd->mStorageControllerType = StorageControllerType_I82078;
            break;
        case StorageBus_SAS:
            m->bd->mPortCount = 8;
            m->bd->mStorageControllerType = StorageControllerType_LsiLogicSas;
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

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* sanity */
    AutoCaller thatCaller(aThat);
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

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);
    /* m->pPeer is left null */

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatlock(aThat COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(aThat->m->bd);

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

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

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
        case StorageBus_SAS:
        {
            if (aControllerType != StorageControllerType_LsiLogicSas)
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
    HRESULT rc = m->pSystemProperties->GetMaxDevicesPerPortForStorageBus(m->bd->mStorageBus, aMaxDevices);

    return rc;
}

STDMETHODIMP StorageController::COMGETTER(MinPortCount) (ULONG *aMinPortCount)
{
    CheckComArgOutPointerValid(aMinPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = m->pSystemProperties->GetMinPortCountForStorageBus(m->bd->mStorageBus, aMinPortCount);

    return rc;
}

STDMETHODIMP StorageController::COMGETTER(MaxPortCount) (ULONG *aMaxPortCount)
{
    CheckComArgOutPointerValid(aMaxPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = m->pSystemProperties->GetMaxPortCountForStorageBus(m->bd->mStorageBus, aMaxPortCount);

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
            if (aPortCount < 1 || aPortCount > 30)
                return setError(E_INVALIDARG,
                                tr("Invalid port count: %lu (must be in range [%lu, %lu])"),
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
                return setError(E_INVALIDARG,
                                tr("Invalid port count: %lu (must be in range [%lu, %lu])"),
                                aPortCount, 16, 16);
            break;
        }
        case StorageBus_IDE:
        {
            /*
             * The port count is fixed to 2.
             */
            if (aPortCount != 2)
                return setError(E_INVALIDARG,
                                tr("Invalid port count: %lu (must be in range [%lu, %lu])"),
                                aPortCount, 2, 2);
            break;
        }
        case StorageBus_Floppy:
        {
            /*
             * The port count is fixed to 1.
             */
            if (aPortCount != 1)
                return setError(E_INVALIDARG,
                                tr("Invalid port count: %lu (must be in range [%lu, %lu])"),
                                aPortCount, 1, 1);
            break;
        }
        case StorageBus_SAS:
        {
            /*
             * The port count is fixed to 8.
             */
            if (aPortCount != 8)
                return setError(E_INVALIDARG,
                                tr("Invalid port count: %lu (must be in range [%lu, %lu])"),
                                aPortCount, 8, 8);
            break;
        }
        default:
            AssertMsgFailed(("Invalid controller type %d\n", m->bd->mStorageBus));
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->mPortCount != aPortCount)
    {
        m->bd.backup();
        m->bd->mPortCount = aPortCount;

        alock.release();
        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);        // m->pParent is const, needs no locking
        m->pParent->setModified(Machine::IsModified_Storage);
        mlock.release();

        m->pParent->onStorageControllerChange();
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

STDMETHODIMP StorageController::COMGETTER(UseHostIOCache) (BOOL *fUseHostIOCache)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* The machine doesn't need to be mutable. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *fUseHostIOCache = m->bd->fUseHostIOCache;

    return S_OK;
}

STDMETHODIMP StorageController::COMSETTER(UseHostIOCache) (BOOL fUseHostIOCache)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fUseHostIOCache != !!fUseHostIOCache)
    {
        m->bd.backup();
        m->bd->fUseHostIOCache = !!fUseHostIOCache;

        alock.release();
        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);        // m->pParent is const, needs no locking
        m->pParent->setModified(Machine::IsModified_Storage);
        mlock.release();

        m->pParent->onStorageControllerChange();
    }

    return S_OK;
}

STDMETHODIMP StorageController::COMGETTER(Bootable) (BOOL *fBootable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* The machine doesn't need to be mutable. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *fBootable = m->bd->fBootable;

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
        return setError(E_NOTIMPL,
                        tr("Invalid controller type"));

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
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->mStorageControllerType != StorageControllerType_IntelAhci)
        return setError(E_NOTIMPL,
                        tr("Invalid controller type"));

    if (aPortNumber < 0 || aPortNumber >= 30)
        return setError(E_INVALIDARG,
                        tr("Invalid port number: %ld (must be in range [%lu, %lu])"),
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

bool StorageController::getBootable() const
{
    return !!m->bd->fBootable;
}

/**
 * Returns S_OK if the given port and device numbers are within the range supported
 * by this controller. If not, it sets an error and returns E_INVALIDARG.
 * @param ulPort
 * @param ulDevice
 * @return
 */
HRESULT StorageController::checkPortAndDeviceValid(LONG aControllerPort,
                                                   LONG aDevice)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ULONG portCount = m->bd->mPortCount;
    ULONG devicesPerPort;
    HRESULT rc = m->pSystemProperties->GetMaxDevicesPerPortForStorageBus(m->bd->mStorageBus, &devicesPerPort);
    if (FAILED(rc)) return rc;

    if (   aControllerPort < 0
        || aControllerPort >= (LONG)portCount
        || aDevice < 0
        || aDevice >= (LONG)devicesPerPort
       )
        return setError(E_INVALIDARG,
                        tr("The port and/or device parameter are out of range: port=%d (must be in range [0, %d]), device=%d (must be in range [0, %d])"),
                        (int)aControllerPort, (int)portCount-1, (int)aDevice, (int)devicesPerPort-1);

    return S_OK;
}

/** @note Locks objects for writing! */
void StorageController::setBootable(BOOL fBootable)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fBootable = fBootable;
}

/** @note Locks objects for writing! */
void StorageController::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();
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

    unconst(m->pPeer) = NULL;
}

Machine* StorageController::getMachine()
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
