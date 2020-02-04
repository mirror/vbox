/* $Id$ */
/** @file
 * VBoxServiceVMInfo.h - Internal VM info definitions.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_common_VBoxService_VBoxServiceVMInfo_h
#define GA_INCLUDED_SRC_common_VBoxService_VBoxServiceVMInfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


extern int VGSvcUserUpdateF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                            const char *pszKey, const char *pszValueFormat, ...);


extern uint32_t g_uVMInfoUserIdleThresholdMS;

#endif /* !GA_INCLUDED_SRC_common_VBoxService_VBoxServiceVMInfo_h */

