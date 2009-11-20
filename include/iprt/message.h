/** @file
 * IPRT - Message Formatting.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___iprt_msg_h
#define ___iprt_msg_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_msg        RTMsg - Message Formatting
 * @ingroup grp_rt
 * @{
 */

/**
 * Print error message to standard error.
 *
 * The message will be prefixed with the file name part of process image name
 * (i.e. no path) and "error: ".  If the message doesn't end with a new line,
 * one will be added.  The caller should call this with an empty string if
 * unsure whether the cursor is currently position at the start of a new line.
 *
 * @returns IPRT status code.
 * @param   pszFormat       The message format string.
 * @param   ...             Format arguments.
 */
RTDECL(int)  RTMsgError(const char *pszFormat, ...);

/**
 * Print error message to standard error.
 *
 * The message will be prefixed with the file name part of process image name
 * (i.e. no path) and "error: ".  If the message doesn't end with a new line,
 * one will be added.  The caller should call this with an empty string if
 * unsure whether the cursor is currently position at the start of a new line.
 *
 * @returns IPRT status code.
 * @param   pszFormat       The message format string.
 * @param   va              Format arguments.
 */
RTDECL(int)  RTMsgErrorV(const char *pszFormat, va_list va);

/** @} */

RT_C_DECLS_END

#endif

