/* $Id$ */
/** @file
 * IPRT Testcase - RTSystemQuerFirmware*.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <iprt/system.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>
#include <iprt/test.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTSystemQueryFirmware", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTSYSFWTYPE fwType;
    rc = RTSystemFirmwareQueryType(&fwType);
    if (RT_FAILURE(rc))
        RTTestIFailed("Querying firmware type failed, rc=%Rrc\n", rc);

    switch (fwType)
    {
        case RTSYSFWTYPE_BIOS:
            RTTestPrintf(hTest, RTTESTLVL_INFO, "Firmware type: BIOS (Legacy)\n");
            break;
        case RTSYSFWTYPE_UEFI:
            RTTestPrintf(hTest, RTTESTLVL_INFO, "Firmware type: UEFI\n");
            break;
        case RTSYSFWTYPE_UNKNOWN: /* Do not fail on not-implemented platforms. */
            RT_FALL_THROUGH();
        default:
            RTTestPrintf(hTest, RTTESTLVL_INFO, "Unknown firmware type\n");
            break;
    }

    PRTSYSFWVALUE pValue;
    rc = RTSystemFirmwareValueQuery(RTSYSFWPROP_SECURE_BOOT, &pValue);
    if (RT_SUCCESS(rc))
    {
        RTTestPrintf(hTest, RTTESTLVL_INFO, "Secure Boot enabled: %RTbool\n", pValue->u.fVal);

        RTSystemFirmwareValueFree(pValue);
    }
    else if (rc != VERR_NOT_SUPPORTED)
        RTTestIFailed("Querying secure boot failed, rc=%Rrc\n", rc);

    return RTTestSummaryAndDestroy(hTest);
}

