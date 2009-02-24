/* $Id$ */
/** @file
 * Internal networking - Usermode testcase for the kernel mode bits.
 *
 * This is a bit hackish as we're mixing context here, however it is
 * very useful when making changes to the internal networking service.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define IN_INTNET_TESTCASE
#define IN_INTNET_R3
#include <VBox/cdefs.h>
#undef INTNETR0DECL
#define INTNETR0DECL INTNETR3DECL
#undef DECLR0CALLBACKMEMBER
#define DECLR0CALLBACKMEMBER(type, name, args) DECLR3CALLBACKMEMBER(type, name, args)
#include <VBox/types.h>
typedef void *MYPSUPDRVSESSION;
#define PSUPDRVSESSION  MYPSUPDRVSESSION

#include <VBox/intnet.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/initterm.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/getopt.h>


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
    uint16_t    au16[7];
} g_TestFrame0 = { { /* dst:*/ 0xffff, 0xffff, 0xffff, /*src:*/0x8086, 0, 0, 0x0800 } },
  g_TestFrame1 = { { /* dst:*/ 0, 0, 0,                /*src:*/0x8086, 0, 1, 0x0800 } };


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

INTNETR3DECL(int) SUPR0ObjAddRefEx(void *pvObj, PSUPDRVSESSION pSession, bool fNoBlocking)
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

INTNETR3DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession)
{
    return SUPR0ObjAddRefEx(pvObj, pSession, false);
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
        return VINF_OBJECT_DESTROYED;
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
    RTMAC Mac;
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
    PRTMAC pMacSrc = (PRTMAC)&abBuf[0];
    PRTMAC pMacDst = pMacSrc + 1;
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
        int rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, g_pSession, abBuf, cb);
#else
        INTNETSG Sg;
        intnetR0SgInitTemp(&Sg, abBuf, cb);
        int rc = intnetR0RingWriteFrame(pArgs->pBuf, &pArgs->pBuf->Send, &Sg, NULL);
        if (RT_SUCCESS(rc))
            rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, g_pSession, NULL, 0);
#endif
        if (RT_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("tstIntNetR0: Failed sending %d bytes, rc=%Rrc (%d)\n", cb, rc, INTNETRingGetWritable(&pArgs->pBuf->Send));
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
        int rc = INTNETR0IfSend(pArgs->pIntNet, pArgs->hIf, g_pSession, abBuf, sizeof(RTMAC) * 2 + sizeof(unsigned) * 4);
        if (RT_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("tstIntNetR0: send failed, rc=%Rrc\n", rc);
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
        int rc = INTNETR0IfWait(pArgs->pIntNet, pArgs->hIf, g_pSession, RT_INDEFINITE_WAIT);
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
                RTPrintf("tstIntNetR0: receiver thread %.6Rhxs got odd return value %Rrc! cbReceived=%u cLostFrames=%u iFrame=%u\n",
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
            unsigned cb = intnetR0RingReadFrame(pArgs->pBuf, &pArgs->pBuf->Recv, abBuf);
            unsigned *puFrame = (unsigned *)&abBuf[sizeof(RTMAC) * 2];

            /* check for termination frame. */
            if (    cb == sizeof(RTMAC) * 2 + sizeof(unsigned) * 4
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
            PRTMAC pMacSrc = (PRTMAC)&abBuf[0];
            PRTMAC pMacDst = pMacSrc + 1;
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

int main(int argc, char **argv)
{
    /*
     * Init the runtime and parse arguments.
     */
    RTR3Init();

    static RTGETOPTDEF const s_aOptions[] =
    {
        { "--recv-buffer",  'r', RTGETOPT_REQ_UINT32 },
        { "--send-buffer",  's', RTGETOPT_REQ_UINT32 },
    };

    uint32_t cbRecv = 32 * _1K;
    uint32_t cbSend = 1536*2;

    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &Value)))
        switch (ch)
        {
            case 'r':
                cbRecv = Value.u32;
                break;

            case 's':
                cbSend = Value.u32;
                break;

            case VINF_GETOPT_NOT_OPTION:
                RTPrintf("tstIntNetR0: invalid argument: %s\n", Value.psz);
                return 1;

            default:
                RTPrintf("tstIntNetR0: invalid argument: %s\n", Value.psz);
                return 1;
        }

    /*
     * Create an INTNET instance.
     */
    RTPrintf("tstIntNetR0: TESTING cbSend=%d cbRecv=%d ...\n", cbSend, cbRecv);
    PINTNET pIntNet;
    int rc = INTNETR0Create(&pIntNet);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNetR0: INTNETR0Create failed, rc=%Rrc\n");
        return 1;
    }

    /*
     * Create two interfaces.
     */
    INTNETIFHANDLE hIf0 = INTNET_HANDLE_INVALID;
    rc = INTNETR0Open(pIntNet, g_pSession, "test", kIntNetTrunkType_None, "", 0, 1536*2 + 4, 0x8000, &hIf0);
    if (RT_SUCCESS(rc))
    {
        if (hIf0 != INTNET_HANDLE_INVALID)
        {
            INTNETIFHANDLE hIf1 = INTNET_HANDLE_INVALID;
            rc = INTNETR0Open(pIntNet, g_pSession, "test", kIntNetTrunkType_None, NULL, 0, 1536*2 + 4, 0x8000, &hIf1);
            if (RT_SUCCESS(rc))
            {
                if (hIf1 != INTNET_HANDLE_INVALID)
                {
                    PINTNETBUF pBuf0;
                    rc = INTNETR0IfGetRing0Buffer(pIntNet, hIf0, g_pSession, &pBuf0);
                    if (RT_FAILURE(rc) || !pBuf0)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfGetRing0Buffer failed! pBuf0=%p rc=%Rrc\n", pBuf0, rc);
                        g_cErrors++;
                    }
                    PINTNETBUF pBuf1;
                    rc = INTNETR0IfGetRing0Buffer(pIntNet, hIf1, g_pSession, &pBuf1);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("tstIntNetR0: INTNETIfGetRing0Buffer failed! pBuf1=%p rc=%Rrc\n", pBuf1, rc);
                        g_cErrors++;
                    }

                    rc = INTNETR0IfSetActive(pIntNet, hIf0, g_pSession, true);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("tstIntNetR0: INTNETR0IfSetActive failed! rc=%Rrc\n", rc);
                        g_cErrors++;
                    }
                    rc = INTNETR0IfSetActive(pIntNet, hIf1, g_pSession, true);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("tstIntNetR0: INTNETR0IfSetActive failed! rc=%Rrc\n", rc);
                        g_cErrors++;
                    }


                    /*
                     * Test basic waiting.
                     */
                    rc = INTNETR0IfWait(pIntNet, hIf0, g_pSession, 1);
                    if (rc != VERR_TIMEOUT)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfWait returned %Rrc expected VERR_TIMEOUT (hIf0)\n", rc);
                        g_cErrors++;
                    }
                    rc = INTNETR0IfWait(pIntNet, hIf1, g_pSession, 0);
                    if (rc != VERR_TIMEOUT)
                    {
                        RTPrintf("tstIntNetR0: INTNETIfWait returned %Rrc expected VERR_TIMEOUT (hIf1)\n", rc);
                        g_cErrors++;
                    }

                    /*
                     * Send and receive.
                     */
                    rc = INTNETR0IfSend(pIntNet, hIf0, g_pSession, &g_TestFrame0, sizeof(g_TestFrame0));
                    if (RT_SUCCESS(rc))
                    {
                        rc = INTNETR0IfWait(pIntNet, hIf0, g_pSession, 1);
                        if (rc != VERR_TIMEOUT)
                        {
                            RTPrintf("tstIntNetR0: INTNETIfWait returned %Rrc expected VERR_TIMEOUT (hIf0, 2nd)\n", rc);
                            g_cErrors++;
                        }
                        rc = INTNETR0IfWait(pIntNet, hIf1, g_pSession, 0);
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
                            unsigned cb = intnetR0RingReadFrame(pBuf1, &pBuf1->Recv, abBuf);
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
                            rc = INTNETR0IfSend(pIntNet, hIf1, g_pSession, &g_TestFrame1, sizeof(g_TestFrame1));
                            if (RT_FAILURE(rc))
                            {
                                RTPrintf("tstIntNetR0: INTNETIfSend returned %Rrc! (hIf1)\n", rc);
                                g_cErrors++;
                            }


                            /*
                             * Start threaded testcase.
                             * Give it 5 mins to finish.
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
                                if (RT_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadRecv1, ReceiveThread, &Args1, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "RECV1");
                                if (RT_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadSend0, SendThread, &Args0, 0, RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "SEND0");
                                if (RT_SUCCESS(rc))
                                    rc = RTThreadCreate(&ThreadSend1, SendThread, &Args1, 0, RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "SEND1");
                                if (RT_SUCCESS(rc))
                                {
                                    int rc2 = VINF_SUCCESS;
                                    rc = RTThreadWait(ThreadSend0, 5*60*1000, &rc2);
#if 1 /** @todo it looks like I'm subject to some false wakeup calls here (2.6.23-gentoo-r3 amd64). See #3023.*/
                                    for (int cTries = 100; rc == VERR_TIMEOUT && cTries > 0; cTries--)
                                    {
                                        RTThreadSleep(1);
                                        rc = RTThreadWait(ThreadSend0, 1, &rc2);
                                    }
#endif
                                    AssertRC(rc);
                                    if (RT_SUCCESS(rc))
                                    {
                                        ThreadSend0 = NIL_RTTHREAD;
                                        rc = RTThreadWait(ThreadSend1, 5*60*1000, RT_SUCCESS(rc2) ? &rc2 : NULL);
#if 1 /** @todo it looks like I'm subject to some false wakeup calls here (2.6.23-gentoo-r3 amd64). See #3023.*/
                                        for (int cTries = 100; rc == VERR_TIMEOUT && cTries > 0; cTries--)
                                        {
                                            RTThreadSleep(1);
                                            rc = RTThreadWait(ThreadSend1, 1, &rc2);
                                        }
#endif
                                        AssertRC(rc);
                                        if (RT_SUCCESS(rc))
                                            ThreadSend1 = NIL_RTTHREAD;
                                    }
                                    if (    RT_SUCCESS(rc)
                                        &&  RT_SUCCESS(rc2))
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
                                        rc = RTThreadWait(ThreadRecv0, 5000, &rc2);
                                        if (RT_SUCCESS(rc))
                                            ThreadRecv0 = NIL_RTTHREAD;
                                        if (RT_FAILURE(rc) || RT_FAILURE(rc2))
                                        {
                                            RTPrintf("tstIntNetR0: Failed waiting on receiver thread 0, rc=%Rrc, rc2=%Rrc\n", rc, rc2);
                                            g_cErrors++;
                                        }

                                        rc = RTThreadWait(ThreadRecv1, 5000, &rc2);
                                        if (RT_SUCCESS(rc))
                                            ThreadRecv1 = NIL_RTTHREAD;
                                        if (RT_FAILURE(rc) || RT_FAILURE(rc2))
                                        {
                                            RTPrintf("tstIntNetR0: Failed waiting on receiver thread 1, rc=%Rrc, rc2=%Rrc\n", rc, rc2);
                                            g_cErrors++;
                                        }

                                        rc = INTNETR0IfClose(pIntNet, hIf0, g_pSession);
                                        if (RT_SUCCESS(rc))
                                        {
                                            hIf0 = INTNET_HANDLE_INVALID;
                                            pBuf0 = NULL;
                                        }
                                        else
                                        {
                                            RTPrintf("tstIntNetR0: INTNETIfClose failed, rc=%Rrc! (hIf0)\n", rc);
                                            g_cErrors++;
                                        }

                                        rc = INTNETR0IfClose(pIntNet, hIf1, g_pSession);
                                        if (RT_SUCCESS(rc))
                                        {
                                            hIf1 = INTNET_HANDLE_INVALID;
                                            pBuf1 = NULL;
                                        }
                                        else
                                        {
                                            RTPrintf("tstIntNetR0: INTNETIfClose failed, rc=%Rrc! (hIf1)\n", rc);
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
                                        RTPrintf("tstIntNetR0: Waiting on senders failed, rc=%Rrc, rc2=%Rrc\n", rc, rc2);
                                        g_cErrors++;
                                    }

                                    /*
                                     * Give them a chance to complete...
                                     */
                                    RTThreadWait(ThreadRecv0, 5000, NULL);
                                    RTThreadWait(ThreadRecv1, 5000, NULL);
                                    RTThreadWait(ThreadSend0, 5000, NULL);
                                    RTThreadWait(ThreadSend1, 5000, NULL);

                                }
                                else
                                {
                                    RTPrintf("tstIntNetR0: Failed to create threads, rc=%Rrc\n", rc);
                                    g_cErrors++;
                                }
                            }
                        }
                        else
                        {
                            RTPrintf("tstIntNetR0: INTNETIfWait returned %Rrc expected VINF_SUCCESS (hIf1)\n", rc);
                            g_cErrors++;
                        }
                    }
                    else
                    {
                        RTPrintf("tstIntNetR0: INTNETIfSend returned %Rrc! (hIf0)\n", rc);
                        g_cErrors++;
                    }
                }
                else
                {
                    RTPrintf("tstIntNetR0: INTNETOpen returned invalid handle on success! (hIf1)\n");
                    g_cErrors++;
                }

                if (hIf1 != INTNET_HANDLE_INVALID)
                    rc = INTNETR0IfClose(pIntNet, hIf1, g_pSession);
            }
            else
            {
                RTPrintf("tstIntNetR0: INTNETOpen failed for the 2nd interface! rc=%Rrc\n", rc);
                g_cErrors++;
            }

            if (hIf0 != INTNET_HANDLE_INVALID)
                rc = INTNETR0IfClose(pIntNet, hIf0, g_pSession);
        }
        else
        {
            RTPrintf("tstIntNetR0: INTNETOpen returned invalid handle on success! (hIf0)\n");
            g_cErrors++;
        }
    }
    else
    {
        RTPrintf("tstIntNetR0: INTNETOpen failed for the 1st interface! rc=%Rrc\n", rc);
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

