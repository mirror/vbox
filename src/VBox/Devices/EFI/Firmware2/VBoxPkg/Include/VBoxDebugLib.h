/* $Id$ */
/** @file
 * VBoxDebugLib.h - Debug and logging routines implemented by VBoxDebugLib.
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

#ifndef ___VBoxPkg_VBoxDebugLib_h
#define ___VBoxPkg_VBoxDebugLib_h

#include <Uefi/UefiBaseType.h>
#include "VBoxPkg.h"

size_t VBoxPrintChar(int ch);
size_t VBoxPrintGuid(CONST EFI_GUID *pGuid);
size_t VBoxPrintHex(UINT64 uValue, size_t cbType);
size_t VBoxPrintHexDump(const void *pv, size_t cb);
size_t VBoxPrintString(const char *pszString);

#endif

