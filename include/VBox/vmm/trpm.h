/** @file
 * TRPM - The Trap Monitor.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_vmm_trpm_h
#define VBOX_INCLUDED_vmm_trpm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <iprt/x86.h>


RT_C_DECLS_BEGIN
/** @defgroup grp_trpm The Trap Monitor API
 * @ingroup grp_vmm
 * @{
 */

/**
 * TRPM event type.
 */
typedef enum
{
    TRPM_TRAP         = 0,
    TRPM_HARDWARE_INT = 1,
    TRPM_SOFTWARE_INT = 2,
    /** The usual 32-bit paranoia. */
    TRPM_32BIT_HACK   = 0x7fffffff
} TRPMEVENT;
/** Pointer to a TRPM event type. */
typedef TRPMEVENT *PTRPMEVENT;
/** Pointer to a const TRPM event type. */
typedef TRPMEVENT const *PCTRPMEVENT;

VMMDECL(int)        TRPMQueryTrap(PVMCPU pVCpu, uint8_t *pu8TrapNo, PTRPMEVENT penmType);
VMMDECL(uint8_t)    TRPMGetTrapNo(PVMCPU pVCpu);
VMMDECL(uint32_t)   TRPMGetErrorCode(PVMCPU pVCpu);
VMMDECL(RTGCUINTPTR) TRPMGetFaultAddress(PVMCPU pVCpu);
VMMDECL(uint8_t)    TRPMGetInstrLength(PVMCPU pVCpu);
VMMDECL(bool)       TRPMIsTrapDueToIcebp(PVMCPU pVCpu);
VMMDECL(int)        TRPMResetTrap(PVMCPU pVCpu);
VMMDECL(int)        TRPMAssertTrap(PVMCPUCC pVCpu, uint8_t u8TrapNo, TRPMEVENT enmType);
VMMDECL(int)        TRPMAssertXcptPF(PVMCPUCC pVCpu, RTGCUINTPTR uCR2, uint32_t uErrorCode);
VMMDECL(void)       TRPMSetErrorCode(PVMCPU pVCpu, uint32_t uErrorCode);
VMMDECL(void)       TRPMSetFaultAddress(PVMCPU pVCpu, RTGCUINTPTR uCR2);
VMMDECL(void)       TRPMSetInstrLength(PVMCPU pVCpu, uint8_t cbInstr);
VMMDECL(void)       TRPMSetTrapDueToIcebp(PVMCPU pVCpu);
VMMDECL(bool)       TRPMIsSoftwareInterrupt(PVMCPU pVCpu);
VMMDECL(bool)       TRPMHasTrap(PVMCPU pVCpu);
VMMDECL(int)        TRPMQueryTrapAll(PVMCPU pVCpu, uint8_t *pu8TrapNo, PTRPMEVENT pEnmType, uint32_t *puErrorCode,
                                     PRTGCUINTPTR puCR2, uint8_t *pcbInstr, bool *pfIcebp);

#ifdef IN_RING3
/** @defgroup grp_trpm_r3   TRPM Host Context Ring 3 API
 * @{
 */
VMMR3DECL(int)      TRPMR3Init(PVM pVM);
VMMR3DECL(void)     TRPMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(void)     TRPMR3ResetCpu(PVMCPU pVCpu);
VMMR3DECL(void)     TRPMR3Reset(PVM pVM);
VMMR3DECL(int)      TRPMR3Term(PVM pVM);
VMMR3DECL(int)      TRPMR3InjectEvent(PVM pVM, PVMCPU pVCpu, TRPMEVENT enmEvent, bool *pfInjected);
/** @} */
#endif

/** @} */
RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_trpm_h */
