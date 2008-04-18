/* $Id$ */

/** @file
 *
 * Implementation of ISATAController.
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



#include "SATAControllerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>
#include <VBox/err.h>

#include <algorithm>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SATAController)

HRESULT SATAController::FinalConstruct()
{
    return S_OK;
}

void SATAController::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 */
HRESULT SATAController::init (Machine *aParent)
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
 * Initializes the SATA controller object given another SATA controller object
 * (a kind of copy constructor). This object shares data with
 * the object passed as an argument.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aPeer         The object to share.
 *
 * @note This object must be destroyed before the original object
 * it shares data with is destroyed.
 */
HRESULT SATAController::init (Machine *aParent, SATAController *aPeer)
{
    LogFlowThisFunc (("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet (aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aPeer;

    AutoWriteLock thatlock (aPeer);
    mData.share (aPeer->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Initializes the SATA controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT SATAController::initCopy (Machine *aParent, SATAController *aPeer)
{
    LogFlowThisFunc (("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet (aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoWriteLock thatlock (aPeer);
    mData.attachCopy (aPeer->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SATAController::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    /* uninit all filters (including those still referenced by clients) */
    uninitDependentChildren();

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}


// ISATAController properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SATAController::COMGETTER(Enabled) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}


STDMETHODIMP SATAController::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock for safety */
        alock.leave();

        mParent->onSATAControllerChange();
    }

    return S_OK;
}

STDMETHODIMP SATAController::COMGETTER(PortCount) (ULONG *aPortCount)
{
    if (!aPortCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aPortCount = mData->mPortCount;

    return S_OK;
}


STDMETHODIMP SATAController::COMSETTER(PortCount) (ULONG aPortCount)
{
    LogFlowThisFunc (("aPortCount=%u\n", aPortCount));

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

        mParent->onSATAControllerChange();
    }

    return S_OK;
}

// ISATAController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SATAController::GetIDEEmulationPort(LONG DevicePosition, LONG *aPortNumber)
{
    if (!aPortNumber)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

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

STDMETHODIMP SATAController::SetIDEEmulationPort(LONG DevicePosition, LONG aPortNumber)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());
    AutoWriteLock alock (this);

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

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT SATAController::loadSettings (const settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* SATA Controller node (required) */
    Key controller = aMachineNode.key ("SATAController");

    /* enabled (required) */
    mData->mEnabled = controller.value <bool> ("enabled");

    /* number of useable ports */
    mData->mPortCount = controller.valueOr <ULONG> ("PortCount", 30);

    /* ide emulation settings (optional, default to 0,1,2,3 respectively) */
    mData->mPortIde0Master = controller.value <ULONG> ("IDE0MasterEmulationPort");
    mData->mPortIde0Slave = controller.value <ULONG> ("IDE0SlaveEmulationPort");
    mData->mPortIde1Master = controller.value <ULONG> ("IDE1MasterEmulationPort");
    mData->mPortIde1Slave = controller.value <ULONG> ("IDE1SlaveEmulationPort");

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT SATAController::saveSettings (settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /* first, delete the entry */
    Key controller = aMachineNode.findKey ("SATAController");
    if (!controller.isNull())
        controller.zap();
    /* then, recreate it */
    controller = aMachineNode.createKey ("SATAController");

    /* enabled */
    controller.setValue <bool> ("enabled", !!mData->mEnabled);

    /* number of useable ports */
    controller.setValue <ULONG> ("PortCount", mData->mPortCount);

    /* ide emulation settings */
    controller.setValue <ULONG> ("IDE0MasterEmulationPort", mData->mPortIde0Master);
    controller.setValue <ULONG> ("IDE0SlaveEmulationPort", mData->mPortIde0Slave);
    controller.setValue <ULONG> ("IDE1MasterEmulationPort", mData->mPortIde1Master);
    controller.setValue <ULONG> ("IDE1SlaveEmulationPort", mData->mPortIde1Slave);

    return S_OK;
}

/** @note Locks objects for reading! */
bool SATAController::isModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    if (mData.isBackedUp())
        return true;

    return false;
}

/** @note Locks objects for reading! */
bool SATAController::isReallyModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    if (mData.hasActualChanges())
        return true;

    return false;
}

/** @note Locks objects for writing! */
bool SATAController::rollback()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* we need the machine state */
    Machine::AutoAnyStateDependency adep (mParent);
    AssertComRCReturn (adep.rc(), false);

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
void SATAController::commit()
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
            AutoWriteLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void SATAController::copyFrom (SATAController *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoMultiLock2 alock (aThat->rlock(), this->wlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

// private methods
/////////////////////////////////////////////////////////////////////////////

