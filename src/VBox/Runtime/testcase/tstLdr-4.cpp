/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Testcase for RTLdrOpen using ldrLdrObjR0.r0.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/ldr.h>
#include <iprt/alloc.h>
#include <iprt/stream.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/err.h>
#include <iprt/string.h>


extern "C" DECLEXPORT(int) DisasmTest1(void);


/**
 * Resolve an external symbol during RTLdrGetBits().
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) testGetImport(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser)
{
    if (     !strcmp(pszSymbol, "AssertMsg1")           || !strcmp(pszSymbol, "_AssertMsg1"))
        *pValue = (uintptr_t)AssertMsg1;
    else if (!strcmp(pszSymbol, "AssertMsg2")           || !strcmp(pszSymbol, "_AssertMsg2"))
        *pValue = (uintptr_t)AssertMsg2;
    else if (!strcmp(pszSymbol, "RTLogDefaultInstance") || !strcmp(pszSymbol, "_RTLogDefaultInstance"))
        *pValue = (uintptr_t)RTLogDefaultInstance;
    else if (!strcmp(pszSymbol, "MyPrintf")             || !strcmp(pszSymbol, "_MyPrintf"))
        *pValue = (uintptr_t)RTPrintf;
    else
    {
        RTPrintf("tstLdr-4: Unexpected import '%s'!\n", pszSymbol);
        return VERR_SYMBOL_NOT_FOUND;
    }
    return VINF_SUCCESS;
}


/**
 * One test iteration with one file.
 *
 * The test is very simple, we load the the file three times
 * into two different regions. The first two into each of the
 * regions the for compare usage. The third is loaded into one
 * and then relocated between the two and other locations a few times.
 *
 * @returns number of errors.
 * @param   pszFilename     The file to load the mess with.
 */
static int testLdrOne(const char *pszFilename)
{
    int             cErrors = 0;
    size_t          cbImage = 0;
    struct Load
    {
        RTLDRMOD    hLdrMod;
        void       *pvBits;
        const char *pszName;
    }   aLoads[6] =
    {
        { NULL, NULL, "foo" },
        { NULL, NULL, "bar" },
        { NULL, NULL, "foobar" },
        { NULL, NULL, "kLdr-foo" },
        { NULL, NULL, "kLdr-bar" },
        { NULL, NULL, "kLdr-foobar" }
    };
    unsigned i;
    int rc;

    /*
     * Load them.
     */
    for (i = 0; i < ELEMENTS(aLoads); i++)
    {
        if (!strncmp(aLoads[i].pszName, "kLdr-", sizeof("kLdr-") - 1))
            rc = RTLdrOpenkLdr(pszFilename, &aLoads[i].hLdrMod);
        else
            rc = RTLdrOpen(pszFilename, &aLoads[i].hLdrMod);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstLdr-4: Failed to open '%s'/%d, rc=%Rrc. aborting test.\n", pszFilename, i, rc);
            Assert(aLoads[i].hLdrMod == NIL_RTLDRMOD);
            cErrors++;
            break;
        }

        /* size it */
        size_t cb = RTLdrSize(aLoads[i].hLdrMod);
        if (cbImage && cb != cbImage)
        {
            RTPrintf("tstLdr-4: Size mismatch '%s'/%d. aborting test.\n", pszFilename, i);
            cErrors++;
            break;
        }
        cbImage = cb;

        /* Allocate bits. */
        aLoads[i].pvBits = RTMemAlloc(cb);
        if (!aLoads[i].pvBits)
        {
            RTPrintf("tstLdr-4: Out of memory '%s'/%d cbImage=%d. aborting test.\n", pszFilename, i, cbImage);
            cErrors++;
            break;
        }

        /* Get the bits. */
        rc = RTLdrGetBits(aLoads[i].hLdrMod, aLoads[i].pvBits, (uintptr_t)aLoads[i].pvBits, testGetImport, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstLdr-4: Failed to get bits for '%s'/%d, rc=%Rrc. aborting test\n", pszFilename, i, rc);
            cErrors++;
            break;
        }
    }

    /*
     * Execute the code.
     */
    if (!cErrors)
    {
        for (i = 0; i < ELEMENTS(aLoads); i += 1)
        {
            /* get the pointer. */
            RTUINTPTR Value;
            rc = RTLdrGetSymbolEx(aLoads[i].hLdrMod, aLoads[i].pvBits, (uintptr_t)aLoads[i].pvBits, "DisasmTest1", &Value);
            if (rc == VERR_SYMBOL_NOT_FOUND)
                rc = RTLdrGetSymbolEx(aLoads[i].hLdrMod, aLoads[i].pvBits, (uintptr_t)aLoads[i].pvBits, "_DisasmTest1", &Value);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdr-4: Failed to get symbol \"DisasmTest1\" from load #%d: %Rrc\n", i, rc);
                cErrors++;
                break;
            }
            DECLCALLBACKPTR(int, pfnDisasmTest1)(void) = (DECLCALLBACKPTR(int, )(void))(uintptr_t)Value; /* eeeh. */
            RTPrintf("tstLdr-4: pfnDisasmTest1=%p / add-symbol-file %s %#x\n", pfnDisasmTest1, pszFilename, aLoads[i].pvBits);

            /* call the test function. */
            rc = pfnDisasmTest1();
            if (rc)
            {
                RTPrintf("tstLdr-4: load #%d Test1 -> %#x\n", i, rc);
                cErrors++;
            }
        }
    }


    /*
     * Clean up.
     */
    for (i = 0; i < ELEMENTS(aLoads); i++)
    {
        if (aLoads[i].pvBits)
            RTMemFree(aLoads[i].pvBits);
        if (aLoads[i].hLdrMod)
        {
            int rc = RTLdrClose(aLoads[i].hLdrMod);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdr-4: Failed to close '%s' i=%d, rc=%Rrc.\n", pszFilename, i, rc);
                cErrors++;
            }
        }
    }

    return cErrors;
}



int main(int argc, char **argv)
{
    int cErrors = 0;
    RTR3Init();

    /*
     * Sanity check.
     */
    int rc = DisasmTest1();
    if (rc)
    {
        RTPrintf("tstLdr-4: FATAL ERROR - DisasmTest1 is buggy: rc=%#x\n", rc);
        return 1;
    }

    /*
     * Execute the test.
     */
    char szPath[RTPATH_MAX];
    rc = RTPathProgram(szPath, sizeof(szPath) - sizeof("/tstLdrObjR0.r0"));
    if (RT_SUCCESS(rc))
    {
        strcat(szPath, "/tstLdrObjR0.r0");
        RTPrintf("tstLdr-4: TESTING '%s'...\n", szPath);
        cErrors += testLdrOne(szPath);
    }
    else
    {
        RTPrintf("tstLdr-4: RTPathProgram -> %Rrc\n", rc);
        cErrors++;
    }

    /*
     * Test result summary.
     */
    if (!cErrors)
        RTPrintf("tstLdr-4: SUCCESS\n");
    else
        RTPrintf("tstLdr-4: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
