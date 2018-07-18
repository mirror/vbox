/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Guest Paging Template.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
/* r3 */
PGM_GST_DECL(int, Enter)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3);
PGM_GST_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta);
PGM_GST_DECL(int, Exit)(PVMCPU pVCpu);

/* all */
PGM_GST_DECL(int, GetPage)(PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
PGM_GST_DECL(int, ModifyPage)(PVMCPU pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGM_GST_DECL(int, GetPDE)(PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPDE);
RT_C_DECLS_END


/**
 * Enters the guest mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   GCPhysCR3   The physical address from the CR3 register.
 */
PGM_GST_DECL(int, Enter)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3)
{
    /*
     * Map and monitor CR3
     */
    int rc = PGM_BTH_PFN(MapCR3, pVCpu)(pVCpu, GCPhysCR3);
    return rc;
}


/**
 * Relocate any GC pointers related to guest mode paging.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   offDelta    The relocation offset.
 */
PGM_GST_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta)
{
    pVCpu->pgm.s.pGst32BitPdRC += offDelta;

    for (unsigned i = 0; i < RT_ELEMENTS(pVCpu->pgm.s.apGstPaePDsRC); i++)
    {
        pVCpu->pgm.s.apGstPaePDsRC[i] += offDelta;
    }
    pVCpu->pgm.s.pGstPaePdptRC += offDelta;

    return VINF_SUCCESS;
}


/**
 * Exits the guest mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
PGM_GST_DECL(int, Exit)(PVMCPU pVCpu)
{
    int rc;

    rc = PGM_BTH_PFN(UnmapCR3, pVCpu)(pVCpu);
    return rc;
}

