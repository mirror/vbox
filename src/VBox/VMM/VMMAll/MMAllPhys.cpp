/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Physical Memory.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_MM_PHYS
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/rem.h>
#include "MMInternal.h"
#include <VBox/vm.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>


/**
 * Convert GC physical address to HC virtual address.
 *
 * @returns HC virtual address.
 * @param   pVM         VM Handle
 * @param   GCPhys      Guest context physical address.
 * @param   cbRange     Physical range
 * @deprecated
 */
MMDECL(void *) MMPhysGCPhys2HCVirt(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange)
{
    void *pv;
    int rc = PGMPhysGCPhys2HCPtr(pVM, GCPhys, cbRange, &pv);
    if (VBOX_SUCCESS(rc))
        return pv;
    AssertMsgFailed(("Invalid address GCPhys=%x\n", GCPhys));
    return NULL;
}

