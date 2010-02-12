/* $Id$ */
/** @file
 * VBoxConsole.c - Helper driver waiting for Ready to Boot event to switch graphic mode into user-defined one.
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

#include "VBoxConsole.h"
#include "VBoxPkg.h"
#include "DevEFI.h"
#include "iprt/asm.h"

/* @todo understand the reasons why TextOutputProtocol.SetMode isn't enough to switch mode. */
#define VBOX_CONSOLE_VAR L"VBOX_CONSOLE_VAR"
/*b53865fd-b76c-4433-9e85-c0cadf65aab8*/
static EFI_GUID gVBoxConsoleVarGuid = { 0xb53865fd, 0xb76c, 0x4433, { 0x9e, 0x85, 0xc0, 0xca, 0xdf, 0x65, 0xaa, 0xb8}};

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOutputProtocol; 
static EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;
static EFI_UGA_DRAW_PROTOCOL           *Uga;

/*
 *   @todo move this function to the library.
 */
static UINT32
GetVmVariable(UINT32 Variable, CHAR8* Buffer, UINT32 Size )
{
    UINT32 VarLen, i;


    ASMOutU32(EFI_INFO_PORT, Variable);
    VarLen = ASMInU32(EFI_INFO_PORT);

    for (i=0; i < VarLen && i < Size; i++)
    {
        Buffer[i] = ASMInU8(EFI_INFO_PORT);
    }

    return VarLen;
}

static VOID
EFIAPI
ConsoleSwitchMode (
  IN EFI_EVENT                Event,
  IN VOID                     *Context
  )
{
    EFI_STATUS r = EFI_NOT_FOUND; /* Neither GOP nor UGA is found*/
    EFI_TPL               OldTpl;
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    DEBUG((DEBUG_INFO, "%a:%d - SwitchMode\n", __FILE__,  __LINE__));
    if (Gop)
    {
        UINT32 mode = 2;
        GetVmVariable(EFI_INFO_INDEX_GOP_MODE, (CHAR8 *)&mode, sizeof(UINT32));
        r = Gop->SetMode(Gop, mode);
    } 
    else if (Uga)
    {
        UINT32 H = 1027;
        UINT32 V = 768;
        GetVmVariable(EFI_INFO_INDEX_UGA_HORISONTAL_RESOLUTION, (CHAR8 *)&H, sizeof(UINT32));
        GetVmVariable(EFI_INFO_INDEX_UGA_VERTICAL_RESOLUTION, (CHAR8 *)&V, sizeof(UINT32));
        r = Uga->SetMode(Uga, H, V, 32, 60);
    }
    if(EFI_ERROR(r))
    {
        DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
        goto done;
    }
    r = TextOutputProtocol->SetMode(TextOutputProtocol, TextOutputProtocol->Mode->MaxMode);
    if(EFI_ERROR(r))
    {
        DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
        goto done;
    }
    done:
    gBS->RestoreTPL (OldTpl);
    return;
}

EFI_STATUS
EFIAPI
VBoxConsoleInit(EFI_HANDLE hImage, EFI_SYSTEM_TABLE *pSysTable)
{
    EFI_STATUS r;
    UINT32 val;
    EFI_EVENT event;
    UINTN size = sizeof(UINT32);
    DEBUG((DEBUG_INFO, "%a:%d - STARTING\n", __FILE__,  __LINE__));
    r = gRT->GetVariable(VBOX_CONSOLE_VAR, &gVBoxConsoleVarGuid, NULL, &size, &val);
    if (   EFI_ERROR(r)
        && r == EFI_NOT_FOUND)
    {
        size = sizeof(UINT32);
        val = 1;
        r = gRT->SetVariable(VBOX_CONSOLE_VAR, &gVBoxConsoleVarGuid, EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, size, &val);
        if (EFI_ERROR(r))
        {
            DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
            return r;
        }
    
        r = gBS->LocateProtocol(&gEfiSimpleTextOutProtocolGuid, NULL, (VOID **)&TextOutputProtocol);
        if(EFI_ERROR(r))
        {
            DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
        }

        r = gBS->LocateProtocol(&gEfiUgaDrawProtocolGuid, NULL, (VOID **)&Uga);
        if(EFI_ERROR(r))
        {
            DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
        }
        r = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
        if(EFI_ERROR(r))
        {
            DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
        }
        ASSERT((Uga || Gop));
        r = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_NOTIFY, ConsoleSwitchMode, NULL, &gEfiEventReadyToBootGuid, &event);
        if (EFI_ERROR(r))
        {
            DEBUG((DEBUG_INFO, "%a:%d - %r\n", __FILE__,  __LINE__, r));
            return r;
        }
        return r;
    }
    if (!EFI_ERROR(r))
    {
        return EFI_ALREADY_STARTED;
    }
    return r;
}

EFI_STATUS
EFIAPI
VBoxConsoleFini(EFI_HANDLE hImage)
{
    return EFI_SUCCESS;
}

