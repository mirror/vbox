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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_GUESTIMPL
#define ____H_GUESTIMPL

#include "VirtualBoxBase.h"

class Console;

class ATL_NO_VTABLE Guest :
    public VirtualBoxSupportErrorInfoImpl <Guest, IGuest>,
    public VirtualBoxSupportTranslation <Guest>,
    public VirtualBoxBase,
    public IGuest
{
public:

    DECLARE_NOT_AGGREGATABLE(Guest)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Guest)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IGuest)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (Guest)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();

    // IGuest properties
    STDMETHOD(COMGETTER(OSTypeId)) (BSTR *aOSTypeId);
    STDMETHOD(COMGETTER(AdditionsActive)) (BOOL *aAdditionsActive);
    STDMETHOD(COMGETTER(AdditionsVersion)) (BSTR *aAdditionsVersion);

    // IGuest methods
    STDMETHOD(SetCredentials)(INPTR BSTR aUserName, INPTR BSTR aPassword,
                              INPTR BSTR aDomain, BOOL aAllowInteractiveLogon);

    // public methods that are not in IDL
    void setAdditionsVersion (Bstr aVersion);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Guest"; }

private:

    struct Data
    {
        Data() : mAdditionsActive (FALSE) {}

        Bstr mOSTypeId;
        BOOL mAdditionsActive;
        Bstr mAdditionsVersion;
    };

    ComObjPtr <Console, ComWeakRef> mParent;
    Data mData;
};

#endif // ____H_GUESTIMPL
