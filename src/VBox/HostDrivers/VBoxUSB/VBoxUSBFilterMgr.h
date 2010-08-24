/* $Id$ */
/** @file
 * VirtualBox Ring-0 USB Filter Manager.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxUSBFilterMgr_h
#define ___VBoxUSBFilterMgr_h

#include <VBox/usbfilter.h>

RT_C_DECLS_BEGIN

int     VBoxUSBFilterInit(void);
void    VBoxUSBFilterTerm(void);
void    VBoxUSBFilterRemoveOwner(RTPROCESS Owner);
int     VBoxUSBFilterAdd(PCUSBFILTER pFilter, RTPROCESS Owner, uintptr_t *puId);
int     VBoxUSBFilterRemove(RTPROCESS Owner, uintptr_t uId);
RTPROCESS VBoxUSBFilterMatch(PCUSBFILTER pDevice, uintptr_t *puId);

RT_C_DECLS_END

#endif
