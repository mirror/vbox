/* $Id$ */
/** @file
 * IPRT Testcase - RTSystemQuerFirmware*.
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
#include <iprt/system.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/test.h>


int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTSystemQueryFirmware", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * RTSystemQueryFirmwareType
     */
    RTTestSub(hTest, "RTSystemQueryFirmwareType");
    RTSYSFWTYPE enmType = (RTSYSFWTYPE)-42;
    int rc = RTSystemQueryFirmwareType(&enmType);
    if (RT_SUCCESS(rc))
    {
        switch (enmType)
        {
            case RTSYSFWTYPE_BIOS:
            case RTSYSFWTYPE_UEFI:
            case RTSYSFWTYPE_UNKNOWN: /* Do not fail on not-implemented platforms. */
                RTTestPrintf(hTest, RTTESTLVL_INFO, "  Firmware type: %s\n", RTSystemFirmwareTypeName(enmType));
                break;
            default:
                RTTestFailed(hTest, "RTSystemQueryFirmwareType return invalid type: %d (%#x)", enmType, enmType);
                break;
        }
    }
    else if (rc != VERR_NOT_SUPPORTED)
        RTTestFailed(hTest, "RTSystemQueryFirmwareType failed: %Rrc", rc);

    /*
     * RTSystemQueryFirmwareBoolean
     */
    RTTestSub(hTest, "RTSystemQueryFirmwareBoolean");
    bool fValue;
    rc = RTSystemQueryFirmwareBoolean(RTSYSFWBOOL_SECURE_BOOT, &fValue);
    if (RT_SUCCESS(rc))
        RTTestPrintf(hTest, RTTESTLVL_INFO, "  Secure Boot:   %s\n", fValue ? "enabled" : "disabled");
    else if (rc != VERR_NOT_SUPPORTED && rc != VERR_SYS_UNSUPPORTED_FIRMWARE_PROPERTY)
        RTTestIFailed("RTSystemQueryFirmwareBoolean/RTSYSFWBOOL_SECURE_BOOT failed: %Rrc", rc);

    return RTTestSummaryAndDestroy(hTest);
}

