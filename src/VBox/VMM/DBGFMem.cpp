/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Memory Methods.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>



/**
 * Scan guest memory for an exact byte string.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   cbRange     The number of bytes to scan.
 * @param   pabNeedle   What to search for - exact search.
 * @param   cbNeedle    Size of the search byte string.
 * @param   pHitAddress Where to put the address of the first hit.
 */
static DECLCALLBACK(int) dbgfR3MemScan(PVM pVM, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle,
                                       PDBGFADDRESS pHitAddress)
{
    /*
     * Validate the input we use, PGM does the rest.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_POINTER;
    if (!VALID_PTR(pHitAddress))
        return VERR_INVALID_POINTER;
    if (DBGFADDRESS_IS_HMA(pAddress))
        return VERR_INVALID_POINTER;

    /*
     * Select DBGF worker by addressing mode.
     */
    int rc;
    PGMMODE enmMode = PGMGetGuestMode(pVM);
    if (    enmMode == PGMMODE_REAL
        ||  enmMode == PGMMODE_PROTECTED
        ||  DBGFADDRESS_IS_PHYS(pAddress)
        )
    {
        RTGCPHYS PhysHit;
        rc = PGMR3DbgScanPhysical(pVM, pAddress->FlatPtr, cbRange, pabNeedle, cbNeedle, &PhysHit);
        if (RT_SUCCESS(rc))
            DBGFR3AddrFromPhys(pVM, pHitAddress, PhysHit);
    }
    else
    {
        RTGCUINTPTR GCPtrHit;
        rc = PGMR3DbgScanVirtual(pVM, pAddress->FlatPtr, cbRange, pabNeedle, cbNeedle, &GCPtrHit);
        if (RT_SUCCESS(rc))
            DBGFR3AddrFromFlat(pVM, pHitAddress, GCPtrHit);
    }

    return rc;
}


/**
 * Scan guest memory for an exact byte string.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS and *pGCPtrHit on success.
 * @retval  VERR_DBGF_MEM_NOT_FOUND if not found.
 * @retval  VERR_INVALID_POINTER if any of the pointer arguments are invalid.
 * @retval  VERR_INVALID_ARGUMENT if any other arguments are invalid.
 *
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   cbRange     The number of bytes to scan.
 * @param   pabNeedle   What to search for - exact search.
 * @param   cbNeedle    Size of the search byte string.
 * @param   pHitAddress Where to put the address of the first hit.
 *
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3MemScan(PVM pVM, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress)
{
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3MemScan, 6,
                         pVM, pAddress, cbRange, pabNeedle, cbNeedle, pHitAddress);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


