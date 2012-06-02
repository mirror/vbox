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
#include <VBox/err.h>


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
 * Checks if two DBGC variables are identical
 *
 * @returns
 * @param   pVar1               .
 * @param   pVar2               .
 */
bool DBGCVarAreIdentical(PCDBGCVAR pVar1, PCDBGCVAR pVar2)
{
    if (!pVar1)
        return false;
    if (pVar1 == pVar2)
        return true;

    if (pVar1->enmType != pVar2->enmType)
        return false;
    switch (pVar1->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            if (pVar1->u.GCFlat != pVar2->u.GCFlat)
                return false;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            if (pVar1->u.GCFar.off != pVar2->u.GCFar.off)
                return false;
            if (pVar1->u.GCFar.sel != pVar2->u.GCFar.sel)
                return false;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            if (pVar1->u.GCPhys != pVar2->u.GCPhys)
                return false;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            if (pVar1->u.pvHCFlat != pVar2->u.pvHCFlat)
                return false;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            if (pVar1->u.HCPhys != pVar2->u.HCPhys)
                return false;
            break;
        case DBGCVAR_TYPE_NUMBER:
            if (pVar1->u.u64Number != pVar2->u.u64Number)
                return false;
            break;
        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_SYMBOL:
            if (RTStrCmp(pVar1->u.pszString, pVar2->u.pszString) != 0)
                return false;
            break;
        default:
            AssertFailedReturn(false);
    }

    if (pVar1->enmRangeType != pVar2->enmRangeType)
        return false;
    switch (pVar1->enmRangeType)
    {
        case DBGCVAR_RANGE_NONE:
            break;

        case DBGCVAR_RANGE_ELEMENTS:
        case DBGCVAR_RANGE_BYTES:
            if (pVar1->u64Range != pVar2->u64Range)
                return false;
            break;
        default:
            AssertFailedReturn(false);
    }

    return true;
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
 * @param   cArgs           The number of expected arguments. -1 if we don't
 *                          want to check the parsed arguments.
 * @param   va              Info about expected parsed arguments. For each
 *                          argument a DBGCVARTYPE, value (depends on type),
 *                          DBGCVARRANGETYPE and optionally range value.
 */
static void tstTryExV(PDBGC pDbgc, const char *pszCmds, int rcCmd, bool fNoExecute, const char *pszExpected,
                      int32_t cArgs, va_list va)
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

    if (cArgs >= 0)
    {
        PCDBGCVAR paArgs = pDbgc->aArgs;
        for (int32_t iArg = 0; iArg < cArgs; iArg++)
        {
            DBGCVAR ExpectedArg;
            ExpectedArg.enmType = (DBGCVARTYPE)va_arg(va, int/*DBGCVARTYPE*/);
            switch (ExpectedArg.enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:  ExpectedArg.u.GCFlat    = va_arg(va, RTGCPTR); break;
                case DBGCVAR_TYPE_GC_FAR:   ExpectedArg.u.GCFar.sel = va_arg(va, int /*RTSEL*/);
                                            ExpectedArg.u.GCFar.off = va_arg(va, uint32_t); break;
                case DBGCVAR_TYPE_GC_PHYS:  ExpectedArg.u.GCPhys    = va_arg(va, RTGCPHYS); break;
                case DBGCVAR_TYPE_HC_FLAT:  ExpectedArg.u.pvHCFlat  = va_arg(va, void *); break;
                case DBGCVAR_TYPE_HC_PHYS:  ExpectedArg.u.HCPhys    = va_arg(va, RTHCPHYS); break;
                case DBGCVAR_TYPE_NUMBER:   ExpectedArg.u.u64Number = va_arg(va, uint64_t); break;
                case DBGCVAR_TYPE_STRING:   ExpectedArg.u.pszString = va_arg(va, const char *); break;
                case DBGCVAR_TYPE_SYMBOL:   ExpectedArg.u.pszString = va_arg(va, const char *); break;
                default:
                    RTTestFailed(g_hTest, "enmType=%u iArg=%u\n", ExpectedArg.enmType, iArg);
                    ExpectedArg.u.u64Number = 0;
                    break;
            }
            ExpectedArg.enmRangeType = (DBGCVARRANGETYPE)va_arg(va, int /*DBGCVARRANGETYPE*/);
            switch (ExpectedArg.enmRangeType)
            {
                case DBGCVAR_RANGE_NONE:        ExpectedArg.u64Range = 0; break;
                case DBGCVAR_RANGE_ELEMENTS:    ExpectedArg.u64Range = va_arg(va, uint64_t); break;
                case DBGCVAR_RANGE_BYTES:       ExpectedArg.u64Range = va_arg(va, uint64_t); break;
                    default:
                        RTTestFailed(g_hTest, "enmRangeType=%u iArg=%u\n", ExpectedArg.enmRangeType, iArg);
                        ExpectedArg.u64Range = 0;
                        break;
            }

            if (!DBGCVarAreIdentical(&ExpectedArg, &paArgs[iArg]))
                RTTestFailed(g_hTest,
                             "Arg #%u\n"
                             "actual:   enmType=%u u64=%#RX64 enmRangeType=%u u64Range=%#RX64\n"
                             "expected: enmType=%u u64=%#RX64 enmRangeType=%u u64Range=%#RX64\n",
                             iArg,
                             paArgs[iArg].enmType, paArgs[iArg].u.u64Number, paArgs[iArg].enmRangeType, paArgs[iArg].u64Range,
                             ExpectedArg.enmType, ExpectedArg.u.u64Number, ExpectedArg.enmRangeType, ExpectedArg.u64Range);
        }
    }
}

/**
 * Tries one command string.
 *
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszCmds         The command to test.
 * @param   rcCmd           The expected result.
 * @param   fNoExecute      When set, the command is not executed.
 * @param   pszExpected     Expected output.  This does not need to include all
 *                          of the output, just the start of it.  Thus the
 *                          prompt can be omitted.
 * @param   cArgs           The number of expected arguments. -1 if we don't
 *                          want to check the parsed arguments.
 * @param   ...             Info about expected parsed arguments. For each
 *                          argument a DBGCVARTYPE, value (depends on type),
 *                          DBGCVARRANGETYPE and optionally range value.
 */
static void tstTryEx(PDBGC pDbgc, const char *pszCmds, int rcCmd, bool fNoExecute, const char *pszExpected, int32_t cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    tstTryExV(pDbgc, pszCmds, rcCmd, fNoExecute, pszExpected, cArgs, va);
    va_end(va);
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
    return tstTryEx(pDbgc, pszCmds, rcCmd, true /*fNoExecute*/, NULL, -1);
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
    return tstTryEx(pDbgc, pszCmds, rcCmd, false /*fNoExecute*/, pszExpected, -1);
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

    return tstTryEx(pDbgc, szCmd, VINF_SUCCESS, false /*fNoExecute*/, szExpected, -1);
}



static void testCodeView_ba(PDBGC pDbgc)
{
    RTTestISub("codeview - ba");
    tstTry(pDbgc, "ba x 1 0f000:0000\n", VINF_SUCCESS);
    tstTry(pDbgc, "ba x 1 0f000:0000 0\n", VINF_SUCCESS);
    tstTry(pDbgc, "ba x 1 0f000:0000 0 ~0\n", VINF_SUCCESS);
    tstTry(pDbgc, "ba x 1 0f000:0000 0 ~0 \"command\"\n", VINF_SUCCESS);
    tstTry(pDbgc, "ba x 1 0f000:0000 0 ~0 \"command\" too_many\n", VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS);
    tstTry(pDbgc, "ba x 1\n", VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS);

    tstTryEx(pDbgc, "ba x 1 0f000:1234 5 1000 \"command\"\n", VINF_SUCCESS,
             true /*fNoExecute*/,  NULL /*pszExpected*/, 6 /*cArgs*/,
             DBGCVAR_TYPE_STRING, "x",                          DBGCVAR_RANGE_BYTES, UINT64_C(1),
             DBGCVAR_TYPE_NUMBER, UINT64_C(1),                  DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_GC_FAR, 0xf000, UINT32_C(0x1234),     DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x5),                DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x1000),             DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_STRING, "command",                    DBGCVAR_RANGE_BYTES, UINT64_C(7));

    tstTryEx(pDbgc, "ba x 1 %0f000:1234 5 1000 \"command\"\n", VINF_SUCCESS,
             true /*fNoExecute*/,  NULL /*pszExpected*/, 6 /*cArgs*/,
             DBGCVAR_TYPE_STRING, "x",                          DBGCVAR_RANGE_BYTES, UINT64_C(1),
             DBGCVAR_TYPE_NUMBER, UINT64_C(1),                  DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_GC_FLAT, UINT64_C(0xf1234),           DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x5),                DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x1000),             DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_STRING, "command",                    DBGCVAR_RANGE_BYTES, UINT64_C(7));

    tstTry(pDbgc, "ba x 1 bad:bad 5 1000 \"command\"\n", VINF_SUCCESS);
    tstTry(pDbgc, "ba x 1 %bad:bad 5 1000 \"command\"\n", VERR_DBGC_PARSE_CONVERSION_FAILED);

    tstTryEx(pDbgc, "ba f 1 0f000:1234 5 1000 \"command\"\n", VINF_SUCCESS,
             true /*fNoExecute*/,  NULL /*pszExpected*/, 6 /*cArgs*/,
             DBGCVAR_TYPE_STRING, "f",                          DBGCVAR_RANGE_BYTES, UINT64_C(1),
             DBGCVAR_TYPE_NUMBER, UINT64_C(1),                  DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_GC_FAR, 0xf000, UINT32_C(0x1234),     DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x5),                DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_NUMBER, UINT64_C(0x1000),             DBGCVAR_RANGE_NONE,
             DBGCVAR_TYPE_STRING, "command",                    DBGCVAR_RANGE_BYTES, UINT64_C(7));

    tstTry(pDbgc, "ba x 1 0f000:1234 qnx 1000 \"command\"\n",   VERR_DBGC_PARSE_ARGUMENT_TYPE_MISMATCH);
    tstTry(pDbgc, "ba x 1 0f000:1234 5 qnx \"command\"\n",      VERR_DBGC_PARSE_ARGUMENT_TYPE_MISMATCH);
    tstTry(pDbgc, "ba x qnx 0f000:1234 5 1000 \"command\"\n",   VERR_DBGC_PARSE_ARGUMENT_TYPE_MISMATCH);
    tstTry(pDbgc, "ba x 1 qnx 5 1000 \"command\"\n",            VERR_DBGC_PARSE_ARGUMENT_TYPE_MISMATCH);
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
        pDbgc->pVM = (PVM)pDbgc;
        rc = dbgcProcessInput(pDbgc, true /* fNoExecute */);
        tstCompleteOutput();
        if (RT_SUCCESS(rc))
        {
            RTTestSub(g_hTest, "basic parsing");
            tstTry(pDbgc, "stop\n", VINF_SUCCESS);
            tstTry(pDbgc, "format 1\n", VINF_SUCCESS);
            tstTry(pDbgc, "format \n", VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS);
            tstTry(pDbgc, "format 0 1 23 4\n", VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS);
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

                tstNumOp(pDbgc, "8 mod 7",                                  1);

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
                tstTry(pDbgc, "r @notavalidregister\n", VERR_DBGF_REGISTER_NOT_FOUND);
            }

            /*
             * Test codeview commands.
             */
#ifdef DEBUG_bird /* This will fail for a while */
            if (RTTestErrorCount(g_hTest) == 0)
            {
                testCodeView_ba(pDbgc);

            }
#endif
        }

        dbgcDestroy(pDbgc);
    }

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(g_hTest);
}
