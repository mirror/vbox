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
#include "VDScriptInternal.h"

/**
 * Block scope.
 */
typedef struct VDSCRIPTINTERPSCOPE
{
    /** Pointer to the enclosing scope if available. */
    struct VDSCRIPTINTERPSCOPE *pParent;
    /** String space of accessible variables. */
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
    /** Current function call entry. */
    PVDSCRIPTINTERPFNCALL       pFnCallCurr;
    /** Stack of calculated values. */
    PVDSCRIPTARG                pStackVal;
    /** Number of values on the stack. */
    unsigned                    cValuesOnStack;
    /** Maximum number of values the stack can hold currently. */
    unsigned                    cValuesOnStackMax;
    /** Stack of executed AST nodes. */
    PVDSCRIPTASTCORE           *ppStackAstCompute;
    /** Number of AST nodes on the stack. */
    unsigned                    cAstNodesOnStack;
    /** Maximum number of AST nodes the stack can hold. */
    unsigned                    cAstNodesOnStackMax;
} VDSCRIPTINTERPCTX;
/** Pointer to an interpreter context. */
typedef VDSCRIPTINTERPCTX *PVDSCRIPTINTERPCTX;

/**
 * Record an error while interpreting.
 *
 * @returns VBox status code passed.
 * @param   pThis      The script context.
 * @param   rc         The status code to record.
 * @param   RT_SRC_POS Position in the source code.
 * @param   pszFmt     Format string.
 */
static int vdScriptInterpreterError(PVDSCRIPTCTXINT pThis, int rc, RT_SRC_POS_DECL, const char *pszFmt, ...)
{
    return rc;
}

DECLHIDDEN(int) vdScriptCtxInterprete(PVDSCRIPTCTXINT pThis, PVDSCRIPTASTFN pAstFn, 
                                      PVDSCRIPTARG paArgs, unsigned cArgs,
                                      PVDSCRIPTARG pRet)
{
    return VERR_NOT_IMPLEMENTED;
}
