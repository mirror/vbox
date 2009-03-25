/******************************Module*Header*******************************\
*
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
*/
/*
* Based in part on Microsoft DDK sample code
*
*                           *******************
*                           * GDI SAMPLE CODE *
*                           *******************
*
* Module Name: debug.c
*
* debug helpers routine
*
* Copyright (c) 1992-1998 Microsoft Corporation
*
\**************************************************************************/

#include "driver.h"

#ifdef LOG_ENABLED

#ifdef VBOX
#include <VBox/log.h>
#endif

ULONG DebugLevel = 0;

/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive debug print
 *      routine.
 *      If the specified debug level for the print statement is lower or equal
 *      to the current debug level, the message will be printed.
 *
 *   Arguments:
 *
 *      DebugPrintLevel - Specifies at which debugging level the string should
 *          be printed
 *
 *      DebugMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
DebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    )

{

    va_list ap;

    va_start(ap, DebugMessage);

#ifdef VBOX
    RTLogBackdoorPrintfV(DebugMessage, ap);
#else
    if (DebugPrintLevel <= DebugLevel)
    {
        EngDebugPrint(STANDARD_DEBUG_PREFIX, DebugMessage, ap);
    }
#endif

    va_end(ap);

}

#endif

ULONG __cdecl DbgPrint(PCH pszFormat, ...)
{
#ifdef LOG_ENABLED
    va_list args;
    va_start(args, pszFormat);
# ifdef VBOX
    RTLogBackdoorPrintfV(pszFormat, args);
# else
    EngDebugPrint(STANDARD_DEBUG_PREFIX, pszFormat, args);
# endif
    va_end(args);
#endif
    return 0;
}

