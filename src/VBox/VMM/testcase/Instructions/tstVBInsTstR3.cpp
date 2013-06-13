/* $Id$ */
/** @file
 * Instruction Test Environment - IPRT ring-3 driver.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#if HC_ARCH_BITS == 64
typedef uint64_t VBINSTSTREG;
#else
typedef uint32_t VBINSTSTREG;
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
RTTEST g_hTest;


RT_C_DECLS_BEGIN
DECLASM(void)    TestInstrMain(void);

DECLEXPORT(void) VBInsTstFailure(const char *pszMessage);
DECLEXPORT(void) VBInsTstFailure1(const char *pszFmt, VBINSTSTREG uArg1);
DECLEXPORT(void) VBInsTstFailure2(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2);
DECLEXPORT(void) VBInsTstFailure3(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2, VBINSTSTREG uArg3);
DECLEXPORT(void) VBInsTstFailure4(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2, VBINSTSTREG uArg3, VBINSTSTREG uArg4);
RT_C_DECLS_END


DECLEXPORT(void) VBInsTstFailure(const char *pszMessage)
{
    RTTestFailed(g_hTest, "%s", pszMessage);
}

DECLEXPORT(void) VBInsTstFailure1(const char *pszFmt, VBINSTSTREG uArg1)
{
    RTTestFailed(g_hTest, pszFmt, uArg1);
}


DECLEXPORT(void) VBInsTstFailure2(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2)
{
    RTTestFailed(g_hTest, pszFmt, uArg1, uArg2);
}


DECLEXPORT(void) VBInsTstFailure3(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2, VBINSTSTREG uArg3)
{
    RTTestFailed(g_hTest, pszFmt, uArg1, uArg2, uArg3);
}


DECLEXPORT(void) VBInsTstFailure4(const char *pszFmt, VBINSTSTREG uArg1, VBINSTSTREG uArg2, VBINSTSTREG uArg3, VBINSTSTREG uArg4)
{
    RTTestFailed(g_hTest, pszFmt, uArg1, uArg2, uArg3, uArg4);
}




int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("VBInsTstR3", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);
    TestInstrMain();
    return RTTestSummaryAndDestroy(g_hTest);
}

