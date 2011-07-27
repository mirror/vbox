/* $Id$ */
/** @file
 * VBox USB Monitor
 */
/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxUsbMon_h___
#define ___VBoxUsbMon_h___

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <VBox/sup.h>
#include <iprt/asm.h>
#include <VBox/log.h>

#ifdef DEBUG
/* disables filters */
//#define VBOXUSBMON_DBG_NO_FILTERS
/* disables pnp hooking */
//#define VBOXUSBMON_DBG_NO_PNPHOOK
#endif

#include "../cmn/VBoxDrvTool.h"
#include "../cmn/VBoxUsbTool.h"

#include "VBoxUsbHook.h"
#include "VBoxUsbFlt.h"

PVOID VBoxUsbMonMemAlloc(SIZE_T cbBytes);
PVOID VBoxUsbMonMemAllocZ(SIZE_T cbBytes);
VOID VBoxUsbMonMemFree(PVOID pvMem);

NTSTATUS VBoxUsbMonGetDescriptor(PDEVICE_OBJECT pDevObj, void *buffer, int size, int type, int index, int language_id);
NTSTATUS VBoxUsbMonQueryBusRelations(PDEVICE_OBJECT pDevObj, PFILE_OBJECT pFileObj, PDEVICE_RELATIONS *pDevRelations);

void vboxUsbDbgPrintUnicodeString(PUNICODE_STRING pUnicodeString);

#endif /* #ifndef ___VBoxUsbMon_h___ */
