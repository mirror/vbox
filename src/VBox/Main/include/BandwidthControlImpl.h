/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_BANDWIDTHCONTROLIMPL
#define ____H_BANDWIDTHCONTROLIMPL

#include "VirtualBoxBase.h"

class BandwidthGroup;

namespace settings
{
    struct IoSettings;
}

class ATL_NO_VTABLE BandwidthControl :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IBandwidthControl)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(BandwidthControl, IBandwidthControl)

    DECLARE_NOT_AGGREGATABLE(BandwidthControl)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(BandwidthControl)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IBandwidthControl)
    END_COM_MAP()

    BandwidthControl() { };
    ~BandwidthControl() { };

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, BandwidthControl *aThat);
    HRESULT initCopy(Machine *aParent, BandwidthControl *aThat);
    void uninit();

    STDMETHOD(CreateBandwidthGroup) (IN_BSTR aName, BandwidthGroupType_T aType, LONG64 aMaxBytesPerSec);
    STDMETHOD(DeleteBandwidthGroup) (IN_BSTR aName);
    STDMETHOD(COMGETTER(NumGroups)) (ULONG *aGroups);
    STDMETHOD(GetBandwidthGroup) (IN_BSTR aName, IBandwidthGroup **aBandwidthGroup);
    STDMETHOD(GetAllBandwidthGroups) (ComSafeArrayOut(IBandwidthGroup *, aBandwidthGroups));

    HRESULT FinalConstruct();
    void FinalRelease();

    // public internal methods

    HRESULT loadSettings(const settings::IoSettings &data);
    HRESULT saveSettings(settings::IoSettings &data);

    void rollback();
    void commit();
    void copyFrom (BandwidthControl *aThat);

    Machine *getMachine() const;

    HRESULT getBandwidthGroupByName(const Utf8Str &aName,
                                    ComObjPtr<BandwidthGroup> &aBandwidthGroup,
                                    bool aSetError /* = false */);

private:
    struct Data;
    Data *m;
};

#endif // ____H_BANDWIDTHCONTROLIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
