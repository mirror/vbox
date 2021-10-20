/* $Id$ */
/** @file
 * PGM - Page Manager, Guest Context Mappings.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/vmm/pgm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/log.h>
#include <iprt/errcore.h>


/**
 * Gets the size of the current guest mappings if they were to be
 * put next to one another.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 * @param   pcb     Where to store the size.
 */
VMMR3DECL(int) PGMR3MappingsSize(PVM pVM, uint32_t *pcb)
{
    RT_NOREF(pVM);
    *pcb = 0;
    Log(("PGMR3MappingsSize: returns zero\n"));
    return VINF_SUCCESS;
}


/**
 * Fixates the guest context mappings in a range reserved from the Guest OS.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   GCPtrBase   The address of the reserved range of guest memory.
 * @param   cb          The size of the range starting at GCPtrBase.
 */
VMMR3DECL(int) PGMR3MappingsFix(PVM pVM, RTGCPTR GCPtrBase, uint32_t cb)
{
    Log(("PGMR3MappingsFix: GCPtrBase=%RGv cb=%#x\n", GCPtrBase, cb));
    RT_NOREF(pVM, GCPtrBase, cb);
    return VINF_SUCCESS;
}


/**
 * Unfixes the mappings.
 *
 * Unless PGMR3MappingsDisable is in effect, mapping conflict detection will be
 * enabled after this call.  If the mappings are fixed, a full CR3 resync will
 * take place afterwards.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) PGMR3MappingsUnfix(PVM pVM)
{
    Log(("PGMR3MappingsUnfix:\n"));
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}

