/* $Id$ */
/** @file
 * VBox Build Tool - A mini C Preprocessor.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <VBox/VBoxTpG.h>

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "scmstream.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The preprocessor mode.
 */
typedef enum VBCPPMODE
{
    kVBCppMode_Invalid = 0,
/*    kVBCppMode_Full,*/
    kVBCppMode_Selective,
    kVBCppMode_SelectiveD,
    kVBCppMode_End
} VBCPPMODE;


/** 
 * A define.
 */
typedef struct VBCPPDEF
{
    /** The string space core. */
    RTSTRSPACECORE      Core;
    /** Whether it's a function. */
    bool                fFunction;
    /** Variable argument count. */
    bool                fVarArg;
    /** The number of known arguments.*/
    uint32_t            cArgs;

    /** @todo More to come later some day... Currently, only care about selective
     *        preprocessing. */

    /** The define value.  (This is followed by the name). */
    char                szValue[1];
} VBCPPDEF;
/** Pointer to a define. */
typedef VBCPPDEF *PVBCPPDEF;


/**
 * C Preprocessor instance data.
 */
typedef struct VBCPP
{
    /** @name Options
     * @{ */ 
    /** The preprocessing mode. */
    VBCPPMODE       enmMode;
    /** Whether to keep comments. */
    bool            fKeepComments;

    /** The number of include directories. */
    uint32_t        cIncludes;
    /** Array of directories to search for include files. */
    const char    **papszIncludes;

    /** The name of the input file. */
    const char     *pszInput;
    /** The name of the output file. NULL if stdout. */
    const char     *pszOutput;
    /** @} */
    
    /** The define string space. */
    RTSTRSPACE      StrSpace;
    /** Indicates whether a C-word might need expansion.
     * The bitmap is indexed by C-word lead character.  Bits that are set 
     * indicates that the lead character is used in a \#define that we know and 
     * should expand. */ 
    uint32_t        bmDefined[256/32];
} VBCPP;
/** Pointer to the C preprocessor instance data. */
typedef VBCPP *PVBCPP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/



static void vbcppInit(PVBCPP pThis)
{
    pThis->enmMode          = kVBCppMode_Selective;
    pThis->fKeepComments    = true;
    pThis->cIncludes        = 0;
    pThis->cIncludes        = 0;
    pThis->papszIncludes    = NULL;
    pThis->pszInput         = NULL;
    pThis->pszOutput        = NULL;
    pThis->StrSpace         = NULL;
    RT_ZERO(pThis->bmDefined);

}


/**
 * Removes a define.
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C processor instance.
 * @param   pszDefine           The define name, no argument list or anything. 
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 */
static RTEXITCODE vbcppRemoveDefine(PVBCPP pThis, const char *pszDefine, size_t cchDefine)
{
    PRTSTRSPACECORE pHit = RTStrSpaceGetN(&pThis->StrSpace, pszDefine, cchDefine);
    if (pHit)
    {
        RTStrSpaceRemove(&pThis->StrSpace, pHit->pszString);
        RTMemFree(pHit);
    }
    return RTEXITCODE_SUCCESS;
}


/**
 * Adds a define. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C processor instance.
 * @param   pszDefine           The define name and optionally the argument 
 *                              list.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 * @param   pszValue            The value. 
 * @param   cchDefine           The length of the value. RTSTR_MAX is ok.
 */
static RTEXITCODE vbcppAddDefine(PVBCPP pThis, const char *pszDefine, size_t cchDefine, 
                                 const char *pszValue, size_t cchValue)
{
    if (cchDefine == RTSTR_MAX)
        cchDefine = strlen(pszDefine);
    if (cchValue == RTSTR_MAX)
        cchValue = strlen(pszValue);

    const char *pszParan = (const char *)memchr(pszDefine, '(', cchDefine);
    if (pszParan)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "function defines are not implemented. sorry");

    PVBCPPDEF pDef = (PVBCPPDEF)RTMemAlloc(sizeof(*pDef) + cchValue + cchDefine + 2);
    if (!pDef)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "out of memory");

    pDef->fFunction = false;
    pDef->fVarArg   = false;
    pDef->cArgs     = 0;
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';
    pDef->Core.pszString = &pDef->szValue[cchValue + 1];
    memcpy(&pDef->szValue[cchValue + 1], pszDefine, cchDefine);
    pDef->szValue[cchValue + 1 + cchDefine] = '\0';

    if (!RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core))
    {
        RTMsgWarning("Redefining '%s'\n", pDef->Core.pszString);
        vbcppRemoveDefine(pThis, pDef->Core.pszString, cchDefine);
        RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core);
    }
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE vbcppAddInclude(PVBCPP pThis, const char *pszDir)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Parses the command line options. 
 *  
 * @returns Program exit code. Exit on non-success or if *pfExit is set.
 * @param   pThis               The C preprocessor instance.
 * @param   argc                The argument count.
 * @param   argv                The argument vector.
 * @param   pfExit              Pointer to the exit indicator.
 */
static RTEXITCODE vbcppParseOptions(PVBCPP pThis, int argc, char **argv, bool *pfExit)
{
    RTEXITCODE rcExit;

    *pfExit = false;

    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--define",                   'D',                    RTGETOPT_REQ_STRING },
        { "--include-dir",              'I',                    RTGETOPT_REQ_STRING },
        { "--undefine",                 'U',                    RTGETOPT_REQ_STRING },
        { "--D-strip",                  'd',                    RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    int rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'd':
                pThis->enmMode = kVBCppMode_SelectiveD;
                break;

            case 'D':
            {
                const char *pszEqual = strchr(ValueUnion.psz, '=');
                if (pszEqual)
                    rcExit = vbcppAddDefine(pThis, ValueUnion.psz, pszEqual - ValueUnion.psz, pszEqual + 1, RTSTR_MAX);
                else
                    rcExit = vbcppAddDefine(pThis, ValueUnion.psz, RTSTR_MAX, "1", 1);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'I':
                rcExit = vbcppAddInclude(pThis, ValueUnion.psz);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;

            case 'U':
                rcExit = vbcppRemoveDefine(pThis, ValueUnion.psz, RTSTR_MAX);
                break;

            case 'h':
                RTPrintf("No help yet, sorry\n");
                *pfExit = true;
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision$";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                *pfExit = true;
                return RTEXITCODE_SUCCESS;
            }

            case VINF_GETOPT_NOT_OPTION:
                if (!pThis->pszInput)
                    pThis->pszInput = ValueUnion.psz;
                else if (!pThis->pszOutput)
                    pThis->pszOutput = ValueUnion.psz;
                else 
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "too many file arguments");
                break;


            /*
             * Errors and bugs.
             */
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    return RTEXITCODE_SUCCESS;
}




int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /* 
     * Parse options.
     */
    VBCPP This;
    vbcppInit(&This);
    bool fExit;
    RTEXITCODE rcExit = vbcppParseOptions(&This, argc, argv, &fExit);
    if (!fExit && rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Process the input file.
         */

    }

    return rcExit;
}


