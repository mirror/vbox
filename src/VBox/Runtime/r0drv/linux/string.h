/* $Id$ */
/** @file
 * innotek Portable Runtime - wrapper for the linux kernel asm/string.h.
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

#ifndef ___string_h
#define ___string_h

#include <iprt/cdefs.h>

__BEGIN_DECLS
#ifndef bool /* Linux 2.6.19 C++ nightmare */
#define bool bool_type
#define true true_type
#define false false_type
#define _Bool int
#define bool_type_r0drv_string_h__
#endif
#include <linux/types.h>
#include <linux/string.h>
#ifdef bool_type_r0drv_string_h__
#undef bool
#undef true
#undef false
#undef bool_type_r0drv_string_h__
#endif
char *strpbrk(const char *pszStr, const char *pszChars)
#if defined(__THROW)
    __THROW
#endif
    ;

__END_DECLS

#endif

