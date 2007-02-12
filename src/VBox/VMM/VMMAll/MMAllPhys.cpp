/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Physical Memory.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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


/**
 * Convert GC virtual address to HC virtual address.
 *
 * This uses the current PD of the guest.
 *
 * @returns HC virtual address.
 * @param   pVM         VM Handle
 * @param   GCPtr       Guest context virtual address.
 * @deprecated
 */
MMDECL(void *) MMPhysGCVirt2HCVirt(PVM pVM, RTGCPTR GCPtr)
{
    void *pv;
    int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtr, &pv);
    if (VBOX_SUCCESS(rc))
        return pv;
    AssertMsgFailed(("%VGv, %Vrc\n", GCPtr, rc));
    return NULL;
}
