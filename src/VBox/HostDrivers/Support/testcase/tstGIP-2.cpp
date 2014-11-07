/* $Id$ */
/** @file
 * SUP Testcase - Global Info Page interface (ring 3).
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>


int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    /*
     * Parse args
     */
    static const RTGETOPTDEF g_aOptions[] =
    {
        { "--iterations",       'i', RTGETOPT_REQ_INT32 },
        { "--hex",              'h', RTGETOPT_REQ_NOTHING },
        { "--decimal",          'd', RTGETOPT_REQ_NOTHING },
        { "--spin",             's', RTGETOPT_REQ_NOTHING }
    };

    uint32_t cIterations = 40;
    bool fHex = true;
    bool fSpin = false;
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'i':
                cIterations = ValueUnion.u32;
                break;

            case 'd':
                fHex = false;
                break;

            case 'h':
                fHex = true;
                break;

            case 's':
                fSpin = true;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    /*
     * Init
     */
    PSUPDRVSESSION pSession = NIL_RTR0PTR;
    int rc = SUPR3Init(&pSession);
    if (RT_SUCCESS(rc))
    {
        if (g_pSUPGlobalInfoPage)
        {
            RTPrintf("tstGIP-2: cCpus=%d  u32UpdateHz=%RU32  u32UpdateIntervalNS=%RU32  u64NanoTSLastUpdateHz=%RX64  u64CpuHz=%RU64  u32Mode=%d (%s) u32Version=%#x\n",
                     g_pSUPGlobalInfoPage->cCpus,
                     g_pSUPGlobalInfoPage->u32UpdateHz,
                     g_pSUPGlobalInfoPage->u32UpdateIntervalNS,
                     g_pSUPGlobalInfoPage->u64NanoTSLastUpdateHz,
                     g_pSUPGlobalInfoPage->u64CpuHz,
                     g_pSUPGlobalInfoPage->u32Mode,
                     SUPGetGIPModeName(g_pSUPGlobalInfoPage),
                     g_pSUPGlobalInfoPage->u32Version);
            RTPrintf(fHex
                     ? "tstGIP-2:     it: u64NanoTS        delta     u64TSC           UpIntTSC H  TransId      CpuHz      TSC Interval History...\n"
                     : "tstGIP-2:     it: u64NanoTS        delta     u64TSC             UpIntTSC H    TransId      CpuHz      TSC Interval History...\n");
            static SUPGIPCPU s_aaCPUs[2][256];
            for (uint32_t i = 0; i < cIterations; i++)
            {
                /* copy the data */
                memcpy(&s_aaCPUs[i & 1][0], &g_pSUPGlobalInfoPage->aCPUs[0], g_pSUPGlobalInfoPage->cCpus * sizeof(g_pSUPGlobalInfoPage->aCPUs[0]));

                /* display it & find something to spin on. */
                uint32_t u32TransactionId = 0;
                uint32_t volatile *pu32TransactionId = NULL;
                for (unsigned iCpu = 0; iCpu < g_pSUPGlobalInfoPage->cCpus; iCpu++)
                    if (    g_pSUPGlobalInfoPage->aCPUs[iCpu].u64CpuHz > 0
                        &&  g_pSUPGlobalInfoPage->aCPUs[iCpu].u64CpuHz != _4G + 1)
                    {
                        PSUPGIPCPU pPrevCpu = &s_aaCPUs[!(i & 1)][iCpu];
                        PSUPGIPCPU pCpu = &s_aaCPUs[i & 1][iCpu];
                        RTPrintf(fHex
                                 ? "tstGIP-2: %4d/%d: %016llx %09llx %016llx %08x %d %08x %15llu %08x %08x %08x %08x %08x %08x %08x %08x (%d)\n"
                                 : "tstGIP-2: %4d/%d: %016llu %09llu %016llu %010u %d %010u %15llu %08x %08x %08x %08x %08x %08x %08x %08x (%d)\n",
                                 i, iCpu,
                                 pCpu->u64NanoTS,
                                 i ? pCpu->u64NanoTS - pPrevCpu->u64NanoTS : 0,
                                 pCpu->u64TSC,
                                 pCpu->u32UpdateIntervalTSC,
                                 pCpu->iTSCHistoryHead,
                                 pCpu->u32TransactionId,
                                 pCpu->u64CpuHz,
                                 pCpu->au32TSCHistory[0],
                                 pCpu->au32TSCHistory[1],
                                 pCpu->au32TSCHistory[2],
                                 pCpu->au32TSCHistory[3],
                                 pCpu->au32TSCHistory[4],
                                 pCpu->au32TSCHistory[5],
                                 pCpu->au32TSCHistory[6],
                                 pCpu->au32TSCHistory[7],
                                 pCpu->cErrors);
                        if (!pu32TransactionId)
                        {
                            pu32TransactionId = &g_pSUPGlobalInfoPage->aCPUs[iCpu].u32TransactionId;
                            u32TransactionId = pCpu->u32TransactionId;
                        }
                    }

                /* wait a bit / spin */
                if (!fSpin)
                    RTThreadSleep(9);
                else
                {
                    if (pu32TransactionId)
                    {
                        while (u32TransactionId == *pu32TransactionId)
                            ASMNopPause();
                    }
                    else
                        RTThreadSleep(1);
                }
            }

            /*
             * Display TSC deltas.
             *
             * First iterative over the APIC ID array to get mostly consistent CPUID to APIC ID mapping.
             * Then iterate over the offline CPUs. It is possible that there's a race between the online/offline
             * states between the two iterations, but that cannot be helped from ring-3 anyway and not a biggie.
             */
            RTPrintf("tstGIP-2: TSC deltas:\n");
            RTPrintf("tstGIP-2:  idApic: i64TSCDelta\n");
            for (unsigned i = 0; i < RT_ELEMENTS(g_pSUPGlobalInfoPage->aiCpuFromApicId); i++)
            {
                uint16_t iCpu = g_pSUPGlobalInfoPage->aiCpuFromApicId[i];
                if (iCpu != UINT16_MAX)
                {
                    RTPrintf("tstGIP-2: %7d: %lld\n", g_pSUPGlobalInfoPage->aCPUs[iCpu].idApic,
                             g_pSUPGlobalInfoPage->aCPUs[iCpu].i64TSCDelta);
                }
            }

            for (unsigned iCpu = 0; iCpu < g_pSUPGlobalInfoPage->cCpus; iCpu++)
                if (g_pSUPGlobalInfoPage->aCPUs[iCpu].idApic == UINT16_MAX)
                    RTPrintf("tstGIP-2: offline: %lld\n", g_pSUPGlobalInfoPage->aCPUs[iCpu].i64TSCDelta);
        }
        else
        {
            RTPrintf("tstGIP-2: g_pSUPGlobalInfoPage is NULL\n");
            rc = -1;
        }

        SUPR3Term(false /*fForced*/);
    }
    else
        RTPrintf("tstGIP-2: SUPR3Init failed: %Rrc\n", rc);
    return !!rc;
}
