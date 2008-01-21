/* $Id$ */
/** @file
 * Virtual Box Runtime - Guest Backdoor Logging.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#ifdef IN_GUEST_R3
# include <VBox/VBoxGuest.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(size_t) rtLogBackdoorOutput(void *pv, const char *pachChars, size_t cbChars);


RTDECL(size_t) RTLogBackdoorPrintf(const char *pszFormat, ...)
{
    va_list args;
    size_t cb;

    va_start(args, pszFormat);
    cb = RTLogBackdoorPrintfV(pszFormat, args);
    va_end(args);

    return cb;
}


RTDECL(size_t) RTLogBackdoorPrintfV(const char *pszFormat, va_list args)
{
    return RTLogFormatV(rtLogBackdoorOutput, NULL, pszFormat, args);
}


/**
 * Callback for RTLogFormatV which writes to the backdoor.
 * See PFNLOGOUTPUT() for details.
 */
static DECLCALLBACK(size_t) rtLogBackdoorOutput(void *pv, const char *pachChars, size_t cbChars)
{
    RTLogWriteUser(pachChars, cbChars);
    return cbChars;
}


#ifdef IN_GUEST_R3

RTDECL(void) RTLogWriteUser(const char *pch, size_t cb)
{
# ifndef RT_OS_WINDOWS /** @todo VbglR3WriteLog on windows */
    VbglR3WriteLog(pch, cb);
# endif
}

#else  /* !IN_GUEST_R3 */

RTDECL(void) RTLogWriteUser(const char *pch, size_t cb)
{
    const uint8_t *pu8;
    for (pu8 = (const uint8_t *)pch; cb-- > 0; pu8++)
        ASMOutU8(RTLOG_DEBUG_PORT, *pu8);
    /** @todo a rep outs could be more efficient, I don't know...
     * @code
     * __asm {
     *      mov     ecx, [cb]
     *      mov     esi, [pch]
     *      mov     dx, RTLOG_DEFAULT_PORT
     *      rep outsb
     * }
     * @endcode
     */
}

# if defined(RT_OS_LINUX) && defined(IN_MODULE)
/*
 * When we build this in the Linux kernel module, we wish to make the
 * symbols available to other modules as well.
 */
#  include "the-linux-kernel.h"
EXPORT_SYMBOL(RTLogBackdoorPrintf);
EXPORT_SYMBOL(RTLogBackdoorPrintfV);
EXPORT_SYMBOL(RTLogWriteUser);
# endif /* RT_OS_LINUX && IN_MODULE */
#endif /* !IN_GUEST_R3 */

