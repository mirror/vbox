/* $Id$ */
/** @file
 * DBGC - Debugger Console, command evaluator.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/ctype.h>

#include "DBGCInternal.h"

/** Rewrite in progress.  */
#define BETTER_ARGUMENT_MATCHING


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Bitmap where set bits indicates the characters the may start an operator name. */
static uint32_t g_bmOperatorChars[256 / (4*8)];



/**
 * Initializes g_bmOperatorChars.
 */
void dbgcEvalInit(void)
{
    memset(g_bmOperatorChars, 0, sizeof(g_bmOperatorChars));
    for (unsigned iOp = 0; iOp < g_cOps; iOp++)
        ASMBitSet(&g_bmOperatorChars[0], (uint8_t)g_aOps[iOp].szName[0]);
}


/**
 * Checks whether the character may be the start of an operator.
 *
 * @returns true/false.
 * @param   ch      The character.
 */
DECLINLINE(bool) dbgcIsOpChar(char ch)
{
    return ASMBitTest(&g_bmOperatorChars[0], (uint8_t)ch);
}



static int dbgcEvalSubString(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pArg)
{
    Log2(("dbgcEvalSubString: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));

    /*
     * Removing any quoting and escapings.
     */
    char ch = *pszExpr;
    if (ch == '"' || ch == '\'')
    {
        if (pszExpr[--cchExpr] != ch)
            return VERR_DBGC_PARSE_UNBALANCED_QUOTE;
        cchExpr--;
        pszExpr++;

        /** @todo string unescaping. */
    }
    pszExpr[cchExpr] = '\0';

    /*
     * Make the argument.
     */
    pArg->pDesc         = NULL;
    pArg->pNext         = NULL;
    pArg->enmType       = DBGCVAR_TYPE_STRING;
    pArg->u.pszString   = pszExpr;
    pArg->enmRangeType  = DBGCVAR_RANGE_BYTES;
    pArg->u64Range      = cchExpr;

    NOREF(pDbgc);
    return VINF_SUCCESS;
}


static int dbgcEvalSubNum(char *pszExpr, unsigned uBase, PDBGCVAR pArg)
{
    Log2(("dbgcEvalSubNum: uBase=%d pszExpr=%s\n", uBase, pszExpr));
    /*
     * Convert to number.
     */
    uint64_t    u64 = 0;
    char        ch;
    while ((ch = *pszExpr) != '\0')
    {
        uint64_t    u64Prev = u64;
        unsigned    u = ch - '0';
        if (u < 10 && u < uBase)
            u64 = u64 * uBase + u;
        else if (ch >= 'a' && (u = ch - ('a' - 10)) < uBase)
            u64 = u64 * uBase + u;
        else if (ch >= 'A' && (u = ch - ('A' - 10)) < uBase)
            u64 = u64 * uBase + u;
        else
            return VERR_DBGC_PARSE_INVALID_NUMBER;

        /* check for overflow - ARG!!! How to detect overflow correctly!?!?!? */
        if (u64Prev != u64 / uBase)
            return VERR_DBGC_PARSE_NUMBER_TOO_BIG;

        /* next */
        pszExpr++;
    }

    /*
     * Initialize the argument.
     */
    pArg->pDesc         = NULL;
    pArg->pNext         = NULL;
    pArg->enmType       = DBGCVAR_TYPE_NUMBER;
    pArg->u.u64Number   = u64;
    pArg->enmRangeType  = DBGCVAR_RANGE_NONE;
    pArg->u64Range      = 0;

    return VINF_SUCCESS;
}


/**
 * dbgcEvalSubUnary worker that handles simple numeric or pointer expressions.
 *
 * @returns VBox status code. pResult contains the result on success.
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   cchExpr     The length of the expression.
 * @param   enmCategory The desired type category (for range / no range).
 * @param   pResult     Where to store the result of the expression evaluation.
 */
static int dbgcEvalSubNumericOrPointer(PDBGC pDbgc, char *pszExpr, size_t cchExpr, DBGCVARCAT enmCategory,
                                       PDBGCVAR pResult)
{
    char const  ch  = pszExpr[0];
    char const  ch2 = pszExpr[1];

    /* 0x<hex digits> */
    if (ch == '0' && (ch2 == 'x' || ch2 == 'X'))
        return dbgcEvalSubNum(pszExpr + 2, 16, pResult);

    /* <hex digits>h */
    if (RT_C_IS_XDIGIT(*pszExpr) && (pszExpr[cchExpr - 1] == 'h' || pszExpr[cchExpr - 1] == 'H'))
    {
        pszExpr[cchExpr] = '\0';
        return dbgcEvalSubNum(pszExpr, 16, pResult);
    }

    /* 0i<decimal digits> */
    if (ch == '0' && ch2 == 'i')
        return dbgcEvalSubNum(pszExpr + 2, 10, pResult);

    /* 0t<octal digits> */
    if (ch == '0' && ch2 == 't')
        return dbgcEvalSubNum(pszExpr + 2, 8, pResult);

    /* 0y<binary digits> */
    if (ch == '0' && ch2 == 'y')
        return dbgcEvalSubNum(pszExpr + 2, 10, pResult);

    /* Hex number? */
    unsigned off = 0;
    while (RT_C_IS_XDIGIT(pszExpr[off]) || pszExpr[off] == '`')
        off++;
    if (off == cchExpr)
        return dbgcEvalSubNum(pszExpr, 16, pResult);

    /*
     * Some kind of symbol?
     */
    DBGCVARTYPE enmType;
    bool        fStripRange = false;
    switch (enmCategory)
    {
        case DBGCVAR_CAT_POINTER_NUMBER:            enmType = DBGCVAR_TYPE_NUMBER; break;
        case DBGCVAR_CAT_POINTER_NUMBER_NO_RANGE:   enmType = DBGCVAR_TYPE_NUMBER; fStripRange = true; break;
        case DBGCVAR_CAT_POINTER:                   enmType = DBGCVAR_TYPE_NUMBER; break;
        case DBGCVAR_CAT_POINTER_NO_RANGE:          enmType = DBGCVAR_TYPE_NUMBER; fStripRange = true; break;
        case DBGCVAR_CAT_GC_POINTER:                enmType = DBGCVAR_TYPE_GC_FLAT; break;
        case DBGCVAR_CAT_GC_POINTER_NO_RANGE:       enmType = DBGCVAR_TYPE_GC_FLAT; fStripRange = true; break;
        case DBGCVAR_CAT_NUMBER:                    enmType = DBGCVAR_TYPE_NUMBER; break;
        case DBGCVAR_CAT_NUMBER_NO_RANGE:           enmType = DBGCVAR_TYPE_NUMBER; fStripRange = true; break;
        default:
            AssertFailedReturn(VERR_DBGC_PARSE_NOT_IMPLEMENTED);
    }

    if (   (*pszExpr == '"' || *pszExpr == '\'')
        && pszExpr[cchExpr - 1] == *pszExpr)
    {
        pszExpr[cchExpr - 1] = '\0';
        pszExpr++;
    }

    int rc = dbgcSymbolGet(pDbgc, pszExpr, enmType, pResult);
    if (RT_SUCCESS(rc))
    {
        if (fStripRange)
        {
            pResult->enmRangeType = DBGCVAR_RANGE_NONE;
            pResult->u64Range     = 0;
        }
    }
    else if (rc == VERR_DBGC_PARSE_NOT_IMPLEMENTED)
        rc = VERR_DBGC_PARSE_INVALID_NUMBER;
    return rc;
}


/**
 * dbgcEvalSubUnary worker that handles simple DBGCVAR_CAT_ANY expressions.
 *
 * @returns VBox status code. pResult contains the result on success.
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   cchExpr     The length of the expression.
 * @param   pResult     Where to store the result of the expression evaluation.
 */
static int dbgcEvalSubUnaryAny(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pResult)
{
    char const  ch  = pszExpr[0];
    char const  ch2 = pszExpr[1];
    unsigned    off = 2;

    /* 0x<hex digits> */
    if (ch == '0' && (ch2 == 'x' || ch2 == 'X'))
    {
        while (RT_C_IS_XDIGIT(pszExpr[off]) || pszExpr[off] == '`')
            off++;
        if (off == cchExpr)
            return dbgcEvalSubNum(pszExpr + 2, 16, pResult);
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
    }

    /* <hex digits>h */
    if (RT_C_IS_XDIGIT(*pszExpr) && (pszExpr[cchExpr - 1] == 'h' || pszExpr[cchExpr - 1] == 'H'))
    {
        cchExpr--;
        while (off < cchExpr && (RT_C_IS_XDIGIT(pszExpr[off]) || pszExpr[off] == '`'))
            off++;
        if (off == cchExpr)
        {
            pszExpr[cchExpr] = '\0';
            return dbgcEvalSubNum(pszExpr, 16, pResult);
        }
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr + 1, pResult);
    }

    /* 0i<decimal digits> */
    if (ch == '0' && ch2 == 'i')
    {
        while (RT_C_IS_DIGIT(pszExpr[off]) || pszExpr[off] == '`')
            off++;
        if (off == cchExpr)
            return dbgcEvalSubNum(pszExpr + 2, 10, pResult);
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
    }

    /* 0t<octal digits> */
    if (ch == '0' && ch2 == 't')
    {
        while (RT_C_IS_ODIGIT(pszExpr[off]) || pszExpr[off] == '`')
            off++;
        if (off == cchExpr)
            return dbgcEvalSubNum(pszExpr + 2, 8, pResult);
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
    }

    /* 0y<binary digits> */
    if (ch == '0' && ch2 == 'y')
    {
        while (pszExpr[off] == '0' || pszExpr[off] == '1' || pszExpr[off] == '`')
            off++;
        if (off == cchExpr)
            return dbgcEvalSubNum(pszExpr + 2, 10, pResult);
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
    }

    /* Ok, no prefix of suffix. Is it a hex number after all? If not it must
       be a string. */
    off = 0;
    while (RT_C_IS_XDIGIT(pszExpr[off]) || pszExpr[off] == '`')
        off++;
    if (off == cchExpr)
        return dbgcEvalSubNum(pszExpr, 16, pResult);
    return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
}


/**
 * Evaluates one argument with respect to unary operators.
 *
 * @returns VBox status code. pResult contains the result on success.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   cchExpr     The length of the expression.
 * @param   enmCategory The target category for the result.
 * @param   pResult     Where to store the result of the expression evaluation.
 */
static int dbgcEvalSubUnary(PDBGC pDbgc, char *pszExpr, size_t cchExpr, DBGCVARCAT enmCategory, PDBGCVAR pResult)
{
    Log2(("dbgcEvalSubUnary: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));

    /*
     * The state of the expression is now such that it will start by zero or more
     * unary operators and being followed by an expression of some kind.
     * The expression is either plain or in parenthesis.
     *
     * Being in a lazy, recursive mode today, the parsing is done as simple as possible. :-)
     * ASSUME: unary operators are all of equal precedence.
     */
    int         rc  = VINF_SUCCESS;
    PCDBGCOP    pOp = dbgcOperatorLookup(pDbgc, pszExpr, false, ' ');
    if (pOp)
    {
        /* binary operators means syntax error. */
        if (pOp->fBinary)
            return VERR_DBGC_PARSE_UNEXPECTED_OPERATOR;

        /*
         * If the next expression (the one following the unary operator) is in a
         * parenthesis a full eval is needed. If not the unary eval will suffice.
         */
        /* calc and strip next expr. */
        char *pszExpr2 = pszExpr + pOp->cchName;
        while (RT_C_IS_BLANK(*pszExpr2))
            pszExpr2++;

        if (*pszExpr2)
        {
            DBGCVAR Arg;
            if (*pszExpr2 == '(')
                rc = dbgcEvalSub(pDbgc, pszExpr2, cchExpr - (pszExpr2 - pszExpr), pOp->enmCatArg1, &Arg);
            else
                rc = dbgcEvalSubUnary(pDbgc, pszExpr2, cchExpr - (pszExpr2 - pszExpr), pOp->enmCatArg1, &Arg);
            if (RT_SUCCESS(rc))
                rc = pOp->pfnHandlerUnary(pDbgc, &Arg, enmCategory, pResult);
        }
        else
            rc = VERR_DBGC_PARSE_EMPTY_ARGUMENT;
        return rc;
    }

    /*
     * Could this be a function call?
     *
     * ASSUMPTIONS:
     *    - A function name only contains alphanumerical chars and it can not
     *      start with a numerical character.
     *    - Immediately following the name is a parenthesis which must cover
     *      the remaining part of the expression.
     */
    bool    fExternal = *pszExpr == '.';
    char   *pszFun    = fExternal ? pszExpr + 1 : pszExpr;
    char   *pszFunEnd = NULL;
    if (pszExpr[cchExpr - 1] == ')' && RT_C_IS_ALPHA(*pszFun))
    {
        pszFunEnd = pszExpr + 1;
        while (*pszFunEnd != '(' && RT_C_IS_ALNUM(*pszFunEnd))
            pszFunEnd++;
        if (*pszFunEnd != '(')
            pszFunEnd = NULL;
    }
    if (pszFunEnd)
    {
        /*
         * Ok, it's a function call.
         */
        if (fExternal)
            pszExpr++, cchExpr--;
        PCDBGCCMD pFun = dbgcRoutineLookup(pDbgc, pszExpr, pszFunEnd - pszExpr, fExternal);
        if (!pFun)
            return VERR_DBGC_PARSE_FUNCTION_NOT_FOUND;
#if 0
        if (!pFun->pResultDesc)
            return VERR_DBGC_PARSE_NOT_A_FUNCTION;

        /*
         * Parse the expression in parenthesis.
         */
        cchExpr -= pszFunEnd - pszExpr;
        pszExpr = pszFunEnd;
        /** @todo implement multiple arguments. */
        DBGCVAR     Arg;
        rc = dbgcEvalSub(pDbgc, pszExpr, cchExpr, enmCategory, &Arg);
        if (!rc)
        {
            rc = dbgcEvalSubMatchVars(pDbgc, pFun->cArgsMin, pFun->cArgsMax, pFun->paArgDescs, pFun->cArgDescs, &Arg, 1);
            if (!rc)
                rc = pFun->pfnHandler(pFun, &pDbgc->CmdHlp, pDbgc->pVM, &Arg, 1, pResult);
        }
        else if (rc == VERR_DBGC_PARSE_EMPTY_ARGUMENT && pFun->cArgsMin == 0)
            rc = pFun->pfnHandler(pFun, &pDbgc->CmdHlp, pDbgc->pVM, NULL, 0, pResult);
#else
        rc = VERR_NOT_IMPLEMENTED;
#endif
        return rc;
    }

    /*
     * Assuming plain expression.
     * Didn't find any operators, so it must be a plain expression.
     * Go by desired category first, then if anythings go, try guess.
     */
    switch (enmCategory)
    {
        case DBGCVAR_CAT_ANY:
            return dbgcEvalSubUnaryAny(pDbgc, pszExpr, cchExpr, pResult);

        case DBGCVAR_CAT_POINTER_NUMBER:
        case DBGCVAR_CAT_POINTER_NUMBER_NO_RANGE:
        case DBGCVAR_CAT_POINTER:
        case DBGCVAR_CAT_POINTER_NO_RANGE:
        case DBGCVAR_CAT_GC_POINTER:
        case DBGCVAR_CAT_GC_POINTER_NO_RANGE:
        case DBGCVAR_CAT_NUMBER:
        case DBGCVAR_CAT_NUMBER_NO_RANGE:
            /* Pointers will be promoted later. */
            return dbgcEvalSubNumericOrPointer(pDbgc, pszExpr, cchExpr, enmCategory, pResult);

        case DBGCVAR_CAT_STRING:
        case DBGCVAR_CAT_SYMBOL:
            /* Symbols will be promoted later. */
            return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);

        case DBGCVAR_CAT_OPTION:
        case DBGCVAR_CAT_OPTION_STRING:
        case DBGCVAR_CAT_OPTION_NUMBER:
            return VERR_DBGC_PARSE_NOT_IMPLEMENTED;
    }

    AssertMsgFailed(("enmCategory=%d\n", enmCategory));
    rc = VERR_NOT_IMPLEMENTED;

    return rc;
}


/**
 * Evaluates one argument.
 *
 * @returns VBox status code.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   enmCategory The target category for the result.
 * @param   pResult     Where to store the result of the expression evaluation.
 */
int dbgcEvalSub(PDBGC pDbgc, char *pszExpr, size_t cchExpr, DBGCVARCAT enmCategory, PDBGCVAR pResult)
{
    Log2(("dbgcEvalSub: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));

    /*
     * First we need to remove blanks in both ends.
     * ASSUMES: There is no quoting unless the entire expression is a string.
     */

    /* stripping. */
    while (cchExpr > 0 && RT_C_IS_BLANK(pszExpr[cchExpr - 1]))
        pszExpr[--cchExpr] = '\0';
    while (RT_C_IS_BLANK(*pszExpr))
        pszExpr++, cchExpr--;
    if (!*pszExpr)
        return VERR_DBGC_PARSE_EMPTY_ARGUMENT;

    /*
     * Check if there are any parenthesis which needs removing.
     */
    if (pszExpr[0] == '(' && pszExpr[cchExpr - 1] == ')')
    {
        do
        {
            unsigned cPar = 1;
            char    *psz = pszExpr + 1;
            char     ch;
            while ((ch = *psz) != '\0')
            {
                if (ch == '(')
                    cPar++;
                else if (ch == ')')
                {
                    if (cPar <= 0)
                        return VERR_DBGC_PARSE_UNBALANCED_PARENTHESIS;
                    cPar--;
                    if (cPar == 0 && psz[1]) /* If not at end, there's nothing to do. */
                        break;
                }
                /* next */
                psz++;
            }
            if (ch)
                break;

            /* remove the parenthesis. */
            pszExpr++;
            cchExpr -= 2;
            pszExpr[cchExpr] = '\0';

            /* strip blanks. */
            while (cchExpr > 0 && RT_C_IS_BLANK(pszExpr[cchExpr - 1]))
                pszExpr[--cchExpr] = '\0';
            while (RT_C_IS_BLANK(*pszExpr))
                pszExpr++, cchExpr--;
            if (!*pszExpr)
                return VERR_DBGC_PARSE_EMPTY_ARGUMENT;
        } while (pszExpr[0] == '(' && pszExpr[cchExpr - 1] == ')');
    }

#if 0  /* why was this needed? It'll screw up strings. */
    /* tabs to spaces */
    char *psz = pszExpr;
    while ((psz = strchr(psz, '\t')) != NULL)
        *psz = ' ';
#endif

    /*
     * Now, we need to look for the binary operator with the lowest precedence.
     *
     * If there are no operators we're left with a simple expression which we
     * evaluate with respect to unary operators
     */
    char       *pszOpSplit  = NULL;
    PCDBGCOP    pOpSplit    = NULL;
    unsigned    cBinaryOps  = 0;
    unsigned    cPar        = 0;
    char        chQuote     = '\0';
    char        chPrev      = ' ';
    bool        fBinary     = false;
    char       *psz         = pszExpr;
    char        ch;

    while ((ch = *psz) != '\0')
    {
        //Log2(("ch=%c cPar=%d fBinary=%d\n", ch, cPar, fBinary));
        /*
         * String quoting.
         */
        if (chQuote)
        {
            if (   ch == chQuote
                && psz[1] != chQuote)
            {
                chQuote = '\0';
                fBinary = true;
            }
        }
        else if (ch == '"' || ch == '\'')
            chQuote = ch;
        /*
         * Parentheses.
         */
        else if (ch == '(')
        {
            cPar++;
            fBinary = false;
        }
        else if (ch == ')')
        {
            if (cPar <= 0)
                return VERR_DBGC_PARSE_UNBALANCED_PARENTHESIS;
            cPar--;
            fBinary = true;
        }
        /*
         * Potential operator.
         */
        else if (cPar == 0 && !RT_C_IS_BLANK(ch))
        {
            PCDBGCOP pOp = dbgcIsOpChar(ch)
                         ? dbgcOperatorLookup(pDbgc, psz, fBinary, chPrev)
                         : NULL;
            if (pOp)
            {
                /* If not the right kind of operator we've got a syntax error. */
                if (pOp->fBinary != fBinary)
                    return VERR_DBGC_PARSE_UNEXPECTED_OPERATOR;

                /*
                 * Update the parse state and skip the operator.
                 */
                if (!pOpSplit)
                {
                    pOpSplit = pOp;
                    pszOpSplit = psz;
                    cBinaryOps = fBinary;
                }
                else if (fBinary)
                {
                    cBinaryOps++;
                    if (pOp->iPrecedence >= pOpSplit->iPrecedence)
                    {
                        pOpSplit = pOp;
                        pszOpSplit = psz;
                    }
                }

                psz += pOp->cchName - 1;
                fBinary = false;
            }
            else
                fBinary = true;
        }

        /* next */
        psz++;
        chPrev = ch;
    } /* parse loop. */

    if (chQuote)
        return VERR_DBGC_PARSE_UNBALANCED_QUOTE;

    /*
     * Either we found an operator to divide the expression by
     * or we didn't find any. In the first case it's divide and
     * conquer. In the latter it's a single expression which
     * needs dealing with its unary operators if any.
     */
    int rc;
    if (    cBinaryOps
        &&  pOpSplit->fBinary)
    {
        /* process 1st sub expression. */
        *pszOpSplit = '\0';
        DBGCVAR     Arg1;
        rc = dbgcEvalSub(pDbgc, pszExpr, pszOpSplit - pszExpr, pOpSplit->enmCatArg1, &Arg1);
        if (RT_SUCCESS(rc))
        {
            /* process 2nd sub expression. */
            char       *psz2 = pszOpSplit + pOpSplit->cchName;
            DBGCVAR     Arg2;
            rc = dbgcEvalSub(pDbgc, psz2, cchExpr - (psz2 - pszExpr), pOpSplit->enmCatArg2, &Arg2);
            if (RT_SUCCESS(rc))
                /* apply the operator. */
                rc = pOpSplit->pfnHandlerBinary(pDbgc, &Arg1, &Arg2, pResult);
        }
    }
    else if (cBinaryOps)
    {
        /* process sub expression. */
        pszOpSplit += pOpSplit->cchName;
        DBGCVAR     Arg;
        rc = dbgcEvalSub(pDbgc, pszOpSplit, cchExpr - (pszOpSplit - pszExpr), pOpSplit->enmCatArg1, &Arg);
        if (RT_SUCCESS(rc))
            /* apply the operator. */
            rc = pOpSplit->pfnHandlerUnary(pDbgc, &Arg, enmCategory, pResult);
    }
    else
        /* plain expression, qutoed string, or using unary operators perhaps with parentheses. */
        rc = dbgcEvalSubUnary(pDbgc, pszExpr, cchExpr, enmCategory, pResult);

    return rc;
}


/**
 * Worker for dbgcProcessArguments that performs type checking and promoptions.
 *
 * @returns VBox status code.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   enmCategory The target category for the result.
 * @param   pArg        The argument to check and promote.
 */
static int dbgcCheckAndTypePromoteArgument(PDBGC pDbgc, DBGCVARCAT enmCategory, PDBGCVAR pArg)
{
    switch (enmCategory)
    {
        /*
         * Anything goes
         */
        case DBGCVAR_CAT_ANY:
            return VINF_SUCCESS;

        /*
         * Pointer with and without range.
         * We can try resolve strings and symbols as symbols and promote
         * numbers to flat GC pointers.
         */
        case DBGCVAR_CAT_POINTER_NO_RANGE:
        case DBGCVAR_CAT_POINTER_NUMBER_NO_RANGE:
            if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_DBGC_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_POINTER:
        case DBGCVAR_CAT_POINTER_NUMBER:
            switch (pArg->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                case DBGCVAR_TYPE_GC_FAR:
                case DBGCVAR_TYPE_GC_PHYS:
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_FLAT, &Var);
                    if (RT_SUCCESS(rc))
                    {
                        /* deal with range */
                        if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                        {
                            Var.enmRangeType = pArg->enmRangeType;
                            Var.u64Range = pArg->u64Range;
                        }
                        else if (enmCategory == DBGCVAR_CAT_POINTER_NO_RANGE)
                            Var.enmRangeType = DBGCVAR_RANGE_NONE;
                        *pArg = Var;
                    }
                    return rc;
                }

                case DBGCVAR_TYPE_NUMBER:
                    if (   enmCategory != DBGCVAR_CAT_POINTER_NUMBER
                        && enmCategory != DBGCVAR_CAT_POINTER_NUMBER_NO_RANGE)
                    {
                        RTGCPTR GCPtr = (RTGCPTR)pArg->u.u64Number;
                        pArg->enmType = DBGCVAR_TYPE_GC_FLAT;
                        pArg->u.GCFlat = GCPtr;
                    }
                    return VINF_SUCCESS;

                default:
                    AssertMsgFailedReturn(("Invalid type %d\n"), VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
            }
            break;                      /* (not reached) */

        /*
         * GC pointer with and without range.
         * We can try resolve strings and symbols as symbols and
         * promote numbers to flat GC pointers.
         */
        case DBGCVAR_CAT_GC_POINTER_NO_RANGE:
            if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_DBGC_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_GC_POINTER:
            switch (pArg->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                case DBGCVAR_TYPE_GC_FAR:
                case DBGCVAR_TYPE_GC_PHYS:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_DBGC_PARSE_CONVERSION_FAILED;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_FLAT, &Var);
                    if (RT_SUCCESS(rc))
                    {
                        /* deal with range */
                        if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                        {
                            Var.enmRangeType = pArg->enmRangeType;
                            Var.u64Range = pArg->u64Range;
                        }
                        else if (enmCategory == DBGCVAR_CAT_POINTER_NO_RANGE)
                            Var.enmRangeType = DBGCVAR_RANGE_NONE;
                        *pArg = Var;
                    }
                    return rc;
                }

                case DBGCVAR_TYPE_NUMBER:
                {
                    RTGCPTR GCPtr = (RTGCPTR)pArg->u.u64Number;
                    pArg->enmType = DBGCVAR_TYPE_GC_FLAT;
                    pArg->u.GCFlat = GCPtr;
                    return VINF_SUCCESS;
                }

                default:
                    AssertMsgFailedReturn(("Invalid type %d\n"), VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
            }
            break;                      /* (not reached) */

        /*
         * Number with or without a range.
         * Numbers can be resolved from symbols, but we cannot demote a pointer
         * to a number.
         */
        case DBGCVAR_CAT_NUMBER_NO_RANGE:
            if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_DBGC_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_NUMBER:
            switch (pArg->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                case DBGCVAR_TYPE_GC_FAR:
                case DBGCVAR_TYPE_GC_PHYS:
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;

                case DBGCVAR_TYPE_NUMBER:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (RT_SUCCESS(rc))
                    {
                        /* deal with range */
                        if (pArg->enmRangeType != DBGCVAR_RANGE_NONE)
                        {
                            Var.enmRangeType = pArg->enmRangeType;
                            Var.u64Range = pArg->u64Range;
                        }
                        else if (enmCategory == DBGCVAR_CAT_POINTER_NO_RANGE)
                            Var.enmRangeType = DBGCVAR_RANGE_NONE;
                        *pArg = Var;
                    }
                    return rc;
                }

                default:
                    AssertMsgFailedReturn(("Invalid type %d\n"), VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
            }
            break;                      /* (not reached) */

        /*
         * Symbols and strings are basically the same thing for the time being.
         */
        case DBGCVAR_CAT_STRING:
        case DBGCVAR_CAT_SYMBOL:
        {
            switch (pArg->enmType)
            {
                case DBGCVAR_TYPE_STRING:
                    if (enmCategory == DBGCVAR_CAT_SYMBOL)
                        pArg->enmType = DBGCVAR_TYPE_SYMBOL;
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_SYMBOL:
                    if (enmCategory == DBGCVAR_CAT_STRING)
                        pArg->enmType = DBGCVAR_TYPE_STRING;
                    return VINF_SUCCESS;
                default:
                    break;
            }

            /* Stringify numeric and poitner values. */
            size_t cbScratch = sizeof(pDbgc->achScratch) - (pDbgc->pszScratch - &pDbgc->achScratch[0]);
            size_t cch = pDbgc->CmdHlp.pfnStrPrintf(&pDbgc->CmdHlp, pDbgc->pszScratch, cbScratch, "%Dv", pArg);
            if (cch + 1 >= cbScratch)
                return VERR_DBGC_PARSE_NO_SCRATCH;

            pArg->enmType      = enmCategory == DBGCVAR_CAT_STRING ? DBGCVAR_TYPE_STRING : DBGCVAR_TYPE_SYMBOL;
            pArg->u.pszString  = pDbgc->pszScratch;
            pArg->enmRangeType = DBGCVAR_RANGE_BYTES;
            pArg->u64Range     = cch;

            pDbgc->pszScratch += cch + 1;
            return VINF_SUCCESS;
        }

        /*
         * These are not yet implemented.
         */
        case DBGCVAR_CAT_OPTION:
        case DBGCVAR_CAT_OPTION_STRING:
        case DBGCVAR_CAT_OPTION_NUMBER:
            AssertMsgFailedReturn(("Not implemented enmCategory=%d\n", enmCategory), VERR_DBGC_PARSE_NOT_IMPLEMENTED);

        default:
            AssertMsgFailedReturn(("Bad enmCategory=%d\n", enmCategory), VERR_DBGC_PARSE_NOT_IMPLEMENTED);
    }
}


/**
 * Parses the arguments of one command.
 *
 * @returns VBox statuc code. On parser errors the index of the troublesome
 *          argument is indicated by *pcArg.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pCmd        Pointer to the command descriptor.
 * @param   pszArg      Pointer to the arguments to parse.
 * @param   paArgs      Where to store the parsed arguments.
 * @param   cArgs       Size of the paArgs array.
 * @param   pcArgs      Where to store the number of arguments.  In the event
 *                      of an error this is (ab)used to store the index of the
 *                      offending argument.
 */
static int dbgcProcessArguments(PDBGC pDbgc, PCDBGCCMD pCmd, char *pszArgs, PDBGCVAR paArgs, unsigned cArgs, unsigned *pcArgs)
{
    Log2(("dbgcProcessArguments: pCmd=%s pszArgs='%s'\n", pCmd->pszCmd, pszArgs));

    /*
     * Check if we have any argument and if the command takes any.
     */
    *pcArgs = 0;
    /* strip leading blanks. */
    while (*pszArgs && RT_C_IS_BLANK(*pszArgs))
        pszArgs++;
    if (!*pszArgs)
    {
        if (!pCmd->cArgsMin)
            return VINF_SUCCESS;
        return VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS;
    }
    /** @todo fixme - foo() doesn't work. */
    if (!pCmd->cArgsMax)
        return VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS;

    /*
     * The parse loop.
     */
    PDBGCVAR        pArg0       = &paArgs[0];
    PDBGCVAR        pArg        = pArg0;
    PCDBGCVARDESC   pPrevDesc   = NULL;
    PCDBGCVARDESC   paVarDescs  = pCmd->paArgDescs;
    unsigned const  cVarDescs   = pCmd->cArgDescs;
    unsigned        cCurDesc    = 0;
    unsigned        iVar        = 0;
    unsigned        iVarDesc    = 0;
    *pcArgs = 0;
    do
    {
        /*
         * Can we have another argument?
         */
        if (*pcArgs >= pCmd->cArgsMax)
            return VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS;
        if (pArg >= &paArgs[cArgs])
            return VERR_DBGC_PARSE_ARGUMENT_OVERFLOW;
        if (iVarDesc >= cVarDescs)
            return VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS;

        /* Walk argument descriptors. */
        if (cCurDesc >= paVarDescs[iVarDesc].cTimesMax)
        {
            iVarDesc++;
            if (iVarDesc >= cVarDescs)
                return VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS;
            cCurDesc = 0;
        }

        /*
         * Find the end of the argument.
         */
        int     cPar    = 0;
        char    chQuote = '\0';
        char   *pszEnd  = NULL;
        char   *psz     = pszArgs;
        char    ch;
        bool    fBinary = false;
        for (;;)
        {
            /*
             * Check for the end.
             */
            if ((ch = *psz) == '\0')
            {
                if (chQuote)
                    return VERR_DBGC_PARSE_UNBALANCED_QUOTE;
                if (cPar)
                    return VERR_DBGC_PARSE_UNBALANCED_PARENTHESIS;
                pszEnd = psz;
                break;
            }
            /*
             * When quoted we ignore everything but the quotation char.
             * We use the REXX way of escaping the quotation char, i.e. double occurrence.
             */
            else if (ch == '\'' || ch == '"')
            {
                if (chQuote)
                {
                    /* end quote? */
                    if (ch == chQuote)
                    {
                        if (psz[1] == ch)
                            psz++;          /* skip the escaped quote char */
                        else
                            chQuote = '\0'; /* end of quoted string. */
                    }
                }
                else
                    chQuote = ch;           /* open new quote */
            }
            /*
             * Parenthesis can of course be nested.
             */
            else if (!chQuote && ch == '(')
            {
                cPar++;
                fBinary = false;
            }
            else if (!chQuote && ch == ')')
            {
                if (!cPar)
                    return VERR_DBGC_PARSE_UNBALANCED_PARENTHESIS;
                cPar--;
                fBinary = true;
            }
            else if (!chQuote && !cPar)
            {
                /*
                 * Encountering blanks may mean the end of it all. A binary operator
                 * will force continued parsing.
                 */
                if (RT_C_IS_BLANK(*psz))
                {
                    pszEnd = psz++;         /* just in case. */
                    while (RT_C_IS_BLANK(*psz))
                        psz++;
                    PCDBGCOP pOp = dbgcOperatorLookup(pDbgc, psz, fBinary, ' ');
                    if (!pOp || pOp->fBinary != fBinary)
                        break;              /* the end. */
                    psz += pOp->cchName;
                    while (RT_C_IS_BLANK(*psz))   /* skip blanks so we don't get here again */
                        psz++;
                    fBinary = false;
                    continue;
                }

                /*
                 * Look for operators without a space up front.
                 */
                if (dbgcIsOpChar(*psz))
                {
                    PCDBGCOP pOp = dbgcOperatorLookup(pDbgc, psz, fBinary, ' ');
                    if (pOp)
                    {
                        if (pOp->fBinary != fBinary)
                        {
                            pszEnd = psz;
                            /** @todo this is a parsing error really. */
                            break;              /* the end. */
                        }
                        psz += pOp->cchName;
                        while (RT_C_IS_BLANK(*psz))   /* skip blanks so we don't get here again */
                            psz++;
                        fBinary = false;
                        continue;
                    }
                }
                fBinary = true;
            }

            /* next char */
            psz++;
        }
        *pszEnd = '\0';
        /* (psz = next char to process) */
        size_t cchArgs = strlen(pszArgs);

        /*
         * Try optional arguments until we find something which matches
         * or can easily be promoted to what the descriptor want.
         */
        for (;;)
        {
            char *pszArgsCopy = (char *)RTMemDup(pszArgs, cchArgs + 1);
            if (!pszArgsCopy)
                return VERR_DBGC_PARSE_NO_MEMORY;

            int rc = dbgcEvalSub(pDbgc, pszArgs, cchArgs, paVarDescs[iVarDesc].enmCategory, pArg);
            if (RT_SUCCESS(rc))
                rc = dbgcCheckAndTypePromoteArgument(pDbgc, paVarDescs[iVarDesc].enmCategory, pArg);
            if (RT_SUCCESS(rc))
            {
                pArg->pDesc = pPrevDesc = &paVarDescs[iVarDesc];
                cCurDesc++;
                RTMemFree(pszArgsCopy);
                break;
            }

            memcpy(pszArgs, pszArgsCopy, cchArgs + 1);
            RTMemFree(pszArgsCopy);

            /* Continue searching optional descriptors? */
            if (   rc != VERR_DBGC_PARSE_INCORRECT_ARG_TYPE
                && rc != VERR_DBGC_PARSE_INVALID_NUMBER
                && rc != VERR_DBGC_PARSE_NO_RANGE_ALLOWED
               )
                   return rc;

            /* Try advance to the next descriptor. */
            if (paVarDescs[iVarDesc].cTimesMin > cCurDesc)
                return rc;
            iVarDesc++;
            if (!cCurDesc)
                while (   iVarDesc < cVarDescs
                       && (paVarDescs[iVarDesc].fFlags & DBGCVD_FLAGS_DEP_PREV))
                    iVarDesc++;
            if (iVarDesc >= cVarDescs)
                return rc;
            cCurDesc = 0;
        }

        /*
         * Next argument.
         */
        iVar++;
        pArg++;
        (*pcArgs)++;
        pszArgs = psz;
        while (*pszArgs && RT_C_IS_BLANK(*pszArgs))
            pszArgs++;
    } while (*pszArgs);

    /*
     * Check that the rest of the argument descriptors indicate optional args.
     */
    if (iVarDesc < cVarDescs)
    {
        if (cCurDesc < paVarDescs[iVarDesc].cTimesMin)
            return VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS;
        iVarDesc++;
        while (iVarDesc < cVarDescs)
        {
            if (paVarDescs[iVarDesc].cTimesMin)
                return VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS;
            iVarDesc++;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Evaluate one command.
 *
 * @returns VBox status code. This is also stored in DBGC::rcCmd.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pszCmd      Pointer to the command.
 * @param   cchCmd      Length of the command.
 * @param   fNoExecute  Indicates that no commands should actually be executed.
 *
 * @todo    Change pszCmd into argc/argv?
 */
int dbgcEvalCommand(PDBGC pDbgc, char *pszCmd, size_t cchCmd, bool fNoExecute)
{
    char *pszCmdInput = pszCmd;

    /*
     * Skip blanks.
     */
    while (RT_C_IS_BLANK(*pszCmd))
        pszCmd++, cchCmd--;

    /* external command? */
    bool const fExternal = *pszCmd == '.';
    if (fExternal)
        pszCmd++, cchCmd--;

    /*
     * Find arguments.
     */
    char *pszArgs = pszCmd;
    while (RT_C_IS_ALNUM(*pszArgs))
        pszArgs++;
    if (*pszArgs && (!RT_C_IS_BLANK(*pszArgs) || pszArgs == pszCmd))
    {
        DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "Syntax error: Invalid command '%s'!\n", pszCmdInput);
        return pDbgc->rcCmd = VERR_DBGC_PARSE_INVALD_COMMAND_NAME;
    }

    /*
     * Find the command.
     */
    PCDBGCCMD pCmd = dbgcRoutineLookup(pDbgc, pszCmd, pszArgs - pszCmd, fExternal);
    if (!pCmd)
    {
        DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "Syntax error: Unknown command '%s'!\n", pszCmdInput);
        return pDbgc->rcCmd = VERR_DBGC_PARSE_COMMAND_NOT_FOUND;
    }

    /*
     * Parse arguments (if any).
     */
    unsigned cArgs;
    int rc = dbgcProcessArguments(pDbgc, pCmd, pszArgs, &pDbgc->aArgs[pDbgc->iArg],
                                  RT_ELEMENTS(pDbgc->aArgs) - pDbgc->iArg, &cArgs);
    if (RT_SUCCESS(rc))
    {
        AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n",  rc));

        /*
         * Execute the command.
         */
        if (!fNoExecute)
            rc = pCmd->pfnHandler(pCmd, &pDbgc->CmdHlp, pDbgc->pVM, &pDbgc->aArgs[0], cArgs);
        pDbgc->rcCmd = rc;
        if (rc == VERR_DBGC_COMMAND_FAILED)
            rc = VINF_SUCCESS;
    }
    else
    {
        pDbgc->rcCmd = rc;

        /* report parse / eval error. */
        switch (rc)
        {
            case VERR_DBGC_PARSE_TOO_FEW_ARGUMENTS:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Too few arguments. Minimum is %d for command '%s'.\n", pCmd->cArgsMin, pCmd->pszCmd);
                break;
            case VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Too many arguments. Maximum is %d for command '%s'.\n", pCmd->cArgsMax, pCmd->pszCmd);
                break;
            case VERR_DBGC_PARSE_ARGUMENT_OVERFLOW:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Too many arguments.\n");
                break;
            case VERR_DBGC_PARSE_UNBALANCED_QUOTE:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Unbalanced quote (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_UNBALANCED_PARENTHESIS:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Unbalanced parenthesis (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_EMPTY_ARGUMENT:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: An argument or subargument contains nothing useful (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_UNEXPECTED_OPERATOR:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Invalid operator usage (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_INVALID_NUMBER:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Syntax error: Invalid numeric value (argument %d). If a string was the intention, then quote it.\n", cArgs);
                break;
            case VERR_DBGC_PARSE_NUMBER_TOO_BIG:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Numeric overflow (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_INVALID_OPERATION:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Invalid operation attempted (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_FUNCTION_NOT_FOUND:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Function not found (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_NOT_A_FUNCTION:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: The function specified is not a function (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_NO_MEMORY:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Out memory in the regular heap! Expect odd stuff to happen...\n", cArgs);
                break;
            case VERR_DBGC_PARSE_INCORRECT_ARG_TYPE:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Incorrect argument type (argument %d?).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_VARIABLE_NOT_FOUND:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: An undefined variable was referenced (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_CONVERSION_FAILED:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: A conversion between two types failed (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_NOT_IMPLEMENTED:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: You hit a debugger feature which isn't implemented yet (argument %d).\n", cArgs);
                break;
            case VERR_DBGC_PARSE_BAD_RESULT_TYPE:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Couldn't satisfy a request for a specific result type (argument %d). (Usually applies to symbols)\n", cArgs);
                break;
            case VERR_DBGC_PARSE_WRITEONLY_SYMBOL:
                rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp,
                    "Error: Cannot get symbol, it's set only (argument %d).\n", cArgs);
                break;

            case VERR_DBGC_COMMAND_FAILED:
                break;

            default:
            {
                PCRTSTATUSMSG pErr = RTErrGet(rc);
                if (strncmp(pErr->pszDefine, RT_STR_TUPLE("Unknown Status")))
                    rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "Error: %s (%d) - %s\n", pErr->pszDefine, rc, pErr->pszMsgFull);
                else
                    rc = DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "Error: Unknown error %d (%#x)!\n", rc, rc);
                break;
            }
        }
    }
    return rc;
}

