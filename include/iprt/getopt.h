/** @file
 * innotek Portable Runtime - Command Line Parsing.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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

/** @def RTGETOPT_REQUIRES_ARGUMENT
 * A command line option requires that an argument (value) follow.
 * The format of the argument can be any string, unless RTGETOPT_ARG_FORMAT_INT32
 * or RTGETOPT_ARG_FORMAT_UINT32 are also specified.
 */
#define RTGETOPT_REQUIRES_ARGUMENT              0x0001

/** @def RTGETOPT_ARG_FORMAT_INT32
 * To be ORed with RTGETOPT_REQUIRES_ARGUMENT; the value must be a valid
 * signed integer, or an error will be returned.
 */
#define RTGETOPT_ARG_FORMAT_INT32               0x0002

/** @def RTGETOPT_ARG_FORMAT_UINT32
 * To be ORed with RTGETOPT_REQUIRES_ARGUMENT; the value must be a valid
 * unsigned integer, or an error will be returned.
 */
#define RTGETOPT_ARG_FORMAT_UINT32              0x0004

typedef struct _rt_getopt_option
{
    const char  *pcszLong;
    char        cShort;
    int         fl;
} RTOPTIONDEF;

typedef union _rt_getopt_value
{
    const char          *pcsz;
    int32_t             i;
    uint32_t            u;
} RTOPTIONUNION;


/**
 * Command line argument parser, handling both long and short options and checking
 * argument formats, if desired.
 *
 * This is to be called in a loop until it returns 0 (meaning that all arguments
 * were parsed) or a negative value (meaning that an error occured).
 *
 * For example, for a program which takes the following options:
 *
 *   --optwithstring (or -s) and a string argument;
 *   --optwithint (or -i) and a signed integer argument;
 *   --verbose (or -v) with no arguments,
 *
 * code would look something like this:
 *
 * @code
 * int main(int argc, char* argv[])
 * {
 *      static const RTOPTIONDEF g_aOptions[]
 *            = {
 *                  { "--optwithstring",    's', RTGETOPT_REQUIRES_ARGUMENT },
 *                  { "--optwithint",       'i', RTGETOPT_REQUIRES_ARGUMENT | RTGETOPT_ARG_FORMAT_INT32 },
 *                  { "--verbose",          'v', 0 },
 *              };
 *
 *      int c;
 *      int i = 1;
 *      RTOPTIONUNION ValueUnion;
 *      while ((c = RTGetOpt(argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), &i, &ValueUnion)))
 *      {
 *          if (c < 0) .... error
 *
 *          // for options that require an argument, ValueUnion has received the value
 *          switch (c)
 *          {
 *              case 's': // --optwithstring or -s
 *                  // string argument, copy ValueUnion.pcsz
 *              break;
 *
 *              case 'i': // --optwithint or -i
 *                  // integer argument, copy ValueUnion.i
 *              break;
 *
 *              case 'v': // --verbose or -v
 *                  g_optVerbose = true;
 *              break;
 *          }
 *      }
 * }
 * @endcode
 *
 * @param argc argument count, to be copied from what comes in with main().
 * @param argv arguments array, to be copied from what comes in with main().
 * @param paOptions array of RTOPTIONDEF structures, which must specify what options are understood by the program.
 * @param cOptions number of array items passed in with paOptions.
 * @param piThis address of stack counter used internally by RTGetOpt; value must be initialized to 1 before the first call!
 * @param pValueUnion union with value; in the event of an error, pcsz member points to erroneous parameter; otherwise, for options
 *             that require an argument, this contains the value of that argument, depending on the type that is required.
 */
RTR3DECL(int) RTGetOpt(int argc, char *argv[], const RTOPTIONDEF *paOptions, int cOptions, int *piThis, RTOPTIONUNION *pValueUnion);

/** @} */

__END_DECLS

#endif

