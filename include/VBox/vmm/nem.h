/** @file
 * NEM - The Native Execution Manager.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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

#ifndef ___VBox_vmm_nem_h
#define ___VBox_vmm_nem_h

#include <VBox/types.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/pgm.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_nem      The Native Execution Manager API
 * @ingroup grp_vmm
 * @{
 */

/** @defgroup grp_nem_r3   The NEM ring-3 Context API
 * @{
 */
VMMR3_INT_DECL(int)  NEMR3InitConfig(PVM pVM);
VMMR3_INT_DECL(int)  NEMR3Init(PVM pVM, bool fFallback, bool fForced);
VMMR3_INT_DECL(int)  NEMR3InitAfterCPUM(PVM pVM);
#ifdef IN_RING3
VMMR3_INT_DECL(int)  NEMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
#endif
VMMR3_INT_DECL(int)  NEMR3Term(PVM pVM);
VMMR3_INT_DECL(void) NEMR3Reset(PVM pVM);
VMMR3_INT_DECL(void) NEMR3ResetCpu(PVMCPU pVCpu, bool fInitIpi);
VMMR3_INT_DECL(VBOXSTRICTRC) NEMR3RunGC(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR3_INT_DECL(bool) NEMR3SetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable);
VMMR3_INT_DECL(void) NEMR3NotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags);

VMMR3_INT_DECL(int)  NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb);
VMMR3_INT_DECL(int)  NEMR3NotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2);
VMMR3_INT_DECL(int)  NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
/** @name Flags for NEMR3NotifyPhysMmioExMap and NEMR3NotifyPhysMmioExUnmap.
 * @{ */
/** Set if it's MMIO2 being mapped or unmapped. */
#define NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2     RT_BIT(0)
/** Set if the range is replacing RAM rather that unused space. */
#define NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE   RT_BIT(1)
/** @} */

VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
/** @name Flags for NEMR3NotifyPhysRomRegisterEarly and NEMR3NotifyPhysRomRegisterLate.
 * @{ */
/** Set if the range is replacing RAM rather that unused space. */
#define NEM_NOTIFY_PHYS_ROM_F_REPLACE       RT_BIT(1)
/** Set if it's MMIO2 being mapped or unmapped. */
#define NEM_NOTIFY_PHYS_ROM_F_SHADOW        RT_BIT(2)
/** @} */

VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled);
/** @} */


/** @defgroup grp_nem_r0    The NEM ring-0 Context API
 * @{  */
VMMR0_INT_DECL(int)  NEMR0InitVM(PGVM pGVM, PVM pVM);
VMMR0_INT_DECL(int)  NEMR0InitVMPart2(PGVM pGVM, PVM pVM);
VMMR0_INT_DECL(void) NEMR0CleanupVM(PGVM pGVM);
VMMR0_INT_DECL(int)  NEMR0MapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  NEMR0UnmapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  NEMR0ExportState(PGVM pGVM, PVM pVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  NEMR0ImportState(PGVM pGVM, PVM pVM, VMCPUID idCpu, uint64_t fWhat);
/** @} */


/** @defgroup grp_nem_hc    The NEM Host Context API
 * @{
 */
VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb);
VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        int fRestoreAsRAM, bool fRestoreAsRAM2);
VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                                    RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM);

VMM_INT_DECL(int)  NEMHCNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                PGMPAGETYPE enmType, uint8_t *pu2State);
VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State);
VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State);
/** @name NEM_PAGE_PROT_XXX - Page protection
 * @{ */
#define NEM_PAGE_PROT_NONE      UINT32_C(0)     /**< All access causes VM exits. */
#define NEM_PAGE_PROT_READ      RT_BIT(0)       /**< Read access. */
#define NEM_PAGE_PROT_EXECUTE   RT_BIT(1)       /**< Execute access. */
#define NEM_PAGE_PROT_WRITE     RT_BIT(2)       /**< write access. */
/** @} */

/** @} */

/** @} */
RT_C_DECLS_END


#endif

