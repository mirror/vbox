/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "BIOSSettingsImpl.h"
#include "MachineImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
//
// BIOSSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct BIOSSettings::Data
{
    Data()
        : pMachine(NULL)
    { }

    Machine * const             pMachine;
    ComObjPtr<BIOSSettings>     pPeer;

    // use the XML settings structure in the members for simplicity
    Backupable<settings::BIOSSettings> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT BIOSSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void BIOSSettings::FinalRelease()
{
    uninit ();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT BIOSSettings::init(Machine *aParent)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    /* share the parent weakly */
    unconst(m->pMachine) = aParent;

    m->bd.allocate();

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the audio adapter object given another audio adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT BIOSSettings::init(Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;
    m->pPeer = that;

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);
    m->bd.share(that->m->bd);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT BIOSSettings::initCopy(Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;
    // mPeer is left null

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(that->m->bd);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void BIOSSettings::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pMachine) = NULL;

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}

// IBIOSSettings properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeIn)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fLogoFadeIn;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeIn)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fLogoFadeIn = !!enable;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeOut)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fLogoFadeOut;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeOut)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fLogoFadeOut = !!enable;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoDisplayTime)(ULONG *displayTime)
{
    if (!displayTime)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *displayTime = m->bd->ulLogoDisplayTime;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoDisplayTime)(ULONG displayTime)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->ulLogoDisplayTime = displayTime;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoImagePath)(BSTR *imagePath)
{
    if (!imagePath)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd->strLogoImagePath.cloneTo(imagePath);
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoImagePath)(IN_BSTR imagePath)
{
    /* NULL strings are not allowed */
    if (!imagePath)
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->strLogoImagePath = imagePath;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(BootMenuMode)(BIOSBootMenuMode_T *bootMenuMode)
{
    if (!bootMenuMode)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *bootMenuMode = m->bd->biosBootMenuMode;
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(BootMenuMode)(BIOSBootMenuMode_T bootMenuMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->biosBootMenuMode = bootMenuMode;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(ACPIEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fACPIEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(ACPIEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fACPIEnabled = !!enable;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(IOAPICEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fIOAPICEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(IOAPICEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fIOAPICEnabled = !!enable;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(PXEDebugEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fPXEDebugEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(PXEDebugEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->fPXEDebugEnabled = !!enable;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(TimeOffset)(LONG64 *offset)
{
    if (!offset)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *offset = m->bd->llTimeOffset;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(TimeOffset)(LONG64 offset)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->llTimeOffset = offset;

    alock.release();
    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
    m->pMachine->setModified(Machine::IsModified_BIOS);

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(NonVolatileStorageFile)(BSTR *pbstrPath)
{
    CheckComArgOutPointerValid(pbstrPath);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        Bstr bstrEmpty("");
        hrc = bstrEmpty.cloneToEx(pbstrPath);
    }

    return hrc;
}



// IBIOSSettings methods
/////////////////////////////////////////////////////////////////////////////

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
HRESULT BIOSSettings::loadSettings(const settings::BIOSSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // simply copy
    *m->bd.data() = data;

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT BIOSSettings::saveSettings(settings::BIOSSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *m->bd.data();

    return S_OK;
}

void BIOSSettings::rollback()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->bd.rollback();
}

void BIOSSettings::commit()
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

void BIOSSettings::copyFrom (BIOSSettings *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);
}

void BIOSSettings::applyDefaults (GuestOSType *aOsType)
{
    AssertReturnVoid (aOsType != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Initialize default BIOS settings here */
    m->bd->fIOAPICEnabled = aOsType->recommendedIOAPIC();
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
