/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Physical Memory.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_MM_PHYS
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/rem.h>
#include "MMInternal.h"
#include <VBox/vm.h>

#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/**
 * Register externally allocated RAM for the virtual machine.
 *
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   pvRam       Virtual address of the guest's physical memory range Must be page aligned.
 * @param   GCPhys      The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 * @param   pszDesc     Description of the memory.
 */
MMR3DECL(int) MMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, unsigned cb, unsigned fFlags, const char *pszDesc)
{
    return MMR3PhysRegisterEx(pVM, pvRam, GCPhys, cb, fFlags, MM_PHYS_TYPE_NORMAL, pszDesc);
}


/**
 * Register externally allocated RAM for the virtual machine.
 *
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   pvRam       Virtual address of the guest's physical memory range Must be page aligned.
 * @param   GCPhys      The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 * @param   enmType     Physical range type (MM_PHYS_TYPE_*)
 * @param   pszDesc     Description of the memory.
 * @thread  The Emulation Thread.
 *
 * @deprecated  For the old dynamic allocation code only. Will be removed with VBOX_WITH_NEW_PHYS_CODE.
 */
/** @todo this function description is not longer up-to-date */
MMR3DECL(int) MMR3PhysRegisterEx(PVM pVM, void *pvRam, RTGCPHYS GCPhys, unsigned cb, unsigned fFlags, MMPHYSREG enmType, const char *pszDesc)
{
    int rc = VINF_SUCCESS;

    Log(("MMR3PhysRegister: pvRam=%p GCPhys=%VGp cb=%#x fFlags=%#x\n", pvRam, GCPhys, cb, fFlags));

    /*
     * Validate input.
     */
    AssertMsg(pVM, ("Invalid VM pointer\n"));
    if (pvRam)
        AssertReturn(RT_ALIGN_P(pvRam, PAGE_SIZE) == pvRam, VERR_INVALID_PARAMETER);
    else
        AssertReturn(fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_Z(cb, PAGE_SIZE) == cb, VERR_INVALID_PARAMETER);
    AssertReturn(enmType == MM_PHYS_TYPE_NORMAL || enmType == MM_PHYS_TYPE_DYNALLOC_CHUNK, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);


    /*
     * Check for conflicts.
     *
     * We do not support overlapping physical memory regions yet,
     * even if that's what the MM_RAM_FLAGS_MMIO2 flags is trying to
     * tell us to do. Provided that all MMIO2 addresses are very high
     * there is no real danger we'll be able to assign so much memory
     * for a guest that it'll ever be a problem.
     */
    AssertMsg(!(fFlags & MM_RAM_FLAGS_MMIO2) || GCPhys > 0xc0000000,
        ("MMIO2 addresses should be above 3GB for avoiding conflicts with real RAM.\n"));
    PMMLOCKEDMEM pCur = pVM->mm.s.pLockedMem;
    while (pCur)
    {
        if (    pCur->eType == MM_LOCKED_TYPE_PHYS
            &&  (   GCPhys - pCur->u.phys.GCPhys < pCur->cb
                 || pCur->u.phys.GCPhys - GCPhys < cb)
            )
        {
            AssertMsgFailed(("Conflicting RAM range. Existing %#x LB%#x, Req %#x LB%#x\n",
                             pCur->u.phys.GCPhys, pCur->cb, GCPhys, cb));
            return VERR_MM_RAM_CONFLICT;
        }

        /* next */
        pCur = pCur->pNext;
    }


    /* Dynamic/on-demand allocation of backing memory? */
    if (fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
    {
        /*
         * Register the ram with PGM.
         */
        rc = PGMR3PhysRegister(pVM, pvRam, GCPhys, cb, fFlags, NULL, pszDesc);
        if (VBOX_SUCCESS(rc))
        {
            if (fFlags == MM_RAM_FLAGS_DYNAMIC_ALLOC)
                pVM->mm.s.cBasePages += cb >> PAGE_SHIFT;

            REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, fFlags);
            return rc;
        }
    }
    else
    {
        /*
         * Lock the memory. (fully allocated by caller)
         */
        PMMLOCKEDMEM    pLockedMem;
        rc = mmR3LockMem(pVM, pvRam, cb, MM_LOCKED_TYPE_PHYS, &pLockedMem, enmType == MM_PHYS_TYPE_DYNALLOC_CHUNK /* fSilentFailure */);
        if (VBOX_SUCCESS(rc))
        {
            pLockedMem->u.phys.GCPhys = GCPhys;

            /*
             * We set any page flags specified.
             */
            if (fFlags)
                for (unsigned i = 0; i < cb >> PAGE_SHIFT; i++)
                    pLockedMem->aPhysPages[i].Phys |= fFlags;

            /*
             * Register the ram with PGM.
             */
            if (enmType == MM_PHYS_TYPE_NORMAL)
            {
                rc = PGMR3PhysRegister(pVM, pvRam, pLockedMem->u.phys.GCPhys, cb, fFlags, &pLockedMem->aPhysPages[0], pszDesc);
                if (VBOX_SUCCESS(rc))
                {
                    if (!fFlags)
                        pVM->mm.s.cBasePages += cb >> PAGE_SHIFT;

                    REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, fFlags);
                    return rc;
                }
            }
            else
            {
                Assert(enmType == MM_PHYS_TYPE_DYNALLOC_CHUNK);
                return PGMR3PhysRegisterChunk(pVM, pvRam, pLockedMem->u.phys.GCPhys, cb, fFlags, &pLockedMem->aPhysPages[0], pszDesc);
            }
        }
        /* Cleanup is done in VM destruction to which failure of this function will lead. */
        /* Not true in case of MM_PHYS_TYPE_DYNALLOC_CHUNK */
    }

    return rc;
}


/**
 * Relocate previously registered externally allocated RAM for the virtual machine.
 *
 * Use this only for MMIO ranges or the guest will become very confused.
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPhysOld   The physical address the ram was registered at.
 * @param   GCPhysNew   The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 */
MMR3DECL(int) MMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, unsigned cb)
{
    Log(("MMR3PhysRelocate: GCPhysOld=%VGp GCPhysNew=%VGp cb=%#x\n", GCPhysOld, GCPhysNew, cb));

    /*
     * Validate input.
     */
    AssertMsg(pVM, ("Invalid VM pointer\n"));
    AssertReturn(RT_ALIGN_T(GCPhysOld, PAGE_SIZE, RTGCPHYS) == GCPhysOld, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(GCPhysNew, PAGE_SIZE, RTGCPHYS) == GCPhysNew, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN(cb, PAGE_SIZE) == cb, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast;
    GCPhysLast = GCPhysOld + (cb - 1);
    AssertReturn(GCPhysLast > GCPhysOld, VERR_INVALID_PARAMETER);
    GCPhysLast = GCPhysNew + (cb - 1);
    AssertReturn(GCPhysLast > GCPhysNew, VERR_INVALID_PARAMETER);

    /*
     * Find the old memory region.
     */
    PMMLOCKEDMEM pCur = pVM->mm.s.pLockedMem;
    while (pCur)
    {
        if (    pCur->eType == MM_LOCKED_TYPE_PHYS
            &&  GCPhysOld == pCur->u.phys.GCPhys
            &&  cb == pCur->cb)
            break;

        /* next */
        pCur = pCur->pNext;
    }
    if (!pCur)
    {
        AssertMsgFailed(("Unknown old region! %VGp LB%#x\n", GCPhysOld, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Region is already locked, just need to change GC address.
     */
/** @todo r=bird: check for conflicts? */
    pCur->u.phys.GCPhys = GCPhysNew;

    /*
     * Relocate the registered RAM range with PGM.
     */
    int rc = PGMR3PhysRelocate(pVM, GCPhysOld, GCPhysNew, cb);
    if (VBOX_SUCCESS(rc))
    {
        /* Somewhat hackish way to relocate the region with REM. There
         * is unfortunately no official way to unregister anything with
         * REM, as there is no way to unregister memory with QEMU.
         * This implementation seems to work, but is not very pretty. */
        /// @todo one day provide a proper MMIO relocation operation
        REMR3NotifyPhysReserve(pVM, GCPhysOld, cb);
        REMR3NotifyPhysRamRegister(pVM, GCPhysNew, cb,
                                   pCur->aPhysPages[0].Phys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2));
    }

    return rc;
}


/**
 * Register a ROM (BIOS) region.
 *
 * It goes without saying that this is read-only memory. The memory region must be
 * in unassigned memory. I.e. from the top of the address space or on the PC in
 * the 0xa0000-0xfffff range.
 *
 * @returns VBox status.
 * @param   pVM                 VM Handle.
 * @param   pDevIns             The device instance owning the ROM region.
 * @param   GCPhys              First physical address in the range.
 *                              Must be page aligned!
 * @param   cbRange             The size of the range (in bytes).
 *                              Must be page aligned!
 * @param   pvBinary            Pointer to the binary data backing the ROM image.
 *                              This must be cbRange bytes big.
 *                              It will be copied and doesn't have to stick around.
 *                              It will be copied and doesn't have to stick around if fShadow is clear.
 * @param   fShadow             Whether to emulate ROM shadowing. This involves leaving
 *                              the ROM writable for a while during the POST and refreshing
 *                              it at reset. When this flag is set, the memory pointed to by
 *                              pvBinary has to stick around for the lifespan of the VM.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 * @remark  There is no way to remove the rom, automatically on device cleanup or
 *          manually from the device yet. At present I doubt we need such features...
 */
MMR3DECL(int) MMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const void *pvBinary,
                                  bool fShadow, const char *pszDesc)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN(cbRange, PAGE_SIZE) == cbRange, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cbRange - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBinary, VERR_INVALID_PARAMETER);


    /*
     * Check if this can fit in an existing range.
     *
     * We do not handle the case where a new chunk of locked memory is
     * required to accommodate the ROM since we assume MMR3PhysReserve()
     * have been called to reserve the memory first.
     *
     * To make things even simpler, the pages in question must be
     * marked as reserved.
     */
    PMMLOCKEDMEM pCur = pVM->mm.s.pLockedMem;
    for ( ; pCur; pCur = pCur->pNext)
        if (    pCur->eType == MM_LOCKED_TYPE_PHYS
            &&  GCPhys - pCur->u.phys.GCPhys < pCur->cb)
            break;
    if (!pCur)
    {
        AssertMsgFailed(("No physical range was found matching the ROM location (%#VGp LB%#x)\n", GCPhys, cbRange));
        return VERR_INVALID_PARAMETER;
    }
    if (GCPhysLast - pCur->u.phys.GCPhys >= pCur->cb)
    {
        AssertMsgFailed(("The ROM range (%#VGp LB%#x) was crossing the end of the physical range (%#VGp LB%#x)\n",
                         GCPhys, cbRange, pCur->u.phys.GCPhys, pCur->cb));
        return VERR_INVALID_PARAMETER;
    }

    /* flags must be all reserved. */
    unsigned iPage = (GCPhys - pCur->u.phys.GCPhys) >> PAGE_SHIFT;
    unsigned iPageEnd = cbRange >> PAGE_SHIFT;
    for (; iPage < iPageEnd; iPage++)
        if (    (pCur->aPhysPages[iPage].Phys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2))
            !=  MM_RAM_FLAGS_RESERVED)
        {
            AssertMsgFailed(("Flags conflict at %VGp, HCPhys=%VHp.\n", pCur->u.phys.GCPhys + (iPage << PAGE_SHIFT), pCur->aPhysPages[iPage].Phys));
            return VERR_INVALID_PARAMETER;
        }

    /*
     * Copy the ram and update the flags.
     */
    iPage = (GCPhys - pCur->u.phys.GCPhys) >> PAGE_SHIFT;
    void *pvCopy = (char *)pCur->pv + (iPage << PAGE_SHIFT);
    memcpy(pvCopy, pvBinary, cbRange);

    const unsigned fSet = fShadow ? MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO2 : MM_RAM_FLAGS_ROM;
    for (; iPage < iPageEnd; iPage++)
    {
        pCur->aPhysPages[iPage].Phys &= ~MM_RAM_FLAGS_RESERVED;
        pCur->aPhysPages[iPage].Phys |= fSet;
    }
    int rc = PGMR3PhysSetFlags(pVM, GCPhys, cbRange, fSet, ~MM_RAM_FLAGS_RESERVED);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * To prevent the shadow page table mappings from being RW in raw-mode, we
         * must currently employ a little hack. We register an write access handler
         * and thereby ensures a RO mapping of the pages. This is NOT very nice,
         * and wasn't really my intention when writing the code, consider it a PGM bug.
         *
         * ASSUMES that REMR3NotifyPhysRomRegister doesn't call cpu_register_physical_memory
         * when there is no HC handler. The result would probably be immediate boot failure.
         */
        rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhys + cbRange - 1,
                                          NULL, NULL,
                                          NULL, "pgmGuestROMWriteHandler", 0,
                                          NULL, "pgmGuestROMWriteHandler", 0, pszDesc);
        AssertRC(rc);
    }

    /*
     * Create a ROM range it so we can make a 'info rom' thingy and more importantly
     * reload and protect/unprotect shadow ROM correctly.
     */
    if (VBOX_SUCCESS(rc))
    {
        PMMROMRANGE pRomRange = (PMMROMRANGE)MMR3HeapAlloc(pVM, MM_TAG_MM, sizeof(*pRomRange));
        AssertReturn(pRomRange, VERR_NO_MEMORY);
        pRomRange->GCPhys = GCPhys;
        pRomRange->cbRange = cbRange;
        pRomRange->pszDesc = pszDesc;
        pRomRange->fShadow = fShadow;
        pRomRange->fWritable = fShadow;
        pRomRange->pvBinary = fShadow ? pvBinary : NULL;
        pRomRange->pvCopy = pvCopy;

        /* sort it for 'info rom' readability.  */
        PMMROMRANGE pPrev = NULL;
        PMMROMRANGE pCur = pVM->mm.s.pRomHead;
        while (pCur && pCur->GCPhys < GCPhys)
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
        pRomRange->pNext = pCur;
        if (pPrev)
            pPrev->pNext = pRomRange;
        else
            pVM->mm.s.pRomHead = pRomRange;
    }

    REMR3NotifyPhysRomRegister(pVM, GCPhys, cbRange, pvCopy, fShadow);
    return rc; /* we're sloppy with error cleanup here, but we're toast anyway if this fails. */
}


/**
 * Reserve physical address space for ROM and MMIO ranges.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address.
 * @param   cbRange         The size of the range.
 * @param   pszDesc         Description string.
 */
MMR3DECL(int) MMR3PhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc)
{
    /*
     * Validate input.
     */
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN(cbRange, PAGE_SIZE) == cbRange, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cbRange - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);

    /*
     * Do we have an existing physical address range for the request?
     */
    PMMLOCKEDMEM pCur = pVM->mm.s.pLockedMem;
    for ( ; pCur; pCur = pCur->pNext)
        if (    pCur->eType == MM_LOCKED_TYPE_PHYS
            &&  GCPhys - pCur->u.phys.GCPhys < pCur->cb)
            break;
    if (!pCur)
    {
        /*
         * No range, we'll just allocate backing pages and register
         * them as reserved using the Ram interface.
         */
        void *pvPages;
        int rc = SUPPageAlloc(cbRange >> PAGE_SHIFT, &pvPages);
        if (VBOX_SUCCESS(rc))
        {
            rc = MMR3PhysRegister(pVM, pvPages, GCPhys, cbRange, MM_RAM_FLAGS_RESERVED, pszDesc);
            if (VBOX_FAILURE(rc))
                SUPPageFree(pvPages, cbRange >> PAGE_SHIFT);
        }
        return rc;
    }
    if (GCPhysLast - pCur->u.phys.GCPhys >= pCur->cb)
    {
        AssertMsgFailed(("The reserved range (%#VGp LB%#x) was crossing the end of the physical range (%#VGp LB%#x)\n",
                         GCPhys, cbRange, pCur->u.phys.GCPhys, pCur->cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Update the flags.
     */
    unsigned iPage = (GCPhys - pCur->u.phys.GCPhys) >> PAGE_SHIFT;
    unsigned iPageEnd = cbRange >> PAGE_SHIFT;
    for (; iPage < iPageEnd; iPage++)
        pCur->aPhysPages[iPage].Phys |= MM_RAM_FLAGS_RESERVED;
    int rc = PGMR3PhysSetFlags(pVM, GCPhys, cbRange, MM_RAM_FLAGS_RESERVED, ~0);
    AssertRC(rc);

    REMR3NotifyPhysReserve(pVM, GCPhys, cbRange);
    return rc;
}


/**
 * Get the size of the base RAM.
 * This usually means the size of the first contigous block of physical memory.
 *
 * @returns The guest base RAM size.
 * @param   pVM         The VM handle.
 * @thread  Any.
 */
MMR3DECL(uint64_t) MMR3PhysGetRamSize(PVM pVM)
{
    return pVM->mm.s.cbRamBase;
}


/**
 * Called by MMR3Reset to reset the shadow ROM.
 *
 * Resetting involves reloading the ROM into RAM and make it
 * wriable again (as it was made read only at the end of the POST).
 *
 * @param   pVM         The VM handle.
 */
void mmR3PhysRomReset(PVM pVM)
{
    for (PMMROMRANGE pCur = pVM->mm.s.pRomHead; pCur; pCur = pCur->pNext)
        if (pCur->fShadow)
        {
            memcpy(pCur->pvCopy, pCur->pvBinary, pCur->cbRange);
            if (!pCur->fWritable)
            {
                int rc = PGMHandlerPhysicalDeregister(pVM, pCur->GCPhys);
                AssertRC(rc);
                pCur->fWritable = true;

                rc = PGMR3PhysSetFlags(pVM, pCur->GCPhys, pCur->cbRange, MM_RAM_FLAGS_MMIO2, ~0); /* ROM -> ROM + MMIO2 */
                AssertRC(rc);

                REMR3NotifyPhysRomRegister(pVM, pCur->GCPhys, pCur->cbRange, pCur->pvCopy, true /* read-write now */);
            }
        }
}


/**
 * Write-protects a shadow ROM range.
 *
 * This is called late in the POST for shadow ROM ranges.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   GCPhys      Start of the registered shadow ROM range
 * @param   cbRange     The length of the registered shadow ROM range.
 *                      This can be NULL (not sure about the BIOS interface yet).
 */
MMR3DECL(int) MMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange)
{
    for (PMMROMRANGE pCur = pVM->mm.s.pRomHead; pCur; pCur = pCur->pNext)
        if (    pCur->GCPhys == GCPhys
            &&  (   pCur->cbRange == cbRange
                 || !cbRange))
        {
            if (pCur->fWritable)
            {
                cbRange = pCur->cbRange;
                int rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhys + cbRange - 1,
                                                      NULL, NULL,
                                                      NULL, "pgmGuestROMWriteHandler", 0,
                                                      NULL, "pgmGuestROMWriteHandler", 0, pCur->pszDesc);
                AssertRCReturn(rc, rc);
                pCur->fWritable = false;

                rc = PGMR3PhysSetFlags(pVM, GCPhys, cbRange, 0, ~MM_RAM_FLAGS_MMIO2); /* ROM + MMIO2 -> ROM */
                AssertRCReturn(rc, rc);
                /* Don't bother with the MM page flags here because I don't think they are
                   really used beyond conflict checking at ROM, RAM, Reservation, etc. */

                REMR3NotifyPhysRomRegister(pVM, GCPhys, cbRange, pCur->pvCopy, false /* read-only now */);
            }
            return VINF_SUCCESS;
        }
    AssertMsgFailed(("GCPhys=%VGp cbRange=%#x\n", GCPhys, cbRange));
    return VERR_INVALID_PARAMETER;
}

