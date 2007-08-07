/* $Id$ */
/** @file
 * innotek Portable Runtime - Logging to Serial Port.
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Comport to log to (COM2).
 * This is also defined in VBox/nasm.mac. */
//#define UART_BASE   0x2f8 /* COM2 */
#define UART_BASE   0x3f8 /* COM1 */


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
    for (const uint8_t *pu8 = (const uint8_t *)pach; cb-- > 0; pu8++)
    {
        /* expand \n -> \r\n */
        if (*pu8 == '\n')
            RTLogWriteCom("\r", 1);

        /* Check if port is ready. */
        register unsigned   cMaxWait = ~0;
        register uint8_t    u8;
        do
        {
            u8 = ASMInU8(UART_BASE + 5);
            cMaxWait--;
        } while (!(u8 & 0x20) && u8 != 0xff && cMaxWait);

        /* write */
        ASMOutU8(UART_BASE, *pu8);
    }
}

