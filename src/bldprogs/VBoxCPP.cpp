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
    kVBCppMode_Standard,
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
    /** Set if originating on the command line. */
    bool                fCmdLine;
    /** The number of known arguments.*/
    uint32_t            cArgs;
    /** Pointer to a list of argument names. */
    const char        **papszArgs;
    /** Lead character bitmap for the argument names. */
    VBCPP_BITMAP_TYPE   bmArgs[VBCPP_BITMAP_SIZE];
    /** The value length. */
    size_t              cchValue;
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
 * The action to take with \#include.
 */
typedef enum VBCPPINCLUDEACTION
{
    kVBCppIncludeAction_Invalid = 0,
    kVBCppIncludeAction_Include,
    kVBCppIncludeAction_PassThru,
    kVBCppIncludeAction_Drop,
    kVBCppIncludeAction_End
} VBCPPINCLUDEACTION;


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
    /** Whether to respect source defines. */
    bool                fRespectSourceDefines;
    /** Whether to let source defines overrides the ones on the command
     *  line. */
    bool                fAllowRedefiningCmdLineDefines;
    /** Whether to pass thru defines. */
    bool                fPassThruDefines;
    /** Whether to allow undecided conditionals. */
    bool                fUndecidedConditionals;
    /** Whether to preforme line splicing.
     * @todo implement line splicing  */
    bool                fLineSplicing;
    /** What to do about include files. */
    VBCPPINCLUDEACTION  enmIncludeAction;

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





/*
 *
 *
 * Message Handling.
 * Message Handling.
 * Message Handling.
 * Message Handling.
 * Message Handling.
 *
 *
 */


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





/*
 *
 *
 * C Identifier/Word Parsing.
 * C Identifier/Word Parsing.
 * C Identifier/Word Parsing.
 * C Identifier/Word Parsing.
 * C Identifier/Word Parsing.
 *
 *
 */


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






/*
 *
 *
 * Output
 * Output
 * Output
 * Output
 * Output
 *
 *
 */


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


static RTEXITCODE vbcppOutputComment(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart, size_t cchOutputted,
                                     size_t cchMinIndent)
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





/*
 *
 *
 * Input
 * Input
 * Input
 * Input
 * Input
 *
 *
 */


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







/*
 *
 *
 * D E F I N E S
 * D E F I N E S
 * D E F I N E S
 * D E F I N E S
 * D E F I N E S
 *
 *
 */


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
 * Looks up a define.
 *
 * @returns Pointer to the define if found, NULL if not.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name and optionally the argument
 *                              list.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 */
static PVBCPPDEF vbcppDefineLookup(PVBCPP pThis, const char *pszDefine, size_t cchDefine)
{
    if (!cchDefine)
        return NULL;
    if (!VBCPP_BITMAP_IS_SET(pThis->bmDefined, *pszDefine))
        return NULL;
    return (PVBCPPDEF)RTStrSpaceGetN(&pThis->StrSpace, pszDefine, cchDefine);
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
 * Inserts a define (rejecting and freeing it in some case).
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pDef                The define to insert.
 */
static RTEXITCODE vbcppDefineInsert(PVBCPP pThis, PVBCPPDEF pDef)
{
    /*
     * Reject illegal macro names.
     */
    if (!strcmp(pDef->Core.pszString, "defined"))
    {
        RTEXITCODE rcExit = vbcppError(pThis, "Cannot use '%s' as a macro name", pDef->Core.pszString);
        vbcppFreeDefine(&pDef->Core, NULL);
        return rcExit;
    }

    /*
     * Ignore in source-file defines when doing selective preprocessing.
     */
    if (   !pThis->fRespectSourceDefines
        && !pDef->fCmdLine)
    {
        /* Ignore*/
        vbcppFreeDefine(&pDef->Core, NULL);
        return RTEXITCODE_SUCCESS;
    }

    /*
     * Insert it and update the lead character hint bitmap.
     */
    if (RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core))
        VBCPP_BITMAP_SET(pThis->bmDefined, *pDef->Core.pszString);
    else
    {
        /*
         * Duplicate. When doing selective D preprocessing, let the command
         * line take precendece.
         */
        PVBCPPDEF pOld = (PVBCPPDEF)RTStrSpaceGet(&pThis->StrSpace, pDef->Core.pszString); Assert(pOld);
        if (   pThis->fAllowRedefiningCmdLineDefines
            || pDef->fCmdLine == pOld->fCmdLine)
        {
            if (pDef->fCmdLine)
                RTMsgWarning("Redefining '%s'\n", pDef->Core.pszString);

            RTStrSpaceRemove(&pThis->StrSpace, pOld->Core.pszString);
            vbcppFreeDefine(&pOld->Core, NULL);

            bool fRc = RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core);
            Assert(fRc);
        }
        else
        {
            RTMsgWarning("Ignoring redefinition of '%s'\n", pDef->Core.pszString);
            vbcppFreeDefine(&pDef->Core, NULL);
        }
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
 * @param   fCmdLine            Set if originating on the command line.
 */
static RTEXITCODE vbcppDefineAddFn(PVBCPP pThis, const char *pszDefine, size_t cchDefine,
                                   const char *pszParams, size_t cchParams,
                                   const char *pszValue, size_t cchValue,
                                   bool fCmdLine)

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
    pDef->fCmdLine  = fCmdLine;
    pDef->cArgs     = cArgs;
    pDef->papszArgs = (const char **)((uintptr_t)pDef + cbDef - sizeof(const char *) * cArgs);
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    pDef->cchValue  = cchValue;
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
 * @param   fCmdLine            Set if originating on the command line.
 */
static RTEXITCODE vbcppDefineAdd(PVBCPP pThis, const char *pszDefine, size_t cchDefine,
                                 const char *pszValue, size_t cchValue, bool fCmdLine)
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
        return vbcppDefineAddFn(pThis, pszDefine, cchDefine, pszParams, cchParams, pszValue, cchValue, fCmdLine);
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
    pDef->fCmdLine  = fCmdLine;
    pDef->cArgs     = 0;
    pDef->papszArgs = NULL;
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    pDef->cchValue  = cchValue;
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';

    return vbcppDefineInsert(pThis, pDef);
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
            /* If it's a function style define, parse out the parameter list. */
            size_t      cchParams = 0;
            const char *pchParams = NULL;
            unsigned    ch = ScmStreamPeekCh(pStrmInput);
            if (ch == '(')
            {
                ScmStreamGetCh(pStrmInput);
                pchParams = ScmStreamGetCur(pStrmInput);

                unsigned chPrev = ch;
                while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
                {
                    if (ch == '\r' || ch == '\n')
                    {
                        if (chPrev != '\\')
                        {
                            rcExit = vbcppError(pThis, "Missing ')'");
                            break;
                        }
                        ScmStreamSeekByLine(pStrmInput, ScmStreamTellLine(pStrmInput) + 1);
                    }
                    if (ch == ')')
                    {
                        cchParams = ScmStreamGetCur(pStrmInput) - pchParams;
                        ScmStreamGetCh(pStrmInput);
                        break;
                    }
                    ScmStreamGetCh(pStrmInput);
                }
            }
            /* The simple kind. */
            else if (!RT_C_IS_SPACE(ch) && ch != ~(unsigned)0)
                rcExit = vbcppError(pThis, "Expected whitespace after macro name");

            /* Parse out the value. */
            if (rcExit == RTEXITCODE_SUCCESS)
                rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                size_t      offValue = ScmStreamTell(pStrmInput);
                const char *pchValue = ScmStreamGetCur(pStrmInput);
                unsigned    chPrev = ch;
                while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
                {
                    if (ch == '\r' || ch == '\n')
                    {
                        if (chPrev != '\\')
                            break;
                        ScmStreamSeekByLine(pStrmInput, ScmStreamTellLine(pStrmInput) + 1);
                    }
                    chPrev = ScmStreamGetCh(pStrmInput);
                }
                size_t cchValue = ScmStreamGetCur(pStrmInput) - pchValue;

                /*
                 * Execute.
                 */
                if (pchParams)
                    rcExit = vbcppDefineAddFn(pThis, pchDefine, cchDefine, pchParams, cchParams, pchValue, cchValue, false);
                else
                    rcExit = vbcppDefineAdd(pThis, pchDefine, cchDefine, pchValue, cchValue, false);

                /*
                 * Pass thru?
                 */
                if (   rcExit == RTEXITCODE_SUCCESS
                    && pThis->fPassThruDefines)
                {
                    unsigned cchIndent = pThis->pCondStack ? pThis->pCondStack->iKeepLevel : 0;
                    size_t   cch;
                    if (pchParams)
                        cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sdefine %.*s(%.*s)",
                                              cchIndent, "", cchDefine, pchDefine, cchParams, pchParams);
                    else
                        cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*sdefine %.*s",
                                              cchIndent, "", cchDefine, pchDefine);
                    if (cch > 0)
                        vbcppOutputComment(pThis, pStrmInput, offValue, cch, 1);
                    else
                        rcExit = vbcppError(pThis, "output error");
                }
            }

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
static RTEXITCODE vbcppProcessUndef(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart)
{
    return vbcppError(pThis, "Not implemented %s", __FUNCTION__);
}





/*
 *
 *
 * C O N D I T I O N A L S
 * C O N D I T I O N A L S
 * C O N D I T I O N A L S
 * C O N D I T I O N A L S
 * C O N D I T I O N A L S
 *
 *
 */


/**
 * Combines current stack result with the one being pushed.
 *
 * @returns Combined result.
 * @param   enmEvalPush         The result of the condition being pushed.
 * @param   enmEvalStack        The current stack result.
 */
static VBCPPEVAL vbcppCondCombine(VBCPPEVAL enmEvalPush, VBCPPEVAL enmEvalStack)
{
    if (enmEvalStack == kVBCppEval_False)
        return kVBCppEval_False;
    return enmEvalPush;
}


/**
 * Pushes an conditional onto the stack.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The current input stream.
 * @param   offStart            Not currently used, using @a pchCondition and
 *                              @a cchCondition instead.
 * @param   enmKind             The kind of conditional.
 * @param   enmResult           The result of the evaluation.
 * @param   pchCondition        The raw condition.
 * @param   cchCondition        The length of @a pchCondition.
 */
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


#if 0
typedef enum VBCPPEXPRKIND
{
    kVBCppExprKind_Invalid = 0,
    kVBCppExprKind_Binary,
    kVBCppExprKind_Unary,
    kVBCppExprKind_SignedValue,
    kVBCppExprKind_UnsignedValue,
    kVBCppExprKind_Define,
    kVBCppExprKind_End
} VBCPPEXPRKIND;

typedef struct VBCPPEXPR *PVBCPPEXPR;

/**
 * Expression parsing structure.
 */
typedef struct VBCPPEXPR
{
    /** Pointer to the first byte of the expression. */
    const char         *pchExpr;
    /** The length of the expression. */
    size_t              cchExpr;

    uint32_t            iDepth;
    /** The kind of expression. */
    VBCPPEXPRKIND       enmKind;
    /** */
    union
    {
        struct
        {
            VBCPPUNARYOP    enmOperator;
            PVBCPPEXPR      pArg;
        } Unary;

        struct
        {
            VBCPPBINARYOP   enmOperator;
            PVBCPPEXPR      pLeft;
            PVBCPPEXPR      pRight;
        } Binary;

        struct
        {
            int64_t         s64;
            unsigned        cBits;
        } SignedValue;

        struct
        {
            uint64_t        u64;
            unsigned        cBits;
        } UnsignedValue;

        struct
        {
            const char     *pch;
            size_t          cch;
        } Define;

    } u;
    /** Parent expression. */
    PVBCPPEXPR          pParent;
} VBCPPEXPR;



typedef struct VBCPPEXPRPARSER
{
    PVBCPPEXPR          pStack
} VBCPPEXPRPARSER;


/**
 * Operator return statuses.
 */
typedef enum
{
    kExprRet_Error = -1,
    kExprRet_Ok = 0,
    kExprRet_Operator,
    kExprRet_Operand,
    kExprRet_EndOfExpr,
    kExprRet_End
} VBCPPEXPRRET;


static VBCPPEXPRRET vbcppExprEatUnaryOrOperand(PVBCPPEXPRPARSER pThis, PSCMSTREAM pStrmInput)
{

}

#endif


/**
 * Evalutes the expression.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszExpr             The expression.
 * @param   cchExpr             The length of the expression.
 * @param   penmResult          Where to store the result.
 */
static RTEXITCODE vbcppExprEval(PVBCPP pThis, char *pszExpr, size_t cchExpr, VBCPPEVAL *penmResult)
{
    Assert(strlen(pszExpr) == cchExpr);
#if 0
    /** @todo */
#else           /* Greatly simplified for getting DTrace working. */
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (strcmp(pszExpr, "1"))
        *penmResult = kVBCppEval_True;
    else if (strcmp(pszExpr, "0"))
        *penmResult = kVBCppEval_False;
    else if (pThis->fUndecidedConditionals)
        *penmResult = kVBCppEval_Undecided;
    else
        rcExit = vbcppError(pThis, "Too compliated expression '%s'", pszExpr);
#endif
    return rcExit;
}


/**
 * Expands known macros in the expression.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   ppszExpr            The expression to expand. Input/Output.
 * @param   pcchExpr            The length of the expression. Input/Output.
 * @param   pcReplacements      Where to store the number of replacements made.
 *                              Optional.
 */
static RTEXITCODE vbcppExprExpand(PVBCPP pThis, char **ppszExpr, size_t *pcchExpr, size_t *pcReplacements)
{
    RTEXITCODE  rcExit      = RTEXITCODE_SUCCESS;
    char       *pszExpr     = *ppszExpr;
    size_t      cchExpr     = pcchExpr ? *pcchExpr :  strlen(pszExpr);
    size_t      cbExprAlloc = cchExpr + 1;
    size_t      cHits       = 0;
    size_t      off         = 0;
    char        ch;
    while ((ch = pszExpr[off]) != '\0')
    {
        if (!vbcppIsCIdentifierLeadChar(ch))
            off++;
        else
        {
            /* Extract the identifier. */
            size_t const offIdentifier = off++;
            while (   off < cchExpr
                   && vbcppIsCIdentifierChar(pszExpr[off]))
                off++;
            size_t const cchIdentifier = off - offIdentifier;

            /* Does it exist?  Will save a whole lot of trouble if it doesn't. */
            PVBCPPDEF pDef = vbcppDefineLookup(pThis, &pszExpr[offIdentifier], cchIdentifier);
            if (pDef)
            {
                /* Skip white space and check for parenthesis. */
                while (   off < cchExpr
                       && RT_C_IS_SPACE(pszExpr[off]))
                    off++;
                if (   off < cchExpr
                    && pszExpr[off] == '(')
                {
                    /* Try expand function define. */
                    rcExit = vbcppError(pThis, "Expanding function macros is not yet implemented");
                    break;
                }
                else
                {
                    /* Expand simple define if found. */
                    if (pDef->cchValue + 2 < cchIdentifier)
                    {
                        size_t offDelta = cchIdentifier - pDef->cchValue - 2;
                        memmove(&pszExpr[offIdentifier], &pszExpr[offIdentifier + offDelta],
                                cchExpr - offIdentifier - offDelta + 1); /* Lazy bird is moving too much! */
                        cchExpr -= offDelta;
                    }
                    else if (pDef->cchValue + 2 > cchIdentifier)
                    {
                        size_t offDelta = pDef->cchValue + 2 - cchIdentifier;
                        if (cchExpr + offDelta + 1 > cbExprAlloc)
                        {
                            do
                            {
                                cbExprAlloc *= 2;
                            } while (cchExpr + offDelta + 1 > cbExprAlloc);
                            void *pv = RTMemRealloc(pszExpr,  cbExprAlloc);
                            if (!pv)
                            {
                                rcExit = vbcppError(pThis, "out of memory (%zu bytes)", cbExprAlloc);
                                break;
                            }
                            pszExpr = (char *)pv;
                        }
                        memmove(&pszExpr[offIdentifier + offDelta], &pszExpr[offIdentifier],
                                cchExpr - offIdentifier + 1); /* Lazy bird is moving too much! */
                        cchExpr += offDelta;
                    }

                    /* Insert with spaces around it. Not entirely sure how
                       standard compliant this is... */
                    pszExpr[offIdentifier] = ' ';
                    memcpy(&pszExpr[offIdentifier + 1], pDef->szValue, pDef->cchValue);
                    pszExpr[offIdentifier + 1 + pDef->cchValue] = ' ';

                    /* Restart parsing at the inserted macro. */
                    off = offIdentifier + 1;
                }
            }
        }
    }

    return rcExit;
}



/**
 * Extracts the expression.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   ppszExpr            Where to return the expression string..
 * @param   pcchExpr            Where to return the expression length.
 *                              Optional.
 * @param   poffComment         Where to note down the position of the final
 *                              comment. Optional.
 */
static RTEXITCODE vbcppExprExtract(PVBCPP pThis, PSCMSTREAM pStrmInput,
                                   char **ppszExpr, size_t *pcchExpr, size_t *poffComment)
{
    RTEXITCODE  rcExit      = RTEXITCODE_SUCCESS;
    size_t      cbExprAlloc = 0;
    size_t      cchExpr     = 0;
    char       *pszExpr     = NULL;
    bool        fInComment  = false;
    size_t      offComment  = ~(size_t)0;
    unsigned    chPrev      = 0;
    unsigned    ch;
    while ((ch = ScmStreamPeekCh(pStrmInput)) != ~(unsigned)0)
    {
        if (ch == '\r' || ch == '\n')
        {
            if (chPrev == '\\')
            {
                ScmStreamSeekByLine(pStrmInput, ScmStreamTellLine(pStrmInput) + 1);
                pszExpr[--cchExpr] = '\0';
                continue;
            }
            if (!fInComment)
                break;
            /* The expression continues after multi-line comments. Cool. :-) */
        }
        else if (!fInComment)
        {
            if (chPrev == '/' && ch == '*' )
            {
                pszExpr[--cchExpr] = '\0';
                fInComment = true;
                offComment = ScmStreamTell(pStrmInput) - 1;
            }
            else if (chPrev == '/' && ch == '/')
            {
                offComment = ScmStreamTell(pStrmInput) - 1;
                rcExit = vbcppProcessSkipWhiteEscapedEolAndComments(pThis, pStrmInput);
                break;                  /* done */
            }
            /* Append the char to the expression. */
            else
            {
                if (cchExpr + 2 > cbExprAlloc)
                {
                    cbExprAlloc = cbExprAlloc ? cbExprAlloc * 2 : 8;
                    void *pv = RTMemRealloc(pszExpr, cbExprAlloc);
                    if (!pv)
                    {
                        rcExit = vbcppError(pThis, "out of memory (%zu bytes)", cbExprAlloc);
                        break;
                    }
                    pszExpr = (char *)pv;
                }
                pszExpr[cchExpr++] = ch;
                pszExpr[cchExpr]   = '\0';
            }
        }
        else if (ch == '/' && chPrev == '*')
            fInComment = false;

        /* advance */
        chPrev = ch;
        ch = ScmStreamGetCh(pStrmInput); Assert(ch == chPrev);
    }

    if (rcExit == RTEXITCODE_SUCCESS)
    {
        *ppszExpr = pszExpr;
        if (pcchExpr)
            *pcchExpr = cchExpr;
        if (poffComment)
            *poffComment;
    }
    else
        RTMemFree(pszExpr);
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
 * @param   enmKind             The kind of directive we're processing.
 */
static RTEXITCODE vbcppProcessIfOrElif(PVBCPP pThis, PSCMSTREAM pStrmInput, size_t offStart,
                                       VBCPPCONDKIND enmKind)
{
    /*
     * Check for missing #if if #elif.
     */
    if (   enmKind == kVBCppCondKind_ElIf
        && !pThis->pCondStack )
        return vbcppError(pThis, "#elif without #if");

    /*
     * Extract and expand the expression string.
     */
    const char *pchCondition = ScmStreamGetCur(pStrmInput);
    char       *pszExpr;
    size_t      cchExpr;
    size_t      offComment;
    RTEXITCODE  rcExit = vbcppExprExtract(pThis, pStrmInput, &pszExpr, &cchExpr, &offComment);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        size_t const    cchCondition = ScmStreamGetCur(pStrmInput) - pchCondition;
        size_t          cReplacements;
        rcExit = vbcppExprExpand(pThis, &pszExpr, &cchExpr, &cReplacements);
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            /*
             * Strip it and check that it's not empty.
             */
            char *pszExpr2 = pszExpr;
            while (cchExpr > 0 && RT_C_IS_SPACE(*pszExpr2))
                pszExpr2++, cchExpr--;

            while (cchExpr > 0 && RT_C_IS_SPACE(pszExpr2[cchExpr - 1]))
                pszExpr2[--cchExpr] = '\0';
            if (cchExpr)
            {
                /*
                 * Now, evalute the expression.
                 */
                VBCPPEVAL enmResult;
                rcExit = vbcppExprEval(pThis, pszExpr2, cchExpr, &enmResult);
                if (rcExit == RTEXITCODE_SUCCESS)
                {
                    /*
                     * Take action.
                     */
                    if (enmKind != kVBCppCondKind_ElIf)
                        rcExit = vbcppCondPush(pThis, pStrmInput, offComment, enmKind, enmResult,
                                               pchCondition, cchCondition);
                    else
                    {
                        PVBCPPCOND pCond = pThis->pCondStack;
                        if (   pCond->enmResult != kVBCppEval_Undecided
                            && (   !pCond->pUp
                                || pCond->pUp->enmStackResult == kVBCppEval_True))
                        {
                            if (enmResult == kVBCppEval_True)
                                pCond->enmStackResult = kVBCppEval_False;
                            else
                                pCond->enmStackResult = kVBCppEval_True;
                            pThis->fIf0Mode = pCond->enmStackResult == kVBCppEval_False;
                        }
                        pCond->enmResult = enmResult;
                        pCond->pchCond   = pchCondition;
                        pCond->cchCond   = cchCondition;

                        /*
                         * Do #elif pass thru.
                         */
                        if (   !pThis->fIf0Mode
                            && pCond->enmResult == kVBCppEval_Undecided)
                        {
                            ssize_t cch = ScmStreamPrintf(&pThis->StrmOutput, "#%*selif", pCond->iKeepLevel - 1, "");
                            if (cch > 0)
                                rcExit = vbcppOutputComment(pThis, pStrmInput, offStart, cch, 2);
                            else
                                rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);
                        }
                    }
                }
            }
            else
                rcExit = vbcppError(pThis, "Empty #if expression");
        }
        RTMemFree(pszExpr);
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
                else if (   !pThis->fUndecidedConditionals
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
                else if (   !pThis->fUndecidedConditionals
                         || RTStrSpaceGetN(&pThis->UndefStrSpace, pchDefine, cchDefine) != NULL)
                    enmEval = kVBCppEval_True;
                else
                    enmEval = kVBCppEval_Undecided;
                rcExit = vbcppCondPush(pThis, pStrmInput, offStart, kVBCppCondKind_IfNDef, enmEval,
                                       pchDefine, cchDefine);
            }
        }
        else
            rcExit = vbcppError(pThis, "Malformed #ifndef");
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
                        rcExit = vbcppOutputComment(pThis, pStrmInput, offStart, cch, 2);
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
                    rcExit = vbcppOutputComment(pThis, pStrmInput, offStart, cch, 1);
                else
                    rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);
            }
        }
        else
            rcExit = vbcppError(pThis, "#endif without #if");
    }
    return rcExit;
}





/*
 *
 *
 * Misc Directives
 * Misc Directives
 * Misc Directives
 * Misc Directives
 *
 *
 */


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
            if (pThis->enmIncludeAction == kVBCppIncludeAction_Include)
            {
                /** @todo Search for the include file and push it onto the input stack.
                 *  Not difficult, just unnecessary rigth now. */
                rcExit = vbcppError(pThis, "Includes are fully implemented");
            }
            else if (pThis->enmIncludeAction == kVBCppIncludeAction_PassThru)
            {
                /* Pretty print the passthru. */
                unsigned cchIndent = pThis->pCondStack ? pThis->pCondStack->iKeepLevel : 0;
                size_t   cch;
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
                    rcExit = vbcppOutputComment(pThis, pStrmInput, offIncEnd, cch, 1);
                else
                    rcExit = vbcppError(pThis, "Output error %Rrc", (int)cch);

            }
            else
                Assert(pThis->enmIncludeAction == kVBCppIncludeAction_Drop);
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
            rcExit = vbcppProcessIfOrElif(pThis, pStrmInput, offStart, kVBCppCondKind_If);
        else if (IS_DIRECTIVE("elif"))
            rcExit = vbcppProcessIfOrElif(pThis, pStrmInput, offStart, kVBCppCondKind_ElIf);
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


/*
 *
 *
 * M a i n   b o d y.
 * M a i n   b o d y.
 * M a i n   b o d y.
 * M a i n   b o d y.
 * M a i n   b o d y.
 *
 *
 */


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
 * Changes the preprocessing mode.
 *
 * @param   pThis               The C preprocessor instance.
 * @param   enmMode             The new mode.
 */
static void vbcppSetMode(PVBCPP pThis, VBCPPMODE enmMode)
{
    switch (enmMode)
    {
        case kVBCppMode_Standard:
            pThis->fKeepComments                    = false;
            pThis->fRespectSourceDefines            = true;
            pThis->fAllowRedefiningCmdLineDefines   = true;
            pThis->fPassThruDefines                 = false;
            pThis->fUndecidedConditionals           = false;
            pThis->fLineSplicing                    = true;
            pThis->enmIncludeAction                 = kVBCppIncludeAction_Include;
            break;

        case kVBCppMode_Selective:
            pThis->fKeepComments                    = true;
            pThis->fRespectSourceDefines            = false;
            pThis->fAllowRedefiningCmdLineDefines   = false;
            pThis->fPassThruDefines                 = true;
            pThis->fUndecidedConditionals           = true;
            pThis->fLineSplicing                    = false;
            pThis->enmIncludeAction                 = kVBCppIncludeAction_PassThru;
            break;

        case kVBCppMode_SelectiveD:
            pThis->fKeepComments                    = true;
            pThis->fRespectSourceDefines            = true;
            pThis->fAllowRedefiningCmdLineDefines   = false;
            pThis->fPassThruDefines                 = false;
            pThis->fUndecidedConditionals           = false;
            pThis->fLineSplicing                    = false;
            pThis->enmIncludeAction                 = kVBCppIncludeAction_Drop;
            break;

        default:
            AssertFailedReturnVoid();
    }
    pThis->enmMode = enmMode;
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
                vbcppSetMode(pThis, kVBCppMode_SelectiveD);
                break;

            case 'D':
            {
                const char *pszEqual = strchr(ValueUnion.psz, '=');
                if (pszEqual)
                    rcExit = vbcppDefineAdd(pThis, ValueUnion.psz, pszEqual - ValueUnion.psz, pszEqual + 1, RTSTR_MAX, true);
                else
                    rcExit = vbcppDefineAdd(pThis, ValueUnion.psz, RTSTR_MAX, "1", 1, true);
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


/**
 * Initializes the C preprocessor instance data.
 *
 * @param   pThis               The C preprocessor instance data.
 */
static void vbcppInit(PVBCPP pThis)
{
    vbcppSetMode(pThis, kVBCppMode_Selective);
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

