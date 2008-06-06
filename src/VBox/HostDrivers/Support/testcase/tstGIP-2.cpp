/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the Global Info Page interface
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
#include <iprt/initterm.h>
#include <iprt/getopt.h>


int main(int argc, char **argv)
{
    RTR3Init();

    /*
     * Parse args
     */
    static const RTOPTIONDEF g_aOptions[] =
    {
        { "--interations",      'i', RTGETOPT_REQ_INT32 }
    };

    uint32_t cIterations = 40;
    int ch;
    int iArg = 1;
    RTOPTIONUNION ValueUnion;
    while ((ch = RTGetOpt(argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), &iArg, &ValueUnion)))
    {
        switch (ch)
        {
            case 'i':
                cIterations = ValueUnion.u32;
                break;

            default:
                if (ch < 0)
                    RTPrintf("tstGIP-2: %Rrc: %s\n", ch, ValueUnion.psz);
                else
                    RTPrintf("tstGIP-2: syntax error: %s\n", ValueUnion.psz);
                return 1;
        }
    }
    if (iArg < argc)
    {
        RTPrintf("tstGIP-2: syntax error: %s\n", ValueUnion.psz);
        return 1;
    }

    /*
     * Init
     */
    PSUPDRVSESSION pSession = NIL_RTR0PTR;
    int rc = SUPInit(&pSession);
    if (VBOX_SUCCESS(rc))
    {
        if (g_pSUPGlobalInfoPage)
        {
            RTPrintf("tstGIP-2: u32UpdateHz=%RU32  u32UpdateIntervalNS=%RU32  u64NanoTSLastUpdateHz=%RX64  u32Mode=%d (%s) u32Version=%#x\n"
                     "tstGIP-2:     it: u64NanoTS        u64TSC           UpIntTSC H TransId            CpuHz TSC Interval History...\n",
                     g_pSUPGlobalInfoPage->u32UpdateHz,
                     g_pSUPGlobalInfoPage->u32UpdateIntervalNS,
                     g_pSUPGlobalInfoPage->u64NanoTSLastUpdateHz,
                     g_pSUPGlobalInfoPage->u32Mode,
                     g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_SYNC_TSC       ? "sync"
                     : g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_ASYNC_TSC    ? "async"
                     :                                                            "???",
                     g_pSUPGlobalInfoPage->u32Version);
            for (int i = 0; i < cIterations; i++)
            {
                for (unsigned iCpu = 0; iCpu < RT_ELEMENTS(g_pSUPGlobalInfoPage->aCPUs); iCpu++)
                    if (    g_pSUPGlobalInfoPage->aCPUs[iCpu].u64CpuHz > 0
                        &&  g_pSUPGlobalInfoPage->aCPUs[iCpu].u64CpuHz != _4G + 1)
                        RTPrintf("tstGIP-2: %4d/%d: %016llx %016llx %08x %d %08x %15llu %08x %08x %08x %08x %08x %08x %08x %08x (%d)\n",
                                 i, iCpu,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].u64NanoTS,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].u64TSC,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].u32UpdateIntervalTSC,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].iTSCHistoryHead,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].u32TransactionId,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].u64CpuHz,
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[0],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[1],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[2],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[3],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[4],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[5],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[6],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].au32TSCHistory[7],
                                 g_pSUPGlobalInfoPage->aCPUs[iCpu].cErrors);
                RTThreadSleep(9);
            }
        }
        else
        {
            RTPrintf("tstGIP-2: g_pSUPGlobalInfoPage is NULL\n");
            rc = -1;
        }

        SUPTerm();
    }
    else
        RTPrintf("tstGIP-2: SUPInit failed: %Vrc\n", rc);
    return !!rc;
}
