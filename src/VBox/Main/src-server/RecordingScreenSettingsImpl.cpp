/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation - Recording settings of one virtual screen.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_RECORDINGSCREENSETTINGS
#include "LoggingNew.h"

#include "RecordingScreenSettingsImpl.h"
#include "RecordingSettingsImpl.h"
#include "MachineImpl.h"

#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"

////////////////////////////////////////////////////////////////////////////////
//
// RecordScreenSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct RecordingScreenSettings::Data
{
    Data()
        : pParent(NULL)
    { }

    RecordingSettings * const          pParent;
    ComObjPtr<RecordingScreenSettings> pPeer;
    uint32_t                           uScreenId;

    // use the XML settings structure in the members for simplicity
    Backupable<settings::RecordingScreenSettings> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(RecordingScreenSettings)

HRESULT RecordingScreenSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void RecordingScreenSettings::FinalRelease()
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
HRESULT RecordingScreenSettings::init(RecordingSettings *aParent, uint32_t uScreenId, const settings::RecordingScreenSettings& data)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    /* Share the parent & machine weakly. */
    unconst(m->pParent)  = aParent;
    /* mPeer is left null. */

    /* Simply copy the settings data. */
    m->uScreenId = uScreenId;
    m->bd.allocate();
    m->bd->operator=(data);

    HRESULT rc = S_OK;

    int vrc = i_initInternal();
    if (RT_SUCCESS(vrc))
    {
        autoInitSpan.setSucceeded();
    }
    else
    {
        autoInitSpan.setFailed();
        rc = E_UNEXPECTED;
    }

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Initializes the recording settings object given another recording settings object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT RecordingScreenSettings::init(RecordingSettings *aParent, RecordingScreenSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    m->pPeer = that;

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);

    m->uScreenId = that->m->uScreenId;
    m->bd.share(that->m->bd);

    HRESULT rc = S_OK;

    int vrc = i_initInternal();
    if (RT_SUCCESS(vrc))
    {
        autoInitSpan.setSucceeded();
    }
    else
    {
        autoInitSpan.setFailed();
        rc = E_UNEXPECTED;
    }

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT RecordingScreenSettings::initCopy(RecordingSettings *aParent, RecordingScreenSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    /* mPeer is left null. */

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);

    m->uScreenId = that->m->uScreenId;
    m->bd.attachCopy(that->m->bd);

    HRESULT rc = S_OK;

    int vrc = i_initInternal();
    if (RT_SUCCESS(vrc))
    {
        autoInitSpan.setSucceeded();
    }
    else
    {
        autoInitSpan.setFailed();
        rc = E_UNEXPECTED;
    }

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void RecordingScreenSettings::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}

HRESULT RecordingScreenSettings::isFeatureEnabled(RecordingFeature_T aFeature, BOOL *aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    settings::RecordingFeatureMap::const_iterator itFeature = m->bd->featureMap.find(aFeature);

    *aEnabled = (   itFeature != m->bd->featureMap.end()
                 && itFeature->second == true);

    return S_OK;
}

HRESULT RecordingScreenSettings::getId(ULONG *id)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *id = m->uScreenId;

    return S_OK;
}

HRESULT RecordingScreenSettings::getEnabled(BOOL *enabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = m->bd->fEnabled ? TRUE : FALSE;

    return S_OK;
}

HRESULT RecordingScreenSettings::setEnabled(BOOL enabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    LogFlowThisFunc(("Screen %RU32\n", m->uScreenId));

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change enabled state of screen while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabled != RT_BOOL(enabled))
    {
        m->bd.backup();
        m->bd->fEnabled = RT_BOOL(enabled);
        alock.release();

        m->pParent->i_onSettingsChanged();
    }

    LogFlowThisFunc(("Screen %RU32\n", m->uScreenId));
    return S_OK;
}

HRESULT RecordingScreenSettings::getFeatures(ULONG *aFeatures)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aFeatures = 0;

    settings::RecordingFeatureMap::const_iterator itFeature = m->bd->featureMap.begin();
    while (itFeature != m->bd->featureMap.end())
    {
        if (itFeature->second) /* Is feature enable? */
            *aFeatures |= (ULONG)itFeature->first;

        ++itFeature;
    }

    return S_OK;
}

HRESULT RecordingScreenSettings::setFeatures(ULONG aFeatures)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change features while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->featureMap.clear();

    if (aFeatures & RecordingFeature_Audio)
        m->bd->featureMap[RecordingFeature_Audio] = true;
    if (aFeatures & RecordingFeature_Video)
        m->bd->featureMap[RecordingFeature_Video] = true;

    alock.release();

    return S_OK;
}

HRESULT RecordingScreenSettings::getDestination(RecordingDestination_T *aDestination)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDestination = m->bd->enmDest;

    return S_OK;
}

HRESULT RecordingScreenSettings::setDestination(RecordingDestination_T aDestination)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change destination type while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->enmDest = aDestination;

    return S_OK;
}

HRESULT RecordingScreenSettings::getFilename(com::Utf8Str &aFilename)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Get default file name if an empty string or a single "." is set. */
    if (   m->bd->File.strName.isEmpty()
        || m->bd->File.strName.equals("."))
    {
        int vrc = m->pParent->i_getDefaultFilename(m->bd->File.strName, true /* fWithFileExtension */);
        if (RT_FAILURE(vrc))
            return setError(E_INVALIDARG, tr("Error retrieving default file name"));
    }

    aFilename = m->bd->File.strName;

    return S_OK;
}

HRESULT RecordingScreenSettings::setFilename(const com::Utf8Str &aFilename)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change file name while recording is enabled"));

    Utf8Str strFile(aFilename);
    if (!RTPathStartsWithRoot(strFile.c_str()))
        return setError(E_INVALIDARG, tr("Recording file name '%s' is not absolute"), strFile.c_str());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->File.strName = strFile;

    return S_OK;
}

HRESULT RecordingScreenSettings::getMaxTime(ULONG *aMaxTimeS)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaxTimeS =  m->bd->ulMaxTimeS;

    return S_OK;
}

HRESULT RecordingScreenSettings::setMaxTime(ULONG aMaxTimeS)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change maximum time while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->ulMaxTimeS = aMaxTimeS;

    return S_OK;
}

HRESULT RecordingScreenSettings::getMaxFileSize(ULONG *aMaxFileSizeMB)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaxFileSizeMB = m->bd->File.ulMaxSizeMB;

    return S_OK;
}

HRESULT RecordingScreenSettings::setMaxFileSize(ULONG aMaxFileSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change maximum file size while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->File.ulMaxSizeMB = aMaxFileSize;

    return S_OK;
}

HRESULT RecordingScreenSettings::getOptions(com::Utf8Str &aOptions)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aOptions = m->bd->strOptions;

    return S_OK;
}

HRESULT RecordingScreenSettings::setOptions(const com::Utf8Str &aOptions)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change options while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->strOptions = aOptions;

    int vrc = RecordingScreenSettings::i_parseOptionsString(aOptions, *m->bd.data());
    if (RT_FAILURE(vrc))
        return setError(E_INVALIDARG, tr("Invalid option specified"));

    return S_OK;
}

HRESULT RecordingScreenSettings::getAudioCodec(RecordingAudioCodec_T *aCodec)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCodec = m->bd->Audio.enmAudioCodec;

    return S_OK;
}

HRESULT RecordingScreenSettings::setAudioCodec(RecordingAudioCodec_T aCodec)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio codec while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Audio.enmAudioCodec = aCodec;

    return S_OK;
}

HRESULT RecordingScreenSettings::getAudioHz(ULONG *aHz)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aHz = m->bd->Audio.uHz;

    return S_OK;
}

HRESULT RecordingScreenSettings::setAudioHz(ULONG aHz)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio Hertz rate while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Audio.uHz = (uint16_t)aHz;

    return S_OK;
}

HRESULT RecordingScreenSettings::getAudioBits(ULONG *aBits)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aBits = m->bd->Audio.cBits;

    return S_OK;
}

HRESULT RecordingScreenSettings::setAudioBits(ULONG aBits)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio bits while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Audio.cBits = (uint8_t)aBits;

    return S_OK;
}

HRESULT RecordingScreenSettings::getAudioChannels(ULONG *aChannels)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aChannels = m->bd->Audio.cChannels;

    return S_OK;
}

HRESULT RecordingScreenSettings::setAudioChannels(ULONG aChannels)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change audio channels while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Audio.cChannels = (uint8_t)aChannels;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoCodec(RecordingVideoCodec_T *aCodec)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCodec = m->bd->Video.enmCodec;

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoCodec(RecordingVideoCodec_T aCodec)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video codec while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Video.enmCodec = aCodec;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoWidth(ULONG *aVideoWidth)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoWidth = m->bd->Video.ulWidth;

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoWidth(ULONG aVideoWidth)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video width while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Video.ulWidth = aVideoWidth;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoHeight(ULONG *aVideoHeight)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoHeight = m->bd->Video.ulHeight;

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoHeight(ULONG aVideoHeight)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video height while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Video.ulHeight = aVideoHeight;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoRate(ULONG *aVideoRate)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoRate = m->bd->Video.ulRate;

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoRate(ULONG aVideoRate)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Video.ulRate = aVideoRate;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoRateControlMode(RecordingVideoRateControlMode_T *aMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMode = RecordingVideoRateControlMode_CBR; /** @todo Implement VBR. */

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoRateControlMode(RecordingVideoRateControlMode_T aMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate control mode while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Implement this. */
    RT_NOREF(aMode);

    return E_NOTIMPL;
}

HRESULT RecordingScreenSettings::getVideoFPS(ULONG *aVideoFPS)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoFPS = m->bd->Video.ulFPS;

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoFPS(ULONG aVideoFPS)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video FPS while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->Video.ulFPS = aVideoFPS;

    return S_OK;
}

HRESULT RecordingScreenSettings::getVideoScalingMethod(RecordingVideoScalingMethod_T *aMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMode = RecordingVideoScalingMethod_None; /** @todo Implement this. */

    return S_OK;
}

HRESULT RecordingScreenSettings::setVideoScalingMethod(RecordingVideoScalingMethod_T aMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->pParent->i_canChangeSettings())
        return setError(E_INVALIDARG, tr("Cannot change video rate scaling method while recording is enabled"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Implement this. */
    RT_NOREF(aMode);

    return E_NOTIMPL;
}

/**
 * Initializes data, internal version.
 *
 * @returns IPRT status code.
 */
int RecordingScreenSettings::i_initInternal(void)
{
    Assert(m);

    int rc = i_parseOptionsString(m->bd->strOptions, *m->bd.data());
    if (RT_FAILURE(rc))
        return rc;

    switch (m->bd->enmDest)
    {
        case RecordingDestination_File:
        {
            if (m->bd->File.strName.isEmpty())
                rc = m->pParent->i_getDefaultFilename(m->bd->File.strName, true /* fWithExtension */);
            break;
        }

        default:
            break;
    }

    return rc;
}

/**
 * Parses a recording screen options string and stores the parsed result in the specified screen settings.
 *
 * @returns IPRT status code.
 * @param   strOptions          Options string to parse.
 * @param   screenSettings      Where to store the parsed result into.
 */
/* static */
int RecordingScreenSettings::i_parseOptionsString(const com::Utf8Str &strOptions,
                                                  settings::RecordingScreenSettings &screenSettings)
{
    /*
     * Parse options string.
     */
    size_t pos = 0;
    com::Utf8Str key, value;
    while ((pos = strOptions.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("vc_quality", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_LIBVPX
            if (value.compare("realtime", Utf8Str::CaseInsensitive) == 0)
                mVideoRecCfg.Video.Codec.VPX.uEncoderDeadline = VPX_DL_REALTIME;
            else if (value.compare("good", Utf8Str::CaseInsensitive) == 0)
                mVideoRecCfg.Video.Codec.VPX.uEncoderDeadline = 1000000 / mVideoRecCfg.Video.uFPS;
            else if (value.compare("best", Utf8Str::CaseInsensitive) == 0)
                mVideoRecCfg.Video.Codec.VPX.uEncoderDeadline = VPX_DL_BEST_QUALITY;
            else
            {
                mVideoRecCfg.Video.Codec.VPX.uEncoderDeadline = value.toUInt32();
            }
#endif
        }
        else if (key.compare("vc_enabled", Utf8Str::CaseInsensitive) == 0)
        {
            if (value.compare("false", Utf8Str::CaseInsensitive) == 0)
            {
                screenSettings.featureMap[RecordingFeature_Video] = false;
            }
        }
        else if (key.compare("ac_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_RECORDING
            if (value.compare("true", Utf8Str::CaseInsensitive) == 0)
            {
                screenSettings.featureMap[RecordingFeature_Audio] = true;
            }
#endif
        }
        else if (key.compare("ac_profile", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_RECORDING
            if (value.compare("low", Utf8Str::CaseInsensitive) == 0)
            {
                screenSettings.Audio.uHz       = 8000;
                screenSettings.Audio.cBits     = 16;
                screenSettings.Audio.cChannels = 1;
            }
            else if (value.startsWith("med" /* "med[ium]" */, Utf8Str::CaseInsensitive) == 0)
            {
                /* Stay with the default set above. */
            }
            else if (value.compare("high", Utf8Str::CaseInsensitive) == 0)
            {
                screenSettings.Audio.uHz       = 48000;
                screenSettings.Audio.cBits     = 16;
                screenSettings.Audio.cChannels = 2;
            }
#endif
        }
        /* else just ignore. */

    } /* while */

    return VINF_SUCCESS;
}

