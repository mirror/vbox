#ifndef QEMU_OSDEP_H
#define QEMU_OSDEP_H

#ifdef VBOX

#include <iprt/alloc.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>

#define qemu_snprintf(pszBuf, cbBuf, ...) RTStrPrintf((pszBuf), (cbBuf), __VA_ARGS__)
#define qemu_vsnprintf(pszBuf, cbBuf, pszFormat, args) \
                            RTStrPrintfV((pszBuf), (cbBuf), (pszFormat), (args))
#define qemu_vprintf(pszFormat, args) \
                            RTLogPrintfV((pszFormat), (args))
#define qemu_printf         RTLogPrintf
#define qemu_malloc(cb)     RTMemAlloc(cb)
#define qemu_mallocz(cb)    RTMemAllocZ(cb)
#define qemu_free(pv)       RTMemFree(pv)
#define qemu_strdup(psz)    RTStrDup(psz)

#define qemu_vmalloc(cb)    RTMemPageAlloc(cb)
#define qemu_vfree(pv)      RTMemPageFree(pv)

#ifndef NULL
# define NULL 0
#endif

#else /* !VBOX */

#include <stdarg.h>

#define qemu_snprintf snprintf   /* bird */
#define qemu_vsnprintf vsnprintf /* bird */
#define qemu_vprintf vprintf     /* bird */

#define qemu_printf printf


void *qemu_malloc(size_t size);
void *qemu_mallocz(size_t size);
void qemu_free(void *ptr);
char *qemu_strdup(const char *str);

void *qemu_vmalloc(size_t size);
void qemu_vfree(void *ptr);

void *get_mmap_addr(unsigned long size);

#endif /* !VBOX */

#endif
