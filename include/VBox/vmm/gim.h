/** @file
 * GIM - Guest Interface Manager.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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

#ifndef ___VBox_vmm_gim_h
#define ___VBox_vmm_gim_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>

#include <VBox/vmm/cpum.h>

/** The value used to specify that VirtualBox must use the newest
 *  implementation version of the GIM provider. */
#define GIM_VERSION_LATEST                  UINT32_C(0)

RT_C_DECLS_BEGIN

/** @defgroup grp_gim   The Guest Interface Manager API
 * @{
 */

/**
 * GIM Provider Identifiers.
 * @remarks Part of saved state!
 */
typedef enum GIMPROVIDERID
{
    /** None. */
    GIMPROVIDERID_NONE = 0,
    /** Minimal. */
    GIMPROVIDERID_MINIMAL,
    /** Microsoft Hyper-V. */
    GIMPROVIDERID_HYPERV,
    /** Linux KVM Interface. */
    GIMPROVIDERID_KVM
} GIMPROVIDERID;
AssertCompileSize(GIMPROVIDERID, sizeof(uint32_t));


/**
 * A GIM MMIO2 region record.
 */
typedef struct GIMMMIO2REGION
{
    /** The region index. */
    uint8_t             iRegion;
    /** Whether an RC mapping is required. */
    bool                fRCMapping;
    /** Whether this region has been registered. */
    bool                fRegistered;
    /** Whether this region is currently mapped. */
    bool                fMapped;
    /** Alignment padding. */
    uint8_t             au8Alignment0[3];
    /** Size of the region (must be page aligned). */
    uint32_t            cbRegion;
    /** Alignment padding. */
    uint32_t            u32Alignment0;
    /** The host ring-0 address of the first page in the region. */
    R0PTRTYPE(void *)   pvPageR0;
    /** The host ring-3 address of the first page in the region. */
    R3PTRTYPE(void *)   pvPageR3;
    /** The ring-context address of the first page in the region. */
    RCPTRTYPE(void *)   pvPageRC;
    /** The guest-physical address of the first page in the region. */
    RTGCPHYS            GCPhysPage;
    /** The description of the region. */
    char                szDescription[32];
} GIMMMIO2REGION;
/** Pointer to a GIM MMIO2 region. */
typedef GIMMMIO2REGION *PGIMMMIO2REGION;
/** Pointer to a const GIM MMIO2 region. */
typedef GIMMMIO2REGION const *PCGIMMMIO2REGION;
AssertCompileMemberAlignment(GIMMMIO2REGION, cbRegion, 8);
AssertCompileMemberAlignment(GIMMMIO2REGION, pvPageR0, 8);

#if 0
/**
 * A GIM Hypercall handler.
 *
 * @param   pVM             Pointer to the VMCPU.
 * @param   pCtx            Pointer to the guest-CPU context.
 */
typedef DECLCALLBACK(int) FNGIMHYPERCALL(PVMCPU pVCpu, PCPUMCTX pCtx);
/** Pointer to a GIM hypercall handler. */
typedef FNGIMHYPERCALL *PFNGIMHYPERCALL;

/**
 * A GIM MSR-read handler.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VMCPU.
 * @param   idMsr           The MSR being read.
 * @param   pRange          The range that the MSR belongs to.
 * @param   puValue         Where to store the value of the MSR.
 */
typedef DECLCALLBACK(int) FNGIMRDMSR(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue);
/** Pointer to a GIM MSR-read handler. */
typedef FNGIMRDMSR *PFNGIMRDMSR;

/**
 * A GIM MSR-write handler.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VMCPU.
 * @param   idMsr           The MSR being written.
 * @param   pRange          The range that the MSR belongs to.
 * @param   uValue          The value to set, ignored bits masked.
 * @param   uRawValue       The raw value with the ignored bits not masked.
 */
typedef DECLCALLBACK(int) FNGIMWRMSR(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue);
/** Pointer to a GIM MSR-write handler. */
typedef FNGIMWRMSR *PFNGIMWRMSR;
#endif


#ifdef IN_RC
/** @defgroup grp_gim_rc  The GIM Raw-mode Context API
 * @{
 */
/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_gim_r0  The GIM Host Context Ring-0 API
 * @{
 */
VMMR0_INT_DECL(int)         GIMR0InitVM(PVM pVM);
VMMR0_INT_DECL(int)         GIMR0TermVM(PVM pVM);
VMMR0_INT_DECL(int)         GIMR0UpdateParavirtTsc(PVM pVM, uint64_t u64Offset);
/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_gim_r3  The GIM Host Context Ring-3 API
 * @{
 */
VMMR3_INT_DECL(int)         GIMR3Init(PVM pVM);
VMMR3_INT_DECL(int)         GIMR3InitCompleted(PVM pVM);
VMMR3_INT_DECL(int)         GIMR3Term(PVM pVM);
VMMR3_INT_DECL(void)        GIMR3Reset(PVM pVM);
VMMR3DECL(void)             GIMR3GimDeviceRegister(PVM pVM, PPDMDEVINS pDevIns);
VMMR3DECL(PGIMMMIO2REGION)  GIMR3GetMmio2Regions(PVM pVM, uint32_t *pcRegions);
/** @} */
#endif /* IN_RING3 */

VMMDECL(bool)               GIMIsEnabled(PVM pVM);
VMMDECL(GIMPROVIDERID)      GIMGetProvider(PVM pVM);
VMM_INT_DECL(bool)          GIMIsParavirtTscEnabled(PVM pVM);
VMM_INT_DECL(bool)          GIMAreHypercallsEnabled(PVMCPU pVCpu);
VMM_INT_DECL(int)           GIMHypercall(PVMCPU pVCpu, PCPUMCTX pCtx);
VMM_INT_DECL(int)           GIMXcptUD(PVMCPU pVCpu, PCPUMCTX pCtx, PDISCPUSTATE pDis);
VMM_INT_DECL(bool)          GIMShouldTrapXcptUD(PVMCPU pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  GIMReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue);
VMM_INT_DECL(VBOXSTRICTRC)  GIMWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue);
/** @} */

RT_C_DECLS_END

#endif

