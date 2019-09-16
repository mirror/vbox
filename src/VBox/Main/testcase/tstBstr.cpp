/* $Id$ */
/** @file
 * API Glue Testcase - Bstr.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/string.h>

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/uni.h>


static void testBstr1(RTTEST hTest)
{
    RTTestSub(hTest, "Bstr::printf");

#define CHECK_BSTR(a_Expr, a_bstr, a_szExpected) \
        do { \
            a_Expr; \
            size_t cchExpect = strlen(a_szExpected); \
            if ((a_bstr).length() != cchExpect) \
                RTTestFailed(hTest, "line %u: length() -> %zu, expected %zu (%ls vs %s)", \
                             __LINE__, (a_bstr).length(), cchExpect, (a_bstr).raw(), a_szExpected); \
            else \
            { \
                int iDiff = (a_bstr).compareUtf8(a_szExpected); \
                if (iDiff) \
                    RTTestFailed(hTest, "line %u: compareUtf8() -> %d: %ls vs %s", \
                                 __LINE__, iDiff, (a_bstr).raw(), a_szExpected); \
            } \
        } while (0)

    com::Bstr bstr1;
    CHECK_BSTR(bstr1.printf(""), bstr1, "");
    CHECK_BSTR(bstr1.printf("1234098694"), bstr1, "1234098694");
    CHECK_BSTR(bstr1.printf("%u-%u-%u-%s-%u", 42, 999, 42, "asdkfhjasldfk0", 42),
               bstr1, "42-999-42-asdkfhjasldfk0-42");

    com::Bstr bstr2;
    CHECK_BSTR(bstr2.printf("%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls::%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls::%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls-%ls",
                            bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(),
                            bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(),
                            bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw(), bstr1.raw()),
               bstr2, "42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42::42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42::42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42-42-999-42-asdkfhjasldfk0-42");
}


int main()
{
    RTTEST      hTest;
    RTEXITCODE  rcExit = RTTestInitAndCreate("tstBstr", &hTest);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTTestBanner(hTest);

        testBstr1(hTest);

        rcExit = RTTestSummaryAndDestroy(hTest);
    }
    return rcExit;
}

