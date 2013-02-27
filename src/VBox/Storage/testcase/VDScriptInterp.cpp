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

#include <VBox/log.h>

#include "VDScriptAst.h"
#include "VDScriptInternal.h"

/**
 * Interpreter context
 */
typedef struct VDSCRIPTINTERPCTX
{
    /** List of currently accessible variables and their value. */

} VDSCRIPTINTERPCTX;
/** Pointer to an interpreter context. */
typedef VDSCRIPTINTERPCTX *PVDSCRIPTINTERPCTX;

/**
 * Record an error while interpreting.
 *
 * @returns VBox status code passed.
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
