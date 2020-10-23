/* $Id$ */
/** @file
 * DBGF - Debugger Facility, R0 part.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include "DBGFInternal.h"
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/vmm/vmm.h>

#include <VBox/log.h>
#include <VBox/sup.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include "dtrace/VBoxVMM.h"


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Initializes the per-VM data for the DBGF.
 *
 * This is called from under the GVMM lock, so it only need to initialize the
 * data so DBGFR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void) DBGFR0InitPerVMData(PGVM pGVM)
{
    pGVM->dbgfr0.s.pTracerR0 = NULL;
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void) DBGFR0CleanupVM(PGVM pGVM)
{
#ifdef VBOX_WITH_DBGF_TRACING
    if (pGVM->dbgfr0.s.pTracerR0)
        dbgfR0TracerDestroy(pGVM, pGVM->dbgfr0.s.pTracerR0);
    pGVM->dbgfr0.s.pTracerR0 = NULL;
#endif
}

