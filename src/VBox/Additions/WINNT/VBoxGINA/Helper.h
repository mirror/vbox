/* $Id$ */
/** @file
 * VBoxGINA - Windows Logon DLL for VirtualBox, Helper Functions.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VBoxGINA_Helper_h
#define GA_INCLUDED_SRC_WINNT_VBoxGINA_Helper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VBoxGuestLib.h>

void VBoxGINAVerbose(DWORD dwLevel, const char *pszFormat, ...);

int  VBoxGINALoadConfiguration();
bool VBoxGINAHandleCurrentSession(void);

int VBoxGINACredentialsPollerCreate(void);
int VBoxGINACredentialsPollerTerminate(void);

int VBoxGINAReportStatus(VBoxGuestFacilityStatus enmStatus);

#endif /* !GA_INCLUDED_SRC_WINNT_VBoxGINA_Helper_h */

