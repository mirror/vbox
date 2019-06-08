/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2014-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_AdditionsFacilityImpl_h
#define MAIN_INCLUDED_AdditionsFacilityImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <vector>
#include <iprt/time.h>

#include "AdditionsFacilityWrap.h"

class Guest;

class ATL_NO_VTABLE AdditionsFacility :
    public AdditionsFacilityWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(AdditionsFacility)

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Guest *a_pParent, AdditionsFacilityType_T a_enmFacility, AdditionsFacilityStatus_T a_enmStatus,
                 uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();


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

    /** @name public internal methods
     * @{ */
    static const AdditionsFacility::FacilityInfo &i_typeToInfo(AdditionsFacilityType_T aType);
    LONG64 i_getLastUpdated() const;
#if 0 /* unused */
    AdditionsFacilityType_T i_getType() const;
    AdditionsFacilityClass_T i_getClass() const;
    const char *i_getName() const;
#endif
    AdditionsFacilityStatus_T i_getStatus() const;
    bool i_update(AdditionsFacilityStatus_T a_enmStatus, uint32_t a_fFlags, PCRTTIMESPEC a_pTimeSpecTS);
    /** @} */

private:

    /** @name Wrapped IAdditionsFacility properties
     * @{ */
    HRESULT getClassType(AdditionsFacilityClass_T *aClassType);
    HRESULT getLastUpdated(LONG64 *aLastUpdated);
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getStatus(AdditionsFacilityStatus_T *aStatus);
    HRESULT getType(AdditionsFacilityType_T *aType);
    /** @} */

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

#endif /* !MAIN_INCLUDED_AdditionsFacilityImpl_h */

