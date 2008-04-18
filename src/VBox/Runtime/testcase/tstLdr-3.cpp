/* $Id$ */
/** @file
 * innotek Portable Runtime - Testcase for parts of RTLdr*, manual inspection.
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
#include <VBox/dis.h>


static bool MyDisBlock(PDISCPUSTATE pCpu, RTHCUINTPTR pvCodeBlock, int32_t cbMax, RTUINTPTR off, RTUINTPTR Addr)
{
    int32_t i = 0;
    while (i < cbMax)
    {
        char        szOutput[256];
        uint32_t    cbInstr;
        if (RT_FAILURE(DISInstr(pCpu, pvCodeBlock + i, off, &cbInstr, szOutput)))
            return false;

        RTPrintf("%s", szOutput);
        if (pvCodeBlock + i + off == Addr)
            RTPrintf("^^^^^^^^\n");

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
 * Enumeration callback function used by RTLdrEnumSymbols().
 *
 * @returns iprt status code. Failure will stop the enumeration.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Symbol name. NULL if ordinal only.
 * @param   uSymbol         Symbol ordinal, ~0 if not used.
 * @param   Value           Symbol value.
 * @param   pvUser          The user argument specified to RTLdrEnumSymbols().
 */
static DECLCALLBACK(int) testEnumSymbol1(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    RTPrintf("  %RTptr %s (%d)\n", Value, pszSymbol, uSymbol);
    return VINF_SUCCESS;
}

/**
 * Current nearest symbol.
 */
typedef struct TESTNEARSYM
{
    RTUINTPTR   Addr;
    struct TESTSYM
    {
        RTUINTPTR   Value;
        unsigned    uSymbol;
        char        szName[512];
    } aSyms[2];
} TESTNEARSYM, *PTESTNEARSYM;

/**
 * Enumeration callback function used by RTLdrEnumSymbols().
 *
 * @returns iprt status code. Failure will stop the enumeration.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Symbol name. NULL if ordinal only.
 * @param   uSymbol         Symbol ordinal, ~0 if not used.
 * @param   Value           Symbol value.
 * @param   pvUser          The user argument specified to RTLdrEnumSymbols().
 */
static DECLCALLBACK(int) testEnumSymbol2(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PTESTNEARSYM pSym = (PTESTNEARSYM)pvUser;

    /* less or equal */
    if (    Value <= pSym->Addr
        &&  (   Value > pSym->aSyms[0].Value
             || (   Value == pSym->aSyms[0].Value
                 && !pSym->aSyms[0].szName[0]
                 && pszSymbol
                 && *pszSymbol
                )
            )
       )
    {
        pSym->aSyms[0].Value     = Value;
        pSym->aSyms[0].uSymbol   = uSymbol;
        pSym->aSyms[0].szName[0] = '\0';
        if (pszSymbol)
            strncat(pSym->aSyms[0].szName, pszSymbol, sizeof(pSym->aSyms[0].szName));
    }

    /* above */
    if (    Value > pSym->Addr
        &&  (   Value < pSym->aSyms[1].Value
             || (   Value == pSym->aSyms[1].Value
                 && !pSym->aSyms[1].szName[1]
                 && pszSymbol
                 && *pszSymbol
                )
            )
       )
    {
        pSym->aSyms[1].Value     = Value;
        pSym->aSyms[1].uSymbol   = uSymbol;
        pSym->aSyms[1].szName[0] = '\0';
        if (pszSymbol)
            strncat(pSym->aSyms[1].szName, pszSymbol, sizeof(pSym->aSyms[1].szName));
    }

    return VINF_SUCCESS;
}


int main(int argc, char **argv)
{
    RTR3Init();

    int rcRet = 0;
    if (argc <= 2)
    {
        RTPrintf("usage: %s <load-addr> <module> [addr1 []]\n", argv[0]);
        return 1;
    }

    /*
     * Load the module.
     */
    RTUINTPTR LoadAddr = (RTUINTPTR)RTStrToUInt64(argv[1]);
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(argv[2], &hLdrMod);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstLdr-3: Failed to open '%s': %Rra\n", argv[2], rc);
        return 1;
    }

    void *pvBits = RTMemAlloc(RTLdrSize(hLdrMod));
    rc = RTLdrGetBits(hLdrMod, pvBits, LoadAddr, testGetImport, NULL);
    if (RT_SUCCESS(rc))
    {
        if (argc > 3)
        {
            for (int i = 3; i < argc; i++)
            {
                TESTNEARSYM NearSym = {0};
                NearSym.Addr = (RTUINTPTR)RTStrToUInt64(argv[i]);
                NearSym.aSyms[1].Value = ~(RTUINTPTR)0;
                rc = RTLdrEnumSymbols(hLdrMod, RTLDR_ENUM_SYMBOL_FLAGS_ALL, pvBits, LoadAddr, testEnumSymbol2, &NearSym);
                if (RT_SUCCESS(rc))
                {
                    RTPrintf("tstLdr-3: Addr=%RTptr\n"
                             "%RTptr %s (%d) - %RTptr %s (%d)\n",
                             NearSym.Addr,
                             NearSym.aSyms[0].Value, NearSym.aSyms[0].szName, NearSym.aSyms[0].uSymbol,
                             NearSym.aSyms[1].Value, NearSym.aSyms[1].szName, NearSym.aSyms[1].uSymbol);
                    if (NearSym.Addr - NearSym.aSyms[0].Value < 0x10000)
                    {
                        DISCPUSTATE Cpu;
                        memset(&Cpu, 0, sizeof(Cpu));
                        Cpu.mode = CPUMODE_32BIT;
                        uint8_t *pbCode = (uint8_t *)pvBits + (NearSym.aSyms[0].Value - LoadAddr);
                        MyDisBlock(&Cpu, (uintptr_t)pbCode,
                                   RT_MAX(NearSym.aSyms[1].Value - NearSym.aSyms[0].Value, 0x20000),
                                   NearSym.aSyms[0].Value - (RTUINTPTR)pbCode,
                                   NearSym.Addr);
                    }
                }
                else
                {
                    RTPrintf("tstLdr-3: Failed to enumerate symbols: %Rra\n", rc);
                    rcRet++;
                }
            }
        }
        else
        {
            /*
             * Enumerate symbols.
             */
            rc = RTLdrEnumSymbols(hLdrMod, RTLDR_ENUM_SYMBOL_FLAGS_ALL, pvBits, LoadAddr, testEnumSymbol1, NULL);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdr-3: Failed to enumerate symbols: %Rra\n", rc);
                rcRet++;
            }
        }
    }
    else
    {
        RTPrintf("tstLdr-3: Failed to get bits for '%s' at %RTptr: %Rra\n", argv[2], LoadAddr, rc);
        rcRet++;
    }
    RTMemFree(pvBits);
    RTLdrClose(hLdrMod);

    /*
     * Test result summary.
     */
    if (!rcRet)
        RTPrintf("tstLdr-3: SUCCESS\n");
    else
        RTPrintf("tstLdr-3: FAILURE - %d errors\n", rcRet);
    return !!rcRet;
}
