/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_AUDIOADAPTER
#define ____H_AUDIOADAPTER

#include "AudioAdapterWrap.h"
namespace settings
{
    struct AudioAdapter;
}

class ATL_NO_VTABLE AudioAdapter :
    public AudioAdapterWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR (AudioAdapter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, AudioAdapter *aThat);
    HRESULT initCopy(Machine *aParent, AudioAdapter *aThat);
    void uninit();


    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::AudioAdapter &data);
    HRESULT i_saveSettings(settings::AudioAdapter &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(AudioAdapter *aThat);

private:

    // wrapped IAudioAdapter properties
    HRESULT getEnabled(BOOL *aEnabled);
    HRESULT setEnabled(BOOL aEnabled);
    HRESULT getAudioDriver(AudioDriverType_T *aAudioDriver);
    HRESULT setAudioDriver(AudioDriverType_T aAudioDriver);
    HRESULT getAudioController(AudioControllerType_T *aAudioController);
    HRESULT setAudioController(AudioControllerType_T aAudioController);

    Machine * const     mParent;
    const ComObjPtr<AudioAdapter> mPeer;
    struct Data;
    Data *mData;
};

#endif // ____H_AUDIOADAPTER
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
