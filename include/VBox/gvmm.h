/* $Id$ */
/** @file
 * GVMM - The Global VM Manager.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

#ifndef ___VBox_gvmm_h
#define ___VBox_gvmm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>

/** @defgroup grp_GVMM  GVMM - The Global VM Manager.
 * @{
 */

/** @def IN_GVMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global VM Manager or not.
 */
/** @def GVMMR0DECL
 * Ring 0 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GVMM_R0
# define GVMMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GVMMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def NIL_GVM_HANDLE
 * The nil GVM VM handle value (VM::hSelf).
 */
#define NIL_GVM_HANDLE 0

GVMMR0DECL(int)  GVMMR0Init(void);
GVMMR0DECL(void) GVMMR0Term(void);
GVMMR0DECL(int)  GVMMR0RegisterVM(PVM pVM);
GVMMR0DECL(int)  GVMMR0DeregisterVM(PVM pVM);
GVMMR0DECL(PGVM) GVMMR0ByHandle(uint32_t hGVM);
GVMMR0DECL(PVM)  GVMMR0GetVMByEMT(RTNATIVETHREAD hEMT);
GVMMR0DECL(PVM)  GVMMR0GetVMByHandle(uint32_t hGVM);


/** @} */

#endif

