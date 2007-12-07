/* $Id$ */
/** @file
 * DBGC Testcase - Command Parser.
 */

/*
 * Copyright (C) 2007 knut st. osmundsen <bird-kStuff-spam@anduin.net>
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

#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/initterm.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(bool) tstDBGCBackInput(PDBGCBACK pBack, uint32_t cMillies);
static DECLCALLBACK(int) tstDBGCBackRead(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead);
static DECLCALLBACK(int) tstDBGCBackWrite(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Global error counter. */
static unsigned g_cErrors = 0;
/** The DBGC backend structure for use in this testcase. */
static DBGCBACK g_tstBack =
{
    tstDBGCBackInput,
    tstDBGCBackRead,
    tstDBGCBackWrite
};
/** For keeping track of output prefixing. */
static bool g_fPendingPrefix = true;
/** Pointer to the the current input position. */
const char *g_pszInput = NULL;

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
        if (g_fPendingPrefix)
        {
            RTPrintf("tstDBGCParser:  OUTPUT: ");
            g_fPendingPrefix = false;
        }
        if (*pch == '\n')
            g_fPendingPrefix = true;
        RTPrintf("%c", *pch);
        pch++;
    }
    return VINF_SUCCESS;
}


/**
 * Completes the output, making sure that we're in
 * the 1 position of a new line.
 */
static void tstCompleteOutput(void)
{
    if (!g_fPendingPrefix)
        RTPrintf("\n");
    g_fPendingPrefix = true;
}


/**
 * Tries one command string.
 * @param   pDbgc           Pointer to the debugger instance.
 * @param   pszCmds         The command to test.
 * @param   rcCmd           The expected result.
 */
static void tstTry(PDBGC pDbgc, const char *pszCmds, int rcCmd)
{
    g_pszInput = pszCmds;
    if (strchr(pszCmds, '\0')[-1] == '\n')
        RTPrintf("tstDBGCParser: RUNNING: %s", pszCmds);
    else
        RTPrintf("tstDBGCParser: RUNNING: %s\n", pszCmds);

    pDbgc->rcCmd = VERR_INTERNAL_ERROR;
    dbgcProcessInput(pDbgc, true /* fNoExecute */);
    tstCompleteOutput();

    if (pDbgc->rcCmd != rcCmd)
    {
        RTPrintf("tstDBGCParser: rcCmd=%Rrc expected =%Rrc\n", pDbgc->rcCmd, rcCmd);
        g_cErrors++;
    }
}


int main()
{
    /*
     * Init.
     */
    RTR3Init();
    RTPrintf("tstDBGCParser: TESTING...\n");

    /*
     * Create a DBGC instance.
     */
    PDBGC pDbgc;
    int rc = dbgcCreate(&pDbgc, &g_tstBack, 0);
    if (RT_SUCCESS(rc))
    {
        rc = dbgcProcessInput(pDbgc, true /* fNoExecute */);
        tstCompleteOutput();
        if (RT_SUCCESS(rc))
        {
            tstTry(pDbgc, "stop\n", VINF_SUCCESS);
            tstTry(pDbgc, "format \n", VERR_PARSE_TOO_FEW_ARGUMENTS);
            tstTry(pDbgc, "format 0 1 23 4\n", VERR_PARSE_TOO_MANY_ARGUMENTS);
            tstTry(pDbgc, "sa 3 23 4 'q' \"21123123\" 'b' \n", VINF_SUCCESS);
        }

        dbgcDestroy(pDbgc);
    }

    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstDBGCParser: SUCCESS\n");
    else
        RTPrintf("tstDBGCParser: FAILURE - %d errors\n", g_cErrors);
    return g_cErrors != 0;
}
