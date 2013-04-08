
/* $Id$ */
/** @file
 * VirtualBox Main - Guest error information.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "GuestErrorInfoImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestErrorInfo)

HRESULT GuestErrorInfo::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestErrorInfo::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestErrorInfo::init(LONG uResult, const Utf8Str &strText)
{
    mData.mResult = uResult;
    mData.mText = strText;

    return VINF_SUCCESS;
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestErrorInfo::COMGETTER(Result)(LONG *aResult)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aResult);

    *aResult = mData.mResult;
    return S_OK;
}

STDMETHODIMP GuestErrorInfo::COMGETTER(Text)(BSTR *aText)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aText);

    mData.mText.cloneTo(aText);
    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

