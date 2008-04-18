/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Logging to Serial Port.
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifndef IPRT_UART_BASE
/** The port address of the COM port to log to.
 *
 * To override the default (COM1) append IPRT_UART_BASE=0xWXYZ to DEFS in your
 * LocalConfig.kmk. Alternatively you can edit this file, but the don't forget
 * to also update the default found in VBox/asmdefs.h.
 *
 * Standard port assignments are: COM1=0x3f8, COM2=0x2f8, COM3=0x3e8, COM4=0x2e8.
 */
# define IPRT_UART_BASE 0x3f8
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(size_t) rtLogComOutput(void *pv, const char *pachChars, size_t cbChars);


/**
 * Prints a formatted string to the serial port used for logging.
 *
 * @returns Number of bytes written.
 * @param   pszFormat   Format string.
 * @param   ...         Optional arguments specified in the format string.
 */
RTDECL(size_t) RTLogComPrintf(const char *pszFormat, ...)
{
    va_list     args;
    size_t      cb;
    va_start(args, pszFormat);
    cb = RTLogComPrintfV(pszFormat, args);
    va_end(args);

    return cb;
}


/**
 * Prints a formatted string to the serial port used for logging.
 *
 * @returns Number of bytes written.
 * @param   pszFormat   Format string.
 * @param   args        Optional arguments specified in the format string.
 */
RTDECL(size_t) RTLogComPrintfV(const char *pszFormat, va_list args)
{
    return RTLogFormatV(rtLogComOutput, NULL, pszFormat, args);
}


/**
 * Callback for RTLogFormatV which writes to the com port.
 * See PFNLOGOUTPUT() for details.
 */
static DECLCALLBACK(size_t) rtLogComOutput(void *pv, const char *pachChars, size_t cbChars)
{
    if (cbChars)
        RTLogWriteCom(pachChars, cbChars);
    return cbChars;
}


/**
 * Write log buffer to COM port.
 *
 * @param   pach        Pointer to the buffer to write.
 * @param   cb          Number of bytes to write.
 */
RTDECL(void) RTLogWriteCom(const char *pach, size_t cb)
{
    const uint8_t *pu8;
    for (pu8 = (const uint8_t *)pach; cb-- > 0; pu8++)
    {
        register unsigned cMaxWait;
        register uint8_t  u8;

        /* expand \n -> \r\n */
        if (*pu8 == '\n')
            RTLogWriteCom("\r", 1);

        /* Check if port is ready. */
        cMaxWait = ~0;
        do
        {
            u8 = ASMInU8(IPRT_UART_BASE + 5);
            cMaxWait--;
        } while (!(u8 & 0x20) && u8 != 0xff && cMaxWait);

        /* write */
        ASMOutU8(IPRT_UART_BASE, *pu8);
    }
}

