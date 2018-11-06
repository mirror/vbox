/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation - Capture settings of one virtual screen.
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

#include "CaptureScreenSettingsImpl.h"
#include "MachineImpl.h"

#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
//
// CaptureScreenSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct CaptureScreenSettings::Data
{
    Data()
        : pMachine(NULL)
    { }

    Machine * const                  pMachine;
    ComObjPtr<CaptureScreenSettings> pPeer;
    uint32_t                         uScreenId;

    // use the XML settings structure in the members for simplicity
    Backupable<settings::CaptureScreenSettings> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(CaptureScreenSettings)

HRESULT CaptureScreenSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CaptureScreenSettings::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT CaptureScreenSettings::init(Machine *aParent, uint32_t uScreenId, const settings::CaptureScreenSettings& data)
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

    /* Simply copy the settings data. */
    m->uScreenId = uScreenId;
    m->bd.allocate();
    m->bd->operator=(data);

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
HRESULT CaptureScreenSettings::init(Machine *aParent, CaptureScreenSettings *that)
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

    m->uScreenId = that->m->uScreenId;
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
HRESULT CaptureScreenSettings::initCopy(Machine *aParent, CaptureScreenSettings *that)
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

    m->uScreenId = that->m->uScreenId;
    m->bd.attachCopy(that->m->bd);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void CaptureScreenSettings::uninit()
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

HRESULT CaptureScreenSettings::isFeatureEnabled(CaptureFeature_T aFeature, BOOL *aEnabled)
{
    settings::CaptureFeatureMap::const_iterator itFeature = m->bd->featureMap.find(aFeature);

    *aEnabled = (   itFeature != m->bd->featureMap.end()
                 && itFeature->second == true);

    return S_OK;
}

HRESULT CaptureScreenSettings::getEnabled(BOOL *enabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fEnabled ? TRUE : FALSE;

    return S_OK;
}

HRESULT CaptureScreenSettings::setEnabled(BOOL enabled)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabled != RT_BOOL(enabled))
    {
        alock.release();

        HRESULT rc = m->pMachine->i_onCaptureChange();
        if (FAILED(rc)) return rc;

        m->pMachine->i_setModified(Machine::IsModified_Capture);

        alock.acquire();
        m->bd->fEnabled = RT_BOOL(enabled);
        alock.release();

        /** Save settings if online - @todo why is this required? -- @bugref{6818} */
        AutoAnyStateDependency adep(m->pMachine);
        AssertComRCReturn(adep.rc(), E_UNEXPECTED);

        if (Global::IsOnline(adep.machineState()))
            m->pMachine->i_saveSettings(NULL);
    }

    return S_OK;
}

HRESULT CaptureScreenSettings::getFeatures(ULONG *aFeatures)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aFeatures = 0;

    settings::CaptureFeatureMap::const_iterator itFeature = m->bd->featureMap.begin();
    while (itFeature != m->bd->featureMap.end())
    {
        if (itFeature->second) /* Is feature enable? */
            *aFeatures |= (ULONG)itFeature->first;

        ++itFeature;
    }

    return S_OK;
}

HRESULT CaptureScreenSettings::setFeatures(ULONG aFeatures)
{
    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change features while capturing is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->featureMap.clear();

    if (aFeatures & CaptureFeature_Audio)
        m->bd->featureMap[CaptureFeature_Audio] = true;
    if (aFeatures & CaptureFeature_Video)
        m->bd->featureMap[CaptureFeature_Video] = true;

    return S_OK;
}

HRESULT CaptureScreenSettings::getDestination(CaptureDestination_T *aDestination)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDestination = m->bd->enmDest;

    return S_OK;
}

HRESULT CaptureScreenSettings::setDestination(CaptureDestination_T aDestination)
{
    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change destination type while capturing is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();
    m->bd->enmDest = aDestination;

    return S_OK;
}

HRESULT CaptureScreenSettings::getFileName(com::Utf8Str &aFileName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->File.strName.isEmpty())
        i_getDefaultCaptureFile(aFileName);
    else
        aFileName = m->bd->File.strName;

    return S_OK;
}

HRESULT CaptureScreenSettings::setFileName(const com::Utf8Str &aFileName)
{
    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change file name while capturing is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strFile(aFileName);

    if (!RTPathStartsWithRoot(strFile.c_str()))
        return setError(E_INVALIDARG, tr("Capture file name '%s' is not absolute"), strFile.c_str());

    if (!strFile.isEmpty())
    {
        Utf8Str defaultFile;
        i_getDefaultCaptureFile(defaultFile);
        if (!RTPathCompare(strFile.c_str(), defaultFile.c_str()))
            strFile.setNull();
    }

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();
    m->bd->File.strName = strFile;

    return S_OK;
}

HRESULT CaptureScreenSettings::getMaxTime(ULONG *aMaxTimeS)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaxTimeS =  m->bd->ulMaxTimeS;

    return S_OK;
}

HRESULT CaptureScreenSettings::setMaxTime(ULONG aMaxTimeS)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change maximum time while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->ulMaxTimeS = aMaxTimeS;

    return S_OK;
}

HRESULT CaptureScreenSettings::getMaxFileSize(ULONG *aMaxFileSizeMB)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaxFileSizeMB = m->bd->File.ulMaxSizeMB;

    return S_OK;
}

HRESULT CaptureScreenSettings::setMaxFileSize(ULONG aMaxFileSize)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change maximum file size while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->File.ulMaxSizeMB = aMaxFileSize;

    return S_OK;
}

HRESULT CaptureScreenSettings::getOptions(com::Utf8Str &aOptions)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aOptions = m->bd->strOptions;

    return S_OK;
}

HRESULT CaptureScreenSettings::setOptions(const com::Utf8Str &aOptions)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change options string while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->strOptions = aOptions;

    return S_OK;
}

HRESULT CaptureScreenSettings::getAudioCodec(CaptureAudioCodec_T *aCodec)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCodec = m->bd->Audio.enmAudioCodec;

    return S_OK;
}

HRESULT CaptureScreenSettings::setAudioCodec(CaptureAudioCodec_T aCodec)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio codec while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Audio.enmAudioCodec = aCodec;

    return S_OK;
}

HRESULT CaptureScreenSettings::getAudioHz(ULONG *aHz)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aHz = m->bd->Audio.uHz;

    return S_OK;
}

HRESULT CaptureScreenSettings::setAudioHz(ULONG aHz)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio Hertz rate while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Audio.uHz = (uint16_t)aHz;

    return S_OK;
}

HRESULT CaptureScreenSettings::getAudioBits(ULONG *aBits)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aBits = m->bd->Audio.cBits;

    return S_OK;
}

HRESULT CaptureScreenSettings::setAudioBits(ULONG aBits)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio bits while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Audio.cBits = (uint8_t)aBits;

    return S_OK;
}

HRESULT CaptureScreenSettings::getAudioChannels(ULONG *aChannels)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aChannels = m->bd->Audio.cChannels;

    return S_OK;
}

HRESULT CaptureScreenSettings::setAudioChannels(ULONG aChannels)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio channels while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Audio.cChannels = (uint8_t)aChannels;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoCodec(CaptureVideoCodec_T *aCodec)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCodec = m->bd->Video.enmCodec;

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoCodec(CaptureVideoCodec_T aCodec)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video codec while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Video.enmCodec = aCodec;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoWidth(ULONG *aVideoWidth)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoWidth = m->bd->Video.ulWidth;

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoWidth(ULONG aVideoWidth)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video width while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Video.ulWidth = aVideoWidth;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoHeight(ULONG *aVideoHeight)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoHeight = m->bd->Video.ulHeight;

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoHeight(ULONG aVideoHeight)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video height while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Video.ulHeight = aVideoHeight;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoRate(ULONG *aVideoRate)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoRate = m->bd->Video.ulRate;

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoRate(ULONG aVideoRate)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Video.ulRate = aVideoRate;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoRateControlMode(CaptureVideoRateControlMode_T *aMode)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMode = CaptureVideoRateControlMode_CBR; /** @todo Implement VBR. */

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoRateControlMode(CaptureVideoRateControlMode_T aMode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate control mode while capturing is enabled"));

    /** @todo Implement this. */
    RT_NOREF(aMode);

    return E_NOTIMPL;
}

HRESULT CaptureScreenSettings::getVideoFPS(ULONG *aVideoFPS)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoFPS = m->bd->Video.ulFPS;

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoFPS(ULONG aVideoFPS)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video FPS while capturing is enabled"));

    m->pMachine->i_setModified(Machine::IsModified_Capture);
    m->bd.backup();

    m->bd->Video.ulFPS = aVideoFPS;

    return S_OK;
}

HRESULT CaptureScreenSettings::getVideoScalingMethod(CaptureVideoScalingMethod_T *aMode)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMode = CaptureVideoScalingMethod_None; /** @todo Implement this. */

    return S_OK;
}

HRESULT CaptureScreenSettings::setVideoScalingMethod(CaptureVideoScalingMethod_T aMode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate scaling method while capturing is enabled"));

    /** @todo Implement this. */
    RT_NOREF(aMode);

    return E_NOTIMPL;
}

bool CaptureScreenSettings::i_canChangeSettings(void)
{
    AutoAnyStateDependency adep(m->pMachine);
    AssertComRCReturn(adep.rc(), false);

    if (   Global::IsOnline(adep.machineState())
        && m->bd->fEnabled)
        return false;

    return true;
}

/**
 * Returns the full path to the default video capture file.
 */
int CaptureScreenSettings::i_getDefaultCaptureFile(Utf8Str &strFile)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    strFile = m->pMachine->i_getSettingsFileFull(); // path/to/machinesfolder/vmname/vmname.vbox
    strFile.stripSuffix();                          // path/to/machinesfolder/vmname/vmname
    strFile.append(".webm");                        // path/to/machinesfolder/vmname/vmname.webm

    return VINF_SUCCESS;
}

