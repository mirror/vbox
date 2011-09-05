/* $Id$ */
/** @file
 * VBoxNetNAT - NAT Service for connecting to IntNet.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_net_nat       VBoxNetNAT
 *
 * Write a few words...
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/net.h>
#include <iprt/initterm.h>
#include <iprt/alloca.h>
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/getopt.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/req.h>
#include <iprt/file.h>
#include <iprt/semaphore.h>
#define LOG_GROUP LOG_GROUP_NAT_SERVICE
#include <VBox/log.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/pdmnetinline.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>

#include <vector>
#include <string>

#include "../NetLib/VBoxNetLib.h"
#include "../NetLib/VBoxNetBaseService.h"
#include <libslirp.h>

#ifdef RT_OS_WINDOWS /* WinMain */
# include <Windows.h>
# include <stdlib.h>
#else
# include <errno.h>
#endif



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

class VBoxNetNAT : public VBoxNetBaseService
{
public:
    VBoxNetNAT();
    virtual ~VBoxNetNAT();
    void usage(void);
    void run(void);
    void init(void);

public:
    PNATState m_pNATState;
    RTNETADDRIPV4    m_Ipv4Netmask;
    bool m_fPassDomain;
    RTTHREAD m_ThrNAT;
    RTTHREAD m_ThrSndNAT;
    RTTHREAD m_ThrUrgSndNAT;
#ifdef RT_OS_WINDOWS
    HANDLE m_hWakeupEvent;
#else
    RTPIPE m_hPipeWrite;
    RTPIPE m_hPipeRead;
#endif
    /** Queue for NAT-thread-external events. */
    /** event to wakeup the guest receive thread */
    RTSEMEVENT              m_EventSend;
    /** event to wakeup the guest urgent receive thread */
    RTSEMEVENT              m_EventUrgSend;

    PRTREQQUEUE             m_pReqQueue;
    PRTREQQUEUE             m_pSendQueue;
    PRTREQQUEUE             m_pUrgSendQueue;
    volatile uint32_t       cUrgPkt;
    volatile uint32_t       cPkt;
    bool                    fIsRunning;
};



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the NAT server. */
class VBoxNetNAT *g_pNAT;
static DECLCALLBACK(int) AsyncIoThread(RTTHREAD pThread, void *pvUser);
static DECLCALLBACK(int) natSndThread(RTTHREAD pThread, void *pvUser);
static DECLCALLBACK(int) natUrgSndThread(RTTHREAD pThread, void *pvUser);
static void SendWorker(struct mbuf *m, size_t cb);
static void IntNetSendWorker(bool urg, void *pvFrame, size_t cbFrame, struct mbuf *m);


static void natNotifyNATThread(void)
{
    int rc;
#ifndef RT_OS_WINDOWS
    /* kick select() */
    size_t cbIgnored;
    rc = RTPipeWrite(g_pNAT->m_hPipeWrite, "", 1, &cbIgnored);
#else
    /* kick WSAWaitForMultipleEvents */
    rc = WSASetEvent(g_pNAT->hWakeupEvent);
#endif
    AssertRC(rc);
}

VBoxNetNAT::VBoxNetNAT()
{
#if defined(RT_OS_WINDOWS)
    /*@todo check if we can remove this*/
    VBoxNetBaseService();
#endif
    m_enmTrunkType             = kIntNetTrunkType_WhateverNone;
    m_TrunkName             = "";
    m_MacAddress.au8[0]     = 0x08;
    m_MacAddress.au8[1]     = 0x00;
    m_MacAddress.au8[2]     = 0x27;
    m_MacAddress.au8[3]     = 0x40;
    m_MacAddress.au8[4]     = 0x41;
    m_MacAddress.au8[5]     = 0x42;
    m_Ipv4Address.u         = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  1)));
    m_Ipv4Netmask.u         = 0xffff0000;
    cPkt = 0;
    cUrgPkt = 0;
}

VBoxNetNAT::~VBoxNetNAT() { }
void VBoxNetNAT::init()
{
    int rc;

    /*
     * Initialize slirp.
     */
    rc = slirp_init(&m_pNATState, m_Ipv4Address.u, m_Ipv4Netmask.u, m_fPassDomain, false, 0, this);
    AssertReleaseRC(rc);

    slirp_set_ethaddr_and_activate_port_forwarding(m_pNATState, &m_MacAddress.au8[0], INADDR_ANY);
#ifndef RT_OS_WINDOWS
    /*
     * Create the control pipe.
     */
    rc = RTPipeCreate(&m_hPipeRead, &m_hPipeWrite, 0 /*fFlags*/);
    AssertReleaseRC(rc);
#else
    m_hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
    AssertReleaseRC(m_hWakeupEvent != NULL);
    slirp_register_external_event(m_pNATState, m_hWakeupEvent, VBOX_WAKEUP_EVENT_INDEX);
#endif
    rc = RTReqCreateQueue(&m_pReqQueue);
    AssertReleaseRC(rc);

    rc = RTReqCreateQueue(&m_pSendQueue);
    AssertReleaseRC(rc);

    rc = RTReqCreateQueue(&m_pUrgSendQueue);
    AssertReleaseRC(rc);

    rc = RTThreadCreate(&m_ThrNAT, AsyncIoThread, this, 128 * _1K, RTTHREADTYPE_DEFAULT, 0, "NAT");
    rc = RTThreadCreate(&m_ThrSndNAT, natSndThread, this, 128 * _1K, RTTHREADTYPE_DEFAULT, 0, "SndNAT");
    rc = RTThreadCreate(&m_ThrUrgSndNAT, natUrgSndThread, this, 128 * _1K, RTTHREADTYPE_DEFAULT, 0, "UrgSndNAT");
    rc = RTSemEventCreate(&m_EventSend);
    rc = RTSemEventCreate(&m_EventUrgSend);
    AssertReleaseRC(rc);
}

/* Mandatory functions */
void VBoxNetNAT::run()
{

    /*
     * The loop.
     */
    fIsRunning = true;
    PINTNETRINGBUF  pRingBuf = &m_pIfBuf->Recv;
    //RTThreadSetType(RTThreadSelf(), RTTHREADTYPE_IO);
    for (;;)
    {
        /*
         * Wait for a packet to become available.
         */
        INTNETIFWAITREQ WaitReq;
        WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        WaitReq.Hdr.cbReq = sizeof(WaitReq);
        WaitReq.pSession = m_pSession;
        WaitReq.hIf = m_hIf;
        WaitReq.cMillies = 2000; /* 2 secs - the sleep is for some reason uninterruptible... */  /** @todo fix interruptability in SrvIntNet! */
#if 1
        RTReqProcess(m_pSendQueue, 0);
        RTReqProcess(m_pUrgSendQueue, 0);
#endif
        int rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_WAIT, 0, &WaitReq.Hdr);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
                continue;
            LogRel(("VBoxNetNAT: VMMR0_DO_INTNET_IF_WAIT returned %Rrc\n", rc));
            return;
        }

        /*
         * Process the receive buffer.
         */
        PCINTNETHDR pHdr;
        while ((pHdr = IntNetRingGetNextFrameToRead(pRingBuf)) != NULL)
        {
            uint16_t const u16Type = pHdr->u16Type;
            if (RT_LIKELY(   u16Type == INTNETHDR_TYPE_FRAME
                          || u16Type == INTNETHDR_TYPE_GSO))
            {
                size_t       cbFrame = pHdr->cbFrame;
                size_t       cbIgnored;
                void        *pvSlirpFrame;
                struct mbuf *m;
                if (u16Type == INTNETHDR_TYPE_FRAME)
                {
                    m = slirp_ext_m_get(g_pNAT->m_pNATState, cbFrame, &pvSlirpFrame, &cbIgnored);
                    if (!m)
                    {
                        LogRel(("NAT: Can't allocate send buffer cbFrame=%u\n", cbFrame));
                        break;
                    }
                    memcpy(pvSlirpFrame, IntNetHdrGetFramePtr(pHdr, m_pIfBuf), cbFrame);
                    IntNetRingSkipFrame(&m_pIfBuf->Recv);

                    /* don't wait, we may have to wakeup the NAT thread first */
                    rc = RTReqCallEx(m_pReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                                     (PFNRT)SendWorker, 2, m, cbFrame);
                    AssertReleaseRC(rc);
                }
                else
                {
                    /** @todo pass these unmodified. */
                    PCPDMNETWORKGSO pGso  = IntNetHdrGetGsoContext(pHdr, m_pIfBuf);
                    if (!PDMNetGsoIsValid(pGso, cbFrame, cbFrame - sizeof(*pGso)))
                    {
                        IntNetRingSkipFrame(&m_pIfBuf->Recv);
                        STAM_REL_COUNTER_INC(&m_pIfBuf->cStatBadFrames);
                        continue;
                    }

                    uint8_t abHdrScratch[256];
                    uint32_t const cSegs = PDMNetGsoCalcSegmentCount(pGso, cbFrame - sizeof(*pGso));
                    for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
                    {
                        uint32_t cbSegFrame;
                        void  *pvSegFrame = PDMNetGsoCarveSegmentQD(pGso, (uint8_t *)(pGso + 1), cbFrame, abHdrScratch,
                                                                    iSeg, cSegs, &cbSegFrame);
                        m = slirp_ext_m_get(g_pNAT->m_pNATState, cbFrame, &pvSlirpFrame, &cbIgnored);
                        if (!m)
                        {
                            LogRel(("NAT: Can't allocate send buffer cbSegFrame=%u seg=%u/%u\n", cbSegFrame, iSeg, cSegs));
                            break;
                        }
                        memcpy(pvSlirpFrame, pvSegFrame, cbFrame);

                        rc = RTReqCallEx(m_pReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                                         (PFNRT)SendWorker, 2, m, cbSegFrame);
                        AssertReleaseRC(rc);
                    }
                    IntNetRingSkipFrame(&m_pIfBuf->Recv);
                }

#ifndef RT_OS_WINDOWS
                /* kick select() */
                size_t cbIgnored;
                rc = RTPipeWrite(m_hPipeWrite, "", 1, &cbIgnored);
                AssertRC(rc);
#else
                /* kick WSAWaitForMultipleEvents */
                rc = WSASetEvent(m_hWakeupEvent);
                AssertRelease(rc == TRUE);
#endif
            }
            else if (u16Type == INTNETHDR_TYPE_PADDING)
                IntNetRingSkipFrame(&m_pIfBuf->Recv);
            else
            {
                IntNetRingSkipFrame(&m_pIfBuf->Recv);
                STAM_REL_COUNTER_INC(&m_pIfBuf->cStatBadFrames);
            }
        }

    }
    fIsRunning = false;
}

void VBoxNetNAT::usage()
{
}

/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    Log2(("NAT: main\n"));
    g_pNAT = new VBoxNetNAT();
    Log2(("NAT: parsing command line\n"));
    int rc = g_pNAT->parseArgs(argc - 1, argv + 1);
    if (!rc)
    {
        Log2(("NAT: initialization\n"));
        g_pNAT->init();
        Log2(("NAT: try go online\n"));
        g_pNAT->tryGoOnline();
        Log2(("NAT: main loop\n"));
        g_pNAT->run();
    }
    delete g_pNAT;
    return 0;
}

/** slirp's hooks */
extern "C" int slirp_can_output(void * pvUser)
{
    return 1;
}

extern "C" void slirp_urg_output(void *pvUser, struct mbuf *m, const uint8_t *pu8Buf, int cb)
{
    int rc = RTReqCallEx(g_pNAT->m_pUrgSendQueue,  NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                         (PFNRT)IntNetSendWorker, 4, (uintptr_t)1, (uintptr_t)pu8Buf, (uintptr_t)cb, (uintptr_t)m);
    ASMAtomicIncU32(&g_pNAT->cUrgPkt);
    RTSemEventSignal(g_pNAT->m_EventUrgSend);
    AssertReleaseRC(rc);
}
extern "C" void slirp_output(void *pvUser, struct mbuf *m, const uint8_t *pu8Buf, int cb)
{
    AssertRelease(g_pNAT == pvUser);
    int rc = RTReqCallEx(g_pNAT->m_pSendQueue,  NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                         (PFNRT)IntNetSendWorker, 4, (uintptr_t)0, (uintptr_t)pu8Buf, (uintptr_t)cb, (uintptr_t)m);
    ASMAtomicIncU32(&g_pNAT->cPkt);
    RTSemEventSignal(g_pNAT->m_EventSend);
    AssertReleaseRC(rc);
}

extern "C" void slirp_output_pending(void *pvUser)
{
  AssertMsgFailed(("Unimplemented"));
}

/**
 * Worker function for drvNATSend().
 * @thread "NAT" thread.
 */
static void SendWorker(struct mbuf *m, size_t cb)
{
    slirp_input(g_pNAT->m_pNATState, m, cb);
}

static void IntNetSendWorker(bool urg, void *pvFrame, size_t cbFrame, struct mbuf *m)
{
    Log2(("VBoxNetNAT: going to send some bytes ... \n"));
    VBoxNetNAT         *pThis = g_pNAT;
    INTNETIFSENDREQ     SendReq;
    int rc;

    if (!urg)
    {
        while (ASMAtomicReadU32(&g_pNAT->cUrgPkt) != 0
               || ASMAtomicReadU32(&g_pNAT->cPkt) == 0)
            rc = RTSemEventWait(g_pNAT->m_EventSend, RT_INDEFINITE_WAIT);
    }
    else
    {
        while (ASMAtomicReadU32(&g_pNAT->cUrgPkt) == 0)
            rc = RTSemEventWait(g_pNAT->m_EventUrgSend, RT_INDEFINITE_WAIT);
    }
    rc = IntNetRingWriteFrame(&pThis->m_pIfBuf->Send, pvFrame, cbFrame);
    if (RT_FAILURE(rc))
    {
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq    = sizeof(SendReq);
        SendReq.pSession     = pThis->m_pSession;
        SendReq.hIf          = pThis->m_hIf;
        rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);

        rc = IntNetRingWriteFrame(&pThis->m_pIfBuf->Send, pvFrame, cbFrame);

    }
    if (RT_SUCCESS(rc))
    {
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq    = sizeof(SendReq);
        SendReq.pSession     = pThis->m_pSession;
        SendReq.hIf          = pThis->m_hIf;
        rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);
    }
    if (RT_FAILURE(rc))
        Log2(("VBoxNetNAT: Failed to send packet; rc=%Rrc\n", rc));

    if (!urg)
    {
        ASMAtomicDecU32(&g_pNAT->cPkt);
    }
    else {
        if (ASMAtomicDecU32(&g_pNAT->cUrgPkt) == 0)
            RTSemEventSignal(g_pNAT->m_EventSend);
    }
    natNotifyNATThread();
    slirp_ext_m_free(pThis->m_pNATState, m, (uint8_t *)pvFrame);
}

static DECLCALLBACK(int) AsyncIoThread(RTTHREAD pThread, void *pvUser)
{
    VBoxNetNAT *pThis = (VBoxNetNAT *)pvUser;
    int     nFDs = -1;
#ifdef RT_OS_WINDOWS
    HANDLE *pahEvents = slirp_get_events(pThis->m_pNATState);
#else /* RT_OS_WINDOWS */
    unsigned int cPollNegRet = 0;
#endif /* !RT_OS_WINDOWS */

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));

    /*
     * Polling loop.
     */
    for(;;)
    {
        /*
         * To prevent concurrent execution of sending/receiving threads
         */
#ifndef RT_OS_WINDOWS
        nFDs = slirp_get_nsock(pThis->m_pNATState);
        /* allocation for all sockets + Management pipe */
        struct pollfd *polls = (struct pollfd *)RTMemAlloc((1 + nFDs) * sizeof(struct pollfd) + sizeof(uint32_t));
        if (polls == NULL)
            return VERR_NO_MEMORY;

        /* don't pass the management pipe */
        slirp_select_fill(pThis->m_pNATState, &nFDs, &polls[1]);
        unsigned int cMsTimeout = slirp_get_timeout_ms(pThis->m_pNATState);

        polls[0].fd = RTPipeToNative(pThis->m_hPipeRead);
        /* POLLRDBAND usually doesn't used on Linux but seems used on Solaris */
        polls[0].events = POLLRDNORM|POLLPRI|POLLRDBAND;
        polls[0].revents = 0;

        int cChangedFDs = poll(polls, nFDs + 1, cMsTimeout);
        if (cChangedFDs < 0)
        {
            if (errno == EINTR)
            {
                Log2(("NAT: signal was caught while sleep on poll\n"));
                /* No error, just process all outstanding requests but don't wait */
                cChangedFDs = 0;
            }
            else if (cPollNegRet++ > 128)
            {
                LogRel(("NAT:Poll returns (%s) suppressed %d\n", strerror(errno), cPollNegRet));
                cPollNegRet = 0;
            }
        }

        if (cChangedFDs >= 0)
        {
            slirp_select_poll(pThis->m_pNATState, &polls[1], nFDs);
            if (polls[0].revents & (POLLRDNORM|POLLPRI|POLLRDBAND))
            {
                /* drain the pipe
                 *
                 * Note!
                 * drvNATSend decoupled so we don't know how many times
                 * device's thread sends before we've entered multiplex,
                 * so to avoid false alarm drain pipe here to the very end
                 *
                 * @todo: Probably we should counter drvNATSend to count how
                 * deep pipe has been filed before drain.
                 *
                 */
                 /** @todo XXX: Make it reading exactly we need to drain the
                  *  pipe.  */
                char    ch;
                size_t  cbRead;
                RTPipeRead(pThis->m_hPipeRead, &ch, 1, &cbRead);
            }
        }
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->m_pReqQueue, 0);
        RTMemFree(polls);

#else /* RT_OS_WINDOWS */
        nFDs = -1;
        slirp_select_fill(pThis->m_pNATState, &nFDs);
        DWORD dwEvent = WSAWaitForMultipleEvents(nFDs, pahEvents, FALSE,
                                                 slirp_get_timeout_ms(pThis->m_pNATState),
                                                 FALSE);
        if (   (dwEvent < WSA_WAIT_EVENT_0 || dwEvent > WSA_WAIT_EVENT_0 + nFDs - 1)
            && dwEvent != WSA_WAIT_TIMEOUT)
        {
            int error = WSAGetLastError();
            LogRel(("NAT: WSAWaitForMultipleEvents returned %d (error %d)\n", dwEvent, error));
            RTAssertReleasePanic();
        }

        if (dwEvent == WSA_WAIT_TIMEOUT)
        {
            /* only check for slow/fast timers */
            slirp_select_poll(pThis->m_pNATState, /* fTimeout=*/true, /*fIcmp=*/false);
            continue;
        }

        /* poll the sockets in any case */
        slirp_select_poll(pThis->m_pNATState, /* fTimeout=*/false, /* fIcmp=*/(dwEvent == WSA_WAIT_EVENT_0));
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->m_pReqQueue, 0);
#endif /* RT_OS_WINDOWS */
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) natSndThread(RTTHREAD pThread, void *pvUser)
{
    while (g_pNAT->fIsRunning)
        RTReqProcess(g_pNAT->m_pSendQueue, 0);
    return VINF_SUCCESS;
}
static DECLCALLBACK(int) natUrgSndThread(RTTHREAD pThread, void *pvUser)
{
    while (g_pNAT->fIsRunning)
        RTReqProcess(g_pNAT->m_pUrgSendQueue, 0);
    return VINF_SUCCESS;
}

#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv, char **envp)
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    return TrustedMain(argc, argv, envp);
}

# if defined(RT_OS_WINDOWS)

static LRESULT CALLBACK WindowProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    if(uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc (hwnd, uMsg, wParam, lParam);
}

static LPCSTR g_WndClassName = "VBoxNetNatClass";

static DWORD WINAPI MsgThreadProc(__in  LPVOID lpParameter)
{
     HWND                 hwnd = 0;
     HINSTANCE hInstance = (HINSTANCE)GetModuleHandle (NULL);
     bool bExit = false;

     /* Register the Window Class. */
     WNDCLASS wc;
     wc.style         = 0;
     wc.lpfnWndProc   = WindowProc;
     wc.cbClsExtra    = 0;
     wc.cbWndExtra    = sizeof(void *);
     wc.hInstance     = hInstance;
     wc.hIcon         = NULL;
     wc.hCursor       = NULL;
     wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
     wc.lpszMenuName  = NULL;
     wc.lpszClassName = g_WndClassName;

     ATOM atomWindowClass = RegisterClass(&wc);

     if (atomWindowClass != 0)
     {
         /* Create the window. */
         hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                 g_WndClassName, g_WndClassName,
                                                   WS_POPUPWINDOW,
                                                  -200, -200, 100, 100, NULL, NULL, hInstance, NULL);

         if (hwnd)
         {
             SetWindowPos(hwnd, HWND_TOPMOST, -200, -200, 0, 0,
                          SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

             MSG msg;
             while (GetMessage(&msg, NULL, 0, 0))
             {
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
             }

             DestroyWindow (hwnd);

             bExit = true;
         }

         UnregisterClass (g_WndClassName, hInstance);
     }

     if(bExit)
     {
         /* no need any accuracy here, in anyway the DHCP server usually gets terminated with TerminateProcess */
         exit(0);
     }

     return 0;
}



/** (We don't want a console usually.) */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#if 0
    NOREF(hInstance); NOREF(hPrevInstance); NOREF(lpCmdLine); NOREF(nCmdShow);

    HANDLE hThread = CreateThread(
      NULL, /*__in_opt   LPSECURITY_ATTRIBUTES lpThreadAttributes, */
      0, /*__in       SIZE_T dwStackSize, */
      MsgThreadProc, /*__in       LPTHREAD_START_ROUTINE lpStartAddress,*/
      NULL, /*__in_opt   LPVOID lpParameter,*/
      0, /*__in       DWORD dwCreationFlags,*/
      NULL /*__out_opt  LPDWORD lpThreadId*/
    );

    if(hThread != NULL)
        CloseHandle(hThread);

#endif
    return main(__argc, __argv, environ);
}
# endif /* RT_OS_WINDOWS */

#endif /* !VBOX_WITH_HARDENING */

