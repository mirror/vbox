/* $Id$ */
/** @file
 * DBGC Testcase - Command Parser.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include <VBox/dbg.h>
#include "../DBGCInternal.h"

#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(bool) tstDBGCBackInput(PDBGCBACK pBack, uint32_t cMillies);
static DECLCALLBACK(int)  tstDBGCBackRead(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead);
static DECLCALLBACK(int)  tstDBGCBackWrite(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten);
static DECLCALLBACK(void) tstDBGCBackSetReady(PDBGCBACK pBack, bool fReady);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The test handle. */
static RTTEST g_hTest = NIL_RTTEST;

/** The DBGC backend structure for use in this testcase. */
static DBGCBACK g_tstBack =
{
    tstDBGCBackInput,
    tstDBGCBackRead,
    tstDBGCBackWrite,
    tstDBGCBackSetReady
};
/** For keeping track of output prefixing. */
static bool     g_fPendingPrefix = true;
/** Pointer to the current input position. */
const char     *g_pszInput = NULL;
/** The output of the last command. */
static char     g_szOutput[1024];
/** The current offset into g_szOutput. */
static size_t   g_offOutput = 0;


/**
 * Checks if there is input.
 *
 * @returns true if there is input ready.
 * @returns false if there not input ready.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   cMillies    Number of milliseconds to wait on input data.
 */
static DECLCALLBACK(bool) tstDBGCBackInput(PDBGCBACK pBack, uint32_t cMillies)
{
    return g_pszInput != NULL
       && *g_pszInput != '\0';
}


/**
 * Read input.
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   pvBuf       Where to put the bytes we read.
 * @param   cbBuf       Maximum nymber of bytes to read.
 * @param   pcbRead     Where to store the number of bytes actually read.
 *                      If NULL the entire buffer must be filled for a
 *                      successful return.
 */
static DECLCALLBACK(int) tstDBGCBackRead(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    if (g_pszInput && *g_pszInput)
    {
        size_t cb = strlen(g_pszInput);
        if (cb > cbBuf)
            cb = cbBuf;
        *pcbRead = cb;
        memcpy(pvBuf, g_pszInput, cb);
        g_pszInput += cb;
    }
    else
        *pcbRead = 0;
    return VINF_SUCCESS;
}


/**
 * Write (output).
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   pvBuf       What to write.
 * @param   cbBuf       Number of bytes to write.
 * @param   pcbWritten  Where to store the number of bytes actually written.
 *                      If NULL the entire buffer must be successfully written.
 */
static DECLCALLBACK(int) tstDBGCBackWrite(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten)
{
    const char *pch = (const char *)pvBuf;
    if (pcbWritten)
        *pcbWritten = cbBuf;
    while (cbBuf-- > 0)
    {
        /* screen/log output */
        if (g_fPendingPrefix)
        {
            RTTestPrintfNl(g_hTest, RTTESTLVL_ALWAYS, "OUTPUT: ");
            g_fPendingPrefix = false;
        }
        if (*pch == '\n')
            g_fPendingPrefix = true;
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "%c", *pch);

        /* buffer output */
        if (g_offOutput < sizeof(g_szOutput) - 1)
        {
            g_szOutput[g_offOutput++] = *pch;
            g_szOutput[g_offOutput] = '\0';
        }

        /* advance */
        pch++;
    }
    return VINF_SUCCESS;
}


/**
 * Ready / busy notification.
 *
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   fReady      Whether it's ready (true) or busy (false).
 */
static DECLCALLBACK(void) tstDBGCBackSetReady(PDBGCBACK pBack, bool fReady)
{
}


/**
 * Completes the output, making sure that we're in
 * the 1 position of a new line.
 */
static void tstCompleteOutput(void)
{
    if (!g_fPendingPrefix)
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "\n");
    g_fPendingPrefix = true;
}


/**
 * Tries one command string.
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszCmds         The command to test.
 * @param   rcCmd           The expected result.
 * @param   fNoExecute      When set, the command is not executed.
 * @param   pszExpected     Expected output.  This does not need to include all
 *                          of the output, just the start of it.  Thus the
 *                          prompt can be omitted.
 */
static void tstTryEx(PDBGC pDbgc, const char *pszCmds, int rcCmd, bool fNoExecute, const char *pszExpected)
{
    RT_ZERO(g_szOutput);
    g_offOutput = 0;
    g_pszInput = pszCmds;
    if (strchr(pszCmds, '\0')[-1] == '\n')
        RTTestPrintfNl(g_hTest, RTTESTLVL_ALWAYS, "RUNNING: %s", pszCmds);
    else
        RTTestPrintfNl(g_hTest, RTTESTLVL_ALWAYS, "RUNNING: %s\n", pszCmds);

    pDbgc->rcCmd = VERR_INTERNAL_ERROR;
    dbgcProcessInput(pDbgc, fNoExecute);
    tstCompleteOutput();

    if (pDbgc->rcCmd != rcCmd)
        RTTestFailed(g_hTest, "rcCmd=%Rrc expected =%Rrc\n", pDbgc->rcCmd, rcCmd);
    else if (   !fNoExecute
             && pszExpected
             && strncmp(pszExpected, g_szOutput, strlen(pszExpected)))
        RTTestFailed(g_hTest, "Wrong output - expected \"%s\"", pszExpected);
}


/**
 * Tries one command string without executing it.
 *
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszCmds         The command to test.
 * @param   rcCmd           The expected result.
 */
static void tstTry(PDBGC pDbgc, const char *pszCmds, int rcCmd)
{
    return tstTryEx(pDbgc, pszCmds, rcCmd, true /*fNoExecute*/, NULL);
}


#ifdef SOME_UNUSED_FUNCTION
/**
 * Tries to execute one command string.
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszCmds         The command to test.
 * @param   rcCmd           The expected result.
 * @param   pszExpected     Expected output.  This does not need to include all
 *                          of the output, just the start of it.  Thus the
 *                          prompt can be omitted.
 */
static void tstTryExec(PDBGC pDbgc, const char *pszCmds, int rcCmd, const char *pszExpected)
{
    return tstTryEx(pDbgc, pszCmds, rcCmd, false /*fNoExecute*/, pszExpected);
}
#endif


/**
 * Test an operator on an expression resulting a plain number.
 *
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszExpr         The express to test.
 * @param   u64Expect       The expected result.
 */
static void tstNumOp(PDBGC pDbgc, const char *pszExpr, uint64_t u64Expect)
{
    char szCmd[80];
    RTStrPrintf(szCmd, sizeof(szCmd), "format %s\n", pszExpr);

    char szExpected[80];
    RTStrPrintf(szExpected, sizeof(szExpected),
                "Number: hex %llx  dec 0i%lld  oct 0t%llo", u64Expect, u64Expect, u64Expect);

    return tstTryEx(pDbgc, szCmd, VINF_SUCCESS, false /*fNoExecute*/, szExpected);
}



int main()
{
    /*
     * Init.
     */
    int rc = RTTestInitAndCreate("tstDBGCParser", &g_hTest);
    if (rc)
        return rc;
    RTTestBanner(g_hTest);

    /*
     * Create a DBGC instance.
     */
    RTTestSub(g_hTest, "dbgcCreate");
    PDBGC pDbgc;
    rc = dbgcCreate(&pDbgc, &g_tstBack, 0);
    if (RT_SUCCESS(rc))
    {
        rc = dbgcProcessInput(pDbgc, true /* fNoExecute */);
        tstCompleteOutput();
        if (RT_SUCCESS(rc))
        {
            RTTestSub(g_hTest, "basic parsing");
            tstTry(pDbgc, "stop\n", VINF_SUCCESS);
            tstTry(pDbgc, "format 1\n", VINF_SUCCESS);
            tstTry(pDbgc, "format \n", VERR_PARSE_TOO_FEW_ARGUMENTS);
            tstTry(pDbgc, "format 0 1 23 4\n", VERR_PARSE_TOO_MANY_ARGUMENTS);
            tstTry(pDbgc, "sa 3 23 4 'q' \"21123123\" 'b' \n", VINF_SUCCESS);

            if (RTTestErrorCount(g_hTest) == 0)
            {
                RTTestSub(g_hTest, "Operators");
                tstNumOp(pDbgc, "1",                                        1);
                tstNumOp(pDbgc, "1",                                        1);
                tstNumOp(pDbgc, "1",                                        1);

                tstNumOp(pDbgc, "+1",                                       1);
                tstNumOp(pDbgc, "++++++1",                                  1);

                tstNumOp(pDbgc, "-1",                                       UINT64_MAX);
                tstNumOp(pDbgc, "--1",                                      1);
                tstNumOp(pDbgc, "---1",                                     UINT64_MAX);
                tstNumOp(pDbgc, "----1",                                    1);

                tstNumOp(pDbgc, "~0",                                       UINT64_MAX);
                tstNumOp(pDbgc, "~1",                                       UINT64_MAX-1);
                tstNumOp(pDbgc, "~~0",                                      0);
                tstNumOp(pDbgc, "~~1",                                      1);

                tstNumOp(pDbgc, "!1",                                       0);
                tstNumOp(pDbgc, "!0",                                       1);
                tstNumOp(pDbgc, "!42",                                      0);
                tstNumOp(pDbgc, "!!42",                                     1);
                tstNumOp(pDbgc, "!!!42",                                    0);
                tstNumOp(pDbgc, "!!!!42",                                   1);

                tstNumOp(pDbgc, "1 +1",                                     2);
                tstNumOp(pDbgc, "1 + 1",                                    2);
                tstNumOp(pDbgc, "1+1",                                      2);
                tstNumOp(pDbgc, "1+ 1",                                     2);

                tstNumOp(pDbgc, "1 - 1",                                    0);
                tstNumOp(pDbgc, "99 - 90",                                  9);

                tstNumOp(pDbgc, "2 * 2",                                    4);

                tstNumOp(pDbgc, "2 / 2",                                    1);
                tstNumOp(pDbgc, "2 / 0",                                    UINT64_MAX);
                tstNumOp(pDbgc, "0i1024 / 0i4",                             256);

                tstNumOp(pDbgc, "1<<1",                                     2);
                tstNumOp(pDbgc, "1<<0i32",                                  UINT64_C(0x0000000100000000));
                tstNumOp(pDbgc, "1<<0i48",                                  UINT64_C(0x0001000000000000));
                tstNumOp(pDbgc, "1<<0i63",                                  UINT64_C(0x8000000000000000));

                tstNumOp(pDbgc, "fedcba0987654321>>0i04",                   UINT64_C(0x0fedcba098765432));
                tstNumOp(pDbgc, "fedcba0987654321>>0i32",                   UINT64_C(0xfedcba09));
                tstNumOp(pDbgc, "fedcba0987654321>>0i48",                   UINT64_C(0x0000fedc));

                tstNumOp(pDbgc, "0ef & 4",                                  4);
                tstNumOp(pDbgc, "01234567891 & fff",                        UINT64_C(0x00000000891));
                tstNumOp(pDbgc, "01234567891 & ~fff",                       UINT64_C(0x01234567000));

                tstNumOp(pDbgc, "1 | 1",                                    1);
                tstNumOp(pDbgc, "0 | 4",                                    4);
                tstNumOp(pDbgc, "4 | 0",                                    4);
                tstNumOp(pDbgc, "4 | 4",                                    4);
                tstNumOp(pDbgc, "1 | 4 | 2",                                7);

                tstNumOp(pDbgc, "1 ^ 1",                                    0);
                tstNumOp(pDbgc, "1 ^ 0",                                    1);
                tstNumOp(pDbgc, "0 ^ 1",                                    1);
                tstNumOp(pDbgc, "3 ^ 1",                                    2);
                tstNumOp(pDbgc, "7 ^ 3",                                    4);

                tstNumOp(pDbgc, "7 || 3",                                   1);
                tstNumOp(pDbgc, "1 || 0",                                   1);
                tstNumOp(pDbgc, "0 || 1",                                   1);
                tstNumOp(pDbgc, "0 || 0",                                   0);

                tstNumOp(pDbgc, "0 && 0",                                   0);
                tstNumOp(pDbgc, "1 && 0",                                   0);
                tstNumOp(pDbgc, "0 && 1",                                   0);
                tstNumOp(pDbgc, "1 && 1",                                   1);
                tstNumOp(pDbgc, "4 && 1",                                   1);
            }

            if (RTTestErrorCount(g_hTest) == 0)
            {
                RTTestSub(g_hTest, "Odd cases");
                tstTry(pDbgc, "r @rax\n", VINF_SUCCESS);
                tstTry(pDbgc, "r @eax\n", VINF_SUCCESS);
                tstTry(pDbgc, "r @ah\n", VINF_SUCCESS);
            }
        }

        dbgcDestroy(pDbgc);
    }

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(g_hTest);
}
