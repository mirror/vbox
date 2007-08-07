/* $Id $ */
/** @file
 * innotek Portable Runtime - Convert Darwin IOKit returns codes to iprt status codes.
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <IOKit/IOReturn.h>

#include <iprt/err.h>
#include <iprt/assert.h>


RTDECL(int) RTErrConvertFromDarwinIO(int iNativeCode)
{
    /*
     * 'optimzied' success case.
     */
    if (iNativeCode == kIOReturnSuccess)
        return VINF_SUCCESS;

    switch (iNativeCode)
    {
        case kIOReturnNoDevice:     return VERR_IO_BAD_UNIT;
    }

    /* unknown error. */
    AssertMsgFailed(("Unhandled error %#x\n", iNativeCode));
    return VERR_UNRESOLVED_ERROR;
}

