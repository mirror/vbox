/* $Id$ */
/** @file
 * gccplugin - GCC plugin for checking IPRT format strings.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <iprt/cdefs.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>

#include "plugin.h"
#include "basic-block.h"
#include "gimple.h"
#include "tree.h"
#include "tree-pass.h"
#include "cp/cp-tree.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** License indicator. */
int plugin_is_GPL_compatible;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define GCCPLUGIN_DEBUG
/** Debug printf. */
#ifdef GCCPLUGIN_DEBUG
# define dprintf(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
# define dprintf(...) do { } while (0)
#endif

/** Convencience macro not present in earlier gcc versions. */
#ifndef VAR_P
# define VAR_P(a_hNode) (TREE_CODE(a_hNode) == VAR_DECL)
#endif


/** For use with messages.
 * @todo needs some more work... Actually, seems we're a bit handicapped by
 *       working on gimplified stuff. */
#define MY_LOC(a_hPreferred, a_pState) EXPR_LOC_OR_LOC(a_hPreferred, (a_pState)->hFmtLoc)

#define MY_ISDIGIT(c) ((c) >= '0' && (c) <= '9')


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Checker state. */
typedef struct MYCHECKSTATE
{
    gimple      hStmt;
    long        iFmt;
    long        iArgs;
    location_t  hFmtLoc;
    const char *pszFmt;
} MYCHECKSTATE;
/** Pointer to my checker state. */
typedef MYCHECKSTATE *PMYCHECKSTATE;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static bool             MyPassGateCallback(void);
static unsigned int     MyPassExecuteCallback(void);
static tree             AttributeHandler(tree *, tree, tree, int, bool *);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Plug-in info. */
static const struct plugin_info g_PlugInInfo =
{
    .version = "0.0.0-ALPHA",
    .help    = "Implements the __iprt_format__ attribute for checking format strings and arguments."
};

/** My pass. */
static struct gimple_opt_pass g_MyPass =
{
    .pass =
    {
        .type                   = GIMPLE_PASS,
        .name                   = "*iprt-format-checks", /* asterisk = no dump */
        .optinfo_flags          = 0,
        .gate                   = MyPassGateCallback,
        .execute                = MyPassExecuteCallback,
        .sub                    = NULL,
        .next                   = NULL,
        .static_pass_number     = 0,
        .tv_id                  = TV_NONE,
        .properties_required    = 0,
        .properties_provided    = 0,
        .properties_destroyed   = 0,
        .todo_flags_start       = 0,
        .todo_flags_finish      = 0,
    }
};

/** The registration info for my pass. */
static const struct register_pass_info  g_MyPassInfo =
{
    .pass                       = &g_MyPass.pass,
    .reference_pass_name        = "ssa",
    .ref_pass_instance_number   = 1,
    .pos_op                     = PASS_POS_INSERT_BEFORE,
};


/** Attribute specification. */
static const struct attribute_spec g_AttribSpecs[] =
{
    {
        .name                   = "iprt_format",
        .min_length             = 2,
        .max_length             = 2,
        .decl_required          = false,
        .type_required          = true,
        .function_type_required = true,
        .handler                = AttributeHandler,
        .affects_type_identity  = false
    },
    {   NULL, 0, 0, false, false, false, NULL, false }
};


#ifdef GCCPLUGIN_DEBUG

/**
 * Debug function for printing the scope of a decl.
 * @param   hDecl               Declaration to print scope for.
 */
static void dprintScope(tree hDecl)
{
# if 0 /* later? */
    tree hScope = CP_DECL_CONTEXT(hDecl);
    if (hScope == global_namespace)
        return;
    if (TREE_CODE(hScope) == RECORD_TYPE)
        hScope = TYPE_NAME(hScope);

    /* recurse */
    dprintScope(hScope);

    /* name the scope. */
    dprintf("::%s", DECL_NAME(hScope) ? IDENTIFIER_POINTER(DECL_NAME(hScope)) : "<noname>");
# endif
}


/**
 * Debug function for printing a declaration.
 * @param   hDecl               The declaration to print.
 */
static void dprintDecl(tree hDecl)
{
    enum tree_code const    enmDeclCode = TREE_CODE(hDecl);
    tree const              hType       = TREE_TYPE(hDecl);
    enum tree_code const    enmTypeCode = hType ? TREE_CODE(hType) : (enum tree_code)-1;
#if 0
    if (   enmTypeCode == RECORD_TYPE
        && enmDeclCode == TYPE_DECL
        && DECL_ARTIFICIAL(hDecl))
        dprint_class(hType);
#endif

    dprintf("%s ", tree_code_name[enmDeclCode]);
    dprintScope(hDecl);
    dprintf("::%s", DECL_NAME(hDecl) ? IDENTIFIER_POINTER(DECL_NAME(hDecl)) : "<noname>");
    if (hType)
        dprintf(" type %s", tree_code_name[enmTypeCode]);
    dprintf(" @%s:%d", DECL_SOURCE_FILE(hDecl),  DECL_SOURCE_LINE(hDecl));
}

#endif /* GCCPLUGIN_DEBUG */


/**
 * Gate callback for my pass that indicates whether it should execute or not.
 * @returns true to execute.
 */
static bool             MyPassGateCallback(void)
{
    dprintf("MyPassGateCallback:\n");
    return true;
}


#define MYSTATE_FMT_FILE(a_pState)      LOCATION_FILE((a_pState)->hFmtLoc)
#define MYSTATE_FMT_LINE(a_pState)      LOCATION_LINE((a_pState)->hFmtLoc)
#define MYSTATE_FMT_COLUMN(a_pState)    LOCATION_COLUMN((a_pState)->hFmtLoc)

void MyCheckWarnFmt(PMYCHECKSTATE pState, const char *pszLoc, const char *pszFormat, ...)
{
    char szTmp[1024];
    va_list va;
    va_start(va, pszFormat);
    vsnprintf(szTmp, sizeof(szTmp), pszFormat, va);
    va_end(va);


    /* Create a location for pszLoc. */
    location_t hLoc      = pState->hFmtLoc;
#if __GNUC__ >= 5 /** @todo figure this... */
    intptr_t   offString = pszLoc - pState->pszFmt;
    if (   offString > 0
        && !linemap_location_from_macro_expansion_p(line_table, hLoc))
    {
        unsigned            uCol    = 1 + offString;
        expanded_location   XLoc    = expand_location_to_spelling_point(hLoc);
        int                 cchLine;
# if __GNUC__ >= 5 /** @todo figure this... */
        const char         *pszLine = location_get_source_line(XLoc, &cchLine);
# else
        const char         *pszLine = location_get_source_line(XLoc);
        int                 cchLine = 0;
        if (pszLine)
        {
            const char *pszEol = strpbrk(pszLine, "\n\r");
            if (!pszEol)
                pszEol = strchr(pszLine, '\0');
            cchLine = (int)(pszEol - pszLine);
        }
# endif
        if (pszLine)
        {
            /** @todo Adjust the position by parsing the source. */
            pszLine += XLoc.column - 1;
            cchLine -= XLoc.column - 1;
        }
        location_t hNewLoc = linemap_position_for_loc_and_offset(line_table, hLoc, 0);
        if (hNewLoc)
            hLoc = hNewLoc;
    }
#endif

    /* display the warning. */
    warning_at(hLoc, 0, "%s", szTmp);
}



/**
 * Checks that @a iFmtArg isn't present or a valid final dummy argument.
 *
 * Will issue warning/error if there are more arguments at @a iFmtArg.
 *
 * @param   pState              The format string checking state.
 * @param   iArg                The index of the end of arguments, this is
 *                              relative to MYCHECKSTATE::iArgs.
 */
void MyCheckFinalArg(PMYCHECKSTATE pState, unsigned iArg)
{
    dprintf("MyCheckFinalArg: iArg=%u iArgs=%ld cArgs=%u\n", iArg, pState->iArgs, gimple_call_num_args(pState->hStmt));
    if (pState->iArgs > 0)
    {
        iArg += pState->iArgs - 1;
        unsigned cArgs = gimple_call_num_args(pState->hStmt);
        if (iArg == cArgs)
        { /* fine */ }
        else if (iArg < cArgs)
        {
            tree hArg = gimple_call_arg(pState->hStmt, iArg);
            if (cArgs - iArg > 1)
                warning_at(MY_LOC(hArg, pState), 0, "%u extra arguments not consumed by format string", cArgs - iArg);
            else if (   TREE_CODE(hArg) != INTEGER_CST
                     || !TREE_INT_CST(hArg).fits_shwi()
                     || TREE_INT_CST(hArg).to_shwi() != -99) /* ignore final dummy argument: ..., -99); */
                warning_at(MY_LOC(hArg, pState), 0, "one extra argument not consumed by format string");
        }
        /* This should be handled elsewhere, but just in case. */
        else if (iArg - 1 == cArgs)
            warning_at(pState->hFmtLoc, 0, "one argument too few");
        else
            warning_at(pState->hFmtLoc, 0, "%u arguments too few", iArg - cArgs);
    }
}

void MyCheckIntArg(PMYCHECKSTATE pState, const char *pszFmt, unsigned iArg, const char *pszMessage)
{

}



/**
 * Does the actual format string checking.
 *
 * @todo    Move this to different file common to both GCC and CLANG later.
 *
 * @param   pState              The format string checking state.
 * @param   pszFmt              The format string.
 */
void MyCheckFormatCString(PMYCHECKSTATE pState, const char *pszFmt)
{
    dprintf("checker2: \"%s\" at %s:%d col %d\n", pszFmt,
            MYSTATE_FMT_FILE(pState), MYSTATE_FMT_LINE(pState), MYSTATE_FMT_COLUMN(pState));
    pState->pszFmt = pszFmt;

    unsigned iArg = 0;
    for (;;)
    {
        /*
         * Skip to the next argument.
         * Quits the loop with the first char following the '%' in 'ch'.
         */
        char ch;
        for (;;)
        {
            ch = *pszFmt++;
            if (ch == '%')
            {
                ch = *pszFmt++;
                if (ch != '%')
                    break;
            }
            else if (ch == '\0')
            {
                MyCheckFinalArg(pState, iArg);
                return;
            }
        }

        /*
         * Flags
         */
        uint32_t fFlags = 0;
        for (;;)
        {
            uint32_t fFlag;
            switch (ch)
            {
                case '#':   fFlag = RTSTR_F_SPECIAL;      break;
                case '-':   fFlag = RTSTR_F_LEFT;         break;
                case '+':   fFlag = RTSTR_F_PLUS;         break;
                case ' ':   fFlag = RTSTR_F_BLANK;        break;
                case '0':   fFlag = RTSTR_F_ZEROPAD;      break;
                case '\'':  fFlag = RTSTR_F_THOUSAND_SEP; break;
                default:    fFlag = 0;                    break;
            }
            if (!fFlag)
                break;
            if (fFlags & fFlag)
                MyCheckWarnFmt(pState, pszFmt - 1, "duplicate flag '%c'", ch);
            fFlags |= fFlag;
            ch = *pszFmt++;
        }

        /*
         * Width.
         */
        int cchWidth = -1;
        if (MY_ISDIGIT(ch))
        {
            cchWidth = ch - '0';
            while (   (ch = *pszFmt++) != '\0'
                   && MY_ISDIGIT(ch))
            {
                cchWidth *= 10;
                cchWidth += ch - '0';
            }
            fFlags |= RTSTR_F_WIDTH;
        }
        else if (ch == '*')
        {
            MyCheckIntArg(pState, pszFmt - 1, iArg, "width should be an 'int' sized argument");
            iArg++;
            cchWidth = 0;
            fFlags |= RTSTR_F_WIDTH;
        }

        /*
         * Precision
         */
        int cchPrecision = -1;
        if (ch == '.')
        {
            ch = *pszFmt++;
            if (MY_ISDIGIT(ch))
            {
                cchPrecision = ch - '0';
                while (   (ch = *pszFmt++) != '\0'
                       && MY_ISDIGIT(ch))
                {
                    cchPrecision *= 10;
                    cchPrecision += ch - '0';
                }
            }
            else if (ch == '*')
            {
                MyCheckIntArg(pState, pszFmt - 1, iArg, "precision should be an 'int' sized argument");
                iArg++;
                cchWidth = 0;
            }
            else
                MyCheckWarnFmt(pState, pszFmt - 1, "Missing precision value, only got the '.'");
            if (cchPrecision < 0)
            {
                MyCheckWarnFmt(pState, pszFmt - 1, "Negative precision value: %d", cchPrecision);
                cchPrecision = 0;
            }
            fFlags |= RTSTR_F_PRECISION;
        }

        /*
         * Argument size.
         */
        char chArgSize = ch;
        switch (ch)
        {
            default:
                chArgSize = '\0';
                break;

            case 'z':
            case 'L':
            case 'j':
            case 't':
                ch = *pszFmt++;
                break;

            case 'l':
                ch = *pszFmt++;
                if (ch == 'l')
                {
                    chArgSize = 'L';
                    ch = *pszFmt++;
                }
                break;

            case 'h':
                ch = *pszFmt++;
                if (ch == 'h')
                {
                    chArgSize = 'H';
                    ch = *pszFmt++;
                }
                break;

            case 'I': /* Used by Win32/64 compilers. */
                if (   pszFmt[0] == '6'
                    && pszFmt[1] == '4')
                {
                    pszFmt += 2;
                    chArgSize = 'L';
                }
                else if (   pszFmt[0] == '3'
                         && pszFmt[1] == '2')
                {
                    pszFmt += 2;
                    chArgSize = 0;
                }
                else
                    chArgSize = 'j';
                ch = *pszFmt++;
                break;

            case 'q': /* Used on BSD platforms. */
                chArgSize = 'L';
                ch = *pszFmt++;
                break;
        }

        /*
         * The type.
         */

    }
}


/**
 * Non-recursive worker for MyCheckFormatRecursive.
 *
 * This will attempt to result @a hFmtArg into a string literal which it then
 * passes on to MyCheckFormatString for the actual analyzis.
 *
 * @param   pState              The format string checking state.
 * @param   hFmtArg             The format string node.
 */
DECL_NO_INLINE(static, void) MyCheckFormatNonRecursive(PMYCHECKSTATE pState, tree hFmtArg)
{
    dprintf("checker: hFmtArg=%p %s\n", hFmtArg, tree_code_name[TREE_CODE(hFmtArg)]);

    /*
     * Try resolve variables into constant strings.
     */
    if (VAR_P(hFmtArg))
    {
        hFmtArg = decl_constant_value(hFmtArg);
        STRIP_NOPS(hFmtArg); /* Used as argument and assigned call result. */
        dprintf("checker1: variable => hFmtArg=%p %s\n", hFmtArg, tree_code_name[TREE_CODE(hFmtArg)]);
    }

    /*
     * Fend off NULLs.
     */
    if (integer_zerop(hFmtArg))
        warning_at(MY_LOC(hFmtArg, pState), 0, "Format string should not be NULL");
    /*
     * Need address expression to get any further.
     */
    else if (TREE_CODE(hFmtArg) != ADDR_EXPR)
        dprintf("checker1: Not address expression (%s)\n", tree_code_name[TREE_CODE(hFmtArg)]);
    else
    {
        pState->hFmtLoc = EXPR_LOC_OR_LOC(hFmtArg, pState->hFmtLoc);
        hFmtArg = TREE_OPERAND(hFmtArg, 0);

        /*
         * Deal with fixed string indexing, if possible.
         */
        HOST_WIDE_INT off = 0;
        if (   TREE_CODE(hFmtArg) == ARRAY_REF
            && TREE_INT_CST(TREE_OPERAND(hFmtArg, 1)).fits_shwi() )
        {
            off = TREE_INT_CST(TREE_OPERAND(hFmtArg, 1)).to_shwi();
            if (off < 0)
            {
                dprintf("checker1: ARRAY_REF, off=%ld\n", off);
                return;
            }
            hFmtArg = TREE_OPERAND(hFmtArg, 0);
            dprintf("checker1: ARRAY_REF => hFmtArg=%p %s, off=%ld\n", hFmtArg, tree_code_name[TREE_CODE(hFmtArg)], off);
        }

        /*
         * Deal with static const char g_szFmt[] = "qwerty";  Take care as
         * the actual string constant may not necessarily include the terminator.
         */
        tree hArraySize = NULL_TREE;
        if (   VAR_P(hFmtArg)
            && TREE_CODE(TREE_TYPE(hFmtArg)) == ARRAY_TYPE)
        {
            tree hArrayInitializer = decl_constant_value(hFmtArg);
            if (   hArrayInitializer != hFmtArg
                && TREE_CODE(hArrayInitializer) == STRING_CST)
            {
                hArraySize = DECL_SIZE_UNIT(hFmtArg);
                hFmtArg = hArrayInitializer;
            }
        }

        /*
         * Are we dealing with a string literal now?
         */
        if (TREE_CODE(hFmtArg) != STRING_CST)
            dprintf("checker1: Not string literal (%s)\n", tree_code_name[TREE_CODE(hFmtArg)]);
        else if (TYPE_MAIN_VARIANT(TREE_TYPE(TREE_TYPE(hFmtArg))) != char_type_node)
            warning_at(pState->hFmtLoc, 0, "expected 'char' type string literal");
        else
        {
            /*
             * Yes we are, so get the pointer to the string and its length.
             */
            const char *pszFmt = TREE_STRING_POINTER(hFmtArg);
            int         cchFmt = TREE_STRING_LENGTH(hFmtArg);

            /* Adjust cchFmt to the initialized array size if appropriate. */
            if (hArraySize != NULL_TREE)
            {
                if (TREE_CODE(hArraySize) != INTEGER_CST)
                    warning_at(pState->hFmtLoc, 0, "Expected integer array size (not %s)", tree_code_name[TREE_CODE(hArraySize)]);
                else if (!TREE_INT_CST(hArraySize).fits_shwi())
                    warning_at(pState->hFmtLoc, 0, "Unexpected integer overflow in array size constant");
                else
                {
                    HOST_WIDE_INT cbArray = TREE_INT_CST(hArraySize).to_shwi();
                    if (   cbArray <= 0
                        || cbArray != (int)cbArray)
                        warning_at(pState->hFmtLoc, 0, "Unexpected integer array size constant value: %ld", cbArray);
                    else if (cchFmt > cbArray)
                    {
                        dprintf("checker1: cchFmt=%d => cchFmt=%ld (=cbArray)\n", cchFmt, cbArray);
                        cchFmt = (int)cbArray;
                    }
                }
            }

            /* Apply the offset, if given. */
            if (off)
            {
                if (off >= cchFmt)
                {
                    dprintf("checker1: off=%ld  >=  cchFmt=%d -> skipping\n", off, cchFmt);
                    return;
                }
                pszFmt += off;
                cchFmt -= (int)off;
            }

            /*
             * Check for unterminated strings.
             */
            if (   cchFmt < 1
                || pszFmt[cchFmt - 1] != '\0')
                warning_at(pState->hFmtLoc, 0, "Unterminated format string (cchFmt=%d)", cchFmt);
            /*
             * Call worker to check the actual string.
             */
            else
                MyCheckFormatCString(pState, pszFmt);
        }
    }
}


/**
 * Deal recursively with special format string constructs.
 *
 * This will call MyCheckFormatNonRecursive to validate each format string.
 *
 * @param   pState              The format string checking state.
 * @param   hFmtArg             The format string node.
 */
static void MyCheckFormatRecursive(PMYCHECKSTATE pState, tree hFmtArg)
{
    /*
     * NULL format strings may cause crashes.
     */
    if (integer_zerop(hFmtArg))
        warning_at(MY_LOC(hFmtArg, pState), 0, "Format string should not be NULL");
    /*
     * Check both branches of a ternary operator.
     */
    else if (TREE_CODE(hFmtArg) == COND_EXPR)
    {
        MyCheckFormatRecursive(pState, TREE_OPERAND(hFmtArg, 1));
        MyCheckFormatRecursive(pState, TREE_OPERAND(hFmtArg, 2));
    }
    /*
     * Strip coercion.
     */
    else if (   CONVERT_EXPR_P(hFmtArg)
             && TYPE_PRECISION(TREE_TYPE(hFmtArg)) == TYPE_PRECISION(TREE_TYPE(TREE_OPERAND(hFmtArg, 0))) )
        MyCheckFormatRecursive(pState, TREE_OPERAND(hFmtArg, 0));
    /*
     * We're good, hand it to the non-recursive worker.
     */
    else
        MyCheckFormatNonRecursive(pState, hFmtArg);
}


/**
 * Execute my pass.
 * @returns Flags indicates stuff todo, we return 0.
 */
static unsigned int     MyPassExecuteCallback(void)
{
    dprintf("MyPassExecuteCallback:\n");

    /*
     * Enumerate the basic blocks.
     */
    basic_block hBasicBlock;
    FOR_EACH_BB(hBasicBlock)
    {
        dprintf(" hBasicBlock=%p\n", hBasicBlock);

        /*
         * Enumerate the statements in the current basic block.
         * We're interested in calls to functions with the __iprt_format__ attribute.
         */
        for (gimple_stmt_iterator hStmtItr = gsi_start_bb(hBasicBlock); !gsi_end_p(hStmtItr); gsi_next(&hStmtItr))
        {
            gimple const            hStmt   = gsi_stmt(hStmtItr);
            enum gimple_code const  enmCode = gimple_code(hStmt);
#ifdef GCCPLUGIN_DEBUG
            unsigned const          cOps    = gimple_num_ops(hStmt);
            dprintf("   hStmt=%p %s (%d) ops=%d\n", hStmt, gimple_code_name[enmCode], enmCode, cOps);
            for (unsigned iOp = 0; iOp < cOps; iOp++)
            {
                tree const hOp = gimple_op(hStmt, iOp);
                if (hOp)
                    dprintf("     %02d: %p, code %s(%d)\n", iOp, hOp, tree_code_name[TREE_CODE(hOp)], TREE_CODE(hOp));
                else
                    dprintf("     %02d: NULL_TREE\n", iOp);
            }
#endif
            if (enmCode == GIMPLE_CALL)
            {
                /*
                 * Check if the function type has the __iprt_format__ attribute.
                 */
                tree const hFn       = gimple_call_fn(hStmt);
                tree const hFnType   = gimple_call_fntype(hStmt);
                tree const hAttr     = lookup_attribute("iprt_format", TYPE_ATTRIBUTES(hFnType));
#ifdef GCCPLUGIN_DEBUG
                tree const hFnDecl   = gimple_call_fndecl(hStmt);
                dprintf("     hFn    =%p %s(%d); args=%d\n",
                        hFn, tree_code_name[TREE_CODE(hFn)], TREE_CODE(hFn), gimple_call_num_args(hStmt));
                if (hFnDecl)
                    dprintf("     hFnDecl=%p %s(%d) type=%p %s:%d\n", hFnDecl, tree_code_name[TREE_CODE(hFnDecl)],
                            TREE_CODE(hFnDecl), TREE_TYPE(hFnDecl), DECL_SOURCE_FILE(hFnDecl), DECL_SOURCE_LINE(hFnDecl));
                if (hFnType)
                    dprintf("     hFnType=%p %s(%d)\n", hFnType, tree_code_name[TREE_CODE(hFnType)], TREE_CODE(hFnType));
#endif
                if (hAttr)
                {
                    /*
                     * Yeah, it has the attribute!
                     */
                    tree const hAttrArgs = TREE_VALUE(hAttr);
                    MYCHECKSTATE State;
                    State.iFmt    = TREE_INT_CST(TREE_VALUE(hAttrArgs)).to_shwi();
                    State.iArgs   = TREE_INT_CST(TREE_VALUE(TREE_CHAIN(hAttrArgs))).to_shwi();
                    State.hStmt   = hStmt;
                    State.hFmtLoc = gimple_location(hStmt);
                    State.pszFmt  = NULL;
                    dprintf("     %s() __iprt_format__(iFmt=%ld, iArgs=%ld)\n",
                            DECL_NAME(hFnDecl) ? IDENTIFIER_POINTER(DECL_NAME(hFnDecl)) : "<unamed>", State.iFmt, State.iArgs);

                    MyCheckFormatRecursive(&State, gimple_call_arg(hStmt, State.iFmt - 1));
                }
            }
        }
    }
    return 0;
}



/**
 * Validate the use of an attribute.
 *
 * @returns ??
 * @param   phOnNode        The node the attribute is being used on.
 * @param   hAttrName       The attribute name.
 * @param   hAttrArgs       The attribute arguments.
 * @param   fFlags          Some kind of flags...
 * @param   pfDontAddAttrib Whether to add the attribute to this node or not.
 */
static tree AttributeHandler(tree *phOnNode, tree hAttrName, tree hAttrArgs, int fFlags, bool *pfDontAddAttrib)
{
    dprintf("AttributeHandler: name=%s fFlags=%#x", IDENTIFIER_POINTER(hAttrName), fFlags);
    long iFmt  = TREE_INT_CST(TREE_VALUE(hAttrArgs)).to_shwi();
    long iArgs = TREE_INT_CST(TREE_VALUE(TREE_CHAIN(hAttrArgs))).to_shwi();
    dprintf(" iFmt=%ld iArgs=%ld", iFmt, iArgs);

    tree hType = *phOnNode;
    dprintf(" hType=%p %s(%d)\n", hType, tree_code_name[TREE_CODE(hType)], TREE_CODE(hType));

    if (pfDontAddAttrib)
        *pfDontAddAttrib = false;
    return NULL_TREE;
}


/**
 * Called when we can register attributes.
 *
 * @param   pvEventData         Ignored.
 * @param   pvUser              Ignored.
 */
static void RegisterAttributesEvent(void *pvEventData, void *pvUser)
{
    NOREF(pvEventData); NOREF(pvUser);
    dprintf("RegisterAttributesEvent: pvEventData=%p\n", pvEventData);

    register_attribute(&g_AttribSpecs[0]);
}


/**
 * The plug-in entry point.
 *
 * @returns 0 to indicate success?
 * @param   pPlugInInfo         Plugin info structure.
 * @param   pGccVer             GCC Version.
 */
int plugin_init(plugin_name_args *pPlugInInfo, plugin_gcc_version *pGccVer)
{
    dprintf("plugin_init: %s\n", pPlugInInfo->full_name);
    dprintf("gcc version: basever=%s datestamp=%s devphase=%s revision=%s\n",
            pGccVer->basever, pGccVer->datestamp, pGccVer->devphase, pGccVer->revision);

    /* Ask for callback in which we may register the attribute. */
    register_callback(pPlugInInfo->base_name, PLUGIN_ATTRIBUTES, RegisterAttributesEvent, NULL /*pvUser*/);

    /* Register our pass. */
    register_callback(pPlugInInfo->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, (void *)&g_MyPassInfo);

    /* Register plug-in info. */
    register_callback(pPlugInInfo->base_name, PLUGIN_INFO, NULL, (void *)&g_PlugInInfo);

    return 0;
}

