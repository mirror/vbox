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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** License indicator. */
int plugin_is_GPL_compatible;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Debug printf. */
#if 1
# define dprintf(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
# define dprintf(...) do { } while (0)
#endif


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
        .name                   = "iprt-format-checks",
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
    .pos_op                     = PASS_POS_INSERT_AFTER,
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


/**
 * Gate callback for my pass that indicates whether it should execute or not.
 * @returns true to execute.
 */
static bool             MyPassGateCallback(void)
{
    dprintf("MyPassGateCallback:\n");
    return true;
}


/**
 * Execute my pass.
 * @returns Flags indicates stuff todo, we return 0.
 */
static unsigned int     MyPassExecuteCallback(void)
{
    dprintf("MyPassExecuteCallback:\n");

    basic_block hBasicBlock;
    FOR_EACH_BB(hBasicBlock)
    {
        dprintf(" hBasicBlock=%p\n", hBasicBlock);

        for (gimple_stmt_iterator hStmtItr = gsi_start_bb(hBasicBlock); gsi_end_p(hStmtItr); gsi_next(&hStmtItr))
        {
            gimple hStmt = gsi_stmt(hStmtItr);
            dprintf(" hStmt=%p ops=%d\n", hStmt, gimple_num_ops(hStmt));

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
    dprintf("AttributeHandler: name=%s fFlags=%#x ", IDENTIFIER_POINTER(hAttrName), fFlags);
    long iFmt  = TREE_INT_CST(TREE_VALUE(hAttrArgs)).to_shwi();
    long iArgs = TREE_INT_CST(TREE_VALUE(TREE_CHAIN(hAttrArgs))).to_shwi();
    dprintf("iFmt=%ld iArgs=%ld\n", iFmt, iArgs);

    tree hType = *phOnNode;

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

