/** @file
 * innotek Portable Runtime / No-CRT - fenv.h wrapper.
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

#ifndef __iprt_nocrt_fenv_h__
#define __iprt_nocrt_fenv_h__

#include <iprt/cdefs.h>
#ifdef __AMD64__
# include <iprt/nocrt/amd64/fenv.h>
#elif defined(__X86__)
# include <iprt/nocrt/x86/fenv.h>
#else
# error "IPRT: no fenv.h available for this platform, or the platform define is missing!"
#endif

#endif
