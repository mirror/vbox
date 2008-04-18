/** @file
 *
 * VBox disassembler:
 * Test application for core.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("VBox Disassembler Test Core\n");
    if (argc != 1)
    {
        //printf("DisasmBlock on printf:\n");
        //DisasmBlock((uint8_t *)printf, 256);
    }
    else
    {
        printf("DISOne on it self:\n");

        unsigned    cb;
        DISCPUSTATE cpu;
        cpu.mode = CPUMODE_32BIT;
        if (DISCoreOne(&cpu, (RTUINTPTR)&DISCoreOne, &cb))
            printf("ok %d\n", cpu.addrmode);
        else
        {
            printf("DISOne failed!\n");
            return 1;
        }
    }

    return 0;
}

