/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include "Collection.h"

#include <VBox/ostypes.h>

class ATL_NO_VTABLE GuestOSType :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <GuestOSType, IGuestOSType>,
    public VirtualBoxSupportTranslation <GuestOSType>,
    public IGuestOSType
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (DVDImage)

    DECLARE_NOT_AGGREGATABLE(GuestOSType)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(GuestOSType)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IGuestOSType)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (GuestOSType)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (const char *aId, const char *aDescription, VBOXOSTYPE aOSType,
                  uint32_t aRAMSize, uint32_t aVRAMSize, uint32_t aHDDSize);
    void uninit();

    // IGuestOSType properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMGETTER(RecommendedRAM)) (ULONG *aRAMSize);
    STDMETHOD(COMGETTER(RecommendedVRAM)) (ULONG *aVRAMSize);
    STDMETHOD(COMGETTER(RecommendedHDD)) (ULONG *aHDDSize);

    // public methods only for internal purposes
    const Bstr &id() const { return mID; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"GuestOSType"; }

private:

    const Bstr mID;
    const Bstr mDescription;
    const VBOXOSTYPE mOSType;
    const uint32_t mRAMSize;
    const uint32_t mVRAMSize;
    const uint32_t mHDDSize;
    const uint32_t mMonitorCount;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (GuestOSType)

#endif // ____H_GUESTOSTYPEIMPL
