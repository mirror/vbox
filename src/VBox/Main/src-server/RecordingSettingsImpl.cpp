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

#define LOG_GROUP LOG_GROUP_MAIN_RECORDINGSETTINGS
#include "LoggingNew.h"

#include "RecordingSettingsImpl.h"
#include "RecordingScreenSettingsImpl.h"
#include "MachineImpl.h"

#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"

////////////////////////////////////////////////////////////////////////////////
//
// RecordSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct RecordingSettings::Data
{
    Data()
        : pMachine(NULL)
    { }

    Machine * const              pMachine;
    ComObjPtr<RecordingSettings> pPeer;
    RecordScreenSettingsMap      mapScreenObj;

    // use the XML settings structure in the members for simplicity
    Backupable<settings::RecordingSettings> bd;
};

DEFINE_EMPTY_CTOR_DTOR(RecordingSettings)

HRESULT RecordingSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void RecordingSettings::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT RecordingSettings::init(Machine *aParent)
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
HRESULT RecordingSettings::init(Machine *aParent, RecordingSettings *that)
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
    m->mapScreenObj = that->m->mapScreenObj;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT RecordingSettings::initCopy(Machine *aParent, RecordingSettings *that)
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
    m->mapScreenObj = that->m->mapScreenObj;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void RecordingSettings::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    /* Note: Do *not* call i_reset() here, as the shared recording configuration
     *       otherwise gets destructed when this object goes out of scope or is destroyed. */

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pMachine) = NULL;

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}

// IRecordSettings properties
/////////////////////////////////////////////////////////////////////////////

HRESULT RecordingSettings::getEnabled(BOOL *enabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fEnabled;

    return S_OK;
}

HRESULT RecordingSettings::setEnabled(BOOL enable)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const bool fEnabled = RT_BOOL(enable);

    HRESULT rc = S_OK;

    if (m->bd->fEnabled != fEnabled)
    {
        m->bd.backup();
        m->bd->fEnabled = fEnabled;

        alock.release();
        rc = m->pMachine->i_onRecordingChange(enable);
        if (FAILED(rc))
        {
            /*
             * Normally we would do the actual change _after_ i_onCaptureChange() succeeded.
             * We cannot do this because that function uses RecordSettings::GetEnabled to
             * determine if it should start or stop capturing. Therefore we need to manually
             * undo change.
             */
            alock.acquire();
            m->bd->fEnabled = m->bd.backedUpData()->fEnabled;
        }
        else
        {
            AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
            m->pMachine->i_setModified(Machine::IsModified_Recording);

            /** Save settings if online - @todo why is this required? -- @bugref{6818} */
            if (Global::IsOnline(m->pMachine->i_getMachineState()))
                rc = m->pMachine->i_saveSettings(NULL);
        }
    }

    return rc;
}

HRESULT RecordingSettings::getScreens(std::vector<ComPtr<IRecordingScreenSettings> > &aRecordScreenSettings)
{
    LogFlowThisFuncEnter();

    i_syncToMachineDisplays();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aRecordScreenSettings.clear();
    aRecordScreenSettings.resize(m->mapScreenObj.size());

    RecordScreenSettingsMap::const_iterator itScreenSettings = m->mapScreenObj.begin();
    size_t i = 0;
    while (itScreenSettings != m->mapScreenObj.end())
    {
        itScreenSettings->second.queryInterfaceTo(aRecordScreenSettings[i].asOutParam());
        Assert(aRecordScreenSettings[i].isNotNull());
        ++i;
        ++itScreenSettings;
    }

    Assert(aRecordScreenSettings.size() == m->mapScreenObj.size());

    return S_OK;
}

HRESULT RecordingSettings::getScreenSettings(ULONG uScreenId, ComPtr<IRecordingScreenSettings> &aRecordScreenSettings)
{
    LogFlowThisFuncEnter();

    i_syncToMachineDisplays();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (uScreenId + 1 > m->mapScreenObj.size())
        return setError(E_INVALIDARG, tr("Invalid screen ID specified"));

    RecordScreenSettingsMap::const_iterator itScreenSettings = m->mapScreenObj.find(uScreenId);
    if (itScreenSettings != m->mapScreenObj.end())
    {
        itScreenSettings->second.queryInterfaceTo(aRecordScreenSettings.asOutParam());
        return S_OK;
    }

    return VBOX_E_OBJECT_NOT_FOUND;
}

// IRecordSettings methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Adds a screen settings object to a particular map.
 *
 * @returns IPRT status code. VERR_ALREADY_EXISTS if the object in question already exists.
 * @param   screenSettingsMap   Map to add screen settings to.
 * @param   uScreenId           Screen ID to add settings for.
 * @param   data                Recording screen settings to use for that screen.
 */
int RecordingSettings::i_createScreenObj(RecordScreenSettingsMap &screenSettingsMap,
                                       uint32_t uScreenId, const settings::RecordingScreenSettings &data)
{
    LogFlowThisFunc(("Screen %RU32\n", uScreenId));

    if (screenSettingsMap.find(uScreenId) != screenSettingsMap.end())
    {
        AssertFailed();
        return VERR_ALREADY_EXISTS;
    }

    int vrc = VINF_SUCCESS;

    ComObjPtr<RecordingScreenSettings> recordingScreenSettings;
    HRESULT rc = recordingScreenSettings.createObject();
    if (SUCCEEDED(rc))
    {
        rc = recordingScreenSettings->init(this, uScreenId, data);
        if (SUCCEEDED(rc))
        {
            try
            {
                screenSettingsMap[uScreenId] = recordingScreenSettings;
            }
            catch (std::bad_alloc &)
            {
                vrc = VERR_NO_MEMORY;
            }
        }
    }

    return vrc;
}

/**
 * Removes a screen settings object from a particular map.
 *
 * @returns IPRT status code. VERR_NOT_FOUND if specified screen was not found.
 * @param   screenSettingsMap   Map to remove screen settings from.
 * @param   uScreenId           ID of screen to remove.
 */
int RecordingSettings::i_destroyScreenObj(RecordScreenSettingsMap &screenSettingsMap, uint32_t uScreenId)
{
    LogFlowThisFunc(("Screen %RU32\n", uScreenId));

    AssertReturn(uScreenId > 0, VERR_INVALID_PARAMETER); /* Removing screen 0 isn't a good idea. */

    RecordScreenSettingsMap::iterator itScreen = screenSettingsMap.find(uScreenId);
    if (itScreen == screenSettingsMap.end())
    {
        AssertFailed();
        return VERR_NOT_FOUND;
    }

    /* Make sure to consume the pointer before the one of the
     * iterator gets released. */
    ComObjPtr<RecordingScreenSettings> pScreenSettings = itScreen->second;

    screenSettingsMap.erase(itScreen);

    pScreenSettings.setNull();

    return VINF_SUCCESS;
}

/**
 * Destroys all screen settings objects of a particular map.
 *
 * @returns IPRT status code.
 * @param   screenSettingsMap   Map to destroy screen settings objects for.
 */
int RecordingSettings::i_destroyAllScreenObj(RecordScreenSettingsMap &screenSettingsMap)
{
    LogFlowThisFuncEnter();

    RecordScreenSettingsMap::iterator itScreen = screenSettingsMap.begin();
    if (itScreen != screenSettingsMap.end())
    {
        /* Make sure to consume the pointer before the one of the
         * iterator gets released. */
        ComObjPtr<RecordingScreenSettings> pScreenSettings = itScreen->second;

        screenSettingsMap.erase(itScreen);

        pScreenSettings.setNull();

        itScreen = screenSettingsMap.begin();
    }

    return VINF_SUCCESS;
}

/**
 * Loads settings from the given settings.
 * May be called once right after this object creation.
 *
 * @param data                  Capture settings to load from.
 *
 * @note Locks this object for writing.
 */
HRESULT RecordingSettings::i_loadSettings(const settings::RecordingSettings &data)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    i_reset();

    LogFlowThisFunc(("Data has %zu screens\n", data.mapScreens.size()));

    settings::RecordingScreenMap::const_iterator itScreen = data.mapScreens.begin();
    while (itScreen != data.mapScreens.end())
    {
        int vrc = i_createScreenObj(m->mapScreenObj,
                                    itScreen->first /* uScreenId */, itScreen->second /* Settings */);
        if (RT_FAILURE(vrc))
        {
            rc = E_OUTOFMEMORY;
            break;
        }

        ++itScreen;
    }

    if (FAILED(rc))
        return rc;

    ComAssertComRC(rc);
    Assert(m->mapScreenObj.size() == data.mapScreens.size());

    // simply copy
    m->bd.assignCopy(&data);

    LogFlowThisFunc(("Returning %Rhrc\n", rc));
    return rc;
}

/**
 * Resets the internal object state by destroying all screen settings objects.
 */
void RecordingSettings::i_reset(void)
{
    LogFlowThisFuncEnter();

    i_destroyAllScreenObj(m->mapScreenObj);
    m->bd->mapScreens.clear();
}

/**
 * Saves settings to the given settings.
 *
 * @param data                  Where to store the capture settings to.
 *
 * @note Locks this object for reading.
 */
HRESULT RecordingSettings::i_saveSettings(settings::RecordingSettings &data)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    int rc2 = i_syncToMachineDisplays();
    AssertRC(rc2);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *m->bd.data();

    settings::RecordingScreenMap::iterator itScreen = data.mapScreens.begin();
    while (itScreen != data.mapScreens.end())
    {
        /* Store relative path of capture file if possible. */
        m->pMachine->i_copyPathRelativeToMachine(itScreen->second.File.strName /* Source */,
                                                 itScreen->second.File.strName /* Target */);
        ++itScreen;
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

void RecordingSettings::i_rollback()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->bd.rollback();
}

void RecordingSettings::i_commit()
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

void RecordingSettings::i_copyFrom(RecordingSettings *aThat)
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

void RecordingSettings::i_applyDefaults(void)
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Initialize default capturing settings here. */
}

/**
 * Returns the full path to the default video capture file.
 */
int RecordingSettings::i_getDefaultFileName(Utf8Str &strFile, bool fWithFileExtension)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    strFile = m->pMachine->i_getSettingsFileFull(); // path/to/machinesfolder/vmname/vmname.vbox
    strFile.stripSuffix();                          // path/to/machinesfolder/vmname/vmname
    if (fWithFileExtension)
        strFile.append(".webm");                    // path/to/machinesfolder/vmname/vmname.webm

    return VINF_SUCCESS;
}

/**
 * Determines whether the recording settings currently can be changed or not.
 *
 * @returns \c true if the settings can be changed, \c false if not.
 */
bool RecordingSettings::i_canChangeSettings(void)
{
    AutoAnyStateDependency adep(m->pMachine);
    if (FAILED(adep.rc()))
        return false;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Only allow settings to be changed when recording is disabled when the machine is running. */
    if (   Global::IsOnline(adep.machineState())
        && m->bd->fEnabled)
    {
        return false;
    }

    return true;
}

/**
 * Gets called when the machine object needs to know that the recording settings
 * have been changed.
 */
void RecordingSettings::i_onSettingsChanged(void)
{
    LogFlowThisFuncEnter();

    AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
    m->pMachine->i_setModified(Machine::IsModified_Recording);
    mlock.release();

    LogFlowThisFuncLeave();
}

/**
 * Synchronizes the screen settings (COM) objects and configuration data
 * to the number of the machine's configured displays.
 */
int RecordingSettings::i_syncToMachineDisplays(void)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(m->pMachine);
    const ULONG cMonitors = m->pMachine->i_getMonitorCount();

    LogFlowThisFunc(("cMonitors=%RU32\n", cMonitors));
    LogFlowThisFunc(("Data screen count = %zu, COM object count = %zu\n", m->bd->mapScreens.size(), m->mapScreenObj.size()));

    /* If counts match, take a shortcut. */
    if (cMonitors == m->mapScreenObj.size())
        return VINF_SUCCESS;

    /* Create all new screen settings objects which are not there yet. */
    for (ULONG i = 0; i < cMonitors; i++)
    {
        if (m->mapScreenObj.find(i) == m->mapScreenObj.end())
        {
            settings::RecordingScreenMap::const_iterator itScreen = m->bd->mapScreens.find(i);
            if (itScreen == m->bd->mapScreens.end())
            {
                settings::RecordingScreenSettings defaultScreenSettings; /* Apply default settings. */
                m->bd->mapScreens[i] = defaultScreenSettings;
            }

            int vrc2 = i_createScreenObj(m->mapScreenObj, i /* Screen ID */, m->bd->mapScreens[i]);
            AssertRC(vrc2);
        }
    }

    /* Remove all left over screen settings objects which are not needed anymore. */
    const ULONG cSettings = (ULONG)m->mapScreenObj.size();
    for (ULONG i = cMonitors; i < cSettings; i++)
    {
        m->bd->mapScreens.erase(i);
        int vrc2 = i_destroyScreenObj(m->mapScreenObj, i /* Screen ID */);
        AssertRC(vrc2);
    }

    Assert(m->mapScreenObj.size() == cMonitors);
    Assert(m->bd->mapScreens.size() == cMonitors);

    LogFlowThisFuncLeave();
    return VINF_SUCCESS;
}

