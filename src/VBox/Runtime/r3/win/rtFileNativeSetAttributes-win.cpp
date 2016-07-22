/* $Id$ */
/** @file
 * IPRT - NtSetInformationFile wrapper.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
/* APIs used here require DDK headers. */
#include <wdm.h>

/* Declare ntdll exports. */
extern "C"
{
NTSYSAPI ULONG NTAPI RtlNtStatusToDosError (IN NTSTATUS Status);

NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(IN HANDLE FileHandle,
                                             OUT PIO_STATUS_BLOCK IoStatusBlock,
                                             IN PVOID FileInformation,
                                             IN ULONG Length,
                                             IN FILE_INFORMATION_CLASS FileInformationClass);
}

/** Windows/NT worker.
 * @todo rename to rtFileWinSetAttributes */
int rtFileNativeSetAttributes(HANDLE hFile, ULONG fAttributes)
{
    IO_STATUS_BLOCK IoStatusBlock;
    memset(&IoStatusBlock, 0, sizeof(IoStatusBlock));

    /*
     * Members that are set 0 will not be modified on the file object.
     * See http://msdn.microsoft.com/en-us/library/aa491634.aspx (the struct docs)
     * for details.
     */
    FILE_BASIC_INFORMATION Info;
    memset(&Info, 0, sizeof(Info));
    Info.FileAttributes = fAttributes;

    /** @todo resolve dynamically to avoid dragging in NtDll? */
    NTSTATUS Status = NtSetInformationFile(hFile,
                                           &IoStatusBlock,
                                           &Info,
                                           sizeof(Info),
                                           FileBasicInformation);

    return RtlNtStatusToDosError(Status);
}

