/* $Id$ */
/** @file
 * Shared Clipboard host service implementation (backend) test case.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "../VBoxSharedClipboardSvc-internal.h"

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/test.h>

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable);

static SHCLCLIENT g_Client;
static VBOXHGCMSVCHELPERS g_Helpers = { NULL };

/** Simple call handle structure for the guest call completion callback */
struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code */
    int32_t rc;
};

/** Call completion callback for guest calls. */
static DECLCALLBACK(int) callComplete(VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    callHandle->rc = rc;
    return VINF_SUCCESS;
}

static int setupTable(VBOXHGCMSVCFNTABLE *pTable)
{
    pTable->cbSize = sizeof(*pTable);
    pTable->u32Version = VBOX_HGCM_SVC_VERSION;
    g_Helpers.pfnCallComplete = callComplete;
    pTable->pHelpers = &g_Helpers;
    return VBoxHGCMSvcLoad(pTable);
}

int ShClBackendInit(void) { return VINF_SUCCESS; }
void ShClBackendDestroy(void) { }
int ShClBackendDisconnect(PSHCLCLIENT) { return VINF_SUCCESS; }
int ShClBackendConnect(PSHCLCLIENT, bool) { return VINF_SUCCESS; }
int ShClBackendFormatAnnounce(PSHCLCLIENT, SHCLFORMATS) { AssertFailed(); return VINF_SUCCESS; }
int ShClBackendReadData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT, void *, uint32_t, unsigned int *) { AssertFailed(); return VERR_WRONG_ORDER; }
int ShClBackendWriteData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT, void *, uint32_t) { AssertFailed(); return VINF_SUCCESS; }
int ShClBackendSync(PSHCLCLIENT) { return VINF_SUCCESS; }

static void testAnnounceAndReadData(void)
{
    struct VBOXHGCMSVCPARM parms[2];
    VBOXHGCMSVCFNTABLE table;
    int rc;

    RTTestISub("Setting up client ...");
    rc = setupTable(&table);
    RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("rc=%Rrc\n", rc));
    /* Unless we are bidirectional the host message requests will be dropped. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_MODE_BIDIRECTIONAL);
    rc = table.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    rc = shClSvcClientInit(&g_Client, 1 /* clientId */);
    RTTESTI_CHECK_RC_OK(rc);
}

int main(int argc, char *argv[])
{
    /*
     * Init the runtime, test and say hello.
     */
    const char *pcszExecName;
    NOREF(argc);
    pcszExecName = strrchr(argv[0], '/');
    pcszExecName = pcszExecName ? pcszExecName + 1 : argv[0];
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate(pcszExecName, &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /* Don't let assertions in the host service panic (core dump) the test cases. */
    RTAssertSetMayPanic(false);

    /*
     * Run the tests.
     */
    testAnnounceAndReadData();

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

