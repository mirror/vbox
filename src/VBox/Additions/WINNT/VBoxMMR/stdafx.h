/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#pragma once

// Windows Header Files:
#include <WinSock2.h>
#include <windows.h>
#include <pchannel.h>
#include <Wmistr.h>     // WNODE_HEADER
#include <Evntrace.h>   // RegisterTraceGuids, etc.

#include <list>
#include <map>
#include <queue>
