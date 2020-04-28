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


/*
 * Include the string table code.
 */
#define BLDPROG_STRTAB_MAX_STRLEN           512
#define BLDPROG_STRTAB_WITH_COMPRESSION
#define BLDPROG_STRTAB_PURE_ASCII
#define BLDPROG_STRTAB_WITH_CAMEL_WORDS
#include <iprt/bldprog-strtab-template.cpp.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* This is define casts the result to DWORD, whereas HRESULT and RTWINERRMSG
   are using long, causing newer compilers to complain. */
#undef _NDIS_ERROR_TYPEDEF_
#define _NDIS_ERROR_TYPEDEF_(lErr) (long)(lErr)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef long VBOXSTATUSTYPE; /* used by errmsgvboxcomdata.h */

/** Used for raw-input and sorting. */
typedef struct RTWINERRMSGINT1
{
    /** Pointer to the full message string. */
    const char     *pszMsgFull;
    /** Pointer to the define string. */
    const char     *pszDefine;
    /** Status code number. */
    long            iCode;
    /** Set if duplicate. */
    bool            fDuplicate;
} RTWINERRMSGINT1;
typedef RTWINERRMSGINT1 *PRTWINERRMSGINT1;


/** This is used when building the string table and printing it. */
typedef struct RTWINERRMSGINT2
{
    /** The full message string. */
    BLDPROGSTRING   MsgFull;
    /** The define string. */
    BLDPROGSTRING   Define;
    /** Pointer to the define string. */
    const char     *pszDefine;
    /** Status code number. */
    long            iCode;
} RTWINERRMSGINT2;
typedef RTWINERRMSGINT2 *PRTWINERRMSGINT2;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char     *g_pszProgName = "errmsgwin-sorter";
static RTWINERRMSGINT1 g_aStatusMsgs[] =
{
#if !defined(IPRT_NO_ERROR_DATA) && !defined(DOXYGEN_RUNNING)
# include "errmsgwindata.h"
# if defined(VBOX) && !defined(IN_GUEST)
#  include "errmsgvboxcomdata.h"
# endif
#endif
    { "Success.", "ERROR_SUCCESS", 0, false },
};


static RTEXITCODE error(const char *pszFormat,  ...)
{
    va_list va;
    va_start(va, pszFormat);
    fprintf(stderr, "%s: error: ", g_pszProgName);
    vfprintf(stderr, pszFormat, va);
    va_end(va);
    return RTEXITCODE_FAILURE;
}


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


int main(int argc, char **argv)
{
    /*
     * Parse arguments.
     */
    enum { kMode_All, kMode_OnlyDefines } enmMode;
    if (argc == 3 && strcmp(argv[1], "--all") == 0)
        enmMode = kMode_All;
    else if (argc == 3 && strcmp(argv[1], "--only-defines") == 0)
        enmMode = kMode_OnlyDefines;
    else
    {
        fprintf(stderr,
                "syntax error!\n"
                "Usage: %s <--all|--only-defines> <outfile>\n", argv[0]);
        return RTEXITCODE_SYNTAX;
    }
    const char * const pszOutFile = argv[2];

    /*
     * Sort the table and mark duplicates.
     */
    qsort(g_aStatusMsgs, RT_ELEMENTS(g_aStatusMsgs), sizeof(g_aStatusMsgs[0]), CompareWinErrMsg);

    int rcExit = RTEXITCODE_SUCCESS;
    long iPrev = (long)0x80000000;
    bool fHaveSuccess = false;
    for (size_t i = 0; i < RT_ELEMENTS(g_aStatusMsgs); i++)
    {
        PRTWINERRMSGINT1 pMsg = &g_aStatusMsgs[i];
        if (pMsg->iCode == iPrev && i != 0)
        {
            pMsg->fDuplicate = true;

            if (iPrev == 0)
                continue;

            PRTWINERRMSGINT1 pPrev = &g_aStatusMsgs[i - 1];
            if (strcmp(pMsg->pszDefine, pPrev->pszDefine) == 0)
                continue;
            rcExit = error("Duplicate value %#lx (%ld) - %s and %s\n",
                           (unsigned long)iPrev, iPrev, pMsg->pszDefine, pPrev->pszDefine);
        }
        else
        {
            pMsg->fDuplicate = false;
            iPrev = pMsg->iCode;
            if (iPrev == 0)
                fHaveSuccess = true;
        }
    }
    if (!fHaveSuccess)
        rcExit = error("No zero / success value in the table!\n");

    /*
     * Create a string table for it all.
     */
    BLDPROGSTRTAB StrTab;
    if (!BldProgStrTab_Init(&StrTab, RT_ELEMENTS(g_aStatusMsgs) * 3))
        return error("Out of memory!\n");

    static RTWINERRMSGINT2 s_aStatusMsgs2[RT_ELEMENTS(g_aStatusMsgs)];
    size_t                 cStatusMsgs = 0;
    for (size_t i = 0; i < RT_ELEMENTS(g_aStatusMsgs); i++)
    {
        if (!g_aStatusMsgs[i].fDuplicate)
        {
            s_aStatusMsgs2[cStatusMsgs].iCode     = g_aStatusMsgs[i].iCode;
            s_aStatusMsgs2[cStatusMsgs].pszDefine = g_aStatusMsgs[i].pszDefine;
            BldProgStrTab_AddStringDup(&StrTab, &s_aStatusMsgs2[cStatusMsgs].Define, g_aStatusMsgs[i].pszDefine);
            if (enmMode != kMode_OnlyDefines)
                BldProgStrTab_AddStringDup(&StrTab, &s_aStatusMsgs2[cStatusMsgs].MsgFull, g_aStatusMsgs[i].pszMsgFull);
            cStatusMsgs++;
        }
    }

    if (!BldProgStrTab_CompileIt(&StrTab, true))
        return error("BldProgStrTab_CompileIt failed!\n");

    /*
     * Prepare output file.
     */
    FILE *pOut = fopen(pszOutFile, "wt");
    if (pOut)
    {
        /*
         * Print the table.
         */
        fprintf(pOut,
                "\n"
                "typedef struct RTMSGWINENTRYINT\n"
                "{\n"
                "    uint32_t offDefine  : 20;\n"
                "    uint32_t cchDefine  : 9;\n"
                "%s"
                "    int32_t  iCode;\n"
                "} RTMSGWINENTRYINT;\n"
                "typedef RTMSGWINENTRYINT *PCRTMSGWINENTRYINT;\n"
                "\n"
                "static const RTMSGWINENTRYINT g_aWinMsgs[ /*%lu*/ ] =\n"
                "{\n"
                ,
                enmMode == kMode_All
                ? "    uint32_t offMsgFull : 23;\n"
                  "    uint32_t cchMsgFull : 9;\n" : "",
                (unsigned long)cStatusMsgs);

        if (enmMode == kMode_All)
            for (size_t i = 0; i < cStatusMsgs; i++)
                fprintf(pOut, "/*%#010lx:*/ { %#08x, %3u, %#08x, %3u, %ld },\n",
                        s_aStatusMsgs2[i].iCode,
                        s_aStatusMsgs2[i].Define.offStrTab,
                        (unsigned)s_aStatusMsgs2[i].Define.cchString,
                        s_aStatusMsgs2[i].MsgFull.offStrTab,
                        (unsigned)s_aStatusMsgs2[i].MsgFull.cchString,
                        s_aStatusMsgs2[i].iCode);
        else if (enmMode == kMode_OnlyDefines)
            for (size_t i = 0; i < cStatusMsgs; i++)
                fprintf(pOut, "/*%#010lx:*/ { %#08x, %3u, %ld },\n",
                        s_aStatusMsgs2[i].iCode,
                        s_aStatusMsgs2[i].Define.offStrTab,
                        (unsigned)s_aStatusMsgs2[i].Define.cchString,
                        s_aStatusMsgs2[i].iCode);
        else
            return error("Unsupported message selection (%d)!\n", enmMode);
        fprintf(pOut,
                "};\n"
                "\n");

        BldProgStrTab_WriteStringTable(&StrTab, pOut, "static ", "g_", "WinMsgStrTab");

        /*
         * Close the output file and we're done.
         */
        fflush(pOut);
        if (ferror(pOut))
            rcExit = error("Error writing '%s'!\n", pszOutFile);
        if (fclose(pOut) != 0)
            rcExit = error("Failed to close '%s' after writing it!\n", pszOutFile);
    }
    else
        rcExit = error("Failed to open '%s' for writing!\n", pszOutFile);
    return rcExit;
}

