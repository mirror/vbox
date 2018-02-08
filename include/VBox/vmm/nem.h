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


RT_C_DECLS_BEGIN

/** @defgroup grp_nem      The Native Execution Manager API
 * @ingroup grp_vmm
 * @{
 */

/** @defgroup grp_hm_r3    The HM ring-3 Context API
 * @{
 */
VMMR3_INT_DECL(int)  NEMR3InitConfig(PVM pVM);
VMMR3_INT_DECL(int)  NEMR3Init(PVM pVM, bool fFallback, bool fHMForced);
#ifdef IN_RING3
VMMR3_INT_DECL(int)  NEMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
#endif
VMMR3_INT_DECL(int)  NEMR3Term(PVM pVM);
VMMR3_INT_DECL(void) NEMR3Reset(PVM pVM);
VMMR3_INT_DECL(void) NEMR3ResetCpu(PVMCPU pVCpu);
/** @} */

/** @} */
RT_C_DECLS_END


#endif

