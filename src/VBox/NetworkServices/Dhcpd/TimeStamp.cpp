/* $Id$ */
/** @file
 * DHCP server - timestamps
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "TimeStamp.h"

#include <iprt/string.h>


size_t TimeStamp::absStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput) const
{
    RTTIMESPEC Spec;
    getAbsTimeSpec(&Spec);

    RTTIME Time;
    RTTimeExplode(&Time, &Spec);

    size_t cb = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                            "%RI32-%02u-%02uT%02u:%02u:%02uZ",
                            Time.i32Year, Time.u8Month, Time.u8MonthDay,
                            Time.u8Hour, Time.u8Minute, Time.u8Second);
    return cb;
}
