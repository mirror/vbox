/* $Id$ */
/** @file
 * IPRT - RTSystemFirmwareTypeName.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/system.h>


RTDECL(const char *) RTSystemFirmwareTypeName(RTSYSFWTYPE enmType)
{
    switch (enmType)
    {
        case RTSYSFWTYPE_INVALID:       return "Invalid";
        case RTSYSFWTYPE_UNKNOWN:       return "Unknown";
        case RTSYSFWTYPE_BIOS:          return "BIOS";
        case RTSYSFWTYPE_UEFI:          return "UEFI";
        case RTSYSFWTYPE_END:
        case RTSYSFWTYPE_32_BIT_HACK:
            break;
    }
    return "bad-firmware-type";
}
RT_EXPORT_SYMBOL(RTSystemFirmwareTypeName);

