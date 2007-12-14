/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

class Machine;

class ATL_NO_VTABLE AudioAdapter :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <AudioAdapter, IAudioAdapter>,
    public VirtualBoxSupportTranslation <AudioAdapter>,
    public IAudioAdapter
{
public:

    struct Data
    {
        Data() {
            mEnabled = false;
            mAudioDriver = AudioDriverType_NullAudioDriver;
        }

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mEnabled == that.mEnabled &&
                    mAudioDriver == that.mAudioDriver);
        }

        BOOL mEnabled;
        AudioDriverType_T mAudioDriver;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (AudioAdapter)

    DECLARE_NOT_AGGREGATABLE(AudioAdapter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(AudioAdapter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IAudioAdapter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (AudioAdapter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent);
    HRESULT init (Machine *aParent, AudioAdapter *aThat);
    HRESULT initCopy (Machine *aParent, AudioAdapter *aThat);
    void uninit();

    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(AudioDriver)) (AudioDriverType_T *aAudioDriverType);
    STDMETHOD(COMSETTER(AudioDriver)) (AudioDriverType_T aAudioDriverType);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aMachineNode);
    HRESULT saveSettings (settings::Key &aMachineNode);

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (AudioAdapter *aThat);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    const Backupable <Data> &data() const { return mData; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"AudioAdapter"; }

private:

    const ComObjPtr <Machine, ComWeakRef> mParent;
    const ComObjPtr <AudioAdapter> mPeer;

    Backupable <Data> mData;
};

#endif // ____H_AUDIOADAPTER
