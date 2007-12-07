/** $Id$ */
/** @file
 * VBox vl.h Replacement.
 *
 * Avoid including this file whenever possible.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __vl_vbox_h__
#define __vl_vbox_h__

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/ssm.h>
#include <VBox/tm.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/pci.h>

#include <iprt/string.h>

#include "Builtins.h"

__BEGIN_DECLS

/*
 * Misc macros.
 */
#define TARGET_PAGE_BITS        (PAGE_SHIFT)
#define TARGET_PAGE_SIZE        (PAGE_SIZE)
#define TARGET_PAGE_MASK        (~PAGE_OFFSET_MASK)

/*
 * Necessary for pckbd and vga.
 */
#define TARGET_I386 1

#ifndef glue
# define _glue(x, y)    x ## y
# define glue(x, y)     _glue(x, y)
# define tostring(s)	#s
# define stringify(s)	tostring(s)
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline _inline
#endif

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif


/*
 * Misc types.
 */
typedef RTGCPHYS                target_phys_addr_t;
typedef PCIDEVICE               PCIDevice;
typedef RTGCUINTREG             target_ulong;



/*
 * Timers.
 */
#define QEMUTimerCB                             FNTMTIMERQEMU
typedef struct TMTIMER QEMUTimer;
#define rt_clock                                TMCLOCK_REAL
#define vm_clock                                TMCLOCK_VIRTUAL
#define ticks_per_sec                           TMCpuTicksPerSecond((PVM)cpu_single_env->pVM)
#define qemu_get_clock(enmClock)                TMR3Clock((PVM)cpu_single_env->pVM, enmClock)
#define qemu_new_timer(clock, callback, user)   (QEMUTimer *)TMR3TimerCreateExternal((PVM)cpu_single_env->pVM, clock, callback, user, __FUNCTION__ )
#define qemu_free_timer(timer)                  TMTimerDestroy(timer)
#define qemu_del_timer(timer)                   TMTimerStop(timer)
#define qemu_mod_timer(timer, expire)           TMTimerSet(timer, (uint64_t)expire)
#define qemu_timer_pending(timer)               TMTimerIsActive(timer)
#define cpu_disable_ticks()                     TMCpuTickPause((PVM)cpu_single_env->pVM)
#define cpu_enable_ticks()                      TMCpuTickResume((PVM)cpu_single_env->pVM)
#define cpu_calibrate_ticks()                   do {} while (0)
#define init_timers()                           do {} while (0)
#define quit_timers()                           do {} while (0)


#ifdef IN_RING3
/*
 * Saved state.
 */
typedef struct SSMHANDLE QEMUFile;

#define qemu_put_buffer(f, pv, cb)                  SSMR3PutMem((f), (pv), (cb))
#define qemu_put_byte(f, u8)                        SSMR3PutU8((f), (uint8_t)(u8))
#define qemu_put_8s(f, pu8)                         SSMR3PutU8((f), *(pu8))
#define qemu_put_be16s(f, pu16)                     SSMR3PutU16((f), *(pu16))
#define qemu_put_be32s(f, pu32)                     SSMR3PutU32((f), *(pu32))
#define qemu_put_be64s(f, pu64)                     SSMR3PutU64((f), *(pu64))
#define qemu_put_be16(f, u16)                       SSMR3PutU16((f), (uint16_t)(u16))
#define qemu_put_be32(f, u32)                       SSMR3PutU32((f), (uint32_t)(u32))
#define qemu_put_be64(f, u64)                       SSMR3PutU64((f), (uint64_t)(u64))

#define qemu_get_8s(f, pu8)                         SSMR3GetU8((f), (pu8))
#define qemu_get_be16s(f, pu16)                     SSMR3GetU16((f), (pu16))
#define qemu_get_be32s(f, pu32)                     SSMR3GetU32((f), (pu32))
#define qemu_get_be64s(f, pu64)                     SSMR3GetU64((f), (pu64))

DECLINLINE(int) qemu_get_buffer(QEMUFile *f, uint8_t *buf, int size)
{
    int rc = SSMR3GetMem(f, buf, size);
    return VBOX_SUCCESS(rc) ? size : 0;
}

DECLINLINE(int) qemu_get_byte(QEMUFile *f)
{
    uint8_t u8;
    int rc = SSMR3GetU8(f, &u8);
    return VBOX_SUCCESS(rc) ? (int)u8 : -1;
}

DECLINLINE(unsigned) qemu_get_be16(QEMUFile *f)
{
    uint16_t u16;
    int rc = SSMR3GetU16(f, &u16);
    return VBOX_SUCCESS(rc) ? u16 : ~0;
}

DECLINLINE(unsigned) qemu_get_be32(QEMUFile *f)
{
    uint32_t u32;
    int rc = SSMR3GetU32(f, &u32);
    return VBOX_SUCCESS(rc) ? u32 : ~0;
}

DECLINLINE(int64_t) qemu_get_be64(QEMUFile *f)
{
    uint64_t u64;
    int rc = SSMR3GetU64(f, &u64);
    return VBOX_SUCCESS(rc) ? u64 : ~0;
}

#define qemu_put_timer(f, ts)   TMR3TimerSave((ts), (f))
#define qemu_get_timer(f, ts)   TMR3TimerLoad((ts), (f))

#endif /* IN_RING3 */


/*
 * Memory access.
 */

DECLINLINE(int) lduw_raw(void *ptr)
{
    uint8_t *p = (uint8_t *)ptr;
    return p[0] | (p[1] << 8);
}

/*
 * Misc.
 */

#ifdef _MSC_VER
/**
 * ffs -- vax ffs instruction
 */
inline int ffs(int mask)
{
    int bit;
    if (mask == 0)
        return(0);
    for (bit = 1; !(mask & 1); bit++)
        mask >>= 1;
    return(bit);
}
#endif /* _MSC_VER */

/* bswap.h */
#ifdef _MSC_VER
#ifndef LITTLE_ENDIAN
#define	LITTLE_ENDIAN   1234
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER      LITTLE_ENDIAN
#endif

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
#undef __extension__
#undef __attribute__

#ifndef LITTLE_ENDIAN
#define	LITTLE_ENDIAN   1234
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER      LITTLE_ENDIAN
#endif

#define bswap_16(x) \
(__extension__ ({ \
	uint16_t __x = (x); \
	((uint16_t)( \
		(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
}))

#define bswap_32(x) \
(__extension__ ({ \
	uint32_t __x = (x); \
	((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
}))

#define bswap_64(x) \
(__extension__ ({ \
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
}))
#endif


#ifndef bswap16
DECLINLINE(uint16_t) bswap16(uint16_t x)
{
    return bswap_16(x);
}
#endif

#ifndef bswap32
DECLINLINE(uint32_t) bswap32(uint32_t x)
{
    return bswap_32(x);
}
#endif

#ifndef bswap64
DECLINLINE(uint64_t) bswap64(uint64_t x)
{
    return bswap_64(x);
}
#endif

DECLINLINE(void) bswap16s(uint16_t *s)
{
    *s = bswap16(*s);
}

DECLINLINE(void) bswap32s(uint32_t *s)
{
    *s = bswap32(*s);
}

DECLINLINE(void) bswap64s(uint64_t *s)
{
    *s = bswap64(*s);
}

#define le_bswap(v, size) (v)
#define be_bswap(v, size) bswap ## size(v)
#define le_bswaps(v, size)
#define be_bswaps(p, size) *p = bswap ## size(*p);

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

CPU_CONVERT(be, 16, uint16_t)
CPU_CONVERT(be, 32, uint32_t)
CPU_CONVERT(be, 64, uint64_t)

CPU_CONVERT(le, 16, uint16_t)
CPU_CONVERT(le, 32, uint32_t)
CPU_CONVERT(le, 64, uint64_t)


#define cpu_to_le16wu(p, v) cpu_to_le16w(p, v)
#define cpu_to_le32wu(p, v) cpu_to_le32w(p, v)
#define le16_to_cpupu(p)    le16_to_cpup(p)
#define le32_to_cpupu(p)    le32_to_cpup(p)

#define cpu_to_be16wu(p, v) cpu_to_be16w(p, v)
#define cpu_to_be32wu(p, v) cpu_to_be32w(p, v)

#define cpu_to_32wu cpu_to_le32wu

#undef le_bswap
#undef be_bswap
#undef le_bswaps
#undef be_bswaps

/* end of bswap.h */

__END_DECLS

#endif /* __vl_vbox_h__ */
