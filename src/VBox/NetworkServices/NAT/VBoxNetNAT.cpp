/* $Id$ */
/** @file
 * VBoxNetNAT - NAT Service for connecting to IntNet.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
#include <iprt/getopt.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/file.h>
#include <iprt/semaphore.h>
#define LOG_GROUP LOG_GROUP_NAT_SERVICE
#include <VBox/log.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm.h>
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
    RTFILE m_PipeWrite;
    RTFILE m_PipeRead;
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

    PRTTIMER              pTmrSlow;
    PRTTIMER              pTmrFast;
    bool                  fIsRunning;
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
static void IntNetSendWorker(bool urg, const void *pvFrame, size_t cbFrame, struct mbuf *m);
static void natNotifyNATThread();

void slirp_arm_fast_timer(void *pvUser)
{
    RTTimerStart(g_pNAT->pTmrFast, 2);
}

void slirp_arm_slow_timer(void *pvUser)
{
    RTTimerStart(g_pNAT->pTmrSlow, 500);
}

static DECLCALLBACK(void) natSlowTimer(PRTTIMER ppTimer, void *pvUser, uint64_t iTick)
{
    natNotifyNATThread();
}

static DECLCALLBACK(void) natFastTimer(PRTTIMER ppTimer, void *pvUser, uint64_t iTick)
{
    natNotifyNATThread();
}

static void natNotifyNATThread()
{
    int rc;
#ifndef RT_OS_WINDOWS
    /* kick select() */
    rc = RTFileWrite(g_pNAT->m_PipeWrite, "", 1, NULL);
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
    rc = slirp_init(&m_pNATState, m_Ipv4Address.u, m_Ipv4Netmask.u, m_fPassDomain, false, this);
    AssertReleaseRC(rc);

    slirp_set_ethaddr_and_activate_port_forwarding(m_pNATState, &m_MacAddress.au8[0], INADDR_ANY);
#ifndef RT_OS_WINDOWS
    /*
     * Create the control pipe.
     */
    int fds[2];
    if (pipe(&fds[0]) != 0) /** @todo RTPipeCreate() or something... */
    {
        rc = RTErrConvertFromErrno(errno);
        AssertReleaseRC(rc);
        return;
    }
    m_PipeRead = fds[0];
    m_PipeWrite = fds[1];
#else
    m_hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
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
    rc = RTTimerCreate(&pTmrSlow, 0 /* one-shot */, natSlowTimer, this);
    rc = RTTimerCreate(&pTmrFast, 0 /* one-shot */ , natFastTimer, this);
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
#if 0
        RTReqProcess(m_pSendQueue, 0);
        RTReqProcess(m_pUrgSendQueue, 0);
#endif
        int rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_WAIT, 0, &WaitReq.Hdr);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_TIMEOUT)
                continue;
            LogRel(("VBoxNetNAT: VMMR0_DO_INTNET_IF_WAIT returned %Rrc\n", rc));
            return;
        }

        /*
         * Process the receive buffer.
         */
        PCINTNETHDR pHdr;
        while ((pHdr = INTNETRingGetNextFrameToRead(pRingBuf)) != NULL)
        {
            if (RT_LIKELY(pHdr->u16Type == INTNETHDR_TYPE_FRAME))
            {
                size_t  cbFrame = pHdr->cbFrame;
                size_t  cbIgnored;
                void   *pvSlirpFrame;
                struct mbuf *m = slirp_ext_m_get(g_pNAT->m_pNATState, cbFrame, &pvSlirpFrame, &cbIgnored);
                if (!m)
                {
                    LogRel(("NAT: Can't allocate send buffer\n"));
                    break;
                }
                memcpy(pvSlirpFrame, INTNETHdrGetFramePtr(pHdr, m_pIfBuf), cbFrame);
                INTNETRingSkipFrame(&m_pIfBuf->Recv);

                /* don't wait, we have to wakeup the NAT thread fist */
                rc = RTReqCallEx(m_pReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                                 (PFNRT)SendWorker, 2, m, cbFrame);
                AssertReleaseRC(rc);

#ifndef RT_OS_WINDOWS
                /* kick select() */
                rc = RTFileWrite(m_PipeWrite, "", 1, NULL);
                AssertRC(rc);
#else
                /* kick WSAWaitForMultipleEvents */
                rc = WSASetEvent(m_hWakeupEvent);
                AssertRelease(rc == TRUE);
#endif
            }
            else if (pHdr->u16Type == INTNETHDR_TYPE_PADDING)
                INTNETRingSkipFrame(&m_pIfBuf->Recv);
            else
            {
                INTNETRingSkipFrame(&m_pIfBuf->Recv);
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
    g_pNAT->parseArgs(argc - 1, argv + 1);
    Log2(("NAT: initialization\n"));
    g_pNAT->init();
    Log2(("NAT: try go online\n"));
    g_pNAT->tryGoOnline();
    Log2(("NAT: main loop\n"));
    g_pNAT->run();
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

/**
 * Worker function for drvNATSend().
 * @thread "NAT" thread.
 */
static void SendWorker(struct mbuf *m, size_t cb)
{
    slirp_input(g_pNAT->m_pNATState, m, cb);
}

static void IntNetSendWorker(bool urg, const void *pvFrame, size_t cbFrame, struct mbuf *m)
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
    rc = INTNETRingWriteFrame(&pThis->m_pIfBuf->Send, pvFrame, cbFrame);
    if (RT_FAILURE(rc))
    {
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq    = sizeof(SendReq);
        SendReq.pSession     = pThis->m_pSession;
        SendReq.hIf          = pThis->m_hIf;
        rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);

        rc = INTNETRingWriteFrame(&pThis->m_pIfBuf->Send, pvFrame, cbFrame);

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
    slirp_ext_m_free(pThis->m_pNATState, m);
#ifdef VBOX_WITH_SLIRP_BSD_MBUF
    RTMemFree((void *)pvFrame);
#endif
}

static DECLCALLBACK(int) AsyncIoThread(RTTHREAD pThread, void *pvUser)
{
    VBoxNetNAT *pThis = (VBoxNetNAT *)pvUser;
    int     nFDs = -1;
    unsigned int ms;
#ifdef RT_OS_WINDOWS
    DWORD   event;
    HANDLE  *phEvents;
    unsigned int cBreak = 0;
#else /* RT_OS_WINDOWS */
    struct pollfd *polls = NULL;
    unsigned int cPollNegRet = 0;
#endif /* !RT_OS_WINDOWS */

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));


#ifdef RT_OS_WINDOWS
    phEvents = slirp_get_events(pThis->m_pNATState);
#endif /* RT_OS_WINDOWS */

    /*
     * Polling loop.
     */
    for(;;)
    {
        nFDs = -1;

        /*
         * To prevent concurent execution of sending/receving threads
         */
#ifndef RT_OS_WINDOWS
        nFDs = slirp_get_nsock(pThis->m_pNATState);
        polls = NULL;
        /* allocation for all sockets + Management pipe */
        polls = (struct pollfd *)RTMemAlloc((1 + nFDs) * sizeof(struct pollfd) + sizeof(uint32_t));
        if (polls == NULL)
            return VERR_NO_MEMORY;

        /* don't pass the managemant pipe */
        slirp_select_fill(pThis->m_pNATState, &nFDs, &polls[1]);
        ms = slirp_get_timeout_ms(pThis->m_pNATState);

        polls[0].fd = pThis->m_PipeRead;
        /* POLLRDBAND usually doesn't used on Linux but seems used on Solaris */
        polls[0].events = POLLRDNORM|POLLPRI|POLLRDBAND;
        polls[0].revents = 0;

        int cChangedFDs = poll(polls, nFDs + 1, -1);
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
                /* drain the pipe */
                char ch[1];
                size_t cbRead;
                int counter = 0;
                /*
                 * drvNATSend decoupled so we don't know how many times
                 * device's thread sends before we've entered multiplex,
                 * so to avoid false alarm drain pipe here to the very end
                 *
                 * @todo: Probably we should counter drvNATSend to count how
                 * deep pipe has been filed before drain.
                 *
                 * XXX:Make it reading exactly we need to drain the pipe.
                 */
                RTFileRead(pThis->m_PipeRead, &ch, 1, &cbRead);
            }
        }
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->m_pReqQueue, 0);
        RTMemFree(polls);
#else /* RT_OS_WINDOWS */
        slirp_select_fill(pThis->m_pNATState, &nFDs);
        event = WSAWaitForMultipleEvents(nFDs, phEvents, FALSE, WSA_INFINITE, FALSE);
        if (   (event < WSA_WAIT_EVENT_0 || event > WSA_WAIT_EVENT_0 + nFDs - 1)
            && event != WSA_WAIT_TIMEOUT)
        {
            int error = WSAGetLastError();
            LogRel(("NAT: WSAWaitForMultipleEvents returned %d (error %d)\n", event, error));
            RTAssertReleasePanic();
        }

        if (event == WSA_WAIT_TIMEOUT)
        {
            /* only check for slow/fast timers */
            slirp_select_poll(pThis->m_pNATState, /* fTimeout=*/true, /*fIcmp=*/false);
            continue;
        }

        /* poll the sockets in any case */
        slirp_select_poll(pThis->m_pNATState, /* fTimeout=*/false, /* fIcmp=*/(event == WSA_WAIT_EVENT_0));
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
    int rc = RTR3InitAndSUPLib();
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: RTR3InitAndSupLib failed, rc=%Rrc\n", rc);
        return 1;
    }

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

