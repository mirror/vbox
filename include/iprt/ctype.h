/** @file
 * InnoTek Portable Runtime - ctype.h wrapper.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

#ifndef __iprt_ctype_h__
#define __iprt_ctype_h__

#include <ctype.h>

#if defined(_MSC_VER) && !defined(isblank)
/* isblank for MSC */
# define isblank(ch)  ( (ch) == ' ' || (ch) == '\t' )
#endif

#endif

