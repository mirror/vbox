/* $Id$ */

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

#ifndef ____H_AUDIOADAPTER
#define ____H_AUDIOADAPTER

#include "VirtualBoxBase.h"

namespace settings
{
    struct AudioAdapter;
}

class ATL_NO_VTABLE AudioAdapter :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IAudioAdapter)
{
public:

    struct Data
    {
        Data();

        BOOL mEnabled;
        AudioDriverType_T mAudioDriver;
        AudioControllerType_T mAudioController;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(AudioAdapter, IAudioAdapter)

    DECLARE_NOT_AGGREGATABLE(AudioAdapter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(AudioAdapter)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IAudioAdapter)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (AudioAdapter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, AudioAdapter *aThat);
    HRESULT initCopy(Machine *aParent, AudioAdapter *aThat);
    void uninit();

    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(AudioDriver))(AudioDriverType_T *aAudioDriverType);
    STDMETHOD(COMSETTER(AudioDriver))(AudioDriverType_T aAudioDriverType);
    STDMETHOD(COMGETTER(AudioController))(AudioControllerType_T *aAudioControllerType);
    STDMETHOD(COMSETTER(AudioController))(AudioControllerType_T aAudioControllerType);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::AudioAdapter &data);
    HRESULT saveSettings(settings::AudioAdapter &data);

    void rollback();
    void commit();
    void copyFrom(AudioAdapter *aThat);

private:

    Machine * const     mParent;
    const ComObjPtr<AudioAdapter> mPeer;

    Backupable<Data>    mData;
};

#endif // ____H_AUDIOADAPTER
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
