/** @file
 * GIM - Guest Interface Manager.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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

RT_C_DECLS_BEGIN

/** @defgroup grp_gim   The Guest Interface Manager API
 * @{
 */

/**
 * Providers identifiers.
 */
typedef enum GIMPROVIDER
{
    /** None. */
    GIMPROVIDER_NONE = 0,
    /** Minimal. */
    GIMPROVIDER_MINIMAL,
    /** Microsoft Hyper-V. */
    GIMPROVIDER_HYPERV,
    /** Linux KVM Interface. */
    GIMPROVIDER_KVM,
    /** Ensure 32-bit type. */
    GIMPROVIDER_32BIT_HACK = 0x7fffffff
} GIMPROVIDER;


/**
 * A GIM Hypercall handler.
 *
 * @param   pVM             Pointer to the VMCPU.
 * @param   pCtx            Pointer to the guest-CPU context.
 */
typedef DECLCALLBACK(int) FNGIMHYPERCALL(PVMCPU pVCpu, PCPUMCTX pCtx);
/** Pointer to a GIM hypercall handler. */
typedef FNGIMHYPERCALL *PFNGIMHYPERCALL;

#ifdef IN_RC
/** @defgroup grp_gim_rc  The GIM Raw-mode Context API
 * @ingroup grp_gim
 * @{
 */
/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_gim_r0  The GIM Host Context Ring-0 API
 * @ingroup grp_gim
 * @{
 */

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_gim_r3  The GIM Host Context Ring-3 API
 * @ingroup grp_gim
 * @{
 */
VMMR3_INT_DECL(int)             GIMR3Init(PVM pVM);
VMMR3_INT_DECL(int)             GIMR3Term(PVM pVM);
/** @} */
#endif /* IN_RING3 */


/**
 * Checks whether GIM is being used by this VM.
 *
 * @retval  @c true if used.
 * @retval  @c false if no GIM provider ("none") is used.
 *
 * @param   pVM       Pointer to the VM.
 * @internal
 */
VMMDECL(bool)                   GIMIsEnabled(PVM pVM);

/** @} */

RT_C_DECLS_END

#endif

