/** @file
 *
 * VirtualBox Windows Guest Shared Folders
 *
 * File System Driver system helpers
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef __VBSFHLP__H
#define __VBSFHLP__H

#include <ntifs.h>
#include <ntverp.h>

// Windows version identifier
typedef enum
{
    UNKNOWN_WINVERSION = 0, WINNT4 = 1, WIN2K = 2, WINXP = 3
} winVersion_t;

void vbsfHlpSleep (ULONG ulMillies);
NTSTATUS vbsfHlpCreateDriveLetter (WCHAR Letter, UNICODE_STRING *pDeviceName);
NTSTATUS vbsfHlpDeleteDriveLetter (WCHAR Letter);

/**
 * Convert VBox IRT file attributes to NT file attributes
 *
 * @returns NT file attributes
 * @param   fMode       IRT file attributes
 *
 */
uint32_t VBoxToNTFileAttributes (uint32_t fMode);

/**
 * Convert VBox IRT file attributes to NT file attributes
 *
 * @returns NT file attributes
 * @param   fMode       IRT file attributes
 *
 */
uint32_t NTToVBoxFileAttributes (uint32_t fMode);

/**
 * Convert VBox error code to NT status code
 *
 * @returns NT status code
 * @param   vboxRC          VBox error code
 *
 */
NTSTATUS VBoxErrorToNTStatus (int vboxRC);

PVOID vbsfAllocNonPagedMem (ULONG ulSize);
void vbsfFreeNonPagedMem (PVOID lpMem);

winVersion_t vboxQueryWinVersion ();

#endif /* __VBSFHLP__H */
