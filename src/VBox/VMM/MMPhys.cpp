/** @file
 *
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
        AssertReturn(ALIGNP(pvRam, PAGE_SIZE) == pvRam, VERR_INVALID_PARAMETER);
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
                pVM->mm.s.cbRAMSize += cb;

            REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, pvRam, fFlags);
            return rc;
        }
    }
    else
    {
        /*
         * Lock the memory. (fully allocated by caller)
         */
        PMMLOCKEDMEM    pLockedMem;
        rc = mmr3LockMem(pVM, pvRam, cb, MM_LOCKED_TYPE_PHYS, &pLockedMem);
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
                        pVM->mm.s.cbRAMSize += cb;

                    REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, pvRam, fFlags);
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
        REMR3NotifyPhysRamRegister(pVM, GCPhysNew, cb, pCur->pv,
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
 * @param   pszDesc             Pointer to description string. This must not be freed.
 * @remark  There is no way to remove the rom, automatically on device cleanup or
 *          manually from the device yet. At present I doubt we need such features...
 */
MMR3DECL(int) MMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const void *pvBinary, const char *pszDesc)
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

    /** @note we rely on the MM_RAM_FLAGS_ROM flag in PGMPhysRead now. Don't change to reserved! */
    /** @todo r=bird: Noone ever talked about changing *to* _RESERVED. The question is whether
     * we should *clear* _RESERVED. I've no idea what the state of that flag is for ROM areas right
     * now, but I will find out later. */
    for (; iPage < iPageEnd; iPage++)
        pCur->aPhysPages[iPage].Phys |= MM_RAM_FLAGS_ROM; /** @todo should be clearing _RESERVED? */
    int rc = PGMR3PhysSetFlags(pVM, GCPhys, cbRange, MM_RAM_FLAGS_ROM, ~0); /** @todo should be clearing _RESERVED? */
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Prevent changes to the ROM memory when executing in raw mode by
         * registering a GC only write access handler.
         *
         * ASSUMES that REMR3NotifyPhysRomRegister doesn't call cpu_register_physical_memory
         * when there is no HC handler. The result would probably be immediate boot failure.
         */
        rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhys + cbRange - 1,
                                          NULL, NULL,
                                          NULL, "pgmGuestROMWriteHandler", 0,
                                          NULL, "pgmGuestROMWriteHandler", 0, "ROM Write Access Handler");
        AssertRC(rc);
    }

    REMR3NotifyPhysRomRegister(pVM, GCPhys, cbRange, pvCopy);
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
                SUPPageFree(pvPages);
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
 * @returns
 * @param   pVM
 * @thread  Any.
 */
MMR3DECL(uint64_t) MMR3PhysGetRamSize(PVM pVM)
{
    return pVM->mm.s.cbRamBase;
}

