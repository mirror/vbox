/** @file
 *
 * VBox HDD container test utility - scripting engine AST node related functions.
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

DECLHIDDEN(void) vdScriptAstNodeFree(PVDSCRIPTASTCORE pAstNode)
{
    switch (pAstNode->enmClass)
    {
        case VDSCRIPTASTCLASS_FUNCTION:
        {
            /** @todo. */
            break;
        }
        case VDSCRIPTASTCLASS_FUNCTIONARG:
        {
            PVDSCRIPTASTFNARG pAstNodeArg = (PVDSCRIPTASTFNARG)pAstNode;
            if (pAstNodeArg->pType)
                vdScriptAstNodeFree(&pAstNodeArg->pType->Core);
            if (pAstNodeArg->pArgIde)
                vdScriptAstNodeFree(&pAstNodeArg->pArgIde->Core);
            break;
        }
        case VDSCRIPTASTCLASS_IDENTIFIER:
            break;
        case VDSCRIPTASTCLASS_DECLARATION:
            break;
        case VDSCRIPTASTCLASS_STATEMENT:
            break;
        case VDSCRIPTASTCLASS_EXPRESSION:
            break;
        case VDSCRIPTASTCLASS_INVALID:
        default:
            AssertMsgFailedReturnVoid(("Invalid AST node class given %d\n", pAstNode->enmClass));
    }

    RTMemFree(pAstNode);
}

DECLHIDDEN(PVDSCRIPTASTCORE) vdScriptAstNodeAlloc(VDSCRIPTASTCLASS enmClass)
{
    size_t cbAlloc = 0;

    switch (enmClass)
    {
        case VDSCRIPTASTCLASS_FUNCTION:
            cbAlloc = sizeof(VDSCRIPTASTFN);
            break;
        case VDSCRIPTASTCLASS_FUNCTIONARG:
            cbAlloc = sizeof(VDSCRIPTASTFNARG);
            break;
        case VDSCRIPTASTCLASS_DECLARATION:
            cbAlloc = sizeof(VDSCRIPTASTDECL);
            break;
        case VDSCRIPTASTCLASS_STATEMENT:
            cbAlloc = sizeof(VDSCRIPTASTSTMT);
            break;
        case VDSCRIPTASTCLASS_EXPRESSION:
            cbAlloc = sizeof(VDSCRIPTASTEXPR);
            break;
        case VDSCRIPTASTCLASS_IDENTIFIER:
        case VDSCRIPTASTCLASS_INVALID:
        default:
            AssertMsgFailedReturn(("Invalid AST node class given %d\n", enmClass), NULL);
    }

    PVDSCRIPTASTCORE pAstNode = (PVDSCRIPTASTCORE)RTMemAllocZ(cbAlloc);
    if (pAstNode)
    {
        pAstNode->enmClass = enmClass;
        RTListInit(&pAstNode->ListNode);
    }

    return pAstNode;
}

DECLHIDDEN(PVDSCRIPTASTIDE) vdScriptAstNodeIdeAlloc(unsigned cchIde)
{
    PVDSCRIPTASTIDE pAstNode = (PVDSCRIPTASTIDE)RTMemAllocZ(RT_OFFSETOF(VDSCRIPTASTIDE, aszIde[cchIde + 1]));
    if (pAstNode)
    {
        pAstNode->Core.enmClass = VDSCRIPTASTCLASS_IDENTIFIER;
        RTListInit(&pAstNode->Core.ListNode);
    }

    return pAstNode;
}
