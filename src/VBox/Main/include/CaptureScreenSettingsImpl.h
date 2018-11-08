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

#ifndef ____H_CAPTURESCREENSETTINGS
#define ____H_CAPTURESCREENSETTINGS

#include "CaptureScreenSettingsWrap.h"

namespace settings
{
    struct CaptureScreenSettings;
}

class ATL_NO_VTABLE CaptureScreenSettings :
    public CaptureScreenSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(CaptureScreenSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent, uint32_t uScreenId, const settings::CaptureScreenSettings& data);
    HRESULT init(Machine *aParent, CaptureScreenSettings *that);
    HRESULT initCopy(Machine *aParent, CaptureScreenSettings *that);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::CaptureScreenSettings &data);
    HRESULT i_saveSettings(settings::CaptureScreenSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(CaptureScreenSettings *aThat);
    void i_applyDefaults();

private:

    // wrapped ICaptureScreenSettings methods
    HRESULT isFeatureEnabled(CaptureFeature_T aFeature, BOOL *aEnabled);

    // wrapped ICaptureScreenSettings properties
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enabled);
    HRESULT getFeatures(ULONG *aFeatures);
    HRESULT setFeatures(ULONG aFeatures);
    HRESULT getDestination(CaptureDestination_T *aDestination);
    HRESULT setDestination(CaptureDestination_T aDestination);

    HRESULT getFileName(com::Utf8Str &aFileName);
    HRESULT setFileName(const com::Utf8Str &aFileName);
    HRESULT getMaxTime(ULONG *aMaxTimeS);
    HRESULT setMaxTime(ULONG aMaxTimeS);
    HRESULT getMaxFileSize(ULONG *aMaxFileSizeMB);
    HRESULT setMaxFileSize(ULONG aMaxFileSizeMB);
    HRESULT getOptions(com::Utf8Str &aOptions);
    HRESULT setOptions(const com::Utf8Str &aOptions);

    HRESULT getAudioCodec(CaptureAudioCodec_T *aCodec);
    HRESULT setAudioCodec(CaptureAudioCodec_T aCodec);
    HRESULT getAudioHz(ULONG *aHz);
    HRESULT setAudioHz(ULONG aHz);
    HRESULT getAudioBits(ULONG *aBits);
    HRESULT setAudioBits(ULONG aBits);
    HRESULT getAudioChannels(ULONG *aChannels);
    HRESULT setAudioChannels(ULONG aChannels);

    HRESULT getVideoCodec(CaptureVideoCodec_T *aCodec);
    HRESULT setVideoCodec(CaptureVideoCodec_T aCodec);
    HRESULT getVideoWidth(ULONG *aVideoWidth);
    HRESULT setVideoWidth(ULONG aVideoWidth);
    HRESULT getVideoHeight(ULONG *aVideoHeight);
    HRESULT setVideoHeight(ULONG aVideoHeight);
    HRESULT getVideoRate(ULONG *aVideoRate);
    HRESULT setVideoRate(ULONG aVideoRate);
    HRESULT getVideoRateControlMode(CaptureVideoRateControlMode_T *aMode);
    HRESULT setVideoRateControlMode(CaptureVideoRateControlMode_T aMode);
    HRESULT getVideoFPS(ULONG *aVideoFPS);
    HRESULT setVideoFPS(ULONG aVideoFPS);
    HRESULT getVideoScalingMethod(CaptureVideoScalingMethod_T *aMode);
    HRESULT setVideoScalingMethod(CaptureVideoScalingMethod_T aMode);

private:

    // internal methods
    int  i_getDefaultFileName(Utf8Str &strFile);
    int  i_initInternal();

private:

    struct Data;
    Data *m;
};

#endif // ____H_CAPTURESCREENSETTINGS

