/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_GUESTOSTYPEIMPL
#define ____H_GUESTOSTYPEIMPL

#include "VirtualBoxBase.h"
#include "Global.h"

#include <VBox/ostypes.h>

class ATL_NO_VTABLE GuestOSType :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <GuestOSType, IGuestOSType>,
    public VirtualBoxSupportTranslation <GuestOSType>,
    VBOX_SCRIPTABLE_IMPL(IGuestOSType)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (GuestOSType)

    DECLARE_NOT_AGGREGATABLE(GuestOSType)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(GuestOSType)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IGuestOSType)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (GuestOSType)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (const char *aFamilyId, const char *aFamilyDescription,
                  const char *aId, const char *aDescription,
                  VBOXOSTYPE aOSType, uint32_t aOSHint,
                  uint32_t aRAMSize, uint32_t aVRAMSize, uint32_t aHDDSize,
                  NetworkAdapterType_T aNetworkAdapterType);
    void uninit();

    // IGuestOSType properties
    STDMETHOD(COMGETTER(FamilyId)) (BSTR *aFamilyId);
    STDMETHOD(COMGETTER(FamilyDescription)) (BSTR *aFamilyDescription);
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMGETTER(Is64Bit)) (BOOL *aIs64Bit);
    STDMETHOD(COMGETTER(RecommendedIOAPIC)) (BOOL *aRecommendedIOAPIC);
    STDMETHOD(COMGETTER(RecommendedVirtEx)) (BOOL *aRecommendedVirtEx);
    STDMETHOD(COMGETTER(RecommendedRAM)) (ULONG *aRAMSize);
    STDMETHOD(COMGETTER(RecommendedVRAM)) (ULONG *aVRAMSize);
    STDMETHOD(COMGETTER(RecommendedHDD)) (ULONG *aHDDSize);
    STDMETHOD(COMGETTER(AdapterType)) (NetworkAdapterType_T *aNetworkAdapterType);

    // public methods only for internal purposes
    const Bstr &id() const { return mID; }
    bool is64Bit() const { return !!(mOSHint & VBOXOSHINT_64BIT); }
    bool recommendedIOAPIC() const { return !!(mOSHint & VBOXOSHINT_IOAPIC); }
    bool recommendedVirtEx() const { return !!(mOSHint & VBOXOSHINT_HWVIRTEX); }
    NetworkAdapterType_T networkAdapterType() const { return mNetworkAdapterType; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"GuestOSType"; }

private:

    const Bstr mFamilyID;
    const Bstr mFamilyDescription;
    const Bstr mID;
    const Bstr mDescription;
    const VBOXOSTYPE mOSType;
    const uint32_t mOSHint;
    const uint32_t mRAMSize;
    const uint32_t mVRAMSize;
    const uint32_t mHDDSize;
    const uint32_t mMonitorCount;
    const NetworkAdapterType_T mNetworkAdapterType;
};

#endif // ____H_GUESTOSTYPEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
