/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the Global Info Page interface
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#include <iprt/runtime.h>


int main(void)
{
    RTR3Init();

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
                     "tstGIP-2: it: u64NanoTS        u64TSC           UpIntTSC H TransId            CpuHz TSC Interval History...\n",
                     g_pSUPGlobalInfoPage->u32UpdateHz,
                     g_pSUPGlobalInfoPage->u32UpdateIntervalNS,
                     g_pSUPGlobalInfoPage->u64NanoTSLastUpdateHz, 
                     g_pSUPGlobalInfoPage->u32Mode,
                     g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_SYNC_TSC       ? "sync"
                     : g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_ASYNC_TSC    ? "async"
                     :                                                            "???",
                     g_pSUPGlobalInfoPage->u32Version);
            for (int i = 0; i < 80; i++)
            {
                RTPrintf("tstGIP-2: %2d: %016llx %016llx %08x %d %08x %15llu %08x %08x %08x %08x %08x %08x %08x %08x\n",
                         i,
                         g_pSUPGlobalInfoPage->aCPUs[0].u64NanoTS,
                         g_pSUPGlobalInfoPage->aCPUs[0].u64TSC,
                         g_pSUPGlobalInfoPage->aCPUs[0].u32UpdateIntervalTSC,
                         g_pSUPGlobalInfoPage->aCPUs[0].iTSCHistoryHead,
                         g_pSUPGlobalInfoPage->aCPUs[0].u32TransactionId,
                         g_pSUPGlobalInfoPage->aCPUs[0].u64CpuHz,
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[0],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[1],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[2],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[3],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[4],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[5],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[6],
                         g_pSUPGlobalInfoPage->aCPUs[0].au32TSCHistory[7]);
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
