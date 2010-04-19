/* $Id$ */
/** @file
 * IPRT R0 Testcase - Mutex Semaphores.
 */

/*
 * Copyright (C) 2009-2010 Sun Microsystems, Inc.
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
#include <iprt/semaphore.h>

#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <VBox/sup.h>
#include "tstRTR0SemMutex.h"



/**
 * Service request callback function.
 *
 * @returns VBox status code.
 * @param   pSession    The caller's session.
 * @param   u64Arg      64-bit integer argument.
 * @param   pReqHdr     The request header. Input / Output. Optional.
 */
DECLEXPORT(int) TSTRTR0SemMutexSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation,
                                             uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr)
{
    if (u64Arg)
        return VERR_INVALID_PARAMETER;
    if (!VALID_PTR(pReqHdr))
        return VERR_INVALID_PARAMETER;
    char   *pszErr = (char *)(pReqHdr + 1);
    size_t  cchErr = pReqHdr->cbReq - sizeof(*pReqHdr);
    if (cchErr < 32 || cchErr >= 0x10000)
        return VERR_INVALID_PARAMETER;
    *pszErr = '\0';

#define SET_ERROR(szFmt)                do { if (!*pszErr) RTStrPrintf(pszErr, cchErr, "!" szFmt); } while (0)
#define SET_ERROR1(szFmt, a1)           do { if (!*pszErr) RTStrPrintf(pszErr, cchErr, "!" szFmt, a1); } while (0)
#define SET_ERROR2(szFmt, a1, a2)       do { if (!*pszErr) RTStrPrintf(pszErr, cchErr, "!" szFmt, a1, a2); } while (0)
#define SET_ERROR3(szFmt, a1, a2, a3)   do { if (!*pszErr) RTStrPrintf(pszErr, cchErr, "!" szFmt, a1, a2, a3); } while (0)
#define CHECK_RC_BREAK(rc, rcExpect, szOp) \
        if ((rc) != (rcExpect)) \
        { \
            RTStrPrintf(pszErr, cchErr, "!%s -> %Rrc, expected %Rrc. line %u", szOp, rc, rcExpect, __LINE__); \
            break; \
        }


    /*
     * The big switch.
     */
    RTSEMMUTEX  hMtx;
    int         rc;
    switch (uOperation)
    {
        case TSTRTR0SEMMUTEX_SANITY_OK:
            break;

        case TSTRTR0SEMMUTEX_SANITY_FAILURE:
            SET_ERROR1("42failure42%1024s", "");
            break;

        case TSTRTR0SEMMUTEX_BASIC:
            rc = RTSemMutexCreate(&hMtx);
            CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexCreate");
            do
            {
                /*
                 * The interruptible version first.
                 */
                /* simple request and release, polling. */
                rc = RTSemMutexRequestNoResume(hMtx, 0);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* simple request and release, wait for ever. */
                rc = RTSemMutexRequestNoResume(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume(indef_wait)");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* simple request and release, wait a tiny while. */
                rc = RTSemMutexRequestNoResume(hMtx, 133);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume(133)");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* nested request and release. */
                rc = RTSemMutexRequestNoResume(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume#1");
                rc = RTSemMutexRequestNoResume(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume#2");
                rc = RTSemMutexRequestNoResume(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume#3");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#3");
                rc = RTSemMutexRequestNoResume(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequestNoResume#3b");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#3b");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#2");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#1");

                /*
                 * The uninterruptible variant.
                 */
                /* simple request and release, polling. */
                rc = RTSemMutexRequest(hMtx, 0);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* simple request and release, wait for ever. */
                rc = RTSemMutexRequest(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest(indef_wait)");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* simple request and release, wait a tiny while. */
                rc = RTSemMutexRequest(hMtx, 133);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest(133)");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease");

                /* nested request and release. */
                rc = RTSemMutexRequest(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest#1");
                rc = RTSemMutexRequest(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest#2");
                rc = RTSemMutexRequest(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest#3");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#3");
                rc = RTSemMutexRequest(hMtx, RT_INDEFINITE_WAIT);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRequest#3b");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#3b");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#2");
                rc = RTSemMutexRelease(hMtx);
                CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexRelease#1");

            } while (false);

            rc = RTSemMutexDestroy(hMtx);
            CHECK_RC_BREAK(rc, VINF_SUCCESS, "RTSemMutexDestroy");
            break;

        default:
            SET_ERROR1("Unknown test #%d", uOperation);
            break;
    }

    /* The error indicator is the '!' in the message buffer. */
    return VINF_SUCCESS;
}

