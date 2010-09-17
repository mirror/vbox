/* $Id$ */
/** @file
 * IPRT R0 Testcase - Timers.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include <iprt/mem.h>

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <VBox/sup.h>
#include "tstRTR0Timer.h"



/**
 * Service request callback function.
 *
 * @returns VBox status code.
 * @param   pSession    The caller's session.
 * @param   u64Arg      64-bit integer argument.
 * @param   pReqHdr     The request header. Input / Output. Optional.
 */
DECLEXPORT(int) TSTRTR0TimerSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation,
                                          uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr)
{
    if (!VALID_PTR(pReqHdr))
        return VERR_INVALID_PARAMETER;
    char   *pszErr = (char *)(pReqHdr + 1);
    size_t  cchErr = pReqHdr->cbReq - sizeof(*pReqHdr);
    if (cchErr < 32 || cchErr >= 0x10000)
        return VERR_INVALID_PARAMETER;
    *pszErr = '\0';

    if (u64Arg)
        return VERR_INVALID_PARAMETER;

    /*
     * The big switch.
     */
    switch (uOperation)
    {
        case TSTRTR0TIMER_SANITY_OK:
            break;

        case TSTRTR0TIMER_SANITY_FAILURE:
            RTStrPrintf(pszErr, cchErr, "!42failure42%1024s", "");
            break;

#if 0
        case TSTRTR0TIMER_BASIC:
        {
            int rc = RTR0MemUserCopyFrom(pbKrnlBuf, R3Ptr, PAGE_SIZE);
            if (rc == VINF_SUCCESS)
            {
                rc = RTR0MemUserCopyTo(R3Ptr, pbKrnlBuf, PAGE_SIZE);
                if (rc == VINF_SUCCESS)
                {
                    if (RTR0MemUserIsValidAddr(R3Ptr))
                    {
                        if (RTR0MemKernelIsValidAddr(pbKrnlBuf))
                        {
                            if (RTR0MemAreKrnlAndUsrDifferent())
                            {
                                RTStrPrintf(pszErr, cchErr, "RTR0MemAreKrnlAndUsrDifferent returns true");
                                if (!RTR0MemUserIsValidAddr((uintptr_t)pbKrnlBuf))
                                {
                                    if (!RTR0MemKernelIsValidAddr((void *)R3Ptr))
                                    {
                                        /* done */
                                    }
                                    else
                                        RTStrPrintf(pszErr, cchErr, "! #5 - RTR0MemKernelIsValidAddr -> true, expected false");
                                }
                                else
                                    RTStrPrintf(pszErr, cchErr, "! #5 - RTR0MemUserIsValidAddr -> true, expected false");
                            }
                            else
                                RTStrPrintf(pszErr, cchErr, "RTR0MemAreKrnlAndUsrDifferent returns false");
                        }
                        else
                            RTStrPrintf(pszErr, cchErr, "! #4 - RTR0MemKernelIsValidAddr -> false, expected true");
                    }
                    else
                        RTStrPrintf(pszErr, cchErr, "! #3 - RTR0MemUserIsValidAddr -> false, expected true");
                }
                else
                    RTStrPrintf(pszErr, cchErr, "! #2 - RTR0MemUserCopyTo -> %Rrc expected %Rrc", rc, VINF_SUCCESS);
            }
            else
                RTStrPrintf(pszErr, cchErr, "! #1 - RTR0MemUserCopyFrom -> %Rrc expected %Rrc", rc, VINF_SUCCESS);
            break;
        }

        case TSTRTR0TIMER_GOOD:
        {
            for (unsigned off = 0; off < 16 && !*pszErr; off++)
                for (unsigned cb = 0; cb < PAGE_SIZE - 16; cb++)
                    TEST_OFF_SIZE(off, cb, VINF_SUCCESS);
            break;
        }

        case TSTRTR0TIMER_BAD:
        {
            for (unsigned off = 0; off < 16 && !*pszErr; off++)
                for (unsigned cb = 0; cb < PAGE_SIZE - 16; cb++)
                    TEST_OFF_SIZE(off, cb, cb > 0 ? VERR_ACCESS_DENIED : VINF_SUCCESS);
            break;
        }

        case TSTRTR0TIMER_INVALID_ADDRESS:
        {
            if (    !RTR0MemUserIsValidAddr(R3Ptr)
                &&  RTR0MemKernelIsValidAddr((void *)R3Ptr))
            {
                for (unsigned off = 0; off < 16 && !*pszErr; off++)
                    for (unsigned cb = 0; cb < PAGE_SIZE - 16; cb++)
                        TEST_OFF_SIZE(off, cb, cb > 0 ? VERR_ACCESS_DENIED : VINF_SUCCESS); /* ... */
            }
            else
                RTStrPrintf(pszErr, cchErr, "RTR0MemUserIsValidAddr returns true");
            break;
        }
#endif

        default:
            RTStrPrintf(pszErr, cchErr, "!Unknown test #%d", uOperation);
            break;
    }

    /* The error indicator is the '!' in the message buffer. */
    return VINF_SUCCESS;
}

