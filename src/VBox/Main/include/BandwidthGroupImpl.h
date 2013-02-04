/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_BANDWIDTHGROUPIMPL
#define ____H_BANDWIDTHGROUPIMPL

#include "VirtualBoxBase.h"
#include "BandwidthControlImpl.h"

class ATL_NO_VTABLE BandwidthGroup :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IBandwidthGroup)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(BandwidthGroup, IBandwidthGroup)

    DECLARE_NOT_AGGREGATABLE(BandwidthGroup)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(BandwidthGroup)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IBandwidthGroup)
    END_COM_MAP()

    BandwidthGroup() { };
    ~BandwidthGroup() { };

    // public initializer/uninitializer for internal purposes only
    HRESULT init(BandwidthControl *aParent,
                 const Utf8Str &aName,
                 BandwidthGroupType_T aType,
                 LONG64 aMaxBytesPerSec);
    HRESULT init(BandwidthControl *aParent, BandwidthGroup *aThat, bool aReshare = false);
    HRESULT initCopy(BandwidthControl *aParent, BandwidthGroup *aThat);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Type))(BandwidthGroupType_T *aType);
    STDMETHOD(COMGETTER(Reference))(ULONG *aReferences);
    STDMETHOD(COMGETTER(MaxBytesPerSec))(LONG64 *aMaxBytesPerSec);
    STDMETHOD(COMSETTER(MaxBytesPerSec))(LONG64 aMaxBytesPerSec);

    // public methods only for internal purposes
    void rollback();
    void commit();
    void unshare();

    const Utf8Str &getName() const;
    BandwidthGroupType_T getType() const;
    LONG64 getMaxBytesPerSec() const;
    ULONG getReferences() const;

    void reference();
    void release();

    ComObjPtr<BandwidthGroup> getPeer();

private:
    struct Data;
    Data *m;
};

#endif // ____H_BANDWIDTHGROUPIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
