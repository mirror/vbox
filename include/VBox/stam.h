/** @file
 * STAM - Statistics Manager.
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

#ifndef ___VBox_stam_h
#define ___VBox_stam_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/stdarg.h>
#ifdef _MSC_VER
# if _MSC_VER >= 1400
#  include <intrin.h>
# endif
#endif

__BEGIN_DECLS

/** @defgroup grp_stam     The Statistics Manager API
 * @{
 */

#if defined(VBOX_WITHOUT_RELEASE_STATISTICS) && defined(VBOX_WITH_STATISTICS)
# error "Both VBOX_WITHOUT_RELEASE_STATISTICS and VBOX_WITH_STATISTICS are defined! Make up your mind!"
#endif


/** @def STAM_GET_TS
 * Gets the CPU timestamp counter.
 *
 * @param   u64     The 64-bit variable which the timestamp shall be saved in.
 */
#ifdef __GNUC__
# if defined(RT_ARCH_X86)
   /* This produces optimal assembler code for x86 but does not work for AMD64 ('A' means 'either rax or rdx') */
#  define STAM_GET_TS(u64) __asm__ __volatile__ ("rdtsc\n\t" : "=A" (u64));
# elif defined(RT_ARCH_AMD64)
#  define STAM_GET_TS(u64) \
    do { uint64_t low; uint64_t high; \
         __asm__ __volatile__ ("rdtsc\n\t" : "=a"(low), "=d"(high)); \
         (u64) = ((high << 32) | low); \
    } while (0)
# endif
#elif _MSC_VER >= 1400
# pragma intrinsic(__rdtsc)
# define STAM_GET_TS(u64)    \
    do { (u64) = __rdtsc(); } while (0)
#else
# define STAM_GET_TS(u64)    \
    do {                               \
        uint64_t u64Tmp;               \
        __asm {                        \
            __asm rdtsc                \
            __asm mov dword ptr [u64Tmp],     eax   \
            __asm mov dword ptr [u64Tmp + 4], edx   \
        }                              \
        (u64) = u64Tmp; \
    } while (0)
#endif


/** @def STAM_REL_STATS
 * Code for inclusion only when VBOX_WITH_STATISTICS is defined.
 * @param   code    A code block enclosed in {}.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_STATS(code) do code while(0)
#else
# define STAM_REL_STATS(code) do {} while(0)
#endif
/** @def STAM_STATS
 * Code for inclusion only when VBOX_WITH_STATISTICS is defined.
 * @param   code    A code block enclosed in {}.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_STATS(code) STAM_REL_STATS(code)
#else
# define STAM_STATS(code) do {} while(0)
#endif


/**
 * Sample type.
 */
typedef enum STAMTYPE
{
    /** Invalid entry. */
    STAMTYPE_INVALID = 0,
    /** Generic counter. */
    STAMTYPE_COUNTER,
    /** Profiling of an function. */
    STAMTYPE_PROFILE,
    /** Profiling of an operation. */
    STAMTYPE_PROFILE_ADV,
    /** Ratio of A to B, uint32_t types. Not reset. */
    STAMTYPE_RATIO_U32,
    /** Ratio of A to B, uint32_t types. Reset both to 0. */
    STAMTYPE_RATIO_U32_RESET,
    /** Callback. */
    STAMTYPE_CALLBACK,
    /** Generic unsigned 8-bit value. Not reset. */
    STAMTYPE_U8,
    /** Generic unsigned 8-bit value. Reset to 0. */
    STAMTYPE_U8_RESET,
    /** Generic hexadecimal unsigned 8-bit value. Not reset. */
    STAMTYPE_X8,
    /** Generic hexadecimal unsigned 8-bit value. Reset to 0. */
    STAMTYPE_X8_RESET,
    /** Generic unsigned 16-bit value. Not reset. */
    STAMTYPE_U16,
    /** Generic unsigned 16-bit value. Reset to 0. */
    STAMTYPE_U16_RESET,
    /** Generic hexadecimal unsigned 16-bit value. Not reset. */
    STAMTYPE_X16,
    /** Generic hexadecimal unsigned 16-bit value. Reset to 0. */
    STAMTYPE_X16_RESET,
    /** Generic unsigned 32-bit value. Not reset. */
    STAMTYPE_U32,
    /** Generic unsigned 32-bit value. Reset to 0. */
    STAMTYPE_U32_RESET,
    /** Generic hexadecimal unsigned 32-bit value. Not reset. */
    STAMTYPE_X32,
    /** Generic hexadecimal unsigned 32-bit value. Reset to 0. */
    STAMTYPE_X32_RESET,
    /** Generic unsigned 64-bit value. Not reset. */
    STAMTYPE_U64,
    /** Generic unsigned 64-bit value. Reset to 0. */
    STAMTYPE_U64_RESET,
    /** Generic hexadecimal unsigned 64-bit value. Not reset. */
    STAMTYPE_X64,
    /** Generic hexadecimal unsigned 64-bit value. Reset to 0. */
    STAMTYPE_X64_RESET,
    /** The end (exclusive). */
    STAMTYPE_END
} STAMTYPE;

/**
 * Sample visibility type.
 */
typedef enum STAMVISIBILITY
{
    /** Invalid entry. */
    STAMVISIBILITY_INVALID = 0,
    /** Always visible. */
    STAMVISIBILITY_ALWAYS,
    /** Only visible when used (/hit). */
    STAMVISIBILITY_USED,
    /** Not visible in the GUI. */
    STAMVISIBILITY_NOT_GUI,
    /** The end (exclusive). */
    STAMVISIBILITY_END
} STAMVISIBILITY;

/**
 * Sample unit.
 */
typedef enum STAMUNIT
{
    /** Invalid entry .*/
    STAMUNIT_INVALID = 0,
    /** No unit. */
    STAMUNIT_NONE,
    /** Number of calls. */
    STAMUNIT_CALLS,
    /** Count of whatever. */
    STAMUNIT_COUNT,
    /** Count of bytes. */
    STAMUNIT_BYTES,
    /** Count of bytes. */
    STAMUNIT_PAGES,
    /** Error count. */
    STAMUNIT_ERRORS,
    /** Number of occurences. */
    STAMUNIT_OCCURENCES,
    /** Ticks per call. */
    STAMUNIT_TICKS_PER_CALL,
    /** Ticks per occurence. */
    STAMUNIT_TICKS_PER_OCCURENCE,
    /** Ratio of good vs. bad. */
    STAMUNIT_GOOD_BAD,
    /** Megabytes. */
    STAMUNIT_MEGABYTES,
    /** Kilobytes. */
    STAMUNIT_KILOBYTES,
    /** Nano seconds. */
    STAMUNIT_NS,
    /** Nanoseconds per call. */
    STAMUNIT_NS_PER_CALL,
    /** Nanoseconds per call. */
    STAMUNIT_NS_PER_OCCURENCE,
    /** Percentage. */
    STAMUNIT_PCT,
    /** The end (exclusive). */
    STAMUNIT_END
} STAMUNIT;


/** @def STAM_REL_U8_INC
 * Increments a uint8_t sample by one.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U8_INC(pCounter) \
    do { ++*(pCounter); } while (0)
#else
# define STAM_REL_U8_INC(pCounter) do { } while (0)
#endif
/** @def STAM_U8_INC
 * Increments a uint8_t sample by one.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U8_INC(pCounter) STAM_REL_U8_INC(pCounter)
#else
# define STAM_U8_INC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U8_DEC
 * Decrements a uint8_t sample by one.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U8_DEC(pCounter) \
    do { --*(pCounter); } while (0)
#else
# define STAM_REL_U8_DEC(pCounter) do { } while (0)
#endif
/** @def STAM_U8_DEC
 * Decrements a uint8_t sample by one.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U8_DEC(pCounter) STAM_REL_U8_DEC(pCounter)
#else
# define STAM_U8_DEC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U8_ADD
 * Increments a uint8_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U8_ADD(pCounter, Addend) \
    do { *(pCounter) += (Addend); } while (0)
#else
# define STAM_REL_U8_ADD(pCounter, Addend) do { } while (0)
#endif
/** @def STAM_U8_ADD
 * Increments a uint8_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint8_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U8_ADD(pCounter, Addend) STAM_REL_U8_ADD(pCounter, Addend
#else
# define STAM_U8_ADD(pCounter, Addend) do { } while (0)
#endif


/** @def STAM_REL_U16_INC
 * Increments a uint16_t sample by one.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U16_INC(pCounter) \
    do { ++*(pCounter); } while (0)
#else
# define STAM_REL_U16_INC(pCounter) do { } while (0)
#endif
/** @def STAM_U16_INC
 * Increments a uint16_t sample by one.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U16_INC(pCounter) STAM_REL_U16_INC(pCounter)
#else
# define STAM_U16_INC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U16_DEC
 * Decrements a uint16_t sample by one.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U16_DEC(pCounter) \
    do { --*(pCounter); } while (0)
#else
# define STAM_REL_U16_DEC(pCounter) do { } while (0)
#endif
/** @def STAM_U16_DEC
 * Decrements a uint16_t sample by one.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U16_DEC(pCounter) STAM_REL_U16_DEC(pCounter)
#else
# define STAM_U16_DEC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U16_INC
 * Increments a uint16_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U16_ADD(pCounter, Addend) \
    do { *(pCounter) += (Addend); } while (0)
#else
# define STAM_REL_U16_ADD(pCounter, Addend) do { } while (0)
#endif
/** @def STAM_U16_INC
 * Increments a uint16_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint16_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U16_ADD(pCounter, Addend) STAM_REL_U16_ADD(pCounter, Addend)
#else
# define STAM_U16_ADD(pCounter, Addend) do { } while (0)
#endif


/** @def STAM_REL_U32_INC
 * Increments a uint32_t sample by one.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U32_INC(pCounter) \
    do { ++*(pCounter); } while (0)
#else
# define STAM_REL_U32_INC(pCounter) do { } while (0)
#endif
/** @def STAM_U32_INC
 * Increments a uint32_t sample by one.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U32_INC(pCounter) STAM_REL_U32_INC(pCounter)
#else
# define STAM_U32_INC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U32_DEC
 * Decrements a uint32_t sample by one.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U32_DEC(pCounter) \
    do { --*(pCounter); } while (0)
#else
# define STAM_REL_U32_DEC(pCounter) do { } while (0)
#endif
/** @def STAM_U32_DEC
 * Decrements a uint32_t sample by one.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U32_DEC(pCounter) STAM_REL_U32_DEC(pCounter)
#else
# define STAM_U32_DEC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U32_ADD
 * Increments a uint32_t sample by value.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U32_ADD(pCounter, Addend) \
    do { *(pCounter) += (Addend); } while (0)
#else
# define STAM_REL_U32_ADD(pCounter, Addend) do { } while (0)
#endif
/** @def STAM_U32_ADD
 * Increments a uint32_t sample by value.
 *
 * @param   pCounter    Pointer to the uint32_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U32_ADD(pCounter, Addend) STAM_REL_U32_ADD(pCounter, Addend)
#else
# define STAM_U32_ADD(pCounter, Addend) do { } while (0)
#endif


/** @def STAM_REL_U64_INC
 * Increments a uint64_t sample by one.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U64_INC(pCounter) \
    do { ++*(pCounter); } while (0)
#else
# define STAM_REL_U64_INC(pCounter) do { } while (0)
#endif
/** @def STAM_U64_INC
 * Increments a uint64_t sample by one.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U64_INC(pCounter) STAM_REL_U64_INC(pCounter)
#else
# define STAM_U64_INC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U64_DEC
 * Decrements a uint64_t sample by one.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U64_DEC(pCounter) \
    do { --*(pCounter); } while (0)
#else
# define STAM_REL_U64_DEC(pCounter) do { } while (0)
#endif
/** @def STAM_U64_DEC
 * Decrements a uint64_t sample by one.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U64_DEC(pCounter) STAM_REL_U64_DEC(pCounter)
#else
# define STAM_U64_DEC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_U64_ADD
 * Increments a uint64_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_U64_ADD(pCounter, Addend) \
    do { *(pCounter) += (Addend); } while (0)
#else
# define STAM_REL_U64_ADD(pCounter, Addend) do { } while (0)
#endif
/** @def STAM_U64_ADD
 * Increments a uint64_t sample by a value.
 *
 * @param   pCounter    Pointer to the uint64_t variable to operate on.
 * @param   Addend      The value to add.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_U64_ADD(pCounter, Addend) STAM_REL_U64_ADD(pCounter, Addend)
#else
# define STAM_U64_ADD(pCounter, Addend) do { } while (0)
#endif


/**
 * Counter sample - STAMTYPE_COUNTER.
 */
typedef struct STAMCOUNTER
{
    /** The current count. */
    volatile uint64_t   c;
} STAMCOUNTER;
/** Pointer to a counter. */
typedef STAMCOUNTER *PSTAMCOUNTER;
/** Pointer to a const counter. */
typedef const STAMCOUNTER *PCSTAMCOUNTER;


/** @def STAM_REL_COUNTER_INC
 * Increments a counter sample by one.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_COUNTER_INC(pCounter) \
    do { (pCounter)->c++; } while (0)
#else
# define STAM_REL_COUNTER_INC(pCounter) do { } while (0)
#endif
/** @def STAM_COUNTER_INC
 * Increments a counter sample by one.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_COUNTER_INC(pCounter) STAM_REL_COUNTER_INC(pCounter)
#else
# define STAM_COUNTER_INC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_COUNTER_DEC
 * Decrements a counter sample by one.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_COUNTER_DEC(pCounter) \
    do { (pCounter)->c--; } while (0)
#else
# define STAM_REL_COUNTER_DEC(pCounter) do { } while (0)
#endif
/** @def STAM_COUNTER_DEC
 * Decrements a counter sample by one.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_COUNTER_DEC(pCounter) STAM_REL_COUNTER_DEC(pCounter)
#else
# define STAM_COUNTER_DEC(pCounter) do { } while (0)
#endif


/** @def STAM_REL_COUNTER_ADD
 * Increments a counter sample by a value.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 * @param   Addend      The value to add to the counter.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_COUNTER_ADD(pCounter, Addend) \
    do { (pCounter)->c += (Addend); } while (0)
#else
# define STAM_REL_COUNTER_ADD(pCounter, Addend) do { } while (0)
#endif
/** @def STAM_COUNTER_ADD
 * Increments a counter sample by a value.
 *
 * @param   pCounter    Pointer to the STAMCOUNTER structure to operate on.
 * @param   Addend      The value to add to the counter.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_COUNTER_ADD(pCounter, Addend) STAM_REL_COUNTER_ADD(pCounter, Addend)
#else
# define STAM_COUNTER_ADD(pCounter, Addend) do { } while (0)
#endif


/** @def STAM_REL_COUNTER_RESET
 * Resets the statistics sample.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_COUNTER_RESET(pCounter) do { (pCounter)->c = 0; } while (0)
#else
# define STAM_REL_COUNTER_RESET(pCounter) do { } while (0)
#endif
/** @def STAM_COUNTER_RESET
 * Resets the statistics sample.
 */
#ifndef VBOX_WITH_STATISTICS
# define STAM_COUNTER_RESET(pCounter) STAM_REL_COUNTER_RESET(pCounter)
#else
# define STAM_COUNTER_RESET(pCounter) do { } while (0)
#endif



/**
 * Profiling sample - STAMTYPE_PROFILE.
 */
typedef struct STAMPROFILE
{
    /** Number of periods. */
    volatile uint64_t   cPeriods;
    /** Total count of ticks. */
    volatile uint64_t   cTicks;
    /** Maximum tick count during a sampling. */
    volatile uint64_t   cTicksMax;
    /** Minimum tick count during a sampling. */
    volatile uint64_t   cTicksMin;
} STAMPROFILE;
/** Pointer to a profile sample. */
typedef STAMPROFILE *PSTAMPROFILE;
/** Pointer to a const profile sample. */
typedef const STAMPROFILE *PCSTAMPROFILE;


/** @def STAM_REL_PROFILE_START
 * Samples the start time of a profiling period.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_START(pProfile, Prefix) \
    uint64_t Prefix##_tsStart; \
    STAM_GET_TS(Prefix##_tsStart)
#else
# define STAM_REL_PROFILE_START(pProfile, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_START
 * Samples the start time of a profiling period.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_START(pProfile, Prefix) STAM_REL_PROFILE_START(pProfile, Prefix)
#else
# define STAM_PROFILE_START(pProfile, Prefix) do { } while (0)
#endif

/** @def STAM_REL_PROFILE_STOP
 * Samples the stop time of a profiling period and updates the sample.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_STOP(pProfile, Prefix) \
    do { \
        uint64_t Prefix##_cTicks; \
        uint64_t Prefix##_tsStop; \
        STAM_GET_TS(Prefix##_tsStop); \
        Prefix##_cTicks = Prefix##_tsStop - Prefix##_tsStart; \
        (pProfile)->cTicks += Prefix##_cTicks; \
        (pProfile)->cPeriods++; \
        if ((pProfile)->cTicksMax < Prefix##_cTicks) \
            (pProfile)->cTicksMax = Prefix##_cTicks; \
        if ((pProfile)->cTicksMin > Prefix##_cTicks) \
            (pProfile)->cTicksMin = Prefix##_cTicks; \
    } while (0)
#else
# define STAM_REL_PROFILE_STOP(pProfile, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_STOP
 * Samples the stop time of a profiling period and updates the sample.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_STOP(pProfile, Prefix) STAM_REL_PROFILE_STOP(pProfile, Prefix)
#else
# define STAM_PROFILE_STOP(pProfile, Prefix) do { } while (0)
#endif


/** @def STAM_REL_PROFILE_STOP_EX
 * Samples the stop time of a profiling period and updates both the sample
 * and an attribution sample.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   pProfile2   Pointer to the STAMPROFILE structure which this
 *                      interval should be attributed too. This may be NULL.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_STOP_EX(pProfile, pProfile2, Prefix) \
    do { \
        uint64_t Prefix##_cTicks; \
        uint64_t Prefix##_tsStop; \
        STAM_GET_TS(Prefix##_tsStop); \
        Prefix##_cTicks = Prefix##_tsStop - Prefix##_tsStart; \
        (pProfile)->cTicks += Prefix##_cTicks; \
        (pProfile)->cPeriods++; \
        if ((pProfile)->cTicksMax < Prefix##_cTicks) \
            (pProfile)->cTicksMax = Prefix##_cTicks; \
        if ((pProfile)->cTicksMin > Prefix##_cTicks) \
            (pProfile)->cTicksMin = Prefix##_cTicks; \
        \
        if ((pProfile2)) \
        { \
            (pProfile2)->cTicks += Prefix##_cTicks; \
            (pProfile2)->cPeriods++; \
            if ((pProfile2)->cTicksMax < Prefix##_cTicks) \
                (pProfile2)->cTicksMax = Prefix##_cTicks; \
            if ((pProfile2)->cTicksMin > Prefix##_cTicks) \
                (pProfile2)->cTicksMin = Prefix##_cTicks; \
        } \
    } while (0)
#else
# define STAM_REL_PROFILE_STOP_EX(pProfile, pProfile2, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_STOP_EX
 * Samples the stop time of a profiling period and updates both the sample
 * and an attribution sample.
 *
 * @param   pProfile    Pointer to the STAMPROFILE structure to operate on.
 * @param   pProfile2   Pointer to the STAMPROFILE structure which this
 *                      interval should be attributed too. This may be NULL.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_STOP_EX(pProfile, pProfile2, Prefix) STAM_REL_PROFILE_STOP_EX(pProfile, pProfile2, Prefix)
#else
# define STAM_PROFILE_STOP_EX(pProfile, pProfile2, Prefix) do { } while (0)
#endif


/**
 * Advanced profiling sample - STAMTYPE_PROFILE_ADV.
 *
 * Identical to a STAMPROFILE sample, but the start timestamp
 * is stored after the STAMPROFILE structure so the sampling
 * can start and stop in different functions.
 */
typedef struct STAMPROFILEADV
{
    /** The STAMPROFILE core. */
    STAMPROFILE         Core;
    /** The start timestamp. */
    volatile uint64_t   tsStart;
} STAMPROFILEADV;
/** Pointer to a advanced profile sample. */
typedef STAMPROFILEADV *PSTAMPROFILEADV;
/** Pointer to a const advanced profile sample. */
typedef const STAMPROFILEADV *PCSTAMPROFILEADV;


/** @def STAM_REL_PROFILE_ADV_START
 * Samples the start time of a profiling period.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_ADV_START(pProfileAdv, Prefix) \
    STAM_GET_TS((pProfileAdv)->tsStart)
#else
# define STAM_REL_PROFILE_ADV_START(pProfileAdv, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_ADV_START
 * Samples the start time of a profiling period.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_ADV_START(pProfileAdv, Prefix) STAM_REL_PROFILE_ADV_START(pProfileAdv, Prefix)
#else
# define STAM_PROFILE_ADV_START(pProfileAdv, Prefix) do { } while (0)
#endif


/** @def STAM_REL_PROFILE_ADV_STOP
 * Samples the stop time of a profiling period and updates the sample.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_ADV_STOP(pProfileAdv, Prefix) \
    do { \
        uint64_t Prefix##_tsStop; \
        STAM_GET_TS(Prefix##_tsStop); \
        if ((pProfileAdv)->tsStart) \
        { \
            uint64_t Prefix##_cTicks = Prefix##_tsStop - (pProfileAdv)->tsStart; \
            (pProfileAdv)->tsStart = 0; \
            (pProfileAdv)->Core.cTicks += Prefix##_cTicks; \
            (pProfileAdv)->Core.cPeriods++; \
            if ((pProfileAdv)->Core.cTicksMax < Prefix##_cTicks) \
                (pProfileAdv)->Core.cTicksMax = Prefix##_cTicks; \
            if ((pProfileAdv)->Core.cTicksMin > Prefix##_cTicks) \
                (pProfileAdv)->Core.cTicksMin = Prefix##_cTicks; \
        } \
    } while (0)
#else
# define STAM_REL_PROFILE_ADV_STOP(pProfileAdv, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_ADV_STOP
 * Samples the stop time of a profiling period and updates the sample.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_ADV_STOP(pProfileAdv, Prefix) STAM_REL_PROFILE_ADV_STOP(pProfileAdv, Prefix)
#else
# define STAM_PROFILE_ADV_STOP(pProfileAdv, Prefix) do { } while (0)
#endif


/** @def STAM_REL_PROFILE_ADV_SUSPEND
 * Suspends the sampling for a while. This can be useful to exclude parts
 * covered by other samples without screwing up the count, and average+min times.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables. The prefix
 *                      must match that of the resume one since it stores the
 *                      suspend time in a stack variable.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_ADV_SUSPEND(pProfileAdv, Prefix) \
    uint64_t Prefix##_tsSuspend; \
    STAM_GET_TS(Prefix##_tsSuspend)
#else
# define STAM_REL_PROFILE_ADV_SUSPEND(pProfileAdv, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_ADV_SUSPEND
 * Suspends the sampling for a while. This can be useful to exclude parts
 * covered by other samples without screwing up the count, and average+min times.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables. The prefix
 *                      must match that of the resume one since it stores the
 *                      suspend time in a stack variable.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_ADV_SUSPEND(pProfileAdv, Prefix) STAM_REL_PROFILE_ADV_SUSPEND(pProfileAdv, Prefix)
#else
# define STAM_PROFILE_ADV_SUSPEND(pProfileAdv, Prefix) do { } while (0)
#endif


/** @def STAM_REL_PROFILE_ADV_RESUME
 * Counter to STAM_REL_PROFILE_ADV_SUSPEND.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables. This must
 *                      match the one used with the SUSPEND!
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_ADV_RESUME(pProfileAdv, Prefix) \
    do { \
        uint64_t Prefix##_tsNow; \
        STAM_GET_TS(Prefix##_tsNow); \
        (pProfileAdv)->tsStart += Prefix##_tsNow - Prefix##_tsSuspend; \
    } while (0)
#else
# define STAM_REL_PROFILE_ADV_RESUME(pProfileAdv, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_ADV_RESUME
 * Counter to STAM_PROFILE_ADV_SUSPEND.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   Prefix      Identifier prefix used to internal variables. This must
 *                      match the one used with the SUSPEND!
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_ADV_RESUME(pProfileAdv, Prefix) STAM_REL_PROFILE_ADV_RESUME(pProfileAdv, Prefix)
#else
# define STAM_PROFILE_ADV_RESUME(pProfileAdv, Prefix) do { } while (0)
#endif


/** @def STAM_REL_PROFILE_ADV_STOP_EX
 * Samples the stop time of a profiling period and updates both the sample
 * and an attribution sample.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   pProfile2   Pointer to the STAMPROFILE structure which this
 *                      interval should be attributed too. This may be NULL.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
# define STAM_REL_PROFILE_ADV_STOP_EX(pProfileAdv, pProfile2, Prefix) \
    do { \
        uint64_t Prefix##_tsStop; \
        STAM_GET_TS(Prefix##_tsStop); \
        if ((pProfileAdv)->tsStart) \
        { \
            uint64_t Prefix##_cTicks = Prefix##_tsStop - (pProfileAdv)->tsStart; \
            (pProfileAdv)->tsStart = 0; \
            (pProfileAdv)->Core.cTicks += Prefix##_cTicks; \
            (pProfileAdv)->Core.cPeriods++; \
            if ((pProfileAdv)->Core.cTicksMax < Prefix##_cTicks) \
                (pProfileAdv)->Core.cTicksMax = Prefix##_cTicks; \
            if ((pProfileAdv)->Core.cTicksMin > Prefix##_cTicks) \
                (pProfileAdv)->Core.cTicksMin = Prefix##_cTicks; \
            if ((pProfile2)) \
            { \
                (pProfile2)->cTicks += Prefix##_cTicks; \
                (pProfile2)->cPeriods++; \
                if ((pProfile2)->cTicksMax < Prefix##_cTicks) \
                    (pProfile2)->cTicksMax = Prefix##_cTicks; \
                if ((pProfile2)->cTicksMin > Prefix##_cTicks) \
                    (pProfile2)->cTicksMin = Prefix##_cTicks; \
            } \
        } \
    } while (0)
#else
# define STAM_REL_PROFILE_ADV_STOP_EX(pProfileAdv, pProfile2, Prefix) do { } while (0)
#endif
/** @def STAM_PROFILE_ADV_STOP_EX
 * Samples the stop time of a profiling period and updates both the sample
 * and an attribution sample.
 *
 * @param   pProfileAdv Pointer to the STAMPROFILEADV structure to operate on.
 * @param   pProfile2   Pointer to the STAMPROFILE structure which this
 *                      interval should be attributed too. This may be NULL.
 * @param   Prefix      Identifier prefix used to internal variables.
 */
#ifdef VBOX_WITH_STATISTICS
# define STAM_PROFILE_ADV_STOP_EX(pProfileAdv, pProfile2, Prefix) STAM_REL_PROFILE_ADV_STOP_EX(pProfileAdv, pProfile2, Prefix)
#else
# define STAM_PROFILE_ADV_STOP_EX(pProfileAdv, pProfile2, Prefix) do { } while (0)
#endif


/**
 * Ratio of A to B, uint32_t types.
 * @remark Use STAM_STATS or STAM_REL_STATS for modifying A & B values.
 */
typedef struct STAMRATIOU32
{
    /** Sample A. */
    uint32_t volatile   u32A;
    /** Sample B. */
    uint32_t volatile   u32B;
} STAMRATIOU32;
/** Pointer to a uint32_t ratio. */
typedef STAMRATIOU32 *PSTAMRATIOU32;
/** Pointer to const a uint32_t ratio. */
typedef const STAMRATIOU32 *PCSTAMRATIOU32;




/** @defgroup grp_stam_r3   The STAM Host Context Ring 3 API
 * @ingroup grp_stam
 * @{
 */

STAMR3DECL(int) STAMR3InitUVM(PUVM pUVM);
STAMR3DECL(void) STAMR3TermUVM(PUVM pUVM);
STAMR3DECL(int) STAMR3RegisterU(PUVM pUVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                const char *pszName, STAMUNIT enmUnit, const char *pszDesc);
STAMR3DECL(int) STAMR3Register(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                               const char *pszName, STAMUNIT enmUnit, const char *pszDesc);

/** @def STAM_REL_REG
 * Registers a statistics sample.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
 *                      Further nesting is possible.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 */
#define STAM_REL_REG(pVM, pvSample, enmType, pszName, enmUnit, pszDesc) \
    STAM_REL_STATS({ int rcStam = STAMR3Register(pVM, pvSample, enmType, STAMVISIBILITY_ALWAYS, pszName, enmUnit, pszDesc); \
                     AssertRC(rcStam); })
/** @def STAM_REG
 * Registers a statistics sample if statistics are enabled.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
 *                      Further nesting is possible.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 */
#define STAM_REG(pVM, pvSample, enmType, pszName, enmUnit, pszDesc) \
    STAM_STATS({STAM_REL_REG(pVM, pvSample, enmType, pszName, enmUnit, pszDesc);})

/** @def STAM_REL_REG_USED
 * Registers a statistics sample which only shows when used.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
 *                      Further nesting is possible.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 */
#define STAM_REL_REG_USED(pVM, pvSample, enmType, pszName, enmUnit, pszDesc) \
    STAM_REL_STATS({ int rcStam = STAMR3Register(pVM, pvSample, enmType, STAMVISIBILITY_USED, pszName, enmUnit, pszDesc); \
                     AssertRC(rcStam);})
/** @def STAM_REG_USED
 * Registers a statistics sample which only shows when used, if statistics are enabled.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
 *                      Further nesting is possible.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 */
#define STAM_REG_USED(pVM, pvSample, enmType, pszName, enmUnit, pszDesc) \
    STAM_STATS({ STAM_REL_REG_USED(pVM, pvSample, enmType, pszName, enmUnit, pszDesc); })

STAMR3DECL(int)  STAMR3RegisterFU(PUVM pUVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                  const char *pszDesc, const char *pszName, ...);
STAMR3DECL(int)  STAMR3RegisterF(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                 const char *pszDesc, const char *pszName, ...);
STAMR3DECL(int)  STAMR3RegisterVU(PUVM pUVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                  const char *pszDesc, const char *pszName, va_list args);
STAMR3DECL(int)  STAMR3RegisterV(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                 const char *pszDesc, const char *pszName, va_list args);

/**
 * Resets the sample.
 * @param   pVM         The VM handle.
 * @param   pvSample    The sample registered using STAMR3RegisterCallback.
 */
typedef void FNSTAMR3CALLBACKRESET(PVM pVM, void *pvSample);
/** Pointer to a STAM sample reset callback. */
typedef FNSTAMR3CALLBACKRESET *PFNSTAMR3CALLBACKRESET;

/**
 * Prints the sample into the buffer.
 *
 * @param   pVM         The VM handle.
 * @param   pvSample    The sample registered using STAMR3RegisterCallback.
 * @param   pszBuf      The buffer to print into.
 * @param   cchBuf      The size of the buffer.
 */
typedef void FNSTAMR3CALLBACKPRINT(PVM pVM, void *pvSample, char *pszBuf, size_t cchBuf);
/** Pointer to a STAM sample print callback. */
typedef FNSTAMR3CALLBACKPRINT *PFNSTAMR3CALLBACKPRINT;

STAMR3DECL(int)  STAMR3RegisterCallback(PVM pVM, void *pvSample, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                        PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                                        const char *pszDesc, const char *pszName, ...);
STAMR3DECL(int)  STAMR3RegisterCallbackV(PVM pVM, void *pvSample, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                         PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                                         const char *pszDesc, const char *pszName, va_list args);
STAMR3DECL(int)  STAMR3DeregisterU(PUVM pUVM, void *pvSample);
STAMR3DECL(int)  STAMR3Deregister(PVM pVM, void *pvSample);

/** @def STAM_REL_DEREG
 * Deregisters a statistics sample if statistics are enabled.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 */
#define STAM_REL_DEREG(pVM, pvSample) \
    STAM_REL_STATS({ int rcStam = STAMR3Deregister(pVM, pvSample); AssertRC(rcStam); })
/** @def STAM_DEREG
 * Deregisters a statistics sample if statistics are enabled.
 *
 * @param   pVM         VM Handle.
 * @param   pvSample    Pointer to the sample.
 */
#define STAM_DEREG(pVM, pvSample) \
    STAM_STATS({ STAM_REL_DEREG(pVM, pvSample); })

STAMR3DECL(int) STAMR3ResetU(PUVM pUVM, const char *pszPat);
STAMR3DECL(int) STAMR3Reset(PVM pVM, const char *pszPat);
STAMR3DECL(int) STAMR3SnapshotU(PUVM pUVM, const char *pszPat, char **ppszSnapshot, size_t *pcchSnapshot, bool fWithDesc);
STAMR3DECL(int) STAMR3Snapshot(PVM pVM, const char *pszPat, char **ppszSnapshot, size_t *pcchSnapshot, bool fWithDesc);
STAMR3DECL(int) STAMR3SnapshotFreeU(PUVM pUVM, char *pszSnapshot);
STAMR3DECL(int) STAMR3SnapshotFree(PVM pVM, char *pszSnapshot);
STAMR3DECL(int) STAMR3DumpU(PUVM pUVM, const char *pszPat);
STAMR3DECL(int) STAMR3Dump(PVM pVM, const char *pszPat);
STAMR3DECL(int) STAMR3DumpToReleaseLogU(PUVM pUVM, const char *pszPat);
STAMR3DECL(int) STAMR3DumpToReleaseLog(PVM pVM, const char *pszPat);
STAMR3DECL(int) STAMR3PrintU(PUVM pUVM, const char *pszPat);
STAMR3DECL(int) STAMR3Print(PVM pVM, const char *pszPat);

/**
 * Callback function for STAMR3Enum().
 *
 * @returns non-zero to halt the enumeration.
 *
 * @param   pszName         The name of the sample.
 * @param   enmType         The type.
 * @param   pvSample        Pointer to the data. enmType indicates the format of this data.
 * @param   enmUnit         The unit.
 * @param   enmVisibility   The visibility.
 * @param   pszDesc         The description.
 * @param   pvUser          The pvUser argument given to STAMR3Enum().
 */
typedef DECLCALLBACK(int) FNSTAMR3ENUM(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                       STAMVISIBILITY enmVisiblity, const char *pszDesc, void *pvUser);
/** Pointer to a FNSTAMR3ENUM(). */
typedef FNSTAMR3ENUM *PFNSTAMR3ENUM;

STAMR3DECL(int) STAMR3EnumU(PUVM pUVM, const char *pszPat, PFNSTAMR3ENUM pfnEnum, void *pvUser);
STAMR3DECL(int) STAMR3Enum(PVM pVM, const char *pszPat, PFNSTAMR3ENUM pfnEnum, void *pvUser);
STAMR3DECL(const char *) STAMR3GetUnit(STAMUNIT enmUnit);

/** @} */

/** @} */

__END_DECLS

#endif
