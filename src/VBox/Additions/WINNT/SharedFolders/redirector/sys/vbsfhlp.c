/** @file
 *
 * VirtualBox Windows Guest Shared Folders
 *
 * File System Driver system helpers
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <ntifs.h>
#include <ntdddisk.h>

#include "vbsfhdrs.h"
#include "vbsfhlp.h"

#ifdef DEBUG
static int s_iAllocRefCount = 0;
#endif

void vbsfHlpSleep (ULONG ulMillies)
{
    KEVENT event;
    LARGE_INTEGER dueTime;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    dueTime.QuadPart = -10000 * (int)ulMillies;

    KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, &dueTime);
}

/**
 * Convert VBox IRT file attributes to NT file attributes
 *
 * @returns NT file attributes
 * @param   fMode       IRT file attributes
 *
 */
uint32_t VBoxToNTFileAttributes (uint32_t fMode)
{
    uint32_t FileAttributes = 0;

    if (fMode & RTFS_DOS_READONLY)
        FileAttributes |= FILE_ATTRIBUTE_READONLY;
    if (fMode & RTFS_DOS_HIDDEN)
        FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    if (fMode & RTFS_DOS_SYSTEM)
        FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
    if (fMode & RTFS_DOS_DIRECTORY)
        FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    if (fMode & RTFS_DOS_ARCHIVED)
        FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    if (fMode & RTFS_DOS_NT_TEMPORARY)
        FileAttributes |= FILE_ATTRIBUTE_TEMPORARY;
    if (fMode & RTFS_DOS_NT_SPARSE_FILE)
        FileAttributes |= FILE_ATTRIBUTE_SPARSE_FILE;
    if (fMode & RTFS_DOS_NT_REPARSE_POINT)
        FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    if (fMode & RTFS_DOS_NT_COMPRESSED)
        FileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
    //    if (fMode & RTFS_DOS_NT_OFFLINE)
    if (fMode & RTFS_DOS_NT_NOT_CONTENT_INDEXED)
        FileAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    if (fMode & RTFS_DOS_NT_ENCRYPTED)
        FileAttributes |= FILE_ATTRIBUTE_ENCRYPTED;
    if (fMode & RTFS_DOS_NT_NORMAL)
    {
        FileAttributes |= FILE_ATTRIBUTE_NORMAL;
    }
    return FileAttributes;
}

/**
 * Convert VBox IRT file attributes to NT file attributes
 *
 * @returns NT file attributes
 * @param   fMode       IRT file attributes
 *
 */
uint32_t NTToVBoxFileAttributes (uint32_t fMode)
{
    uint32_t FileAttributes = 0;

    if (fMode & FILE_ATTRIBUTE_READONLY)
        FileAttributes |= RTFS_DOS_READONLY;
    if (fMode & FILE_ATTRIBUTE_HIDDEN)
        FileAttributes |= RTFS_DOS_HIDDEN;
    if (fMode & FILE_ATTRIBUTE_SYSTEM)
        FileAttributes |= RTFS_DOS_SYSTEM;
    if (fMode & FILE_ATTRIBUTE_DIRECTORY)
        FileAttributes |= RTFS_DOS_DIRECTORY;
    if (fMode & FILE_ATTRIBUTE_ARCHIVE)
        FileAttributes |= RTFS_DOS_ARCHIVED;
    if (fMode & FILE_ATTRIBUTE_TEMPORARY)
        FileAttributes |= RTFS_DOS_NT_TEMPORARY;
    if (fMode & FILE_ATTRIBUTE_SPARSE_FILE)
        FileAttributes |= RTFS_DOS_NT_SPARSE_FILE;
    if (fMode & FILE_ATTRIBUTE_REPARSE_POINT)
        FileAttributes |= RTFS_DOS_NT_REPARSE_POINT;
    if (fMode & FILE_ATTRIBUTE_COMPRESSED)
        FileAttributes |= RTFS_DOS_NT_COMPRESSED;
    if (fMode & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
        FileAttributes |= RTFS_DOS_NT_NOT_CONTENT_INDEXED;
    if (fMode & FILE_ATTRIBUTE_ENCRYPTED)
        FileAttributes |= RTFS_DOS_NT_ENCRYPTED;
    if (fMode & FILE_ATTRIBUTE_NORMAL)
    {
        FileAttributes |= RTFS_DOS_NT_NORMAL;
    }
    return FileAttributes;
}

NTSTATUS vbsfHlpCreateDriveLetter (WCHAR Letter, UNICODE_STRING *pDeviceName)
{
    UNICODE_STRING driveName;
    RtlInitUnicodeString(&driveName,L"\\??\\_:" );

    /* Replace '_' with actual drive letter */
    driveName.Buffer[driveName.Length/sizeof(WCHAR) - 2] = Letter;

    return IoCreateSymbolicLink (&driveName, pDeviceName);
}

NTSTATUS vbsfHlpDeleteDriveLetter (WCHAR Letter)
{
    UNICODE_STRING driveName;
    RtlInitUnicodeString(&driveName,L"\\??\\_:" );

    /* Replace '_' with actual drive letter */
    driveName.Buffer[driveName.Length/sizeof(WCHAR) - 2] = Letter;

    return IoDeleteSymbolicLink (&driveName);
}

    /**
     * Convert VBox error code to NT status code
     *
     * @returns NT status code
     * @param   vboxRC          VBox error code
     *
     */
NTSTATUS VBoxErrorToNTStatus (int vboxRC)
{
    NTSTATUS Status;

    switch (vboxRC)
    {
    case VINF_SUCCESS:
        Status = STATUS_SUCCESS;
        break;

    case VERR_ACCESS_DENIED:
        Status = STATUS_ACCESS_DENIED;
        break;

    case VERR_NO_MORE_FILES:
        Status = STATUS_NO_MORE_FILES;
        break;

    case VERR_PATH_NOT_FOUND:
        Status = STATUS_OBJECT_PATH_NOT_FOUND;
        break;

    case VERR_FILE_NOT_FOUND:
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        break;

    case VERR_DIR_NOT_EMPTY:
        Status = STATUS_DIRECTORY_NOT_EMPTY;
        break;

    case VERR_SHARING_VIOLATION:
        Status = STATUS_SHARING_VIOLATION;
        break;

    case VERR_FILE_LOCK_VIOLATION:
        Status = STATUS_FILE_LOCK_CONFLICT;
        break;

    case VERR_FILE_LOCK_FAILED:
        Status = STATUS_LOCK_NOT_GRANTED;
        break;

    case VINF_BUFFER_OVERFLOW:
        Status = STATUS_BUFFER_OVERFLOW;
        break;

    case VERR_EOF:
    case VINF_EOF:
        Status = STATUS_END_OF_FILE;
        break;

    case VERR_READ_ERROR:
    case VERR_WRITE_ERROR:
    case VERR_FILE_IO_ERROR:
        Status = STATUS_UNEXPECTED_IO_ERROR;
        break;

    case VERR_WRITE_PROTECT:
        Status = STATUS_MEDIA_WRITE_PROTECTED;
        break;

    case VERR_ALREADY_EXISTS:
        Status = STATUS_OBJECT_NAME_COLLISION;
        break;

    case VERR_NOT_A_DIRECTORY:
        Status = STATUS_NOT_A_DIRECTORY;
        break;

    case VERR_SEEK:
        Status = STATUS_INVALID_PARAMETER;
        break;

    case VERR_INVALID_PARAMETER:
        Status = STATUS_INVALID_PARAMETER;
        break;

    case VERR_NOT_SUPPORTED:
        Status = STATUS_NOT_SUPPORTED;
        break;

    default:
        /* @todo error handling */
        Status = STATUS_INVALID_PARAMETER;
        AssertMsgFailed(("Unexpected vbox error %d\n", vboxRC));
        break;
    }
    return Status;
}

PVOID vbsfAllocNonPagedMem (ULONG ulSize)
{
    PVOID pMemory = NULL;

#ifdef DEBUG
    s_iAllocRefCount = s_iAllocRefCount + 1;
    Log(("vbsfAllocNonPagedMem: RefCnt after incrementing: %d\n", s_iAllocRefCount));
#endif

    /* Tag is reversed (a.k.a "SHFL") to display correctly in debuggers, so search for "SHFL" */
    pMemory = ExAllocatePoolWithTag(NonPagedPool, ulSize, 'LFHS');

    if (NULL != pMemory)
    {
        RtlZeroMemory(pMemory, ulSize);
#ifdef DEBUG
        Log(("vbsfAllocNonPagedMem: Allocated %d bytes of memory at %p.\n", ulSize, pMemory));
#endif
    }
    else
    {
#ifdef DEBUG
        Log(("vbsfAllocNonPagedMem: ERROR: Could not allocate %d bytes of memory!\n", ulSize));
#endif
    }

    return pMemory;
}

void vbsfFreeNonPagedMem (PVOID lpMem)
{
#ifdef DEBUG
    s_iAllocRefCount = s_iAllocRefCount - 1;
    Log(("vbsfFreeNonPagedMem: RefCnt after decrementing: %d\n", s_iAllocRefCount));
#endif

    Assert(lpMem);

    /* MSDN: The ExFreePoolWithTag routine issues a bug check if the specified value for Tag does not match the tag value passed
     to the routine that originally allocated the memory block. Otherwise, the behavior of this routine is identical to ExFreePool. */
    ExFreePoolWithTag(lpMem, 'LFHS');
    lpMem = NULL;
}

winVersion_t vboxQueryWinVersion ()
{
    static winVersion_t winVersion = UNKNOWN_WINVERSION;
    ULONG majorVersion;
    ULONG minorVersion;
    ULONG buildNumber;

    if (winVersion != UNKNOWN_WINVERSION)
        return winVersion;

    PsGetVersion(&majorVersion, &minorVersion, &buildNumber, NULL);

    Log(("VBoxVideo::vboxQueryWinVersion: running on Windows NT version %d.%d, build %d\n", majorVersion, minorVersion, buildNumber));

    if (majorVersion >= 5)
    {
        if (minorVersion >= 1)
        {
            winVersion = WINXP;
        }
        else
        {
            winVersion = WIN2K;
        }
    }
    else if (majorVersion == 4)
    {
        winVersion = WINNT4;
    }
    else
    {
        Log(("VBoxVideo::vboxQueryWinVersion: NT4 required!\n"));
    }
    return winVersion;
}

#if 0 //def DEBUG
/**
 * Callback for RTLogFormatV which writes to the backdoor.
 * See PFNLOGOUTPUT() for details.
 */
static DECLCALLBACK(size_t) rtLogBackdoorOutput(void *pv, const char *pachChars, size_t cbChars)
{
    RTLogWriteUser(pachChars, cbChars);
    return cbChars;
}

int RTLogBackdoorPrintf1(const char *pszFormat, ...)
{
    va_list args;

    LARGE_INTEGER time;

    KeQueryTickCount(&time);

    RTLogBackdoorPrintf("T=%RX64 ", time.QuadPart);
    va_start(args, pszFormat);
    RTLogFormatV(rtLogBackdoorOutput, NULL, pszFormat, args);
    va_end(args);

    return 0;
}
#endif
