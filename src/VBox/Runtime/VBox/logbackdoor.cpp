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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>


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

