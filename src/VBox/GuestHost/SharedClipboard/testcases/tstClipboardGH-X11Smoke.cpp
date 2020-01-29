/* $Id$ */
/** @file
 * Shared Clipboard guest/host X11 code smoke tests.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* This is a simple test case that just starts a copy of the X11 clipboard
 * backend, checks the X11 clipboard and exits.  If ever needed I will add an
 * interactive mode in which the user can read and copy to the clipboard from
 * the command line. */

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/test.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-x11.h>
#include <VBox/GuestHost/clipboard-helper.h>

DECLCALLBACK(int) ShClX11RequestDataForX11Callback(PSHCLCONTEXT pCtx, SHCLFORMAT Format, void **ppv, uint32_t *pcb)
{
    RT_NOREF(pCtx, Format, ppv, pcb);
    return VERR_NO_DATA;
}

DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, SHCLFORMATS Formats)
{
    RT_NOREF(pCtx, Formats);
}

DECLCALLBACK(void) ShClX11RequestFromX11CompleteCallback(PSHCLCONTEXT pCtx, int rc, CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    RT_NOREF(pCtx, rc, pReq, pv, cb);
}

int main()
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstClipboardX11GH-Smoke", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Run the test.
     */
    rc = VINF_SUCCESS;
    /* We can't test anything without an X session, so just return success
     * in that case. */
    if (!RTEnvExist("DISPLAY"))
    {
        RTTestPrintf(hTest, RTTESTLVL_INFO,
                     "X11 not available, not running test\n");
        return RTTestSummaryAndDestroy(hTest);
    }
    SHCLX11CTX X11Ctx;
    rc = ShClX11Init(&X11Ctx, NULL, false);
    AssertRCReturn(rc, 1);
    rc = ShClX11ThreadStart(&X11Ctx, false /* fGrab */);
    AssertRCReturn(rc, 1);
    /* Give the clipboard time to synchronise. */
    RTThreadSleep(500);
    rc = ShClX11ThreadStop(&X11Ctx);
    AssertRCReturn(rc, 1);
    ShClX11Destroy(&X11Ctx);
    return RTTestSummaryAndDestroy(hTest);
}

