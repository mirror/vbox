/* $Id$ */
/** @file
 * IPRT - Status code messages, Windows, sorter build program.
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
#include <iprt/win/windows.h>

#include <iprt/errcore.h>
#include <iprt/asm.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* This is define casts the result to DWORD, whereas HRESULT and RTWINERRMSG
   are using long, causing newer compilers to complain. */
#undef _NDIS_ERROR_TYPEDEF_
#define _NDIS_ERROR_TYPEDEF_(lErr) (long)(lErr)


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
typedef long VBOXSTATUSTYPE; /* used by errmsgvboxcomdata.h */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTWINERRMSG  g_aStatusMsgs[] =
{
#if !defined(IPRT_NO_ERROR_DATA) && !defined(DOXYGEN_RUNNING)
# include "errmsgwindata.h"
# if defined(VBOX) && !defined(IN_GUEST)
#  include "errmsgvboxcomdata.h"
# endif
#endif
    { "Success.", "ERROR_SUCCESS", 0 },
};


/** qsort callback. */
static int CompareWinErrMsg(const void *pv1, const void *pv2)
{
    PCRTWINERRMSG p1 = (PCRTWINERRMSG)pv1;
    PCRTWINERRMSG p2 = (PCRTWINERRMSG)pv2;
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
    qsort(g_aStatusMsgs, RT_ELEMENTS(g_aStatusMsgs), sizeof(g_aStatusMsgs[0]), CompareWinErrMsg);

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
        static char s_szMsgTmp[_256K];
        long iPrev = (long)0x80000000;
        rcExit = RTEXITCODE_SUCCESS;
        for (size_t i = 0; i < RT_ELEMENTS(g_aStatusMsgs); i++)
        {
            PCRTWINERRMSG pMsg = &g_aStatusMsgs[i];

            /* Paranoid ERROR_SUCCESS handling: */
            if (pMsg->iCode > 0 && iPrev < 0)
                fprintf(pOut, "/*%#010lx:*/ { \"Success.\", \"ERROR_SUCCESS\", 0 }\n", (unsigned long)0);
            else if (pMsg->iCode == 0 && iPrev == 0)
                continue;

            /* Deal with duplicates in a gentle manner: */
            if (pMsg->iCode == iPrev && i != 0)
            {
                PCRTWINERRMSG pPrev = &g_aStatusMsgs[i - 1];
                if (strcmp(pMsg->pszDefine, pPrev->pszDefine) == 0)
                    continue;
                fprintf(stderr, "%s: error: Duplicate value %#lx (%ld) - %s and %s\n",
                        argv[0], (unsigned long)iPrev, iPrev, pMsg->pszDefine, pPrev->pszDefine);
                rcExit = RTEXITCODE_FAILURE;
            }
            iPrev = pMsg->iCode;

            /* Produce the output: */
            fprintf(pOut, "/*%#010lx:*/ ENTRY(\"%s\", \"%s\", %ld),\n", (unsigned long)pMsg->iCode,
                    EscapeString(pMsg->pszMsgFull, s_szMsgTmp, sizeof(s_szMsgTmp)), pMsg->pszDefine, pMsg->iCode);
        }

        /*
         * Close the output file and we're done.
         */
        if (fclose(pOut) != 0)
        {
            fprintf(stderr, "%s: Failed to flush/close '%s' after writing it!\n", argv[0], argv[1]);
            rcExit = RTEXITCODE_FAILURE;
        }
    }
    else
        fprintf(stderr, "%s: Failed to open '%s' for writing!\n", argv[0], argv[1]);
    return rcExit;
}

