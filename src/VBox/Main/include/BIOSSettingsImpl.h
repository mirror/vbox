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

#ifndef ____H_BIOSSETTINGS
#define ____H_BIOSSETTINGS

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE BIOSSettings :
    public VirtualBoxSupportErrorInfoImpl <BIOSSettings, IBIOSSettings>,
    public VirtualBoxSupportTranslation <BIOSSettings>,
    public VirtualBoxBaseNEXT,
    public IBIOSSettings
{
public:

    struct Data
    {
        Data()
        {
            mLogoFadeIn = true;
            mLogoFadeOut = true;
            mLogoDisplayTime = 0;
            mBootMenuMode = BIOSBootMenuMode_MessageAndMenu;
            mACPIEnabled = true;
            mIOAPICEnabled = false;
            mPXEDebugEnabled = false;
            mTimeOffset = 0;
            mIDEControllerType = IDEControllerType_PIIX4;
        }

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mLogoFadeIn         == that.mLogoFadeIn &&
                    mLogoFadeOut        == that.mLogoFadeOut &&
                    mLogoDisplayTime    == that.mLogoDisplayTime &&
                    mLogoImagePath      == that.mLogoImagePath &&
                    mBootMenuMode       == that.mBootMenuMode &&
                    mACPIEnabled        == that.mACPIEnabled &&
                    mIOAPICEnabled      == that.mIOAPICEnabled &&
                    mPXEDebugEnabled    == that.mPXEDebugEnabled &&
                    mIDEControllerType  == that.mIDEControllerType &&
                    mTimeOffset         == that.mTimeOffset);
        }

        BOOL                mLogoFadeIn;
        BOOL                mLogoFadeOut;
        ULONG               mLogoDisplayTime;
        Bstr                mLogoImagePath;
        BIOSBootMenuMode_T  mBootMenuMode;
        BOOL                mACPIEnabled;
        BOOL                mIOAPICEnabled;
        BOOL                mPXEDebugEnabled;
        LONG64              mTimeOffset;
        IDEControllerType_T mIDEControllerType;
    };

    DECLARE_NOT_AGGREGATABLE(BIOSSettings)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(BIOSSettings)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IBIOSSettings)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *parent);
    HRESULT init (Machine *parent, BIOSSettings *that);
    HRESULT initCopy (Machine *parent, BIOSSettings *that);
    void uninit();

    STDMETHOD(COMGETTER(LogoFadeIn))(BOOL *enabled);
    STDMETHOD(COMSETTER(LogoFadeIn))(BOOL enable);
    STDMETHOD(COMGETTER(LogoFadeOut))(BOOL *enabled);
    STDMETHOD(COMSETTER(LogoFadeOut))(BOOL enable);
    STDMETHOD(COMGETTER(LogoDisplayTime))(ULONG *displayTime);
    STDMETHOD(COMSETTER(LogoDisplayTime))(ULONG displayTime);
    STDMETHOD(COMGETTER(LogoImagePath))(BSTR *imagePath);
    STDMETHOD(COMSETTER(LogoImagePath))(INPTR BSTR imagePath);
    STDMETHOD(COMGETTER(BootMenuMode))(BIOSBootMenuMode_T *bootMenuMode);
    STDMETHOD(COMSETTER(BootMenuMode))(BIOSBootMenuMode_T bootMenuMode);
    STDMETHOD(COMGETTER(ACPIEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(ACPIEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(IOAPICEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(IOAPICEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(PXEDebugEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(PXEDebugEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(IDEControllerType))(IDEControllerType_T *controllerType);
    STDMETHOD(COMSETTER(IDEControllerType))(IDEControllerType_T controllerType);
    STDMETHOD(COMGETTER)(TimeOffset)(LONG64 *offset);
    STDMETHOD(COMSETTER)(TimeOffset)(LONG64 offset);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aMachineNode);
    HRESULT saveSettings (settings::Key &aMachineNode);

    const Backupable <Data> &data() const { return mData; }

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    void rollback() { AutoLock alock (this); mData.rollback(); }
    void commit();
    void copyFrom (BIOSSettings *aThat);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"BIOSSettings"; }

private:

    ComObjPtr <Machine, ComWeakRef> mParent;
    ComObjPtr <BIOSSettings> mPeer;
    Backupable <Data> mData;
};

#endif // ____H_BIOSSETTINGS

