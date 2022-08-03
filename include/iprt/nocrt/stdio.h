/** @file
 * IPRT / No-CRT - Mostly empty stdio.h.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_stdio_h
#define IPRT_INCLUDED_nocrt_stdio_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/stream.h>
#include <iprt/nocrt/sys/types.h> /* errno_t, off_t */

typedef RTFOFF fpos_t;


#ifdef IPRT_NO_CRT_FOR_3RD_PARTY
/*
 * Only for external libraries and such, but even then it would be best to
 * check each printf and fprintf call as IPRT isn't 100% compatible...
 */
RT_C_DECLS_BEGIN

typedef struct RTSTREAM FILE;
# define stdin    g_pStdIn
# define stdout   g_pStdOut
# define stderr   g_pStdErr

# define printf   RTPrintf
# define vprintf  RTPrintfV
# define fprintf  RTStrmPrintf
# define vfprintf RTStrmPrintfV
int     RT_NOCRT(snprintf)(char *, size_t, const char *, ...);
int     RT_NOCRT(vsnprintf)(char *, size_t, const char *, va_list);
int     RT_NOCRT(scprintf)(const char *, ...);
int     RT_NOCRT(vscprintf)(const char *, va_list);

FILE   *RT_NOCRT(fopen)(const char *pszFilename, const char *pszMode);
FILE   *RT_NOCRT(fdopen)(int fd, const char *pszMode);
FILE   *RT_NOCRT(tmpfile)(void);
errno_t RT_NOCRT(tmpfile_s)(FILE **ppFile);
int     RT_NOCRT(fileno)(FILE *pFile);
int     RT_NOCRT(fclose)(FILE *pFile);
int     RT_NOCRT(fflush)(FILE *pFile);
int     RT_NOCRT(setvbuf)(FILE *pFile, char *pchBuf, int iBufferingType, size_t cbBuf);
int     RT_NOCRT(fseek)(FILE *pFile, long, int);
int     RT_NOCRT(fseeko)(FILE *pFile, off_t, int);
long    RT_NOCRT(ftell)(FILE *pFile);
off_t   RT_NOCRT(ftello)(FILE *pFile);
size_t  RT_NOCRT(fwrite)(void const *pvBuf, size_t cbItem, size_t cItems, FILE *pFile);
int     RT_NOCRT(fputs)(const char *psz, FILE *pFile);
int     RT_NOCRT(puts)(const char *psz);
int     RT_NOCRT(fputc)(int, FILE *pFile);
size_t  RT_NOCRT(fread)(void *pvBuf, size_t cbItem, size_t cItems, FILE *pFile);
int     RT_NOCRT(fgetc)(FILE *pFile);
int     RT_NOCRT(getc)(FILE *pFile);
int     RT_NOCRT(ferror)(FILE *pFile);
void    RT_NOCRT(clearerr)(FILE *pFile);
int     RT_NOCRT(remove)(const char *pszFilename);
int     RT_NOCRT(sscanf)(const char *pszString, const char *pszFormat, ...);

# ifndef RT_NOCRT_EOF /* also in string */
#  define RT_NOCRT_EOF  (-1)
# endif
# define EOF            RT_NOCRT_EOF

/* Underscored variants: */
# define _printf   RTPrintf
# define _vprintf  RTPrintfV
# define _fprintf  RTStrmPrintf
# define _vfprintf RTStrmPrintfV
int     RT_NOCRT(_snprintf)(char *, size_t, const char *, ...);
int     RT_NOCRT(_vsnprintf)(char *, size_t, const char *, va_list);
int     RT_NOCRT(_scprintf)(const char *, ...);
int     RT_NOCRT(_vscprintf)(const char *, va_list);

FILE   *RT_NOCRT(_fopen)(const char *pszFilename, const char *pszMode);
FILE   *RT_NOCRT(_fdopen)(int fd, const char *pszMode);
FILE   *RT_NOCRT(_tmpfile)(void);
errno_t RT_NOCRT(_tmpfile_s)(FILE **ppFile);
int     RT_NOCRT(_fileno)(FILE *pFile);
int     RT_NOCRT(_fclose)(FILE *pFile);
int     RT_NOCRT(_fflush)(FILE *pFile);
int     RT_NOCRT(_setvbuf)(FILE *pFile, char *pchBuf, int iBufferingType, size_t cbBuf);
int     RT_NOCRT(_fseek)(FILE *pFile, long, int);
int     RT_NOCRT(_fseeko)(FILE *pFile, off_t, int);
long    RT_NOCRT(_ftell)(FILE *pFile);
off_t   RT_NOCRT(_ftello)(FILE *pFile);
size_t  RT_NOCRT(_fwrite)(void const *pvBuf, size_t cbItem, size_t cItems, FILE *pFile);
int     RT_NOCRT(_fputs)(const char *psz, FILE *pFile);
int     RT_NOCRT(_fputc)(int, FILE *pFile);
size_t  RT_NOCRT(_fread)(void *pvBuf, size_t cbItem, size_t cItems, FILE *pFile);
int     RT_NOCRT(_fgetc)(FILE *pFile);
int     RT_NOCRT(_getc)(FILE *pFile);
int     RT_NOCRT(_ferror)(FILE *pFile);
void    RT_NOCRT(_clearerr)(FILE *pFile);
int     RT_NOCRT(_remove)(const char *pszFilename);
int     RT_NOCRT(_sscanf)(const char *pszString, const char *pszFormat, ...);

# define _IONBF         (1) /**< No buffering. */
# define _IOLBF         (2) /**< Line buffered. */
# define _IOFBF         (3) /**< Fully buffered. */

/* Aliases: */
# if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
#  define snprintf       RT_NOCRT(snprintf)
#  define vsnprintf      RT_NOCRT(vsnprintf)
#  define scprintf       RT_NOCRT(scprintf)
#  define vscprintf      RT_NOCRT(vscprintf)

#  define fopen          RT_NOCRT(fopen)
#  define fdopen         RT_NOCRT(fdopen)
#  define tmpfile        RT_NOCRT(tmpfile)
#  define tmpfile_s      RT_NOCRT(tmpfile_s)
#  define fileno         RT_NOCRT(fileno)
#  define fclose         RT_NOCRT(fclose)
#  define fflush         RT_NOCRT(fflush)
#  define setvbuf        RT_NOCRT(setvbuf)
#  define fseek          RT_NOCRT(fseek)
#  define fseeko         RT_NOCRT(fseeko)
#  define ftell          RT_NOCRT(ftell)
#  define ftello         RT_NOCRT(ftello)
#  define fwrite         RT_NOCRT(fwrite)
#  define fputs          RT_NOCRT(fputs)
#  define fputc          RT_NOCRT(fputc)
#  define fread          RT_NOCRT(fread)
#  define fgetc          RT_NOCRT(fgetc)
#  define getc           RT_NOCRT(getc)
#  define ferror         RT_NOCRT(ferror)
#  define clearerr       RT_NOCRT(clearerr)
#  define remove         RT_NOCRT(remove)
#  define sscanf         RT_NOCRT(sscanf)


/* Underscored variants: */
#  define _snprintf      RT_NOCRT(snprintf)
#  define _vsnprintf     RT_NOCRT(vsnprintf)
#  define _scprintf      RT_NOCRT(scprintf)
#  define _vscprintf     RT_NOCRT(vscprintf)

#  define _fopen         RT_NOCRT(fopen)
#  define _fdopen        RT_NOCRT(fdopen)
#  define _tmpfile       RT_NOCRT(tmpfile)
#  define _tmpfile_s     RT_NOCRT(tmpfile_s)
#  define _fileno        RT_NOCRT(fileno)
#  define _fclose        RT_NOCRT(fclose)
#  define _flush         RT_NOCRT(fflush)
#  define _setvbuf       RT_NOCRT(setvbuf)
#  define _fseek         RT_NOCRT(fseek)
#  define _fseeko        RT_NOCRT(fseeko)
#  define _ftell         RT_NOCRT(ftell)
#  define _ftello        RT_NOCRT(ftello)
#  define _fwrite        RT_NOCRT(fwrite)
#  define _fputs         RT_NOCRT(fputs)
#  define _fputc         RT_NOCRT(fputc)
#  define _fread         RT_NOCRT(fread)
#  define _fgetc         RT_NOCRT(fgetc)
#  define _getc          RT_NOCRT(getc)
#  define _ferror        RT_NOCRT(ferror)
#  define _clearerr      RT_NOCRT(clearerr)
#  define _remove        RT_NOCRT(remove)
#  define _sscanf        RT_NOCRT(_sscanf)
# endif

RT_C_DECLS_END

#endif

#endif /* !IPRT_INCLUDED_nocrt_stdio_h */

