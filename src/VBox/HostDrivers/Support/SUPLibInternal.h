/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Shared code:
 * Internal header for support library
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __SupInternal_h__
#define __SupInternal_h__

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
int     suplibOsIOCtl(unsigned uFunction, void *pvIn, size_t cbIn, void *pvOut, size_t cbOut);
#ifdef VBOX_WITHOUT_IDT_PATCHING
int     suplibOSIOCtlFast(unsigned uFunction);
#endif
int     suplibOsPageAlloc(size_t cPages, void **ppvPages);
int     suplibOsPageFree(void *pvPages);
__END_DECLS


#endif

