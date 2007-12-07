/** @file
 *
 * VirtualBox COM: logging macros and function definitions
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

#ifndef ____H_LOGGING
#define ____H_LOGGING

/** @def LOG_GROUP_MAIN_OVERRIDE
 *  Define this macro to point to the desired log group before including
 *  the |Logging.h| header if you want to use a group other than LOG_GROUP_MAIN
 *  for logging from within Main source files.
 *
 *  @example #define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
 */

/*
 *  We might be including the VBox logging subsystem before
 *  including this header file, so reset the logging group.
 */
#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#ifdef LOG_GROUP_MAIN_OVERRIDE
# define LOG_GROUP LOG_GROUP_MAIN_OVERRIDE
#else
# define LOG_GROUP LOG_GROUP_MAIN
#endif

/* Ensure log macros are enabled if release logging is requested */
#if defined (VBOX_MAIN_RELEASE_LOG) && !defined (DEBUG)
# ifndef LOG_ENABLED
#  define LOG_ENABLED
# endif
#endif

#include <VBox/log.h>
#include <iprt/assert.h>

/** @deprecated Please use LogFlowThisFunc instead! */
#define LogFlowMember(m)     \
    do { LogFlow (("{%p} ", this)); LogFlow (m); } while (0)

#endif // ____H_LOGGING
