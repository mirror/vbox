/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_ADDITIONSFACILITYIMPL
#define ____H_ADDITIONSFACILITYIMPL

#include <vector>
#include <iprt/time.h>

#include "VirtualBoxBase.h"

class Guest;

class ATL_NO_VTABLE AdditionsFacility :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IAdditionsFacility)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(AdditionsFacility, IAdditionsFacility)

    DECLARE_NOT_AGGREGATABLE(AdditionsFacility)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(AdditionsFacility)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IAdditionsFacility)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(AdditionsFacility)

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Guest *a_pParent, AdditionsFacilityType_T a_enmFacility, AdditionsFacilityStatus_T a_enmStatus,
                 uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    // IAdditionsFacility properties
    STDMETHOD(COMGETTER(ClassType))(AdditionsFacilityClass_T *aClass);
    STDMETHOD(COMGETTER(LastUpdated))(LONG64 *aTimestamp);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Status))(AdditionsFacilityStatus_T *aStatus);
    STDMETHOD(COMGETTER(Type))(AdditionsFacilityType_T *aType);

public:
    /** Facility <-> string mappings. */
    struct FacilityInfo
    {
        /** The facilitie's name. */
        const char              *mName; /* utf-8 */
        /** The facilitie's type. */
        AdditionsFacilityType_T  mType;
        /** The facilitie's class. */
        AdditionsFacilityClass_T mClass;
    };
    static const FacilityInfo s_aFacilityInfo[8];

    // public internal methods
    static const AdditionsFacility::FacilityInfo &typeToInfo(AdditionsFacilityType_T aType);
    AdditionsFacilityClass_T getClass() const;
    LONG64 getLastUpdated() const;
    Bstr getName() const;
    AdditionsFacilityStatus_T getStatus() const;
    AdditionsFacilityType_T getType() const;
    void update(AdditionsFacilityStatus_T a_enmStatus, uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);

private:
    /** A structure for keeping a facility status
     *  set at a certain time. Good for book-keeping. */
    struct FacilityState
    {
        RTTIMESPEC                mTimestamp;
        /** The facilitie's current status. */
        AdditionsFacilityStatus_T mStatus;
    };

    struct Data
    {
        /** Record of current and previous facility
         *  states, limited to the 10 last states set.
         *  Note: This intentionally only is kept in
         *        Main so far! */
        std::vector<FacilityState> mStates;
        /** The facilitie's ID/type. */
        AdditionsFacilityType_T    mType;
    } mData;
};

#endif // ____H_ADDITIONSFACILITYIMPL

