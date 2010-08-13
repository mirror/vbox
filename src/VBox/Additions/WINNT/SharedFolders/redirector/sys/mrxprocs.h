/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 mrxprocs.h

 Abstract:

 The module contains the prototype definitions for all cross referenced
 routines.

 --*/

#ifndef _MRXPROCS_H_
#define _MRXPROCS_H_

//cross-referenced internal routines

//from rename.c
NTSTATUS VBoxMRxRename(
        IN PRX_CONTEXT RxContext,
        IN FILE_INFORMATION_CLASS FileInformationClass,
        IN PVOID pBuffer,
        IN ULONG BufferLength);

NTSTATUS VBoxMRxRemove(IN PRX_CONTEXT RxContext);

// from usrcnnct.c
NTSTATUS
VBoxMRxDeleteConnection (
IN PRX_CONTEXT RxContext,
OUT PBOOLEAN PostToFsp
);

NTSTATUS
VBoxMRxCreateConnection (
IN PRX_CONTEXT RxContext,
OUT PBOOLEAN PostToFsp
);

NTSTATUS
VBoxMRxDoConnection(
IN PRX_CONTEXT RxContext,
ULONG CreateDisposition
);

#endif   // _MRXPROCS_H_

