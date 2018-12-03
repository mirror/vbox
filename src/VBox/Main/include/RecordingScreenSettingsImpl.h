/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation - Recording settings of one virtual screen.
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

#ifndef ____H_RecordingScreenSettings
#define ____H_RecordingScreenSettings

#include "RecordingScreenSettingsWrap.h"

class RecordingSettings;

namespace settings
{
    struct RecordingScreenSettings;
}

class ATL_NO_VTABLE RecordingScreenSettings :
    public RecordingScreenSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(RecordingScreenSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(RecordingSettings *aParent, uint32_t uScreenId, const settings::RecordingScreenSettings& data);
    HRESULT init(RecordingSettings *aParent, RecordingScreenSettings *that);
    HRESULT initCopy(RecordingSettings *aParent, RecordingScreenSettings *that);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::RecordingScreenSettings &data);
    HRESULT i_saveSettings(settings::RecordingScreenSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(RecordingScreenSettings *aThat);
    void i_applyDefaults();

private:

    // wrapped IRecordingScreenSettings methods
    HRESULT isFeatureEnabled(RecordingFeature_T aFeature, BOOL *aEnabled);

    // wrapped IRecordingScreenSettings properties
    HRESULT getId(ULONG *id);
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enabled);
    HRESULT getFeatures(ULONG *aFeatures);
    HRESULT setFeatures(ULONG aFeatures);
    HRESULT getDestination(RecordingDestination_T *aDestination);
    HRESULT setDestination(RecordingDestination_T aDestination);

    HRESULT getFilename(com::Utf8Str &aFilename);
    HRESULT setFilename(const com::Utf8Str &aFilename);
    HRESULT getMaxTime(ULONG *aMaxTimeS);
    HRESULT setMaxTime(ULONG aMaxTimeS);
    HRESULT getMaxFileSize(ULONG *aMaxFileSizeMB);
    HRESULT setMaxFileSize(ULONG aMaxFileSizeMB);
    HRESULT getOptions(com::Utf8Str &aOptions);
    HRESULT setOptions(const com::Utf8Str &aOptions);

    HRESULT getAudioCodec(RecordingAudioCodec_T *aCodec);
    HRESULT setAudioCodec(RecordingAudioCodec_T aCodec);
    HRESULT getAudioHz(ULONG *aHz);
    HRESULT setAudioHz(ULONG aHz);
    HRESULT getAudioBits(ULONG *aBits);
    HRESULT setAudioBits(ULONG aBits);
    HRESULT getAudioChannels(ULONG *aChannels);
    HRESULT setAudioChannels(ULONG aChannels);

    HRESULT getVideoCodec(RecordingVideoCodec_T *aCodec);
    HRESULT setVideoCodec(RecordingVideoCodec_T aCodec);
    HRESULT getVideoWidth(ULONG *aVideoWidth);
    HRESULT setVideoWidth(ULONG aVideoWidth);
    HRESULT getVideoHeight(ULONG *aVideoHeight);
    HRESULT setVideoHeight(ULONG aVideoHeight);
    HRESULT getVideoRate(ULONG *aVideoRate);
    HRESULT setVideoRate(ULONG aVideoRate);
    HRESULT getVideoRateControlMode(RecordingVideoRateControlMode_T *aMode);
    HRESULT setVideoRateControlMode(RecordingVideoRateControlMode_T aMode);
    HRESULT getVideoFPS(ULONG *aVideoFPS);
    HRESULT setVideoFPS(ULONG aVideoFPS);
    HRESULT getVideoScalingMethod(RecordingVideoScalingMethod_T *aMode);
    HRESULT setVideoScalingMethod(RecordingVideoScalingMethod_T aMode);

private:

    // internal methods
    int i_initInternal();

private:

    static int i_parseOptionsString(const com::Utf8Str &strOptions, settings::RecordingScreenSettings &screenSettings);

private:

    struct Data;
    Data *m;
};

#endif // ____H_RecordingScreenSettings

