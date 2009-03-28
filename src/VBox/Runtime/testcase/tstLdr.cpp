/* $Id$ */
/** @file
 * IPRT - Testcase for parts of RTLdr*.
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
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** If set, don't bitch when failing to resolve symbols. */
static bool     g_fDontBitchOnResolveFailure = false;
/** Whether it's kernel model code or not.. */
static bool     g_fKernel = true;
/** Module architectur bit count. */
static uint32_t g_cBits = HC_ARCH_BITS;


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
    /* check the name format and only permit certain names... later, right?  */

    if (g_cBits == 32)
        *pValue = 0xabcdef0f;
    else
    {
        RTUINTPTR BaseAddr = *(PCRTUINTPTR)pvUser;
        if (g_fKernel)
            *pValue = BaseAddr & RT_BIT(31) ? -(int32_t)0x76634935 : 0x7f304938;
        else
            *pValue = (int32_t)0x76634935 * ((BaseAddr >> 8) & 7);
    }
    return VINF_SUCCESS;
}


/**
 * One test iteration with one file.
 *
 * The test is very simple, we load the file three times
 * into two different regions. The first two into each of the
 * regions the for compare usage. The third is loaded into one
 * and then relocated between the two and other locations a few times.
 *
 * @returns number of errors.
 * @param   pszFilename     The file to load the mess with.
 */
static int testLdrOne(const char *pszFilename)
{
    int             rcRet = 0;
    size_t          cbImage = 0;
    struct Load
    {
        RTLDRMOD    hLdrMod;
        void       *pvBits;
        RTUINTPTR   Addr;
        const char *pszName;
    }   aLoads[6] =
    {
        { NULL, NULL, (int32_t)0xefefef00, "foo" },
        { NULL, NULL, (int32_t)0x40404040, "bar" },
        { NULL, NULL, (int32_t)0xefefef00, "foobar" },
        { NULL, NULL, (int32_t)0xefefef00, "kLdr-foo" },
        { NULL, NULL, (int32_t)0x40404040, "kLdr-bar" },
        { NULL, NULL, (int32_t)0xefefef00, "kLdr-foobar" }
    };
    unsigned i;

    /*
     * Load them.
     */
    for (i = 0; i < RT_ELEMENTS(aLoads); i++)
    {
        /* adjust load address and announce our intentions */
        if (g_cBits == 32)
            aLoads[i].Addr &= UINT32_C(0xffffffff);
        RTPrintf("tstLdr: Loading image at %RTptr\n", aLoads[i].Addr);

        /* open it */
        int rc;
        if (!strncmp(aLoads[i].pszName, "kLdr-", sizeof("kLdr-") - 1))
            rc = RTLdrOpenkLdr(pszFilename, 0, RTLDRARCH_WHATEVER, &aLoads[i].hLdrMod);
        else
            rc = RTLdrOpen(pszFilename, 0, RTLDRARCH_WHATEVER, &aLoads[i].hLdrMod);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstLdr: Failed to open '%s'/%d, rc=%Rrc. aborting test.\n", pszFilename, i, rc);
            Assert(aLoads[i].hLdrMod == NIL_RTLDRMOD);
            rcRet++;
            break;
        }

        /* size it */
        size_t cb = RTLdrSize(aLoads[i].hLdrMod);
        if (cbImage && cb != cbImage)
        {
            RTPrintf("tstLdr: Size mismatch '%s'/%d. aborting test.\n", pszFilename, i);
            rcRet++;
            break;
        }
        cbImage = cb;

        /* Allocate bits. */
        aLoads[i].pvBits = RTMemAlloc(cb);
        if (!aLoads[i].pvBits)
        {
            RTPrintf("tstLdr: Out of memory '%s'/%d cbImage=%d. aborting test.\n", pszFilename, i, cbImage);
            rcRet++;
            break;
        }

        /* Get the bits. */
        rc = RTLdrGetBits(aLoads[i].hLdrMod, aLoads[i].pvBits, aLoads[i].Addr, testGetImport, &aLoads[i].Addr);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstLdr: Failed to get bits for '%s'/%d, rc=%Rrc. aborting test\n", pszFilename, i, rc);
            rcRet++;
            break;
        }
    }

    /*
     * Continue with the relocations and symbol resolving.
     */
    if (!rcRet)
    {
        static RTUINTPTR aRels[] =
        {
            (int32_t)0xefefef00,        /* same. */
            (int32_t)0x40404040,        /* the other. */
            (int32_t)0xefefef00,        /* back. */
            (int32_t)0x40404040,        /* the other. */
            (int32_t)0xefefef00,        /* back again. */
            (int32_t)0x77773420,        /* somewhere entirely else. */
            (int32_t)0xf0000000,        /* somewhere entirely else. */
            (int32_t)0x40404040,        /* the other. */
            (int32_t)0xefefef00         /* back again. */
        };
        struct Symbols
        {
            /** The symbol offset. -1 indicates the first time. */
            unsigned    off;
            /** The symbol name. */
            const char *pszName;
        } aSyms[] =
        {
            { ~0, "Entrypoint" },
            { ~0, "SomeExportFunction1" },
            { ~0, "SomeExportFunction2" },
            { ~0, "SomeExportFunction3" },
            { ~0, "SomeExportFunction4" },
            { ~0, "SomeExportFunction5" },
            { ~0, "SomeExportFunction5" },
            { ~0, "DISCoreOne" }
        };

        unsigned iRel = 0;
        for (;;)
        {
            /* adjust load address and announce our intentions */
            if (g_cBits == 32)
                aRels[iRel] &= UINT32_C(0xffffffff);

            /* Compare all which are at the same address. */
            for (i = 0; i < RT_ELEMENTS(aLoads) - 1; i++)
            {
                for (unsigned j = i + 1; j < RT_ELEMENTS(aLoads); j++)
                {
                    if (aLoads[j].Addr == aLoads[i].Addr)
                    {
                        if (memcmp(aLoads[j].pvBits, aLoads[i].pvBits, cbImage))
                        {
                            RTPrintf("tstLdr: Mismatch between load %d and %d. ('%s')\n", j, i, pszFilename);
#if 1
                            const uint8_t *pu8J = (const uint8_t *)aLoads[j].pvBits;
                            const uint8_t *pu8I = (const uint8_t *)aLoads[i].pvBits;
                            for (uint32_t off = 0; off < cbImage; off++, pu8J++, pu8I++)
                                if (*pu8J != *pu8I)
                                    RTPrintf("  %08x  %02x != %02x\n", off, *pu8J, *pu8I);
#else
                            const uint32_t *pu32J = (const uint32_t *)aLoads[j].pvBits;
                            const uint32_t *pu32I = (const uint32_t *)aLoads[i].pvBits;
                            for (uint32_t off = 0; off < cbImage; off += 4, pu32J++, pu32I++)
                                if (*pu32J != *pu32I)
                                    RTPrintf("  %08x  %08x != %08x\n", off, *pu32J, *pu32I);
#endif
                            rcRet++;
                            break;
                        }
                    }
                }
            }

            /* compare symbols. */
            for (i = 0; i < RT_ELEMENTS(aLoads); i++)
            {
                for (unsigned iSym = 0; iSym < RT_ELEMENTS(aSyms); iSym++)
                {
                    RTUINTPTR Value;
                    int rc = RTLdrGetSymbolEx(aLoads[i].hLdrMod, aLoads[i].pvBits, aLoads[i].Addr, aSyms[iSym].pszName, &Value);
                    if (RT_SUCCESS(rc))
                    {
                        unsigned off = Value - aLoads[i].Addr;
                        if (off < cbImage)
                        {
                            if (aSyms[iSym].off == ~0U)
                                aSyms[iSym].off = off;
                            else if (off != aSyms[iSym].off)
                            {
                                RTPrintf("tstLdr: Mismatching symbol '%s' in '%s'/%d. expected off=%d got %d\n",
                                         aSyms[iSym].pszName, pszFilename, i, aSyms[iSym].off, off);
                                rcRet++;
                            }
                        }
                        else
                        {
                            RTPrintf("tstLdr: Invalid value for symbol '%s' in '%s'/%d. off=%#x Value=%#x\n",
                                     aSyms[iSym].pszName, pszFilename, i, off, Value);
                            rcRet++;
                        }
                    }
                    else if (!g_fDontBitchOnResolveFailure)
                    {
                        RTPrintf("tstLdr: Failed to resolve symbol '%s' in '%s'/%d.\n", aSyms[iSym].pszName, pszFilename, i);
                        rcRet++;
                    }
                }
            }

            if (iRel >= RT_ELEMENTS(aRels))
                break;

            /* relocate it stuff. */
            RTPrintf("tstLdr: Relocating image 2 from %RTptr to %RTptr\n", aLoads[2].Addr, aRels[iRel]);
            int rc = RTLdrRelocate(aLoads[2].hLdrMod, aLoads[2].pvBits, aRels[iRel], aLoads[2].Addr, testGetImport, &aRels[iRel]);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdr: Relocate of '%s' from %#x to %#x failed, rc=%Rrc. Aborting test.\n",
                         pszFilename, aRels[iRel], aLoads[2].Addr, rc);
                rcRet++;
                break;
            }
            aLoads[2].Addr = aRels[iRel];

            /* next */
            iRel++;
        }
    }

    /*
     * Clean up.
     */
    for (i = 0; i < RT_ELEMENTS(aLoads); i++)
    {
        if (aLoads[i].pvBits)
            RTMemFree(aLoads[i].pvBits);
        if (aLoads[i].hLdrMod)
        {
            int rc = RTLdrClose(aLoads[i].hLdrMod);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdr: Failed to close '%s' i=%d, rc=%Rrc.\n", pszFilename, i, rc);
                rcRet++;
            }
        }
    }

    return rcRet;
}



int main(int argc, char **argv)
{
    RTR3Init();

    int rcRet = 0;
    if (argc <= 1)
    {
        RTPrintf("usage: %s [-32|-64] [-kernel] <module> [more options/modules]\n", argv[0]);
        return 1;
    }

    /*
     * Iterate the files.
     */
    for (int argi = 1; argi < argc; argi++)
    {
        if (!strcmp(argv[argi], "-n"))
            g_fDontBitchOnResolveFailure = true;
        else if (!strcmp(argv[argi], "-32"))
            g_cBits = 32;
        else if (!strcmp(argv[argi], "-64"))
            g_cBits = 64;
        else if (!strcmp(argv[argi], "-kernel"))
            g_fKernel = true;
        else
        {
            RTPrintf("tstLdr: TESTING '%s'...\n", argv[argi]);
            rcRet += testLdrOne(argv[argi]);
        }
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
