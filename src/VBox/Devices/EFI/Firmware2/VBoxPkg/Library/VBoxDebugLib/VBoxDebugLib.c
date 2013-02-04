/* $Id$ */
/** @file
 * VBoxDebugLib.c - Debug logging and assertions support routines using DevEFI.
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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
#include <Base.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>

#include "VBoxDebugLib.h"
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathToText.h>
#include <Uefi/UefiSpec.h>
#include <Library/UefiBootServicesTableLib.h>
#include "DevEFI.h"

static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *g_DevPath2Txt;


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

CHAR16* VBoxDebugDevicePath2Str(IN EFI_DEVICE_PATH_PROTOCOL  *pDevicePath)
{
    EFI_STATUS rc;
    if (!g_DevPath2Txt)
    {
        rc = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID **)&g_DevPath2Txt);
        if (EFI_ERROR(rc))
        {
            DEBUG((DEBUG_INFO, "gEfiDevicePathToTextProtocolGuid:%g isn't instantied\n", gEfiDevicePathToTextProtocolGuid));
            return NULL;
        }
    }
    return g_DevPath2Txt->ConvertDevicePathToText(pDevicePath, TRUE, FALSE);
}

CHAR16* VBoxDebugHandleDevicePath2Str(IN EFI_HANDLE hHandle)
{
    EFI_STATUS rc;
    EFI_DEVICE_PATH_PROTOCOL *pDevicePath = NULL;
    CHAR16 *psz16TxtDevicePath;
    rc = gBS->OpenProtocol(hHandle,
                           &gEfiDevicePathProtocolGuid,
                           (VOID **)pDevicePath,
                           NULL,
                           hHandle,
                           EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(rc))
    {
        DEBUG((DEBUG_INFO, "%a:%d failed(%r) to open Device Path Protocol for Handle %p\n",
                __FUNCTION__,
                __LINE__,
                rc,
                hHandle));
        return NULL;
    }
    psz16TxtDevicePath = VBoxDebugHandleDevicePath2Str(pDevicePath);
    return psz16TxtDevicePath;
}
CHAR16* VBoxDebugPrintDevicePath(IN EFI_DEVICE_PATH_PROTOCOL  *pDevicePath)
{
    EFI_STATUS rc;
    if (!g_DevPath2Txt)
    {
        rc = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID **)&g_DevPath2Txt);
        if (EFI_ERROR(rc))
        {
            DEBUG((DEBUG_INFO, "gEfiDevicePathToTextProtocolGuid:%g isn't instantied\n", gEfiDevicePathToTextProtocolGuid));
            return NULL;
        }
    }
    return g_DevPath2Txt->ConvertDevicePathToText(pDevicePath, TRUE, FALSE);
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

