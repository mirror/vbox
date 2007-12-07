/** @file
 * innotek Portable Runtime - Command Line Parsing.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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

/** @name RTOPTIONDEF::f
 * @{ */

/** Requires no extra argument. 
 * (Can be assumed to be 0 for ever.) */
#define RTGETOPT_REQ_NOTHING                    0
/** A value is required or error will be returned. */
#define RTGETOPT_REQ_STRING                     1
/** The value must be a valid signed 32-bit integer or an error will be returned. */
#define RTGETOPT_REQ_INT32                      2
/** The value must be a valid signed 32-bit integer or an error will be returned. */
#define RTGETOPT_REQ_UINT32                     3
/** The mask of the valid required types. */
#define RTGETOPT_REQ_MASK                       3
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
 */
typedef union RTOPTIONUNION
{
    /** Pointer to the definition on failure or when the option doesn't take an argument.
     * This can be NULL for some errors. */
    PCRTOPTIONDEF   pDef;
    /** A RTGETOPT_ARG_FORMAT_STRING option argument. */
    const char     *psz;
    /** A RTGETOPT_ARG_FORMAT_INT32 option argument. */
    int32_t         i32;
    /** A RTGETOPT_ARG_FORMAT_UINT32 option argument . */
    uint32_t        u32;
} RTOPTIONUNION;
/** Pointer to an option argument union. */
typedef RTOPTIONUNION *PRTOPTIONUNION;


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
RTDECL(int) RTGetOpt(int argc, char *argv[], PCRTOPTIONDEF paOptions, size_t cOptions, int *piThis, PRTOPTIONUNION pValueUnion);

/** @} */

__END_DECLS

#endif

