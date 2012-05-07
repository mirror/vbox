/* $Id$ */
/** @file
 * VBox Build Tool - A mini C Preprocessor.
 *
 * Purposes to which this preprocessor will be put:
 *      - Preprocessig vm.h into dtrace/lib/vm.d so we can access the VM
 *        structure (as well as substructures) from DTrace without having
 *        to handcraft it all.
 *      - Removing \#ifdefs relating to a new feature that has become
 *        stable and no longer needs \#ifdef'ing.
 *      - Pretty printing preprocessor directives.  This will be used by
 *        SCM.
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
#include <iprt/asm.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scmstream.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The bitmap type. */
#define VBCPP_BITMAP_TYPE                   uint64_t
/** The bitmap size as a multiple of VBCPP_BITMAP_TYPE. */
#define VBCPP_BITMAP_SIZE                   (128 / 64)
/** Checks if a bit is set. */
#define VBCPP_BITMAP_IS_SET(a_bm, a_ch)     ASMBitTest(a_bm, (a_ch) & 0x7f)
/** Sets a bit. */
#define VBCPP_BITMAP_SET(a_bm, a_ch)        ASMBitSet(a_bm, (a_ch) & 0x7f)
/** Empties the bitmap. */
#define VBCPP_BITMAP_EMPTY(a_bm)            do { (a_bm)[0] = 0; (a_bm)[1] = 0; } while (0)
/** Joins to bitmaps by OR'ing their values.. */
#define VBCPP_BITMAP_OR(a_bm1, a_bm2)       do { (a_bm1)[0] |= (a_bm2)[0]; (a_bm1)[1] |= (a_bm2)[1]; } while (0)


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
    /** Pointer to a list of argument names. */
    const char        **papszArgs;
    /** Lead character bitmap for the argument names. */
    VBCPP_BITMAP_TYPE   bmArgs[VBCPP_BITMAP_SIZE];
    /** The define value.  (This is followed by the name and arguments.) */
    char                szValue[1];
} VBCPPDEF;
/** Pointer to a define. */
typedef VBCPPDEF *PVBCPPDEF;


/**
 * Expansion context.
 */
typedef struct VBCPPCTX
{
    /** The next context on the stack. */
    struct VBCPPCTX    *pUp;
    /** The define being expanded. */
    PVBCPPDEF           pDef;
    /** Arguments. */
    struct VBCPPCTXARG
    {
        /** The value. */
        const char *pchValue;
        /** The value length. */
        const char *cchValue;
    }                   aArgs[1];
} VBCPPCTX;
/** Pointer to an define expansion context. */
typedef VBCPPCTX *PVBCPPCTX;

/**
 * Evaluation result.
 */
typedef enum VBCPPEVAL
{
    kVBCppEval_Invalid = 0,
    kVBCppEval_True,
    kVBCppEval_False,
    kVBCppEval_Undecided,
    kVBCppEval_End
} VBCPPEVAL;


/**
 * The condition kind.
 */
typedef enum VBCPPCONDKIND
{
    kVBCppCondKind_Invalid = 0,
    /** \#if expr  */
    kVBCppCondKind_If,
    /** \#ifdef define  */
    kVBCppCondKind_IfDef,
    /** \#ifndef define  */
    kVBCppCondKind_IfNDef,
    /** \#elif expr */
    kVBCppCondKind_ElIf,
    /** The end of valid values. */
    kVBCppCondKind_End
} VBCPPCONDKIND;


/**
 * Conditional stack entry.
 */
typedef struct VBCPPCOND
{
    /** The next conditional on the stack. */
    struct VBCPPCOND   *pUp;
    /** The kind of conditional. This changes on encountering \#elif. */
    VBCPPCONDKIND       enmKind;
    /** Evaluation result. */
    VBCPPEVAL           enmResult;
    /** The evaluation result of the whole stack. */
    VBCPPEVAL           enmStackResult;

    /** Whether we've seen the last else. */
    bool                fSeenElse;
    /** The nesting level of this condition. */
    uint16_t            iLevel;
    /** The nesting level of this condition wrt the ones we keep. */
    uint16_t            iKeepLevel;

    /** The condition string. (Points within the stream buffer.) */
    const char         *pchCond;
    /** The condition length. */
    size_t              cchCond;
} VBCPPCOND;
/** Pointer to a conditional stack entry. */
typedef VBCPPCOND *PVBCPPCOND;


/**
 * Input buffer stack entry.
 */
typedef struct VBCPPINPUT
{
    /** Pointer to the next input on the stack. */
    struct VBCPPINPUT  *pUp;
    /** The input stream. */
    SCMSTREAM           StrmInput;
    /** Pointer into szName to the part which was specified. */
    const char         *pszSpecified;
    /** The input file name with include path. */
    char                szName[1];
} VBCPPINPUT;
/** Pointer to a input buffer stack entry */
typedef VBCPPINPUT *PVBCPPINPUT;


/**
 * C Preprocessor instance data.
 */
typedef struct VBCPP
{
    /** @name Options
     * @{ */
    /** The preprocessing mode. */
    VBCPPMODE           enmMode;
    /** Whether to keep comments. */
    bool                fKeepComments;

    /** The number of include directories. */
    uint32_t            cIncludes;
    /** Array of directories to search for include files. */
    char              **papszIncludes;

    /** The name of the input file. */
    const char         *pszInput;
    /** The name of the output file. NULL if stdout. */
    const char         *pszOutput;
    /** @} */

    /** The define string space. */
    RTSTRSPACE          StrSpace;
    /** The string space holding explicitly undefined macros for selective
     * preprocessing runs. */
    RTSTRSPACE          UndefStrSpace;
    /** Indicates whether a C-word might need expansion.
     * The bitmap is indexed by C-word lead character.  Bits that are set
     * indicates that the lead character is used in a \#define that we know and
     * should expand. */
    VBCPP_BITMAP_TYPE   bmDefined[VBCPP_BITMAP_SIZE];
    /** Indicates whether a C-word might need argument expansion.
     * The bitmap is indexed by C-word lead character.  Bits that are set
     * indicates that the lead character is used in an argument of an currently
     * expanding  \#define. */
    VBCPP_BITMAP_TYPE   bmArgs[VBCPP_BITMAP_SIZE];

    /** Expansion context stack. */
    PVBCPPCTX           pExpStack;
    /** The current expansion stack depth. */
    uint32_t            cExpStackDepth;

    /** The current depth of the conditional stack. */
    uint32_t            cCondStackDepth;
    /** Conditional stack. */
    PVBCPPCOND          pCondStack;
    /** The current condition evaluates to kVBCppEval_False, don't output. */
    bool                fIf0Mode;

    /** Whether the current line could be a preprocessor line.
     * This is set when EOL is encountered and cleared again when a
     * non-comment-or-space character is encountered.  See vbcppPreprocess. */
    bool                fMaybePreprocessorLine;

    /** The input stack depth */
    uint32_t            cInputStackDepth;
    /** The input buffer stack. */
    PVBCPPINPUT         pInputStack;

    /** The output stream. */
    SCMSTREAM           StrmOutput;

    /** The status of the whole job, as far as we know. */
    RTEXITCODE          rcExit;
    /** Whether StrmOutput is valid (for vbcppTerm). */
    bool                fStrmOutputValid;
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
    pThis->UndefStrSpace    = NULL;
    pThis->pExpStack        = NULL;
    pThis->cExpStackDepth   = 0;
    pThis->cCondStackDepth  = 0;
    pThis->pCondStack       = NULL;
    pThis->fIf0Mode         = false;
    pThis->fMaybePreprocessorLine = true;
    VBCPP_BITMAP_EMPTY(pThis->bmDefined);
    VBCPP_BITMAP_EMPTY(pThis->bmArgs);
    pThis->cCondStackDepth  = 0;
    pThis->pInputStack      = NULL;
    RT_ZERO(pThis->StrmOutput);
    pThis->rcExit           = RTEXITCODE_SUCCESS;
    pThis->fStrmOutputValid = false;
}


/**
 * Displays an error message.
 *
 * @returns RTEXITCODE_FAILURE
 * @param   pThis               The C preprocessor instance.
 * @param   pszMsg              The message.
 * @param   ...                 Message arguments.
 */
static RTEXITCODE vbcppError(PVBCPP pThis, const char *pszMsg, ...)
{
    NOREF(pThis);
    if (pThis->pInputStack)
    {
        PSCMSTREAM pStrm = &pThis->pInputStack->StrmInput;

        size_t const off     = ScmStreamTell(pStrm);
        size_t const iLine   = ScmStreamTellLine(pStrm);
        ScmStreamSeekByLine(pStrm, iLine);
        size_t const offLine = ScmStreamTell(pStrm);

        va_list va;
        va_start(va, pszMsg);
        RTPrintf("%s:%d:%zd: error: %N.\n", pThis->pInputStack->szName, iLine + 1, off - offLine + 1, pszMsg, va);
        va_end(va);

        size_t cchLine;
        SCMEOL enmEof;
        const char *pszLine = ScmStreamGetLineByNo(pStrm, iLine, &cchLine, &enmEof);
        if (pszLine)
            RTPrintf("  %.*s\n"
                     "  %*s^\n",
                     cchLine, pszLine, off - offLine, "");

        ScmStreamSeekAbsolute(pStrm, off);
    }
    else
    {
        va_list va;
        va_start(va, pszMsg);
        RTMsgErrorV(pszMsg, va);
        va_end(va);
    }
    return pThis->rcExit = RTEXITCODE_FAILURE;
}


/**
 * Displays an error message.
 *
 * @returns RTEXITCODE_FAILURE
 * @param   pThis               The C preprocessor instance.
 * @param   pszPos              Pointer to the offending character.
 * @param   pszMsg              The message.
 * @param   ...                 Message arguments.
 */
static RTEXITCODE vbcppErrorPos(PVBCPP pThis, const char *pszPos, const char *pszMsg, ...)
{
    NOREF(pszPos); NOREF(pThis);
    va_list va;
    va_start(va, pszMsg);
    RTMsgErrorV(pszMsg, va);
    va_end(va);
    return pThis->rcExit = RTEXITCODE_FAILURE;
}


/**
 * Checks if the given character is a valid C identifier lead character.
 *
 * @returns true / false.
 * @param   ch                  The character to inspect.
 */
DECLINLINE(bool) vbcppIsCIdentifierLeadChar(char ch)
{
    return RT_C_IS_ALPHA(ch)
        || ch == '_';
}


/**
 * Checks if the given character is a valid C identifier character.
 *
 * @returns true / false.
 * @param   ch                  The character to inspect.
 */
DECLINLINE(bool) vbcppIsCIdentifierChar(char ch)
{
    return RT_C_IS_ALNUM(ch)
        || ch == '_';
}



/**
 *
 * @returns @c true if valid, @c false if not. Error message already displayed
 *          on failure.
 * @param   pThis           The C preprocessor instance.
 * @param   pchIdentifier   The start of the identifier to validate.
 * @param   cchIdentifier   The length of the identifier. RTSTR_MAX if not
 *                          known.
 */
static bool vbcppValidateCIdentifier(PVBCPP pThis, const char *pchIdentifier, size_t cchIdentifier)
{
    if (cchIdentifier == RTSTR_MAX)
        cchIdentifier = strlen(pchIdentifier);

    if (cchIdentifier == 0)
    {
        vbcppErrorPos(pThis, pchIdentifier, "Zero length identifier");
        return false;
    }

    if (!vbcppIsCIdentifierLeadChar(*pchIdentifier))
    {
        vbcppErrorPos(pThis, pchIdentifier, "Bad lead chararacter in identifier: '%.*s'", cchIdentifier, pchIdentifier);
        return false;
    }

    for (size_t off = 1; off < cchIdentifier; off++)
    {
        if (!vbcppIsCIdentifierChar(pchIdentifier[off]))
        {
            vbcppErrorPos(pThis, pchIdentifier + off, "Illegal chararacter in identifier: '%.*s' (#%zu)", cchIdentifier, pchIdentifier, off + 1);
            return false;
        }
    }

    return true;
}


/**
 * Frees a define.
 *
 * @returns VINF_SUCCESS (used when called by RTStrSpaceDestroy)
 * @param   pStr                Pointer to the VBCPPDEF::Core member.
 * @param   pvUser              Unused.
 */
static DECLCALLBACK(int) vbcppFreeDefine(PRTSTRSPACECORE pStr, void *pvUser)
{
    RTMemFree(pStr);
    NOREF(pvUser);
    return VINF_SUCCESS;
}


/**
 * Removes a define.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name, no argument list or anything.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 * @param   fExplicitUndef      Explicit undefinition, that is, in a selective
 *                              preprocessing run it will evaluate to undefined.
 */
static RTEXITCODE vbcppDefineUndef(PVBCPP pThis, const char *pszDefine, size_t cchDefine, bool fExplicitUndef)
{
    PRTSTRSPACECORE pHit = RTStrSpaceGetN(&pThis->StrSpace, pszDefine, cchDefine);
    if (pHit)
    {
        RTStrSpaceRemove(&pThis->StrSpace, pHit->pszString);
        vbcppFreeDefine(pHit, NULL);
    }

    if (fExplicitUndef)
    {
        if (cchDefine == RTSTR_MAX)
            cchDefine = strlen(pszDefine);

        PRTSTRSPACECORE pStr = (PRTSTRSPACECORE)RTMemAlloc(sizeof(*pStr) + cchDefine + 1);
        if (!pStr)
            return vbcppError(pThis, "out of memory");
        char *pszDst = (char *)(pStr + 1);
        pStr->pszString = pszDst;
        memcpy(pszDst, pszDefine, cchDefine);
        pszDst[cchDefine] = '\0';
        if (!RTStrSpaceInsert(&pThis->UndefStrSpace, pStr))
            RTMemFree(pStr);
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Inserts a define.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pDef                The define to insert.
 */
static RTEXITCODE vbcppDefineInsert(PVBCPP pThis, PVBCPPDEF pDef)
{
    if (RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core))
        VBCPP_BITMAP_SET(pThis->bmDefined, *pDef->Core.pszString);
    else
    {
        RTMsgWarning("Redefining '%s'\n", pDef->Core.pszString);
        PVBCPPDEF pOld = (PVBCPPDEF)vbcppDefineUndef(pThis, pDef->Core.pszString, pDef->Core.cchString, false);
        bool fRc = RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core);
        Assert(fRc); Assert(pOld);
        vbcppFreeDefine(&pOld->Core, NULL);
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Adds a define.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name, no parameter list.
 * @param   cchDefine           The length of the name.
 * @param   pszParams           The parameter list.
 * @param   cchParams           The length of the parameter list.
 * @param   pszValue            The value.
 * @param   cchDefine           The length of the value.
 */
static RTEXITCODE vbcppDefineAddFn(PVBCPP pThis, const char *pszDefine, size_t cchDefine,
                                   const char *pszParams, size_t cchParams,
                                   const char *pszValue, size_t cchValue)

{
    Assert(RTStrNLen(pszDefine, cchDefine) == cchDefine);
    Assert(RTStrNLen(pszParams, cchParams) == cchParams);
    Assert(RTStrNLen(pszValue,  cchValue)  == cchValue);

    /*
     * Determin the number of arguments and how much space their names
     * requires.  Performing syntax validation while parsing.
     */
    uint32_t cchArgNames = 0;
    uint32_t cArgs       = 0;
    for (size_t off = 0; off < cchParams; off++)
    {
        /* Skip blanks and maybe one comma. */
        bool fIgnoreComma = cArgs != 0;
        while (off < cchParams)
        {
            if (!RT_C_IS_SPACE(pszParams[off]))
            {
                if (pszParams[off] != ',' || !fIgnoreComma)
                {
                    if (vbcppIsCIdentifierLeadChar(pszParams[off]))
                        break;
                    /** @todo variadic macros. */
                    return vbcppErrorPos(pThis, &pszParams[off], "Unexpected character");
                }
                fIgnoreComma = false;
            }
            off++;
        }
        if (off >= cchParams)
            break;

        /* Found and argument. First character is already validated. */
        cArgs++;
        cchArgNames += 2;
        off++;
        while (   off < cchParams
               && vbcppIsCIdentifierChar(pszParams[off]))
            off++, cchArgNames++;
    }

    /*
     * Allocate a structure.
     */
    size_t    cbDef = RT_OFFSETOF(VBCPPDEF, szValue[cchValue + 1 + cchDefine + 1 + cchArgNames])
                    + sizeof(const char *) * cArgs;
    cbDef = RT_ALIGN_Z(cbDef, sizeof(const char *));
    PVBCPPDEF pDef  = (PVBCPPDEF)RTMemAlloc(cbDef);
    if (!pDef)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "out of memory");

    char *pszDst = &pDef->szValue[cchValue + 1];
    pDef->Core.pszString = pszDst;
    memcpy(pszDst, pszDefine, cchDefine);
    pszDst += cchDefine;
    *pszDst++ = '\0';
    pDef->fFunction = true;
    pDef->fVarArg   = false;
    pDef->cArgs     = cArgs;
    pDef->papszArgs = (const char **)((uintptr_t)pDef + cbDef - sizeof(const char *) * cArgs);
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';

    /*
     * Set up the arguments.
     */
    uint32_t iArg = 0;
    for (size_t off = 0; off < cchParams; off++)
    {
        /* Skip blanks and maybe one comma. */
        bool fIgnoreComma = cArgs != 0;
        while (off < cchParams)
        {
            if (!RT_C_IS_SPACE(pszParams[off]))
            {
                if (pszParams[off] != ',' || !fIgnoreComma)
                    break;
                fIgnoreComma = false;
            }
            off++;
        }
        if (off >= cchParams)
            break;

        /* Found and argument. First character is already validated. */
        pDef->papszArgs[iArg] = pszDst;
        do
        {
            *pszDst++ = pszParams[off++];
        } while (   off < cchParams
                 && vbcppIsCIdentifierChar(pszParams[off]));
        *pszDst++ = '\0';
        iArg++;
    }
    Assert((uintptr_t)pszDst <= (uintptr_t)pDef->papszArgs);

    return vbcppDefineInsert(pThis, pDef);
}


/**
 * Adds a define.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name and optionally the argument
 *                              list.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 * @param   pszValue            The value.
 * @param   cchDefine           The length of the value. RTSTR_MAX is ok.
 */
static RTEXITCODE vbcppDefineAdd(PVBCPP pThis, const char *pszDefine, size_t cchDefine,
                                 const char *pszValue, size_t cchValue)
{
    /*
     * We need the lengths. Trim the input.
     */
    if (cchDefine == RTSTR_MAX)
        cchDefine = strlen(pszDefine);
    while (cchDefine > 0 && RT_C_IS_SPACE(*pszDefine))
        pszDefine++, cchDefine--;
    while (cchDefine > 0 && RT_C_IS_SPACE(pszDefine[cchDefine - 1]))
        cchDefine--;
    if (!cchDefine)
        return vbcppErrorPos(pThis, pszDefine, "The define has no name");

    if (cchValue == RTSTR_MAX)
        cchValue = strlen(pszValue);
    while (cchValue > 0 && RT_C_IS_SPACE(*pszValue))
        pszValue++, cchValue--;
    while (cchValue > 0 && RT_C_IS_SPACE(pszValue[cchValue - 1]))
        cchValue--;

    /*
     * Arguments make the job a bit more annoying.  Handle that elsewhere
     */
    const char *pszParams = (const char *)memchr(pszDefine, '(', cchDefine);
    if (pszParams)
    {
        size_t cchParams = pszDefine + cchDefine - pszParams;
        cchDefine -= cchParams;
        if (!vbcppValidateCIdentifier(pThis, pszDefine, cchDefine))
            return RTEXITCODE_FAILURE;
        if (pszParams[cchParams - 1] != ')')
            return vbcppErrorPos(pThis, pszParams + cchParams - 1, "Missing closing parenthesis");
        pszParams++;
        cchParams -= 2;
        return vbcppDefineAddFn(pThis, pszDefine, cchDefine, pszParams, cchParams, pszValue, cchValue);
    }

    /*
     * Simple define, no arguments.
     */
    if (!vbcppValidateCIdentifier(pThis, pszDefine, cchDefine))
        return RTEXITCODE_FAILURE;

    PVBCPPDEF pDef = (PVBCPPDEF)RTMemAlloc(RT_OFFSETOF(VBCPPDEF, szValue[cchValue + 1 + cchDefine + 1]));
    if (!pDef)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "out of memory");

    pDef->Core.pszString = &pDef->szValue[cchValue + 1];
    memcpy((char *)pDef->Core.pszString, pszDefine, cchDefine);
    ((char *)pDef->Core.pszString)[cchDefine] = '\0';
    pDef->fFunction = false;
    pDef->fVarArg   = false;
    pDef->cArgs     = 0;
    pDef->papszArgs = NULL;
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';

    return vbcppDefineInsert(pThis, pDef);
}


/**
 * Checks if a define exists.
 *
 * @returns true or false.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name and optionally the argument
 *                              list.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 */
static bool vbcppDefineExists(PVBCPP pThis, const char *pszDefine, size_t cchDefine)
{
    return cchDefine > 0
        && VBCPP_BITMAP_IS_SET(pThis->bmDefined, *pszDefine)
        && RTStrSpaceGetN(&pThis->StrSpace, pszDefine, cchDefine) != NULL;
}


/**
 * Adds an include directory.
 *
 * @returns Program exit code, with error message on failure.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDir              The directory to add.
 */
static RTEXITCODE vbcppAddInclude(PVBCPP pThis, const char *pszDir)
{
    uint32_t cIncludes = pThis->cIncludes;
    if (cIncludes >= _64K)
        return vbcppError(pThis, "Too many include directories");

    void *pv = RTMemRealloc(pThis->papszIncludes, (cIncludes + 1) * sizeof(char **));
    if (!pv)
        return vbcppError(pThis, "No memory for include directories");
    pThis->papszIncludes = (char **)pv;

    int rc = RTStrDupEx(&pThis->papszIncludes[cIncludes], pszDir);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "No string memory for include directories");

    pThis->cIncludes = cIncludes + 1;
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
        { "--keep-comments",            'C',                    RTGETOPT_REQ_NOTHING },
        { "--strip-comments",           'c',                    RTGETOPT_REQ_NOTHING },
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
            case 'c':
                pThis->fKeepComments = false;
                break;

            case 'C':
                pThis->fKeepComments = false;
                break;

            case 'd':
                pThis->enmMode = kVBCppMode_SelectiveD;
                pThis->fKeepComments = true;
                break;

            case 'D':
            {
                const char *pszEqual = strchr(ValueUnion.psz, '=');
                if (pszEqual)
                    rcExit = vbcppDefineAdd(pThis, ValueUnion.psz, pszEqual - ValueUnion.psz, pszEqual + 1, RTSTR_MAX);
                else
                    rcExit = vbcppDefineAdd(pThis, ValueUnion.psz, RTSTR_MAX, "1", 1);
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
                rcExit = vbcppDefineUndef(pThis, ValueUnion.psz, RTSTR_MAX, true);
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


/**
 * Opens the input and output streams.
 *
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance.
 */
static RTEXITCODE vbcppOpenStreams(PVBCPP pThis)
{
    if (!pThis->pszInput)
        return vbcppError(pThis, "Preprocessing the standard input stream is currently not supported");

    size_t      cchName = strlen(pThis->pszInput);
    PVBCPPINPUT pInput = (PVBCPPINPUT)RTMemAlloc(RT_OFFSETOF(VBCPPINPUT, szName[cchName + 1]));
    if (!pInput)
        return vbcppError(pThis, "out of memory");
    pInput->pUp          = pThis->pInputStack;
    pInput->pszSpecified = pInput->szName;
    memcpy(pInput->szName, pThis->pszInput, cchName + 1);
    pThis->pInputStack   = pInput;
    int rc = ScmStreamInitForReading(&pInput->StrmInput, pThis->pszInput);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "ScmStreamInitForReading returned %Rrc when opening input file (%s)",
                          rc, pThis->pszInput);

    rc = ScmStreamInitForWriting(&pThis->StrmOutput, &pInput->StrmInput);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "ScmStreamInitForWriting returned %Rrc", rc);

    pThis->fStrmOutputValid = true;
    return RTEXITCODE_SUCCESS;
}


/**
 * Outputs a character.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   ch                  The character to output.
 */
static RTEXITCODE vbcppOutputCh(PVBCPP pThis, char ch)
{
    int rc = ScmStreamPutCh(&pThis->StrmOutput, ch);
    if (RT_SUCCESS(rc))
        return RTEXITCODE_SUCCESS;
    return vbcppError(pThis, "Output error: %Rrc", rc);
}


/**
 * Outputs a string.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pch                 The string.
 * @param   cch                 The number of characters to write.
 */
static RTEXITCODE vbcppOutputWrite(PVBCPP pThis, const char *pch, size_t cch)
{
    int rc = ScmStreamWrite(&pThis->StrmOutput, pch, cch);
    if (RT_SUCCESS(rc))
        return RTEXITCODE_SUCCESS;
    return vbcppError(pThis, "Output error: %Rrc", rc);
}


static RTEXITCODE vbcppOutputComment(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart, size_t cchOutputted)
{
    size_t offCur = ScmStreamTell(pStrmInput);
    if (offStart < offCur)
    {
        int rc = ScmStreamSeekAbsolute(pStrmInput, offStart);
        AssertRCReturn(rc, vbcppError(pThis, "Input seek error: %Rrc", rc));

        /*
         * Use the same indent, if possible.
         */
        size_t cchIndent = offStart - ScmStreamTellOffsetOfLine(pStrmInput, ScmStreamTellLine(pStrmInput));
        if (cchOutputted < cchIndent)
            rc = ScmStreamPrintf(&pThis->StrmOutput, "%*s", cchIndent - cchOutputted, "");
        else
            rc = ScmStreamPutCh(&pThis->StrmOutput, ' ');
        if (RT_FAILURE(rc))
            return vbcppError(pThis, "Output error: %Rrc", rc);

        /*
         * Copy the bytes.
         */
        while (ScmStreamTell(pStrmInput) < offCur)
        {
            unsigned ch = ScmStreamGetCh(pStrmInput);
            if (ch == ~(unsigned)0)
                return vbcppError(pThis, "Input error: %Rrc", rc);
            rc = ScmStreamPutCh(&pThis->StrmOutput, ch);
            if (RT_FAILURE(rc))
                return vbcppError(pThis, "Output error: %Rrc", rc);
        }
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Processes a multi-line comment.
 *
 * Must either string the comment or keep it. If the latter, we must refrain
 * from replacing C-words in it.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessMultiLineComment(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    /* The open comment sequence. */
    ScmStreamGetCh(pStrmInput);         /* '*' */
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (   pThis->fKeepComments
        && !pThis->fIf0Mode)
        rcExit = vbcppOutputWrite(pThis, "/*", 2);

    /* The comment.*/
    unsigned ch;
    while (   rcExit == RTEXITCODE_SUCCESS
           && (ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0 )
    {
        if (ch == '*')
        {
            /* Closing sequence? */
            unsigned ch2 = ScmStreamPeekCh(pStrmInput);
            if (ch2 == '/')
            {
                ScmStreamGetCh(pStrmInput);
                if (   pThis->fKeepComments
                    && !pThis->fIf0Mode)
                    rcExit = vbcppOutputWrite(pThis, "*/", 2);
                break;
            }
        }

        if (   (   pThis->fKeepComments
                && !pThis->fIf0Mode)
            || ch == '\r'
            || ch == '\n')
        {
            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            /* Reset the maybe-preprocessor-line indicator when necessary. */
            if (ch == '\r' || ch == '\n')
                pThis->fMaybePreprocessorLine = true;
        }
    }
    return rcExit;
}


/**
 * Processes a single line comment.
 *
 * Must either string the comment or keep it. If the latter, we must refrain
 * from replacing C-words in it.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessOneLineComment(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE  rcExit;
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pszLine = ScmStreamGetLine(pStrmInput, &cchLine, &enmEol); Assert(pszLine);
    pszLine--; cchLine++;               /* unfetching the first slash. */
    for (;;)
    {
        if (   pThis->fKeepComments
            && !pThis->fIf0Mode)
            rcExit = vbcppOutputWrite(pThis, pszLine, cchLine + enmEol);
        else
            rcExit = vbcppOutputWrite(pThis, pszLine + cchLine, enmEol);
        if (rcExit != RTEXITCODE_SUCCESS)
            break;
        if (   cchLine == 0
            || pszLine[cchLine - 1] != '\\')
            break;

        pszLine = ScmStreamGetLine(pStrmInput, &cchLine, &enmEol);
        if (!pszLine)
            break;
    }
    pThis->fMaybePreprocessorLine = true;
    return rcExit;
}


/**
 * Processes a double quoted string.
 *
 * Must not replace any C-words in strings.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessDoubleQuotedString(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE rcExit = vbcppOutputCh(pThis, '"');
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        bool fEscaped = false;
        for (;;)
        {
            unsigned ch = ScmStreamGetCh(pStrmInput);
            if (ch == ~(unsigned)0)
            {
                rcExit = vbcppError(pThis, "Unterminated double quoted string");
                break;
            }

            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            if (ch == '"' && !fEscaped)
                break;
            fEscaped = !fEscaped && ch == '\\';
        }
    }
    return rcExit;
}


/**
 * Processes a single quoted litteral.
 *
 * Must not replace any C-words in strings.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessSingledQuotedString(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE rcExit = vbcppOutputCh(pThis, '\'');
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        bool fEscaped = false;
        for (;;)
        {
            unsigned ch = ScmStreamGetCh(pStrmInput);
            if (ch == ~(unsigned)0)
            {
                rcExit = vbcppError(pThis, "Unterminated singled quoted string");
                break;
            }

            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            if (ch == '\'' && !fEscaped)
                break;
            fEscaped = !fEscaped && ch == '\\';
        }
    }
    return rcExit;
}


/**
 * Skips white spaces, including escaped new-lines.
 *
 * @param   pStrmInput          The input stream.
 */
static void vbcppProcessSkipWhiteAndEscapedEol(PSCMSTREAM pStrmInput)
{
    unsigned chPrev = ~(unsigned)0;
    unsigned ch;
    while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
    {
        if (ch == '\r' || ch == '\n')
        {
            if (chPrev != '\\')
                break;
            chPrev = ch;
            ScmStreamSeekByLine(pStrmInput, ScmStreamTellLine(pStrmInput) + 1);
        }
        else if (RT_C_IS_SPACE(ch))
        {
            ch = chPrev;
            ch = ScmStreamGetCh(pStrmInput);
            Assert(ch == chPrev);
        }
        else
            break;
    }
}


/**
 * Skips white spaces, escaped new-lines and multi line comments.
 *
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessSkipWhiteEscapedEolAndComments(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    unsigned chPrev = ~(unsigned)0;
    unsigned ch;
    while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
    {
        if (!RT_C_IS_SPACE(ch))
        {
            /* Multi-line Comment? */
            if (ch != '/')
                break;                  /* most definitely, not. */

            size_t offSaved = ScmStreamTell(pStrmInput);
            ScmStreamGetCh(pStrmInput);
            if (ScmStreamPeekCh(pStrmInput) != '*')
            {
                ScmStreamSeekAbsolute(pStrmInput, offSaved);
                break;              /* no */
            }

            /* Skip to the end of the comment. */
            while ((ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0)
            {
                if (ch == '*')
                {
                    ch = ScmStreamGetCh(pStrmInput);
                    if (ch == '/')
                        break;
                    if (ch == ~(unsigned)0)
                        break;
                }
            }
            if (ch == ~(unsigned)0)
                return vbcppError(pThis, "unterminated multi-line comment");
            chPrev = '/';
        }
        /* New line (also matched by RT_C_IS_SPACE). */
        else if (ch == '\r' || ch == '\n')
        {
            /* Stop if not escaped. */
            if (chPrev != '\\')
                break;
            chPrev = ch;
            ScmStreamSeekByLine(pStrmInput, ScmStreamTellLine(pStrmInput) + 1);
        }
        /* Real space char. */
        else
        {
            chPrev = ch;
            ch = ScmStreamGetCh(pStrmInput);
            Assert(ch == chPrev);
        }
    }
    return RTEXITCODE_SUCCESS;
}


/**
 * Skips white spaces, escaped new-lines, and multi line comments, then checking
 * that we're at the end of a line.
 *
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        unsigned ch = ScmStreamPeekCh(pStrmInput);
        if (   ch != ~(unsigned)0
            && ch != '\r'
            && ch != '\n')
            rcExit = vbcppError(pThis, "Did not expected anything more on this line");
    }
    return rcExit;
}


/**
 * Skips white spaces.
 *
 * @returns The current location upon return..
 * @param   pStrmInput          The input stream.
 */
static size_t vbcppProcessSkipWhite(PSCMSTREAM pStrmInput)
{
    unsigned ch;
    while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
    {
        if (!RT_C_IS_SPACE(ch) || ch == '\r' || ch == '\n')
            break;
        unsigned chCheck = ScmStreamGetCh(pStrmInput);
        AssertBreak(chCheck == ch);
    }
    return ScmStreamTell(pStrmInput);
}



/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessInclude(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    /*
     * Parse it.
     */
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        size_t      cchFileSpec = 0;
        const char *pchFileSpec = NULL;
        size_t      cchFilename = 0;
        const char *pchFilename = NULL;

        unsigned ch = ScmStreamPeekCh(pStrmInput);
        unsigned chType = ch;
        if (ch == '"' || ch == '<')
        {
            ScmStreamGetCh(pStrmInput);
            pchFileSpec = pchFilename = ScmStreamGetCur(pStrmInput);
            unsigned chEnd  = chType == '<' ? '>' : '"';
            unsigned chPrev = ch;
            while (   (ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0
                   &&  ch != chEnd)
            {
                if (ch == '\r' || ch == '\n')
                {
                    rcExit = vbcppError(pThis, "Multi-line include file specfications are not supported");
                    break;
                }
            }

            if (rcExit == RTEXITCODE_SUCCESS)
            {
                if (ch != ~(unsigned)0)
                    cchFileSpec = cchFilename = ScmStreamGetCur(pStrmInput) - pchFilename - 1;
                else
                    rcExit = vbcppError(pThis, "Expected '%c'", chType);
            }
        }
        else if (vbcppIsCIdentifierLeadChar(ch))
        {
            //pchFileSpec = ScmStreamCGetWord(pStrmInput, &cchFileSpec);
            rcExit = vbcppError(pThis, "Including via a define is not implemented yet");
        }
        else
            rcExit = vbcppError(pThis, "Malformed include directive");

        /*
         * Take down the location of the next non-white space, in case we need
         * to pass thru the directive further down. Then skip to the end of the
         * line.
         */
        size_t const offIncEnd = vbcppProcessSkipWhite(pStrmInput);
        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(pThis, pStrmInput);

        if (rcExit == RTEXITCODE_SUCCESS)
        {
            /*
             * Execute it.
             */
            if (pThis->enmMode < kVBCppMode_Selective)
            {
                /** @todo Search for the include file and push it onto the input stack.
                 *  Not difficult, just unnecessary rigth now. */
                rcExit = vbcppError(pThis, "Includes are fully implemented");
            }
            else if (pThis->enmMode != kVBCppMode_SelectiveD)
            {
                /* Pretty print the passthru. */
                unsigned cchIndent = pThis->pCondStack ? pThis->pCondStack->iKeepLevel : 0;
                size_t cch;
                if (chType == '<')
                    cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sinclude <%.*s>",
                                          cchIndent, "", cchFileSpec, pchFileSpec);
                else if (chType == '"')
                    cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sinclude \"%.*s\"",
                                          cchIndent, "", cchFileSpec, pchFileSpec);
                else
                    cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sinclude %.*s",
                                          cchIndent, "", cchFileSpec, pchFileSpec);
                if (cch > 0)
                    rcExit = vbcppOutputComment(pThis, pStrmInput, offIncEnd, cch);
                else
                    rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);

            }
            /* else: drop it */
        }
    }
    return rcExit;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessDefine(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented %s", __FUNCTION__);
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessUndef(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented %s", __FUNCTION__);
}


static VBCPPEVAL vbcppCondCombine(VBCPPEVAL enmEvalPush, VBCPPEVAL enmEvalTop)
{
    if (enmEvalTop == kVBCppEval_False)
        return kVBCppEval_False;
    return enmEvalPush;
}


static RTEXITCODE vbcppCondPush(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart,
                                VBCPPCONDKIND enmKind, VBCPPEVAL enmResult,
                                const char *pchCondition, size_t cchCondition)
{
    if (pThis->cCondStackDepth >= _64K)
        return vbcppError(pThis, "Too many nested #if/#ifdef/#ifndef statements");

    /*
     * Allocate a new entry and push it.
     */
    PVBCPPCOND pCond = (PVBCPPCOND)RTMemAlloc(sizeof(*pCond));
    if (!pCond)
        return vbcppError(pThis, "out of memory");

    PVBCPPCOND pUp = pThis->pCondStack;
    pCond->enmKind          = enmKind;
    pCond->enmResult        = enmResult;
    pCond->enmStackResult   = pUp ? vbcppCondCombine(enmResult, pUp->enmStackResult) : enmResult;
    pCond->fSeenElse        = false;
    pCond->iLevel           = pThis->cCondStackDepth;
    pCond->iKeepLevel       = (pUp ? pUp->iKeepLevel : 0) + enmResult == kVBCppEval_Undecided;
    pCond->pchCond          = pchCondition;
    pCond->cchCond          = cchCondition;

    pCond->pUp              = pThis->pCondStack;
    pThis->pCondStack       = pCond;
    pThis->fIf0Mode         = pCond->enmStackResult == kVBCppEval_False;

    /*
     * Do pass thru.
     */
    if (   !pThis->fIf0Mode
        && enmResult == kVBCppEval_Undecided)
    {
        /** @todo this is stripping comments of \#ifdef and \#ifndef atm. */
        const char *pszDirective;
        switch (enmKind)
        {
            case kVBCppCondKind_If:     pszDirective = "if"; break;
            case kVBCppCondKind_IfDef:  pszDirective = "ifdef"; break;
            case kVBCppCondKind_IfNDef: pszDirective = "ifndef"; break;
            case kVBCppCondKind_ElIf:   pszDirective = "elif"; break;
            default: AssertFailedReturn(RTEXITCODE_FAILURE);
        }
        ssize_t cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*s%s %.*s",
                                      pCond->iKeepLevel - 1, "", pszDirective, cchCondition, pchCondition);
        if (cch < 0)
            return vbcppError(pThis, "Output error %Rrc", (int)cch);
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessIf(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented %s", __FUNCTION__);
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessIfDef(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    /*
     * Parse it.
     */
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        size_t      cchDefine;
        const char *pchDefine = ScmStreamCGetWord(pStrmInput, &cchDefine);
        if (pchDefine)
        {
            rcExit = vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(pThis, pStrmInput);
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                /*
                 * Evaluate it.
                 */
                VBCPPEVAL enmEval;
                if (vbcppDefineExists(pThis, pchDefine, cchDefine))
                    enmEval = kVBCppEval_True;
                else if (   pThis->enmMode < kVBCppMode_Selective
                         || RTStrSpaceGetN(&pThis->UndefStrSpace, pchDefine, cchDefine) != NULL)
                    enmEval = kVBCppEval_False;
                else
                    enmEval = kVBCppEval_Undecided;
                rcExit = vbcppCondPush(pThis, pStrmInput, offStart, kVBCppCondKind_IfDef, enmEval,
                                       pchDefine, cchDefine);
            }
        }
        else
            rcExit = vbcppError(pThis, "Malformed #ifdef");
    }
    return rcExit;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessIfNDef(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    /*
     * Parse it.
     */
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        size_t      cchDefine;
        const char *pchDefine = ScmStreamCGetWord(pStrmInput, &cchDefine);
        if (pchDefine)
        {
            rcExit = vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(pThis, pStrmInput);
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                /*
                 * Evaluate it.
                 */
                VBCPPEVAL enmEval;
                if (vbcppDefineExists(pThis, pchDefine, cchDefine))
                    enmEval = kVBCppEval_False;
                else if (   pThis->enmMode < kVBCppMode_Selective
                         || RTStrSpaceGetN(&pThis->UndefStrSpace, pchDefine, cchDefine) != NULL)
                    enmEval = kVBCppEval_True;
                else
                    enmEval = kVBCppEval_Undecided;
                rcExit = vbcppCondPush(pThis, pStrmInput, offStart, kVBCppCondKind_IfNDef, enmEval,
                                       pchDefine, cchDefine);
            }
        }
        else
            rcExit = vbcppError(pThis, "Malformed #ifdef");
    }
    return rcExit;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessElse(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    /*
     * Nothing to parse, just comment positions to find and note down.
     */
    offStart = vbcppProcessSkipWhite(pStrmInput);
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Execute.
         */
        PVBCPPCOND pCond = pThis->pCondStack;
        if (pCond)
        {
            if (!pCond->fSeenElse)
            {
                pCond->fSeenElse = true;
                if (   pCond->enmResult != kVBCppEval_Undecided
                    && (   !pCond->pUp
                        || pCond->pUp->enmStackResult == kVBCppEval_True))
                {
                    if (pCond->enmResult == kVBCppEval_True)
                        pCond->enmStackResult = kVBCppEval_False;
                    else
                        pCond->enmStackResult = kVBCppEval_True;
                    pThis->fIf0Mode = pCond->enmStackResult == kVBCppEval_False;
                }

                /*
                 * Do pass thru.
                 */
                if (   !pThis->fIf0Mode
                    && pCond->enmResult == kVBCppEval_Undecided)
                {
                    ssize_t cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*selse", pCond->iKeepLevel - 1, "");
                    if (cch > 0)
                        rcExit = vbcppOutputComment(pThis, pStrmInput, offStart, cch);
                    else
                        rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);
                }
            }
            else
                rcExit = vbcppError(pThis, "Double #else or/and missing #endif");
        }
        else
            rcExit = vbcppError(pThis, "#else without #if");
    }
    return rcExit;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessEndif(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    /*
     * Nothing to parse, just comment positions to find and note down.
     */
    offStart = vbcppProcessSkipWhite(pStrmInput);
    RTEXITCODE rcExit = vbcppProcessSkipWhiteEscapedEolAndCommentsCheckEol(pThis, pStrmInput);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Execute.
         */
        PVBCPPCOND pCond = pThis->pCondStack;
        if (pCond)
        {
            pThis->pCondStack = pCond->pUp;
            pThis->fIf0Mode = pCond->pUp && pCond->pUp->enmStackResult == kVBCppEval_False;

            /*
             * Do pass thru.
             */
            if (   !pThis->fIf0Mode
                && pCond->enmResult == kVBCppEval_Undecided)
            {
                ssize_t cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sendif", pCond->iKeepLevel - 1, "");
                if (cch > 0)
                    rcExit = vbcppOutputComment(pThis, pStrmInput, offStart, cch);
                else
                    rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);
            }
        }
        else
            rcExit = vbcppError(pThis, "#endif without #if");
    }
    return rcExit;
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessPragma(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented: %s", __FUNCTION__);
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   offStart            The stream position where the directive
 *                              started (for pass thru).
 */
static RTEXITCODE vbcppProcessLineNo(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented: %s", __FUNCTION__);
}


/**
 * Processes a abbreviated line number directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessLineNoShort(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    return vbcppError(pThis, "Not implemented: %s", __FUNCTION__);
}


/**
 * Handles a preprocessor directive.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessDirective(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    /*
     * Get the directive and do a string switch on it.
     */
    RTEXITCODE  rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    size_t      cchDirective;
    const char *pchDirective = ScmStreamCGetWord(pStrmInput, &cchDirective);
    if (pchDirective)
    {
        size_t const offStart = ScmStreamTell(pStrmInput);
#define IS_DIRECTIVE(a_sz) ( sizeof(a_sz) - 1 == cchDirective && strncmp(pchDirective, a_sz, sizeof(a_sz) - 1) == 0)
        if (IS_DIRECTIVE("if"))
            rcExit = vbcppProcessIf(pThis, pStrmInput, offStart);
        else if (IS_DIRECTIVE("ifdef"))
            rcExit = vbcppProcessIfDef(pThis, pStrmInput, offStart);
        else if (IS_DIRECTIVE("ifndef"))
            rcExit = vbcppProcessIfNDef(pThis, pStrmInput, offStart);
        else if (IS_DIRECTIVE("else"))
            rcExit = vbcppProcessElse(pThis, pStrmInput, offStart);
        else if (IS_DIRECTIVE("endif"))
            rcExit = vbcppProcessEndif(pThis, pStrmInput, offStart);
        else if (!pThis->fIf0Mode)
        {
            if (IS_DIRECTIVE("include"))
                rcExit = vbcppProcessInclude(pThis, pStrmInput, offStart);
            else if (IS_DIRECTIVE("define"))
                rcExit = vbcppProcessDefine(pThis, pStrmInput, offStart);
            else if (IS_DIRECTIVE("undef"))
                rcExit = vbcppProcessUndef(pThis, pStrmInput, offStart);
            else if (IS_DIRECTIVE("pragma"))
                rcExit = vbcppProcessPragma(pThis, pStrmInput, offStart);
            else if (IS_DIRECTIVE("line"))
                rcExit = vbcppProcessLineNo(pThis, pStrmInput, offStart);
            else
                rcExit = vbcppError(pThis, "Unknown preprocessor directive '#%.*s'", cchDirective, pchDirective);
        }
#undef IS_DIRECTIVE
    }
    else if (!pThis->fIf0Mode)
    {
        /* Could it be a # <num> "file" directive? */
        unsigned ch = ScmStreamPeekCh(pStrmInput);
        if (RT_C_IS_DIGIT(ch))
            rcExit = vbcppProcessLineNoShort(pThis, pStrmInput);
        else
            rcExit = vbcppError(pThis, "Malformed preprocessor directive");
    }
    return rcExit;
}


/**
 * Processes a C word, possibly replacing it with a definition.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   ch                  The first character.
 */
static RTEXITCODE vbcppProcessCWord(PVBCPP pThis, PSCMSTREAM pStrmInput, char ch)
{
    /** @todo Implement this... */
    return vbcppOutputCh(pThis, ch);
}


/**
 * Does the actually preprocessing of the input file.
 *
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance.
 */
static RTEXITCODE vbcppPreprocess(PVBCPP pThis)
{
    RTEXITCODE  rcExit = RTEXITCODE_SUCCESS;

    /*
     * Parse.
     */
    while (pThis->pInputStack)
    {
        pThis->fMaybePreprocessorLine = true;

        PSCMSTREAM  pStrmInput = &pThis->pInputStack->StrmInput;
        unsigned    ch;
        while ((ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0)
        {
            if (ch == '/')
            {
                ch = ScmStreamPeekCh(pStrmInput);
                if (ch == '*')
                    rcExit = vbcppProcessMultiLineComment(pThis, pStrmInput);
                else if (ch == '/')
                    rcExit = vbcppProcessOneLineComment(pThis, pStrmInput);
                else
                {
                    pThis->fMaybePreprocessorLine = false;
                    if (!pThis->fIf0Mode)
                        rcExit = vbcppOutputCh(pThis, '/');
                }
            }
            else if (ch == '#' && pThis->fMaybePreprocessorLine)
            {
                rcExit = vbcppProcessDirective(pThis, pStrmInput);
                pStrmInput = &pThis->pInputStack->StrmInput;
            }
            else if (ch == '\r' || ch == '\n')
            {
                pThis->fMaybePreprocessorLine = true;
                rcExit = vbcppOutputCh(pThis, ch);
            }
            else if (RT_C_IS_SPACE(ch))
            {
                if (!pThis->fIf0Mode)
                    rcExit = vbcppOutputCh(pThis, ch);
            }
            else
            {
                pThis->fMaybePreprocessorLine = false;
                if (!pThis->fIf0Mode)
                {
                    if (ch == '"')
                        rcExit = vbcppProcessDoubleQuotedString(pThis, pStrmInput);
                    else if (ch == '\'')
                        rcExit = vbcppProcessSingledQuotedString(pThis, pStrmInput);
                    else if (vbcppIsCIdentifierLeadChar(ch))
                        rcExit = vbcppProcessCWord(pThis, pStrmInput, ch);
                    else
                        rcExit = vbcppOutputCh(pThis, ch);
                }
            }
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        /*
         * Check for errors.
         */
        if (rcExit != RTEXITCODE_SUCCESS)
            break;

        /*
         * Pop the input stack.
         */
        PVBCPPINPUT pPopped = pThis->pInputStack;
        pThis->pInputStack = pPopped->pUp;
        RTMemFree(pPopped);
    }

    return rcExit;
}


/**
 * Terminates the preprocessor.
 *
 * This may return failure if an error was delayed.
 *
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance.
 */
static RTEXITCODE vbcppTerm(PVBCPP pThis)
{
    /*
     * Flush the output first.
     */
    if (pThis->fStrmOutputValid)
    {
        if (pThis->pszOutput)
        {
            int rc = ScmStreamWriteToFile(&pThis->StrmOutput, "%s", pThis->pszOutput);
            if (RT_FAILURE(rc))
                vbcppError(pThis, "ScmStreamWriteToFile failed with %Rrc when writing '%s'", rc, pThis->pszOutput);
        }
        else
        {
            int rc = ScmStreamWriteToStdOut(&pThis->StrmOutput);
            if (RT_FAILURE(rc))
                vbcppError(pThis, "ScmStreamWriteToStdOut failed with %Rrc", rc);
        }
    }

    /*
     * Cleanup.
     */
    while (pThis->pInputStack)
    {
        ScmStreamDelete(&pThis->pInputStack->StrmInput);
        void *pvFree = pThis->pInputStack;
        pThis->pInputStack = pThis->pInputStack->pUp;
        RTMemFree(pvFree);
    }

    ScmStreamDelete(&pThis->StrmOutput);

    RTStrSpaceDestroy(&pThis->StrSpace, vbcppFreeDefine, NULL);
    pThis->StrSpace = NULL;

    uint32_t i = pThis->cIncludes;
    while (i-- > 0)
        RTStrFree(pThis->papszIncludes[i]);
    RTMemFree(pThis->papszIncludes);
    pThis->papszIncludes = NULL;

    return pThis->rcExit;
}



int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Do the job.  The code says it all.
     */
    VBCPP This;
    vbcppInit(&This);
    bool fExit;
    RTEXITCODE rcExit = vbcppParseOptions(&This, argc, argv, &fExit);
    if (!fExit && rcExit == RTEXITCODE_SUCCESS)
    {
        rcExit = vbcppOpenStreams(&This);
        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = vbcppPreprocess(&This);
    }

    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = vbcppTerm(&This);
    else
        vbcppTerm(&This);
    return rcExit;
}

