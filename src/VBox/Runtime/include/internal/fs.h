/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Internal RTFs header.
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

#ifndef __fs_h__
#define __fs_h__

#include <iprt/types.h>
#ifndef __WIN__
# include <sys/stat.h>
#endif

__BEGIN_DECLS

RTFMODE rtFsModeFromDos(RTFMODE fMode, const char *pszName, unsigned cbName);
RTFMODE rtFsModeFromUnix(RTFMODE fMode, const char *pszName, unsigned cbName);
RTFMODE rtFsModeNormalize(RTFMODE fMode, const char *pszName, unsigned cbName);
bool    rtFsModeIsValid(RTFMODE fMode);
bool    rtFsModeIsValidPermissions(RTFMODE fMode);

size_t  rtPathVolumeSpecLen(const char *pszPath);
#ifndef __WIN__
void    rtFsConvertStatToObjInfo(PRTFSOBJINFO pObjInfo, const struct stat *pStat, const char *pszName, unsigned cbName);
#endif

#ifdef __LINUX__
# ifdef __USE_MISC
#  define HAVE_STAT_TIMESPEC_BRIEF
# else
#  define HAVE_STAT_NSEC
# endif
#endif

__END_DECLS

#endif
