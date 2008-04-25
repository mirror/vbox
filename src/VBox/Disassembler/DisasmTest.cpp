/** @file
 *
 * VBox disassembler:
 * Test application
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
#include <VBox/err.h>
#include <stdio.h>
#include <iprt/string.h>

DECLASM(int) TestProc();
DECLASM(int) TestProc64();
//uint8_t aCode16[] = { 0x66, 0x67, 0x89, 0x07 };

int main(int argc, char **argv)
{
    printf("VBox Disassembler Test\n");
    if (argc != 1)
    {
        //printf("DisasmBlock on printf:\n");
        //DisasmBlock((uint8_t *)printf, 256);
    }
    else
    {
        RTUINTPTR pInstr = (RTUINTPTR)TestProc;

        for (int i=0;i<50;i++)
        {
            unsigned    cb;
            DISCPUSTATE cpu;
            char         szOutput[256];

            memset(&cpu, 0, sizeof(cpu));
            cpu.mode = CPUMODE_32BIT;
            if (VBOX_SUCCESS(DISInstr(&cpu, pInstr, 0, &cb, szOutput)))
                printf(szOutput);
            else
            {
                printf("DISOne failed!\n");
                return 1;
            }
            pInstr += cb;
        }

        printf("\n64 bits disassembly\n");
        pInstr = (RTUINTPTR)TestProc64;

        for (int i=0;i<50;i++)
        {
            unsigned    cb;
            DISCPUSTATE cpu;
            char         szOutput[256];

            memset(&cpu, 0, sizeof(cpu));
            cpu.mode = CPUMODE_64BIT;
//__debugbreak();
            if (VBOX_SUCCESS(DISInstr(&cpu, pInstr, 0, &cb, szOutput)))
                printf(szOutput);
            else
            {
                printf("DISOne failed!\n");
                return 1;
            }
            pInstr += cb;
        }
    }
    return 0;
}

