/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Ring-0.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/pgm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>



/**
 * Worker function for PGMR3PhysAllocateHandyPages and pgmPhysEnsureHandyPage.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is set in this case.
 *
 * @param   pVM         The VM handle.
 *
 * @remarks Must be called from within the PGM critical section.
 */
PGMR0DECL(int) PGMR0PhysAllocateHandyPages(PVM pVM)
{

    return VERR_NOT_IMPLEMENTED;
}


