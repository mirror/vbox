/* $Id$ */
/** @file
 * IPRT - Utility for translating addresses into symbols+offset.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dbg.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>



/**
 * Tries to parse out an address at the head of the string.
 *
 * @returns true if found address, false if not.
 * @param   psz                 Where to start parsing.
 * @param   pcchAddress         Where to store the address length.
 * @param   pu64Address         Where to store the address value.
 */
static bool TryParseAddress(const char *psz, size_t *pcchAddress, uint64_t *pu64Address)
{
    const char *pszStart = psz;

    /*
     * Hex prefix?
     */
    if (psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X'))
        psz += 2;

    /*
     * How many hex digits?  We want at least 4 and at most 16.
     */
    size_t off = 0;
    while (RT_C_IS_XDIGIT(psz[off]))
        off++;
    if (off < 4 || off > 16)
        return false;

    /*
     * Check for separator (xxxxxxxx'yyyyyyyy).
     */
    bool fHave64bitSep = off <= 8
                      && psz[off] == '\''
                      && RT_C_IS_XDIGIT(psz[off + 1])
                      && RT_C_IS_XDIGIT(psz[off + 2])
                      && RT_C_IS_XDIGIT(psz[off + 3])
                      && RT_C_IS_XDIGIT(psz[off + 4])
                      && RT_C_IS_XDIGIT(psz[off + 5])
                      && RT_C_IS_XDIGIT(psz[off + 6])
                      && RT_C_IS_XDIGIT(psz[off + 7])
                      && RT_C_IS_XDIGIT(psz[off + 8])
                      && !RT_C_IS_XDIGIT(psz[off + 9]);
    if (fHave64bitSep)
    {
        uint32_t u32High;
        int rc = RTStrToUInt32Ex(psz, NULL, 16, &u32High);
        if (rc != VWRN_TRAILING_CHARS)
            return false;

        uint32_t u32Low;
        rc = RTStrToUInt32Ex(&psz[off + 1], NULL, 16, &u32Low);
        if (   rc != VINF_SUCCESS
            && rc != VWRN_TRAILING_SPACES
            && rc != VWRN_TRAILING_CHARS)
            return false;

        *pu64Address = RT_MAKE_U64(u32Low, u32High);
        off += 1 + 8;
    }
    else
    {
        int rc = RTStrToUInt64Ex(psz, NULL, 16, pu64Address);
        if (   rc != VINF_SUCCESS
            && rc != VWRN_TRAILING_SPACES
            && rc != VWRN_TRAILING_CHARS)
            return false;
    }

    *pcchAddress = psz + off - pszStart;
    return true;
}


int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Create an empty address space that we can load modules and stuff into
     * as we parse the parameters.
     */
    RTDBGAS hAs;
    rc = RTDbgAsCreate(&hAs, 0, RTUINTPTR_MAX, "");
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDBgAsCreate -> %Rrc\n", rc);


    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--later", 'l', RTGETOPT_REQ_STRING },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
                RTPrintf("Usage: %s [options] <module> <address> [<module> <address> [..]]\n"
                         "\n"
                         "Options:\n"
                         "  -h, -?, --help\n"
                         "      Display this help text and exit successfully.\n"
                         "  -V, --version\n"
                         "      Display the revision and exit successfully.\n"
                         , RTPathFilename(argv[0]));
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("$Revision$\n");
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
            {
                /* <module> <address> */
                const char *pszModule = ValueUnion.psz;

                rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX);
                if (RT_FAILURE(rc))
                    return RTGetOptPrintError(rc, &ValueUnion);
                uint64_t u64Address = ValueUnion.u64;

                RTDBGMOD hMod;
                rc = RTDbgModCreateFromImage(&hMod, pszModule, NULL, 0 /*fFlags*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDbgModCreateFromImage(,%s,,) -> %Rrc\n", pszModule, rc);

                rc = RTDbgAsModuleLink(hAs, hMod, u64Address, 0 /* fFlags */);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDbgAsModuleLink(,%s,%llx,) -> %Rrc\n", pszModule, u64Address, rc);
                break;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Read text from standard input and see if there is anything we can translate.
     */
    for (;;)
    {
        /* Get a line. */
        char szLine[_64K];
        rc = RTStrmGetLine(g_pStdIn, szLine, sizeof(szLine));
        if (rc == VERR_EOF)
            break;
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTStrmGetLine() -> %Rrc\n", rc);

        /*
         * Search the line for potential addresses and replace them with
         * symbols+offset.
         */
        const char *pszStart = szLine;
        const char *psz      = szLine;
        char        ch;
        while ((ch = *psz) != '\0')
        {
            size_t      cchAddress;
            uint64_t    u64Address;

            if (   (   ch == '0'
                    && (psz[1] == 'x' || psz[1] == 'X')
                    && TryParseAddress(psz, &cchAddress, &u64Address))
                || (   RT_C_IS_XDIGIT(ch)
                    && TryParseAddress(psz, &cchAddress, &u64Address))
               )
            {
                if (pszStart != psz)
                    RTStrmWrite(g_pStdOut, pszStart, psz - pszStart);
                pszStart = psz;

                RTDBGSYMBOL Symbol;
                RTINTPTR    off;
                rc = RTDbgAsSymbolByAddr(hAs, u64Address, &off, &Symbol, NULL);
                if (RT_SUCCESS(rc))
                {
                    if (!off)
                        RTStrmPrintf(g_pStdOut, "%.*s=[%s]", cchAddress, psz, Symbol.szName);
                    else if (off > 0)
                        RTStrmPrintf(g_pStdOut, "%.*s=[%s+%#llx]", cchAddress, psz, Symbol.szName, off);
                    else
                        RTStrmPrintf(g_pStdOut, "%.*s=[%s-%#llx]", cchAddress, psz, Symbol.szName, -off);
                    psz += cchAddress;
                    pszStart = psz;
                }
                else
                    psz += cchAddress;
            }
            else
                psz++;
        }

        if (pszStart != psz)
            RTStrmWrite(g_pStdOut, pszStart, psz - pszStart);
        RTStrmPutCh(g_pStdOut, '\n');

    }

    return RTEXITCODE_SUCCESS;
}

