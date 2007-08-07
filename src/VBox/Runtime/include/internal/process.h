/* $Id$ */
/** @file
 * innotek Portable Runtime - Internal RTProc header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___internal_process_h
#define ___internal_process_h

#include <iprt/process.h>

__BEGIN_DECLS

extern RTPROCESS        g_ProcessSelf;
extern RTPROCPRIORITY   g_enmProcessPriority;

/**
 * Validates and sets the process priority.
 * This will check that all rtThreadNativeSetPriority() will success for all the
 * thread types when applied to the current thread.
 *
 * @returns iprt status code.
 * @param   enmPriority     The priority to validate and set.
 * @remark  Located in sched.
 */
int rtProcNativeSetPriority(RTPROCPRIORITY enmPriority);

__END_DECLS

#endif

