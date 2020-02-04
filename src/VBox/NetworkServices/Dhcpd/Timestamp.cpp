/* $Id$ */
/** @file
 * DHCP server - timestamps
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "DhcpdInternal.h"
#include "Timestamp.h"


size_t Timestamp::strFormatHelper(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput) const RT_NOEXCEPT
{
    RTTIMESPEC TimeSpec;
    RTTIME     Time;
    char       szBuf[64];
    ssize_t    cchBuf = RTTimeToStringEx(RTTimeExplode(&Time, getAbsTimeSpec(&TimeSpec)), szBuf, sizeof(szBuf), 0);
    Assert(cchBuf > 0);
    return pfnOutput(pvArgOutput, szBuf, cchBuf);
}

