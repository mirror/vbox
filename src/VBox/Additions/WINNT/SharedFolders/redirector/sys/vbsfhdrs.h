/** @file
 *
 * VirtualBox Windows Guest Shared Folders
 *
 * File System Driver main header
 *
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef __VBOXHDRS__H
#define __VBOXHDRS__H

#include "VBoxGuestR0LibSharedFolders.h"

/*
 * UNICODE name of the virtual drive device and FSD device.
 */
#define VBOXSF_DEVICE_NAME_U L"VBoxSharedFolders"

/*
 * The path in the ob namespace for a virtual drive device.
 * ? is replaced with a drive letter.
 */
#define VBOXSF_DEVICE_NAME_TEMPLATE_U L"\\Device\\" VBOXSF_DEVICE_NAME_U L"?"

#if 0 /** @todo r=bird: defined in two .c files but with different parameter lists. dunno who uses them.
       * Vitali / Sander, please fix when you got time. */
NTSTATUS vbsfFsdStartPollerThread (void);
void vbsfFsdStopPollerThread (void);
#endif

NTSTATUS
FatMountVolume (
        IN struct _VOLUME_DEVICE_OBJECT *pVdo
);

typedef struct _VBSFDIRENT
{
    ULONG64 FileSize;
    ULONG64 AllocationSize;

    ULONG64 LastAccessTime;
    ULONG64 CreationTime;
    ULONG64 LastWriteTime;

    UCHAR Attributes;

#if 0
    FAT8DOT3 FileName; //  offset =  0
    UCHAR Attributes; //  offset = 11
    UCHAR NtByte; //  offset = 12
    UCHAR CreationMSec; //  offset = 13
    FAT_TIME_STAMP CreationTime; //  offset = 14
    FAT_DATE LastAccessDate; //  offset = 18
    union
    {
        USHORT ExtendedAttributes; //  offset = 20
        USHORT FirstClusterOfFileHi; //  offset = 20
    };
    FAT_TIME_STAMP LastWriteTime; //  offset = 22
    USHORT FirstClusterOfFile; //  offset = 26
    ULONG32 FileSize; //  offset = 28
#endif
} VBSFDIRENT; //  sizeof = 32
typedef VBSFDIRENT *PVBSFDIRENT;

#endif /* __VBOXHDRS__H */
