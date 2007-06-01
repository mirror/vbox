/** @file
 * innotek Portable Runtime / No-CRT - Our own setjmp header.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __setjmp_h__
#define __setjmp_h__

#include <iprt/types.h>

__BEGIN_DECLS

#ifdef __AMD64__
typedef uint64_t RT_NOCRT(jmp_buf)[8];
#else
typedef uint32_t RT_NOCRT(jmp_buf)[6+2];
#endif

extern int RT_NOCRT(setjmp)(RT_NOCRT(jmp_buf));
extern int RT_NOCRT(longjmp)(RT_NOCRT(jmp_buf), int);

#ifndef RT_WITHOUT_NOCRT_WRAPPERS
# define jmp_buf RT_NOCRT(jmp_buf)
# define setjmp RT_NOCRT(setjmp)
# define longjmp RT_NOCRT(longjmp)
#endif

__END_DECLS

#endif

