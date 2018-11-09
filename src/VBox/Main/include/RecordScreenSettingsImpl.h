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

#ifndef ____H_RecordScreenSettings
#define ____H_RecordScreenSettings

#include "RecordScreenSettingsWrap.h"

class RecordSettings;

namespace settings
{
    struct RecordScreenSettings;
}

class ATL_NO_VTABLE RecordScreenSettings :
    public RecordScreenSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(RecordScreenSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(RecordSettings *aParent, uint32_t uScreenId, const settings::RecordScreenSettings& data);
    HRESULT init(RecordSettings *aParent, RecordScreenSettings *that);
    HRESULT initCopy(RecordSettings *aParent, RecordScreenSettings *that);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::RecordScreenSettings &data);
    HRESULT i_saveSettings(settings::RecordScreenSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(RecordScreenSettings *aThat);
    void i_applyDefaults();

private:

    // wrapped IRecordScreenSettings methods
    HRESULT isFeatureEnabled(RecordFeature_T aFeature, BOOL *aEnabled);

    // wrapped IRecordScreenSettings properties
    HRESULT getId(ULONG *id);
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enabled);
    HRESULT getFeatures(ULONG *aFeatures);
    HRESULT setFeatures(ULONG aFeatures);
    HRESULT getDestination(RecordDestination_T *aDestination);
    HRESULT setDestination(RecordDestination_T aDestination);

    HRESULT getFileName(com::Utf8Str &aFileName);
    HRESULT setFileName(const com::Utf8Str &aFileName);
    HRESULT getMaxTime(ULONG *aMaxTimeS);
    HRESULT setMaxTime(ULONG aMaxTimeS);
    HRESULT getMaxFileSize(ULONG *aMaxFileSizeMB);
    HRESULT setMaxFileSize(ULONG aMaxFileSizeMB);
    HRESULT getOptions(com::Utf8Str &aOptions);
    HRESULT setOptions(const com::Utf8Str &aOptions);

    HRESULT getAudioCodec(RecordAudioCodec_T *aCodec);
    HRESULT setAudioCodec(RecordAudioCodec_T aCodec);
    HRESULT getAudioHz(ULONG *aHz);
    HRESULT setAudioHz(ULONG aHz);
    HRESULT getAudioBits(ULONG *aBits);
    HRESULT setAudioBits(ULONG aBits);
    HRESULT getAudioChannels(ULONG *aChannels);
    HRESULT setAudioChannels(ULONG aChannels);

    HRESULT getVideoCodec(RecordVideoCodec_T *aCodec);
    HRESULT setVideoCodec(RecordVideoCodec_T aCodec);
    HRESULT getVideoWidth(ULONG *aVideoWidth);
    HRESULT setVideoWidth(ULONG aVideoWidth);
    HRESULT getVideoHeight(ULONG *aVideoHeight);
    HRESULT setVideoHeight(ULONG aVideoHeight);
    HRESULT getVideoRate(ULONG *aVideoRate);
    HRESULT setVideoRate(ULONG aVideoRate);
    HRESULT getVideoRateControlMode(RecordVideoRateControlMode_T *aMode);
    HRESULT setVideoRateControlMode(RecordVideoRateControlMode_T aMode);
    HRESULT getVideoFPS(ULONG *aVideoFPS);
    HRESULT setVideoFPS(ULONG aVideoFPS);
    HRESULT getVideoScalingMethod(RecordVideoScalingMethod_T *aMode);
    HRESULT setVideoScalingMethod(RecordVideoScalingMethod_T aMode);

private:

    // internal methods
    int  i_initInternal();

private:

    struct Data;
    Data *m;
};

#endif // ____H_RecordScreenSettings

