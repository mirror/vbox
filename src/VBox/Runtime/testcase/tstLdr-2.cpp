/* $Id$ */
/** @file
 * IPRT - Testcase for parts of RTLdr*, manual inspection.
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
#include <VBox/dis.h>
#include <iprt/err.h>
#include <iprt/string.h>


bool MyDisBlock(PDISCPUSTATE pCpu, RTHCUINTPTR pvCodeBlock, int32_t cbMax, RTUINTPTR off)
{
    int32_t i = 0;
    while (i < cbMax)
    {
        char        szOutput[256];
        uint32_t    cbInstr;
        if (RT_FAILURE(DISInstr(pCpu, pvCodeBlock + i, off, &cbInstr, szOutput)))
            return false;

        RTPrintf("%s", szOutput);

        /* next */
        i += cbInstr;
    }
    return true;
}



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
    /* check the name format and only permit certain names */
    *pValue = 0xf0f0f0f0;
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
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(pszFilename, &hLdrMod);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstLdr: Failed to open '%s', rc=%Vrc. aborting test.\n", pszFilename, rc);
        Assert(hLdrMod == NIL_RTLDRMOD);
        return 1;
    }

    int rcRet = 1;
    size_t cb = RTLdrSize(hLdrMod);
    if (cb > 100)
    {
        void *pvBits = RTMemAlloc(cb);
        if (pvBits)
        {
            RTUINTPTR Addr = 0xc0000000;
            rc = RTLdrGetBits(hLdrMod, pvBits, Addr, testGetImport, NULL);
            if (RT_SUCCESS(rc))
            {
                RTUINTPTR Value;
                rc = RTLdrGetSymbolEx(hLdrMod, pvBits, Addr, "Entrypoint", &Value);
                if (RT_SUCCESS(rc))
                {
                    unsigned off = Value - Addr;
                    if (off < cb)
                    {
                        DISCPUSTATE Cpu;

                        memset(&Cpu, 0, sizeof(Cpu));
                        Cpu.mode = CPUMODE_32BIT;
                        if (MyDisBlock(&Cpu, (RTUINTPTR)pvBits + off, 200, Addr - (uintptr_t)pvBits))
                        {
                            RTUINTPTR Addr2 = 0xd0000000;
                            rc = RTLdrRelocate(hLdrMod, pvBits, Addr2, Addr, testGetImport, NULL);
                            if (RT_SUCCESS(rc))
                            {
                                if (MyDisBlock(&Cpu, (RTUINTPTR)pvBits + off, 200, Addr2 - (uintptr_t)pvBits))
                                    rcRet = 0;
                                else
                                    RTPrintf("tstLdr: Disassembly failed!\n");
                            }
                            else
                                RTPrintf("tstLdr: Relocate of '%s' from %#x to %#x failed, rc=%Vrc. Aborting test.\n",
                                         pszFilename, Addr2, Addr, rc);
                        }
                        else
                            RTPrintf("tstLdr: Disassembly failed!\n");
                    }
                    else
                        RTPrintf("tstLdr: Invalid value for symbol '%s' in '%s'. off=%#x Value=%#x\n",
                                 "Entrypoint", pszFilename, off, Value);
                }
                else
                    RTPrintf("tstLdr: Failed to resolve symbol '%s' in '%s', rc=%Vrc.\n", "Entrypoint", pszFilename, rc);
            }
            else
                RTPrintf("tstLdr: Failed to get bits for '%s', rc=%Vrc. aborting test\n", pszFilename, rc);
            RTMemFree(pvBits);
        }
        else
            RTPrintf("tstLdr: Out of memory '%s' cb=%d. aborting test.\n", pszFilename, cb);
    }
    else
        RTPrintf("tstLdr: Size is odd, '%s'. aborting test.\n", pszFilename);


    /* cleanup */
    rc = RTLdrClose(hLdrMod);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstLdr: Failed to close '%s', rc=%Vrc.\n", pszFilename, rc);
        rcRet++;
    }

    return rcRet;
}



int main(int argc, char **argv)
{
    RTR3Init();

    int rcRet = 0;
    if (argc <= 1)
    {
        RTPrintf("usage: %s <module> [more modules]\n", argv[0]);
        return 1;
    }

    /*
     * Iterate the files.
     */
    for (int argi = 1; argi < argc; argi++)
    {
        RTPrintf("tstLdr: TESTING '%s'...\n", argv[argi]);
        rcRet += testLdrOne(argv[argi]);
    }

    /*
     * Test result summary.
     */
    if (!rcRet)
        RTPrintf("tstLdr: SUCCESS\n");
    else
        RTPrintf("tstLdr: FAILURE - %d errors\n", rcRet);
    return !!rcRet;
}
