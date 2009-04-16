/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Mixed Address Methods.
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/mm.h>
#include <VBox/err.h>
#include <VBox/log.h>



/**
 * Checks if an address is in the HMA or not.
 * @returns true if it's inside the HMA.
 * @returns flase if it's not inside the HMA.
 * @param   pVM         The VM handle.
 * @param   FlatPtr     The address in question.
 */
DECLINLINE(bool) dbgfR3IsHMA(PVM pVM, RTGCUINTPTR FlatPtr)
{
    return MMHyperIsInsideArea(pVM, FlatPtr);
}


/**
 * Creates a mixed address from a Sel:off pair.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   Sel         The selector part.
 * @param   off         The offset part.
 */
VMMR3DECL(int) DBGFR3AddrFromSelOff(PVM pVM, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off)
{
    pAddress->Sel = Sel;
    pAddress->off = off;
    if (Sel != DBGF_SEL_FLAT)
    {
        /* @todo SMP support!! */
        PVMCPU pVCpu = &pVM->aCpus[0];

        SELMSELINFO SelInfo;
        int rc = SELMR3GetSelectorInfo(pVM, pVCpu, Sel, &SelInfo);
        if (RT_FAILURE(rc))
            return rc;

        /* check limit. */
        if (SELMSelInfoIsExpandDown(&SelInfo))
        {
            if (    !SelInfo.Raw.Gen.u1Granularity
                &&  off > UINT32_C(0xffff))
                return VERR_OUT_OF_SELECTOR_BOUNDS;
            if (off <= SelInfo.cbLimit)
                return VERR_OUT_OF_SELECTOR_BOUNDS;
        }
        else if (off > SelInfo.cbLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        pAddress->FlatPtr = SelInfo.GCPtrBase + off;
        /** @todo fix this flat selector test! */
        if (    !SelInfo.GCPtrBase
            &&  SelInfo.Raw.Gen.u1Granularity
            &&  SelInfo.Raw.Gen.u1DefBig)
            pAddress->fFlags = DBGFADDRESS_FLAGS_FLAT;
        else if (SelInfo.cbLimit <= UINT32_C(0xffff))
            pAddress->fFlags = DBGFADDRESS_FLAGS_FAR16;
        else if (SelInfo.cbLimit <= UINT32_C(0xffffffff))
            pAddress->fFlags = DBGFADDRESS_FLAGS_FAR32;
        else
            pAddress->fFlags = DBGFADDRESS_FLAGS_FAR64;
    }
    else
    {
        pAddress->FlatPtr = off;
        pAddress->fFlags = DBGFADDRESS_FLAGS_FLAT;
    }
    pAddress->fFlags |= DBGFADDRESS_FLAGS_VALID;
    if (dbgfR3IsHMA(pVM, pAddress->FlatPtr))
        pAddress->fFlags |= DBGFADDRESS_FLAGS_HMA;

    return VINF_SUCCESS;
}


/**
 * Creates a mixed address from a flat address.
 *
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   FlatPtr     The flat pointer.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromFlat(PVM pVM, PDBGFADDRESS pAddress, RTGCUINTPTR FlatPtr)
{
    pAddress->Sel     = DBGF_SEL_FLAT;
    pAddress->off     = FlatPtr;
    pAddress->FlatPtr = FlatPtr;
    pAddress->fFlags  = DBGFADDRESS_FLAGS_FLAT | DBGFADDRESS_FLAGS_VALID;
    if (dbgfR3IsHMA(pVM, pAddress->FlatPtr))
        pAddress->fFlags |= DBGFADDRESS_FLAGS_HMA;
    return pAddress;
}


/**
 * Creates a mixed address from a guest physical address.
 *
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   PhysAddr    The guest physical address.
 */
VMMR3DECL(void) DBGFR3AddrFromPhys(PVM pVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr)
{
    pAddress->Sel     = DBGF_SEL_FLAT;
    pAddress->off     = PhysAddr;
    pAddress->FlatPtr = PhysAddr;
    pAddress->fFlags  = DBGFADDRESS_FLAGS_PHYS | DBGFADDRESS_FLAGS_VALID;
}


/**
 * Checks if the specified address is valid (checks the structure pointer too).
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM         The VM handle.
 * @param   pAddress    The address to validate.
 */
VMMR3DECL(bool) DBGFR3AddrIsValid(PVM pVM, PCDBGFADDRESS pAddress)
{
    if (!VALID_PTR(pAddress))
        return false;
    if (!DBGFADDRESS_IS_VALID(pAddress))
        return false;
    /* more? */
    return true;
}

