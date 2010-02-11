/* $Id$ */
/** @file
 * VBoxPrintHex.c - Implementation of the VBoxPrintHex() debug logging routine.
 */

/*
 * Copyright (C) 2009-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxDebugLib.h"
#include "DevEFI.h"


/**
 * Prints a char.
 * @param   ch              The char to print.
 */
DECLINLINE(void) vboxPrintHexChar(int ch)
{
    ASMOutU8(EFI_DEBUG_PORT, (uint8_t)ch);
}


/**
 * Print a hex number, up to 64-bit long.
 *
 * @returns Number of chars printed.
 *
 * @param   uValue          The value.
 * @param   cbType          The size of the value type.
 */
size_t VBoxPrintHex(UINT64 uValue, size_t cbType)
{
    static const char s_szHex[17] = "0123456789abcdef";
    switch (cbType)
    {
        case 8:
            vboxPrintHexChar(s_szHex[(uValue >> 60) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 56) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 52) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 48) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 44) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 40) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 36) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 32) & 0xf]);
        case 4:
            vboxPrintHexChar(s_szHex[(uValue >> 28) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 24) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 20) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >> 16) & 0xf]);
        case 2:
            vboxPrintHexChar(s_szHex[(uValue >> 12) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue >>  8) & 0xf]);
        case 1:
            vboxPrintHexChar(s_szHex[(uValue >>  4) & 0xf]);
            vboxPrintHexChar(s_szHex[(uValue      ) & 0xf]);
            break;
    }
    return cbType * 2;
}

