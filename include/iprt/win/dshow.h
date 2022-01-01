/** @file
 * Safe way to include dshow.h.
 */

/*
 * Copyright (C) 2016-2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_win_dshow_h
#define IPRT_INCLUDED_win_dshow_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef _MSC_VER
# pragma warning(push)
# if _MSC_VER >= 1920 /*RT_MSC_VER_VC142*/
#  pragma warning(disable:5204) /* strmif.h(16270): warning C5204: 'IAMFilterGraphCallback': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly */
# endif
#endif

#include <dshow.h>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif /* !IPRT_INCLUDED_win_dshow_h */

