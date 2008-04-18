/** @file
 * Incredibly Portable Runtime - Command Line Parsing.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_getopt_h
#define ___iprt_getopt_h


#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_getopt    RTGetOpt - Command Line Parsing
 * @ingroup grp_rt
 * @{
 */

/** @name RTOPTIONDEF::fFlags
 *
 * @remarks When neither of the RTGETOPT_FLAG_HEX, RTGETOPT_FLAG_OCT and RTGETOPT_FLAG_DEC
 *          flags are specified with a integer value format, RTGetOpt will default to
 *          decimal but recognize the 0x prefix when present. RTGetOpt will not look for
 *          for the octal prefix (0).
 * @{ */
/** Requires no extra argument.
 * (Can be assumed to be 0 for ever.) */
#define RTGETOPT_REQ_NOTHING                    0
/** A value is required or error will be returned. */
#define RTGETOPT_REQ_STRING                     1
/** The value must be a valid signed 8-bit integer or an error will be returned. */
#define RTGETOPT_REQ_INT8                       2
/** The value must be a valid unsigned 8-bit integer or an error will be returned. */
#define RTGETOPT_REQ_UINT8                      3
/** The value must be a valid signed 16-bit integer or an error will be returned. */
#define RTGETOPT_REQ_INT16                      4
/** The value must be a valid unsigned 16-bit integer or an error will be returned. */
#define RTGETOPT_REQ_UINT16                     5
/** The value must be a valid signed 32-bit integer or an error will be returned. */
#define RTGETOPT_REQ_INT32                      6
/** The value must be a valid unsigned 32-bit integer or an error will be returned. */
#define RTGETOPT_REQ_UINT32                     7
/** The value must be a valid signed 64-bit integer or an error will be returned. */
#define RTGETOPT_REQ_INT64                      8
/** The value must be a valid unsigned 64-bit integer or an error will be returned. */
#define RTGETOPT_REQ_UINT64                     9
/** The mask of the valid required types. */
#define RTGETOPT_REQ_MASK                       15
/** Treat the value as hexadecimal - only applicable with the RTGETOPT_REQ_*INT*. */
#define RTGETOPT_FLAG_HEX                       RT_BIT(16)
/** Treat the value as octal - only applicable with the RTGETOPT_REQ_*INT*. */
#define RTGETOPT_FLAG_OCT                       RT_BIT(17)
/** Treat the value as decimal - only applicable with the RTGETOPT_REQ_*INT*. */
#define RTGETOPT_FLAG_DEC                       RT_BIT(18)
/** Mask of valid bits - for validation. */
#define RTGETOPT_VALID_MASK                     ( RTGETOPT_REQ_MASK | RTGETOPT_FLAG_HEX | RTGETOPT_FLAG_OCT | RTGETOPT_FLAG_DEC )
/** @} */

/**
 * An option definition.
 */
typedef struct RTOPTIONDEF
{
    /** The long option.
     * This is optional */
    const char     *pszLong;
    /** The short option character.
     * This doesn't have to be a character, it may also be a \#define or enum value if
     * there isn't any short version of this option. */
    int             iShort;
    /** The flags (RTGETOPT_*). */
    unsigned        fFlags;
} RTOPTIONDEF;
/** Pointer to an option definition. */
typedef RTOPTIONDEF *PRTOPTIONDEF;
/** Pointer to an const option definition. */
typedef const RTOPTIONDEF *PCRTOPTIONDEF;

/**
 * Option argument union.
 *
 * What ends up here depends on argument format in the option definition.
 *
 * @remarks Integers will bet put in the \a i and \a u members and sign/zero extended
 *          according to the signedness indicated by the \a fFlags. So, you can choose
 *          use which ever of the integer members for accessing the value regardless
 *          of restrictions indicated in the \a fFlags.
 */
typedef union RTOPTIONUNION
{
    /** Pointer to the definition on failure or when the option doesn't take an argument.
     * This can be NULL for some errors. */
    PCRTOPTIONDEF   pDef;
    /** A RTGETOPT_ARG_FORMAT_STRING option argument. */
    const char     *psz;

#if !defined(RT_ARCH_AMD64) && !defined(RT_ARCH_X86)
# error "PORTME: big-endian systems will need to fix the layout here to get the next two fields working right"
#endif

    /** A RTGETOPT_ARG_FORMAT_INT8 option argument. */
    int8_t          i8;
    /** A RTGETOPT_ARG_FORMAT_UINT8 option argument . */
    uint8_t         u8;
    /** A RTGETOPT_ARG_FORMAT_INT16 option argument. */
    int16_t         i16;
    /** A RTGETOPT_ARG_FORMAT_UINT16 option argument . */
    uint16_t        u16;
    /** A RTGETOPT_ARG_FORMAT_INT16 option argument. */
    int32_t         i32;
    /** A RTGETOPT_ARG_FORMAT_UINT32 option argument . */
    uint32_t        u32;
    /** A RTGETOPT_ARG_FORMAT_INT64 option argument. */
    int64_t         i64;
    /** A RTGETOPT_ARG_FORMAT_UINT64 option argument. */
    uint64_t        u64;
    /** A signed integer value. */
    int64_t         i;
    /** An unsigned integer value. */
    uint64_t        u;
} RTOPTIONUNION;
/** Pointer to an option argument union. */
typedef RTOPTIONUNION *PRTOPTIONUNION;
/** Pointer to a const option argument union. */
typedef RTOPTIONUNION const *PCRTOPTIONUNION;


/**
 * Command line argument parser, handling both long and short options and checking
 * argument formats, if desired.
 *
 * This is to be called in a loop until it returns 0 (meaning that all options
 * were parsed) or a negative value (meaning that an error occured). The passed in
 * argument vector is sorted into options and non-option arguments, such that when
 * returning 0 the *piThis is the index of the first non-option argument.
 *
 * For example, for a program which takes the following options:
 *
 *   --optwithstring (or -s) and a string argument;
 *   --optwithint (or -i) and a 32-bit signed integer argument;
 *   --verbose (or -v) with no arguments,
 *
 * code would look something like this:
 *
 * @code
 * int main(int argc, char *argv[])
 * {
 *      static const RTOPTIONDEF g_aOptions[] =
 *      {
 *          { "--optwithstring",    's', RTGETOPT_REQ_STRING },
 *          { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
 *          { "--verbose",          'v', 0 },
 *      };
 *
 *      int ch;
 *      int i = 1;
 *      RTOPTIONUNION ValueUnion;
 *      while ((ch = RTGetOpt(argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), &i, &ValueUnion)))
 *      {
 *          if (ch < 0) { .... error }
 *
 *          // for options that require an argument, ValueUnion has received the value
 *          switch (ch)
 *          {
 *              case 's': // --optwithstring or -s
 *                  // string argument, copy ValueUnion.psz
 *                  break;
 *
 *              case 'i': // --optwithint or -i
 *                  // integer argument, copy ValueUnion.i32
 *                  break;
 *
 *              case 'v': // --verbose or -v
 *                  g_fOptVerbose = true;
 *                  break;
 *          }
 *      }
 *
 *      while (i < argc)
 *      {
 *          //do stuff to argv[i].
 *      }
 *
 *      return 0;
 * }
 * @endcode
 *
 * @param argc          argument count, to be copied from what comes in with main().
 * @param argv          argument array, to be copied from what comes in with main().
 *                      This may end up being modified by the option/argument sorting.
 * @param paOptions     array of RTOPTIONDEF structures, which must specify what options are understood by the program.
 * @param cOptions      number of array items passed in with paOptions.
 * @param piThis        address of stack counter used internally by RTGetOpt; value must be initialized to 1 before the first call!
 * @param pValueUnion   union with value; in the event of an error, psz member points to erroneous parameter; otherwise, for options
 *                      that require an argument, this contains the value of that argument, depending on the type that is required.
 */
RTDECL(int) RTGetOpt(int argc, char **argv, PCRTOPTIONDEF paOptions, size_t cOptions, int *piThis, PRTOPTIONUNION pValueUnion);

/** @} */

__END_DECLS

#endif

