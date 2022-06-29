/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_AudioSettingsImpl_h
#define MAIN_INCLUDED_AudioSettingsImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "AudioAdapterImpl.h"
#include "GuestOSTypeImpl.h"

#include "AudioSettingsWrap.h"
namespace settings
{
    struct AudioSettings;
}

class ATL_NO_VTABLE AudioSettings :
    public AudioSettingsWrap
{
public:

    DECLARE_COMMON_CLASS_METHODS(AudioSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, AudioSettings *aThat);
    HRESULT initCopy(Machine *aParent, AudioSettings *aThat);
    void uninit();

    HRESULT getHostAudioDevice(AudioDirection_T aUsage, ComPtr<IHostAudioDevice> &aDevice);
    HRESULT setHostAudioDevice(const ComPtr<IHostAudioDevice> &aDevice, AudioDirection_T aUsage);
    HRESULT getAdapter(ComPtr<IAudioAdapter> &aAdapter);

    // public methods only for internal purposes
    Machine* i_getParent(void);
    bool     i_canChangeSettings(void);
    void     i_onAdapterChanged(IAudioAdapter *pAdapter);
    void     i_onHostDeviceChanged(IHostAudioDevice *pDevice, bool fIsNew, AudioDeviceState_T enmState, IVirtualBoxErrorInfo *pErrInfo);
    void     i_onSettingsChanged(void);
    HRESULT  i_loadSettings(const settings::AudioAdapter &data);
    HRESULT  i_saveSettings(settings::AudioAdapter &data);
    void     i_copyFrom(AudioSettings *aThat);
    HRESULT  i_applyDefaults(ComObjPtr<GuestOSType> &aGuestOsType);

    void i_rollback();
    void i_commit();

private:

    struct Data;
    Data *m;
};
#endif /* !MAIN_INCLUDED_AudioSettingsImpl_h */

