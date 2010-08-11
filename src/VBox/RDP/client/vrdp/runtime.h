/** @file
 *
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __RUNTIME__H
#define __RUNTIME__H

#include <endian.h>
#include <byteswap.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#ifndef cpu_to_le16
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(_le16) (_le16)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(_le16) bswap_16(_le16)
#else
#error Unsupported host byte order!
#endif
#endif /* cpu_to_le16 */

//#include <asm/byteorder.h>
//#define cpu_to_le16 __cpu_to_le16

#if 0
#define uint8_t uint8
#define uint16_t uint16
#define uint32_t uint32
#define uint64_t long long
#endif
#define bool int
#define false 0
#define true 1

#define OPSTATIC
#define RDPUSB_DEBUG
#ifdef RDPUSB_DEBUG
#define DoLog(a) do { \
        printf("Time %llu: ", (long long unsigned) RTTimeMilliTS()); \
        printf a; \
} while(0)

#define LogFlow(a) DoLog(a)
#define Log(a) DoLog(a)
#define Log2(a) DoLog(a)
#else
#define LogFlow(a) do {} while (0)
#define Log(a)     do {} while (0)
#define Log2(a)    do {} while (0)
#endif

#define LogRel(a) printf a

/* Runtime wrappers. */
#define RTMemAlloc xmalloc
#define RTMemRealloc xrealloc
#define RTMemFree xfree

#define _1K 1024

#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U16(u16)   RT_BSWAP_U16(u16)
# define RT_LE2H_U16(u16)   RT_BSWAP_U16(u16)
#else
# define RT_H2LE_U16(u16)   (u16)
# define RT_LE2H_U16(u16)   (u16)
#endif
#define RT_BSWAP_U16(u16) RT_MAKE_U16(RT_HIBYTE(u16), RT_LOBYTE(u16))
#define RT_MAKE_U16(Lo, Hi) \
    ((uint16_t)(  (uint16_t)((uint8_t)(Hi)) << 8 \
                | (uint8_t)(Lo) ))
#define RT_LOBYTE(a)                            ( (a) & 0xff )
#define RT_HIBYTE(a)                            ( (a) >> 8 )

#define VINF_SUCCESS 0
#define VERR_NO_MEMORY        (-8)
#define VERR_INVALID_POINTER  (-6)
#define VERR_UNRESOLVED_ERROR (-35)
#define VERR_NOT_SUPPORTED    (-37)
#define VERR_ACCESS_DENIED    (-38)
#define VERR_VUSB_USBFS_PERMISSION (-2005)

static inline const char *RTErrGetShort(int iErr)
{
    return   iErr == VINF_SUCCESS               ? "VINF_SUCCESS"
           : iErr == VERR_NO_MEMORY             ? "VERR_NO_MEMORY"
           : iErr == VERR_INVALID_POINTER       ? "VERR_INVALID_POINTER"
           : iErr == VERR_UNRESOLVED_ERROR      ? "VERR_UNRESOLVED_ERROR"
           : iErr == VERR_NOT_SUPPORTED         ? "VERR_NOT_SUPPORTED"
           : iErr == VERR_ACCESS_DENIED         ? "VERR_ACCESS_DENIED"
           : iErr == VERR_VUSB_USBFS_PERMISSION ? "VERR_VUSB_USBFS_PERMISSION"
           : "Unknown error";
}

static inline int RTErrConvertFromErrno(int iErrno)
{
    return   iErrno == 0 ? VINF_SUCCESS
           : iErrno == ENOMEM ? VERR_NO_MEMORY
           : iErrno == ENODEV ? VERR_NOT_SUPPORTED
           : iErrno == ENOSYS ? VERR_NOT_SUPPORTED
           : iErrno == EPERM ? VERR_ACCESS_DENIED
           : iErrno == EACCES ? VERR_ACCESS_DENIED
           : VERR_UNRESOLVED_ERROR;
}

#define RT_SUCCESS(_rc) ((_rc) >= 0)

#define RTFILE int
#define RTCRITSECT void *

#define Assert(_expr) do {                            \
    if (!(_expr))                                     \
    {                                                 \
        Log(("Assertion failed: {%s}!!!\n", #_expr)); \
    }                                                 \
} while (0)

#define AssertMsgFailed(_msg) do {                    \
        Log(("Assertion failed msg:!!!\n"));          \
	Log(_msg);                                    \
} while (0)

#define AssertReturn(_expr, _retval) do {             \
    if (!(_expr))                                     \
    {                                                 \
        Log(("Assertion failed: {%s}, returning 0x%08X!!!\n", #_expr, _retval)); \
	return (_retval);                             \
    }                                                 \
} while (0)

#define AssertRC(_rc) Assert(RT_SUCCESS(_rc))

#define RT_FAILURE(_rc) (!RT_SUCCESS(_rc))

#define NOREF(_a) ((void)_a)

#define VALID_PTR(ptr) ((ptr) != NULL)

#define RT_C_DECLS_BEGIN
#define RT_C_DECLS_END

#define RTCALL
#define DECLR3CALLBACKMEMBER(type, name, args)  type (RTCALL * name) args
#define DECLINLINE(type) static __inline__ type
#define DECLCALLBACK(type)      type RTCALL
#define DECLCALLBACKMEMBER(type, name)  type (RTCALL * name)
#define RTDECL(type)       RTCALL type

#define RT_BIT(bit)                             ( 1U << (bit) )
#define RT_MAX(Value1, Value2)                  ( (Value1) >= (Value2) ? (Value1) : (Value2) )
#define RT_MIN(Value1, Value2)                  ( (Value1) <= (Value2) ? (Value1) : (Value2) )
typedef uint32_t            RTGCPHYS32;
typedef uint32_t            RTMSINTERVAL;

static inline uint64_t RTTimeMilliTS (void)
{
     struct timeval tv;
     gettimeofday (&tv, NULL);
     return (uint64_t)tv.tv_sec * (uint64_t)(1000)
            + (uint64_t)(tv.tv_usec / 1000);
}

static inline int RTCritSectInit (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectDelete (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectEnter (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectLeave (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline void *RTMemDupEx (const void *pvSrc, size_t cbSrc, size_t cbExtra)
{
    void *p = RTMemAlloc (cbSrc + cbExtra);

    if (p)
    {
         memcpy (p, pvSrc, cbSrc);
         memset ((char *)p + cbSrc, 0, cbExtra);
    }

    return p;
}

static inline void *RTMemAllocZ (size_t cb)
{
    void *p = RTMemAlloc (cb);

    if (p)
    {
         memset (p, 0, cb);
    }

    return p;
}

static inline void *RTMemAllocZVar (size_t cbUnaligned)
{
    return RTMemAllocZ(cbUnaligned);
}

static inline int RTStrToUInt32Ex (const char *pszValue, char **ppszNext, unsigned uBase, uint32_t *pu32)
{
    *pu32 = strtoul (pszValue, ppszNext, uBase);
    return VINF_SUCCESS;
}

#define PRTSTREAM FILE *

static inline int RTStrmOpen (const char *pszFileName, const char *pszMode, PRTSTREAM *ppStream)
{
    *ppStream = fopen (pszFileName, pszMode);

    if (*ppStream)
    {
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

static inline int RTStrmClose (PRTSTREAM pStream)
{
    fclose (pStream);
    return VINF_SUCCESS;
}

static inline int RTStrmGetLine (PRTSTREAM pStream, char *pszString, size_t cchString)
{
    if (fgets (pszString, cchString, pStream))
    {
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

static inline char *RTStrStripL (const char *psz)
{
    while (isspace (*psz))
        psz++;
    return (char *)psz;
}

#define NIL_RTFILE -1

#define RTFILE_O_READWRITE 0x00000003
#define RTFILE_O_OPEN      0x00000000
#define RTFILE_O_DENY_NONE 0x00000000

static inline int RTFileOpen (RTFILE *pFile, const char *pszFileName, unsigned fOpen)
{
    Assert (fOpen == RTFILE_O_READWRITE);

    *pFile = open (pszFileName, O_RDWR, 00600);

    if (*pFile != -1)
    {
        return VINF_SUCCESS;
    }

    return VERR_ACCESS_DENIED;
}

static inline int RTFileClose (RTFILE file)
{
    close (file);
    return VINF_SUCCESS;
}
#endif /* __RUNTIME__H  */
