/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor - All context code.
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
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/err.h>


/**
 * Maps a range of physical pages at a given virtual address
 * in the guest context.
 *
 * The GC virtual address range must be within an existing mapping.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   GCPtr       Where to map the page(s). Must be page aligned.
 * @param   HCPhys      Start of the range of physical pages. Must be page aligned.
 * @param   cbPages     Number of bytes to map. Must be page aligned.
 * @param   fFlags      Page flags (X86_PTE_*).
 */
VMMDECL(int) PGMMap(PVM pVM, RTGCUINTPTR GCPtr, RTHCPHYS HCPhys, uint32_t cbPages, unsigned fFlags)
{
    AssertMsg(pVM->pgm.s.offVM, ("Bad init order\n"));

    /*
     * Validate input.
     */
    AssertMsg(RT_ALIGN_T(GCPtr, PAGE_SIZE, RTGCUINTPTR) == GCPtr, ("Invalid alignment GCPtr=%#x\n", GCPtr));
    AssertMsg(cbPages > 0 && RT_ALIGN_32(cbPages, PAGE_SIZE) == cbPages, ("Invalid cbPages=%#x\n",  cbPages));
    AssertMsg(!(fFlags & X86_PDE_PG_MASK), ("Invalid flags %#x\n", fFlags));

    /* hypervisor defaults */
    if (!fFlags)
        fFlags = X86_PTE_P | X86_PTE_A | X86_PTE_D;

    /*
     * Find the mapping.
     */
    PPGMMAPPING pCur = pVM->pgm.s.CTX_SUFF(pMappings);
    while (pCur)
    {
        if (GCPtr - pCur->GCPtr < pCur->cb)
        {
            if (GCPtr + cbPages - 1 > pCur->GCPtrLast)
            {
                AssertMsgFailed(("Invalid range!!\n"));
                return VERR_INVALID_PARAMETER;
            }

            /*
             * Setup PTE.
             */
            X86PTEPAE Pte;
            Pte.u = fFlags | (HCPhys & X86_PTE_PAE_PG_MASK);

            /*
             * Update the page tables.
             */
            for (;;)
            {
                RTGCUINTPTR     off = GCPtr - pCur->GCPtr;
                const unsigned  iPT = off >> X86_PD_SHIFT;
                const unsigned  iPageNo = (off >> PAGE_SHIFT) & X86_PT_MASK;

                /* 32-bit */
                pCur->aPTs[iPT].CTX_SUFF(pPT)->a[iPageNo].u = (uint32_t)Pte.u;      /* ASSUMES HCPhys < 4GB and/or that we're never gonna do 32-bit on a PAE host! */

                /* pae */
                pCur->aPTs[iPT].CTX_SUFF(paPaePTs)[iPageNo / 512].a[iPageNo % 512].u = Pte.u;

                /* next */
                cbPages -= PAGE_SIZE;
                if (!cbPages)
                    break;
                GCPtr += PAGE_SIZE;
                Pte.u += PAGE_SIZE;
            }

            return VINF_SUCCESS;
        }

        /* next */
        pCur = pCur->CTX_SUFF(pNext);
    }

    AssertMsgFailed(("GCPtr=%#x was not found in any mapping ranges!\n",  GCPtr));
    return VERR_INVALID_PARAMETER;
}


/**
 * Sets (replaces) the page flags for a range of pages in a mapping.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 */
VMMDECL(int) PGMMapSetPage(PVM pVM, RTGCPTR GCPtr, uint64_t cb, uint64_t fFlags)
{
    return PGMMapModifyPage(pVM, GCPtr, cb, fFlags, 0);
}


/**
 * Modify page flags for a range of pages in a mapping.
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*, excluding the page mask of course.
 */
VMMDECL(int)  PGMMapModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    /*
     * Validate input.
     */
    AssertMsg(!(fFlags & X86_PTE_PAE_PG_MASK), ("fFlags=%#x\n", fFlags));
    Assert(cb);

    /*
     * Align the input.
     */
    cb     += (RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK;
    cb      = RT_ALIGN_Z(cb, PAGE_SIZE);
    GCPtr   = (RTGCPTR)((RTGCUINTPTR)GCPtr & PAGE_BASE_GC_MASK);

    /*
     * Find the mapping.
     */
    PPGMMAPPING pCur = pVM->pgm.s.CTX_SUFF(pMappings);
    while (pCur)
    {
        RTGCUINTPTR off = (RTGCUINTPTR)GCPtr - (RTGCUINTPTR)pCur->GCPtr;
        if (off < pCur->cb)
        {
            AssertMsgReturn(off + cb <= pCur->cb,
                            ("Invalid page range %#x LB%#x. mapping '%s' %#x to %#x\n",
                             GCPtr, cb, pCur->pszDesc, pCur->GCPtr, pCur->GCPtrLast),
                            VERR_INVALID_PARAMETER);

            /*
             * Perform the requested operation.
             */
            while (cb > 0)
            {
                unsigned iPT  = off >> X86_PD_SHIFT;
                unsigned iPTE = (off >> PAGE_SHIFT) & X86_PT_MASK;
                while (cb > 0 && iPTE < RT_ELEMENTS(pCur->aPTs[iPT].CTX_SUFF(pPT)->a))
                {
                    /* 32-Bit */
                    pCur->aPTs[iPT].CTX_SUFF(pPT)->a[iPTE].u &= fMask | X86_PTE_PG_MASK;
                    pCur->aPTs[iPT].CTX_SUFF(pPT)->a[iPTE].u |= fFlags & ~X86_PTE_PG_MASK;

                    /* PAE */
                    pCur->aPTs[iPT].CTX_SUFF(paPaePTs)[iPTE / 512].a[iPTE % 512].u &= fMask | X86_PTE_PAE_PG_MASK;
                    pCur->aPTs[iPT].CTX_SUFF(paPaePTs)[iPTE / 512].a[iPTE % 512].u |= fFlags & ~X86_PTE_PAE_PG_MASK;

                    /* invalidate tls */
                    PGM_INVL_PG((RTGCUINTPTR)pCur->GCPtr + off);

                    /* next */
                    iPTE++;
                    cb -= PAGE_SIZE;
                    off += PAGE_SIZE;
                }
            }

            return VINF_SUCCESS;
        }
        /* next */
        pCur = pCur->CTX_SUFF(pNext);
    }

    AssertMsgFailed(("Page range %#x LB%#x not found\n", GCPtr, cb));
    return VERR_INVALID_PARAMETER;
}



