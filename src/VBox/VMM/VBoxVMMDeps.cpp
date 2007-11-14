/* $Id$ */
/** @file
 * VBoxVMM link dependencies - drag all we want into the link!
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/vm.h>
#include <VBox/em.h>
#include <VBox/iom.h>
#include <VBox/dbgf.h>
#include <VBox/dbg.h>

VMMR3DECL(int) VMMDoTest(PVM pVM);

/** Just a dummy global structure containing a bunch of
 * function pointers to code which is wanted in the link.
 */
PFNRT g_apfnDeps[] =
{
    (PFNRT)DBGFR3DisasInstrEx,
    (PFNRT)DBGFR3LogModifyFlags,
    (PFNRT)DBGFR3StackWalkEnd,
    (PFNRT)DBGFR3SymbolByAddr,
    (PFNRT)DBGFR3MemScan,
    (PFNRT)EMInterpretInstruction,
    (PFNRT)IOMIOPortRead,
    (PFNRT)PDMQueueInsert,
    (PFNRT)PDMCritSectEnter,
    (PFNRT)PGMInvalidatePage,
    (PFNRT)PGMR3DbgHCPtr2GCPhys,
    (PFNRT)VMR3Create,
    (PFNRT)VMMDoTest,
#ifdef VBOX_WITH_DEBUGGER
    (PFNRT)DBGCCreate,
#endif
    NULL
};
