/* $Id$ */
/** @file
 * GVM - Global VM Manager.
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

#ifndef ___VBox_gvm_h
#define ___VBox_gvm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>

/** @defgroup grp_GVM   GVM - Global VM Manager.
 * @{
 */

/** @def IN_GVM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global VM Manager or not.
 */
/** @def GVMR0DECL
 * Ring 0 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GVM_R0
# define GVMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GVMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def NIL_GVM_HANDLE
 * The nil GVM VM handle value (VM::hSelf).
 */
#define NIL_GVM_HANDLE 0

GVMR0DECL(int)  GVMR0Init(void);
GVMR0DECL(void) GVMR0Term(void);
GVMR0DECL(int)  GVMR0RegisterVM(PVM pVM);
GVMR0DECL(int)  GVMR0DeregisterVM(PVM pVM);
GVMR0DECL(PVM)  GVMR0ByEMT(RTNATIVETHREAD hEMT);
GVMR0DECL(PVM)  GVMR0ByHandle(uint32_t hGVM);


/** @} */

#endif 
