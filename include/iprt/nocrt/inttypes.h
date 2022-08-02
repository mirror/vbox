/** @file
 * IPRT / No-CRT - Our minimal inttypes.h.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef IPRT_INCLUDED_nocrt_inttypes_h
#define IPRT_INCLUDED_nocrt_inttypes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

#define PRId8  "RI8"
#define PRIi8  "RI8"
#define PRIx8  "RX8"
#define PRIu8  "RU8"
#define PRIo8  huh? anyone using this? great!

#define PRId16 "RI16"
#define PRIi16 "RI16"
#define PRIx16 "RX16"
#define PRIu16 "RU16"
#define PRIo16 huh? anyone using this? great!

#define PRId32 "RI32"
#define PRIi32 "RI32"
#define PRIx32 "RX32"
#define PRIu32 "RU32"
#define PRIo32 huh? anyone using this? great!

#define PRId64 "RI64"
#define PRIi64 "RI64"
#define PRIx64 "RX64"
#define PRIu64 "RU64"
#define PRIo64 huh? anyone using this? great!

#define PRIdMAX "RI64"
#define PRIiMAX "RI64"
#define PRIxMAX "RX64"
#define PRIuMAX "RU64"
#define PRIoMAX huh? anyone using this? great!

#endif /* !IPRT_INCLUDED_nocrt_inttypes_h */

