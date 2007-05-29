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

#ifndef ____H_HOSTDVDDRIVEIMPL
#define ____H_HOSTDVDDRIVEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

class ATL_NO_VTABLE HostDVDDrive :
    public VirtualBoxSupportErrorInfoImpl <HostDVDDrive, IHostDVDDrive>,
    public VirtualBoxSupportTranslation <HostDVDDrive>,
    public VirtualBoxBase,
    public IHostDVDDrive
{
public:

    HostDVDDrive();
    virtual ~HostDVDDrive();

    DECLARE_NOT_AGGREGATABLE(HostDVDDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostDVDDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostDVDDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    // public initializer/uninitializer for internal purposes only
    HRESULT init (INPTR BSTR driveName);
    HRESULT init (INPTR BSTR driveName, INPTR BSTR driveDescription);

    // IHostDVDDrive properties
    STDMETHOD(COMGETTER(Name)) (BSTR *driveName);
    STDMETHOD(COMGETTER(Description)) (BSTR *driveDescription);

    // public methods for internal purposes only

    const Bstr &driveName() const { return mDriveName; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostDVDDrive"; }

private:

    Bstr mDriveName;
    Bstr mDriveDescription;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (HostDVDDrive)

    STDMETHOD(FindByName) (INPTR BSTR name, IHostDVDDrive **drive)
    {
        if (!name)
            return E_INVALIDARG;
        if (!drive)
            return E_POINTER;

        *drive = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr n;
            (*it)->COMGETTER(Name) (n.asOutParam());
            if (n == name)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, HostDVDDriveCollection::tr (
                "The host DVD drive named '%ls' could not be found"), name);

        return found.queryInterfaceTo (drive);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (HostDVDDrive)

#endif // ____H_HOSTDVDDRIVEIMPL
