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

#ifndef ____H_HOSTDVDDRIVEIMPL
#define ____H_HOSTDVDDRIVEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

class ATL_NO_VTABLE HostDVDDrive :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostDVDDrive, IHostDVDDrive>,
    public VirtualBoxSupportTranslation <HostDVDDrive>,
    public IHostDVDDrive
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostDVDDrive)

    DECLARE_NOT_AGGREGATABLE (HostDVDDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostDVDDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostDVDDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostDVDDrive)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (INPTR BSTR aName, INPTR BSTR aUdi = NULL,
                  INPTR BSTR aDescription = NULL);
    void uninit();

    // IHostDVDDrive properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(Udi)) (BSTR *aUdi);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);

    // public methods for internal purposes only

    /* @note Must be called from under the object read lock. */
    const Bstr &name() const { return mName; }

    /* @note Must be called from under the object read lock. */
    const Bstr &udi() const { return mUdi; }

    /* @note Must be called from under the object read lock. */
    const Bstr &description() const { return mDescription; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostDVDDrive"; }

private:

    const Bstr mName;
    const Bstr mDescription;
    const Bstr mUdi;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (HostDVDDrive)

    STDMETHOD(FindByName) (INPTR BSTR aName, IHostDVDDrive **aDrive)
    {
        if (!aName)
            return E_INVALIDARG;
        if (!aDrive)
            return E_POINTER;

        *aDrive = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr n;
            (*it)->COMGETTER(Name) (n.asOutParam());
            if (n == aName)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, HostDVDDriveCollection::tr (
                "The host DVD drive named '%ls' could not be found"), aName);

        return found.queryInterfaceTo (aDrive);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (HostDVDDrive)

#endif // ____H_HOSTDVDDRIVEIMPL
