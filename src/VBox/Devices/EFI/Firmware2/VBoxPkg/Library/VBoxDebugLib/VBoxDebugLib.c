/* $Id$ */
/** @file
 * VBoxDebugLib.c - Debug logging and assertions support routines using DevEFI.
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
#include <Base.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>

#include "VBoxDebugLib.h"
#include "DevEFI.h"



VOID EFIAPI
DebugPrint(IN UINTN ErrorLevel, IN CONST CHAR8 *Format, ...)
{
    CHAR8       szBuf[256];
    VA_LIST     va;
    UINTN       cch;
    RTCCUINTREG SavedFlags;

    /* No pool noise, please. */
    if (ErrorLevel == DEBUG_POOL)
        return;

    VA_START(va, Format);
    cch = AsciiVSPrint(szBuf, sizeof(szBuf), Format, va);
    VA_END(va);

    /* make sure it's terminated and doesn't end with a newline */
    if (cch >= sizeof(szBuf))
        cch = sizeof(szBuf) - 1;
    while (cch > 0 && (szBuf[cch - 1] == '\n' || szBuf[cch - 1] == '\r'))
        cch--;
    szBuf[cch] = '\0';

    SavedFlags = ASMIntDisableFlags();

    VBoxPrintString("dbg/");
    VBoxPrintHex(ErrorLevel, sizeof(ErrorLevel));
    VBoxPrintChar(' ');
    VBoxPrintString(szBuf);
    VBoxPrintChar('\n');

    ASMSetFlags(SavedFlags);

}


VOID EFIAPI
DebugAssert(IN CONST CHAR8 *FileName, IN UINTN LineNumber, IN CONST CHAR8 *Description)
{
    RTCCUINTREG SavedFlags = ASMIntDisableFlags();

    VBoxPrintString("EFI Assertion failed! File=");
    VBoxPrintString(FileName ? FileName : "<NULL>");
    VBoxPrintString(" line=0x");
    VBoxPrintHex(LineNumber, sizeof(LineNumber));
    VBoxPrintString("\nDescription: ");
    VBoxPrintString(Description ? Description : "<NULL>");

    ASMOutU8(EFI_PANIC_PORT, 2); /** @todo fix this. */

    ASMSetFlags(SavedFlags);
}


VOID * EFIAPI
DebugClearMemory(OUT VOID *Buffer, IN UINTN Length)
{
    return Buffer;
}


BOOLEAN EFIAPI
DebugAssertEnabled(VOID)
{
    return TRUE;
}


BOOLEAN EFIAPI
DebugPrintEnabled(VOID)
{
    /** @todo some PCD for this so we can disable it in release builds. */
    return TRUE;
}


BOOLEAN EFIAPI
DebugCodeEnabled(VOID)
{
    /** @todo ditto */
    return TRUE;
}


BOOLEAN EFIAPI
DebugClearMemoryEnabled(VOID)
{
    return FALSE;
}

