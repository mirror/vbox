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
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/err.h>


DECLASM(int) TestProc32(void);
DECLASM(int) TestProc32_EndProc(void);
#ifndef RT_OS_OS2
DECLASM(int) TestProc64(void);
DECLASM(int) TestProc64_EndProc(void);
#endif
//uint8_t aCode16[] = { 0x66, 0x67, 0x89, 0x07 };

static void testDisasp(const char *pszSub, uint8_t const *pabInstrs, uintptr_t uEndPtr, DISCPUMODE enmDisCpuMode)
{
    RTTestISub(pszSub);
    size_t const cbInstrs = uEndPtr - (uintptr_t)pabInstrs;
    for (size_t off = 0; off < cbInstrs;)
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
        RTStrStripR(szOutput);
        RTTESTI_CHECK(szOutput[0]);
        if (szOutput[0])
        {
            char *pszBytes = strchr(szOutput, '[');
            RTTESTI_CHECK(pszBytes);
            if (pszBytes)
            {
                RTTESTI_CHECK(pszBytes[-1] == ' ');
                RTTESTI_CHECK(RT_C_IS_XDIGIT(pszBytes[1]));
                RTTESTI_CHECK(pszBytes[cb * 3] == ']');
                RTTESTI_CHECK(pszBytes[cb * 3 + 1] == ' ');

                size_t cch = strlen(szOutput);
                RTTESTI_CHECK(szOutput[cch - 1] != ',');
            }
        }
        if (cErrBefore != RTTestIErrorCount())
            RTTestIFailureDetails("rc=%Rrc, off=%#x (%u) cbInstr=%u enmDisCpuMode=%d\n",
                                  rc, off, Cpu.opsize, enmDisCpuMode);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s\n", szOutput);
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

    testDisas("32-bit", (uint8_t const *)(uintptr_t)TestProc32, (uintptr_t)&TestProc32_EndProc, DISCPUMODE_32BIT);
#ifndef RT_OS_OS2
    testDisas("64-bit", (uint8_t const *)(uintptr_t)TestProc64, (uintptr_t)&TestProc64_EndProc, DISCPUMODE_64BIT);
#endif

    return RTTestSummaryAndDestroy(hTest);
}

