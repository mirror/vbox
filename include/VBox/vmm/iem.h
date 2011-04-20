/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___VBox_vmm_iem_h
#define ___VBox_vmm_iem_h

#include <VBox/types.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_iem       The Interpreted Execution Manager API.
 * @{
 */



VMMDECL(VBOXSTRICTRC) IEMExecOne(PVMCPU pVCpu);



/** @defgroup grp_em_r3     The IEM Host Context Ring-3 API.
 * @ingroup grp_em
 * @{
 */
VMMR3DECL(int)      IEMR3Init(PVM pVM);
VMMR3DECL(int)      IEMR3Term(PVM pVM);
VMMR3DECL(void)     IEMR3Relocate(PVM pVM);
/** @} */

/** @} */

RT_C_DECLS_END

#endif

