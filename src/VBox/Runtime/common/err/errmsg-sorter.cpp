/* $Id$ */
/** @file
 * IPRT - Status code messages, sorter build program.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/err.h>

#include <stdio.h>
#include <stdlib.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTSTATUSMSG g_aStatusMsgs[] =
{
#if !defined(IPRT_NO_ERROR_DATA) && !defined(DOXYGEN_RUNNING)
# include "errmsgdata.h"
#else
    { "Success.", "Success.", "VINF_SUCCESS", 0 },
#endif
};


/** qsort callback. */
static int CompareErrMsg(const void *pv1, const void *pv2)
{
    PCRTSTATUSMSG p1 = (PCRTSTATUSMSG)pv1;
    PCRTSTATUSMSG p2 = (PCRTSTATUSMSG)pv2;
    int iDiff;
    if (p1->iCode < p2->iCode)
        iDiff = -1;
    else if (p1->iCode > p2->iCode)
        iDiff = 1;
    else
        iDiff = 0;
    return iDiff;
}


/**
 * Checks whether @a pszDefine is a deliberate duplicate define that should be
 * omitted.
 */
static bool IgnoreDuplicateDefine(const char *pszDefine)
{
    size_t const cchDefine = strlen(pszDefine);

    static const RTSTRTUPLE s_aTails[] =
    {
        { RT_STR_TUPLE("_FIRST") },
        { RT_STR_TUPLE("_LAST") },
        { RT_STR_TUPLE("_HIGEST") },
        { RT_STR_TUPLE("_LOWEST") },
    };
    for (size_t i = 0; i < RT_ELEMENTS(s_aTails); i++)
        if (   cchDefine > s_aTails[i].cch
            && memcmp(&pszDefine[cchDefine - s_aTails[i].cch], s_aTails[i].psz, s_aTails[i].cch) == 0)
            return true;

    static const RTSTRTUPLE s_aDeliberateOrSilly[] =
    {
        { RT_STR_TUPLE("VERR_VRDP_TIMEOUT") },
        { RT_STR_TUPLE("VINF_VRDP_SUCCESS") },
        { RT_STR_TUPLE("VWRN_CONTINUE_RECOMPILE") },
        { RT_STR_TUPLE("VWRN_PATM_CONTINUE_SEARCH") },
    };
    for (size_t i = 0; i < RT_ELEMENTS(s_aDeliberateOrSilly); i++)
        if (   cchDefine == s_aDeliberateOrSilly[i].cch
            && memcmp(pszDefine, s_aDeliberateOrSilly[i].psz, cchDefine) == 0)
            return true;

    return false;
}


/**
 * Escapes @a pszString using @a pszBuf as needed.
 * @note Duplicated in errmsg-sorter.cpp.
 */
static const char *EscapeString(const char *pszString, char *pszBuf, size_t cbBuf)
{
    if (strpbrk(pszString, "\n\t\r\"\\") == NULL)
        return pszString;

    char *pszDst = pszBuf;
    char  ch;
    do
    {
        ch = *pszString++;
        switch (ch)
        {
            default:
                *pszDst++ = ch;
                break;
            case '\\':
            case '"':
                *pszDst++ = '\\';
                *pszDst++ = ch;
                break;
            case '\n':
                *pszDst++ = '\\';
                *pszDst++ = 'n';
                break;
            case '\t':
                *pszDst++ = '\\';
                *pszDst++ = 't';
                break;
            case '\r':
                break; /* drop it */
        }
    } while (ch);

    if ((uintptr_t)(pszDst - pszBuf) > cbBuf)
        fprintf(stderr, "Escape buffer overrun!\n");

    return pszBuf;
}


int main(int argc, char **argv)
{
    /*
     * Argument check.
     */
    if (argc != 1 && argc != 2)
    {
        fprintf(stderr,
                "syntax error!\n"
                "Usage: %s [outfile]\n", argv[0]);
        return RTEXITCODE_SYNTAX;
    }

    /*
     * Sort the table.
     */
    qsort(g_aStatusMsgs, RT_ELEMENTS(g_aStatusMsgs), sizeof(g_aStatusMsgs[0]), CompareErrMsg);

    /*
     * Prepare output file.
     */
    int rcExit = RTEXITCODE_FAILURE;
    FILE *pOut = stdout;
    if (argc > 1)
        pOut = fopen(argv[1], "wt");
    if (pOut)
    {
        /*
         * Print the table.
         */
        static char s_szMsgTmp1[_32K];
        static char s_szMsgTmp2[_64K];
        int iPrev = INT32_MAX;
        for (size_t i = 0; i < RT_ELEMENTS(g_aStatusMsgs); i++)
        {
            PCRTSTATUSMSG pMsg = &g_aStatusMsgs[i];

            /* Deal with duplicates, trying to eliminate unnecessary *_FIRST, *_LAST,
               *_LOWEST, and *_HIGHEST entries as well as some deliberate duplicate entries.
               This means we need to look forward and backwards here. */
            if (pMsg->iCode == iPrev && i != 0)
            {
                if (IgnoreDuplicateDefine(pMsg->pszDefine))
                    continue;
                PCRTSTATUSMSG pPrev = &g_aStatusMsgs[i - 1];
                fprintf(stderr, "%s: warning: Duplicate value %d - %s and %s\n",
                        argv[0], iPrev, pMsg->pszDefine, pPrev->pszDefine);
            }
            else if (i + 1 < RT_ELEMENTS(g_aStatusMsgs))
            {
                PCRTSTATUSMSG pNext = &g_aStatusMsgs[i];
                if (   pMsg->iCode == pNext->iCode
                    && IgnoreDuplicateDefine(pMsg->pszDefine))
                    continue;
            }
            iPrev = pMsg->iCode;

            /* Produce the output: */
            fprintf(pOut, "/*%8d:*/ ENTRY(\"%s\", \"%s\", \"%s\", %s),\n",
                    pMsg->iCode,
                    EscapeString(pMsg->pszMsgShort, s_szMsgTmp1, sizeof(s_szMsgTmp1)),
                    EscapeString(pMsg->pszMsgFull, s_szMsgTmp2, sizeof(s_szMsgTmp2)),
                    pMsg->pszDefine, pMsg->pszDefine);

        }

        /*
         * Close the output file and we're done.
         */
        if (fclose(pOut) == 0)
            rcExit = RTEXITCODE_SUCCESS;
        else
            fprintf(stderr, "%s: Failed to flush/close '%s' after writing it!\n", argv[0], argv[1]);
    }
    else
        fprintf(stderr, "%s: Failed to open '%s' for writing!\n", argv[0], argv[1]);
    return rcExit;
}

