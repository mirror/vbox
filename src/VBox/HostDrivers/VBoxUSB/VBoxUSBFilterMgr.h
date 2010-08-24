/* $Id$ */
/** @file
 * VirtualBox Ring-0 USB Filter Manager.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
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
