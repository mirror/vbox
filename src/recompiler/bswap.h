#ifndef BSWAP_H
#define BSWAP_H

#include "config-host.h"

#ifndef _MSC_VER
#include <inttypes.h>
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#else
#ifdef _MSC_VER
static _inline uint16_t bswap_16(register uint16_t x)
{
    return ((uint16_t)( \
		(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(x) & (uint16_t)0xff00U) >> 8) )); \
}

static _inline uint32_t bswap_32(register uint32_t x) \
{ \
    return ((uint32_t)( \
		(((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24) )); \
}

static _inline uint64_t bswap_64(register uint64_t x) \
{ \
    return ((uint64_t)( \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	        (uint64_t)(((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
}

#else

#define bswap_16(x) __extension__ /* <- VBOX */ \
({ \
	uint16_t __x = (x); \
	((uint16_t)( \
		(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})

#define bswap_32(x) __extension__ /* <- VBOX */ \
({ \
	uint32_t __x = (x); \
	((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})

#define bswap_64(x) __extension__ /* <- VBOX */ \
({ \
	uint64_t __x = (x); \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})
#endif

#endif /* !HAVE_BYTESWAP_H */

#ifndef bswap16 /* BSD endian.h clash */
#ifndef VBOX
static inline uint16_t bswap16(uint16_t x)
#else
DECLINLINE(uint16_t) bswap16(uint16_t x)
#endif
{
    return bswap_16(x);
}
#endif

#ifndef bswap32 /* BSD endian.h clash */
#ifndef VBOX
static inline uint32_t bswap32(uint32_t x)
#else
DECLINLINE(uint32_t) bswap32(uint32_t x)
#endif
{
    return bswap_32(x);
}
#endif

#ifndef bswap64 /* BSD endian.h clash. */
#ifndef VBOX
static inline uint64_t bswap64(uint64_t x)
#else
DECLINLINE(uint64_t) bswap64(uint64_t x)
#endif
{
    return bswap_64(x);
}
#endif

#ifndef VBOX
static inline void bswap16s(uint16_t *s)
#else
DECLINLINE(void) bswap16s(uint16_t *s)
#endif
{
    *s = bswap16(*s);
}

#ifndef VBOX
static inline void bswap32s(uint32_t *s)
#else
DECLINLINE(void) bswap32s(uint32_t *s)
#endif
{
    *s = bswap32(*s);
}

#ifndef VBOX
static inline void bswap64s(uint64_t *s)
#else
DECLINLINE(void) bswap64s(uint64_t *s)
#endif
{
    *s = bswap64(*s);
}

#if defined(WORDS_BIGENDIAN)
#define be_bswap(v, size) (v)
#define le_bswap(v, size) bswap ## size(v)
#define be_bswaps(v, size)
#define le_bswaps(p, size) *p = bswap ## size(*p);
#else
#define le_bswap(v, size) (v)
#define be_bswap(v, size) bswap ## size(v)
#define le_bswaps(v, size)
#define be_bswaps(p, size) *p = bswap ## size(*p);
#endif

#ifndef VBOX
#define CPU_CONVERT(endian, size, type)\
static inline type endian ## size ## _to_cpu(type v)\
{\
    return endian ## _bswap(v, size);\
}\
\
static inline type cpu_to_ ## endian ## size(type v)\
{\
    return endian ## _bswap(v, size);\
}\
\
static inline void endian ## size ## _to_cpus(type *p)\
{\
    endian ## _bswaps(p, size)\
}\
\
static inline void cpu_to_ ## endian ## size ## s(type *p)\
{\
    endian ## _bswaps(p, size)\
}\
\
static inline type endian ## size ## _to_cpup(const type *p)\
{\
    return endian ## size ## _to_cpu(*p);\
}\
\
static inline void cpu_to_ ## endian ## size ## w(type *p, type v)\
{\
     *p = cpu_to_ ## endian ## size(v);\
}
#else  /* VBOX */
#define CPU_CONVERT(endian, size, type)\
DECLINLINE(type) endian ## size ## _to_cpu(type v)\
{\
    return endian ## _bswap(v, size);\
}\
\
DECLINLINE(type) cpu_to_ ## endian ## size(type v)\
{\
    return endian ## _bswap(v, size);\
}\
\
DECLINLINE(void) endian ## size ## _to_cpus(type *p)\
{\
    endian ## _bswaps(p, size)\
}\
\
DECLINLINE(void) cpu_to_ ## endian ## size ## s(type *p)\
{\
    endian ## _bswaps(p, size)\
}\
\
DECLINLINE(type) endian ## size ## _to_cpup(const type *p)\
{\
    return endian ## size ## _to_cpu(*p);\
}\
\
DECLINLINE(void) cpu_to_ ## endian ## size ## w(type *p, type v)\
{\
     *p = cpu_to_ ## endian ## size(v);\
}
#endif /* VBOX */

CPU_CONVERT(be, 16, uint16_t)
CPU_CONVERT(be, 32, uint32_t)
CPU_CONVERT(be, 64, uint64_t)

CPU_CONVERT(le, 16, uint16_t)
CPU_CONVERT(le, 32, uint32_t)
CPU_CONVERT(le, 64, uint64_t)

/* unaligned versions (optimized for frequent unaligned accesses)*/

#if defined(__i386__) || defined(__powerpc__)

#define cpu_to_le16wu(p, v) cpu_to_le16w(p, v)
#define cpu_to_le32wu(p, v) cpu_to_le32w(p, v)
#define le16_to_cpupu(p) le16_to_cpup(p)
#define le32_to_cpupu(p) le32_to_cpup(p)

#define cpu_to_be16wu(p, v) cpu_to_be16w(p, v)
#define cpu_to_be32wu(p, v) cpu_to_be32w(p, v)

#else

static inline void cpu_to_le16wu(uint16_t *p, uint16_t v)
{
    uint8_t *p1 = (uint8_t *)p;

    p1[0] = (uint8_t)v;
    p1[1] = v >> 8;
}

static inline void cpu_to_le32wu(uint32_t *p, uint32_t v)
{
    uint8_t *p1 = (uint8_t *)p;

    p1[0] = (uint8_t)v;
    p1[1] = v >> 8;
    p1[2] = v >> 16;
    p1[3] = v >> 24;
}

static inline uint16_t le16_to_cpupu(const uint16_t *p)
{
    const uint8_t *p1 = (const uint8_t *)p;
    return p1[0] | (p1[1] << 8);
}

static inline uint32_t le32_to_cpupu(const uint32_t *p)
{
    const uint8_t *p1 = (const uint8_t *)p;
    return p1[0] | (p1[1] << 8) | (p1[2] << 16) | (p1[3] << 24);
}

static inline void cpu_to_be16wu(uint16_t *p, uint16_t v)
{
    uint8_t *p1 = (uint8_t *)p;

    p1[0] = v >> 8;
    p1[1] = (uint8_t)v;
}

static inline void cpu_to_be32wu(uint32_t *p, uint32_t v)
{
    uint8_t *p1 = (uint8_t *)p;

    p1[0] = v >> 24;
    p1[1] = v >> 16;
    p1[2] = v >> 8;
    p1[3] = (uint8_t)v;
}

#endif

#ifdef WORDS_BIGENDIAN
#define cpu_to_32wu cpu_to_be32wu
#else
#define cpu_to_32wu cpu_to_le32wu
#endif

#undef le_bswap
#undef be_bswap
#undef le_bswaps
#undef be_bswaps

#endif /* BSWAP_H */
