/** @file
 *
 * VMM - Test program #3.
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
#include <stdlib.h>



int main(int argc, char **argv)
{
    int i, j;
    uint32_t *pau32;

    /*
     * Dump input and name and such.
     */
    printf("*** tstProg-3\n");
    printf("%d Arguments:\n", argc);
    for (i = 0; i < argc; i++)
        printf("%d: %s\n", i, argv[i]);

    /*
     * This test program loops for ever doing 'nop'.
     */
    pau32 = malloc(9*1024*1024);
    if (pau32)
    {
        /*
         * initialize memory.
         */
        for (i = 0; i < (9*1024*1024) / sizeof(uint32_t); i++)
            pau32[i] = i;

        /*
         * Verify this for ever.
         */
        for (j = 0;; j++)
            for (i = 0; i < (9*1024*1024) / sizeof(uint32_t); i++)
                if (pau32[i] != i)
                {
                    printf("entry %d (%#x) at addr %p is changed from %08x to %08x. j=%d\n",
                           i, i, (void *)&pau32[i], i, pau32[i], j);
                    return 1;
                }
    }
    else
        printf("failed to allocate 9MB from heap!\n");
    return 1;
}

