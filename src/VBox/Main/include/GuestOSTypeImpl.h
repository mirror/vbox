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

#ifndef ____H_GUESTOSTYPEIMPL
#define ____H_GUESTOSTYPEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include <VBox/ostypes.h>

class ATL_NO_VTABLE GuestOSType :
    public VirtualBoxSupportErrorInfoImpl <GuestOSType, IGuestOSType>,
    public VirtualBoxSupportTranslation <GuestOSType>,
    public VirtualBoxBase,
    public IGuestOSType
{
public:

    GuestOSType();
    virtual ~GuestOSType();

    DECLARE_NOT_AGGREGATABLE(GuestOSType)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(GuestOSType)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IGuestOSType)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    // public initializer/uninitializer for internal purposes only
    HRESULT init (const char *id, const char *description, OSType osType,
                  uint32_t ramSize, uint32_t vramSize, uint32_t hddSize);

    // IGuestOSType properties
    STDMETHOD(COMGETTER(Id)) (BSTR *id);
    STDMETHOD(COMGETTER(Description)) (BSTR *description);
    STDMETHOD(COMGETTER(RecommendedRAM)) (ULONG *ramSize);
    STDMETHOD(COMGETTER(RecommendedVRAM)) (ULONG *ramSize);
    STDMETHOD(COMGETTER(RecommendedHDD)) (ULONG *hddSize);

    // public methods only for internal purposes
    const Bstr &id() const { return mID; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"GuestOSType"; }

private:

    Bstr mID;
    Bstr mDescription;
    OSType mOSType;
    uint32_t mRAMSize;
    uint32_t mVRAMSize;
    uint32_t mHDDSize;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (GuestOSType)

#endif // ____H_GUESTOSTYPEIMPL
