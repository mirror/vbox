/* $Id$ */
/** @file
 * VBoxMimicry.h - Debug and logging routines implemented by VBoxDebugLib.
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


#ifndef __VBOXMIMICRY_H__
#define __VBOXMIMICRY_H__
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

#define MIMICRY_INTERFACE_COUNT 1
#define DO_9_FAKE_DECL(name)        \
static EFI_STATUS name ## _fake_impl0();  \
static EFI_STATUS name ## _fake_impl1();  \
static EFI_STATUS name ## _fake_impl2();  \
static EFI_STATUS name ## _fake_impl3();  \
static EFI_STATUS name ## _fake_impl4();  \
static EFI_STATUS name ## _fake_impl5();  \
static EFI_STATUS name ## _fake_impl6();  \
static EFI_STATUS name ## _fake_impl7();  \
static EFI_STATUS name ## _fake_impl8();  \
static EFI_STATUS name ## _fake_impl9();

#define FAKE_IMPL(name, guid)                                       \
static EFI_STATUS name ()                                           \
{                                                                   \
    DEBUG((DEBUG_INFO, #name ": of %g called\n", &guid));           \
    return EFI_SUCCESS;                                             \
}

typedef struct 
{
    EFI_HANDLE hImage;
} VBOXMIMICRY, *PVBOXMIMICRY;

PVBOXMIMICRY gThis;

EFI_STATUS install_mimic_interfaces();
EFI_STATUS uninstall_mimic_interfaces();
#endif
