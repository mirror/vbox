/** @file
 *
 * VMM - Test program no.2.
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

DECLASM(int) tstProg2ForeverAsm(uint32_t *pEflags1, uint32_t *pEflags2);

CPUMCTX ctx1;
CPUMCTX ctx2;


int main(int argc, char **argv)
{
    int i;
    uint32_t eflags1, eflags2;

    /*
     * Dump input and name and such.
     */
    printf("*** tstProg-2\n");
    printf("%d Arguments:\n", argc);
    for (i = 0; i < argc; i++)
        printf("%d: %s\n", i, argv[i]);

    /*
     * This test program loops for ever doing 'nop'.
     */
    eflags1 = 0;
    eflags2 = 0;
    i = tstProg2ForeverAsm(&eflags1, &eflags2);
    printf("eflags1=%#x eflags2=%#x i=%d\n", eflags1, eflags2, i);
    return 1;
}
