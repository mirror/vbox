/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Support driver profiling testcase
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


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
PCSUPGLOBALINFOPAGE g_pSUPGlobalInfoPage = NULL;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
void VBOXCALL   supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS);
int rtTimeNanoTSInternal(uint64_t *pu64NanoTS);


/**
 * Attempt to trash the cache.
 */
static void TrashCache(void *pvBuffer)
{
    /*
     * Generate 1M-4 of movsd.
     */
    unsigned *puHead = (unsigned *)pvBuffer;
    unsigned cLeft = _1M / sizeof(unsigned);
    while (cLeft-- > 0)
        *puHead++ = 0xa5a5a5a5; /* movsd * 4 */
    cLeft = 128;
    while (cLeft-- > 0)
        *puHead++ = 0xc3c3c3c3; /* ret * 4 */
#ifdef _MSC_VER
    __asm
    {
        mov     eax, pvBuffer
        push    edi
        push    esi
        cld
        mov     edi, pvBuffer
        lea     esi, [edi + 256]
        call    eax
        pop     esi
        pop     edi
    }
#else
    __asm__ __volatile__(
        "cld\n\t"
        "movl   %0, %%edi\n\t"
        "leal   32(%%edi), %%esi\n\t"
        "call *%0"
        :: "r" (pvBuffer)
        : "esi", "edi", "ecx", "memory");
#endif
}


/* cut & past from SUPDRVShared.c */
/**
 * Updates the GIP.
 *
 * @param   pGip        Pointer to the GIP.
 * @param   u64NanoTS   The current nanosecond timesamp.
 */
void VBOXCALL   supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS)
{
    uint64_t    u64TSC;
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;

    /*
     * Start update transaction.
     */
    if (!(ASMAtomicIncU32(&pGip->u32TransactionId) & 1))
    {
        AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGip->u32TransactionId));
        ASMAtomicIncU32(&pGip->u32TransactionId);
        pGip->cErrors++;
    }

    ASMAtomicXchgU64(&pGip->u64NanoTS, u64NanoTS);

    /*
     * Calc TSC delta.
     */
    /** @todo validate the NanoTS delta, don't trust the OS to call us when it should... */
    u64TSC = ASMReadTSC();
    u64TSCDelta = u64TSC - pGip->u64TSC;
    ASMAtomicXchgU64(&pGip->u64TSC, u64TSC);

    if (u64TSCDelta >> 32)
    {
        u64TSCDelta = pGip->u32UpdateIntervalTSC;
        pGip->cErrors++;
    }

    /*
     * TSC History.
     */
    Assert(ELEMENTS(pGip->au32TSCHistory) == 8);

    iTSCHistoryHead = (pGip->iTSCHistoryHead + 1) & 7;
    ASMAtomicXchgU32(&pGip->iTSCHistoryHead, iTSCHistoryHead);
    ASMAtomicXchgU32(&pGip->au32TSCHistory[iTSCHistoryHead], (uint32_t)u64TSCDelta);

    /*
     * UpdateIntervalTSC = average of last 8,2,1 intervals depending on update HZ.
     */
    if (pGip->u32UpdateHz >= 1000)
    {
        uint32_t u32;
        u32  = pGip->au32TSCHistory[0];
        u32 += pGip->au32TSCHistory[1];
        u32 += pGip->au32TSCHistory[2];
        u32 += pGip->au32TSCHistory[3];
        u32 >>= 2;
        u32UpdateIntervalTSC  = pGip->au32TSCHistory[4];
        u32UpdateIntervalTSC += pGip->au32TSCHistory[5];
        u32UpdateIntervalTSC += pGip->au32TSCHistory[6];
        u32UpdateIntervalTSC += pGip->au32TSCHistory[7];
        u32UpdateIntervalTSC >>= 2;
        u32UpdateIntervalTSC += u32;
        u32UpdateIntervalTSC >>= 1;
    }
    else if (pGip->u32UpdateHz >= 100)
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
        u32UpdateIntervalTSC += pGip->au32TSCHistory[(iTSCHistoryHead - 1) & 7];
        u32UpdateIntervalTSC >>= 1;
    }
    else
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
    ASMAtomicXchgU32(&pGip->u32UpdateIntervalTSC, u32UpdateIntervalTSC);

    /*
     * CpuHz.
     */
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, pGip->u32UpdateHz);
    ASMAtomicXchgU64(&pGip->u64CpuHz, u64CpuHz);

    /*
     * Complete transaction.
     */
    if (ASMAtomicIncU32(&pGip->u32TransactionId) & 1)
    {
        AssertMsgFailed(("Invalid transaction id, %#x, not even!\n", pGip->u32TransactionId));
        ASMAtomicIncU32(&pGip->u32TransactionId);
        pGip->cErrors++;
    }
}


int main(void)
{
    PSUPGLOBALINFOPAGE  pGip;
    void               *pvBuffer;
    uint64_t            u64ElapsedTSC;
    uint64_t            u64MinTSC;
    int                 i;
    int                 cResample;

    RTR3Init(true);

    /*
     * Allocate cache killer buffer and the GIP page.
     * We allocate 8 MB, on 32-bit systems only half is used.
     */
    pvBuffer = RTMemExecAlloc(8 * _1M + PAGE_SIZE);
    if (!pvBuffer)
    {
        RTPrintf("tstGIP-1: Failed to allocate 8MB of memory.\n");
        return 1;
    }
    pGip = (PSUPGLOBALINFOPAGE)RTMemPageAllocZ(sizeof(*pGip));
    if (!pGip)
    {
        RTPrintf("tstGIP-1: Failed to allocate 1 PAGE for GIP.\n");
        return 1;
    }
    g_pSUPGlobalInfoPage = pGip;

    /*
     * Init the GIP.
     */
    pGip->u32Magic = SUPGLOBALINFOPAGE_MAGIC;
    pGip->u32UpdateHz = 100;
    pGip->u32UpdateIntervalNS = 1000000000 / pGip->u32UpdateHz;

    /*
     * Profile the updating.
     */
    RTPrintf("tstGIP-1: profiling update routine.\n");
    u64MinTSC = ~0;
    u64ElapsedTSC = 0;
    for (i = cResample = 0; i < 1024; i++)
    {
        uint64_t u64StartTSC;
        uint64_t u64DeltaTSC;

        TrashCache(pvBuffer);
        RTThreadYield();
        RTThreadYield();
        RTThreadYield();

        u64StartTSC = ASMReadTSC();
        supdrvGipUpdate(pGip, i * 1000000);
        u64DeltaTSC = ASMReadTSC() - u64StartTSC;

        if (u64MinTSC > u64DeltaTSC)
            u64MinTSC = u64DeltaTSC;

        if (    i
            &&  u64DeltaTSC >= u64MinTSC * 2
            &&  cResample-- > 0)
        {
            /*RTPrintf("tstGIP-1: i=%d, cResample=%d u64DeltaTSC=%llx\n", i, cResample, u64DeltaTSC);*/
            i--;
        }
        else
        {
            u64ElapsedTSC += u64DeltaTSC;
            cResample = 10;
        }
    }
    RTPrintf("tstGIP-1: Update: %llu / %llu (min/avg) ticks per update (%lld total for 1024)\n",
             u64MinTSC, u64ElapsedTSC / 1024, u64ElapsedTSC);

    /*
     * Profile the updating without cache trashing.
     */
    RTPrintf("tstGIP-1: profiling update routine (w/ cache).\n");
    u64MinTSC = ~0;
    u64ElapsedTSC = 0;
    for (i = cResample = 0; i < 1024; i++)
    {
        uint64_t u64StartTSC;
        uint64_t u64DeltaTSC;

        RTThreadYield();

        u64StartTSC = ASMReadTSC();
        supdrvGipUpdate(pGip, i * 1000000);
        u64DeltaTSC = ASMReadTSC() - u64StartTSC;

        if (u64MinTSC > u64DeltaTSC)
            u64MinTSC = u64DeltaTSC;

        if (    i
            &&  u64DeltaTSC >= u64MinTSC * 2
            &&  cResample-- > 0)
        {
            /*RTPrintf("tstGIP-1: i=%d, cResample=%d u64DeltaTSC=%llx\n", i, cResample, u64DeltaTSC);*/
            i--;
        }
        else
        {
            u64ElapsedTSC += u64DeltaTSC;
            cResample = 10;
        }
    }
    RTPrintf("tstGIP-1: Cached update: %llu / %llu (min/avg) ticks per update (%lld total for 1024)\n",
             u64MinTSC, u64ElapsedTSC / 1024, u64ElapsedTSC);

    return 0;
}
