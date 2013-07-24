/* $Id$ */
/** @file
 * Implementation of IUSBController.
 */

/*
 * Copyright (C) 2005-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "USBControllerImpl.h"

#include "Global.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "HostImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>
#include <VBox/com/array.h>

#include <algorithm>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

// defines
/////////////////////////////////////////////////////////////////////////////

struct BackupableUSBData
{
    BackupableUSBData()
        : fEnabled(false),
          fEnabledEHCI(false)
    { }

    BOOL fEnabled;
    BOOL fEnabledEHCI;
};

struct USBController::Data
{
    Data(Machine *pMachine)
        : pParent(pMachine)
    { }

    ~Data()
    {};

    Machine * const                 pParent;

    // peer machine's USB controller
    const ComObjPtr<USBController>  pPeer;

    Backupable<BackupableUSBData>   bd;
};



// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(USBController)

HRESULT USBController::FinalConstruct()
{
    return BaseFinalConstruct();
}

void USBController::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 */
HRESULT USBController::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    m->bd.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Initializes the USB controller object given another USB controller object
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
HRESULT USBController::init(Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    unconst(m->pPeer) = aPeer;

    AutoWriteLock thatlock(aPeer COMMA_LOCKVAL_SRC_POS);
    m->bd.share(aPeer->m->bd);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Initializes the USB controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT USBController::initCopy(Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    AutoWriteLock thatlock(aPeer COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(aPeer->m->bd);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBController::uninit()
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


// IUSBController properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::COMGETTER(Enabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->bd->fEnabled;

    return S_OK;
}


STDMETHODIMP USBController::COMSETTER(Enabled)(BOOL aEnabled)
{
    LogFlowThisFunc(("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabled != aEnabled)
    {
        m->bd.backup();
        m->bd->fEnabled = aEnabled;

        // leave the lock for safety
        alock.release();

        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
        m->pParent->setModified(Machine::IsModified_USB);
        mlock.release();

        m->pParent->onUSBControllerChange();
    }

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(EnabledEHCI)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->bd->fEnabledEHCI;

    return S_OK;
}

STDMETHODIMP USBController::COMSETTER(EnabledEHCI)(BOOL aEnabled)
{
    LogFlowThisFunc(("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabledEHCI != aEnabled)
    {
        m->bd.backup();
        m->bd->fEnabledEHCI = aEnabled;

        // leave the lock for safety
        alock.release();

        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
        m->pParent->setModified(Machine::IsModified_USB);
        mlock.release();

        m->pParent->onUSBControllerChange();
    }

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(ProxyAvailable)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

#ifdef VBOX_WITH_USB
    *aEnabled = true;
#else
    *aEnabled = false;
#endif

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(USBStandard)(USHORT *aUSBStandard)
{
    CheckComArgOutPointerValid(aUSBStandard);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* not accessing data -- no need to lock */

    /** @todo This is no longer correct */
    *aUSBStandard = 0x0101;

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
 *  @note Does not lock "this" as Machine::loadHardware, which calls this, does not lock either.
 */
HRESULT USBController::loadSettings(const settings::USBController &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    m->bd->fEnabled = data.fEnabled;
    m->bd->fEnabledEHCI = data.fEnabledEHCI;

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT USBController::saveSettings(settings::USBController &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!m->bd->fEnabled;
    data.fEnabledEHCI = !!m->bd->fEnabledEHCI;

    return S_OK;
}

/** @note Locks objects for writing! */
void USBController::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* we need the machine state */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBController::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            /* attach new data to the peer and reshare it */
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);
            m->pPeer->m->bd.attach(m->bd);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void USBController::copyFrom(USBController *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* even more sanity */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());
    /* Machine::copyFrom() may not be called when the VM is running */
    AssertReturnVoid(!Global::IsOnline(adep.machineState()));

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);
}

// private methods
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
