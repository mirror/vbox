/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_AUDIOADAPTER
#define ____H_AUDIOADAPTER

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE AudioAdapter :
    public VirtualBoxSupportErrorInfoImpl <AudioAdapter, IAudioAdapter>,
    public VirtualBoxSupportTranslation <AudioAdapter>,
    public VirtualBoxBase,
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

    DECLARE_NOT_AGGREGATABLE(AudioAdapter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(AudioAdapter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IAudioAdapter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *parent);
    HRESULT init (Machine *parent, AudioAdapter *that);
    HRESULT initCopy (Machine *parent, AudioAdapter *that);
    void uninit();

    STDMETHOD(COMGETTER(Enabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL enabled);
    STDMETHOD(COMGETTER(AudioDriver)) (AudioDriverType_T *audioDriverType);
    STDMETHOD(COMSETTER(AudioDriver)) (AudioDriverType_T audioDriverType);

    // public methods only for internal purposes

    const Backupable <Data> &data() const { return mData; }

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    void rollback() { AutoLock alock (this); mData.rollback(); }
    void commit();
    void copyFrom (AudioAdapter *aThat);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"AudioAdapter"; }

private:

    ComObjPtr <Machine, ComWeakRef> mParent;
    ComObjPtr <AudioAdapter> mPeer;
    Backupable <Data> mData;
};

#endif // ____H_AUDIOADAPTER
