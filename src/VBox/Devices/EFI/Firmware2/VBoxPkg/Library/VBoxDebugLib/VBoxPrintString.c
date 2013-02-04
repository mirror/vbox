/* $Id$ */
/** @file
 * VBoxPrintString.c - Implementation of the VBoxPrintString() debug logging routine.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxDebugLib.h"
#include "DevEFI.h"


/**
 * Prints a string to the EFI debug port.
 *
 * @returns The string length.
 * @param   pszString       The string to print.
 */
size_t VBoxPrintString(const char *pszString)
{
    const char *pszEnd = pszString;
    while (*pszEnd)
        pszEnd++;
    ASMOutStrU8(EFI_DEBUG_PORT, (uint8_t const *)pszString, pszEnd - pszString);
    return pszEnd - pszString;
}

