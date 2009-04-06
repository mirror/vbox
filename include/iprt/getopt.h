/** @file
 * IPRT - Command Line Parsing.
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

/** @name RTGETOPTDEF::fFlags
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
/** The value must be a valid IPv4 address.
 * (Not a name, but 4 values in the 0..255 range with dots separating them). */
#define RTGETOPT_REQ_IPV4ADDR                   10
#if 0
/** The value must be a valid IPv4 CIDR.
 * As with RTGETOPT_REQ_IPV4ADDR, no name.
 * @todo Mix CIDR with types.h or/and net.h first and find a way to make the
 *       mask optional like with ifconfig. See RTCidrStrToIPv4. */
#define RTGETOPT_REQ_IPV4CIDR                   11
#endif
/** The value must be a valid ethernet MAC address. */
#define RTGETOPT_REQ_MACADDR                    14
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
typedef struct RTGETOPTDEF
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
} RTGETOPTDEF;
/** Pointer to an option definition. */
typedef RTGETOPTDEF *PRTGETOPTDEF;
/** Pointer to an const option definition. */
typedef const RTGETOPTDEF *PCRTGETOPTDEF;

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
typedef union RTGETOPTUNION
{
    /** Pointer to the definition on failure or when the option doesn't take an argument.
     * This can be NULL for some errors. */
    PCRTGETOPTDEF   pDef;
    /** A RTGETOPT_REQ_STRING option argument. */
    const char     *psz;

#if !defined(RT_ARCH_AMD64) && !defined(RT_ARCH_X86)
# error "PORTME: big-endian systems will need to fix the layout here to get the next two fields working right"
#endif

    /** A RTGETOPT_REQ_INT8 option argument. */
    int8_t          i8;
    /** A RTGETOPT_REQ_UINT8 option argument . */
    uint8_t         u8;
    /** A RTGETOPT_REQ_INT16 option argument. */
    int16_t         i16;
    /** A RTGETOPT_REQ_UINT16 option argument . */
    uint16_t        u16;
    /** A RTGETOPT_REQ_INT16 option argument. */
    int32_t         i32;
    /** A RTGETOPT_REQ_UINT32 option argument . */
    uint32_t        u32;
    /** A RTGETOPT_REQ_INT64 option argument. */
    int64_t         i64;
    /** A RTGETOPT_REQ_UINT64 option argument. */
    uint64_t        u64;
#ifdef ___iprt_net_h
    /** A RTGETOPT_REQ_IPV4ADDR option argument. */
    RTNETADDRIPV4   IPv4Addr;
#endif
    /** A RTGETOPT_REQ_MACADDR option argument. */
    RTMAC           MacAddr;
    /** A signed integer value. */
    int64_t         i;
    /** An unsigned integer value. */
    uint64_t        u;
} RTGETOPTUNION;
/** Pointer to an option argument union. */
typedef RTGETOPTUNION *PRTGETOPTUNION;
/** Pointer to a const option argument union. */
typedef RTGETOPTUNION const *PCRTGETOPTUNION;


/**
 * RTGetOpt state.
 */
typedef struct RTGETOPTSTATE
{
    /** The next argument. */
    int             iNext;
    /** Argument array. */
    char          **argv;
    /** Number of items in argv. */
    int             argc;
    /** Option definition array. */
    PCRTGETOPTDEF   paOptions;
    /** Number of items in paOptions. */
    size_t          cOptions;
    /** The next short option.
     * (For parsing ls -latrT4 kind of option lists.) */
    const char     *pszNextShort;
    /** The option definition which matched. NULL otherwise. */
    PCRTGETOPTDEF   pDef;
    /* More members will be added later for dealing with initial
       call, optional sorting, '--' and so on. */
} RTGETOPTSTATE;
/** Pointer to RTGetOpt state. */
typedef RTGETOPTSTATE *PRTGETOPTSTATE;


/**
 * Initialize the RTGetOpt state.
 *
 * The passed in argument vector may be sorted if fFlags indicates that this is
 * desired (to be implemented).
 *
 * @returns VINF_SUCCESS, VERR_INVALID_PARAMETER or VERR_INVALID_POINTER.
 * @param   pState      The state.
 *
 * @param   argc        Argument count, to be copied from what comes in with
 *                      main().
 * @param   argv        Argument array, to be copied from what comes in with
 *                      main(). This may end up being modified by the
 *                      option/argument sorting.
 * @param   paOptions   Array of RTGETOPTDEF structures, which must specify what
 *                      options are understood by the program.
 * @param   cOptions    Number of array items passed in with paOptions.
 * @param   iFirst      The argument to start with (in argv).
 * @param   fFlags      The flags. MBZ for now.
 */
RTDECL(int) RTGetOptInit(PRTGETOPTSTATE pState, int argc, char **argv,
                         PCRTGETOPTDEF paOptions, size_t cOptions,
                         int iFirst, uint32_t fFlags);

/**
 * Command line argument parser, handling both long and short options and checking
 * argument formats, if desired.
 *
 * This is to be called in a loop until it returns 0 (meaning that all options
 * were parsed) or a negative value (meaning that an error occured). How non-option
 * arguments are dealt with depends on the flags passed to RTGetOptInit. The default
 * (fFlags = 0) is to return VINF_GETOPT_NOT_OPTION with pValueUnion->psz pointing to
 * the argument string.
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
int main(int argc, char **argv)
{
     RTR3Init();

     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--optwithstring",    's', RTGETOPT_REQ_STRING },
         { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
         { "--verbose",          'v', 0 },
     };

     int ch;
     int i = 1;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
     while ((ch = RTGetOpt(&GetState, &ValueUnion)))
     {
         // for options that require an argument, ValueUnion has received the value
         switch (ch)
         {
             case 's': // --optwithstring or -s
                 // string argument, copy ValueUnion.psz
                 break;

             case 'i': // --optwithint or -i
                 // integer argument, copy ValueUnion.i32
                 break;

             case 'v': // --verbose or -v
                 g_fOptVerbose = true;
                 break;

             case VINF_GETOPT_NOT_OPTION:
                 // handle non-option argument in ValueUnion.psz.
                 break;

             default:
                 if (ch > 0)
                 {
                     if (RT_C_IS_GRAPH(ch))
                         Error("unhandled option: -%c\n", ch);
                     else
                         Error("unhandled option: %i\n", ch);
                 }
                 else if (ch == VERR_GETOPT_UNKNOWN_OPTION)
                     Error("unknown option: %s\n", ValueUnion.psz);
                 else if (ValueUnion.pDef)
                     Error("%s: %Rrs\n", ValueUnion.pDef->pszLong, ch);
                 else
                     Error("%Rrs\n", ch);
                 return 1;
         }
     }

     return 0;
}
   @endcode
 *
 * @returns 0 when done parsing.
 * @returns the iShort value of the option. pState->pDef points to the option
 *          definition which matched.
 * @returns IPRT error status on parse error.
 * @returns VINF_GETOPT_NOT_OPTION when encountering a non-option argument and
 *          RTGETOPT_FLAG_SORT was not specified. pValueUnion->psz points to the
 *          argument string.
 * @returns VERR_GETOPT_UNKNOWN_OPTION when encountering an unknown option.
 *          pValueUnion->psz points to the option string.
 * @returns VERR_GETOPT_REQUIRED_ARGUMENT_MISSING and pValueUnion->pDef if
 *          a required argument (aka value) was missing for an option.
 * @returns VERR_GETOPT_INVALID_ARGUMENT_FORMAT and pValueUnion->pDef if
 *          argument (aka value) conversion failed.
 *
 * @param   pState      The state previously initialized with RTGetOptInit.
 * @param   pValueUnion Union with value; in the event of an error, psz member
 *                      points to erroneous parameter; otherwise, for options
 *                      that require an argument, this contains the value of
 *                      that argument, depending on the type that is required.
 */
RTDECL(int) RTGetOpt(PRTGETOPTSTATE pState, PRTGETOPTUNION pValueUnion);

/** @} */

__END_DECLS

#endif

