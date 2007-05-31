/* $Id $ */
/** @file
 * InnoTek Portable Runtime - Convert Darwin COM returns codes to iprt status codes.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <IOKit/IOCFPlugIn.h>

#include <iprt/err.h>
#include <iprt/assert.h>

RTDECL(int) RTErrConvertFromDarwinCOM(int32_t iNativeCode)
{
    /*
     * 'optimzied' success case.
     */
    if (iNativeCode == S_OK)
        return VINF_SUCCESS;

    switch (iNativeCode)
    {
        //case E_UNEXPECTED:
        case E_NOTIMPL:             return VERR_NOT_IMPLEMENTED;
        case E_OUTOFMEMORY:         return VERR_NO_MEMORY;
        case E_INVALIDARG:          return VERR_INVALID_PARAMETER;
        //case E_NOINTERFACE:         return VERR_NOT_SUPPORTED;
        case E_POINTER:             return VERR_INVALID_POINTER;
        case E_HANDLE:              return VERR_INVALID_HANDLE;
        //case E_ABORT:               return VERR_CANCELLED;
        case E_FAIL:                return VERR_GENERAL_FAILURE;
        case E_ACCESSDENIED:        return VERR_ACCESS_DENIED;
    }

    /* unknown error. */
    AssertMsgFailed(("Unhandled error %#x\n", iNativeCode));
    return VERR_UNRESOLVED_ERROR;
}


