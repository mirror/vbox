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
 * @param   idCpu       The CPU ID.
 * @param   pAddress    Where to store the mixed address.
 * @param   Sel         The selector part.
 * @param   off         The offset part.
 */
VMMR3DECL(int) DBGFR3AddrFromSelOff(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off)
{
    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_PARAMETER);

    pAddress->Sel = Sel;
    pAddress->off = off;
    if (Sel != DBGF_SEL_FLAT)
    {
        SELMSELINFO SelInfo;
        int rc = SELMR3GetSelectorInfo(pVM, VMMGetCpuById(pVM, idCpu), Sel, &SelInfo);
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
 * @returns pAddress.
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
 * @returns pAddress.
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   PhysAddr    The guest physical address.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromPhys(PVM pVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr)
{
    pAddress->Sel     = DBGF_SEL_FLAT;
    pAddress->off     = PhysAddr;
    pAddress->FlatPtr = PhysAddr;
    pAddress->fFlags  = DBGFADDRESS_FLAGS_PHYS | DBGFADDRESS_FLAGS_VALID;
    return pAddress;
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
 * @param   pVM             The VM handle.
 * @param   idCpu           The ID of the CPU context to convert virtual
 *                          addresses.
 * @param   pAddress        The address.
 * @param   pGCPhys         Where to return the physical address.
 */
VMMR3DECL(int)  DBGFR3AddrToPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTGCPHYS pGCPhys)
{
    /*
     * Parameter validation.
     */
    AssertPtr(pGCPhys);
    *pGCPhys = NIL_RTGCPHYS;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_STATE);
    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_PARAMETER);

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
    else
    {
        PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
        if (VMCPU_IS_EMT(pVCpu))
            rc = dbgfR3AddrToPhysOnVCpu(pVCpu, pAddress, pGCPhys);
        else
        {
            PVMREQ pReq = NULL;
            rc = VMR3ReqCall(pVCpu->pVMR3, pVCpu->idCpu, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)dbgfR3AddrToPhysOnVCpu, 3, pVCpu, pAddress, pGCPhys);
            if (RT_SUCCESS(rc))
            {
                rc = pReq->iStatus;
                VMR3ReqFree(pReq);
            }
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
 * @param   pVM             The VM handle.
 * @param   idCpu           The ID of the CPU context to convert virtual
 *                          addresses.
 * @param   pAddress        The address.
 * @param   pHCPhys         Where to return the physical address.
 */
VMMR3DECL(int)  DBGFR3AddrToHostPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys)
{
    /*
     * Parameter validation.
     */
    AssertPtr(pHCPhys);
    *pHCPhys = NIL_RTHCPHYS;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_STATE);
    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_PARAMETER);

    /*
     * Convert it if we can.
     */
    int rc;
    if (pAddress->fFlags & DBGFADDRESS_FLAGS_HMA)
        rc = VERR_NOT_SUPPORTED; /** @todo implement this */
    else
    {
        RTGCPHYS GCPhys;
        rc = DBGFR3AddrToPhys(pVM, idCpu, pAddress, &GCPhys);
        if (RT_SUCCESS(rc))
            rc = PGMPhysGCPhys2HCPhys(pVM, pAddress->FlatPtr, pHCPhys);
    }
    return rc;
}


/**
 * Called on the EMT for the VCpu.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   idCpu           The ID of the CPU context.
 * @param   pAddress        The address.
 * @param   fReadOnly       Whether returning a read-only page is fine or not.
 * @param   ppvR3Ptr        Where to return the address.
 */
static DECLCALLBACK(int) dbgfR3AddrToVolatileR3PtrOnVCpu(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    Assert(idCpu == VMMGetCpuId(pVM));

    int rc;
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
            PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
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
 * @param   pVM             The VM handle.
 * @param   idCpu           The ID of the CPU context to convert virtual
 *                          addresses.
 * @param   pAddress        The address.
 * @param   fReadOnly       Whether returning a read-only page is fine or not.
 *                          If set to thru the page may have to be made writable
 *                          before we return.
 * @param   ppvR3Ptr        Where to return the address.
 */
VMMR3DECL(int)  DBGFR3AddrToVolatileR3Ptr(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    /*
     * Parameter validation.
     */
    AssertPtr(ppvR3Ptr);
    *ppvR3Ptr = NULL;
    AssertPtr(pAddress);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_STATE);
    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_PARAMETER);

    /*
     * Convert it.
     */
    PVMREQ pReq = NULL;
    int rc = VMR3ReqCall(pVM, idCpu, &pReq, RT_INDEFINITE_WAIT,
                         (PFNRT)dbgfR3AddrToVolatileR3PtrOnVCpu, 5, pVM, idCpu, pAddress, fReadOnly, ppvR3Ptr);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    return rc;
}


/**
 * Adds an offset to an address.
 *
 * @returns pAddress.
 *
 * @param   pAddress        The address.
 * @param   uAddend         How much to add.
 *
 * @remarks No address space or segment limit checks are performed,
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrAdd(PDBGFADDRESS pAddress, RTGCUINTPTR uAddend)
{
    /*
     * Parameter validation.
     */
    AssertPtrReturn(pAddress, NULL);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), NULL);

    /*
     * Add the stuff.
     */
    pAddress->off     += uAddend;
    pAddress->FlatPtr += uAddend;

    return pAddress;
}


/**
 * Subtracts an offset from an address.
 *
 * @returns VINF_SUCCESS on success.
 *
 * @param   pAddress        The address.
 * @param   uSubtrahend     How much to subtract.
 *
 * @remarks No address space or segment limit checks are performed,
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrSub(PDBGFADDRESS pAddress, RTGCUINTPTR uSubtrahend)
{
    /*
     * Parameter validation.
     */
    AssertPtrReturn(pAddress, NULL);
    AssertReturn(DBGFADDRESS_IS_VALID(pAddress), NULL);

    /*
     * Add the stuff.
     */
    pAddress->off     -= uSubtrahend;
    pAddress->FlatPtr -= uSubtrahend;

    return pAddress;
}

