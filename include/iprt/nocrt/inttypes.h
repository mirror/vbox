/** @file
 * innotek Portable Runtime / No-CRT - Our minimal inttypes.h.
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

#ifndef ___iprt_nocrt_inttypes_h
#define ___iprt_nocrt_inttypes_h

#include <iprt/types.h>

#define PRId64 "RI64"
#define PRIx64 "RX64"
#define PRIu64 "RU64"
#define PRIo64 huh? anyone using this? great!

#endif

