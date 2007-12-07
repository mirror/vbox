/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - SMP TSC testcase.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/runtime.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct TSCDATA
{
    /** The TSC.  */
    uint64_t volatile   TSC;           
    /** The APIC ID. */
    uint8_t volatile    u8ApicId;
    /** Did it succeed? */
    bool volatile       fRead;
    /** Did it fail? */
    bool volatile       fFailed;
    /** The thread handle. */
    RTTHREAD            Thread;
} TSCDATA, *PTSCDATA;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The number of CPUs waiting on their user event semaphore. */
static volatile uint32_t g_cWaiting;
/** The number of CPUs ready (in spin) to do the TSC read. */
static volatile uint32_t g_cReady;
/** The variable the CPUs are spinning on.
 * 0: Spin. 
 * 1: Go ahead. 
 * 2: You're too late, back to square one. */ 
static volatile uint32_t g_u32Go;
/** The number of CPUs that managed to read the TSC. */
static volatile uint32_t g_cRead;
/** The number of CPUs that failed to read the TSC. */
static volatile uint32_t g_cFailed;

/** Indicator forcing the threads to quit. */
static volatile bool g_fDone;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) ThreadFunction(RTTHREAD Thread, void *pvUser);


/**
 * Thread function for catching the other cpus. 
 *  
 * @returns VINF_SUCCESS (we don't care).
 * @param   Thread  The thread handle.
 * @param   pvUser  PTSCDATA.
 */
static DECLCALLBACK(int) ThreadFunction(RTTHREAD Thread, void *pvUser)
{
    PTSCDATA pTscData = (PTSCDATA)pvUser;

    while (!g_fDone)
    {
        /*
         * Wait.
         */
        ASMAtomicIncU32(&g_cWaiting);
        RTThreadUserWait(Thread, RT_INDEFINITE_WAIT);
        RTThreadUserReset(Thread);
        ASMAtomicDecU32(&g_cWaiting);
        if (g_fDone)
            break;

        /*
         * Spin.
         */
        ASMAtomicIncU32(&g_cReady);
        while (!g_fDone)
        {
            const uint8_t   ApicId1 = ASMGetApicId();
            const uint64_t  TSC1    = ASMReadTSC();
            const uint32_t  u32Go   = g_u32Go;
            if (u32Go == 0) 
                continue;

            if (u32Go == 1)
            {
                /* do the reading. */
                const uint8_t   ApicId2 = ASMGetApicId();
                const uint64_t  TSC2    = ASMReadTSC();
                const uint8_t   ApicId3 = ASMGetApicId();
                const uint64_t  TSC3    = ASMReadTSC();
                const uint8_t   ApicId4 = ASMGetApicId();

                if (    ApicId1 == ApicId2 
                    &&  ApicId1 == ApicId3
                    &&  ApicId1 == ApicId4
                    &&  TSC3 - TSC1 < 2250 /* WARNING: This is just a guess, increase if it doesn't work for you. */
                    &&  TSC2 - TSC1 < TSC3 - TSC1
                    )
                {
                    /* succeeded. */
                    pTscData->TSC = TSC2;
                    pTscData->u8ApicId = ApicId1;
                    pTscData->fFailed = false;
                    pTscData->fRead = true;
                    ASMAtomicIncU32(&g_cRead);
                    break;
                }
            }

            /* failed */
            pTscData->fFailed = true;
            pTscData->fRead = false;
            ASMAtomicIncU32(&g_cFailed);
            break;
        }
    }
    
    return VINF_SUCCESS;
}


int main()
{
    RTR3Init();

    /*
     * This is only relevant to on SMP systems.
     */
    const unsigned cCpus = RTSystemProcessorGetCount();
    if (cCpus <= 1) 
    {
        RTPrintf("tstTSC: SKIPPED - Only relevant on SMP systems\n");
        return 0;
    }

    /*
     * Create the threads.
     */
    static TSCDATA s_aData[254];
    uint32_t i;
    if (cCpus > RT_ELEMENTS(s_aData)) 
    {
        RTPrintf("tstTSC: FAILED - too many CPUs (%u)\n", cCpus);
        return 1;
    }

    /* ourselves. */
    s_aData[0].Thread = RTThreadSelf();

    /* the others */
    for (i = 1; i < cCpus; i++)
    {
        int rc = RTThreadCreate(&s_aData[i].Thread, ThreadFunction, &s_aData[i], 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "OTHERCPU");
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstTSC: FAILURE - RTThreatCreate failed when creating thread #%u, rc=%Rrc!\n", i, rc);
            ASMAtomicXchgSize(&g_fDone, true);
            while (i-- > 1)
            {
                RTThreadUserSignal(s_aData[i].Thread);
                RTThreadWait(s_aData[i].Thread, 5000, NULL);
            }
            return 1;
        }
    }

    /*
     * Retry untill we get lucky (or give up).
     */
    for (unsigned cTries = 0; ; cTries++)
    {
        if (cTries > 10240)
        {
            RTPrintf("tstTSC: FAILURE - %d attempts, giving.\n", cTries);
            break;
        }

        /*
         * Wait for the other threads to get ready (brute force active wait, I'm lazy).
         */
        i = 0;
        while (g_cWaiting < cCpus - 1)
        {
            if (i++ > _2G32)
                break;
            RTThreadSleep(i & 0xf);
        }
        if (g_cWaiting != cCpus - 1)
        {
            RTPrintf("tstTSC: FAILURE - threads failed to get waiting (%d != %d (i=%d))\n", g_cWaiting + 1, cCpus, i);
            break;
        }

        /*
         * Send them spinning.
         */
        ASMAtomicXchgU32(&g_cReady, 0);
        ASMAtomicXchgU32(&g_u32Go, 0);
        ASMAtomicXchgU32(&g_cRead, 0);
        ASMAtomicXchgU32(&g_cFailed, 0);
        for (i = 1; i < cCpus; i++)
        {
            ASMAtomicXchgSize(&s_aData[i].fFailed, false);
            ASMAtomicXchgSize(&s_aData[i].fRead, false);
            ASMAtomicXchgU8(&s_aData[i].u8ApicId, 0xff);

            int rc = RTThreadUserSignal(s_aData[i].Thread);
            if (RT_FAILURE(rc))
                RTPrintf("tstTSC: WARNING - RTThreadUserSignal(%#u) -> rc=%Rrc!\n", i, rc);
        }

        /* wait for them to get ready. */
        i = 0;
        while (g_cReady < cCpus - 1)
        {
            if (i++ > _2G32)
                break;
        }
        if (g_cReady != cCpus - 1)
        {
            RTPrintf("tstTSC: FAILURE - threads failed to get ready (%d != %d, i=%d)\n", g_cWaiting + 1, cCpus, i);
            break;
        }

        /* 
         * Flip the "go" switch and do our readings.
         * We give the other threads the slack it takes to two extra TSC and APIC ID reads.
         */
        const uint8_t   ApicId1 = ASMGetApicId();
        const uint64_t  TSC1    = ASMReadTSC();
        ASMAtomicXchgU32(&g_u32Go, 1);
        const uint8_t   ApicId2 = ASMGetApicId();
        const uint64_t  TSC2    = ASMReadTSC();
        const uint8_t   ApicId3 = ASMGetApicId();
        const uint64_t  TSC3    = ASMReadTSC();
        const uint8_t   ApicId4 = ASMGetApicId();
        const uint64_t  TSC4    = ASMReadTSC();
        ASMAtomicXchgU32(&g_u32Go, 2);
        const uint8_t   ApicId5 = ASMGetApicId();
        const uint64_t  TSC5    = ASMReadTSC();
        const uint8_t   ApicId6 = ASMGetApicId();

        /* Compose our own result. */
        if (    ApicId1 == ApicId2 
            &&  ApicId1 == ApicId3
            &&  ApicId1 == ApicId4
            &&  ApicId1 == ApicId5
            &&  ApicId1 == ApicId6
            &&  TSC5 - TSC1 < 2750  /* WARNING: This is just a guess, increase if it doesn't work for you. */
            &&  TSC4 - TSC1 < TSC5 - TSC1
            &&  TSC3 - TSC1 < TSC4 - TSC1
            &&  TSC2 - TSC1 < TSC3 - TSC1
            )
        {
            /* succeeded. */
            s_aData[0].TSC = TSC2;
            s_aData[0].u8ApicId = ApicId1;
            s_aData[0].fFailed = false;
            s_aData[0].fRead = true;
            ASMAtomicIncU32(&g_cRead);
        }
        else
        {
            /* failed */
            s_aData[0].fFailed = true;
            s_aData[0].fRead = false;
            ASMAtomicIncU32(&g_cFailed);
        }

        /*
         * Wait a little while to let the other ones to finish.
         */
        i = 0;
        while (g_cRead + g_cFailed < cCpus)
        {
            if (i++ > _2G32)
                break;
            if (i > _1M)
                RTThreadSleep(i & 0xf);
        }
        if (g_cRead + g_cFailed != cCpus)
        {
            RTPrintf("tstTSC: FAILURE - threads failed to complete reading (%d + %d != %d)\n", g_cRead, g_cFailed, cCpus);
            break;
        }

        /*
         * If everone succeeded, print the results.
         */
        if (!g_cFailed)
        {
            RTPrintf(" #  ID  TSC\n"
                     "-----------------------\n");
            for (i = 0; i < cCpus; i++)
                RTPrintf("%2d  %02x %RX64\n", i, s_aData[i].u8ApicId, s_aData[i].TSC);
            RTPrintf("(Needed %u attempt%s.)\n", cTries + 1, cTries ? "s" : "");
            break;
        }
    }

    /*
     * Destroy the threads.
     */
    ASMAtomicXchgSize(&g_fDone, true);
    for (i = 1; i < cCpus; i++)
    {
        int rc = RTThreadUserSignal(s_aData[i].Thread);
        if (RT_FAILURE(rc))
            RTPrintf("tstTSC: WARNING - RTThreadUserSignal(%#u) -> rc=%Rrc! (2)\n", i, rc);
    }
    for (i = 1; i < cCpus; i++)
    {
        int rc = RTThreadWait(s_aData[i].Thread, 5000, NULL);
        if (RT_FAILURE(rc))
            RTPrintf("tstTSC: WARNING - RTThreadWait(%#u) -> rc=%Rrc!\n", i, rc);
    }

    return g_cFailed != 0 || g_cRead != cCpus;
}
