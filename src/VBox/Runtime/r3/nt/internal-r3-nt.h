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
#include "internal/iprt.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Indicates that we're targetting native NT in the current source. */
#define RT_USE_NATIVE_NT                1
/** Initializes a IO_STATUS_BLOCK. */
#define MY_IO_STATUS_BLOCK_INITIALIZER  { STATUS_FAILED_DRIVER_ENTRY, ~(uintptr_t)42 }
/** Similar to INVALID_HANDLE_VALUE in the Windows environment. */
#define MY_INVALID_HANDLE_VALUE         ( (HANDLE)~(uintptr_t)0 )

#ifdef DEBUG_bird
/** Enables the "\\!\" NT path pass thru as well as hacks for listing NT object
 * directories. */
# define IPRT_WITH_NT_PATH_PASSTHRU 1
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
int  rtNtPathOpen(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fFileAttribs, ULONG fShareAccess,
                  ULONG fCreateDisposition, ULONG fCreateOptions, ULONG fObjAttribs,
                  PHANDLE phHandle, PULONG_PTR puDisposition);
int  rtNtPathOpenDir(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fShareAccess, ULONG fCreateOptions,
                     ULONG fObjAttribs, PHANDLE phHandle, bool *pfObjDir);
int  rtNtPathClose(HANDLE hHandle);


/**
 * Internal helper for comparing a WCHAR string with a char string.
 *
 * @returns @c true if equal, @c false if not.
 * @param   pwsz1               The first string.
 * @param   cb1                 The length of the first string, in bytes.
 * @param   psz2                The second string.
 * @param   cch2                The length of the second string.
 */
DECLINLINE(bool) rtNtCompWideStrAndAscii(WCHAR const *pwsz1, size_t cch1, const char *psz2, size_t cch2)
{
    if (cch1 != cch2 * 2)
        return false;
    while (cch2-- > 0)
    {
        unsigned ch1 = *pwsz1++;
        unsigned ch2 = (unsigned char)*psz2++;
        if (ch1 != ch2)
            return false;
    }
    return true;
}


/*******************************************************************************
*   NT APIs                                                                    *
*******************************************************************************/

RT_C_DECLS_BEGIN

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

NTSTATUS NTAPI NtOpenDirectoryObject(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);

typedef struct _OBJECT_DIRECTORY_INFORMATION
{
    UNICODE_STRING Name;
    UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION;
typedef OBJECT_DIRECTORY_INFORMATION *POBJECT_DIRECTORY_INFORMATION;

NTSTATUS NTAPI NtQueryDirectoryObject(HANDLE, PVOID, ULONG, BOOLEAN, BOOLEAN, PULONG, PULONG);


RT_C_DECLS_END

#endif

