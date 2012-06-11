/** @file
 *
 * VBox disassembler:
 * Test application
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dis.h>
#include <VBox/err.h>
#include <stdio.h>
#include <iprt/string.h>
#include <iprt/asm.h>

DECLASM(int) TestProc();
#ifndef RT_OS_OS2
DECLASM(int) TestProc64();
#endif
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
        RTUINTPTR pInstr = (uintptr_t)TestProc;

        for (int i=0;i<50;i++)
        {
            unsigned    cb;
            DISCPUSTATE cpu;
            char        szOutput[256];

            if (RT_SUCCESS(DISInstr(pInstr, CPUMODE_32BIT, &cpu, &cb, szOutput)))
            {
                printf("%s", szOutput);
            }
            else
            {
                printf("DISOne failed!\n");
                return 1;
            }
            pInstr += cb;
        }

#ifndef RT_OS_OS2
        printf("\n64 bits disassembly\n");
        pInstr = (uintptr_t)TestProc64;

////__debugbreak();
        for (int i=0;i<50;i++)
        {
            unsigned    cb;
            DISCPUSTATE cpu;
            char        szOutput[256];

            if (RT_SUCCESS(DISInstr(pInstr, CPUMODE_64BIT, &cpu, &cb, szOutput)))
                printf("%s", szOutput);
            else
            {
                printf("DISOne failed!\n");
                return 1;
            }
            pInstr += cb;
        }
#endif
    }
    return 0;
}

