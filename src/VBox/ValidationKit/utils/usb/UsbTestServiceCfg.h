/* $Id$ */
/** @file
 * UsbTestServ - Remote USB test configuration and execution server, Config file API.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___UsbTestServiceCfg_h___
#define ___UsbTestServiceCfg_h___

#include <iprt/err.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/**
 * Config AST node types.
 */
typedef enum CFGASTNODETYPE
{
    /** Invalid. */
    CFGASTNODETYPE_INVALID = 0,
    /** Key/Value pair. */
    CFGASTNODETYPE_KEYVALUE,
    /** Compound type. */
    CFGASTNODETYPE_COMPOUND,
    /** List type. */
    CFGASTNODETYPE_LIST,
    /** 32bit hack. */
    CFGASTNODETYPE_32BIT_HACK = 0x7fffffff
} CFGASTNODETYPE;
/** Pointer to a config AST node type. */
typedef CFGASTNODETYPE *PCFGASTNODETYPE;
/** Pointer to a const config AST node type. */
typedef const CFGASTNODETYPE *PCCFGASTNODETYPE;

/**
 * Config AST.
 */
typedef struct CFGAST
{
    /** AST node type. */
    CFGASTNODETYPE        enmType;
    /** Key or scope id. */
    char                 *pszKey;
    /** Type dependent data. */
    union
    {
        /** Key value pair. */
        struct
        {
            /** Number of characters in the value - excluding terminator. */
            size_t        cchValue;
            /** Value string - variable in size. */
            char          aszValue[1];
        } KeyValue;
        /** Compound type. */
        struct
        {
            /** Number of AST node entries in the array. */
            unsigned       cAstNodes;
            /** AST node array - variable in size. */
            struct CFGAST *apAstNodes[1];
        } Compound;
        /** List type. */
        struct
        {
            /** Number of entries in the list. */
            unsigned       cListEntries;
            /** Array of list entries - variable in size. */
            char          *apszEntries[1];
        } List;
    } u;
} CFGAST, *PCFGAST;

/**
 * Parse the given configuration file and return the interesting config parameters.
 *
 * @returns VBox status code.
 * @param   pszFilename    The config file to parse.
 * @param   ppCfgAst       Where to store the pointer to the root AST node on success.
 * @param   ppErrInfo      Where to store the pointer to the detailed error message on failure -
 *                         optional.
 */
DECLHIDDEN(int) utsParseConfig(const char *pszFilename, PCFGAST *ppCfgAst, PRTERRINFO *ppErrInfo);

/**
 * Destroys the config AST and frees all resources.
 *
 * @returns nothing.
 * @param   pCfgAst        The config AST.
 */
DECLHIDDEN(void) utsConfigAstDestroy(PCFGAST pCfgAst);

/**
 * Return the config AST node with the given name or NULL if it doesn't exist.
 *
 * @returns Matching config AST node for the given name or NULL if not found.
 * @param   pCfgAst    The config ASt to search.
 * @param   pszName    The name to search for.
 */
DECLHIDDEN(PCFGAST) utsConfigAstGetByName(PCFGAST pCfgAst, const char *pszName);

RT_C_DECLS_END

#endif

