/** @file
 *
 * VBox HDD container test utility - scripting engine, AST related structures.
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
#ifndef _VDScriptAst_h__
#define _VDScriptAst_h__

#include <iprt/list.h>

/**
 * AST node classes.
 */
typedef enum VDSCRIPTASTCLASS
{
    /** Invalid. */
    VDSCRIPTASTCLASS_INVALID = 0,
    /** Function node. */
    VDSCRIPTASTCLASS_FUNCTION,
    /** Function argument. */
    VDSCRIPTASTCLASS_FUNCTIONARG,
    /** Identifier node. */
    VDSCRIPTASTCLASS_IDENTIFIER,
    /** Declaration node. */
    VDSCRIPTASTCLASS_DECLARATION,
    /** Statement node. */
    VDSCRIPTASTCLASS_STATEMENT,
    /** Expression node. */
    VDSCRIPTASTCLASS_EXPRESSION,
    /** @todo: Add if ... else, loops, ... */
    /** 32bit blowup. */
    VDSCRIPTASTCLASS_32BIT_HACK = 0x7fffffff
} VDSCRIPTASTCLASS;
/** Pointer to an AST node class. */
typedef VDSCRIPTASTCLASS *PVDSCRIPTASTCLASS;

/**
 * Core AST structure.
 */
typedef struct VDSCRIPTASTCORE
{
    /** The node class, used for verification. */
    VDSCRIPTASTCLASS enmClass;
    /** List which might be used. */
    RTLISTNODE       ListNode;
} VDSCRIPTASTCORE;
/** Pointer to an AST core structure. */
typedef VDSCRIPTASTCORE *PVDSCRIPTASTCORE;

/** Pointer to an statement node - forward declaration. */
typedef struct VDSCRIPTASTSTMT *PVDSCRIPTASTSTMT;

/**
 * AST identifier node.
 */
typedef struct VDSCRIPTASTIDE
{
    /** Core structure. */
    VDSCRIPTASTCORE    Core;
    /** Number of characters in the identifier, excluding the zero terminator. */
    unsigned           cchIde;
    /** Identifier, variable size. */
    char               aszIde[1];
} VDSCRIPTASTIDE;
/** Pointer to an identifer node. */
typedef VDSCRIPTASTIDE *PVDSCRIPTASTIDE;

/**
 * AST declaration node.
 */
typedef struct VDSCRIPTASTDECL
{
    /** Core structure. */
    VDSCRIPTASTCORE    Core;
    /** @todo */
} VDSCRIPTASTDECL;
/** Pointer to an declaration node. */
typedef VDSCRIPTASTDECL *PVDSCRIPTASTDECL;

/**
 * AST expression node.
 */
typedef struct VDSCRIPTASTEXPR
{
    /** Core structure. */
    VDSCRIPTASTCORE    Core;
    /** @todo */
    unsigned           uDummy;
} VDSCRIPTASTEXPR;
/** Pointer to an expression node. */
typedef VDSCRIPTASTEXPR *PVDSCRIPTASTEXPR;

/**
 * AST if node.
 */
typedef struct VDSCRIPTASTIF
{
    /** Conditional expression. */
    PVDSCRIPTASTEXPR   pCond;
    /** The true branch */
    PVDSCRIPTASTSTMT   pTrueStmt;
    /** The else branch, can be NULL if no else branch. */
    PVDSCRIPTASTSTMT   pElseStmt;
} VDSCRIPTASTIF;
/** Pointer to an expression node. */
typedef VDSCRIPTASTIF *PVDSCRIPTASTIF;

/**
 * AST switch node.
 */
typedef struct VDSCRIPTASTSWITCH
{
    /** Conditional expression. */
    PVDSCRIPTASTEXPR   pCond;
    /** The statement to follow. */
    PVDSCRIPTASTSTMT   pStmt;
} VDSCRIPTASTSWITCH;
/** Pointer to an expression node. */
typedef VDSCRIPTASTSWITCH *PVDSCRIPTASTSWITCH;

/**
 * AST while or do ... while node.
 */
typedef struct VDSCRIPTASTWHILE
{
    /** Flag whether this is a do while loop. */
    bool               fDoWhile;
    /** Conditional expression. */
    PVDSCRIPTASTEXPR   pCond;
    /** The statement to follow. */
    PVDSCRIPTASTSTMT   pStmt;
} VDSCRIPTASTWHILE;
/** Pointer to an expression node. */
typedef VDSCRIPTASTWHILE *PVDSCRIPTASTWHILE;

/**
 * AST for node.
 */
typedef struct VDSCRIPTASTFOR
{
    /** Initializer expression. */
    PVDSCRIPTASTEXPR   pExprStart;
    /** The exit condition. */
    PVDSCRIPTASTEXPR   pExprCond;
    /** The third expression (normally used to increase/decrease loop variable). */
    PVDSCRIPTASTEXPR   pExpr3;
    /** The for loop body. */
    PVDSCRIPTASTSTMT   pStmt;
} VDSCRIPTASTFOR;
/** Pointer to an expression node. */
typedef VDSCRIPTASTFOR *PVDSCRIPTASTFOR;

/**
 * Statement types.
 */
typedef enum VDSCRIPTSTMTTYPE
{
    /** Invalid. */
    VDSCRIPTSTMTTYPE_INVALID = 0,
    /** Labeled statement. */
    VDSCRIPTSTMTTYPE_LABELED,
    /** Compound statement. */
    VDSCRIPTSTMTTYPE_COMPOUND,
    /** Expression statement. */
    VDSCRIPTSTMTTYPE_EXPRESSION,
    /** if statement. */
    VDSCRIPTSTMTTYPE_IF,
    /** switch statement. */
    VDSCRIPTSTMTTYPE_SWITCH,
    /** while statement. */
    VDSCRIPTSTMTTYPE_WHILE,
    /** for statement. */
    VDSCRIPTSTMTTYPE_FOR,
    /** continue statement. */
    VDSCRIPTSTMTTYPE_CONTINUE,
    /** break statement. */
    VDSCRIPTSTMTTYPE_BREAK,
    /** return statement. */
    VDSCRIPTSTMTTYPE_RETURN,
    /** case statement. */
    VDSCRIPTSTMTTYPE_CASE,
    /** default statement. */
    VDSCRIPTSTMTTYPE_DEFAULT,
    /** 32bit hack. */
    VDSCRIPTSTMTTYPE_32BIT_HACK = 0x7fffffff
} VDSCRIPTSTMTTYPE;
/** Pointer to a statement type. */
typedef VDSCRIPTSTMTTYPE *PVDSCRIPTSTMTTYPE;

/**
 * AST statement node.
 */
typedef struct VDSCRIPTASTSTMT
{
    /** Core structure. */
    VDSCRIPTASTCORE    Core;
    /** Statement type */
    VDSCRIPTSTMTTYPE   enmStmtType;
    /** Statement type dependent data. */
    union
    {
        /** Labeled statement (case, default). */
        struct
        {
            /** Conditional expression, if NULL this is a statement for "default" */
            PVDSCRIPTASTEXPR   pCondExpr;
            /** Statement to execute. */
            PVDSCRIPTASTSTMT   pExec;
        } Labeled;
        /** Compound statement. */
        struct
        {
            /** List of declarations - VDSCRIPTASTDECL. */
            RTLISTANCHOR       ListDecls;
            /** List of statements - VDSCRIPTASTSTMT. */
            RTLISTANCHOR       ListStmts;
        } Compound;
        /** case statement. */
        struct
        {
            /** Pointer to the expression. */
            PVDSCRIPTASTEXPR   pExpr;
            /** Pointer to the statement. */
            PVDSCRIPTASTSTMT   pStmt;
        } Case;
        /** "if" statement. */
        VDSCRIPTASTIF          If;
        /** "switch" statement. */
        VDSCRIPTASTSWITCH      Switch;
        /** "while" or "do ... while" loop. */
        VDSCRIPTASTWHILE       While;
        /** "for" loop. */
        VDSCRIPTASTFOR         For;
        /** Pointer to another statement. */
        PVDSCRIPTASTSTMT       pStmt;
        /** Expression statement. */
        PVDSCRIPTASTEXPR       pExpr;
    };
} VDSCRIPTASTSTMT;

/**
 * AST node for one function argument.
 */
typedef struct VDSCRIPTASTFNARG
{
    /** Core structure. */
    VDSCRIPTASTCORE    Core;
    /** Identifier describing the type of the argument. */
    PVDSCRIPTASTIDE    pType;
    /** The name of the argument. */
    PVDSCRIPTASTIDE    pArgIde;
} VDSCRIPTASTFNARG;
/** Pointer to a AST function argument node. */
typedef VDSCRIPTASTFNARG *PVDSCRIPTASTFNARG;

/**
 * AST node describing a function.
 */
typedef struct VDSCRIPTASTFN
{
    /** Core structure. */
    VDSCRIPTASTCORE      Core;
    /** Identifier describing the return type. */
    PVDSCRIPTASTIDE      pRetType;
    /** Name of the function. */
    PVDSCRIPTASTIDE      pFnIde;
    /** Number of arguments in the list. */
    unsigned             cArgs;
    /** Argument list - VDSCRIPTASTFNARG. */
    RTLISTANCHOR         ListArgs;
    /** Compound statement node. */
    PVDSCRIPTASTSTMT     pCompoundStmts;
} VDSCRIPTASTFN;
/** Pointer to a function AST node. */
typedef VDSCRIPTASTFN *PVDSCRIPTASTFN;

/**
 * Free the given AST node and all subsequent nodes pointed to
 * by the given node.
 *
 * @returns nothing.
 * @param   pAstNode    The AST node to free.
 */
DECLHIDDEN(void) vdScriptAstNodeFree(PVDSCRIPTASTCORE pAstNode);

/**
 * Allocate a non variable in size AST node of the given class.
 *
 * @returns Pointer to the allocated node.
 *          NULL if out of memory.
 * @param   enmClass    The class of the AST node.
 */
DECLHIDDEN(PVDSCRIPTASTCORE) vdScriptAstNodeAlloc(VDSCRIPTASTCLASS enmClass);

/**
 * Allocate a IDE node which can hold the given number of characters.
 *
 * @returns Pointer to the allocated node.
 *          NULL if out of memory.
 * @param   cchIde      Number of characters which can be stored in the node.
 */
DECLHIDDEN(PVDSCRIPTASTIDE) vdScriptAstNodeIdeAlloc(unsigned cchIde);

#endif /* _VDScriptAst_h__ */
