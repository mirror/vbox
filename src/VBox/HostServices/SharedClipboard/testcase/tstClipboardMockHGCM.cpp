/* $Id$ */
/** @file
 * Shared Clipboard host service test case.
 */

/*
 * Copyright (C) 2011-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "../VBoxSharedClipboardSvc-internal.h"

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/VBoxGuestLib.h>
#ifdef RT_OS_LINUX
# include <VBox/GuestHost/SharedClipboard-x11.h>
#endif

#include <VBox/GuestHost/HGCMMock.h>

#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Static globals                                                                                                               *
*********************************************************************************************************************************/
static RTTEST     g_hTest;
static SHCLCLIENT g_Client;


/*********************************************************************************************************************************
*   Shared Clipboard testing                                                                                                     *
*********************************************************************************************************************************/
struct TESTDESC;
/** Pointer to a test description. */
typedef TESTDESC *PTESTDESC;

struct TESTPARMS;
/** Pointer to a test parameter structure. */
typedef TESTPARMS *PTESTPARMS;

struct TESTCTX;
/** Pointer to a test context. */
typedef TESTCTX *PTESTCTX;

/** Pointer a test descriptor. */
typedef TESTDESC *PTESTDESC;

typedef DECLCALLBACKTYPE(int, FNTESTSETUP,(PTESTPARMS pTstParms, void **ppvCtx));
/** Pointer to an test setup callback. */
typedef FNTESTSETUP *PFNTESTSETUP;

typedef DECLCALLBACKTYPE(int, FNTESTEXEC,(PTESTPARMS pTstParms, void *pvCtx));
/** Pointer to an test exec callback. */
typedef FNTESTEXEC *PFNTESTEXEC;

typedef DECLCALLBACKTYPE(int, FNTESTGSTTHREAD,(PTESTCTX pCtx, void *pvCtx));
/** Pointer to an test guest thread callback. */
typedef FNTESTGSTTHREAD *PFNTESTGSTTHREAD;

typedef DECLCALLBACKTYPE(int, FNTESTDESTROY,(PTESTPARMS pTstParms, void *pvCtx));
/** Pointer to an test destroy callback. */
typedef FNTESTDESTROY *PFNTESTDESTROY;

typedef struct TESTTASK
{
    RTSEMEVENT  hEvent;
    int         rcCompleted;
    int         rcExpected;
    SHCLFORMATS enmFmtHst;
    SHCLFORMATS enmFmtGst;
    /** For chunked reads / writes. */
    size_t      cbChunk;
    size_t      cbData;
    void       *pvData;
} TESTTASK;
typedef TESTTASK *PTESTTASK;

/**
 * Structure for keeping a test context.
 */
typedef struct TESTCTX
{
    PTSTHGCMMOCKSVC      pSvc;
    /** Currently we only support one task at a time. */
    TESTTASK             Task;
    struct
    {
        RTTHREAD         hThread;
        VBGLR3SHCLCMDCTX CmdCtx;
        volatile bool    fShutdown;
        PFNTESTGSTTHREAD pfnThread;
    } Guest;
    struct
    {
        RTTHREAD         hThread;
        volatile bool    fShutdown;
    } Host;
} TESTCTX;

/** The one and only test context. */
TESTCTX  g_TstCtx;

/**
 * Test parameters.
 */
typedef struct TESTPARMS
{
    /** Pointer to test context to use. */
    PTESTCTX             pTstCtx;
} TESTPARMS;

typedef struct TESTDESC
{
    /** The setup callback. */
    PFNTESTSETUP         pfnSetup;
    /** The exec callback. */
    PFNTESTEXEC          pfnExec;
    /** The destruction callback. */
    PFNTESTDESTROY       pfnDestroy;
} TESTDESC;

typedef struct SHCLCONTEXT
{
} SHCLCONTEXT;


#if 0
static void tstBackendWriteData(HGCMCLIENTID idClient, SHCLFORMAT uFormat, void *pvData, size_t cbData)
{
    ShClBackendSetClipboardData(&s_tstHgcmClient[idClient].Client, uFormat, pvData, cbData);
}

/** Adds a host data read request message to the client's message queue. */
static int tstSvcMockRequestDataFromGuest(uint32_t idClient, SHCLFORMATS fFormats, PSHCLEVENT *ppEvent)
{
    AssertPtrReturn(ppEvent, VERR_INVALID_POINTER);

    int rc = ShClSvcGuestDataRequest(&s_tstHgcmClient[idClient].Client, fFormats, ppEvent);
    RTTESTI_CHECK_RC_OK_RET(rc, rc);

    return rc;
}
#endif

static int tstSetModeRc(PTSTHGCMMOCKSVC pSvc, uint32_t uMode, int rc)
{
    VBOXHGCMSVCPARM aParms[2];
    HGCMSvcSetU32(&aParms[0], uMode);
    int rc2 = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, aParms);
    RTTESTI_CHECK_MSG_RET(rc == rc2, ("Expected %Rrc, got %Rrc\n", rc, rc2), rc2);
    uint32_t const uModeRet = ShClSvcGetMode();
    RTTESTI_CHECK_MSG_RET(uMode == uModeRet, ("Expected mode %RU32, got %RU32\n", uMode, uModeRet), VERR_WRONG_TYPE);
    return rc2;
}

static int tstSetMode(PTSTHGCMMOCKSVC pSvc, uint32_t uMode)
{
    return tstSetModeRc(pSvc, uMode, VINF_SUCCESS);
}

static bool tstGetMode(PTSTHGCMMOCKSVC pSvc, uint32_t uModeExpected)
{
    RT_NOREF(pSvc);
    RTTESTI_CHECK_RET(ShClSvcGetMode() == uModeExpected, false);
    return true;
}

static void tstOperationModes(void)
{
    struct VBOXHGCMSVCPARM parms[2];
    uint32_t u32Mode;
    int rc;

    RTTestISub("Testing VBOX_SHCL_HOST_FN_SET_MODE");

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    /* Reset global variable which doesn't reset itself. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_MODE_OFF);
    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    u32Mode = ShClSvcGetMode();
    RTTESTI_CHECK_MSG(u32Mode == VBOX_SHCL_MODE_OFF, ("u32Mode=%u\n", (unsigned) u32Mode));

    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    HGCMSvcSetU64(&parms[0], 99);
    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    tstSetMode(pSvc, VBOX_SHCL_MODE_HOST_TO_GUEST);
    tstSetModeRc(pSvc, 99, VERR_NOT_SUPPORTED);
    tstGetMode(pSvc, VBOX_SHCL_MODE_OFF);
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
static void testSetTransferMode(void)
{
    RTTestISub("Testing VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE");

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    /* Invalid parameter. */
    VBOXHGCMSVCPARM parms[2];
    HGCMSvcSetU64(&parms[0], 99);
    int rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    /* Invalid mode. */
    HGCMSvcSetU32(&parms[0], 99);
    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_FLAGS);

    /* Enable transfers. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_TRANSFER_MODE_ENABLED);
    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

    /* Disable transfers again. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_TRANSFER_MODE_DISABLED);
    rc = TstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/* Does testing of VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, needed for providing compatibility to older Guest Additions clients. */
static void testHostGetMsgOld(void)
{
    RTTestISub("Setting up VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT test");

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    VBOXHGCMSVCPARM parms[2];
    RT_ZERO(parms);

    /* Unless we are bidirectional the host message requests will be dropped. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_MODE_BIDIRECTIONAL);
    int rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);

    RTTestISub("Testing one format, waiting guest u.Call.");
    RT_ZERO(g_Client);
    VBOXHGCMCALLHANDLE_TYPEDEF call;
    rc = VERR_IPE_UNINITIALIZED_STATUS;
    pSvc->fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);

    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    pSvc->fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC_OK(rc);

    //testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
#if 0
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VINF_SUCCESS);
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing one format, no waiting guest calls.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VINF_SUCCESS);
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, waiting guest u.Call.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This should get updated only when the guest call completes. */
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, no waiting guest calls.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
#endif
    pSvc->fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);
}

static void testGuestSimple(void)
{
    RTTestISub("Testing client (guest) API - Simple");

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    /* Preparations. */
    VBGLR3SHCLCMDCTX Ctx;
    RT_ZERO(Ctx);

    /*
     * Multiple connects / disconnects.
     */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
    /* Report bogus guest features while connecting. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, 0xdeadbeef));
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));

    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));

    /*
     * Feature tests.
     */

    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFeatures(Ctx.idClient, 0x0,        NULL /* pfHostFeatures */));
    /* Report bogus features to the host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFeatures(Ctx.idClient, 0xdeadb33f, NULL /* pfHostFeatures */));

    /*
     * Access denied tests.
     */

    /* Try reading data from host. */
    uint8_t abData[32]; uint32_t cbIgnored;
    RTTESTI_CHECK_RC(VbglR3ClipboardReadData(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT,
                                             abData, sizeof(abData), &cbIgnored), VERR_ACCESS_DENIED);
    /* Try writing data without reporting formats before (legacy). */
    RTTESTI_CHECK_RC(VbglR3ClipboardWriteData(Ctx.idClient, 0xdeadb33f, abData, sizeof(abData)), VERR_ACCESS_DENIED);
    /* Try writing data without reporting formats before. */
    RTTESTI_CHECK_RC(VbglR3ClipboardWriteDataEx(&Ctx, 0xdeadb33f, abData, sizeof(abData)), VERR_ACCESS_DENIED);
    /* Report bogus formats to the host. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReportFormats(Ctx.idClient, 0xdeadb33f), VERR_ACCESS_DENIED);
    /* Report supported formats to host. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReportFormats(Ctx.idClient,
                                                  VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_BITMAP | VBOX_SHCL_FMT_HTML),
                                                  VERR_ACCESS_DENIED);
    /*
     * Access allowed tests.
     */
    tstSetMode(pSvc, VBOX_SHCL_MODE_BIDIRECTIONAL);

    /* Try writing data without reporting formats before. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardWriteDataEx(&Ctx, 0xdeadb33f, abData, sizeof(abData)));
    /* Try reading data from host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReadData(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT,
                                                abData, sizeof(abData), &cbIgnored));
    /* Report bogus formats to the host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient, 0xdeadb33f));
    /* Report supported formats to host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient,
                                                     VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_BITMAP | VBOX_SHCL_FMT_HTML));
    /* Tear down. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
}

static void testGuestWrite(void)
{
    RTTestISub("Testing client (guest) API - Writing");
}

#if 0
/**
 * Generate a random codepoint for simple UTF-16 encoding.
 */
static RTUTF16 tstGetRandUtf16(void)
{
    RTUTF16 wc;
    do
    {
        wc = (RTUTF16)RTRandU32Ex(1, 0xfffd);
    } while (wc >= 0xd800 && wc <= 0xdfff);
    return wc;
}

static PRTUTF16 tstGenerateUtf16StringA(uint32_t uCch)
{
    PRTUTF16 pwszRand = (PRTUTF16)RTMemAlloc((uCch + 1) * sizeof(RTUTF16));
    for (uint32_t i = 0; i < uCch; i++)
        pwszRand[i] = tstGetRandUtf16();
    pwszRand[uCch] = 0;
    return pwszRand;
}
#endif

#if 0
static void testGuestRead(void)
{
    RTTestISub("Testing client (guest) API - Reading");

    /* Preparations. */
    tstSetMode(VBOX_SHCL_MODE_BIDIRECTIONAL);

    VBGLR3SHCLCMDCTX Ctx;
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));
    RTThreadSleep(500); /** @todo BUGBUG -- Seems to be a startup race when querying the initial clipboard formats. */

    uint8_t abData[_4K]; uint32_t cbData; uint32_t cbRead;

    /* Issue a host request that we want to read clipboard data from the guest. */
    PSHCLEVENT pEvent;
    tstSvcMockRequestDataFromGuest(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT, &pEvent);

    /* Write guest clipboard data to the host side. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT));
    cbData = RTRandU32Ex(1, sizeof(abData));
    PRTUTF16 pwszStr = tstGenerateUtf16String(cbData);
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardWriteDataEx(&Ctx, VBOX_SHCL_FMT_UNICODETEXT, pwszStr, cbData));
    RTMemFree(pwszStr);

    PSHCLEVENTPAYLOAD pPayload;
    int rc = ShClEventWait(pEvent, RT_MS_30SEC, &pPayload);
    if (RT_SUCCESS(rc))
    {

    }
    ShClEventRelease(pEvent);
    pEvent = NULL;


    /* Read clipboard data from the host back to the guest side. */
    /* Note: Also could return VINF_BUFFER_OVERFLOW, so check for VINF_SUCCESS explicitly here. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReadDataEx(&Ctx, VBOX_SHCL_FMT_UNICODETEXT,
                                               abData, sizeof(abData), &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == cbData);

    RTPrintf("Data (%RU32): %ls\n", cbRead, (PCRTUTF16)abData);

    /* Tear down. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
}
#endif

static DECLCALLBACK(int) tstGuestThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PTESTCTX pCtx = (PTESTCTX)pvUser;
    AssertPtr(pCtx);

    RTThreadUserSignal(hThread);

    if (pCtx->Guest.pfnThread)
        return pCtx->Guest.pfnThread(pCtx, NULL);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstHostThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PTESTCTX pCtx = (PTESTCTX)pvUser;
    AssertPtr(pCtx);

    int rc = VINF_SUCCESS;

    RTThreadUserSignal(hThread);

    for (;;)
    {
        RTThreadSleep(100);

        if (ASMAtomicReadBool(&pCtx->Host.fShutdown))
            break;
    }

    return rc;
}

static void testSetHeadless(void)
{
    RTTestISub("Testing HOST_FN_SET_HEADLESS");

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    VBOXHGCMSVCPARM parms[2];
    HGCMSvcSetU32(&parms[0], false);
    int rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    bool fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == false, ("fHeadless=%RTbool\n", fHeadless));
    rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU64(&parms[0], 99);
    rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU32(&parms[0], true);
    rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
    HGCMSvcSetU32(&parms[0], 99);
    rc = pSvc->fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
}

static void testHostCall(void)
{
    tstOperationModes();
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    testSetTransferMode();
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
    testSetHeadless();
}

static int tstGuestStart(PTESTCTX pTstCtx, PFNTESTGSTTHREAD pFnThread)
{
    pTstCtx->Guest.pfnThread = pFnThread;

    int rc = RTThreadCreate(&pTstCtx->Guest.hThread, tstGuestThread, pTstCtx, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "tstShClGst");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pTstCtx->Guest.hThread, RT_MS_30SEC);

    return rc;
}

static int tstGuestStop(PTESTCTX pTstCtx)
{
    ASMAtomicWriteBool(&pTstCtx->Guest.fShutdown, true);

    int rcThread;
    int rc = RTThreadWait(pTstCtx->Guest.hThread, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
        rc = rcThread;
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Shutting down guest thread failed with %Rrc\n", rc);

    pTstCtx->Guest.hThread = NIL_RTTHREAD;

    return rc;
}

static int tstHostStart(PTESTCTX pTstCtx)
{
    int rc = RTThreadCreate(&pTstCtx->Host.hThread, tstHostThread, pTstCtx, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "tstShClHst");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pTstCtx->Host.hThread, RT_MS_30SEC);

    return rc;
}

static int tstHostStop(PTESTCTX pTstCtx)
{
    ASMAtomicWriteBool(&pTstCtx->Host.fShutdown, true);

    int rcThread;
    int rc = RTThreadWait(pTstCtx->Host.hThread, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
        rc = rcThread;
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Shutting down host thread failed with %Rrc\n", rc);

    pTstCtx->Host.hThread = NIL_RTTHREAD;

    return rc;
}

#if defined(RT_OS_LINUX)
static DECLCALLBACK(int) tstShClUserMockReportFormatsCallback(PSHCLCONTEXT pCtx, uint32_t fFormats, void *pvUser)
{
    RT_NOREF(pCtx, fFormats, pvUser);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "tstShClUserMockReportFormatsCallback: fFormats=%#x\n", fFormats);
    return VINF_SUCCESS;
}

/*
static DECLCALLBACK(int) tstTestReadFromHost_RequestDataFromSourceCallback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb, void *pvUser)
{
    RT_NOREF(pCtx, uFmt, ppv, pvUser);

    PTESTTASK pTask = &TaskRead;

    uint8_t *pvData = (uint8_t *)RTMemDup(pTask->pvData, pTask->cbData);

    *ppv = pvData;
    *pcb = pTask->cbData;

    return VINF_SUCCESS;
}
*/

#if 0
static DECLCALLBACK(int) tstShClUserMockSendDataCallback(PSHCLCONTEXT pCtx, void *pv, uint32_t cb, void *pvUser)
{
    RT_NOREF(pCtx, pv, cb, pvUser);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "tstShClUserMockSendDataCallback\n");

    PTESTTASK pTask = &TaskRead;

    memcpy(pv, pTask->pvData, RT_MIN(pTask->cbData, cb));

    return VINF_SUCCESS;
}
#endif

static DECLCALLBACK(int) tstShClUserMockOnGetDataCallback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, size_t *pcb, void *pvUser)
{
    RT_NOREF(pCtx, uFmt, pvUser);

    PTESTTASK pTask = &g_TstCtx.Task;

    uint8_t *pvData = pTask->cbData ? (uint8_t *)RTMemDup(pTask->pvData, pTask->cbData) : NULL;
    size_t   cbData = pTask->cbData;

    *ppv = pvData;
    *pcb = cbData;

    return VINF_SUCCESS;
}
#endif /* RT_OS_LINUX */

typedef struct TSTUSERMOCK
{
#if defined(RT_OS_LINUX)
    SHCLX11CTX   X11Ctx;
#endif
    PSHCLCONTEXT pCtx;
} TSTUSERMOCK;
typedef TSTUSERMOCK *PTSTUSERMOCK;

static void tstShClUserMockInit(PTSTUSERMOCK pUsrMock, const char *pszName)
{
#if defined(RT_OS_LINUX)
    SHCLCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnReportFormats   = tstShClUserMockReportFormatsCallback;
    Callbacks.pfnOnClipboardRead = tstShClUserMockOnGetDataCallback;

    pUsrMock->pCtx = (PSHCLCONTEXT)RTMemAllocZ(sizeof(SHCLCONTEXT));
    AssertPtrReturnVoid(pUsrMock->pCtx);

    ShClX11Init(&pUsrMock->X11Ctx, &Callbacks, pUsrMock->pCtx, false);
    ShClX11ThreadStartEx(&pUsrMock->X11Ctx, pszName, false /* fGrab */);
    /* Give the clipboard time to synchronise. */
    RTThreadSleep(500);
#else
    RT_NOREF(pUsrMock);
#endif /* RT_OS_LINUX */
}

static void tstShClUserMockDestroy(PTSTUSERMOCK pUsrMock)
{
#if defined(RT_OS_LINUX)
    ShClX11ThreadStop(&pUsrMock->X11Ctx);
    ShClX11Destroy(&pUsrMock->X11Ctx);
    RTMemFree(pUsrMock->pCtx);
#else
    RT_NOREF(pUsrMock);
#endif
}

static int tstTaskGuestRead(PTESTCTX pCtx, PTESTTASK pTask)
{
    size_t   cbReadTotal = 0;
    size_t   cbToRead    = pTask->cbData;

    switch (pTask->enmFmtGst)
    {
        case VBOX_SHCL_FMT_UNICODETEXT:
            cbToRead *= sizeof(RTUTF16);
            break;

        default:
            break;
    }

    size_t   cbDst       = _64K;
    uint8_t *pabDst      = (uint8_t *)RTMemAllocZ(cbDst);
    AssertPtrReturn(pabDst, VERR_NO_MEMORY);

    Assert(pTask->cbChunk);                  /* Buggy test? */
    Assert(pTask->cbChunk <= pTask->cbData); /* Ditto. */

    uint8_t *pabSrc = (uint8_t *)pTask->pvData;

    do
    {
        /* Note! VbglR3ClipboardReadData() currently does not support chunked reads!
          *      It in turn returns VINF_BUFFER_OVERFLOW when the supplied buffer was too small. */
        uint32_t const cbChunk    = cbDst;
        uint32_t const cbExpected = cbToRead;

        uint32_t cbRead = 0;
        RTTEST_CHECK_RC(g_hTest, VbglR3ClipboardReadData(pCtx->Guest.CmdCtx.idClient,
                                                         pTask->enmFmtGst, pabDst, cbChunk, &cbRead), pTask->rcExpected);
        RTTEST_CHECK_MSG(g_hTest, cbRead == cbExpected, (g_hTest, "Read %RU32 bytes, expected %RU32\n", cbRead, cbExpected));
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Guest side received %RU32 bytes\n", cbRead);
        cbReadTotal += cbRead;
        Assert(cbReadTotal <= cbToRead);

    } while (cbReadTotal < cbToRead);

    if (pTask->enmFmtGst == VBOX_SHCL_FMT_UNICODETEXT)
    {
        RTTEST_CHECK_RC_OK(g_hTest, RTUtf16ValidateEncoding((PRTUTF16)pabDst));
    }
    else
        RTTEST_CHECK(g_hTest, memcmp(pabSrc, pabDst, RT_MIN(pTask->cbData, cbDst) == 0));

    RTMemFree(pabDst);

    return VINF_SUCCESS;
}

static void tstTaskInit(PTESTTASK pTask)
{
    RTSemEventCreate(&pTask->hEvent);
}

static void tstTaskDestroy(PTESTTASK pTask)
{
    RTSemEventDestroy(pTask->hEvent);
}

static void tstTaskWait(PTESTTASK pTask, RTMSINTERVAL msTimeout)
{
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventWait(pTask->hEvent, msTimeout));
    RTTEST_CHECK_RC(g_hTest, pTask->rcCompleted, pTask->rcExpected);
}

static void tstTaskSignal(PTESTTASK pTask, int rc)
{
    pTask->rcCompleted = rc;
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventSignal(pTask->hEvent));
}

static DECLCALLBACK(int) tstTestReadFromHostThreadGuest(PTESTCTX pCtx, void *pvCtx)
{
    RT_NOREF(pvCtx);

    RTThreadSleep(5000);
RT_BREAKPOINT();

    RT_ZERO(pCtx->Guest.CmdCtx);
    RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardConnectEx(&pCtx->Guest.CmdCtx, VBOX_SHCL_GF_0_CONTEXT_ID));

#if 1
    PTESTTASK pTask = &pCtx->Task;
    tstTaskGuestRead(pCtx, pTask);
    tstTaskSignal(pTask, VINF_SUCCESS);
#endif

#if 0
    for (;;)
    {
        PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
        AssertPtrBreakStmt(pEvent, rc = VERR_NO_MEMORY);

        uint32_t idMsg  = 0;
        uint32_t cParms = 0;
        RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardMsgPeekWait(&pCtx->Guest.CmdCtx, &idMsg, &cParms, NULL /* pidRestoreCheck */));
        RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardEventGetNext(idMsg, cParms, &pCtx->Guest.CmdCtx, pEvent));

        if (pEvent)
        {
            VbglR3ClipboardEventFree(pEvent);
            pEvent = NULL;
        }

        if (ASMAtomicReadBool(&pCtx->Guest.fShutdown))
            break;

        RTThreadSleep(100);
    }
#endif

    RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardDisconnectEx(&pCtx->Guest.CmdCtx));

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostExec(PTESTPARMS pTstParms, void *pvCtx)
{
    RT_NOREF(pvCtx, pTstParms);

    PTESTTASK pTask = &pTstParms->pTstCtx->Task;

    pTask->enmFmtGst = VBOX_SHCL_FMT_UNICODETEXT;
    pTask->enmFmtHst = pTask->enmFmtGst;
    pTask->pvData    = RTStrAPrintf2("foo!");
    pTask->cbData    = strlen((char *)pTask->pvData) + 1;
    pTask->cbChunk   = pTask->cbData;

    PTSTHGCMMOCKSVC    pSvc        = TstHgcmMockSvcInst();
    PTSTHGCMMOCKCLIENT pMockClient = TstHgcmMockSvcWaitForConnect(pSvc);

    AssertPtrReturn(pMockClient, VERR_INVALID_POINTER);

    bool fUseMock = false;
    TSTUSERMOCK UsrMock;
    if (fUseMock)
        tstShClUserMockInit(&UsrMock, "tstX11Hst");

    RTThreadSleep(RT_MS_1SEC * 4);

#if 1
    PSHCLBACKEND pBackend = ShClSvcGetBackend();

    ShClBackendReportFormats(pBackend, (PSHCLCLIENT)pMockClient->pvClient, pTask->enmFmtHst);
    tstTaskWait(pTask, RT_MS_30SEC);
#endif

RTThreadSleep(RT_MS_30SEC);

    //PSHCLCLIENT pClient = &pMockClient->Client;

#if 1
    if (1)
    {
        //RTTEST_CHECK_RC_OK(g_hTest, ShClBackendMockSetData(pBackend, pTask->enmFmt, pwszStr, cbData));
        //RTMemFree(pwszStr);
    }
#endif

    if (fUseMock)
        tstShClUserMockDestroy(&UsrMock);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostSetup(PTESTPARMS pTstParms, void **ppvCtx)
{
    RT_NOREF(ppvCtx);

    PTESTCTX pCtx = pTstParms->pTstCtx;

    tstHostStart(pCtx);

    PSHCLBACKEND pBackend = ShClSvcGetBackend();

    SHCLCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnReportFormats = tstShClUserMockReportFormatsCallback;
    //Callbacks.pfnOnRequestDataFromSource = tstTestReadFromHost_RequestDataFromSourceCallback;
    Callbacks.pfnOnClipboardRead     = tstShClUserMockOnGetDataCallback;
    ShClBackendSetCallbacks(pBackend, &Callbacks);

    tstGuestStart(pCtx, tstTestReadFromHostThreadGuest);

    RTThreadSleep(1000);

    tstSetMode(pCtx->pSvc, VBOX_SHCL_MODE_BIDIRECTIONAL);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostDestroy(PTESTPARMS pTstParms, void *pvCtx)
{
    RT_NOREF(pvCtx);

    int rc = VINF_SUCCESS;

    tstGuestStop(pTstParms->pTstCtx);
    tstHostStop(pTstParms->pTstCtx);

    return rc;
}

/** Test definition table. */
TESTDESC g_aTests[] =
{
    { tstTestReadFromHostSetup,       tstTestReadFromHostExec,      tstTestReadFromHostDestroy }
};
/** Number of tests defined. */
unsigned g_cTests = RT_ELEMENTS(g_aTests);

static int tstOne(PTSTHGCMMOCKSVC pSvc, PTESTDESC pTstDesc)
{
    PTESTCTX pTstCtx = &g_TstCtx;

    TESTPARMS TstParms;
    RT_ZERO(TstParms);

    pTstCtx->pSvc    = pSvc;
    TstParms.pTstCtx = pTstCtx;

    void *pvCtx;
    int rc = pTstDesc->pfnSetup(&TstParms, &pvCtx);
    if (RT_SUCCESS(rc))
    {
        rc = pTstDesc->pfnExec(&TstParms, pvCtx);

        int rc2 = pTstDesc->pfnDestroy(&TstParms, pvCtx);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
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
    RTEXITCODE rcExit = RTTestInitAndCreate(pcszExecName, &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    /* Don't let assertions in the host service panic (core dump) the test cases. */
    RTAssertSetMayPanic(false);

    PTSTHGCMMOCKSVC pSvc = TstHgcmMockSvcInst();

    TstHgcmMockSvcCreate(pSvc, sizeof(SHCLCLIENT));
    TstHgcmMockSvcStart(pSvc);

    /*
     * Run the tests.
     */
    if (1)
    {
        testGuestSimple();
        testGuestWrite();
        testHostCall();
        testHostGetMsgOld();
    }

    RT_ZERO(g_TstCtx);
    tstTaskInit(&g_TstCtx.Task);
    for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstOne(pSvc, &g_aTests[i]);
    tstTaskDestroy(&g_TstCtx.Task);

    TstHgcmMockSvcStop(pSvc);
    TstHgcmMockSvcDestroy(pSvc);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(g_hTest);
}
