/** @file
 * IPRT / No-CRT - fcntl.h
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

#ifndef IPRT_INCLUDED_nocrt_fcntl_h
#define IPRT_INCLUDED_nocrt_fcntl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/file.h>

#ifdef IPRT_NO_CRT_FOR_3RD_PARTY

/* Open flags:*/
AssertCompile(RT_IS_POWER_OF_TWO(RTFILE_O_OPEN_CREATE));
AssertCompile(RT_IS_POWER_OF_TWO(RTFILE_O_CREATE));
# define _O_CREAT       RTFILE_O_OPEN_CREATE
# define _O_EXCL        RTFILE_O_CREATE     /**< Will remove RTFILE_O_OPEN_CREATE when processing it. */
# define _O_TRUNC       RTFILE_O_TRUNCATE
# define _O_APPEND      RTFILE_O_APPEND
# define _O_RDONLY      RTFILE_O_READ
# define _O_WRONLY      RTFILE_O_WRITE
# define _O_RDWR        (RTFILE_O_READ | RTFILE_O_WRITE)
# define _O_CLOEXEC     RTFILE_O_INHERIT    /**< Invert meaning when processing it. */
# define _O_NOINHERIT   O_CLOEXEC
# define _O_LARGEFILE   0
# define _O_BINARY      0

# define O_CREAT        _O_CREAT
# define O_EXCL         _O_EXCL
# define O_TRUNC        _O_TRUNC
# define O_APPEND       _O_APPEND
# define O_RDONLY       _O_RDONLY
# define O_WRONLY       _O_WRONLY
# define O_RDWR         _O_RDWR
# define O_CLOEXEC      _O_CLOEXEC
# define O_NOINHERIT    _O_NOINHERIT
# define O_BINARY       _O_BINARY

RT_C_DECLS_BEGIN

int  RT_NOCRT(open)(const char *pszFilename, uint64_t fFlags, ... /*RTFMODE fMode*/);
int  RT_NOCRT(_open)(const char *pszFilename, uint64_t fFlags, ... /*RTFMODE fMode*/);

# if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
#  define open      RT_NOCRT(open)
#  define _open     RT_NOCRT(_open)
# endif

#endif /* IPRT_NO_CRT_FOR_3RD_PARTY */


RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_nocrt_fcntl_h */

