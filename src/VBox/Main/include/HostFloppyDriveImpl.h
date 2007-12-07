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

#ifndef ____H_HOSTFLOPPYDRIVEIMPL
#define ____H_HOSTFLOPPYDRIVEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

class ATL_NO_VTABLE HostFloppyDrive :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostFloppyDrive, IHostFloppyDrive>,
    public VirtualBoxSupportTranslation <HostFloppyDrive>,
    public IHostFloppyDrive
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostFloppyDrive)

    DECLARE_NOT_AGGREGATABLE (HostFloppyDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostFloppyDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostFloppyDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostFloppyDrive)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (INPTR BSTR aName, INPTR BSTR aUdi = NULL,
                  INPTR BSTR aDescription = NULL);
    void uninit();

    // IHostFloppyDrive properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMGETTER(Udi)) (BSTR *aUdi);

    // public methods for internal purposes only

    /* @note Must be called from under the object read lock. */
    const Bstr &name() const { return mName; }

    /* @note Must be called from under the object read lock. */
    const Bstr &udi() const { return mUdi; }

    /* @note Must be called from under the object read lock. */
    const Bstr &description() const { return mDescription; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostFloppyDrive"; }

private:

    const Bstr mName;
    const Bstr mDescription;
    const Bstr mUdi;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (HostFloppyDrive)

    STDMETHOD(FindByName) (INPTR BSTR aName, IHostFloppyDrive **aDrive)
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
            return setError (E_INVALIDARG, HostFloppyDriveCollection::tr (
                "The host floppy drive named '%ls' could not be found"), aName);

        return found.queryInterfaceTo (aDrive);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (HostFloppyDrive)

#endif // ____H_HOSTFLOPPYDRIVEIMPL
