/******************************Module*Header*******************************\
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

ULONG __cdecl DbgPrint(PCH pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
#ifdef VBOX
    RTLogBackdoorPrintfV(pszFormat, args);
#else
    EngDebugPrint(STANDARD_DEBUG_PREFIX, pszFormat, args);
#endif 
    va_end(args);
    return 0;
}

#endif

