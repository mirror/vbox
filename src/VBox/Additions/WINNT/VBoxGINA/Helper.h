/** @file
 *
 * VBoxGINA -- Windows Logon DLL for VirtualBox Helper Functions
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __H_GINAHELPER
#define __H_GINAHELPER

#include <iprt/thread.h>
#include <iprt/semaphore.h>

/* the credentials */
extern wchar_t g_Username[];
extern wchar_t g_Password[];
extern wchar_t g_Domain[];

void credentialsReset(void);
bool credentialsAvailable(void);
bool credentialsRetrieve(void);
bool credentialsPollerCreate(void);
bool credentialsPollerTerminate(void);

#endif /* __H_GINAHELPER */