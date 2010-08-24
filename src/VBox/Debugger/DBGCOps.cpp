/* $Id$ */
/** @file
 * DBGC - Debugger Console, Operators.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <VBox/dbgf.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/dis.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>

#include <stdlib.h>
#include <stdio.h>

#include "DBGCInternal.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgcOpMinus(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpPluss(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpVar(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);

static DECLCALLBACK(int) dbgcOpAddrFar(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpMult(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpDiv(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpMod(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAdd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpSub(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseShiftLeft(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseShiftRight(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseXor(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeLength(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeLengthBytes(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeTo(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/**
 * Generic implementation of a binary operator.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc           Debugger console instance data.
 * @param   pArg1           The first argument.
 * @param   pArg2           The 2nd argument.
 * @param   pResult         Where to store the result.
 * @param   Operator        The C operator.
 * @param   fIsDiv          Set if it's division and we need to check for zero on the
 *                          right hand side.
 */
#define DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, Operator, fIsDiv) \
    do \
    { \
        /* Get the 64-bit right side value. */ \
        uint64_t u64Right; \
        int rc = dbgcOpHelperGetNumber((pDbgc), (pArg2), &u64Right); \
        if ((fIsDiv) && RT_SUCCESS(rc) && !u64Right) /* div/0 kludge */ \
            DBGCVAR_INIT_NUMBER((pResult), UINT64_MAX); \
        else if (RT_SUCCESS(rc)) \
        { \
            /* Apply it to the left hand side. */ \
            if (   (pArg1)->enmType == DBGCVAR_TYPE_SYMBOL \
                || (pArg1)->enmType == DBGCVAR_TYPE_STRING) \
            { \
                rc = dbgcSymbolGet((pDbgc), (pArg1)->u.pszString, DBGCVAR_TYPE_ANY, (pResult)); \
                if (RT_FAILURE(rc)) \
                    return rc; \
            } \
            else \
                *(pResult) = *(pArg1); \
            switch ((pResult)->enmType) \
            { \
                case DBGCVAR_TYPE_GC_FLAT: \
                    (pResult)->u.GCFlat     = (pResult)->u.GCFlat     Operator  u64Right; \
                    break; \
                case DBGCVAR_TYPE_GC_FAR: \
                    (pResult)->u.GCFar.off  = (pResult)->u.GCFar.off  Operator  u64Right; \
                    break; \
                case DBGCVAR_TYPE_GC_PHYS: \
                    (pResult)->u.GCPhys     = (pResult)->u.GCPhys     Operator  u64Right; \
                    break; \
                case DBGCVAR_TYPE_HC_FLAT: \
                    (pResult)->u.pvHCFlat   = (void *)((uintptr_t)(pResult)->u.pvHCFlat  Operator  u64Right); \
                    break; \
                case DBGCVAR_TYPE_HC_FAR: \
                    (pResult)->u.HCFar.off  = (pResult)->u.HCFar.off  Operator  u64Right; \
                    break; \
                case DBGCVAR_TYPE_HC_PHYS: \
                    (pResult)->u.HCPhys     = (pResult)->u.HCPhys     Operator  u64Right; \
                    break; \
                case DBGCVAR_TYPE_NUMBER: \
                    (pResult)->u.u64Number  = (pResult)->u.u64Number  Operator  u64Right; \
                    break; \
                default: \
                    return VERR_PARSE_INCORRECT_ARG_TYPE; \
            } \
        } \
        return rc; \
    } while (0)


/**
 * Switch the factors/whatver so we preserve pointers.
 * Far pointers are considered more  important that physical and flat pointers.
 *
 * @param   pArg1           The left side argument.  Input & output.
 * @param   pArg2           The right side argument.  Input & output.
 */
#define DBGC_GEN_ARIT_POINTER_TO_THE_LEFT(pArg1, pArg2) \
    do \
    { \
        if (    DBGCVAR_ISPOINTER((pArg2)->enmType) \
            &&  (   !DBGCVAR_ISPOINTER((pArg1)->enmType) \
                 || (   DBGCVAR_IS_FAR_PTR((pArg2)->enmType) \
                     && !DBGCVAR_IS_FAR_PTR((pArg1)->enmType)))) \
        { \
            PCDBGCVAR pTmp = (pArg1); \
            (pArg2) = (pArg1); \
            (pArg1) = pTmp; \
        } \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Operators. */
const DBGCOP g_aOps[] =
{
    /* szName is initialized as a 4 char array because of M$C elsewise optimizing it away in /Ox mode (the 'const char' vs 'char' problem). */
    /* szName,          cchName, fBinary,    iPrecedence,    pfnHandlerUnary,    pfnHandlerBitwise  */
    { {'-'},            1,       false,      1,              dbgcOpMinus,        NULL,                       "Unary minus." },
    { {'+'},            1,       false,      1,              dbgcOpPluss,        NULL,                       "Unary plus." },
    { {'!'},            1,       false,      1,              dbgcOpBooleanNot,   NULL,                       "Boolean not." },
    { {'~'},            1,       false,      1,              dbgcOpBitwiseNot,   NULL,                       "Bitwise complement." },
    { {':'},            1,       true,       2,              NULL,               dbgcOpAddrFar,              "Far pointer." },
    { {'%'},            1,       false,      3,              dbgcOpAddrFlat,     NULL,                       "Flat address." },
    { {'%','%'},        2,       false,      3,              dbgcOpAddrPhys,     NULL,                       "Physical address." },
    { {'#'},            1,       false,      3,              dbgcOpAddrHost,     NULL,                       "Flat host address." },
    { {'#','%','%'},    3,       false,      3,              dbgcOpAddrHostPhys, NULL,                       "Physical host address." },
    { {'$'},            1,       false,      3,              dbgcOpVar,          NULL,                       "Reference a variable." },
    { {'*'},            1,       true,       10,             NULL,               dbgcOpMult,                 "Multiplication." },
    { {'/'},            1,       true,       11,             NULL,               dbgcOpDiv,                  "Division." },
    { {'%'},            1,       true,       12,             NULL,               dbgcOpMod,                  "Modulus." },
    { {'+'},            1,       true,       13,             NULL,               dbgcOpAdd,                  "Addition." },
    { {'-'},            1,       true,       14,             NULL,               dbgcOpSub,                  "Subtraction." },
    { {'<','<'},        2,       true,       15,             NULL,               dbgcOpBitwiseShiftLeft,     "Bitwise left shift." },
    { {'>','>'},        2,       true,       16,             NULL,               dbgcOpBitwiseShiftRight,    "Bitwise right shift." },
    { {'&'},            1,       true,       17,             NULL,               dbgcOpBitwiseAnd,           "Bitwise and." },
    { {'^'},            1,       true,       18,             NULL,               dbgcOpBitwiseXor,           "Bitwise exclusiv or." },
    { {'|'},            1,       true,       19,             NULL,               dbgcOpBitwiseOr,            "Bitwise inclusive or." },
    { {'&','&'},        2,       true,       20,             NULL,               dbgcOpBooleanAnd,           "Boolean and." },
    { {'|','|'},        2,       true,       21,             NULL,               dbgcOpBooleanOr,            "Boolean or." },
    { {'L'},            1,       true,       22,             NULL,               dbgcOpRangeLength,          "Range elements." },
    { {'L','B'},        2,       true,       23,             NULL,               dbgcOpRangeLengthBytes,     "Range bytes." },
    { {'T'},            1,       true,       24,             NULL,               dbgcOpRangeTo,              "Range to." }
};

/** Number of operators in the operator array. */
const unsigned g_cOps = RT_ELEMENTS(g_aOps);


/**
 * Convers an argument to a number value.
 *
 * @returns VBox status code.
 * @param   pDbgc               The DBGC instance.
 * @param   pArg                The argument to convert.
 * @param   pu64Ret             Where to return the value.
 */
static int dbgcOpHelperGetNumber(PDBGC pDbgc, PCDBGCVAR pArg, uint64_t *pu64Ret)
{
    DBGCVAR Var = *pArg;
    switch (Var.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            *pu64Ret = Var.u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            *pu64Ret = Var.u.GCFar.off;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            *pu64Ret = Var.u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            *pu64Ret = (uintptr_t)Var.u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            *pu64Ret = Var.u.HCFar.off;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            *pu64Ret = Var.u.HCPhys;
            break;
        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_SYMBOL:
        {
            int rc = dbgcSymbolGet(pDbgc, Var.u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
            if (RT_FAILURE(rc))
                return rc;
            /* fall thru */
        }
        case DBGCVAR_TYPE_NUMBER:
            *pu64Ret = Var.u.u64Number;
            break;
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return VINF_SUCCESS;
}


/**
 * Minus (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMinus(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpMinus\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFlat       = -(RTGCINTPTR)pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.GCFar.off    = -(int32_t)pResult->u.GCFar.off;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.GCPhys       = (RTGCPHYS) -(int64_t)pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat     = (void *) -(intptr_t)pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.HCFar.off    = -(int32_t)pResult->u.HCFar.off;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.HCPhys       = (RTHCPHYS) -(int64_t)pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = -(int64_t)pResult->u.u64Number;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return VINF_SUCCESS;
}


/**
 * Pluss (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpPluss(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpPluss\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_NUMBER:
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return VINF_SUCCESS;
}


/**
 * Boolean not (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBooleanNot\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.u64Number    = !pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.u64Number    = !pResult->u.GCFar.off && pResult->u.GCFar.sel <= 3;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.u64Number    = !pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.u64Number    = !pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.u64Number    = !pResult->u.HCFar.off && pResult->u.HCFar.sel <= 3;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.u64Number    = !pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = !pResult->u.u64Number;
            break;
        case DBGCVAR_TYPE_STRING:
            pResult->u.u64Number    = !pResult->u64Range;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    pResult->enmType = DBGCVAR_TYPE_NUMBER;
    NOREF(pDbgc);
    return VINF_SUCCESS;
}


/**
 * Bitwise not (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseNot\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFlat       = ~pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.GCFar.off    = ~pResult->u.GCFar.off;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.GCPhys       = ~pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat     = (void *)~(uintptr_t)pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.HCFar.off= ~pResult->u.HCFar.off;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.HCPhys       = ~pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = ~pResult->u.u64Number;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return VINF_SUCCESS;
}


/**
 * Reference variable (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpVar(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpVar: %s\n", pArg->u.pszString));

    /*
     * Parse sanity.
     */
    if (pArg->enmType != DBGCVAR_TYPE_STRING)
        return VERR_PARSE_INCORRECT_ARG_TYPE;

    /*
     * Lookup the variable.
     */
    const char *pszVar = pArg->u.pszString;
    for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
    {
        if (!strcmp(pszVar, pDbgc->papVars[iVar]->szName))
        {
            *pResult = pDbgc->papVars[iVar]->Var;
            return VINF_SUCCESS;
        }
    }

    return VERR_PARSE_VARIABLE_NOT_FOUND;
}


/**
 * Flat address (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
DECLCALLBACK(int) dbgcOpAddrFlat(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAddrFlat\n"));
    int     rc;
    *pResult = *pArg;

    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, pDbgc->idCpu, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (RT_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_GC_FLAT;
                pResult->u.GCFlat = Address.FlatPtr;
                return VINF_SUCCESS;
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            //rc = MMR3PhysGCPhys2GCVirtEx(pDbgc->pVM, pResult->u.GCPhys, ..., &pResult->u.GCFlat); - yea, sure.
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_FLAT:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType        = DBGCVAR_TYPE_HC_FLAT;
            rc = MMR3HCPhys2HCVirt(pDbgc->pVM, pResult->u.HCPhys, &pResult->u.pvHCFlat);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_GC_FLAT;
            pResult->u.GCFlat   = (RTGCPTR)pResult->u.u64Number;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_FLAT, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
}


/**
 * Physical address (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
DECLCALLBACK(int) dbgcOpAddrPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAddrPhys\n"));
    int         rc;
    DBGFADDRESS Address;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
            rc = DBGFR3AddrToPhys(pDbgc->pVM, pDbgc->idCpu,
                                  DBGFR3AddrFromFlat(pDbgc->pVM, &Address, pArg->u.GCFlat),
                                  &pResult->u.GCPhys);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
            Assert(pDbgc->pVM);
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, pDbgc->idCpu, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (RT_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
                rc = DBGFR3AddrToPhys(pDbgc->pVM, pDbgc->idCpu, &Address, &pResult->u.GCPhys);
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;
            }
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_PHYS:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_HC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
            rc = PGMR3DbgR3Ptr2GCPhys(pDbgc->pVM, pArg->u.pvHCFlat, &pResult->u.GCPhys);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            /** @todo more memory types! */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_GC_PHYS;
            pResult->u.GCPhys   = (RTGCPHYS)pResult->u.u64Number;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_PHYS, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return VINF_SUCCESS;
}


/**
 * Physical host address (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
DECLCALLBACK(int) dbgcOpAddrHostPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAddrPhys\n"));
    DBGFADDRESS Address;
    int         rc;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = DBGFR3AddrToHostPhys(pDbgc->pVM, pDbgc->idCpu,
                                      DBGFR3AddrFromFlat(pDbgc->pVM, &Address, pArg->u.GCFlat),
                                      &pResult->u.GCPhys);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, pDbgc->idCpu, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (RT_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
                rc = DBGFR3AddrToHostPhys(pDbgc->pVM, pDbgc->idCpu, &Address, &pResult->u.GCPhys);
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = DBGFR3AddrToHostPhys(pDbgc->pVM, pDbgc->idCpu,
                                      DBGFR3AddrFromPhys(pDbgc->pVM, &Address, pArg->u.GCPhys),
                                      &pResult->u.GCPhys);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = PGMR3DbgR3Ptr2HCPhys(pDbgc->pVM, pArg->u.pvHCFlat, &pResult->u.HCPhys);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            /** @todo more memory types! */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_HC_PHYS;
            pResult->u.HCPhys   = (RTGCPHYS)pResult->u.u64Number;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_HC_PHYS, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return VINF_SUCCESS;
}


/**
 * Host address (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
DECLCALLBACK(int) dbgcOpAddrHost(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAddrHost\n"));
    int             rc;
    DBGFADDRESS     Address;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
            rc = DBGFR3AddrToVolatileR3Ptr(pDbgc->pVM, pDbgc->idCpu,
                                           DBGFR3AddrFromFlat(pDbgc->pVM, &Address, pArg->u.GCFlat),
                                           false /*fReadOnly */,
                                           &pResult->u.pvHCFlat);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
            Assert(pDbgc->pVM);
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, pDbgc->idCpu, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (RT_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
                rc = DBGFR3AddrToVolatileR3Ptr(pDbgc->pVM, pDbgc->idCpu, &Address,
                                               false /*fReadOnly*/, &pResult->u.pvHCFlat);
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;
            }
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
            rc = DBGFR3AddrToVolatileR3Ptr(pDbgc->pVM, pDbgc->idCpu,
                                           DBGFR3AddrFromPhys(pDbgc->pVM, &Address, pArg->u.GCPhys),
                                           false /*fReadOnly */,
                                           &pResult->u.pvHCFlat);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FLAT:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
            /** @todo !*/
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_HC_FLAT;
            pResult->u.pvHCFlat = (void *)(uintptr_t)pResult->u.u64Number;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_HC_FLAT, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
}


/**
 * Bitwise not (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrFar(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAddrFar\n"));
    int     rc;

    switch (pArg1->enmType)
    {
        case DBGCVAR_TYPE_STRING:
            rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_NUMBER, pResult);
            if (RT_FAILURE(rc))
                return rc;
            break;
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            break;

        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    pResult->u.GCFar.sel = (RTSEL)pResult->u.u64Number;

    /* common code for the two types we support. */
    switch (pArg2->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFar.off = pArg2->u.GCFlat;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.HCFar.off = pArg2->u.GCFlat;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_NUMBER:
            pResult->u.GCFar.off = (RTGCPTR)pArg2->u.u64Number;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_STRING:
        {
            DBGCVAR Var;
            rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u.GCFar.off = (RTGCPTR)Var.u.u64Number;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;
        }

        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return VINF_SUCCESS;

}


/**
 * Multiplication operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMult(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpMult\n"));
    DBGC_GEN_ARIT_POINTER_TO_THE_LEFT(pArg1, pArg2);
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, *, false);
}


/**
 * Division operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpDiv(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpDiv\n"));
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, /, true);
}


/**
 * Modulus operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMod(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpMod\n"));
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, %, false);
}


/**
 * Addition operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAdd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpAdd\n"));

    /*
     * An addition operation will return (when possible) the left side type in the
     * expression. We make an omission for numbers, where we'll take the right side
     * type instead. An expression where only the left hand side is a string we'll
     * use the right hand type assuming that the string is a symbol.
     */
    if (    (pArg1->enmType == DBGCVAR_TYPE_NUMBER && pArg2->enmType != DBGCVAR_TYPE_STRING)
        ||  (pArg1->enmType == DBGCVAR_TYPE_STRING && pArg2->enmType != DBGCVAR_TYPE_STRING))
    {
        PCDBGCVAR pTmp = pArg2;
        pArg2 = pArg1;
        pArg1 = pTmp;
    }
    DBGCVAR     Sym1, Sym2;
    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, &Sym1);
        if (RT_FAILURE(rc))
            return rc;
        pArg1 = &Sym1;

        rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_ANY, &Sym2);
        if (RT_FAILURE(rc))
            return rc;
        pArg2 = &Sym2;
    }

    int         rc;
    DBGCVAR     Var;
    DBGCVAR     Var2;
    switch (pArg1->enmType)
    {
        /*
         * GC Flat
         */
        case DBGCVAR_TYPE_GC_FLAT:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat += pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Far
         */
        case DBGCVAR_TYPE_GC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.GCFar.off += (RTGCPTR)pArg2->u.u64Number;
                    break;
                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat += pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Phys
         */
        case DBGCVAR_TYPE_GC_PHYS:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrPhys(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    if (Var.enmType != DBGCVAR_TYPE_GC_PHYS)
                        return VERR_PARSE_INVALID_OPERATION;
                    pResult->u.GCPhys += Var.u.GCPhys;
                    break;
            }
            break;

        /*
         * HC Flat
         */
        case DBGCVAR_TYPE_HC_FLAT:
            *pResult = *pArg1;
            rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
            if (RT_FAILURE(rc))
                return rc;
            rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat + (uintptr_t)Var.u.pvHCFlat;
            break;

        /*
         * HC Far
         */
        case DBGCVAR_TYPE_HC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.HCFar.off += (uintptr_t)pArg2->u.u64Number;
                    break;

                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat + (uintptr_t)Var.u.pvHCFlat;
                    break;
            }
            break;

        /*
         * HC Phys
         */
        case DBGCVAR_TYPE_HC_PHYS:
            *pResult = *pArg1;
            rc = dbgcOpAddrHostPhys(pDbgc, pArg2, &Var);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u.HCPhys += Var.u.HCPhys;
            break;

        /*
         * Numbers (see start of function)
         */
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                    rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                case DBGCVAR_TYPE_NUMBER:
                    pResult->u.u64Number += pArg2->u.u64Number;
                    break;
                default:
                    return VERR_PARSE_INVALID_OPERATION;
            }
            break;

        default:
            return VERR_PARSE_INVALID_OPERATION;

    }
    return VINF_SUCCESS;
}


/**
 * Subtration operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpSub(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpSub\n"));

    /*
     * An subtraction operation will return the left side type in the expression.
     * However, if the left hand side is a number and the right hand a pointer of
     * some kind we'll convert the left hand side to the same type as the right hand.
     * Any strings will be attempted resolved as symbols.
     */
    DBGCVAR     Sym1, Sym2;
    if (    pArg2->enmType == DBGCVAR_TYPE_STRING
        &&  (   pArg1->enmType == DBGCVAR_TYPE_NUMBER
             || pArg1->enmType == DBGCVAR_TYPE_STRING))
    {
        int rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_ANY, &Sym2);
        if (RT_FAILURE(rc))
            return rc;
        pArg2 = &Sym2;
    }

    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        DBGCVARTYPE enmType;
        switch (pArg2->enmType)
        {
            case DBGCVAR_TYPE_NUMBER:
                enmType = DBGCVAR_TYPE_ANY;
                break;
            case DBGCVAR_TYPE_GC_FLAT:
            case DBGCVAR_TYPE_GC_PHYS:
            case DBGCVAR_TYPE_HC_FLAT:
            case DBGCVAR_TYPE_HC_PHYS:
                enmType = pArg2->enmType;
                break;
            case DBGCVAR_TYPE_GC_FAR:
                enmType = DBGCVAR_TYPE_GC_FLAT;
                break;
            case DBGCVAR_TYPE_HC_FAR:
                enmType = DBGCVAR_TYPE_HC_FLAT;
                break;

            default:
            case DBGCVAR_TYPE_STRING:
                AssertMsgFailed(("Can't happen\n"));
                enmType = DBGCVAR_TYPE_STRING;
                break;
        }
        if (enmType != DBGCVAR_TYPE_STRING)
        {
            int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, &Sym1);
            if (RT_FAILURE(rc))
                return rc;
            pArg1 = &Sym1;
        }
    }
    else if (pArg1->enmType == DBGCVAR_TYPE_NUMBER)
    {
        PFNDBGCOPUNARY pOp = NULL;
        switch (pArg2->enmType)
        {
            case DBGCVAR_TYPE_GC_FAR:
            case DBGCVAR_TYPE_GC_FLAT:
                pOp = dbgcOpAddrFlat;
                break;
            case DBGCVAR_TYPE_GC_PHYS:
                pOp = dbgcOpAddrPhys;
                break;
            case DBGCVAR_TYPE_HC_FAR:
            case DBGCVAR_TYPE_HC_FLAT:
                pOp = dbgcOpAddrHost;
                break;
            case DBGCVAR_TYPE_HC_PHYS:
                pOp = dbgcOpAddrHostPhys;
                break;
            case DBGCVAR_TYPE_NUMBER:
                break;
            default:
            case DBGCVAR_TYPE_STRING:
                AssertMsgFailed(("Can't happen\n"));
                break;
        }
        if (pOp)
        {
            int rc = pOp(pDbgc, pArg1, &Sym1);
            if (RT_FAILURE(rc))
                return rc;
            pArg1 = &Sym1;
        }
    }


    /*
     * Normal processing.
     */
    int         rc;
    DBGCVAR     Var;
    DBGCVAR     Var2;
    switch (pArg1->enmType)
    {
        /*
         * GC Flat
         */
        case DBGCVAR_TYPE_GC_FLAT:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat -= pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Far
         */
        case DBGCVAR_TYPE_GC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.GCFar.off -= (RTGCPTR)pArg2->u.u64Number;
                    break;
                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat -= pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Phys
         */
        case DBGCVAR_TYPE_GC_PHYS:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrPhys(pDbgc, pArg2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    if (Var.enmType != DBGCVAR_TYPE_GC_PHYS)
                        return VERR_PARSE_INVALID_OPERATION;
                    pResult->u.GCPhys -= Var.u.GCPhys;
                    break;
            }
            break;

        /*
         * HC Flat
         */
        case DBGCVAR_TYPE_HC_FLAT:
            *pResult = *pArg1;
            rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
            if (RT_FAILURE(rc))
                return rc;
            rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat - (uintptr_t)Var.u.pvHCFlat;
            break;

        /*
         * HC Far
         */
        case DBGCVAR_TYPE_HC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.HCFar.off -= (uintptr_t)pArg2->u.u64Number;
                    break;

                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                    pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat - (uintptr_t)Var.u.pvHCFlat;
                    break;
            }
            break;

        /*
         * HC Phys
         */
        case DBGCVAR_TYPE_HC_PHYS:
            *pResult = *pArg1;
            rc = dbgcOpAddrHostPhys(pDbgc, pArg2, &Var);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u.HCPhys -= Var.u.HCPhys;
            break;

        /*
         * Numbers (see start of function)
         */
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                    rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (RT_FAILURE(rc))
                        return rc;
                case DBGCVAR_TYPE_NUMBER:
                    pResult->u.u64Number -= pArg2->u.u64Number;
                    break;
                default:
                    return VERR_PARSE_INVALID_OPERATION;
            }
            break;

        default:
            return VERR_PARSE_INVALID_OPERATION;

    }
    return VINF_SUCCESS;
}


/**
 * Bitwise shift left operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseShiftLeft(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseShiftLeft\n"));
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, <<, false);
}


/**
 * Bitwise shift right operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseShiftRight(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseShiftRight\n"));
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, >>, false);
}


/**
 * Bitwise and operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseAnd\n"));
    DBGC_GEN_ARIT_POINTER_TO_THE_LEFT(pArg1, pArg2);
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, &, false);
}


/**
 * Bitwise exclusive or operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseXor(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseXor\n"));
    DBGC_GEN_ARIT_POINTER_TO_THE_LEFT(pArg1, pArg2);
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, ^, false);
}


/**
 * Bitwise inclusive or operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseOr\n"));
    DBGC_GEN_ARIT_POINTER_TO_THE_LEFT(pArg1, pArg2);
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, |, false);
}


/**
 * Boolean and operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBooleanAnd\n"));
    /** @todo force numeric return value? */
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, &&, false);
}


/**
 * Boolean or operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBooleanOr\n"));
    /** @todo force numeric return value? */
    DBGC_GEN_ARIT_BINARY_OP(pDbgc, pArg1, pArg2, pResult, ||, false);
}


/**
 * Range to operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeLength(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpRangeLength\n"));

    /*
     * Make result. Strings needs to be resolved into symbols.
     */
    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, pResult);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
        *pResult = *pArg1;

    /*
     * Convert 2nd argument to element count.
     */
    pResult->enmRangeType = DBGCVAR_RANGE_ELEMENTS;
    switch (pArg2->enmType)
    {
        case DBGCVAR_TYPE_NUMBER:
            pResult->u64Range = pArg2->u.u64Number;
            break;

        case DBGCVAR_TYPE_STRING:
        {
            int rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, pResult);
            if (RT_FAILURE(rc))
                return rc;
            pResult->u64Range = pArg2->u.u64Number;
            break;
        }

        default:
            return VERR_PARSE_INVALID_OPERATION;
    }

    return VINF_SUCCESS;
}


/**
 * Range to operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeLengthBytes(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpRangeLengthBytes\n"));
    int rc = dbgcOpRangeLength(pDbgc, pArg1, pArg2, pResult);
    if (RT_SUCCESS(rc))
        pResult->enmRangeType = DBGCVAR_RANGE_BYTES;
    return rc;
}


/**
 * Range to operator (binary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeTo(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpRangeTo\n"));

    /*
     * Calc number of bytes between the two args.
     */
    DBGCVAR Diff;
    int rc = dbgcOpSub(pDbgc, pArg2, pArg1, &Diff);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Use the diff as the range of Arg1.
     */
    *pResult = *pArg1;
    pResult->enmRangeType = DBGCVAR_RANGE_BYTES;
    switch (Diff.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u64Range = (RTGCUINTPTR)Diff.u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u64Range = Diff.u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u64Range = (uintptr_t)Diff.u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u64Range = Diff.u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u64Range = Diff.u.u64Number;
            break;

        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_HC_FAR:
        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_PARSE_INVALID_OPERATION;
    }

    return VINF_SUCCESS;
}


/**
 * Searches for an operator descriptor which matches the start of
 * the expression given us.
 *
 * @returns Pointer to the operator on success.
 * @param   pDbgc           The debug console instance.
 * @param   pszExpr         Pointer to the expression string which might start with an operator.
 * @param   fPreferBinary   Whether to favour binary or unary operators.
 *                          Caller must assert that it's the disired type! Both types will still
 *                          be returned, this is only for resolving duplicates.
 * @param   chPrev          The previous char. Some operators requires a blank in front of it.
 */
PCDBGCOP dbgcOperatorLookup(PDBGC pDbgc, const char *pszExpr, bool fPreferBinary, char chPrev)
{
    PCDBGCOP    pOp = NULL;
    for (unsigned iOp = 0; iOp < RT_ELEMENTS(g_aOps); iOp++)
    {
        if (     g_aOps[iOp].szName[0] == pszExpr[0]
            &&  (!g_aOps[iOp].szName[1] || g_aOps[iOp].szName[1] == pszExpr[1])
            &&  (!g_aOps[iOp].szName[2] || g_aOps[iOp].szName[2] == pszExpr[2]))
        {
            /*
             * Check that we don't mistake it for some other operator which have more chars.
             */
            unsigned j;
            for (j = iOp + 1; j < RT_ELEMENTS(g_aOps); j++)
                if (    g_aOps[j].cchName > g_aOps[iOp].cchName
                    &&  g_aOps[j].szName[0] == pszExpr[0]
                    &&  (!g_aOps[j].szName[1] || g_aOps[j].szName[1] == pszExpr[1])
                    &&  (!g_aOps[j].szName[2] || g_aOps[j].szName[2] == pszExpr[2]) )
                    break;
            if (j < RT_ELEMENTS(g_aOps))
                continue;       /* we'll catch it later. (for theoretical +,++,+++ cases.) */
            pOp = &g_aOps[iOp];

            /*
             * Prefered type?
             */
            if (g_aOps[iOp].fBinary == fPreferBinary)
                break;
        }
    }

    if (pOp)
        Log2(("dbgcOperatorLookup: pOp=%p %s\n", pOp, pOp->szName));
    NOREF(pDbgc); NOREF(chPrev);
    return pOp;
}

