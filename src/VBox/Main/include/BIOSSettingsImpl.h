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

#ifndef ____H_BIOSSETTINGS
#define ____H_BIOSSETTINGS

#include "VirtualBoxBase.h"

class GuestOSType;

namespace settings
{
    struct BIOSSettings;
}

class ATL_NO_VTABLE BIOSSettings :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IBIOSSettings)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(BIOSSettings, IBIOSSettings)

    DECLARE_NOT_AGGREGATABLE(BIOSSettings)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(BIOSSettings)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IBIOSSettings)
    END_COM_MAP()

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
    STDMETHOD(COMSETTER(LogoImagePath))(IN_BSTR imagePath);
    STDMETHOD(COMGETTER(BootMenuMode))(BIOSBootMenuMode_T *bootMenuMode);
    STDMETHOD(COMSETTER(BootMenuMode))(BIOSBootMenuMode_T bootMenuMode);
    STDMETHOD(COMGETTER(ACPIEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(ACPIEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(IOAPICEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(IOAPICEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(PXEDebugEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(PXEDebugEnabled))(BOOL enable);
    STDMETHOD(COMGETTER)(TimeOffset)(LONG64 *offset);
    STDMETHOD(COMSETTER)(TimeOffset)(LONG64 offset);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::BIOSSettings &data);
    HRESULT saveSettings(settings::BIOSSettings &data);

    void rollback();
    void commit();
    void copyFrom (BIOSSettings *aThat);
    void applyDefaults (GuestOSType *aOsType);

private:
    struct Data;
    Data *m;
};

#endif // ____H_BIOSSETTINGS

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
