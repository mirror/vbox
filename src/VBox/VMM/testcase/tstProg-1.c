/** @file
 *
 * VMM - Test program.
 *
 * This program will loop for ever constantly checking its state.
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
#include "tstHelp.h"
#include <stdio.h>

DECLASM(int) tstProg1ForeverAsm(PCPUMCTX pCtxOrg, PCPUMCTX pCtxCur);

CPUMCTX ctx1;
CPUMCTX ctx2;


int main(int argc, char **argv)
{
    int i;
    int rc;

    /*
     * Dump input and name and such.
     */
    printf("*** tstProg-1\n");
    printf("%d Arguments:\n", argc);
    for (i = 0; i < argc; i++)
        printf("%d: %s\n", i, argv[i]);
/*
    printf("CPUMCTX.fpu is at %d\n", RT_OFFSETOF(CPUMCTX, fpu));
    printf("CPUMCTX.eax is at %d\n", RT_OFFSETOF(CPUMCTX, eax));
    printf("CPUMCTX.ebx is at %d\n", RT_OFFSETOF(CPUMCTX, ebx));
    printf("CPUMCTX.edx is at %d\n", RT_OFFSETOF(CPUMCTX, edx));
    printf("CPUMCTX.ecx is at %d\n", RT_OFFSETOF(CPUMCTX, ecx));
    printf("CPUMCTX.eip is at %d\n", RT_OFFSETOF(CPUMCTX, eip));
    printf("CPUMCTX.cs is at %d\n",  RT_OFFSETOF(CPUMCTX, cs));
    printf("CPUMCTX.ds is at %d\n",  RT_OFFSETOF(CPUMCTX, ds));
    printf("CPUMCTX.fs is at %d\n",  RT_OFFSETOF(CPUMCTX, fs));
    printf("CPUMCTX.eflags is at %d\n", RT_OFFSETOF(CPUMCTX, eflags));
    printf("CPUMCTX.cr0 is at %d\n", RT_OFFSETOF(CPUMCTX, cr0));
    printf("CPUMCTX.dr0 is at %d\n", RT_OFFSETOF(CPUMCTX, dr0));
 */

    /*
     * This test program loops for ever doing 'nop'.
     */
    ctx1.cr0 = 1;
    ctx2.cr0 = 1;
    rc = tstProg1ForeverAsm(&ctx1, &ctx2);
    tstDumpCtx(&ctx1, "Expected context.");
    tstDumpCtx(&ctx2, "Actual context...");
    printf("rc=%#x\n", rc);
    if ((rc & 0xffff) >= 5 && (rc & 1))
    {
        int off = (rc & 0xffff) - 5;
        printf("difference at offset %d (%#x) %#x != %#x  iterations=%d\n", off, off,
               ((uint32_t*)&ctx1)[off / sizeof(uint32_t)],
               ((uint32_t*)&ctx2)[off / sizeof(uint32_t)],
               rc >> 16);
    }
    return 1;
}
