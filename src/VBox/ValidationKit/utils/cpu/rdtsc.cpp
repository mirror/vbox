/* $Id$ */
/** @file
 * rdtsc - Test if three consecutive rdtsc instructions return different values.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct RDTSCRESULT
{
    RTCCUINTREG uLow, uHigh;
} RDTSCRESULT;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern "C" RDTSCRESULT g_aRdTscResults[]; /* rdtsc-asm.asm */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/**
 * Does 3 (32-bit) or 6 (64-bit) fast TSC reads and stores the result
 * in g_aRdTscResults, starting with the 2nd entry.
 *
 * Starting the result storing at g_aRdTscResults[1] make it easy to do the
 * comparisons in a loop.
 *
 * @returns Number of results read into g_aRdTscResults[1] and onwards.
 */
DECLASM(uint32_t) DoTscReads(void);




int main(int argc, char **argv)
{

    /*
     * Tunables.
     */
    uint64_t        offJumpThreshold  = _4G * 2;
    unsigned        cMaxLoops         = 10000000;
    unsigned        cStatusEvery      = 2000000;
    unsigned        cMinSeconds       = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *psz = argv[i];
        if (*psz == '-')
        {
            psz++;
            char chOpt;
            while ((chOpt = *psz++) != '\0')
            {
                /* Option value. */
                const char *pszValue = NULL;
                unsigned long uValue = 0;
                switch (chOpt)
                {
                    case 'l':
                    case 's':
                    case 'm':
                        if (*psz == '\0')
                        {
                            if (i + 1 >= argc)
                            {
                                printf("syntax error: The %c option requires a value\n", chOpt);
                                return RTEXITCODE_SYNTAX;
                            }
                            pszValue = argv[++i];
                        }
                        else
                            pszValue = psz + (*psz == ':' || *psz == '=');
                        switch (chOpt)
                        {
                            case 'l':
                            case 's':
                            case 'm':
                            {
                                char *pszNext = NULL;
                                uValue = strtoul(pszValue, &pszNext, 0);
                                if (pszNext && *pszNext != '\0')
                                {
                                    if (*pszNext == 'M'&& pszNext[1] == '\0')
                                        uValue *= _1M;
                                    else if (*pszNext == 'K' && pszNext[1] == '\0')
                                        uValue *= _1K;
                                    else if (*pszNext == 'G' && pszNext[1] == '\0')
                                        uValue *= _1G;
                                    else
                                    {
                                        printf("syntax error: Bad value format for option %c: %s\n", chOpt, pszValue);
                                        return RTEXITCODE_SYNTAX;
                                    }
                                }
                                break;
                            }
                        }
                        break;
                }

                /* handle the option. */
                switch (chOpt)
                {
                    case 'l':
                        cMaxLoops = uValue;
                        break;

                    case 'm':
                        cMinSeconds = uValue;
                        break;

                    case 's':
                        cStatusEvery = uValue;
                        break;

                    case 'h':
                    case '?':
                        printf("usage: rdtsc [-l <loops>] [-s <loops-between-status>]\n"
                               "             [-m <minimum-seconds-to-run>]\n");
                        return RTEXITCODE_SUCCESS;

                    default:
                        printf("syntax error: Unknown option %c (argument %d)\n", chOpt, i);
                        return RTEXITCODE_SYNTAX;
                }
            }
        }
        else
        {
            printf("synatx error: argument %d (%s): not an option\n", i, psz);
            return RTEXITCODE_SYNTAX;
        }
    }

    /*
     * Do the job.
     */
    time_t          uSecStart;
    time(&uSecStart);
    unsigned        cOuterLoops        = 0;
    unsigned        cLoopsToNextStatus = cStatusEvery;
    unsigned        cRdTscInstructions = 0;
    unsigned        cBackwards         = 0;
    unsigned        cSame              = 0;
    unsigned        cBadValues         = 0;
    unsigned        cJumps             = 0;
    uint64_t        offMaxJump         = 0;
    uint64_t        offMinIncr         = UINT64_MAX;
    uint64_t        offMaxIncr         = 0;

    g_aRdTscResults[0] = g_aRdTscResults[DoTscReads() - 1];

    for (;;)
    {
        for (unsigned iLoop = 0; iLoop < cMaxLoops; iLoop++)
        {
            uint32_t const cResults = DoTscReads();
            cRdTscInstructions += cResults;

            for (uint32_t i = 0; i < cResults; i++)
            {
                uint64_t uPrev = RT_MAKE_U64((uint32_t)g_aRdTscResults[i    ].uLow, (uint32_t)g_aRdTscResults[i    ].uHigh);
                uint64_t uCur  = RT_MAKE_U64((uint32_t)g_aRdTscResults[i + 1].uLow, (uint32_t)g_aRdTscResults[i + 1].uHigh);
                if (RT_LIKELY(uCur != uPrev))
                {
                    int64_t offDelta = uCur - uPrev;
                    if (RT_LIKELY(offDelta >= 0))
                    {
                        if (RT_LIKELY((uint64_t)offDelta < offJumpThreshold))
                        {
                            if ((uint64_t)offDelta < offMinIncr)
                                offMinIncr = offDelta;
                            if ((uint64_t)offDelta > offMaxIncr && i != 0)
                                offMaxIncr = offDelta;
                        }
                        else
                        {
                            cJumps++;
                            if ((uint64_t)offDelta > offMaxJump)
                                offMaxJump = offDelta;
                            printf("%u/%u: Jump: %#010x`%08x -> %#010x`%08x\n", cOuterLoops, iLoop,
                                   (unsigned)g_aRdTscResults[i].uHigh, (unsigned)g_aRdTscResults[i].uLow,
                                   (unsigned)g_aRdTscResults[i + 1].uHigh, (unsigned)g_aRdTscResults[i + 1].uLow);
                        }
                    }
                    else
                    {
                        cBackwards++;
                        printf("%u/%u: Back: %#010x`%08x -> %#010x`%08x\n", cOuterLoops, iLoop,
                               (unsigned)g_aRdTscResults[i].uHigh, (unsigned)g_aRdTscResults[i].uLow,
                               (unsigned)g_aRdTscResults[i + 1].uHigh, (unsigned)g_aRdTscResults[i + 1].uLow);
                    }
                }
                else
                {
                    cSame++;
                    printf("%u/%u: Same: %#010x`%08x -> %#010x`%08x\n", cOuterLoops, iLoop,
                           (unsigned)g_aRdTscResults[i].uHigh, (unsigned)g_aRdTscResults[i].uLow,
                           (unsigned)g_aRdTscResults[i + 1].uHigh, (unsigned)g_aRdTscResults[i + 1].uLow);
                }
#if ARCH_BITS == 64
                if ((g_aRdTscResults[i + 1].uLow >> 32) || (g_aRdTscResults[i + 1].uHigh >> 32))
                    cBadValues++;
#endif
            }

            /* Copy the last value for the next iteration. */
            g_aRdTscResults[0] = g_aRdTscResults[cResults];

            /* Display status. */
            if (RT_LIKELY(--cLoopsToNextStatus > 0))
            { /* likely */ }
            else
            {
                cLoopsToNextStatus = cStatusEvery;
                printf("%u/%u: %#010x`%08x\n", cOuterLoops, iLoop,
                       (unsigned)g_aRdTscResults[cResults].uHigh, (unsigned)g_aRdTscResults[cResults].uLow);
            }
        }

        /*
         * Check minimum number of seconds.
         */
        cOuterLoops++;
        if (!cMinSeconds)
            break;
        time_t uSecNow;
        if (   time(&uSecNow) == (time_t)-1
            || uSecNow        == (time_t)-1
            || uSecStart      == (time_t)-1
            || uSecNow - uSecStart >= (time_t)cMinSeconds)
            break;
    }

    /*
     * Summary.
     */
    if (cBackwards == 0 && cSame == 0 && cJumps == 0 && cBadValues == 0)
    {
        printf("rdtsc: Success (%u RDTSC over %u*%u loops, deltas: %#x`%08x..%#x`%08x)\n",
               cRdTscInstructions, cOuterLoops, cMaxLoops,
               (unsigned)(offMinIncr >> 32), (unsigned)offMinIncr, (unsigned)(offMaxIncr >> 32), (unsigned)offMaxIncr);
        return RTEXITCODE_SUCCESS;
    }
    printf("RDTSC instructions: %u\n", cRdTscInstructions);
    printf("Loops:              %u * %u => %u\n", cMaxLoops, cOuterLoops, cOuterLoops * cMaxLoops);
    printf("Backwards:          %u\n", cBackwards);
    printf("Jumps:              %u\n", cJumps);
    printf("Max jumps:          %#010x`%08x\n", (unsigned)(offMaxJump >> 32), (unsigned)offMaxJump);
    printf("Same value:         %u\n", cSame);
    printf("Bad values:         %u\n", cBadValues);
    printf("Min increment:      %#010x`%08x\n", (unsigned)(offMinIncr >> 32), (unsigned)offMinIncr);
    printf("Max increment:      %#010x`%08x\n", (unsigned)(offMaxIncr >> 32), (unsigned)offMaxIncr);
    return RTEXITCODE_FAILURE;
}

