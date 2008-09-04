/* $Id$ */
/** @file
 * Runtime VBox - Logger, the ellipsis variants.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/stdarg.h>


/**
 * Write to a logger instance.
 *
 * @param   pLogger     Pointer to logger instance.
 * @param   pvCallerRet Ignored.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
RTDECL(void) RTLogLogger(PRTLOGGER pLogger, void *pvCallerRet, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
#if defined(RT_OS_DARWIN) && defined(RT_ARCH_X86) && defined(IN_RING3)
    /* manually align the stack before doing the call.
     * We boldly assume that there is a stack frame here! */
    __asm__ __volatile__("andl $-32, %%esp\t\n" ::: "%esp");
    RTLogLoggerExV(pLogger, 0, ~0U, pszFormat, args);
#else
    RTLogLoggerExV(pLogger, 0, ~0U, pszFormat, args);
#endif
    va_end(args);
}


/**
 * Write to a logger instance.
 *
 * This function will check whether the instance, group and flags makes up a
 * logging kind which is currently enabled before writing anything to the log.
 *
 * @param   pLogger     Pointer to logger instance. If NULL the default logger instance will be attempted.
 * @param   fFlags      The logging flags.
 * @param   iGroup      The group.
 *                      The value ~0U is reserved for compatability with RTLogLogger[V] and is
 *                      only for internal usage!
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 * @remark  This is a worker function of LogIt.
 */
RTDECL(void) RTLogLoggerEx(PRTLOGGER pLogger, unsigned fFlags, unsigned iGroup, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTLogLoggerExV(pLogger, fFlags, iGroup, pszFormat, args);
    va_end(args);
}


/**
 * printf like function for writing to the default log.
 *
 * @param   pszFormat   Printf like format string.
 * @param   ...         Optional arguments as specified in pszFormat.
 *
 * @remark The API doesn't support formatting of floating point numbers at the moment.
 */
RTDECL(void) RTLogPrintf(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTLogPrintfV(pszFormat, args);
    va_end(args);
}

