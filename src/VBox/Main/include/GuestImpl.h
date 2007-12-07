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

#ifndef ____H_GUESTIMPL
#define ____H_GUESTIMPL

#include "VirtualBoxBase.h"

class Console;

#define GUEST_STAT_INVALID          (ULONG)-1

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
    STDMETHOD(COMGETTER(SupportsSeamless)) (BOOL *aSupportsSeamless);
    STDMETHOD(COMGETTER(MemoryBalloonSize)) (ULONG *aMemoryBalloonSize);
    STDMETHOD(COMSETTER(MemoryBalloonSize)) (ULONG aMemoryBalloonSize);
    STDMETHOD(COMGETTER(StatisticsUpdateInterval)) (ULONG *aUpdateInterval);
    STDMETHOD(COMSETTER(StatisticsUpdateInterval)) (ULONG aUpdateInterval);

    // IGuest methods
    STDMETHOD(SetCredentials)(INPTR BSTR aUserName, INPTR BSTR aPassword,
                              INPTR BSTR aDomain, BOOL aAllowInteractiveLogon);
    STDMETHOD(GetStatistic)(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG *aStatVal);

    // public methods that are not in IDL
    void setAdditionsVersion (Bstr aVersion);

    void setSupportsSeamless (BOOL aSupportsSeamless);

    STDMETHOD(SetStatistic)(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG aStatVal);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Guest"; }

private:

    struct Data
    {
        Data() : mAdditionsActive (FALSE), mSupportsSeamless (FALSE) {}

        Bstr  mOSTypeId;
        BOOL  mAdditionsActive;
        Bstr  mAdditionsVersion;
        BOOL  mSupportsSeamless;
    };

    ULONG mMemoryBalloonSize;
    ULONG mStatUpdateInterval;

    ULONG mCurrentGuestStat[GuestStatisticType_MaxVal];

    ComObjPtr <Console, ComWeakRef> mParent;
    Data mData;
};

#endif // ____H_GUESTIMPL
