/** @file
 * innotek Portable Runtime / No-CRT - compiler specifics.
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
 *
 */

#ifndef ___iprt_nocrt_compiler_compiler_h
#define ___iprt_nocrt_compiler_compiler_h

#ifdef __GNUC__
# include <iprt/nocrt/compiler/gcc.h>
#elif defined(_MSC_VER)
# include <iprt/nocrt/compiler/msc.h>
#else
# error "Unsupported compiler."
#endif

#endif
