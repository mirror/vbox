/* $Id$ */
/** @file
 * IPRT - Minimal NTDLL import library defintion file.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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


#include <Windows.h>

#undef  NTSYSAPI
#define NTSYSAPI __declspec(dllexport)

typedef LONG    NTSTATUS;
typedef PVOID   PIO_STATUS_BLOCK;
typedef INT     FILE_INFORMATION_CLASS;
typedef INT     FS_INFORMATION_CLASS;
typedef INT     MEMORY_INFORMATION_CLASS;
typedef INT     PROCESSINFOCLASS;
typedef PVOID   POBJECT_ATTRIBUTES;
typedef PVOID   PIO_APC_ROUTINE;
typedef PVOID   PUNICODE_STRING;


/* Error/status conversion: */

NTSYSAPI ULONG NTAPI RtlNtStatusToDosError(IN NTSTATUS Status)
{
    return 1;
}


/* Queries: */

NTSYSAPI LONG NTAPI NtQueryTimerResolution(OUT PULONG MaximumResolution,
                                           OUT PULONG MinimumResolution,
                                           OUT PULONG CurrentResolution)
{
    return -1;
}

NTSYSAPI LONG NTAPI NtQueryDirectoryFile(IN HANDLE FileHandle,
                                         IN HANDLE Event OPTIONAL,
                                         IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                         IN PVOID ApcContext OPTIONAL,
                                         OUT PIO_STATUS_BLOCK IoStatusBlock,
                                         OUT PVOID FileInformation,
                                         IN ULONG Length,
                                         IN FILE_INFORMATION_CLASS FileInformationClass,
                                         IN BOOLEAN ReturnSingleEntry,
                                         IN PUNICODE_STRING FileName OPTIONAL,
                                         IN BOOLEAN RestartScan)
{
    return -1;
}

NTSYSAPI LONG NTAPI NtQueryDirectoryObject(IN HANDLE ObjectHandle,
                                           OUT PVOID Buffer,
                                           IN ULONG Length,
                                           IN BOOLEAN ReturnSingleEntry,
                                           IN BOOLEAN RestartScan,
                                           IN OUT PULONG Context,
                                           OUT PULONG ReturneLength)
{
    return -1;
}

NTSYSAPI NTSTATUS WINAPI NtQueryInformationFile(IN HANDLE h,
                                                OUT PIO_STATUS_BLOCK b,
                                                OUT PVOID pvBuf,
                                                IN LONG cbBuf,
                                                IN FILE_INFORMATION_CLASS e)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtQueryInformationProcess(IN HANDLE hProcess,
                                                  IN PROCESSINFOCLASS enmProcInfo,
                                                  OUT PVOID pvBuf,
                                                  IN SIZE_T cbBuf,
                                                  OUT PSIZE_T pcbReturned OPTIONAL)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtQueryVolumeInformationFile(IN HANDLE hFile,
                                                     OUT PIO_STATUS_BLOCK IoStatusBlock,
                                                     OUT PVOID pvBuf,
                                                     IN ULONG cbBuf,
                                                     IN FS_INFORMATION_CLASS FsInformationClass)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtQueryVirtualMemory(IN HANDLE hProcess,
                                             IN LPCVOID pvWhere,
                                             IN MEMORY_INFORMATION_CLASS MemoryInfo,
                                             OUT PVOID pvBuf,
                                             IN SIZE_T cbBuf,
                                             OUT PSIZE_T pcbReturned OPTIONAL)
{
    return -1;
}


/* Setters: */

NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(IN HANDLE FileHandle,
                                             OUT PIO_STATUS_BLOCK IoStatusBlock,
                                             IN PVOID FileInformation,
                                             IN ULONG Length,
                                             IN FILE_INFORMATION_CLASS FileInformationClass)
{
    return -1;
}

NTSYSAPI LONG NTAPI NtSetTimerResolution(IN ULONG DesiredResolution,
                                         IN BOOLEAN SetResolution,
                                         OUT PULONG CurrentResolution)
{
    return -1;
}



/* Handles: */

NTSYSAPI NTSTATUS NTAPI NtCreateFile(OUT PHANDLE FileHandle,
                                     IN ACCESS_MASK DesiredAccess,
                                     IN POBJECT_ATTRIBUTES ObjectAttributes,
                                     OUT PIO_STATUS_BLOCK IoStatusBlock,
                                     IN PLARGE_INTEGER AllocationSize OPTIONAL,
                                     IN ULONG FileAttributes,
                                     IN ULONG ShareAccess,
                                     IN ULONG CreateDisposition,
                                     IN ULONG CreateOptions,
                                     IN PVOID EaBuffer,
                                     IN ULONG EaLength)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtOpenDirectoryObject(OUT PHANDLE ObjectHandle,
                                              IN ACCESS_MASK DesiredAccess,
                                              IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtClose(IN HANDLE Handle)
{
    return -1;
}

NTSYSAPI NTSTATUS NTAPI NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
                                              PIO_STATUS_BLOCK IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer,
                                              LONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength)
{
    return -1;
}

