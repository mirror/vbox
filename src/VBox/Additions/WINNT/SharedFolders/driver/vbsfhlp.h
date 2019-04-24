/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders - File System Driver helpers
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfhlp_h
#define GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfhlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/nt/nt.h> /* includes ntifs.h and wdm.h */
#include <iprt/win/ntverp.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLibSharedFolders.h>


void     vbsfHlpSleep(ULONG ulMillies);
NTSTATUS vbsfHlpCreateDriveLetter(WCHAR Letter, UNICODE_STRING *pDeviceName);
NTSTATUS vbsfHlpDeleteDriveLetter(WCHAR Letter);
uint32_t VBoxToNTFileAttributes(uint32_t fIprtMode);
uint32_t NTToVBoxFileAttributes(uint32_t fNtAttribs);
NTSTATUS VBoxErrorToNTStatus(int vrc);

PVOID    vbsfAllocNonPagedMem(ULONG ulSize);
void     vbsfFreeNonPagedMem(PVOID lpMem);

#if defined(DEBUG) || defined(LOG_ENABLED)
PCHAR MajorFunctionString(UCHAR MajorFunction, LONG MinorFunction);
#endif

NTSTATUS vbsfShflStringFromUnicodeAlloc(PSHFLSTRING *ppShflString, const WCHAR *pwc, uint16_t cb);

#endif /* !GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfhlp_h */

