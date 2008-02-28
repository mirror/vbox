/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Testcase for the No-CRT assembly bits.
 */

/*
 * Copyright (C) 2008 innotek GmbH
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
#include <iprt/nocrt/string.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#ifdef RT_WITHOUT_NOCRT_WRAPPERS
# error "Build error."
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned g_cErrors = 0;


static void my_memset(void *pv, int ch, size_t cb)
{
    uint8_t *pb = (uint8_t *)pv;
    while (cb--)
        *pb++ = ch;
}

static void my_memcheck(const void *pv, int ch, size_t cb, const char *pszDesc)
{
    uint8_t *pb = (uint8_t *)pv;
    while (cb--)
    {
        if (*pb != (uint8_t)ch)
        {
            g_cErrors++;
            size_t off;
            for (off = 1; off < cb & pb[off] != (uint8_t)ch; off++)
                /* nandemonai */;
            off--;
            pb += off;
            cb -= off;
            if (off)
                RTPrintf("tstNoCrt-1: %s: %p:%p - %02x instead of %02x\n",
                         pszDesc, (uintptr_t)pb - (uintptr_t)pv,
                         (uint8_t *)pb - (uint8_t *)pv + off, *pb, (uint8_t)ch);
            else
                RTPrintf("tstNoCrt-1: %s: %p - %02x instead of %02x\n",
                         pszDesc, (uint8_t *)pb - (uint8_t *)pv, *pb, (uint8_t)ch);
        }

        /* next*/
        pb++;
    }
}

#if 0 /* enable this to test the testcase. */
# undef RT_NOCRT
# define RT_NOCRT(a) a
#endif

int main()
{
    /*
     * Prologue.
     */
    RTR3Init();
    RTPrintf("tstNoCrt-1: TESTING...\n");

    /*
     * Sanity.
     */
    uint8_t abBuf1[8192];
    my_memset(abBuf1, 1, sizeof(abBuf1));
    my_memcheck(abBuf1, 1, sizeof(abBuf1), "sanity buf1");

    uint8_t abBuf2[8192];
    my_memset(abBuf2, 2, sizeof(abBuf2));
    my_memcheck(abBuf2, 2, sizeof(abBuf2), "sanity buf2");


    /*
     * memmove.
     */
    RTPrintf("tstNoCrt-1: memmove\n");
    my_memset(abBuf1, 1, sizeof(abBuf1));
    my_memset(abBuf2, 2, sizeof(abBuf2));
    RT_NOCRT(memmove)(abBuf1, abBuf2, sizeof(abBuf1));
    my_memcheck(abBuf1, 2, sizeof(abBuf1), "memmove1-dst");
    my_memcheck(abBuf2, 2, sizeof(abBuf2), "memmove1-src");

    my_memset(abBuf1, 3, sizeof(abBuf1));
    my_memset(abBuf2, 4, sizeof(abBuf2));
    RT_NOCRT(memmove)(abBuf2, abBuf1, sizeof(abBuf2));
    my_memcheck(abBuf1, 3, sizeof(abBuf1), "memmove2-dst");
    my_memcheck(abBuf2, 3, sizeof(abBuf2), "memmove2-src");

    for (unsigned off1 = 1; off1 < 256; off1++)
    {
        for (unsigned off2 = 0; off2 < 256; off2++)
        {
            /* forward */
            char sz[32];
            RTStrPrintf(sz, sizeof(sz), "memmove4-%d-%d", off1, off2);
            my_memset(abBuf1, off1 + 1, sizeof(abBuf1));
            my_memset(abBuf1, off1, off1);
            RT_NOCRT(memmove)(abBuf1, &abBuf1[off2], sizeof(abBuf1) - off2);
            if (off2 < off1)
            {
                unsigned cbLead = off1 - off2;
                my_memcheck(abBuf1, off1, cbLead, sz);
                my_memcheck(&abBuf1[cbLead], off1 + 1, sizeof(abBuf1) - cbLead, sz);
            }
            else
                my_memcheck(abBuf1, off1 + 1, sizeof(abBuf1), sz);

            /* backward */
            RTStrPrintf(sz, sizeof(sz), "memmove5-%d-%d", off1, off2);
            my_memset(abBuf1, off1 + 1, sizeof(abBuf1));
            my_memset(&abBuf1[sizeof(abBuf1) - off1], off1, off1);
            RT_NOCRT(memmove)(&abBuf1[off2], abBuf1, sizeof(abBuf1) - off2);
            if (off2 < off1)
            {
                unsigned cbLead = off1 - off2;
                my_memcheck(&abBuf1[sizeof(abBuf1) - cbLead], off1, cbLead, sz);
                my_memcheck(abBuf1, off1 + 1, sizeof(abBuf1) - cbLead, sz);
            }
            else
                my_memcheck(abBuf1, off1 + 1, sizeof(abBuf1), sz);
        }
    }

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstNoCrt-1: SUCCESS\n");
    else
        RTPrintf("tstNoCrt-1: FAILURE - %d errors\n", g_cErrors);
    return !!g_cErrors;
}

