/* $Id$ */
/** @file
 * innotek Portable Runtime - Convert NT status codes to iprt status codes.
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
#include <ntstatus.h>
typedef long NTSTATUS;                  /** @todo figure out which headers to include to get this one typedef... */

#include <iprt/err.h>
#include <iprt/assert.h>



RTDECL(int)  RTErrConvertFromNtStatus(long lNativeCode)
{
    switch (lNativeCode)
    {
        case STATUS_SUCCESS:
            return VINF_SUCCESS;
        case STATUS_ALERTED:
        case STATUS_USER_APC:
            return VERR_INTERRUPTED;
    }

    /* unknown error. */
    AssertMsgFailed(("Unhandled error %ld\n", lNativeCode));
    return VERR_UNRESOLVED_ERROR;
}

