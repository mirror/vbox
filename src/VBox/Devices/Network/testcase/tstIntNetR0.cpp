/** @file
 *
 * VBox - Testcase for the Ring-0 part of internal networking.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define IN_INTNET_TESTCASE
#define IN_INTNET_R3
#include <VBox/cdefs.h>
#undef INTNETR0DECL
#define INTNETR0DECL INTNETR3DECL
#include <VBox/intnet.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/runtime.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/asm.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Security objectype.
 */
typedef enum SUPDRVOBJTYPE
{
    /** The usual invalid object. */
    SUPDRVOBJTYPE_INVALID = 0,
    /** Internal network. */
    SUPDRVOBJTYPE_INTERNAL_NETWORK,
    /** Internal network interface. */
    SUPDRVOBJTYPE_INTERNAL_NETWORK_INTERFACE,
    /** The first invalid object type in this end. */
    SUPDRVOBJTYPE_END,
    /** The usual 32-bit type size hack. */
    SUPDRVOBJTYPE_32_BIT_HACK = 0x7ffffff
} SUPDRVOBJTYPE;

/**
 * Object destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     The first user argument.
 * @param   pvUser2     The second user argument.
 */
typedef DECLCALLBACK(void) FNSUPDRVDESTRUCTOR(void *pvObj, void *pvUser1, void *pvUser2);
/** Pointer to a FNSUPDRVDESTRUCTOR(). */
typedef FNSUPDRVDESTRUCTOR *PFNSUPDRVDESTRUCTOR;


/**
 * Dummy
 */
typedef struct OBJREF
{
    PFNSUPDRVDESTRUCTOR pfnDestructor;
    void *pvUser1;
    void *pvUser2;
    uint32_t volatile cRefs;
} OBJREF, *POBJREF;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;

/** Fake session handle. */
const PSUPDRVSESSION g_pSession = (PSUPDRVSESSION)0xdeadface;

/** Testframe 0 */
struct TESTFRAME
{
    uint16_t    au16[6];
} g_TestFrame0 = { { /* dst:*/ 0xffff, 0xffff, 0xffff, /*src:*/0x8086, 0, 0} },
  g_TestFrame1 = { { /* dst:*/0, 0, 0,                /*src:*/0x8086, 0, 1} };


INTNETR3DECL(void *) SUPR0ObjRegister(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return NULL;
    }
    POBJREF pRef = (POBJREF)RTMemAlloc(sizeof(OBJREF));
    if (!pRef)
        return NULL;
    pRef->cRefs = 1;
    pRef->pfnDestructor = pfnDestructor;
    pRef->pvUser1 = pvUser1;
    pRef->pvUser2 = pvUser2;
    return pRef;
}

INTNETR3DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return VERR_INVALID_PARAMETER;
    }
    POBJREF pRef = (POBJREF)pvObj;
    ASMAtomicIncU32(&pRef->cRefs);
    return VINF_SUCCESS;
}

INTNETR3DECL(int) SUPR0ObjRelease(void *pvObj, PSUPDRVSESSION pSession)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return VERR_INVALID_PARAMETER;
    }
    POBJREF pRef = (POBJREF)pvObj;
    if (!ASMAtomicDecU32(&pRef->cRefs))
    {
        pRef->pfnDestructor(pRef, pRef->pvUser1, pRef->pvUser2);
        RTMemFree(pRef);
    }
    return VINF_SUCCESS;
}

INTNETR3DECL(int) SUPR0ObjVerifyAccess(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}

INTNETR3DECL(int) SUPR0MemAlloc(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return VERR_INVALID_PARAMETER;
    }
    void *pv = RTMemAlloc(cb);
    if (!pv)
        return VERR_NO_MEMORY;
    *ppvR0 = (RTR0PTR)pv;
    if (ppvR3)
        *ppvR3 = pv;
    return VINF_SUCCESS;
}

INTNETR3DECL(int) SUPR0MemFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    if (pSession != g_pSession)
    {
        RTPrintf("tstIntNetR0: Invalid session pointer %p, %s!\n", pSession, __FUNCTION__);
        g_cErrors++;
        return VERR_INVALID_PARAMETER;
    }
    RTMemFree((void *)uPtr);
    return VINF_SUCCESS;
}



/* ugly but necessary for making R0 code compilable for R3. */
#undef LOG_GROUP
#include "../SrvIntNetR0.cpp"

typedef struct ARGS
{
    PINTNET pIntNet;
    PINTNETBUF pBuf;
    INTNETIFHANDLE hIf;
    PDMMAC Mac;
    uint64_t u64Start;
    uint64_t u64End;
} ARGS, *PARGS;


#define TEST_TRANSFER_SIZE (_1M*384)

/**
 * Send thread.
 * This is constantly broadcasting frames to the network.
 */
DECLCALLBACK(int) SendThread(RTTHREAD Thread, void *pvArg)
{
    PARGS   pArgs = (PARGS)pvArg;

    /*
     * Send 64 MB of data.
     */
    uint8_t abBuf[4096] = {0};
    PPDMMAC pMacSrc = (PPDMMAC)&abBuf[0];
    PPDMMAC pMacDst = pMacSrc + 1;
    *pMacSrc = pArgs->Mac;
    *pMacDst = pArgs->Mac;
    pMacDst->au16[2] = pArgs->Mac.au16[2] ? 0 : 1;
    unsigned *puFrame = (unsigned *)(pMacDst + 1);
    unsigned iFrame = 0;
    unsigned cbSent = 0;
    pArgs->u64Start = RTTimeNanoTS();
    for (; cbSent < TEST_TRANSFER_SIZE; iFrame++)
    {
        const unsigned cb = iFrame % 1519 + 12 + sizeof(unsigned);
        *puFrame = iFrame;
#if 0
        int rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, abBuf, cb);
#else
        int rc = INTNETRingWriteFrame(pArgs->pBuf, &pArgs->pBuf->Send, abBuf, cb);
        if (RT_SUCCESS(rc))
            rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, NULL, 0);
#endif
        if (VBOX_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("tstIntNetR0: Failed sending %d bytes, rc=%Vrc (%d)\n", cb, rc, INTNETRingGetWritable(&pArgs->pBuf->Send));
        }
        cbSent += cb;
    }

    /*
     * Termination frames.
     */
    puFrame[0] = 0xffffdead;
    puFrame[1] = 0xffffdead;
    puFrame[2] = 0xffffdead;
    puFrame[3] = 0xffffdead;
    for (unsigned c = 0; c < 20; c++)
    {
        int rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, abBuf, sizeof(PDMMAC) * 2 + sizeof(unsigned) * 4);
        if (VBOX_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("tstIntNetR0: send failed, rc=%Vrc\n", rc);
        }
        RTThreadSleep(1);
    }

    RTPrintf("tstIntNetR0: sender thread %.6Rhxs terminating. iFrame=%d cbSent=%d\n", &pArgs->Mac, iFrame, cbSent);
    return 0;
}


/** Ignore lost frames. It only makes things worse to bitch about it. */
#define IGNORE_LOST_FRAMES

/**
 * Receive thread.
 * This is reading stuff from the network.
 */
DECLCALLBACK(int) ReceiveThread(RTTHREAD Thread, void *pvArg)
{
    unsigned    cbReceived = 0;
    unsigned    cLostFrames = 0;
    unsigned    iFrame = ~0;
    PARGS       pArgs = (PARGS)pvArg;
    for (;;)
    {
        /*
         * Wait for data.
         */
        int rc = INTNETR0IfWait(pArgs->pIntNet, pArgs->hIf, RT_INDEFINITE_WAIT);
        switch (rc)
        {
            case VERR_INTERRUPTED:
            case VINF_SUCCESS:
                break;
            case VERR_SEM_DESTROYED:
                RTPrintf("tstIntNetR0: receiver thread %.6Rhxs terminating. cbReceived=%u cLostFrames=%u iFrame=%u\n",
                         &pArgs->Mac, cbReceived, cLostFrames, iFrame);
                return VINF_SUCCESS;

            default:
                RTPrintf("tstIntNetR0: receiver thread %.6Rhxs got odd return value %Vrc! cbReceived=%u cLostFrames=%u iFrame=%u\n",
                         &pArgs->Mac, rc, cbReceived, cLostFrames, iFrame);
                g_cErrors++;
                return rc;
        }

        /*
         * Read data.
         */
        while (INTNETRingGetReadable(&pArgs->pBuf->Recv))
        {
            uint8_t abBuf[16384];
            unsigned cb = INTNETRingReadFrame(pArgs->pBuf, &pArgs->pBuf->Recv, abBuf);
            unsigned *puFrame = (unsigned *)&abBuf[sizeof(PDMMAC) * 2];

            /* check for termination frame. */
            if (    cb == sizeof(PDMMAC) * 2 + sizeof(unsigned) * 4
                &&  puFrame[0] == 0xffffdead
                &&  puFrame[1] == 0xffffdead
                &&  puFrame[2] == 0xffffdead
                &&  puFrame[3] == 0xffffdead)
            {
                RTPrintf("tstIntNetR0: receiver thread %.6Rhxs terminating. cbReceived=%u cLostFrames=%u iFrame=%u\n",
                         &pArgs->Mac, cbReceived, cLostFrames, iFrame);
                pArgs->u64End = RTTimeNanoTS();
                return VINF_SUCCESS;
            }

            /* validate frame header */
            PPDMMAC pMacSrc = (PPDMMAC)&abBuf[0];
            PPDMMAC pMacDst = pMacSrc + 1;
            if (    pMacDst->au16[0] != 0x8086
                ||  pMacDst->au16[1] != 0
                ||  pMacDst->au16[2] != pArgs->Mac.au16[2]
                ||  pMacSrc->au16[0] != 0x8086
                ||  pMacSrc->au16[1] != 0
                ||  pMacSrc->au16[2] == pArgs->Mac.au16[2])
            {
                RTPrintf("tstIntNetR0: receiver thread %.6Rhxs received frame header: %.16Rhxs\n",
                         &pArgs->Mac, abBuf);
                g_cErrors++;
            }

            /* frame stuff and stats. */
            int off = iFrame + 1 - *puFrame;
            if (off)
            {
                if (off > 0)
                {
                    RTPrintf("tstIntNetR0: receiver thread %.6Rhxs: iFrame=%d *puFrame=%d off=%d\n",
                             &pArgs->Mac, iFrame, *puFrame, off);
                    g_cErrors++;
                    cLostFrames++;
                }
                else
                {
                    cLostFrames += -off;
#ifndef IGNORE_LOST_FRAMES
                    if (off < 50)
                    {
                        RTPrintf("tstIntNetR0: receiver thread %.6Rhxs: iFrame=%d *puFrame=%d off=%d\n",
                                 &pArgs->Mac, iFrame, *puFrame, off);
                        g_cErrors++;
                    }
#endif
                }
            }
            iFrame = *puFrame;
            cbReceived += cb;
        }
    }
}

int main()
{

    /*
     * Init runtime and create an INTNET instance.
     */
    RTR3Init();
    RTPrintf("tstIntNetR0: TESTING...\n");
    PINTNET pIntNet;
    int rc = INTNETR0Create(&pIntNet);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstIntNetR0: INTNETR0Create failed, rc=%Vrc\n");
        return 1;
    }

    /*
     * Create two interfaces.
     */
    INTNETIFHANDLE hIf0 = INTNET_HANDLE_INVALID;
    rc = INTNETR0Open(pIntNet, g_pSession, "test", 1536*2 + 4, 0x8000, true, &hIf0);
    if (VBOX_SUCCESS(rc))
    {
        if (hIf0 != INTNET_HANDLE_INVALID)
        {
            INTNETIFHANDLE hIf1 = INTNET_HANDLE_INVALID;
            rc = INTNETR0Open(pIntNet, g_pSession, "test", 1536*2 + 4, 0x8000, true, &hIf1);
            if (VBOX_SUCCESS(rc))
            {
                if (hIf1 != INTNET_HANDLE_INVALID)
                {
                    PINTNETBUF pBuf0;
                    rc = INTNETR0IfGetRing0Buffer(pIntNet, hIf0, &pBuf0);
                    if (VBOX_FAILURE(rc) || !pBuf0)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfGetRing0Buffer failed! pBuf0=%p rc=%Vrc\n", pBuf0, rc);
                        g_cErrors++;
                    }
                    PINTNETBUF pBuf1;
                    rc = INTNETR0IfGetRing0Buffer(pIntNet, hIf1, &pBuf1);
                    if (VBOX_FAILURE(rc))
                    {
                        RTPrintf("tstIntNetR0: INTNETIfGetRing0Buffer failed! pBuf1=%p rc=%Vrc\n", pBuf1, rc);
                        g_cErrors++;
                    }

                    /*
                     * Test basic waiting.
                     */
                    rc = INTNETR0IfWait(pIntNet, hIf0, 1);
                    if (rc != VERR_TIMEOUT)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfWait returned %Vrc expected VERR_TIMEOUT (hIf0)\n", rc);
                        g_cErrors++;
                    }
                    rc = INTNETR0IfWait(pIntNet, hIf1, 0);
                    if (rc != VERR_TIMEOUT)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfWait returned %Vrc expected VERR_TIMEOUT (hIf1)\n", rc);
                        g_cErrors++;
                    }

                    /*
                     * Send and receive.
                     */
                    rc = INTNETR0IfSend(pIntNet, hIf0, &g_TestFrame0, sizeof(g_TestFrame0));
                    if (VBOX_SUCCESS(rc))
                    {
                        rc = INTNETR0IfWait(pIntNet, hIf0, 1);
                        if (rc != VERR_TIMEOUT)
                        {
                            RTPrintf("tstIntNetR0: INTNETIfWait returned %Vrc expected VERR_TIMEOUT (hIf0, 2nd)\n", rc);
                            g_cErrors++;
                        }
                        rc = INTNETR0IfWait(pIntNet, hIf1, 0);
                        if (rc == VINF_SUCCESS)
                        {
                            /* receive it */
                            uint8_t abBuf[sizeof(g_TestFrame0)];
                            const unsigned cbExpect = RT_ALIGN(sizeof(g_TestFrame0) + sizeof(INTNETHDR), sizeof(INTNETHDR));
                            if (INTNETRingGetReadable(&pBuf1->Recv) != cbExpect)
                            {
                                RTPrintf("tstIntNetR0: %d readable bytes, expected %d!\n", INTNETRingGetReadable(&pBuf1->Recv), cbExpect);
                                g_cErrors++;
                            }
                            unsigned cb = INTNETRingReadFrame(pBuf1, &pBuf1->Recv, abBuf);
                            if (cb != sizeof(g_TestFrame0))
                            {
                                RTPrintf("tstIntNetR0: read %d frame bytes, expected %d!\n", cb, sizeof(g_TestFrame0));
                                g_cErrors++;
                            }
                            else if (memcmp(abBuf, &g_TestFrame0, sizeof(g_TestFrame0)))
                            {
                                RTPrintf("tstIntNetR0: Got invalid data!\n"
                                         "received: %.*Rhxs\n"
                                         "expected: %.*Rhxs\n",
                                         cb, abBuf, sizeof(g_TestFrame0), &g_TestFrame0);
                                g_cErrors++;
                            }

                            /*
                             * Send a packet from If1 just to set its MAC address.
                             */
                            rc = INTNETR0IfSend(pIntNet, hIf1, &g_TestFrame1, sizeof(g_TestFrame1));
                            if (VBOX_FAILURE(rc))
                            {
                                RTPrintf("tstIntNetR0: INTNETIfSend returned %Vrc! (hIf1)\n", rc);
                                g_cErrors++;
                            }


                            /*
                             * Start threaded testcase.
                             */
                            if (!g_cErrors)
                            {
                                ARGS Args0 = {0};
                                Args0.hIf = hIf0;
                                Args0.pBuf = pBuf0;
                                Args0.pIntNet = pIntNet;
                                Args0.Mac.au16[0] = 0x8086;
                                Args0.Mac.au16[1] = 0;
                                Args0.Mac.au16[2] = 0;

                                ARGS Args1 = {0};
                                Args1.hIf = hIf1;
                                Args1.pBuf = pBuf1;
                                Args1.pIntNet = pIntNet;
                                Args1.Mac.au16[0] = 0x8086;
                                Args1.Mac.au16[1] = 0;
                                Args1.Mac.au16[2] = 1;

                                RTTHREAD ThreadRecv0 = NIL_RTTHREAD;
                                RTTHREAD ThreadRecv1 = NIL_RTTHREAD;
                                RTTHREAD ThreadSend0 = NIL_RTTHREAD;
                                RTTHREAD ThreadSend1 = NIL_RTTHREAD;
                                rc = RTThreadCreate(&ThreadRecv0, ReceiveThread, &Args0, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "RECV0");
                                if (VBOX_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadRecv1, ReceiveThread, &Args1, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "RECV1");
                                if (VBOX_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadSend0, SendThread, &Args0, 0, RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "SEND0");
                                if (VBOX_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadSend1, SendThread, &Args1, 0, RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "SEND1");
                                if (VBOX_SUCCESS(rc))
                                {
                                    int rc2 = VINF_SUCCESS;
                                    rc = RTThreadWait(ThreadSend0, 30000, &rc2);
                                    if (    VBOX_SUCCESS(rc)
                                        &&  VBOX_SUCCESS(rc2))
                                        rc = RTThreadWait(ThreadSend1, 30000, &rc2);
                                    if (    VBOX_SUCCESS(rc)
                                        &&  VBOX_SUCCESS(rc2))
                                    {
                                        /*
                                         * Wait a bit for the receivers to finish up.
                                         */
                                        unsigned cYields = 100000;
                                        while (     (   INTNETRingGetReadable(&pBuf0->Recv)
                                                    ||  INTNETRingGetReadable(&pBuf1->Recv))
                                               &&   cYields-- > 0)
                                            RTThreadYield();

                                        uint64_t u64Elapsed = RT_MAX(Args0.u64End, Args1.u64End) - RT_MIN(Args0.u64Start, Args1.u64Start);
                                        uint64_t u64Speed = (uint64_t)((2 * TEST_TRANSFER_SIZE / 1024) / (u64Elapsed / 1000000000.0));
                                        RTPrintf("tstIntNetR0: transfered %d bytes in %RU64 ns (%RU64 KB/s)\n",
                                                 2 * TEST_TRANSFER_SIZE, u64Elapsed, u64Speed);

                                        /*
                                         * Closing time...
                                         */
                                        rc = INTNETR0IfClose(pIntNet, hIf0);
                                        if (VBOX_SUCCESS(rc))
                                        {
                                            hIf0 = INTNET_HANDLE_INVALID;
                                            pBuf0 = NULL;
                                        }
                                        else
                                        {
                                            RTPrintf("tstIntNetR0: INTNETIfClose failed, rc=%Vrc! (hIf0)\n", rc);
                                            g_cErrors++;
                                        }
                                        rc = INTNETR0IfClose(pIntNet, hIf1);
                                        if (VBOX_SUCCESS(rc))
                                        {
                                            hIf1 = INTNET_HANDLE_INVALID;
                                            pBuf1 = NULL;
                                        }
                                        else
                                        {
                                            RTPrintf("tstIntNetR0: INTNETIfClose failed, rc=%Vrc! (hIf1)\n", rc);
                                            g_cErrors++;
                                        }

                                        rc = RTThreadWait(ThreadRecv0, 5000, &rc2);
                                        if (VBOX_FAILURE(rc) || VBOX_FAILURE(rc2))
                                        {
                                            RTPrintf("tstIntNetR0: Failed waiting on receiver thread 0, rc=%Vrc, rc2=%Vrc\n", rc, rc2);
                                            g_cErrors++;
                                        }

                                        rc = RTThreadWait(ThreadRecv1, 5000, &rc2);
                                        if (VBOX_FAILURE(rc) || VBOX_FAILURE(rc2))
                                        {
                                            RTPrintf("tstIntNetR0: Failed waiting on receiver thread 1, rc=%Vrc, rc2=%Vrc\n", rc, rc2);
                                            g_cErrors++;
                                        }

                                        /* check if the network still exist... */
                                        if (pIntNet->pNetworks)
                                        {
                                            RTPrintf("tstIntNetR0: The network wasn't deleted! (g_cErrors=%d)\n", g_cErrors);
                                            g_cErrors++;
                                        }
                                    }
                                    else
                                    {
                                        RTPrintf("tstIntNetR0: Waiting on senders failed, rc=%Vrc, rc2=%Vrc\n", rc, rc2);
                                        g_cErrors++;
                                    }
                                }
                                else
                                {
                                    RTPrintf("tstIntNetR0: Failed to create threads, rc=%Vrc\n", rc);
                                    g_cErrors++;
                                }
                            }
                        }
                        else
                        {
                            RTPrintf("tstIntNetR0: INTNETIfWait returned %Vrc expected VINF_SUCCESS (hIf1)\n", rc);
                            g_cErrors++;
                        }
                    }
                    else
                    {
                        RTPrintf("tstIntNetR0: INTNETIfSend returned %Vrc! (hIf0)\n", rc);
                        g_cErrors++;
                    }
                }
                else
                {
                    RTPrintf("tstIntNetR0: INTNETOpen returned invalid handle on success! (hIf1)\n");
                    g_cErrors++;
                }
            }
            else
            {
                RTPrintf("tstIntNetR0: INTNETOpen failed for the 2nd interface! rc=%Vrc\n", rc);
                g_cErrors++;
            }
        }
        else
        {
            RTPrintf("tstIntNetR0: INTNETOpen returned invalid handle on success! (hIf0)\n");
            g_cErrors++;
        }
    }
    else
    {
        RTPrintf("tstIntNetR0: INTNETOpen failed for the 1st interface! rc=%Vrc\n", rc);
        g_cErrors++;
    }

    /*
     * Destroy the service.
     */
    INTNETR0Destroy(pIntNet);

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstIntNetR0: SUCCESS\n");
    else
        RTPrintf("tstIntNetR0: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

