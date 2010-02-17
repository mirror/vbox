/* $Id$ */
/** @file
 * IPRT Testcase - RTSystemQueryDmi*.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/test.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTSystemQueryDmi", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Simple stuff.
     */
    char szInfo[256];

    rc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_NAME, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "PRODUCT_NAME: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_VERSION, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "PRODUCT_VERSION: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_UUID, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "PRODUCT_UUID: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_SERIAL, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "PRODUCT_SERIAL: \"%s\", rc=%Rrc\n", szInfo, rc);

    /*
     * Check that unsupported stuff is terminated correctly.
     */
    for (int i = RTSYSDMISTR_INVALID + 1; i < RTSYSDMISTR_END; i++)
    {
        memset(szInfo, ' ', sizeof(szInfo));
        rc = RTSystemQueryDmiString((RTSYSDMISTR)i, szInfo, sizeof(szInfo));
        if (    rc == VERR_NOT_SUPPORTED
            &&  szInfo[0] != '\0')
            RTTestIFailed("level=%d; unterminated buffer on VERR_NOT_SUPPORTED\n", i);
        else if (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW || rc == VERR_ACCESS_DENIED)
            RTTESTI_CHECK(memchr(szInfo, '\0', sizeof(szInfo)) != NULL);
        else if (rc != VERR_NOT_SUPPORTED)
            RTTestIFailed("level=%d unexpected rc=%Rrc\n", i, rc);
    }

    /*
     * Check buffer overflow
     */
    RTAssertSetQuiet(true);
    RTAssertSetMayPanic(false);
    for (int i = RTSYSDMISTR_INVALID + 1; i < RTSYSDMISTR_END; i++)
    {
        rc = VERR_BUFFER_OVERFLOW;
        for (size_t cch = 0; cch < sizeof(szInfo) && rc == VERR_BUFFER_OVERFLOW; cch++)
        {
            memset(szInfo, 0x7f, sizeof(szInfo));
            rc = RTSystemQueryDmiString((RTSYSDMISTR)i, szInfo, cch);

            /* check the padding. */
            for (size_t off = cch; off < sizeof(szInfo); off++)
                if (szInfo[off] != 0x7f)
                {
                    RTTestIFailed("level=%d, rc=%Rrc, cch=%zu, off=%zu: Wrote too much!\n", i, rc, cch, off);
                    break;
                }

            /* check for zero terminator. */
            if (    (   rc == VERR_BUFFER_OVERFLOW
                     || rc == VERR_NOT_SUPPORTED
                     || RT_SUCCESS(rc))
                &&  cch > 0
                &&  !memchr(szInfo, '\0', cch))
                RTTestIFailed("level=%d, rc=%Rrc, cch=%zu: Buffer not terminated!\n", i, rc, cch);
        }
    }

    return RTTestSummaryAndDestroy(hTest);
}

