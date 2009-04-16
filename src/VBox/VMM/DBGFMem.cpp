/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Memory Methods.
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/mm.h>



/**
 * Scan guest memory for an exact byte string.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    Where to store the mixed address.
 * @param   pcbRange    The number of bytes to scan. Passed as a pointer because 
 *                      it may be 64-bit.
 * @param   pabNeedle   What to search for - exact search.
 * @param   cbNeedle    Size of the search byte string.
 * @param   pHitAddress Where to put the address of the first hit.
 */
static DECLCALLBACK(int) dbgfR3MemScan(PVM pVM, PCDBGFADDRESS pAddress, PCRTGCUINTPTR pcbRange, const uint8_t *pabNeedle, size_t cbNeedle,
                                       PDBGFADDRESS pHitAddress)
{
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate the input we use, PGM does the rest.
     */
    RTGCUINTPTR cbRange = *pcbRange;
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
    PGMMODE enmMode = PGMGetGuestMode(pVCpu);
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
#if GC_ARCH_BITS > 32
        if (    (   pAddress->FlatPtr >= _4G
                 || pAddress->FlatPtr + cbRange > _4G)
            &&  enmMode != PGMMODE_AMD64
            &&  enmMode != PGMMODE_AMD64_NX)
            return VERR_DBGF_MEM_NOT_FOUND;
#endif
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
VMMR3DECL(int) DBGFR3MemScan(PVM pVM, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress)
{
    /** @todo SMP support!! */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3MemScan, 6,
                         pVM, pAddress, &cbRange, pabNeedle, cbNeedle, pHitAddress);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Read guest memory.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pAddress        Where to start reading.
 * @param   pvBuf           Where to store the data we've read.
 * @param   cbRead          The number of bytes to read.
 */
static DECLCALLBACK(int) dbgfR3MemRead(PVM pVM, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead)
{
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate the input we use, PGM does the rest.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_POINTER;
    if (!VALID_PTR(pvBuf))
        return VERR_INVALID_POINTER;
    if (DBGFADDRESS_IS_HMA(pAddress))
        return VERR_INVALID_POINTER;

    /*
     * Select DBGF worker by addressing mode.
     */
    int rc;
    PGMMODE enmMode = PGMGetGuestMode(pVCpu);
    if (    enmMode == PGMMODE_REAL
        ||  enmMode == PGMMODE_PROTECTED
        ||  DBGFADDRESS_IS_PHYS(pAddress) )
        rc = PGMPhysSimpleReadGCPhys(pVM, pvBuf, pAddress->FlatPtr, cbRead);
    else
    {
#if GC_ARCH_BITS > 32
        if (    (   pAddress->FlatPtr >= _4G
                 || pAddress->FlatPtr + cbRead > _4G)
            &&  enmMode != PGMMODE_AMD64
            &&  enmMode != PGMMODE_AMD64_NX)
            return VERR_PAGE_TABLE_NOT_PRESENT;
#endif
        rc = PGMPhysSimpleReadGCPtr(pVCpu, pvBuf, pAddress->FlatPtr, cbRead);
    }
    return rc;
}


/**
 * Read guest memory.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pAddress        Where to start reading.
 * @param   pvBuf           Where to store the data we've read.
 * @param   cbRead          The number of bytes to read.
 */
VMMR3DECL(int) DBGFR3MemRead(PVM pVM, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead)
{
    /** @todo SMP support!! */
    PVMREQ pReq;
    int rc = VMR3ReqCallU(pVM->pUVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)dbgfR3MemRead, 4,
                          pVM, pAddress, pvBuf, cbRead);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Read a zero terminated string from guest memory.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pAddress        Where to start reading.
 * @param   pszBuf          Where to store the string.
 * @param   cchBuf          The size of the buffer.
 */
static DECLCALLBACK(int) dbgfR3MemReadString(PVM pVM, PCDBGFADDRESS pAddress, char *pszBuf, size_t cchBuf)
{
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate the input we use, PGM does the rest.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_POINTER;
    if (!VALID_PTR(pszBuf))
        return VERR_INVALID_POINTER;
    if (DBGFADDRESS_IS_HMA(pAddress))
        return VERR_INVALID_POINTER;

    /*
     * Select DBGF worker by addressing mode.
     */
    int rc;
    PGMMODE enmMode = PGMGetGuestMode(pVCpu);
    if (    enmMode == PGMMODE_REAL
        ||  enmMode == PGMMODE_PROTECTED
        ||  DBGFADDRESS_IS_PHYS(pAddress) )
        rc = PGMPhysSimpleReadGCPhys(pVM, pszBuf, pAddress->FlatPtr, cchBuf);
    else
    {
#if GC_ARCH_BITS > 32
        if (    (   pAddress->FlatPtr >= _4G
                 || pAddress->FlatPtr + cchBuf > _4G)
            &&  enmMode != PGMMODE_AMD64
            &&  enmMode != PGMMODE_AMD64_NX)
            return VERR_PAGE_TABLE_NOT_PRESENT;
#endif
        rc = PGMPhysSimpleReadGCPtr(pVCpu, pszBuf, pAddress->FlatPtr, cchBuf);
    }

    /*
     * Make sure the result is terminated and that overflow is signaled.
     */
    if (!memchr(pszBuf, '\0', cchBuf))
    {
        pszBuf[cchBuf - 1] = '\0';
        rc = VINF_BUFFER_OVERFLOW;
    }
    /*
     * Handle partial reads (not perfect).
     */
    else if (RT_FAILURE(rc))
    {
        if (pszBuf[0])
            rc = VINF_SUCCESS;
    }

    return rc;
}


/**
 * Read a zero terminated string from guest memory.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pAddress        Where to start reading.
 * @param   pszBuf          Where to store the string.
 * @param   cchBuf          The size of the buffer.
 */
VMMR3DECL(int) DBGFR3MemReadString(PVM pVM, PCDBGFADDRESS pAddress, char *pszBuf, size_t cchBuf)
{
    /*
     * Validate and zero output.
     */
    if (!VALID_PTR(pszBuf))
        return VERR_INVALID_POINTER;
    if (cchBuf <= 0)
        return VERR_INVALID_PARAMETER;
    memset(pszBuf, 0, cchBuf);

    /*
     * Pass it on to the EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCallU(pVM->pUVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)dbgfR3MemReadString, 4,
                          pVM, pAddress, pszBuf, cchBuf);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Read memory from GC virtual address using the current guest CR3.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pvDst       Destination address (HC of course).
 * @param   GCPtr       GC virtual address.
 * @param   cb          Number of bytes to read.
 *
 * @remarks Intended for the debugger facility only.
 */
VMMR3DECL(int) DBGFR3ReadGCVirt(PVM pVM, PVMCPU pVCpu, void *pvDst, RTGCPTR GCPtr, size_t cb)
{
    if (MMHyperIsInsideArea(pVM, GCPtr))
        return MMR3HyperReadGCVirt(pVM, pvDst, GCPtr, cb);
    return PGMPhysSimpleReadGCPtr(pVCpu, pvDst, GCPtr, cb);
}


/**
 * Write to memory at GC virtual address translated using the current guest CR3.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   GCPtrDst    GC virtual address.
 * @param   pvSrc       The source address (HC of course).
 * @param   cb          Number of bytes to read.
 *
 * @remarks Intended for the debugger facility only.
 */
VMMR3DECL(int) DBGFR3WriteGCVirt(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    if (MMHyperIsInsideArea(pVM, GCPtrDst))
        return VERR_ACCESS_DENIED;
    return PGMPhysSimpleWriteGCPtr(pVCpu, GCPtrDst, pvSrc, cb);
}
