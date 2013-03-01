/** @file
 *
 * VBox HDD container test utility - scripting engine, interpreter.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOGGROUP LOGGROUP_DEFAULT
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include <VBox/log.h>

#include "VDScriptAst.h"
#include "VDScriptStack.h"
#include "VDScriptInternal.h"

/**
 * Interpreter variable.
 */
typedef struct VDSCRIPTINTERPVAR
{
    /** String space core. */
    RTSTRSPACECORE              Core;
    /** Value. */
    VDSCRIPTARG                 Value;
} VDSCRIPTINTERPVAR;
/** Pointer to an interpreter variable. */
typedef VDSCRIPTINTERPVAR *PVDSCRIPTINTERPVAR;

/**
 * Block scope.
 */
typedef struct VDSCRIPTINTERPSCOPE
{
    /** Pointer to the enclosing scope if available. */
    struct VDSCRIPTINTERPSCOPE *pParent;
    /** String space of declared variables in this scope. */
    RTSTRSPACE                  hStrSpaceVar;
} VDSCRIPTINTERPSCOPE;
/** Pointer to a scope block. */
typedef VDSCRIPTINTERPSCOPE *PVDSCRIPTINTERPSCOPE;

/**
 * Function call.
 */
typedef struct VDSCRIPTINTERPFNCALL
{
    /** Pointer to the caller of this function. */
    struct VDSCRIPTINTERPFNCALL *pCaller;
    /** Root scope of this function. */
    VDSCRIPTINTERPSCOPE          ScopeRoot;
    /** Current scope in this function. */
    PVDSCRIPTINTERPSCOPE         pScopeCurr;
} VDSCRIPTINTERPFNCALL;
/** Pointer to a function call. */
typedef VDSCRIPTINTERPFNCALL *PVDSCRIPTINTERPFNCALL;

/**
 * Interpreter context.
 */
typedef struct VDSCRIPTINTERPCTX
{
    /** Pointer to the script context. */
    PVDSCRIPTCTXINT             pScriptCtx;
    /** Current function call entry. */
    PVDSCRIPTINTERPFNCALL       pFnCallCurr;
    /** Stack of calculated values. */
    VDSCRIPTSTACK               StackValues;
    /** Evaluation control stack. */
    VDSCRIPTSTACK               StackCtrl;
} VDSCRIPTINTERPCTX;
/** Pointer to an interpreter context. */
typedef VDSCRIPTINTERPCTX *PVDSCRIPTINTERPCTX;

/**
 * Interpreter control type.
 */
typedef enum VDSCRIPTINTERPCTRLTYPE
{
    VDSCRIPTINTERPCTRLTYPE_INVALID = 0,
    /** Function call to evaluate now, all values are computed
     * and are stored on the value stack.
     */
    VDSCRIPTINTERPCTRLTYPE_FN_CALL,
    /** Cleanup the function call, deleting the scope and restoring the previous one. */
    VDSCRIPTINTERPCTRLTYPE_FN_CALL_CLEANUP,
    /** If statement to evaluate now, the guard is on the stack. */
    VDSCRIPTINTERPCTRLTYPE_IF,
    VDSCRIPTINTERPCTRLTYPE_32BIT_HACK = 0x7fffffff
} VDSCRIPTINTERPCTRLTYPE;
/** Pointer to a control type. */
typedef VDSCRIPTINTERPCTRLTYPE *PVDSCRIPINTERPCTRLTYPE;

/**
 * Interpreter stack control entry.
 */
typedef struct VDSCRIPTINTERPCTRL
{
    /** Flag whether this entry points to an AST node to evaluate. */
    bool                           fEvalAst;
    /** Flag dependent data. */
    union
    {
        /** Pointer to the AST node to interprete. */
        PVDSCRIPTASTCORE           pAstNode;
        /** Interpreter control structure. */
        struct
        {
            /** Type of control. */
            VDSCRIPTINTERPCTRLTYPE enmCtrlType;
            /** Function call data. */
            struct
            {
                /** Function to call. */
                PVDSCRIPTASTFN     pAstFn;
            } FnCall;
        } Ctrl;
    };
} VDSCRIPTINTERPCTRL;
/** Pointer to an exec stack control entry. */
typedef VDSCRIPTINTERPCTRL *PVDSCRIPTINTERPCTRL;

/**
 * Record an error while interpreting.
 *
 * @returns VBox status code passed.
 * @param   pThis      The interpreter context.
 * @param   rc         The status code to record.
 * @param   RT_SRC_POS Position in the source code.
 * @param   pszFmt     Format string.
 */
static int vdScriptInterpreterError(PVDSCRIPTINTERPCTX pThis, int rc, RT_SRC_POS_DECL, const char *pszFmt, ...)
{
    return rc;
}

/**
 * Pops the topmost value from the value stack.
 *
 * @returns nothing.
 * @param   pThis      The interpreter context.
 * @param   pVal       Where to store the value.
 */
DECLINLINE(void) vdScriptInterpreterPopValue(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTARG pVal)
{
    PVDSCRIPTARG pValStack = (PVDSCRIPTARG)vdScriptStackGetUsed(&pThis->StackValues);
    AssertPtrReturnVoid(pValStack);

    *pVal = *pValStack;
    vdScriptStackPop(&pThis->StackValues);
}

/**
 * Pushes a given value onto the value stack.
 */
DECLINLINE(int) vdScriptInterpreterPushValue(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTARG pVal)
{
    PVDSCRIPTARG pValStack = (PVDSCRIPTARG)vdScriptStackGetUsed(&pThis->StackValues);
    if (!pValStack)
        return vdScriptInterpreterError(pThis, VERR_NO_MEMORY, RT_SRC_POS, "Out of memory pushing a value on the value stack");

    *pValStack = *pVal;
    vdScriptStackPush(&pThis->StackValues);
    return VINF_SUCCESS;
}

/**
 * Pushes a control entry without additional data onto the stack.
 *
 * @returns VBox status code.
 * @param   pThis          The interpreter context.
 * @param   enmCtrlType    The control entry type.
 */
DECLINLINE(int) vdScriptInterpreterPushNonDataCtrlEntry(PVDSCRIPTINTERPCTX pThis,
                                                        VDSCRIPTINTERPCTRLTYPE enmCtrlType)
{
    PVDSCRIPTINTERPCTRL pCtrl = NULL;

    pCtrl = (PVDSCRIPTINTERPCTRL)vdScriptStackGetUnused(&pThis->StackCtrl);
    if (pCtrl)
    {
        pCtrl->fEvalAst = false;
        pCtrl->Ctrl.enmCtrlType = enmCtrlType;
        vdScriptStackPush(&pThis->StackCtrl);
        return VINF_SUCCESS;
    }

    return vdScriptInterpreterError(pThis, VERR_NO_MEMORY, RT_SRC_POS, "Out of memory adding an entry on the control stack");
}

/**
 * Pushes an AST node onto the control stack.
 *
 * @returns VBox status code.
 * @param   pThis          The interpreter context.
 * @param   enmCtrlType    The control entry type.
 */
DECLINLINE(int) vdScriptInterpreterPushAstEntry(PVDSCRIPTINTERPCTX pThis,
                                                PVDSCRIPTASTCORE pAstNode)
{
    PVDSCRIPTINTERPCTRL pCtrl = NULL;

    pCtrl = (PVDSCRIPTINTERPCTRL)vdScriptStackGetUnused(&pThis->StackCtrl);

    if (pCtrl)
    {
        pCtrl->fEvalAst = true;
        pCtrl->pAstNode = pAstNode;
        vdScriptStackPush(&pThis->StackCtrl);
        return VINF_SUCCESS;
    }

    return vdScriptInterpreterError(pThis, VERR_NO_MEMORY, RT_SRC_POS, "Out of memory adding an entry on the control stack");
}

/**
 * Evaluate a statement.
 *
 * @returns VBox status code.
 * @param   pThis      The interpreter context.
 * @param   pStmt      The statement to evaluate.
 */
static int vdScriptInterpreterEvaluateStatement(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTASTSTMT pStmt)
{
    int rc = VERR_NOT_IMPLEMENTED;
    return rc;
}

/**
 * Evaluate an expression.
 *
 * @returns VBox status code.
 * @param   pThis      The interpreter context.
 * @param   pExpr      The expression to evaluate.
 */
static int vdScriptInterpreterEvaluateExpression(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTASTEXPR pExpr)
{
    int rc = VERR_NOT_IMPLEMENTED;
    return rc;
}

/**
 * Evaluates the given AST node.
 *
 * @returns VBox statuse code.
 * @param   pThis      The interpreter context.
 * @param   pAstNode   The AST node to interpret.
 */
static int vdScriptInterpreterEvaluateAst(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTASTCORE pAstNode)
{
    int rc = VERR_NOT_IMPLEMENTED;

    switch (pAstNode->enmClass)
    {
        case VDSCRIPTASTCLASS_DECLARATION:
        {
            break;
        }
        case VDSCRIPTASTCLASS_IDENTIFIER:
        {
            /*
             * Identifiers are pushed only to the stack as an identifier for a variable.
             * Look it up and push the value onto the value stack.
             */
            PVDSCRIPTASTIDE pIde = (PVDSCRIPTASTIDE)pAstNode;
            PVDSCRIPTINTERPVAR pVar = (PVDSCRIPTINTERPVAR)RTStrSpaceGet(&pThis->pFnCallCurr->pScopeCurr->hStrSpaceVar, pIde->aszIde);

            AssertPtrReturn(pVar, VERR_IPE_UNINITIALIZED_STATUS);
            rc = vdScriptInterpreterPushValue(pThis, &pVar->Value);
            break;
        }
        case VDSCRIPTASTCLASS_STATEMENT:
        {
            rc = vdScriptInterpreterEvaluateStatement(pThis, (PVDSCRIPTASTSTMT)pAstNode);
            break;
        }
        case VDSCRIPTASTCLASS_EXPRESSION:
        {
            rc = vdScriptInterpreterEvaluateExpression(pThis, (PVDSCRIPTASTEXPR)pAstNode);
            break;
        }
        /* These should never ever appear here. */
        case VDSCRIPTASTCLASS_FUNCTION:
        case VDSCRIPTASTCLASS_FUNCTIONARG:
        case VDSCRIPTASTCLASS_INVALID:
        default:
            AssertMsgFailed(("Invalid AST node class: %d\n", pAstNode->enmClass));
    }

    return rc;
}

/**
 * Destroy variable string space callback.
 */
static DECLCALLBACK(int) vdScriptInterpreterVarSpaceDestroy(PRTSTRSPACECORE pStr, void *pvUser)
{
    RTMemFree(pStr);
    return VINF_SUCCESS;
}

/**
 * Evaluate a function call.
 *
 * @returns VBox status code.
 * @param   pThis      The interpreter context.
 * @param
 */
static int vdScriptInterpreterFnCall(PVDSCRIPTINTERPCTX pThis, PVDSCRIPTASTFN pAstFn)
{
    int rc = VINF_SUCCESS;

    /* Add function call cleanup marker on the stack first. */
    rc = vdScriptInterpreterPushNonDataCtrlEntry(pThis, VDSCRIPTINTERPCTRLTYPE_FN_CALL_CLEANUP);
    if (RT_SUCCESS(rc))
    {
        /* Create function call frame and set it up. */
        PVDSCRIPTINTERPFNCALL pFnCall = (PVDSCRIPTINTERPFNCALL)RTMemAllocZ(sizeof(VDSCRIPTINTERPFNCALL));
        if (pFnCall)
        {
            pFnCall->pCaller = pThis->pFnCallCurr;
            pFnCall->ScopeRoot.pParent = NULL;
            pFnCall->ScopeRoot.hStrSpaceVar = NULL;

            /* Add the variables, remember order. The first variable in the argument has the value at the top of the value stack. */
            PVDSCRIPTASTFNARG pArg = RTListGetFirst(&pAstFn->ListArgs, VDSCRIPTASTFNARG, Core.ListNode);
            for (unsigned i = 0; i < pAstFn->cArgs; i++)
            {
                PVDSCRIPTINTERPVAR pVar = (PVDSCRIPTINTERPVAR)RTMemAllocZ(sizeof(VDSCRIPTINTERPVAR));
                if (pVar)
                {
                    pVar->Core.pszString = pArg->pArgIde->aszIde;
                    pVar->Core.cchString = pArg->pArgIde->cchIde;
                    vdScriptInterpreterPopValue(pThis, &pVar->Value);
                    bool fInserted = RTStrSpaceInsert(&pFnCall->ScopeRoot.hStrSpaceVar, &pVar->Core);
                    Assert(fInserted);
                }
                else
                {
                    rc = vdScriptInterpreterError(pThis, VERR_NO_MEMORY, RT_SRC_POS, "Out of memory creating a variable");
                    break;
                }
                pArg = RTListGetNext(&pAstFn->ListArgs, pArg, VDSCRIPTASTFNARG, Core.ListNode);
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * Push compount statement on the control stack and make the newly created
                 * call frame the current one.
                 */
                rc = vdScriptInterpreterPushAstEntry(pThis, &pAstFn->pCompoundStmts->Core);
                if (RT_SUCCESS(rc))
                    pThis->pFnCallCurr = pFnCall;
            }

            if (RT_FAILURE(rc))
            {
                RTStrSpaceDestroy(&pFnCall->ScopeRoot.hStrSpaceVar, vdScriptInterpreterVarSpaceDestroy, NULL);
                RTMemFree(pFnCall);
            }
        }
        else
            rc = vdScriptInterpreterError(pThis, VERR_NO_MEMORY, RT_SRC_POS, "Out of memory creating a call frame");
    }

    return rc;
}

/**
 * The interpreter evaluation core loop.
 *
 * @returns VBox status code.
 * @param   pThis      The interpreter context.
 */
static int vdScriptInterpreterEvaluate(PVDSCRIPTINTERPCTX pThis)
{
    int rc = VINF_SUCCESS;
    PVDSCRIPTINTERPCTRL pCtrl = NULL;

    pCtrl = (PVDSCRIPTINTERPCTRL)vdScriptStackGetUsed(&pThis->StackCtrl);
    while (pCtrl)
    {
        if (pCtrl->fEvalAst)
        {
            PVDSCRIPTASTCORE pAstNode = pCtrl->pAstNode;
            vdScriptStackPop(&pThis->StackCtrl);

            rc = vdScriptInterpreterEvaluateAst(pThis, pAstNode);
        }
        else
        {
            switch (pCtrl->Ctrl.enmCtrlType)
            {
                case VDSCRIPTINTERPCTRLTYPE_FN_CALL:
                {
                    PVDSCRIPTASTFN pAstFn = pCtrl->Ctrl.FnCall.pAstFn;

                    vdScriptStackPop(&pThis->StackCtrl);
                    rc = vdScriptInterpreterFnCall(pThis, pAstFn);
                    break;
                }
                case VDSCRIPTINTERPCTRLTYPE_FN_CALL_CLEANUP:
                {
                    vdScriptStackPop(&pThis->StackCtrl);

                    /* Delete function call entry. */
                    AssertPtr(pThis->pFnCallCurr);
                    PVDSCRIPTINTERPFNCALL pFnCallFree = pThis->pFnCallCurr;

                    pThis->pFnCallCurr = pFnCallFree->pCaller;
                    Assert(pFnCallFree->pScopeCurr == &pFnCallFree->ScopeRoot);
                    RTStrSpaceDestroy(&pFnCallFree->ScopeRoot.hStrSpaceVar, vdScriptInterpreterVarSpaceDestroy, NULL);
                    RTMemFree(pFnCallFree);
                    break;
                }
                default:
                    AssertMsgFailed(("Invalid evaluation control type on the stack: %d\n",
                                     pCtrl->Ctrl.enmCtrlType));
            }
        }
        pCtrl = (PVDSCRIPTINTERPCTRL)vdScriptStackGetUsed(&pThis->StackCtrl);
    }

    return rc;
}

DECLHIDDEN(int) vdScriptCtxInterprete(PVDSCRIPTCTXINT pThis, const char *pszFn,
                                      PVDSCRIPTARG paArgs, unsigned cArgs,
                                      PVDSCRIPTARG pRet)
{
    int rc = VINF_SUCCESS;
    VDSCRIPTINTERPCTX InterpCtx;
    PVDSCRIPTFN pFn = NULL;

    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFn, VERR_INVALID_POINTER);
    AssertReturn(   (!cArgs && !paArgs)
                 || (cArgs && paArgs), VERR_INVALID_PARAMETER);

    InterpCtx.pScriptCtx  = pThis;
    InterpCtx.pFnCallCurr = NULL;
    vdScriptStackInit(&InterpCtx.StackValues, sizeof(VDSCRIPTARG));
    vdScriptStackInit(&InterpCtx.StackCtrl, sizeof(VDSCRIPTINTERPCTRL));

    pFn = (PVDSCRIPTFN)RTStrSpaceGet(&pThis->hStrSpaceFn, pszFn);
    if (pFn)
    {
        if (cArgs == pFn->cArgs)
        {
            /* Push the arguments onto the stack. */
            /** @todo: Check expected and given argument types. */
            for (unsigned i = 0; i < cArgs; i++)
            {
                PVDSCRIPTARG pArg = (PVDSCRIPTARG)vdScriptStackGetUnused(&InterpCtx.StackValues);
                *pArg = paArgs[i];
                vdScriptStackPush(&InterpCtx.StackValues);
            }

            if (RT_SUCCESS(rc))
            {
                /* Push the AST onto the stack. */
                PVDSCRIPTINTERPCTRL pCtrlFn = (PVDSCRIPTINTERPCTRL)vdScriptStackGetUnused(&InterpCtx.StackCtrl);
                pCtrlFn->fEvalAst = false;
                pCtrlFn->Ctrl.enmCtrlType = VDSCRIPTINTERPCTRLTYPE_FN_CALL;
                pCtrlFn->Ctrl.FnCall.pAstFn = pFn->Type.Internal.pAstFn;
                vdScriptStackPush(&InterpCtx.StackCtrl);

                /* Run the interpreter. */
                rc = vdScriptInterpreterEvaluate(&InterpCtx);
            }
        }
        else
            rc = vdScriptInterpreterError(&InterpCtx, VERR_INVALID_PARAMETER, RT_SRC_POS, "Invalid number of parameters, expected %d got %d", pFn->cArgs, cArgs);
    }
    else
        rc = vdScriptInterpreterError(&InterpCtx, VERR_NOT_FOUND, RT_SRC_POS, "Function with identifier \"%s\" not found", pszFn);


    return rc;
}
