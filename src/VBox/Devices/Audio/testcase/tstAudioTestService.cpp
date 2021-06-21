/* $Id$ */
/** @file
 * Audio testcase - Tests for the Audio Test Service (ATS).
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include "../AudioTestService.h"
#include "../AudioTestServiceClient.h"


static size_t g_cToRead = _1M;
static size_t g_cbRead  = 0;


/** @copydoc ATSCALLBACKS::pfnTestSetSendRead */
static DECLCALLBACK(int) tstTestSetSendReadCallback(void const *pvUser,
                                                    const char *pszTag, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    RT_NOREF(pvUser, pszTag);

    size_t cbToRead = RT_MIN(g_cToRead - g_cbRead, cbBuf);
    if (cbToRead)
    {
        memset(pvBuf, 0x42, cbToRead);
        g_cbRead += cbToRead;
    }

    *pcbRead = cbToRead;

    return VINF_SUCCESS;
}

int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstAudioTestService", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    ATSCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnTestSetSendRead  = tstTestSetSendReadCallback;

    ATSCLIENT Client;

    ATSSERVER Srv;
    rc = AudioTestSvcInit(&Srv, "127.0.0.1", ATS_TCP_HOST_DEFAULT_PORT, &Callbacks);
    RTTEST_CHECK_RC_OK(hTest, rc);
    if (RT_SUCCESS(rc))
    {
        rc = AudioTestSvcStart(&Srv);
        RTTEST_CHECK_RC_OK(hTest, rc);
        if (RT_SUCCESS(rc))
        {
            rc = AudioTestSvcClientConnect(&Client, "127.0.0.1", ATS_TCP_HOST_DEFAULT_PORT);
            RTTEST_CHECK_RC_OK(hTest, rc);
        }
    }

    if (RT_SUCCESS(rc))
    {
        char szTemp[RTPATH_MAX];
        rc = RTPathTemp(szTemp, sizeof(szTemp));
        RTTEST_CHECK_RC_OK(hTest, rc);
        if (RT_SUCCESS(rc))
        {
            char szFile[RTPATH_MAX];
            rc = RTPathJoin(szFile, sizeof(szFile), szTemp, "tstAudioTestService");
            RTTEST_CHECK_RC_OK(hTest, rc);
            if (RT_SUCCESS(rc))
            {
                rc = AudioTestSvcClientTestSetDownload(&Client, "ignored", szFile);
                RTTEST_CHECK_RC_OK(hTest, rc);
            }

            rc = RTFileDelete(szFile);
            RTTEST_CHECK_RC_OK(hTest, rc);
        }
    }

    rc = AudioTestSvcClientClose(&Client);
    RTTEST_CHECK_RC_OK(hTest, rc);

    rc = AudioTestSvcShutdown(&Srv);
    RTTEST_CHECK_RC_OK(hTest, rc);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}
