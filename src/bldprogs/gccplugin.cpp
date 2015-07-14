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


/** For use with messages. */
#define MY_LOC(a_hPreferred, a_pState) \
    (DECL_P(a_hPreferred) ? DECL_SOURCE_LOCATION(a_hPreferred) : gimple_location((a_pState)->hStmt))


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Checker state. */
typedef struct MYCHECKSTATE
{
    gimple      hStmt;
    long        iFmt;
    long        iArgs;
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
    tree hScope = CP_DECL_CONTEXT(hDecl);
    if (hScope == global_namespace)
        return;
    if (TREE_CODE(hScope) == RECORD_TYPE)
        hScope = TYPE_NAME(hScope);

    /* recurse */
    dprintScope(hScope);

    /* name the scope. */
    dprintf("::%s", DECL_NAME(hScope) ? IDENTIFIER_POINTER(DECL_NAME(hScope)) : "<noname>");
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


static void MyCheckFormatWorker(PMYCHECKSTATE pState, tree hFmtArg)
{

}


/**
 * Deal recursively with special format string constructs.
 *
 * This will call MyCheckFormatWorker to validate each format string.
 *
 * @param   pState              The format string check state.
 * @param   hFmtArg             The format string node.
 */
static void MyCheckFormatRecursive(PMYCHECKSTATE pState, tree hFmtArg)
{
    if (integer_zerop(hFmtArg))
        warning_at(MY_LOC(hFmtArg, pState), 0, "Format string should not be NULL");
    else
    {
        /** @todo more to do here. ternary expression, ++. */

        MyCheckFormatWorker(pState, hFmtArg);
    }
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
                    State.iFmt  = TREE_INT_CST(TREE_VALUE(hAttrArgs)).to_shwi();
                    State.iArgs = TREE_INT_CST(TREE_VALUE(TREE_CHAIN(hAttrArgs))).to_shwi();
                    State.hStmt = hStmt;
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

