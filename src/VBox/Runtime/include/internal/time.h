/* $Id$ */
/** @file
 * innotek Portable Runtime - Internal RTTime header
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

#ifndef ___internal_time_h
#define ___internal_time_h

#include <iprt/types.h>

__BEGIN_DECLS

#if defined(IN_RING3) || defined(IN_GC)

extern uint64_t g_u64ProgramStartNanoTS;
extern uint64_t g_u64ProgramStartMicroTS;
extern uint64_t g_u64ProgramStartMilliTS;

#endif

__END_DECLS

#endif
