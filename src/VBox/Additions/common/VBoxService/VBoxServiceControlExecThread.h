/* $Id$ */
/** @file
 * VBoxServiceControlExecThread - Thread for an executed guest process.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxServiceControlExecThread_h
#define ___VBoxServiceControlExecThread_h

#include "VBoxServiceInternal.h"

void VBoxServiceControlExecThreadDestroy(PVBOXSERVICECTRLTHREAD pThread);
int VBoxServiceControlExecThreadPerform(uint32_t uPID, PVBOXSERVICECTRLREQUEST pRequest);
int VBoxServiceControlExecThreadShutdown(const PVBOXSERVICECTRLTHREAD pThread);

#endif  /* !___VBoxServiceControlExecThread_h */

