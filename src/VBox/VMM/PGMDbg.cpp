/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor - Debugger & Debugging APIs.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include <VBox/stam.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>



/**
 * Converts a HC pointer to a GC physical address. 
 * 
 * Only for the debugger.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 * 
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys)
{
#ifdef NEW_PHYS_CODE
    *pGCPhys = NIL_RTGCPHYS;
    return VERR_NOT_IMPLEMENTED;

#else
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        *pGCPhys = pRam->GCPhys + iChunk*PGM_DYNAMIC_CHUNK_SIZE + off;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                *pGCPhys = pRam->GCPhys + off;
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_POINTER;
#endif
}


/**
 * Converts a HC pointer to a GC physical address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pHCPhys is set.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical page but has no physical backing.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 * 
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys)
{
#ifdef NEW_PHYS_CODE
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_NOT_IMPLEMENTED;

#else
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        PPGMPAGE pPage = &pRam->aPages[off >> PAGE_SHIFT];
                        if (PGM_PAGE_IS_RESERVED(pPage))
                            return VERR_PGM_PHYS_PAGE_RESERVED;
                        *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage)
                                 | (off & PAGE_OFFSET_MASK);
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                PPGMPAGE pPage = &pRam->aPages[off >> PAGE_SHIFT];
                if (PGM_PAGE_IS_RESERVED(pPage))
                    return VERR_PGM_PHYS_PAGE_RESERVED;
                *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage)
                         | (off & PAGE_OFFSET_MASK);
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_POINTER;
#endif
}


/**
 * Converts a HC physical address to a GC physical address.
 * 
 * Only for the debugger.
 *
 * @returns VBox status code
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the HC physical address is not within the GC physical memory.
 * 
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys)
{
    /*
     * Validate and adjust the input a bit.
     */
    if (HCPhys == NIL_RTHCPHYS)
        return VERR_INVALID_POINTER;
    unsigned off = HCPhys & PAGE_OFFSET_MASK;
    HCPhys &= X86_PTE_PAE_PG_MASK;
    if (HCPhys == 0)
        return VERR_INVALID_POINTER;

    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (    PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys
                &&  !PGM_PAGE_IS_RESERVED(&pRam->aPages[iPage]))
            {
                *pGCPhys = pRam->GCPhys + (iPage << PAGE_SHIFT) + off;
                return VINF_SUCCESS;
            }
    }
    return VERR_INVALID_POINTER;
}


