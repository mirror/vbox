/* $Id$ */
/** @file
 * VBoxDebugLib.h - Debug and logging routines implemented by VBoxDebugLib.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
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

