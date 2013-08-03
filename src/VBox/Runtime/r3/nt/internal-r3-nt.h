/* $Id$ */
/** @file
 * IPRT - Internal Header for the Native NT code.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___internal_r3_nt_h___
#define ___internal_r3_nt_h___


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <ntstatus.h>
#ifdef IPRT_NT_USE_WINTERNL
# define WIN32_NO_STATUS
# include <windef.h>
# include <winnt.h>
# include <winternl.h>
# define IPRT_NT_NEED_API_GROUP_1

#elif defined(IPRT_NT_USE_WDM)
# include <wdm.h>
# define IPRT_NT_NEED_API_GROUP_1

#else
# include <ntifs.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Initializes a IO_STATUS_BLOCK. */
#define MY_IO_STATUS_BLOCK_INITIALIZER  { STATUS_FAILED_DRIVER_ENTRY, ~(uintptr_t)42 }
/** Similar to INVALID_HANDLE_VALUE in the Windows environment. */
#define MY_INVALID_HANDLE_VALUE         ( (HANDLE)~(uintptr_t)0 )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
int  rtNtPathOpen(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fFileAttribs, ULONG fShareAccess,
                  ULONG fCreateDisposition, ULONG fCreateOptions, ULONG fObjAttribs,
                  PHANDLE phHandle, PULONG_PTR puDisposition);
int  rtNtPathClose(HANDLE hHandle);


/*******************************************************************************
*   NT APIs                                                                    *
*******************************************************************************/

#ifdef IPRT_NT_NEED_API_GROUP_1

typedef struct _FILE_FS_ATTRIBUTE_INFORMATION
{
    ULONG   FileSystemAttributes;
    LONG    MaximumComponentNameLength;
    ULONG   FileSystemNameLength;
    WCHAR   FileSystemName[1];
} FILE_FS_ATTRIBUTE_INFORMATION;
typedef FILE_FS_ATTRIBUTE_INFORMATION *PFILE_FS_ATTRIBUTE_INFORMATION;
extern "C" NTSTATUS NTAPI NtQueryVolumeInformationFile(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FS_INFORMATION_CLASS);

#endif


#endif

