/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Central calls header
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBOXCALLS__H
#define __VBOXCALLS__H

#include <VBox/VBoxGuestLib.h>
#ifndef _NTIFS_
# ifdef RT_OS_WINDOWS
#  if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#   include <iprt/asm.h>
#   define _InterlockedExchange           _InterlockedExchange_StupidDDKvsCompilerCrap
#   define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKvsCompilerCrap
#   define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKvsCompilerCrap
#   define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKvsCompilerCrap
    __BEGIN_DECLS
#   include <ntddk.h>
    __END_DECLS
#   undef  _InterlockedExchange
#   undef  _InterlockedExchangeAdd
#   undef  _InterlockedCompareExchange
#   undef  _InterlockedAddLargeStatistic
#  else
    __BEGIN_DECLS
#   include <ntddk.h>
    __END_DECLS
#  endif
# endif
#endif

#ifdef DEBUG
# define LOG_ENABLED
#endif
#include "VBoxGuestLog.h"

#include <iprt/assert.h>
#define ASSERTVBSF AssertRelease

#include <VBox/shflsvc.h>

typedef struct _VBSFCLIENT
{
    uint32_t ulClientID;
    VBGLHGCMHANDLE handle;
} VBSFCLIENT;
typedef VBSFCLIENT *PVBSFCLIENT;

typedef struct _VBSFMAP
{
    SHFLROOT root;
} VBSFMAP, *PVBSFMAP;


#define VBSF_DRIVE_LETTER_FIRST   L'A'
#define VBSF_DRIVE_LETTER_LAST    L'Z'

#define VBSF_MAX_DRIVES           (VBSF_DRIVE_LETTER_LAST - VBSF_DRIVE_LETTER_FIRST)

/* Poller thread flags. */
#define VBSF_TF_NONE             (0x0000)
#define VBSF_TF_STARTED          (0x0001)
#define VBSF_TF_TERMINATE        (0x0002)
#define VBSF_TF_START_PROCESSING (0x0004)

#define DRIVE_FLAG_WORKING         (0x1)
#define DRIVE_FLAG_LOCKED          (0x2)
#define DRIVE_FLAG_WRITE_PROTECTED (0x4)

#ifdef RT_OS_WINDOWS
/** Device extension structure for each drive letter we created. */
typedef struct _VBSFDRIVE
{
    /*  A pointer to the Driver object we created for the drive. */
    PDEVICE_OBJECT pDeviceObject;

    /** Root handle to access the drive. */
    SHFLROOT root;

    /** Informational string - the resource name on host. */
    WCHAR awcNameHost[256];

    /** Guest drive letter. */
    WCHAR wcDriveLetter;

    /** DRIVE_FLAG_* */
    uint32_t u32DriveFlags;

    /** Head of FCB list. */
    LIST_ENTRY FCBHead;

    /* Synchronise requests directed to the drive. */
    ERESOURCE DriveResource;
} VBSFDRIVE;
typedef VBSFDRIVE *PVBSFDRIVE;
#endif /* RT_OS_WINDOWS */

/* forward decl */
struct _MRX_VBOX_DEVICE_EXTENSION;
typedef struct _MRX_VBOX_DEVICE_EXTENSION *PMRX_VBOX_DEVICE_EXTENSION;

DECLVBGL(int)  vboxInit (void);
DECLVBGL(void) vboxUninit (void);
DECLVBGL(int)  vboxConnect (PVBSFCLIENT pClient);
DECLVBGL(void) vboxDisconnect (PVBSFCLIENT pClient);

DECLVBGL(int) vboxCallQueryMappings (PVBSFCLIENT pClient, SHFLMAPPING paMappings[], uint32_t *pcMappings);

DECLVBGL(int) vboxCallQueryMapName (PVBSFCLIENT pClient, SHFLROOT root, SHFLSTRING *pString, uint32_t size);

/**
 * Create a new file or folder or open an existing one in a shared folder.  Proxies
 * to vbsfCreate in the host shared folder service.
 *
 * @returns IPRT status code, but see note below
 * @param   pClient      Host-guest communication connection
 * @param   pMap         The mapping for the shared folder in which the file
 *                       or folder is to be created
 * @param   pParsedPath  The path of the file or folder relative to the shared
 *                       folder
 * @param   pCreateParms Parameters for file/folder creation.  See the
 *                       structure description in shflsvc.h
 * @retval  pCreateParms See the structure description in shflsvc.h
 *
 * @note This function reports errors as follows.  The return value is always
 *       VINF_SUCCESS unless an exceptional condition occurrs - out of
 *       memory, invalid arguments, etc.  If the file or folder could not be
 *       opened or created, pCreateParms->Handle will be set to
 *       SHFL_HANDLE_NIL on return.  In this case the value in
 *       pCreateParms->Result provides information as to why (e.g.
 *       SHFL_FILE_EXISTS).  pCreateParms->Result is also set on success
 *       as additional information.
 */
DECLVBGL(int) vboxCallCreate (PVBSFCLIENT pClient, PVBSFMAP pMap, PSHFLSTRING pParsedPath, PSHFLCREATEPARMS pCreateParms);

DECLVBGL(int) vboxCallClose (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE Handle);
DECLVBGL(int) vboxCallRemove (PVBSFCLIENT pClient, PVBSFMAP pMap, PSHFLSTRING pParsedPath, uint32_t flags);
DECLVBGL(int) vboxCallRename (PVBSFCLIENT pClient, PVBSFMAP pMap, PSHFLSTRING pSrcPath, PSHFLSTRING pDestPath, uint32_t flags);
DECLVBGL(int) vboxCallFlush (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile);

DECLVBGL(int) vboxCallRead (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile, uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer, bool fLocked);
DECLVBGL(int) vboxCallWrite (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile, uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer, bool fLocked);

DECLVBGL(int) vboxCallLock (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile, uint64_t offset, uint64_t cbSize, uint32_t fLock);

DECLVBGL(int) vboxCallDirInfo (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile,PSHFLSTRING ParsedPath, uint32_t flags,
                               uint32_t index, uint32_t *pcbBuffer, PSHFLDIRINFO pBuffer, uint32_t *pcFiles);
DECLVBGL(int) vboxCallFSInfo (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile, uint32_t flags, uint32_t *pcbBuffer, PSHFLDIRINFO pBuffer);

DECLVBGL(int) vboxCallMapFolder (PVBSFCLIENT pClient, PSHFLSTRING szFolderName, PVBSFMAP pMap);
DECLVBGL(int) vboxCallUnmapFolder (PVBSFCLIENT pClient, PVBSFMAP pMap);
DECLVBGL(int) vboxCallSetUtf8 (PVBSFCLIENT pClient);

#endif /* __VBOXCALLS__H */
