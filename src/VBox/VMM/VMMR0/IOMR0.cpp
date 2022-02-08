/* $Id$ */
/** @file
 * IOM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/vmm/iom.h>
#include <VBox/vmm/pgm.h>
#include "IOMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/assertcompile.h>
#include <iprt/errcore.h>



/**
 * Initializes the per-VM data for the IOM.
 *
 * This is called from under the GVMM lock, so it should only initialize the
 * data so IOMR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void) IOMR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->iom.s) <= sizeof(pGVM->iom.padding));
    AssertCompile(sizeof(pGVM->iomr0.s) <= sizeof(pGVM->iomr0.padding));

    iomR0IoPortInitPerVMData(pGVM);
    iomR0MmioInitPerVMData(pGVM);
}


/**
 * Called during ring-0 init (vmmR0InitVM).
 *
 * @returns VBox status code.
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(int) IOMR0InitVM(PGVM pGVM)
{
    int rc = PGMR0HandlerPhysicalTypeSetUpContext(pGVM, PGMPHYSHANDLERKIND_MMIO, 0 /*fFlags*/,
                                                  iomMmioHandlerNew, iomMmioPfHandlerNew,
                                                  "MMIO", pGVM->iom.s.hNewMmioHandlerType);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void) IOMR0CleanupVM(PGVM pGVM)
{
    iomR0IoPortCleanupVM(pGVM);
    iomR0MmioCleanupVM(pGVM);
}

