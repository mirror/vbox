/* $Id$ */
/** @file
 * VirtualBox Support Library - Internal header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___SupInternal_h___
#define ___SupInternal_h___

#include <VBox/cdefs.h>
#include <VBox/types.h>



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The negotiated interrupt number. */
extern uint8_t          g_uchInterruptNo;
/** The negotiated cookie. */
extern uint32_t         g_u32Cookie;
/** The negotiated cookie. */
extern uint32_t         g_u32CookieSession;



/*******************************************************************************
*   OS Specific Function                                                       *
*******************************************************************************/
__BEGIN_DECLS
int     suplibOsInstall(void);
int     suplibOsUninstall(void);
int     suplibOsInit(size_t cbReserve);
int     suplibOsTerm(void);
int     suplibOsIOCtl(uintptr_t uFunction, void *pvReq, size_t cbReq);
int     suplibOsIOCtlFast(uintptr_t uFunction);
int     suplibOsPageAlloc(size_t cPages, void **ppvPages);
int     suplibOsPageFree(void *pvPages, size_t cPages);
__END_DECLS


#endif

