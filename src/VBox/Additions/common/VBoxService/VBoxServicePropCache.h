/* $Id: */
/** @file
 * VBoxServicePropCache - Guest property cache.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxServicePropCache_h
#define ___VBoxServicePropCache_h

#include "VBoxServiceInternal.h"

#ifdef VBOX_WITH_GUEST_PROPS

#define VBOXSERVICEPROPCACHEFLAG_TEMPORARY          RT_BIT(1)
#define VBOXSERVICEPROPCACHEFLAG_ALWAYS_UPDATE      RT_BIT(2)

int VBoxServicePropCacheInit(PVBOXSERVICEVEPROPCACHE pCache, uint32_t uClientId);
PVBOXSERVICEVEPROPCACHEENTRY VBoxServicePropCacheFind(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t uFlags);
int VBoxServicePropCacheUpdateEntry(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags, const char *pszValueReset);
int VBoxServicePropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...);
int VBoxServicePropCacheUpdateEx(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags, const char *pszValueReset, const char *pszValueFormat, ...);
void VBoxServicePropCacheDestroy(PVBOXSERVICEVEPROPCACHE pCache);
#endif

#endif

