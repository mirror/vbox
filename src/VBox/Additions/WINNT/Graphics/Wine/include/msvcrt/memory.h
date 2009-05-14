/*
 * Memory definitions
 *
 * Derived from the mingw header written by Colin Peters.
 * Modified for Wine use by Jon Griffiths and Francois Gouget.
 * This file is in the public domain.
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef __WINE_MEMORY_H
#define __WINE_MEMORY_H

#include <crtdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRT_MEMORY_DEFINED
#define _CRT_MEMORY_DEFINED

void* __cdecl memchr(const void*,int,size_t);
int   __cdecl memcmp(const void*,const void*,size_t);
void* __cdecl memcpy(void*,const void*,size_t);
void* __cdecl memset(void*,int,size_t);
void* __cdecl _memccpy(void*,const void*,int,unsigned int);
int   __cdecl _memicmp(const void*,const void*,unsigned int);

static inline int memicmp(const void* s1, const void* s2, size_t len) { return _memicmp(s1, s2, len); }
static inline void* memccpy(void *s1, const void *s2, int c, size_t n) { return _memccpy(s1, s2, c, n); }

#endif /* _CRT_MEMORY_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* __WINE_MEMORY_H */
