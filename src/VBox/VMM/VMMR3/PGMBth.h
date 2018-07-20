/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Shadow+Guest Paging Template.
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
PGM_BTH_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta);

PGM_BTH_DECL(int, Enter)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3);
PGM_BTH_DECL(int, Trap0eHandler)(PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, bool *pfLockTaken);
PGM_BTH_DECL(int, SyncCR3)(PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal);
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVMCPU pVCpu, RTGCPTR Addr, unsigned fPage, unsigned uError);
PGM_BTH_DECL(int, InvalidatePage)(PVMCPU pVCpu, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, PrefetchPage)(PVMCPU pVCpu, RTGCPTR GCPtrPage);
PGM_BTH_DECL(unsigned, AssertCR3)(PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr = 0, RTGCPTR cb = ~(RTGCPTR)0);
PGM_BTH_DECL(int, MapCR3)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3);
PGM_BTH_DECL(int, UnmapCR3)(PVMCPU pVCpu);
RT_C_DECLS_END


/**
 * Relocate any GC pointers related to shadow mode paging.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   offDelta    The relocation offset.
 */
PGM_BTH_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta)
{
    /* nothing special to do here - InitData does the job. */
    NOREF(pVCpu); NOREF(offDelta);
    return VINF_SUCCESS;
}

