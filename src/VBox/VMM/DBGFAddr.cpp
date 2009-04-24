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
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
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


/**
 * Called on the EMT for the VCpu.
 *
 * @returns VBox status code.
 * @param   pVCpu           The virtual CPU handle.
 * @param   pAddress        The address.
 * @param   pGCPhys         Where to return the physical address.
 */
static DECLCALLBACK(int) dbgfR3AddrToPhysOnVCpu(PVMCPU pVCpu, PDBGFADDRESS pAddress, PRTGCPHYS pGCPhys)
{
    VMCPU_ASSERT_EMT(pVCpu);
    /* This is just a wrapper because we cannot pass FlatPtr thru VMR3ReqCall directly. */
    return PGMGstGetPage(pVCpu, pAddress->FlatPtr, NULL, pGCPhys);
}


/**
 * Converts an address to a guest physical address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_PARAMETER if the address is invalid.
 * @retval  VERR_INVALID_STATE if the VM is being terminated or if the virtual
 *          CPU handle is invalid.
 * @retval  VERR_NOT_SUPPORTED is the type of address cannot be converted.
 * @retval  VERR_PAGE_NOT_PRESENT
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT
 * @retval  VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT
 * @retval  VERR_PAGE_MAP_LEVEL4_NOT_PRESENT
 *
 * @param   pVCpu           The virtual CPU handle.
 * @param   pAddress        The address.
 * @param   pGCPhys         Where to return the physical address.
 */
VMMR3DECL(int)  DBGFR3AddrToPhys(PVMCPU pVCpu, PDBGFADDRESS pAddress, PRTGCPHYS pGCPhys)
{
    /*
     * Parameter validation.
     */
    AssertPtr(pGCPhys);
    *pGCPhys = NIL_RTGCPHYS;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VMCPU_ASSERT_VALID_EXT_RETURN(pVCpu->pVMR3, VERR_INVALID_STATE);

    /*
     * Convert by address type.
     */
    int rc;
    if (pAddress->fFlags & DBGFADDRESS_FLAGS_HMA)
        rc = VERR_NOT_SUPPORTED;
    else if (pAddress->fFlags & DBGFADDRESS_FLAGS_PHYS)
    {
        *pGCPhys = pAddress->FlatPtr;
        rc = VINF_SUCCESS;
    }
    else if (VMCPU_IS_EMT(pVCpu))
        rc = dbgfR3AddrToPhysOnVCpu(pVCpu, pAddress, pGCPhys);
    else
    {
        PVMREQ pReq = NULL;
        rc = VMR3ReqCall(pVCpu->pVMR3, VMREQDEST_FROM_VMCPU(pVCpu), &pReq, RT_INDEFINITE_WAIT,
                         (PFNRT)dbgfR3AddrToPhysOnVCpu, 3, pVCpu, pAddress, pGCPhys);
        if (RT_SUCCESS(rc))
        {
            rc = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
    }
    return rc;
}


/**
 * Converts an address to a host physical address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_PARAMETER if the address is invalid.
 * @retval  VERR_INVALID_STATE if the VM is being terminated or if the virtual
 *          CPU handle is invalid.
 * @retval  VERR_NOT_SUPPORTED is the type of address cannot be converted.
 * @retval  VERR_PAGE_NOT_PRESENT
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT
 * @retval  VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT
 * @retval  VERR_PAGE_MAP_LEVEL4_NOT_PRESENT
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS
 *
 * @param   pVCpu           The virtual CPU handle.
 * @param   pAddress        The address.
 * @param   pHCPhys         Where to return the physical address.
 */
VMMR3DECL(int)  DBGFR3AddrToHostPhys(PVMCPU pVCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys)
{
    /*
     * Parameter validation.
     */
    AssertPtr(pHCPhys);
    *pHCPhys = NIL_RTHCPHYS;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VMCPU_ASSERT_VALID_EXT_RETURN(pVCpu->pVMR3, VERR_INVALID_STATE);

    /*
     * Convert it if we can.
     */
    int rc;
    if (pAddress->fFlags & DBGFADDRESS_FLAGS_HMA)
        rc = VERR_NOT_SUPPORTED; /** @todo implement this */
    else
    {
        RTGCPHYS GCPhys;
        rc = DBGFR3AddrToPhys(pVCpu, pAddress, &GCPhys);
        if (RT_SUCCESS(rc))
            rc = PGMPhysGCPhys2HCPhys(pVCpu->pVMR3, pAddress->FlatPtr, pHCPhys);
    }
    return rc;
}


/**
 * Called on the EMT for the VCpu.
 *
 * @returns VBox status code.
 * @param   pVCpu           The virtual CPU handle.
 * @param   pAddress        The address.
 * @param   fReadOnly       Whether returning a read-only page is fine or not.
 * @param   ppvR3Ptr        Where to return the address.
 */
static DECLCALLBACK(int) dbgfR3AddrToVolatileR3PtrOnVCpu(PVMCPU pVCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    int rc;
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pAddress->fFlags & DBGFADDRESS_FLAGS_HMA)
    {
        rc = VERR_NOT_SUPPORTED; /** @todo create some dedicated errors for this stuff. */
        /** @todo this may assert, create a debug version of this which doesn't. */
        if (MMHyperIsInsideArea(pVM, pAddress->FlatPtr))
        {
            void *pv = MMHyperRCToCC(pVM, (RTRCPTR)pAddress->FlatPtr);
            if (pv)
            {
                *ppvR3Ptr = pv;
                rc = VINF_SUCCESS;
            }
        }
    }
    else
    {
        /*
         * This is a tad ugly, but it gets the job done.
         */
        PGMPAGEMAPLOCK Lock;
        if (pAddress->fFlags & DBGFADDRESS_FLAGS_PHYS)
        {
            if (fReadOnly)
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, pAddress->FlatPtr, (void const **)ppvR3Ptr, &Lock);
            else
                rc = PGMPhysGCPhys2CCPtr(pVM, pAddress->FlatPtr, ppvR3Ptr, &Lock);
        }
        else
        {
            if (fReadOnly)
                rc = PGMPhysGCPtr2CCPtrReadOnly(pVCpu, pAddress->FlatPtr, (void const **)ppvR3Ptr, &Lock);
            else
                rc = PGMPhysGCPtr2CCPtr(pVCpu, pAddress->FlatPtr, ppvR3Ptr, &Lock);
        }
        if (RT_SUCCESS(rc))
            PGMPhysReleasePageMappingLock(pVM, &Lock);
    }
    return rc;
}




/**
 * Converts an address to a volatile host virtual address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_PARAMETER if the address is invalid.
 * @retval  VERR_INVALID_STATE if the VM is being terminated or if the virtual
 *          CPU handle is invalid.
 * @retval  VERR_NOT_SUPPORTED is the type of address cannot be converted.
 * @retval  VERR_PAGE_NOT_PRESENT
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT
 * @retval  VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT
 * @retval  VERR_PAGE_MAP_LEVEL4_NOT_PRESENT
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS
 *
 * @param   pVCpu           The virtual CPU handle.
 * @param   pAddress        The address.
 * @param   fReadOnly       Whether returning a read-only page is fine or not.
 *                          If set to thru the page may have to be made writable
 *                          before we return.
 * @param   ppvR3Ptr        Where to return the address.
 */
VMMR3DECL(int)  DBGFR3AddrToVolatileR3Ptr(PVMCPU pVCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    /*
     * Parameter validation.
     */
    AssertPtr(ppvR3Ptr);
    *ppvR3Ptr = NULL;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VMCPU_ASSERT_VALID_EXT_RETURN(pVCpu->pVMR3, VERR_INVALID_STATE);

    /*
     * Convert it.
     */
    int rc;
    if (VMCPU_IS_EMT(pVCpu))
        rc = dbgfR3AddrToVolatileR3PtrOnVCpu(pVCpu, pAddress, fReadOnly, ppvR3Ptr);
    else
    {
        PVMREQ pReq = NULL;
        rc = VMR3ReqCall(pVCpu->pVMR3, VMREQDEST_FROM_VMCPU(pVCpu), &pReq, RT_INDEFINITE_WAIT,
                         (PFNRT)dbgfR3AddrToVolatileR3PtrOnVCpu, 4, pVCpu, pAddress, fReadOnly, ppvR3Ptr);
        if (RT_SUCCESS(rc))
        {
            rc = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
    }

    return rc;
}

