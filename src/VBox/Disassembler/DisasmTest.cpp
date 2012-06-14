/* $Id$ */
/** @file
 * VBox disassembler - Test application 
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <iprt/test.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/err.h>


DECLASM(int) TestProc32(void);
char TestProc32_End;
#ifndef RT_OS_OS2
DECLASM(int) TestProc64(void);
char TestProc64_End;
#endif
//uint8_t aCode16[] = { 0x66, 0x67, 0x89, 0x07 };

static void testDisas(uint8_t const *pabInstrs, size_t cbInstrs, DISCPUMODE enmDisCpuMode)
{
    for (size_t off = 0; off < cbInstrs; off++)
    {
        uint32_t const  cErrBefore = RTTestIErrorCount();
        uint32_t        cb = 1;
        DISCPUSTATE     Cpu;
        char            szOutput[256] = {0};
        int rc = DISInstrToStr(&pabInstrs[off], enmDisCpuMode, &Cpu, &cb, szOutput, sizeof(szOutput));


        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        RTTESTI_CHECK(cb == Cpu.opsize);
        RTTESTI_CHECK(cb > 0);
        RTTESTI_CHECK(cb <= 16);
        if (cErrBefore != RTTestIErrorCount())
            RTTestIFailureDetails("rc=%Rrc, off=%#x (%u) cbInstr=%u enmDisCpuMode=%d",
                                  rc, off, Cpu.opsize, enmDisCpuMode);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s", szOutput);
        off += cb;
    }

}


int main(int argc, char **argv)
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstDisasm", &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);


    testDisas((uint8_t const *)(uintptr_t)TestProc32, (uintptr_t)&TestProc32_End - (uintptr_t)TestProc32, DISCPUMODE_32BIT);
#ifndef RT_OS_OS2
    testDisas((uint8_t const *)(uintptr_t)TestProc64, (uintptr_t)&TestProc64_End - (uintptr_t)TestProc64, DISCPUMODE_64BIT);
#endif

    return RTTestSummaryAndDestroy(hTest);
}

