/* $Id$ */
/** @file
 * VM - The Virtual Machine Monitor, Ring-3 API VTable Definitions.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#define RT_RELAXED_CALLBACKS_TYPES
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm/vmmr3vtable.h>

#include <iprt/asm.h>
#include <iprt/errcore.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) vmmR3ReservedVTableEntry(void);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const VMMR3VTABLE g_VMMR3VTable =
{
    /* .uMagicVersion = */      VMMR3VTABLE_MAGIC_VERSION,
    /* .fFlags = */             0,
    /* .pszDescription = */     "x86 & amd64",

#define VTABLE_ENTRY(a_Api)     a_Api,
#define VTABLE_RESERVED(a_Name) vmmR3ReservedVTableEntry,

#include <VBox/vmm/vmmr3vtable-def.h>

#undef VTABLE_ENTRY
#undef VTABLE_RESERVED

    /* .uMagicVersionEnd = */   VMMR3VTABLE_MAGIC_VERSION,
};


/**
 * Reserved VMM function table entry.
 */
static DECLCALLBACK(int) vmmR3ReservedVTableEntry(void)
{
    void * volatile pvCaller = ASMReturnAddress();
    AssertLogRel(("Reserved VMM function table entry called from %p!\n", pvCaller ));
    return VERR_INTERNAL_ERROR;
}


VMMR3DECL(PCVMMR3VTABLE) VMMR3GetVTable(void)
{
    return &g_VMMR3VTable;
}

