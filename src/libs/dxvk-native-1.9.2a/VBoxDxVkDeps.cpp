/* $Id$ */
/** @file
 * VBoxDxVk - For dragging in library objects.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
 *
 * Oracle Corporation confidential
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/types.h>

#include <d3d11.h>

/** Just a dummy global structure containing a bunch of
 * function pointers to code which is wanted in the link.
 */
struct CLANG11WEIRDNESS { PFNRT pfn; } g_apfnVBoxDxVkDeps[] =
{
    { (PFNRT)D3D11CreateDevice },
    { NULL },
};

