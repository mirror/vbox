/** @file
 * innotek Portable Runtime / No-CRT - string.h.
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


#ifndef __iprt_nocrt_string_h__
#define __iprt_nocrt_string_h__

#include <iprt/types.h>

__BEGIN_DECLS

void *  RT_NOCRT(memchr)(const void *pv, int ch, size_t cb);
int     RT_NOCRT(memcmp)(const void *pv1, const void *pv2, size_t cb);
void *  RT_NOCRT(memcpy)(void *pvDst, const void *pvSrc, size_t cb);
void *  RT_NOCRT(memmove)(void *pvDst, const void *pvSrc, size_t cb);
void *  RT_NOCRT(memset)(void *pvDst, int ch, size_t cb);

char *  RT_NOCRT(strcat)(char *pszDst, const char *pszSrc);
char *  RT_NOCRT(strncat)(char *pszDst, const char *pszSrc, size_t cch);
char *  RT_NOCRT(strchr)(const char *psz, int ch);
int     RT_NOCRT(strcmp)(char *psz1, const char *psz2);
int     RT_NOCRT(strncmp)(char *psz1, const char *psz2, size_t cch);
int     RT_NOCRT(stricmp)(char *psz1, const char *psz2);
int     RT_NOCRT(strnicmp)(char *psz1, const char *psz2, size_t cch);
char *  RT_NOCRT(strcpy)(char *pszDst, const char *pszSrc);
char *  RT_NOCRT(strncpy)(char *pszDst, const char *pszSrc, size_t cch);
char *  RT_NOCRT(strcat)(char *pszDst, const char *pszSrc);
char *  RT_NOCRT(strncat)(char *pszDst, const char *pszSrc, size_t cch);
size_t  RT_NOCRT(strlen)(const char *psz);
size_t  RT_NOCRT(strnlen)(const char *psz, size_t cch);
char *  RT_NOCRT(strstr)(const char *psz, const char *pszSub);

#ifndef RT_WITHOUT_NOCRT_WRAPPERS
# define memchr   RT_NOCRT(memchr)
# define memcmp   RT_NOCRT(memcmp)
# define memcpy   RT_NOCRT(memcpy)
# define memmove  RT_NOCRT(memmove)
# define memset   RT_NOCRT(memset)
# define strcat   RT_NOCRT(strcat)
# define strncat  RT_NOCRT(strncat)
# define strchr   RT_NOCRT(strchr)
# define strcmp   RT_NOCRT(strcmp)
# define strncmp  RT_NOCRT(strncmp)
# define stricmp  RT_NOCRT(stricmp)
# define strnicmp RT_NOCRT(strnicmp)
# define strcpy   RT_NOCRT(strcpy)
# define strncpy  RT_NOCRT(strncpy)
# define strcat   RT_NOCRT(strcat)
# define strncat  RT_NOCRT(strncat)
# define strlen   RT_NOCRT(strlen)
# define strnlen  RT_NOCRT(strnlen)
# define strstr   RT_NOCRT(strstr)
#endif

__END_DECLS

#endif
