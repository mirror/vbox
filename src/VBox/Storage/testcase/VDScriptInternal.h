/** @file
 *
 * VBox HDD container test utility - scripting engine, internal script structures.
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
#ifndef _VDScriptInternal_h__
#define _VDScriptInternal_h__

#include <iprt/list.h>
#include <iprt/string.h>

#include "VDScript.h"

/** Pointer to a tokenize state. */
typedef struct VDTOKENIZER *PVDTOKENIZER;

/**
 * Script context.
 */
typedef struct VDSCRIPTCTXINT
{
    /** String space of external registered and source defined functions. */
    RTSTRSPACE        hStrSpaceFn;
    /** List of ASTs for functions - VDSCRIPTASTFN. */
    RTLISTANCHOR      ListAst;
    /** Pointer to the current tokenizer state. */
    PVDTOKENIZER      pTokenizer;
} VDSCRIPTCTXINT;
/** Pointer to a script context. */
typedef VDSCRIPTCTXINT *PVDSCRIPTCTXINT;

/**
 * Check the context for type correctness.
 *
 * @returns VBox status code.
 * @param   pThis    The script context.
 */
DECLHIDDEN(int) vdScriptCtxCheck(PVDSCRIPTCTXINT pThis);

/**
 * Interprete a given function AST. The executed functions
 * must be type correct, otherwise the behavior is undefined
 * (Will assert in debug builds).
 *
 * @returns VBox status code.
 * @param   pThis    The script context.
 * @param   pAstFn   The function AST to interpret.
 * @param   paArgs   Arguments to pass to the function.
 * @param   cArgs    Number of arguments.
 * @param   pRet     Where to store the return value on success.
 */
DECLHIDDEN(int) vdScriptCtxInterprete(PVDSCRIPTCTXINT pThis, PVDSCRIPTASTFN pAstFn, 
                                      PVDSCRIPTARG paArgs, unsigned cArgs,
                                      PVDSCRIPTARG pRet);

#endif /* _VDScriptInternal_h__ */
