/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation - Machine capture settings.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "CaptureSettingsImpl.h"
#include "CaptureScreenSettingsImpl.h"
#include "MachineImpl.h"

#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
//
// CaptureSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct CaptureSettings::Data
{
    Data()
        : pMachine(NULL)
    { }

    Machine * const             pMachine;
    ComObjPtr<CaptureSettings>  pPeer;
    CaptureScreenSettingsMap    mapScreenSettings;

    // use the XML settings structure in the members for simplicity
    Backupable<settings::CaptureSettings> bd;
};

DEFINE_EMPTY_CTOR_DTOR(CaptureSettings)

HRESULT CaptureSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CaptureSettings::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT CaptureSettings::init(Machine *aParent)
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
 *  Initializes the capture settings object given another capture settings object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT CaptureSettings::init(Machine *aParent, CaptureSettings *that)
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
    m->mapScreenSettings = that->m->mapScreenSettings;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT CaptureSettings::initCopy(Machine *aParent, CaptureSettings *that)
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
    m->mapScreenSettings = that->m->mapScreenSettings;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void CaptureSettings::uninit()
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

// ICaptureSettings properties
/////////////////////////////////////////////////////////////////////////////

HRESULT CaptureSettings::getEnabled(BOOL *enabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fEnabled;

    return S_OK;
}

HRESULT CaptureSettings::setEnabled(BOOL enable)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const bool fEnabled = RT_BOOL(enable);

    HRESULT rc = S_OK;

    if (m->bd->fEnabled != fEnabled)
    {
        m->bd.backup();
        m->bd->fEnabled = fEnabled;

        alock.release();
        rc = m->pMachine->i_onCaptureChange();
        if (FAILED(rc))
        {
            /*
             * Normally we would do the actual change _after_ i_onVideoCaptureChange() succeeded.
             * We cannot do this because that function uses Machine::GetVideoCaptureEnabled to
             * determine if it should start or stop capturing. Therefore we need to manually
             * undo change.
             */
            alock.acquire();
            m->bd->fEnabled = m->bd.backedUpData()->fEnabled;
            return rc;
        }
        else
        {
            AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
            m->pMachine->i_setModified(Machine::IsModified_Capture);

            /** Save settings if online - @todo why is this required? -- @bugref{6818} */
            if (Global::IsOnline(adep.machineState()))
                m->pMachine->i_saveSettings(NULL);
        }
    }

    return rc;
}

HRESULT CaptureSettings::getScreens(std::vector<ComPtr<ICaptureScreenSettings> > &aCaptureScreenSettings)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aCaptureScreenSettings.clear();
    aCaptureScreenSettings.resize(m->mapScreenSettings.size());

    CaptureScreenSettingsMap::const_iterator itScreenSettings = m->mapScreenSettings.begin();
    size_t i = 0;
    while (itScreenSettings != m->mapScreenSettings.end())
    {
        itScreenSettings->second.queryInterfaceTo(aCaptureScreenSettings[i].asOutParam());
        ++i;
        ++itScreenSettings;
    }

    return S_OK;
}

HRESULT CaptureSettings::getScreenSettings(ULONG uScreenId, ComPtr<ICaptureScreenSettings> &aCaptureScreenSettings)
{
    if (uScreenId + 1 > m->mapScreenSettings.size())
        return setError(E_INVALIDARG, tr("Invalid screen ID specified"));

    CaptureScreenSettingsMap::const_iterator itScreenSettings = m->mapScreenSettings.find(uScreenId);
    if (itScreenSettings != m->mapScreenSettings.end())
    {
        itScreenSettings->second.queryInterfaceTo(aCaptureScreenSettings.asOutParam());
        return S_OK;
    }

    return VBOX_E_OBJECT_NOT_FOUND;
}

// ICaptureSettings methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Loads settings from the given settings.
 * May be called once right after this object creation.
 *
 * @param data                  Capture settings to load from.
 *
 * @note Locks this object for writing.
 */
HRESULT CaptureSettings::i_loadSettings(const settings::CaptureSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    m->mapScreenSettings.clear();

    settings::CaptureScreenMap::const_iterator itScreen = data.mapScreens.begin();
    while (itScreen != data.mapScreens.end())
    {
        ComObjPtr<CaptureScreenSettings> captureScreenSettings;
        rc = captureScreenSettings.createObject();
        if (FAILED(rc))
            break;

        rc = captureScreenSettings->init(m->pMachine, itScreen->first /* uScreenId */, itScreen->second /* Settings */);
        if (FAILED(rc))
            break;

        m->mapScreenSettings[itScreen->first] = captureScreenSettings;

        ++itScreen;
    }

    ComAssertComRC(rc);
    Assert(m->mapScreenSettings.size() == data.mapScreens.size());

    // simply copy
    m->bd.assignCopy(&data);

    return rc;
}

/**
 * Saves settings to the given settings.
 *
 * @param data                  Where to store the capture settings to.
 *
 * @note Locks this object for reading.
 */
HRESULT CaptureSettings::i_saveSettings(settings::CaptureSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *m->bd.data();

    settings::CaptureScreenMap::iterator itScreen = data.mapScreens.begin();
    while (itScreen != data.mapScreens.end())
    {
        /* Store relative path of capture file if possible. */
        m->pMachine->i_copyPathRelativeToMachine(itScreen->second.File.strName /* Source */,
                                                 itScreen->second.File.strName /* Target */);
        ++itScreen;
    }

    return S_OK;
}

void CaptureSettings::i_rollback()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->bd.rollback();
}

void CaptureSettings::i_commit()
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

void CaptureSettings::i_copyFrom(CaptureSettings *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);
}

void CaptureSettings::i_applyDefaults(void)
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Initialize default capturing settings here. */
}

