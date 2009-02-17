/* $Revision$ */
/** @file cbinding.h
 * Glue for dynamically linking with VBoxXPCOMC.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBoxXPCOMC_cglue_h
#define ___VBoxXPCOMC_cglue_h

#include "cbinding.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The dlopen handle for VBoxXPCOMC. */
extern void *g_hVBoxXPCOMC;
/** The last load error. */
extern char g_szVBoxErrMsg[256];
/** Pointer to the VBoxXPCOMC function table.  */
extern PCVBOXXPCOM g_pVBoxFuncs;

int VBoxCGlueInit(const char *pszMsgPrefix);
void VBoxCGlueTerm(void);


#ifdef __cplusplus
}
#endif

#endif

